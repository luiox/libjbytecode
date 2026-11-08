.. _util:

Util 设计
=========

状态：定稿。Printer 模式与 Trace/Check 工具族。

概述
----

Util（``jbytecode::util``\）提供调试/追踪/验证工具\，基于 Printer 模式。对标 ASM 的 ``org.objectweb.asm.util``\。

.. _util-printer:

Printer 模式
------------

``Printer`` 是一个**与 Visitor 镜像的事件接收器**\：每个 ``visitXxx`` 对应一个虚方法\，收集文本到公开成员 ``text``\（按行的 ``Utf8StringRef`` 视图）与累积器 ``stringBuilder``\。``Textifier`` / ``ASMifier`` / ``CodeGen`` 是 Printer 的三个具体实现。

.. code-block:: text

   ClassReader → TraceClassVisitor ─┬─ 委托写 → ClassWriter（或任意下游 visitor）
                                    └─ 委托打印 → Textifier / ASMifier（Printer）
                                                    → 可读文本 / 等价调用代码

.. _util-classes:

类关系
------

.. list-table::
   :header-rows: 1

   * - 类
     - 作用
   * - ``Printer``
     - 事件 → 文本的抽象基类（visit/visitField/visitMethod … 与 Visitor 镜像）
   * - ``Textifier``
     - 生成**人类可读**的字节码清单（指令、属性、注解）
   * - ``ASMifier``
     - 生成**等价 ASM API 调用**的源码
   * - ``CodeGen``
     - 代码生成辅助\：从 visit 事件生成带变量名的等价调用代码\，label 经内部 arena 命名为 ``label<N>``
   * - ``TraceClassVisitor``
     - ClassVisitor 装饰器\：同时转发到真实 visitor 和 Printer
   * - ``TraceSignatureVisitor``
     - 解析并打印泛型签名
   * - ``CheckClassAdapter`` / ``CheckMethodAdapter`` / ``CheckFieldAdapter`` / ``CheckAnnotationAdapter`` / ``CheckModuleAdapter`` / ``CheckRecordComponentAdapter`` / ``CheckSignatureAdapter``
     - 运行时校验事件序列合法性（非法 visit 顺序、非法参数\，抛 ``std::invalid_argument``\）
   * - ``CheckFrameAnalyzer``
     - 用 Analysis 引擎（``BasicVerifier``\）做字节码验证

.. _util-api:

关键 API
--------

TraceClassVisitor（打印字节码）
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

   ca::str::Utf8StringArena arena;
   ClassReader              reader(arena, data, len);
   ClassWriter              writer(arena);

   util::Textifier          textifier;
   util::TraceClassVisitor  tcv(&writer, &textifier);   // cv + printer
   reader.accept(&tcv, 0);

   // 文本在 textifier.text（按行）或 textifier.stringBuilder（累积）
   for (const auto& line : textifier.text) {
       std::cout << line.to_std_string() << '\n';
   }

ASMifier（生成等价代码）
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

   ca::str::Utf8StringArena arena;
   ClassReader              reader(arena, data, len);
   util::ASMifier           asmifier;
   util::TraceClassVisitor  tcv(/*cv=*/nullptr, &asmifier);
   reader.accept(&tcv, 0);

   // asmifier.text = 等价的 ASM API 调用序列

CheckClassAdapter（验证）
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

   ca::str::Utf8StringArena arena;
   ClassReader              reader(arena, data, len);
   ClassWriter              writer(arena);
   util::CheckClassAdapter  cca(&writer);   // 非法事件序列/参数抛 std::invalid_argument
   reader.accept(&cca, 0);

minidemo
--------

.. code-block:: cpp

   // 打印任意 .class 的可读清单
   ca::str::Utf8StringArena arena;
   ClassReader              reader(arena, data, len);
   util::Textifier          textifier;
   util::TraceClassVisitor  tcv(nullptr, &textifier);
   reader.accept(&tcv, /*parsingOptions=*/0);
   for (const auto& line : textifier.text) {
       std::cout << line.to_std_string() << '\n';
   }
