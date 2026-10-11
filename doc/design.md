# libjbytecode 总体设计

> 本文是 **面向新接手者的设计总览**：用最短路径讲清楚「这个库是什么、怎么组织、
> 想做某件事该去哪个头文件找 API」。
> 它**不列举** API 名称与用法——那些写在各头文件的 Doxygen 注释里，以代码为准。
> 想深入某个子模块的内部机制，看同目录下的专题文档（见文末「延伸阅读」）。

---

## 1. 这个库是什么

libjbytecode 是 **OW2 ASM（`org.objectweb.asm`）的 C++17 移植**，提供 Java `.class`
字节码的**解析、生成、分析、变换**四类能力。设计目标是「行为对齐 ASM」：熟悉 ASM 的人
能凭直觉使用，移植代码时能逐类对照（每个头文件注释标注其对应的 ASM Java 源文件）。

几条贯穿全库的基调，先记住它们，后面所有设计都从这里推导：

- **规避 RTTI（约定层面）。** 构建当前不强制 `/GR-`（MSVC）/ `-fno-rtti`，但全库
  约束不使用 `dynamic_cast` / `typeid`：所有「向下转型」一律用
  `ca::core::tag_cast::cast<T>()`（libca core 提供的基于 tag 的安全转型）。
  这影响到指令节点、Value、Attribute 等所有多态层级的类型判别方式。
- **Visitor（访问者）是一切的中枢。** 读、写、变换、打印、校验，全部表达为同一套
  visitor 事件流的生产者/消费者。理解了 visitor 链，就理解了全库的数据流。
- **裸指针 + 调用方负责生命周期。** 树节点、Label、Attribute 等大量用 `new` 裸指针互联，
  库**不做**自动回收。这是对齐 ASM（Java 有 GC）的代价，使用约束集中在 `safety.md`。
- **线程不安全。** 没有任何同步原语，单线程使用或外部加锁。
- **错误用异常。** 解析畸形输入抛 `std::runtime_error` 及子类；写出侧基本不抛。

命名空间统一 `jbytecode`，子模块各占一个子命名空间。统一入口是聚合头
`<jbytecode/jbytecode.hpp>`，也可按需单独包含子模块的头。

---

## 2. 五个子模块与依赖分层

整个库切成五个子模块，每个是一个独立静态子库，聚合库 `libjbytecode` 串联它们：

```
        ┌─────────────────────────────────────────────┐
        │  core   —— 字节码读写的地基（Visitor + 二进制）│
        └─────────────────────────────────────────────┘
                 ▲          ▲          ▲          ▲
       ┌─────────┘   ┌──────┘    ┌─────┘     ┌────┘
       │             │           │           │
   ┌───────┐   ┌──────────┐  ┌────────┐  ┌──────┐
   │ tree  │   │ commons  │  │  util  │  │      │
   └───────┘   └──────────┘  └────────┘  │      │
       ▲                          │      │      │
       └──────────┐               │      │      │
              ┌────────┐          │      │      │
              │analysis│◄─────────┘  （util/analysis 也用 tree）
              └────────┘
```

- **core** 只依赖 libca（基础类型/字符串/SHA-1）与 libjstr。它是地基，不依赖其它子模块。
- **tree / commons / util / analysis** 都建立在 core 之上，并复用 libca 的 `ca::core::tag_cast`。
- **analysis** 还依赖 tree（分析的输入是 tree 的 `MethodNode`）。
- **util** 的字节码校验部分也会用到 analysis。

新接手者的阅读顺序建议：**先 core（尤其是 visitor 与读写两端），再 tree**。这两个吃透了，
commons/util/analysis 都是「在 visitor 链上挂一个特定用途的节点」，看一眼就懂。

各子模块对应的源码根目录（API 头文件都在 `<模块>/src/jbytecode/<模块>/` 下）：

| 子模块 | 源码根 | 命名空间 | 一句话职责 |
|--------|--------|----------|-----------|
| core | `core/src/jbytecode/core/` | `jbytecode::core` | Visitor 模式读写 `.class` 二进制 |
| tree | `tree/src/jbytecode/tree/` | `jbytecode::tree` | 内存中可随机修改的 class 树 |
| commons | `commons/src/jbytecode/commons/` | `jbytecode::commons` | 现成的 visitor 适配器（重映射/生成/插桩…） |
| analysis | `analysis/src/jbytecode/analysis/` | `jbytecode::analysis` | 数据流分析（类型推断/验证/来源追踪） |
| util | `util/src/jbytecode/util/` | `jbytecode::util` | 调试工具（反汇编打印/等价代码生成/校验） |

---

## 3. 核心思想：Visitor 链

理解全库的钥匙只有一个概念：**一次 `.class` 的处理 = 一串 visitor 事件，从生产者流向消费者。**

生产者把 class 的结构「讲」出来——「这是一个类」「它有个方法」「方法里第一条指令是
ALOAD_0」……每个「讲」就是一次 `visitXxx` 调用。消费者实现这些 `visitXxx` 来「听」并
做事：写回二进制、建内存树、打印文本、改名字。

```
┌────────────┐   visitXxx 事件流   ┌─────────────────┐
│  生产者     │ ─────────────────► │   消费者         │
│ ClassReader │                    │ ClassWriter      │  ← 写回二进制
│ ClassNode   │                    │ ClassNode        │  ← 建/改内存树
│ (任何 visitor)│                  │ TraceClassVisitor│  ← 打印
└────────────┘                    └─────────────────┘
```

关键设计有三点：

**(1) 生产者和消费者是同一组接口的两面。** `ClassVisitor` / `MethodVisitor` / `FieldVisitor`
/ `AnnotationVisitor` / `ModuleVisitor` / `RecordComponentVisitor` 这组抽象类，既被消费者
继承（实现 `visitXxx`），也被生产者持有（调用 `visitXxx`）。`ClassReader` 读二进制后调用
visitor；`ClassWriter` 是 visitor、把调用编码成二进制；`tree::ClassNode` 既是 visitor
（能被填充）又是生产者（能 `accept` 回放）。

**(2) 委托（delegate）让 visitor 可以串成链。** 每个 visitor 可持有一个下游 `delegate`，
默认实现把事件原样转发给 delegate（delegate 为空则退化为 no-op）。于是「变换」就是在链中间
插一个**只覆盖自己关心的事件、其余转发**的 visitor：

```
ClassReader ──► ClassRemapper ──► CheckClassAdapter ──► ClassWriter
              （改类名后转发）   （顺带校验后转发）    （编码成字节）
```

这条链里，重映射、校验、写出三件事互不知道彼此，却能叠加生效。commons/util 里的绝大多数
类就是「为某个用途写好的、挂在链中间的 visitor」。

**(3) 层级 visitor 由父级工厂方法按需创建。** 例如 `ClassVisitor::visitMethod` 返回一个
`MethodVisitor*`（可能为 `nullptr` 表示「这个方法我不关心」）。这样链的分叉天然跟随
class 的嵌套结构：类→方法→注解、类→字段→注解、类→record 组件……每一层都能独立决定
是否深入、是否变换。

> 各 visitor 的**事件发射顺序是有契约的**（例如 `visitCode` 必须在指令之前、`visitMaxs`
> 必须在最后）。这套顺序契约写在 `safety.md`，违反会让下游 visitor（尤其 `ClassWriter`）
> 产出错误结果。

---

## 4. 两条主线数据流

库里所有任务都是这两条流的组合：

**A. 直通 / 变换（流式，省内存）**
```
.class bytes → ClassReader.accept → [中间 visitor 链] → ClassWriter → .class bytes
```
一遍流过，不在内存里建完整模型。适合「只改局部、其余照搬」的场景（改类名、插桩、加校验）。
中间想做什么，就往链里插对应的 commons/util visitor。

**B. 建树 / 随机修改（吃内存，能任意改）**
```
.class bytes → ClassReader.accept → tree::ClassNode      （读进内存树）
               ... 任意遍历/增删改 ClassNode/MethodNode/InsnList ...
               ClassNode.accept → ClassWriter → .class bytes   （回放写出）
```
当你需要「看到整个方法的指令列表、前后跳着改」时用树。tree 模块把字节码指令表示成
双向链表 `InsnList`，节点是 `AbstractInsnNode` 的各子类。**分析（analysis）也以树为输入**，
因为它要在指令链上构建控制流图。

> 树节点和 Label 都是裸指针、库不回收，迭代中增删有失效规则——这是 tree/analysis 使用时
> 最容易踩的坑，**动手改 `InsnList` 前先读 `safety.md`**。

---

## 5. 想做某件事，该看哪里

下表是「任务 → 入口子模块 / 头文件位置」的导航。**具体类名、构造参数、方法语义看对应头文件
的 Doxygen 注释**，这里只给方向。

| 我想…… | 去哪个模块 | 头文件大致位置 / 关键词 |
|--------|-----------|------------------------|
| 读 `.class`、拿类名/父类/接口、驱动 visitor | core | `readers.hpp`（`ClassReader`） |
| 写出 `.class`、自动算 maxStack/StackMapTable | core | `writers.hpp`（`ClassWriter`/`MethodWriter`）、`symbol_table.hpp` |
| 实现自己的读/写中间处理 | core | `visitors.hpp`（`ClassVisitor`/`MethodVisitor`/`FieldVisitor` 等一组 visitor 基类） |
| 解析/构造类型描述符 | core | `type.hpp`（`Type`） |
| 操作码常量、ASM/类版本号 | core | `opcodes.hpp` |
| 字节码偏移锚点、跳转目标；`Handle`/`ConstantDynamic`/类型注解目标 | core | `primitives.hpp`（`Label`/`Handle`/`ConstantDynamic`/`TypePath`）、`type_reference.hpp` |
| 自定义/未知属性的读写 | core | `primitives.hpp`（`Attribute`） |
| 把 class 读进可随机修改的内存模型 | tree | `nodes.hpp`（`ClassNode`/`MethodNode`/`FieldNode`） |
| 增删改指令、遍历指令链 | tree | `insn_list.hpp`（`InsnList` 与 `AbstractInsnNode` 子类）、`insn_type_traits.hpp`（节点 tag/转型） |
| 批量改类名/方法名/描述符（重映射） | commons | `remapper.hpp`、`remapper_visitors.hpp`、`signature_remapper.hpp` |
| 用高层 API 生成方法体（而非裸 opcode） | commons | `generator_adapter.hpp`、`instruction_adapter.hpp` |
| 在方法入口/出口注入代码（AOP 插桩） | commons | `advice_adapter.hpp` |
| 插桩后重排局部变量槽位 | commons | `local_variables_sorter.hpp` |
| 实时查询「当前指令处的栈帧」 | commons | `analyzer_adapter.hpp` |
| 内联消除 JSR/RET、合并 `<clinit>`、排序异常表、注入 serialVersionUID | commons | `jsr_inliner_adapter.hpp`、`static_init_merger.hpp`、`try_catch_block_sorter.hpp`、`serial_version_uid_adder.hpp` |
| 做数据流分析 / 类型推断 / 字节码验证 | analysis | `analyzer.hpp`、`interpreter.hpp` + 各 `*_interpreter.hpp`/`*_verifier.hpp` |
| 追踪某个值由哪些指令产生（def-use/死代码） | analysis | `source_interpreter.hpp`、`small_set.hpp` |
| 把 `.class` 反汇编成可读文本 | util | `textifier.hpp`、`trace_class_visitor.hpp` |
| 生成「等价的 ASM 调用代码」 | util | `asmifier.hpp` |
| 在 visitor 链上校验事件序列/参数合法性 | util | `check_adapters.hpp` |

---

## 6. 内存与所有权模型（务必先建立的心智）

ASM 跑在有 GC 的 JVM 上，这个移植没有 GC，所以所有权规则必须显式记住：

- **树节点建点走 arena（B7 起）**：指令节点经 `MethodNode::create<T>()` / `InsnList::create<T>()`
  在归属方法的内容 arena 上分配，寿命 = 方法寿命，析构由 arena 逆序统一执行。
  `ClassNode`/`MethodNode` 等**根对象**仍堆分配、由拥有型容器（或调用方 RAII）释放。
  混合清除纪律：绑定列表析构/`clear`/`set` 对 arena 节点只摘链（arena 统一回收）、
  对堆节点（暂存管线产物）维持容器 `delete`；`remove` 一律只摘链。
  详见 `safety.md` 的失效表。
- **`Label` 走归属制（B8）**：`Label` 带 owner arena 盖章；方法域 label 经
  `MethodNode::createLabel()` 在 `nodeArena_` 建点、随 arena 回收。visitor 边界
  `canonicalizeLabel`：本域 label O(1) 直返，外来 label 拷贝收编（原对象归创建方）。
  旧「label 池 + taken 标志 + onNodeAdded 回调」制度已物理退役。详见 §6.1。
- **向下转型走 `ca::core::tag_cast::cast<T>()`**（libca core），不是 `dynamic_cast`。判别指令/Value 的具体类型时，
  用各类提供的 tag（如指令节点的 `getType()`、Value 的种类枚举）配合 `tag_cast`。
- **字符串用 libca 的语义类型**（`Utf8StringRef` 借用 / `Utf8String` 拥有），不是
  `std::string`。借用型不延长生命周期，注意别让底层 buffer 先析构。
- **常量池由 `SymbolTable` 统一注册去重**，不是各处自己拼。写出侧的常量池序列化是
  两阶段的（先注册收集、再写头和方法体），细节见 `core.md`。

这套模型的完整约束（迭代器失效、accept 事件顺序、线程/异常安全边界）集中在
**`safety.md`——动手写变换前必读那一篇**。

### 6.1 Label 所有权（B8 归属制：owner arena 盖章，禁止跨方法共享）

> 本节为 B8 后现行制度（批次9 已物理退役旧「label 池 + taken + onNodeAdded」方案，
> 迁移过程见 `doc_ai/arena-migration-ledger.md`）。

`Label` 在 core 层（`primitives.hpp`），带 `owner()` 归属锚（指向建点 arena）。
`Label*` 在 JumpInsnNode/LabelNode/TryCatchBlock/Switch/LineNumber/LocalVariable 里
到处被引用，都是**借用**；Label 的生命周期锚定在其 owner arena（即建点方法的方法域）。

**契约**：
1. **建点走 `MethodNode::createLabel()`**：在 `nodeArena_` 上分配并盖章
   `setOwner(&nodeArena_)`。方法域 label 随 arena 析构统一回收，无手工 delete。
2. **visitor 边界收编 `canonicalizeLabel(label)`**：7 个携带 Label 的 visitor 在方法
   边界统一调用。owner == 本方法 arena：O(1) 直返（identity 保持）；外来 label：
   查 `foreignLabels_` 映射，未命中则拷贝收编（`{bytecodeOffset, line, flags}`，
   身份换新）并登记。**原对象归创建方**（reader 的 labelArena_ / 调用方 scratch），
   树不 delete 外来 label。
3. **clone 走 `clone(arena, clonedLabels)` 新协议**：节点在目标 arena 分配，携带
   label 经自由函数 `resolveCloneLabel(arena, map, label)` 解析（同域直返、map 命中
   直返、否则建副本登记）。`MethodNode::resolveCloneLabel` 为其方法域便利封装。
4. **禁止跨方法长期共享 `Label*`**：跨方法传递必须经 canonicalize/clone 收编为目标
   方法域副本。reader 借用窗口有额外约束：MethodWriter 把 Label 当 CFG 节点改写，
   reader 的 label 必须活到 `toByteArray()`（DEV-10，见 `readers.hpp` 借约注释）。
5. **Label 可以是 arena 产物也可以是调用方 scratch 产物**——owner 盖章是判别依据，
   不再要求「必须裸 new」。栈 Label 仍禁止（生命周期不足）。

---

## 7. 构建与测试形态

- 子库目标：`libjbytecode_core` / `_tree` / `_commons` / `_analysis` / `_util`，
  聚合目标 `libjbytecode`。最小可用只需 core（+ libca / libjstr / libgarena 包）。
- 测试默认关闭，`xmake f --with_tests=y` 打开；demo 默认关闭，`--with_demo=y` 打开。
- 每个子模块有独立的 `*_unittest` 目标；core 另有一份与 ASM 参考输出做结构化对拍的回归测试。
- 具体命令见 `readme.md`「构建目标」与「快速开始」。

---

## 8. 延伸阅读（子模块专题与约束）

本文给地图，细节在这些专题文档与头文件注释里：

| 想深入…… | 看 |
|----------|-----|
| Core 的 visitor 链、二遍扫描、常量池两阶段序列化、栈帧计算（COMPUTE_FRAMES） | [core.md](core.md) |
| Tree 的节点结构、指令继承体系、回放机制 | [tree.md](tree.md) |
| Commons 的 Remapper 体系与 Adapter 继承链 | [commons.md](commons.md) |
| Analysis 的 Analyzer + Interpreter 数据流框架 | [analysis.md](analysis.md) |
| Util 的 Printer/Textifier/ASMifier、Trace、Check | [util.md](util.md) |
| **迭代器失效、accept 事件顺序、线程/异常安全（写变换前必读）** | [safety.md](safety.md) |
| 每个类的精确 API、参数、语义、对应的 ASM Java 源 | **各头文件的 Doxygen 注释（以代码为准）** |

