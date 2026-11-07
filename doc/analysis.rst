.. _analysis:

Analysis 设计
=============

状态：定稿。Analyzer + Interpreter 数据流框架、验证器层级与来源追踪。

概述
----

Analysis（``jbytecode::analysis``\）提供基于 ASM 分析框架的数据流分析引擎\：类型推断、字节码验证、指令来源跟踪。对标 ASM 的 ``org.objectweb.asm.tree.analysis``\。

需要 Tree API 的 ``MethodNode``\（CFG 基于指令链表构建）\。

.. _analysis-model:

类关系
------

.. code-block:: text

   Value（接口：getSize()）
   ├── BasicValue    类型类别值（INT/FLOAT/REF/UNINITIALIZED…）
   └── SourceValue   来源值（SmallSet<const AbstractInsnNode*> insns_）

   Interpreter<V>（抽象：newValue/newOperation/copyOperation/unaryOperation/
                  binaryOperation/ternaryOperation/returnOperation/merge）
   ├── BasicInterpreter    → BasicValue
   ├── BasicVerifier       → BasicValue   （继承 BasicInterpreter）
   ├── SimpleVerifier      → BasicValue   （继承 BasicVerifier，类层次感知）
   └── SourceInterpreter   → SourceValue

   Frame<V>   某指令处的抽象栈帧（locals + stack）
   Analyzer<V> 驱动器：analyze(owner, method) → vector<Frame<V>*>

``Analyzer`` 对 ``ACC_ABSTRACT`` / ``ACC_NATIVE`` 方法直接返回空结果\（无方法体可分析）\。

.. _analysis-flow:

分析流程
--------

1. 初始化入口帧\：按描述符填 ``this`` + 参数。
2. worklist 循环\：取一条指令\，``Frame::execute`` 用 ``Interpreter`` 模拟其效果。
3. 确定后继\：普通指令后继 i+1\；分支指令后继为目标 + i+1\；异常路径后继为 handler。
4. 后继帧已在\，则 ``merge``\；``merge`` 改变了后继帧则重新入队\，直到 fixpoint。

异常处理路径由 ``TryCatchBlockNode`` 表驱动\；子程序（JSR/RET）路径由 ``subroutine.hpp`` 的 ``Subroutine`` 结构跟踪。

.. _analysis-interpreters:

Interpreter 选择
----------------

.. list-table::
   :header-rows: 1

   * - Interpreter
     - 输出 Value
     - 用途
   * - ``BasicInterpreter``
     - ``BasicValue``\（类型类别\：INT/FLOAT/REF/…\）
     - 栈帧形状分析、maxStack/maxLocals 验证
   * - ``BasicVerifier``
     - ``BasicValue``
     - JVM 字节码验证规则（操作数类型匹配）
   * - ``SimpleVerifier``
     - ``BasicValue``
     - 类层次感知的精确验证（可配置类层次）
   * - ``SourceInterpreter``
     - ``SourceValue``\（产生该值的指令集合\）
     - 死代码检测、数据流追踪、def-use

.. _analysis-api:

关键 API
--------

Analyzer
^^^^^^^^

.. code-block:: cpp

   template <typename V>
   class Analyzer {
   public:
       explicit Analyzer(Interpreter<V>* interpreter);
       std::vector<Frame<V>*> analyze(ca::str::Utf8StringRef owner,
                                      tree::MethodNode* method);
       // analyze 返回 frames[i] = 指令 i 处的栈帧（nullptr 表示不可达）
   };

Frame
^^^^^

.. code-block:: cpp

   template <typename V>
   class Frame {
   public:
       static Frame newFrame(int nLocals, int maxStack);
       void push(const V& value);
       V    pop();
       V    getLocal(int i) const;
       V    getStack(int i) const;
       int  getLocals() const;
       int  getStackSize() const;
       // execute() + merge() 由 Analyzer 内部调用
   };

自定义 Interpreter
^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

   class MyInterpreter : public Interpreter<MyValue> {
       MyValue newValue(const core::Type* t) override;
       MyValue unaryOperation(const tree::AbstractInsnNode* insn, const MyValue& v) override;
       MyValue merge(const MyValue& v1, const MyValue& v2) override;
   };

minidemo
--------

.. code-block:: cpp

   ca::str::Utf8StringArena arena;
   tree::ClassNode          cn;
   ClassReader              reader(arena, data, len);
   reader.accept(&cn, 0);

   for (auto* method : cn.methods) {
       SimpleVerifier       verifier;
       Analyzer<BasicValue> analyzer(&verifier);
       auto frames = analyzer.analyze(cn.name, method);
       // frames[i] 包含指令 i 处的栈帧（nullptr = 不可达/死代码）
   }
