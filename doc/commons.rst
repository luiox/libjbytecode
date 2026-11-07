.. _commons:

Commons 设计
============

状态：定稿。Remapper 体系与 Adapter 继承链。

概述
----

Commons（``jbytecode::commons``\）在 Core/Tree API 之上提供实用的 Visitor 包装器\：类名重映射、代码生成、局部变量重排、静态块合并等。对标 ASM 的 ``org.objectweb.asm.commons``\。

.. _commons-remapper:

Remapper 体系
-------------

``Remapper`` 是重映射的抽象基类\：``map`` 家族方法把内部类名/描述符/签名映射到新值\，默认实现原样返回。核心方法\：

.. code-block:: text

   Remapper
   ├── map(internalName)                      内部类名
   ├── mapFieldName(owner, name, desc)        字段名
   ├── mapMethodName(owner, name, desc)       方法名
   ├── mapInvokeDynamicMethodName(name, desc) invokedynamic 调用点名
   ├── mapRecordComponentName(owner, name, desc) record 组件名
   ├── mapDesc / mapType / mapMethodDesc / mapTypes / mapSignature   描述符与签名
   └── mapValue(value)                        注解/常量值

``SimpleRemapper`` 用一张 ``unordered_map``（或一组 old→new 名字对）实现 ``map``\；其余 ``*Remapper`` 是各 visitor 层级的映射转发器\：

.. code-block:: text

   ClassRemapper       (ClassVisitor)          ── 持有 Remapper，逐事件改写后转发
   MethodRemapper      (MethodVisitor)
   FieldRemapper       (FieldVisitor)
   AnnotationRemapper  (AnnotationVisitor)
   ModuleRemapper      (ModuleVisitor)
   RecordComponentRemapper (RecordComponentVisitor)
   SignatureRemapper   (SignatureVisitor)      ── 签名里的类类型走 Remapper

注意\：``Remapper`` 及其实现一律注入 ``Utf8StringArena``\（作业级字符串池\，映射产物 intern 进它\）；旧的无 arena 兜底构造已退役。

.. _commons-adapters:

Adapter 继承链
--------------

方法级适配器都直接或间接继承 ``MethodVisitor``\：

.. code-block:: text

   MethodVisitor
   ├── LocalVariablesSorter
   │     └── GeneratorAdapter
   │           └── AdviceAdapter
   ├── InstructionAdapter
   ├── AnalyzerAdapter
   ├── CodeSizeEvaluator
   ├── JsrInlinerAdapter
   ├── SerialVersionUIDAdder
   ├── StaticInitMerger
   └── TryCatchBlockSorter

.. list-table::
   :header-rows: 1

   * - Adapter
     - 作用
   * - ``InstructionAdapter``
     - 用方法名（如 ``load``/``store``/``add``\）代替裸 opcode 的友好生成器
   * - ``GeneratorAdapter``
     - 高层代码生成（``push``/``ifCmp``/``newInstance``/``getTableSwitch``\）\，内置 ``LocalVariablesSorter``
   * - ``AdviceAdapter``
     - 在方法入口/构造器 super 调用后注入代码（AOP 切面）
   * - ``AnalyzerAdapter``
     - 用 Analysis 引擎实时计算当前栈帧\，供 visitor 查询
   * - ``LocalVariablesSorter``
     - 重排局部变量槽位（用于插桩后避免槽位冲突）
   * - ``CodeSizeEvaluator``
     - 统计生成的字节码大小
   * - ``JsrInlinerAdapter``
     - 内联 JSR/RET 子程序（消除遗留子程序指令）
   * - ``SerialVersionUIDAdder``
     - 计算/注入 ``serialVersionUID`` 字段（基于 SHA-1\，libca crypto\）
   * - ``StaticInitMerger``
     - 合并多个 ``<clinit>``
   * - ``TryCatchBlockSorter``
     - 按规范排序异常表

.. _commons-api:

关键 API
--------

SimpleRemapper + ClassRemapper
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

   // 类名重映射：old → new（arena 注入是唯一构造路径）
   std::unordered_map<std::string, std::string> mapping = {
       {"org/old/Foo", "org/new/Foo"},
       {"org/old/Bar", "org/new/Bar"},
   };
   SimpleRemapper remapper(arena, mapping);
   ClassRemapper  cr(&writer, &remapper);
   reader.accept(&cr, 0);   // reader → ClassRemapper → writer

GeneratorAdapter
^^^^^^^^^^^^^^^^

.. code-block:: cpp

   GeneratorAdapter gen(mv, /*access=*/access, /*name=*/name, /*desc=*/desc);
   gen.push(42);
   gen.visitMethodInsn(Opcodes::INVOKESTATIC, Utf8StringRef::from_cstr("Main"),
                       Utf8StringRef::from_cstr("run"), Utf8StringRef::from_cstr("(I)V"), false);
   gen.returnValue();
   gen.endMethod();   // 触发 visitMaxs（自动计算）

AdviceAdapter
^^^^^^^^^^^^^

.. code-block:: cpp

   class TraceAdvice : public AdviceAdapter {
       void onMethodEnter() override { /* 方法入口注入 */ }
       void onMethodExit(int opcode) override { /* 每个 return 前注入 */ }
   };

minidemo
--------

.. code-block:: cpp

   // 类名重映射
   ca::str::Utf8StringArena arena;
   ClassReader              reader(arena, data, len);
   ClassWriter              writer(arena);
   SimpleRemapper remapper(arena,
                           Utf8StringRef::from_cstr("org/old/Foo"),
                           Utf8StringRef::from_cstr("org/new/Foo"));
   ClassRemapper  cr(&writer, &remapper);
   reader.accept(&cr, 0);
   auto out = writer.toByteArray();
