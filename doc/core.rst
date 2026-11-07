.. _core:

Core API 设计
=============

状态：定稿。Visitor 层次、二遍扫描、常量池两阶段序列化与栈帧计算（COMPUTE_FRAMES）。

概述
----

Core API 提供 Java ``.class`` 文件的二进制解析与生成，基于 **Visitor 模式**\（``jbytecode::core``\）。对标 ASM 的 ``org.objectweb.asm`` 顶层 API。命名空间\：``jbytecode::core``\。

.. _core-visitor-contract:

Visitor 层次与事件流
--------------------

所有 visitor 使用**委托模式**\：每个类持有一个 ``delegate`` 指针，默认实现转发给 delegate，delegate 为 ``nullptr`` 时退化为 no-op（工厂方法返回 ``nullptr``\）。这是 visitor 链透明变换的基础。

层级 visitor 由父级工厂方法按需创建：

.. code-block:: text

   ClassVisitor
   ├── visitField()            → FieldVisitor
   ├── visitMethod()           → MethodVisitor
   │     └── visitAnnotation() → AnnotationVisitor
   ├── visitModule()           → ModuleVisitor
   └── visitRecordComponent()  → RecordComponentVisitor

事件契约（发射顺序、``visitMaxs`` 位置等）见 :ref:`safety-accept-contract`\。

.. _core-pipeline:

整体执行流程
------------

1. ``.class`` 字节构造 ``ClassReader``\（或直接从文件/流构造）。
2. ``ClassReader::accept`` 把 class 结构讲成 ``visitXxx`` 事件流，注入中间 visitor 链。
3. 链上节点各自处理/转发\：``ClassRemapper`` 改名后转发、``tree::ClassNode`` 建内存树、``ClassWriter`` 编码成字节。
4. ``ClassWriter::toByteArray()`` 产出新的 ``.class`` 字节。

.. _core-two-pass:

二遍扫描（Code 属性解析）
-------------------------

``ClassReader`` 内部用二遍扫描解析 Code 属性。

第一遍（扫描）：

1. 遍历指令\，识别跳转目标。
2. 扫描异常表。
3. 扫描子属性（LineNumber/LocalVar/StackMapTable）。
4. 创建 Label\，记录偏移。

第二遍（发射）：

1. ``visitCode``\。
2. ``visitTryCatchBlock``\。
3. 顺序发射指令。
4. ``visitLabel`` → 查 StackMapTable → ``visitFrame``\。
5. ``visitLineNumber`` / ``visitLocalVariable``\。
6. ``visitMaxs``\。

.. _core-symbol-table:

常量池两阶段序列化
------------------

与 ASM Java 不同，常量池由独立的 ``SymbolTable``\（而非 ``ClassWriter``\）管理。``SymbolTable`` 负责 CP 条目注册 + 去重，序列化字节缓存在 ``ClassWriter::pool_``\。因此 reader → writer 输出不保证复现 ASM Java 的常量池顺序或属性布局\；稳定契约是 class 结构和指令语义一致，详见 :ref:`safety-round-trip`\。

``ClassWriter::toByteArray()`` 的两个阶段：

1. **phase-1（注册收集）**\：每个 ``MethodWriter::toMethodInfo(temp)`` 把方法体内引用的 CP 条目经 ``SymbolTable::addUtf8`` / ``addClass`` / … 注册（tag + data 写入 ``pool_``\），``ClassWriter`` 捕获 ``cpCount`` / ``cpDataSize``\。
2. **phase-2（正式写出）**\：先写 magic/version/cpCount/常量池，再写 access/this/super/interfaces；随后 ``toMethodInfo(out)`` 写出 Code 属性（内部 CP 索引此时已稳定）。

.. _core-forward-ref:

标签正向引用回填
----------------

1. ``visitJumpInsn``\：目标 label 未到达，写入 ``0x0000`` 占位偏移。
2. ``addForwardRef``\：在 ``code_`` 里记录占位位置。
3. ``visitLabel``\：目标到达\，``resolve`` 把记录的全部前向引用回填为实际偏移。

.. _core-frames:

栈帧计算（COMPUTE_FRAMES）
--------------------------

``ClassWriter(COMPUTE_FRAMES)`` 启用自动栈帧计算，在 ``toByteArray()`` 时对每个方法做数据流分析，生成优化编码的 StackMapTable 属性。``COMPUTE_FRAMES`` 隐式包含 ``COMPUTE_MAXS``\。

实现结构与 ASM Frame.java 对齐（``Frame`` / ``CurrentFrame`` 类 + ``computeAllFrames()`` fixpoint）\，不是简化版。能正确处理 javac 生成的全部合法字节码。worklist 带迭代预算，不收敛时抛 ``FrameComputationException``\，避免继承环或病态 provider 导致进程卡死。

.. _core-frames-algorithm:

算法：两步法 + worklist fixpoint
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. 构造隐式第一帧（``setInputFrameFromDescriptor``\）并作为起点。
2. worklist 非空则取一个基本块\，遍历其 ``outgoingEdges``\，对每个后继执行 ``frame.merge(successor, edgeInfo)``\。
3. 后继帧发生改变则重新入队\，循环直到 fixpoint（无基本块再变化）。
4. 遍历所有基本块\：可达且为跳转目标的\，``frame.accept`` 写出 StackMapTable\；不可达基本块的字节被替换为 ``NOP...ATHROW`` 并补一个 ``Throwable`` 帧（与 ASM 完全一致）。

对应 ``MethodWriter::computeAllFrames()``\（``method_writer.cpp``\，对齐 ASM L1560-1658）\：

1. **CFG 构建** —— 把每个异常处理器范围转成 ``Edge(catchType, handlerBlock)``\，挂到范围内各基本块的 ``outgoingEdges``\。
2. **第一帧** —— ``setInputFrameFromDescriptor(access, descriptor, maxLocals)``\：构造器填 ``UNINITIALIZED_THIS``\，普通方法填 ``this`` 类型\，按描述符填参数。
3. **Fixpoint** —— worklist 上反复 ``merge()``\，直到无基本块再变化。``merge()`` 在汇聚点合并 locals/stack\，对象类型用 ``ICommonSuperClassProvider`` 求公共父类。
4. **死代码处理** —— 不可达基本块的字节被替换为 ``NOP...ATHROW`` 并补一个 ``Throwable`` 帧。

.. _core-frames-abstract-type:

AbstractType —— 单 int 类型编码（ASM Frame.java L137-190）
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

   // 单 int 编码: DIM(6bit) | KIND(4bit) | FLAGS(2bit) | VALUE(20bit)
   // KIND:  CONSTANT_KIND(1) | REFERENCE_KIND(2) | UNINITIALIZED_KIND(3)
   //        FORWARD_UNINITIALIZED_KIND(4) | LOCAL_KIND(5) | STACK_KIND(6)
   //
   // 关键：输出帧用 LOCAL_KIND|idx / STACK_KIND|pos 表示
   //       "输入帧 locals[idx]/stack[pos] 的类型"，在输入帧未知时也能算输出帧
   int t = Frame::INTEGER;                    // CONSTANT_KIND | ITEM_INTEGER
   int t = Frame::REFERENCE_KIND | classIdx;  // 对象引用
   int t = Frame::ARRAY_OF | Frame::INTEGER;  // [I

- ``getConcreteOutputType()`` 把输出帧里的 ``LOCAL_KIND``/``STACK_KIND`` 解析回具体类型（在 ``merge()`` 中调用）。
- ``getLocal``/``setLocal``/``push``/``pop`` 维护输出帧的 locals/stack。

.. _core-frames-init:

构造器与 NEW 初始化链
^^^^^^^^^^^^^^^^^^^^^

- 构造器首帧 ``this = UNINITIALIZED_THIS``\。
- ``INVOKESPECIAL <init>`` 触发 ``addInitializedType(被初始化的对象类型)``\，记录一次初始化事件。
- ``merge()`` 时对输出帧里的 UNINITIALIZED 类型调用 ``getInitializedType()``\：若匹配某次初始化事件\，替换为具体类类型（``UNINITIALIZED_THIS`` → 当前类名\；``NEW`` 产生的 → ``NEW`` 指令对应的类）。

.. _core-frames-provider:

公共父类接口（ICommonSuperClassProvider）
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``merge()`` 合并两个不同非数组对象类型时\，需要它们的公共父类。实现 ``ICommonSuperClassProvider`` 提供精确的运行时类层次：

.. code-block:: cpp

   class MyProvider : public core::ICommonSuperClassProvider {
       ca::str::Utf8StringRef getCommonSuperClass(
           const ca::str::Utf8StringRef& t1,
           const ca::str::Utf8StringRef& t2) override {
           return ca::str::Utf8StringRef::from_cstr("java/lang/Number");  // 用 Class.forName 或预计算表
       }
   };
   MyProvider                provider;
   ca::str::Utf8StringArena  arena;
   ClassWriter               cw(arena, ClassWriter::COMPUTE_FRAMES);
   cw.setTypeResolver(&provider);

默认 ``DefaultCommonSuperClassProvider`` 内置常见 JCL 类的小型继承表（Number/Throwable/Exception/Error/RuntimeException/IndexOutOfBoundsException 等）\，表外类型退化为 ``java/lang/Object``\。对绝大多数 javac 输出足够\；若要精确处理任意第三方库的类层次\，注入自定义 provider。

.. _core-frames-encoding:

StackMapTable 优化编码
^^^^^^^^^^^^^^^^^^^^^^

``MethodWriter::putFrame()`` 把每条帧编码为最紧凑格式\：

.. list-table::
   :header-rows: 1

   * - 格式
     - 帧类型值
     - 条件
   * - SAME
     - 0-63
     - 局部变量未变\，栈为空\，delta ≤ 63
   * - SAME1
     - 64-127
     - 局部变量未变\，栈 1 项\，delta ≤ 63
   * - SAME_FRAME_EXTENDED
     - 251
     - 局部变量未变\，栈为空\，delta > 63
   * - SAME1_EXTENDED
     - 247
     - 局部变量未变\，栈 1 项\，delta > 63
   * - CHOP
     - 248-250
     - 局部变量减少 k (1-3)\，delta ≤ 63
   * - APPEND
     - 252-254
     - 局部变量增加 k (1-3)\，delta ≤ 63
   * - FULL_FRAME
     - 255
     - 以上均不满足\，全量描述

实现时先修剪尾部 TOP locals 再比较前后帧差异。

.. _core-frames-limits:

局限
^^^^

1. **JSR/RET** —— 不支持。``COMPUTE_FRAMES`` 模式遇到 JSR/RET 抛 ``std::invalid_argument``\（与 ASM 相同限制）。用 ``JsrInlinerAdapter`` 先内联消除。
2. **公共父类的精度** —— 默认 provider 只覆盖 JCL 常见类\，自定义类层次需注入 ``ICommonSuperClassProvider``\（见上）。
3. **provider 死循环兜底** —— ``BoundedCommonSuperClassProvider``（``bounded_common_super_class_provider.hpp``\）给任意 provider 包上调用预算\；继承环等缺陷导致的无限调用超预算后抛 ``CommonSuperClassBudgetExceeded``\，由 ``MethodWriter`` 转 ``FrameComputationException``\，调用方按既有路径降级而不是进程卡死。

Tree 回放的事件顺序、迭代器失效和线程安全约束统一记录在 :doc:`safety`\，不在各 API 文档中重复维护。

.. _core-api:

关键 API
--------

.. _core-api-reader:

ClassReader
^^^^^^^^^^^

.. code-block:: cpp

   ClassReader(ca::str::Utf8StringArena& arena, const std::vector<ca::u8>& classFile);  // 字节拷贝进 reader
   ClassReader(ca::str::Utf8StringArena& arena, std::istream& inputStream);             // 从流读完
   ClassReader(ca::str::Utf8StringArena& arena, const std::string& classFilePath);      // 从文件
   ClassReader(ca::str::Utf8StringArena& arena, const ca::u8* buf, ca::i32 len);        // 借用 buf，不复制

   ca::i32                            getAccess() const;
   ca::str::Utf8StringRef             getClassName();
   ca::str::Utf8StringRef             getSuperName();
   std::vector<ca::str::Utf8StringRef> getInterfaces();

   void accept(ClassVisitor* cv, ca::i32 parsingOptions);
   void accept(ClassVisitor* cv, Attribute* prototypes, ca::i32 parsingOptions);

   // parsingOptions: SKIP_CODE | SKIP_DEBUG | SKIP_FRAMES | EXPAND_FRAMES

.. _core-api-writer:

ClassWriter
^^^^^^^^^^^

.. code-block:: cpp

   ClassWriter(ca::str::Utf8StringArena& arena);             // maxs/frames 由调用方提供
   ClassWriter(ca::str::Utf8StringArena& arena, int flags);  // flags = COMPUTE_MAXS | COMPUTE_FRAMES

   void setTypeResolver(ICommonSuperClassProvider* r);  // 注入公共父类解析器
   std::vector<ca::u8> toByteArray();                   // 序列化（触发 phase-1/phase-2）

.. _core-api-events:

MethodVisitor（核心事件顺序）
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: text

   visitCode()
     → [visitTryCatchBlock ...]
     → visitInsn / visitVarInsn / visitFieldInsn / visitMethodInsn / visitJumpInsn / visitLabel / visitFrame ...
     → visitLocalVariable ...
     → visitMaxs(maxStack, maxLocals)
   visitEnd()

.. _core-api-helpers:

辅助类型
^^^^^^^^

.. list-table::
   :header-rows: 1

   * - 类
     - 作用
   * - ``Opcodes``
     - 逻辑操作码 + ASM API 版本 + 类版本（``V1_1`` ~ ``V27``\）
   * - ``Type``
     - 类型描述符解析（``getType`` / ``getMethodDescriptor`` / ``getArgumentsAndReturnSizes``\）
   * - ``Label``
     - 字节码偏移锚点\，支持正向引用回填
   * - ``Handle``
     - ``CONSTANT_MethodHandle`` 引用
   * - ``ConstantDynamic``
     - ``CONSTANT_Dynamic`` 引用
   * - ``SymbolTable``
     - 常量池注册/去重（``ClassWriter`` 内部）
   * - ``Attribute``
     - 自定义属性的原型（可通过 ``accept`` 的 prototypes 参数扩展）
   * - ``TypePath`` / ``TypeReference``
     - 类型注解的目标定位

.. _core-minidemo:

minidemo
--------

.. code-block:: cpp

   // 读取 → 修改 → 写出
   ca::str::Utf8StringArena arena;
   ClassReader              reader(arena, data, len);
   ClassWriter              writer(arena, ClassWriter::COMPUTE_MAXS);
   reader.accept(&writer, 0);    // 直通回放（无修改）
   std::vector<ca::u8> result = writer.toByteArray();

   // 或者通过 Tree API 修改后再写
   tree::ClassNode cn;
   reader.accept(&cn, 0);
   // ... 修改 cn ...
   cn.accept(&writer);
