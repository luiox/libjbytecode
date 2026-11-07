.. _tree:

Tree API 设计
=============

状态：定稿。节点结构、指令继承体系与回放机制。

概述
----

Tree API 在 Core API 之上提供内存中的完整 class 树形表示（``jbytecode::tree``\）\，支持通过 ``accept(ClassVisitor*)`` 回放到 Core API 的 Visitor 链。对标 ASM 的 ``org.objectweb.asm.tree``\。

.. _tree-nodes:

类节点结构
----------

``ClassNode`` 是树根\，持有字段/方法/模块/record 组件等子节点（子节点本体为堆对象\，由 ``ClassNode`` 析构释放\；类级注解等直接内容物走类的内容 arena）\：

.. code-block:: text

   ClassNode
   ├── version / access / name / signature / superName / interfaces
   ├── vector<FieldNode*>             fields
   ├── vector<MethodNode*>            methods
   ├── ModuleNode*                    moduleNode
   ├── vector<RecordComponentNode*>   recordComponents
   ├── vector<AnnotationNode*>        visible/invisibleAnnotations 等
   └── vector<InnerClassNode*>        innerClasses

   MethodNode
   ├── access / name / desc / signature / exceptions
   ├── InsnList                       instructions   （绑定方法域 owner arena）
   ├── vector<TryCatchBlockNode*>     tryCatchBlocks
   ├── vector<LocalVariableNode*>     localVariables
   └── int                            maxStack, maxLocals

   FieldNode
   └── access / name / desc / signature / value

``ClassNode`` / ``MethodNode`` / ``FieldNode`` 都是 visitor（能被 ``ClassReader`` 填充）\，也都能 ``accept`` 回放。

.. _tree-insn-hierarchy:

指令节点继承体系
----------------

所有指令节点继承 ``AbstractInsnNode``\（持有 ``opcode`` 与双向链指针 ``previousInsn``/``nextInsn``\）\，按指令形态分 16 个子类\：

.. code-block:: text

   AbstractInsnNode
   ├── InsnNode                （无操作数：NOP/ACONST_NULL/IRETURN…）
   ├── IntInsnNode             （一个 int 操作数：BIPUSH/SIPUSH/NEWARRAY）
   ├── VarInsnNode             （局部变量：ILOAD/ASTORE/RET…）
   ├── TypeInsnNode            （类型描述符：NEW/ANEWARRAY/CHECKCAST/INSTANCEOF）
   ├── FieldInsnNode           （owner+name+desc：GETFIELD/PUTSTATIC…）
   ├── MethodInsnNode          （owner+name+desc+itf：INVOKEVIRTUAL…）
   ├── InvokeDynamicInsnNode   （name+desc+bsm+bsmArgs）
   ├── JumpInsnNode            （core::Label* 跳转目标）
   ├── LabelNode               （core::Label* 锚点）
   ├── LdcInsnNode             （常量装载）
   ├── IincInsnNode            （var+incr）
   ├── TableSwitchInsnNode     （min+max+dflt+labels）
   ├── LookupSwitchInsnNode    （dflt+keys+labels）
   ├── MultiANewArrayInsnNode  （desc+dims）
   ├── FrameNode               （frameType+local+stack）
   └── LineNumberNode          （line+start）

节点类型判别不走 ``dynamic_cast``\：用 ``getType()`` 返回的 tag 配 ``insn_type_traits.hpp`` 的 ``TypeOf<T>`` 映射与 ``ca::core::tag_cast`` 转型（见 :ref:`design-positioning`\）。

.. _tree-insn-list:

InsnList
--------

``InsnList`` 是双向链表\，节点由 ``previousInsn`` / ``nextInsn`` 链接。提供索引访问（O(n)）与迭代器遍历。**迭代中修改链表需遵守失效规则**\，详见 :ref:`safety-iterator`\。

主要操作（增删改的 arena/堆归属纪律见 :ref:`safety-lifecycle`\）\：

.. code-block:: cpp

   class InsnList {
   public:
       void add(AbstractInsnNode* n);                              // 尾插
       void insert(AbstractInsnNode* n);                           // 头插
       void insert(AbstractInsnNode* before, AbstractInsnNode* n); // 插到 before 之前
       void insertBefore(AbstractInsnNode* where, AbstractInsnNode* n); // 同上（别名）
       void set(AbstractInsnNode* loc, AbstractInsnNode* n);       // 原位替换（arena 节点只摘链，堆节点 delete）
       void remove(AbstractInsnNode* n);                           // 只摘链，不销毁
       void removeAndDelete(AbstractInsnNode* n);                  // 摘链并销毁堆节点（arena 节点只摘链）
       template <class T, class... Args>
       T* create(Args&&... args);                                  // 在 owner arena 上建节点
       AbstractInsnNode* getFirst() const;  AbstractInsnNode* getLast() const;
       AbstractInsnNode* get(int index) const;
       int size() const;
       void accept(jbytecode::core::MethodVisitor* mv) const;      // 顺序回放
       std::vector<AbstractInsnNode*> toArray() const;             // 快照（迭代修改用）
   };

.. _tree-replay:

回放机制
--------

``ClassNode::accept`` 与 ``ClassReader::accept`` 的事件顺序并不完全相同\；visitor 若依赖事件顺序\，必须遵守 :ref:`safety-accept-contract`\。当前回放要求完整保留 class 结构和指令语义\，不承诺输出字节与 ASM Java 完全一致。

回放路径\：

1. ``ClassNode::accept`` 依序发射类级事件（version/access/name → source → module → 注解 → 属性 → innerClasses → recordComponents），fields/methods 在构建期已由 ``visitField``/``visitMethod`` 回调建好节点\，回放时不再重复发射。
2. ``MethodNode::accept`` 先发射注解/参数类事件\，再 ``visitCode``\，然后按链序回放 ``InsnList``\，最后 ``visitMaxs`` + ``visitEnd``\。
3. 下游 ``ClassWriter`` 在 ``toByteArray()`` 时做两阶段序列化（见 :ref:`core-symbol-table`\）。

.. _tree-api:

关键 API
--------

ClassNode
^^^^^^^^^

.. code-block:: cpp

   tree::ClassNode cn;
   reader.accept(&cn, /*parsingOptions=*/0);   // 从 ClassReader 填充
   cn.accept(&writer);                          // 回放到 ClassVisitor

MethodNode
^^^^^^^^^^

.. code-block:: cpp

   class MethodNode : public MethodVisitor {
   public:
       InsnList instructions;
       std::vector<TryCatchBlockNode*> tryCatchBlocks;
       std::vector<LocalVariableNode*> localVariables;
       int maxStack, maxLocals;
       void accept(MethodVisitor* mv);   // 顺序回放指令 + 属性
   };

``MethodNode`` 持有方法域 ``nodeArena_``\：指令/注解等内容经 ``create<T>()`` 建点、随方法析构统一回收（见 :ref:`design-memory-model`\）。

minidemo
--------

.. code-block:: cpp

   // 读取到树、修改、回写
   ca::str::Utf8StringArena arena;
   tree::ClassNode          cn;
   ClassReader              reader(arena, data, len);
   reader.accept(&cn, 0);

   // 在方法开头插入指令：节点由方法 arena 分配，寿命随 MethodNode
   tree::MethodNode* m = cn.methods[0];
   m->instructions.insert(m->instructions.getFirst(),
       m->instructions.create<tree::InsnNode>(Opcodes::ACONST_NULL));

   // 回写
   ClassWriter writer(arena);
   cn.accept(&writer);
   auto result = writer.toByteArray();
