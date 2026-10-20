# libjbytecode

ASM 兼容的独立 C++17 实现：Java `.class` 字节码的解析、生成、分析与变换库（xmake）。

visitor 事件、字节码语义和主要类型命名与 ASM 保持一致，但不是逐行移植。JVM 的 GC 与 finalize 生命周期无法搬进 C++：早期照搬 ASM 裸 `new` 结构的版本只能靠零散 delete 补丁维持，泄漏与重复释放反复出现。本实现改为 arena 持有方法域内的指令节点与 `Label`，根对象交给拥有型容器，跨边界视图一律借用，所有权规则见 [doc/safety.md](doc/safety.md)，与 ASM 的模型差异见 [doc/design.md](doc/design.md)。

- **语言**：C++17，规避 RTTI（向下转型用 libca 的 `ca::core::tag_cast::cast<T>`，不用 `dynamic_cast`）
- **命名空间**：`jbytecode`（子模块 `jbytecode::core` / `::tree` / `::commons` / `::analysis` / `::util`）
- **统一入口**：`#include <jbytecode/jbytecode.hpp>`（聚合头，也可按需单独包含子模块头）
- **线程安全**：所有 API 均线程不安全，单线程使用或外部加锁

## 特性速览

| 能力 | 内容 |
|------|------|
| Core API | Visitor 模式流式读写 `.class`：`ClassReader` / `ClassWriter`、`Opcodes`、`Type`、符号表、栈帧计算、输入净化 |
| Tree API | 内存中 class 树形表示：`ClassNode` / `MethodNode` / `InsnList`，可直接增删改指令后写回 |
| Commons | 常用适配器：`Remapper`、`GeneratorAdapter`、`AdviceAdapter` 等 |
| Analysis | 数据流分析引擎：`Analyzer` + `Interpreter` + `Frame` / `Value` |
| Util | 调试与验证：`Textifier`、`ASMifier`、`TraceClassVisitor`、`CheckAdapter` |

## 架构

```
libjbytecode (聚合静态库)
├── core/      Core API —— Visitor 模式读写 .class
├── tree/      Tree API —— 内存树形表示（ClassNode/MethodNode/InsnList）
├── commons/   实用适配器（Remapper、GeneratorAdapter、AdviceAdapter 等）
├── analysis/  数据流分析引擎（Analyzer、Interpreter、Frame、Value）
└── util/      调试/追踪/验证工具（Printer、Textifier、ASMifier、CheckAdapter、TraceClassVisitor）
```

依赖链：`core`（依赖 libca / libjstr / libgarena 包）→ `tree` / `commons` / `analysis` / `util`（依赖 core，analysis 另依赖 tree）。
聚合库 `libjbytecode` 串联全部子库并对外暴露统一头文件。

## 快速开始

**1. 注册包仓库**（libjbytecode 及其依赖经 [luiox-repo](https://github.com/luiox/luiox-repo) git 直连包分发）：

```sh
xmake repo --add luiox-repo https://github.com/luiox/luiox-repo.git
```

**2. 在你的 `xmake.lua` 中引入**（版本锁写法参考本仓库根 `xmake.lua`）：

```lua
add_requires("libjbytecode")

target("myapp")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("libjbytecode")
```

**3. 最小示例** —— 读取 `.class`、遍历指令、写回：

```cpp
#include <jbytecode/jbytecode.hpp>
using namespace jbytecode;
using namespace jbytecode::core;

// arena 借宿常量池解码出的 UTF-8 字符串，生命周期需覆盖 reader/writer
ca::str::Utf8StringArena arena;

std::vector<ca::u8> bytes = /* 自行读入 .class 字节 */;

// 直通回放：读取 → 写出
ClassReader reader(arena, bytes.data(), (ca::i32)bytes.size());
ClassWriter writer(arena);
reader.accept(&writer, /*parsingOptions=*/0);
std::vector<ca::u8> out = writer.toByteArray();

// Tree API：读取 → 修改 → 写回
tree::ClassNode cn;
reader.accept(&cn, 0);
for (auto* m : cn.methods) {
    for (auto* insn = m->instructions.getFirst(); insn; insn = insn->getNext()) {
        // 遍历指令，如 insn->getOpcode()
    }
}
auto*           method = cn.methods[0];
tree::InsnList& insns  = method->instructions;
// 指令节点在方法 arena 上分配，寿命随 MethodNode；不要手工 new 后挂入
insns.insert(insns.getFirst(), insns.create<tree::InsnNode>(Opcodes::ACONST_NULL));

ClassWriter treeWriter(arena);
cn.accept(&treeWriter);
std::vector<ca::u8> modified = treeWriter.toByteArray();
```

## 构建与测试

本仓库自建（xmake ≥ 3.0，依赖包经 luiox-repo git 直连解析，gtest 走 xrepo）：

```sh
xmake repo --add luiox-repo https://github.com/luiox/luiox-repo.git
xmake f --with_tests=y -y     # 配置（gtest 仅测试需要；demo 另加 --with_demo=y）
xmake build libjbytecode      # 聚合库
# 测试 target 默认关闭，按需逐个构建（xmake build 一次只接受一个 target）
xmake build libjbytecode_asm_core_unittest
xmake build libjbytecode_tree_unittest
xmake build libjbytecode_analysis_unittest
xmake build libjbytecode_commons_unittest
xmake build libjbytecode_util_unittest
xmake build libjbytecode_core_unittest
xmake run libjbytecode_asm_core_unittest
```

构建目标：

| 目标 | 类型 | 说明 |
|------|------|------|
| `libjbytecode_core` / `_tree` / `_commons` / `_analysis` / `_util` | 静态库 | 各子模块独立子库 |
| `libjbytecode` | 静态库 | 聚合库（统一头 + 串联子库） |
| `libjbytecode_asm_core_unittest` / `_tree_unittest` / `_commons_unittest` / `_analysis_unittest` / `_util_unittest` | 测试 | 子模块单元测试（默认关闭，`--with_tests=y`） |
| `libjbytecode_core_unittest` | 测试 | ASM 扩展与集成测试（依赖全部子库，默认关闭） |
| `demo_*` | 可执行 | 各类用法示例（默认关闭，`--with_demo=y`） |

测试纪律（资源来源、round-trip 契约边界、`skip` 使用限制）见
[doc/safety.md](doc/safety.md#round-trip-验证边界)。

## 依赖

| 库 | 用于 | 用途 |
|----|------|------|
| libca | 全部子库 | `ca::i32` / `ca::u8` 基础类型、`Utf8StringRef` 语义字符串、`ca::core::tag_cast` 向下转型、SHA-1（`SerialVersionUIDAdder`）、log 宏、`ca::test` 测试布局 |
| libjstr | core | MUTF-8 / WTF-8 字符串转换 |
| libgarena | core | arena 内存底座 |
| libjzip | demo | Jar 读写（仅 `--with_demo=y`） |
| gtest | 测试 | 单元测试（仅 `--with_tests=y`，走 xrepo） |

## 状态与 License

- **开发中，未定版**：仓库不打 tag，API 可能调整；包版本以 [luiox-repo](https://github.com/luiox/luiox-repo) 的 `packages/l/libjbytecode/xmake.lua` 版本锁为准，本仓库 `xmake.lua` 固定其依赖版本。
- 依赖包 libjstr / libgarena / libjzip 均为公开仓库。
- 基于 [MIT License](LICENSE) 发布。

## 深入阅读

| 文档 | 内容 |
|------|------|
| [doc/design.md](doc/design.md) | **总体设计总览（新接手者先读这篇）**：库定位、五模块分层、Visitor 链核心思想、两条主线数据流、「想做某事去哪找 API」导航 |
| [doc/core.md](doc/core.md) | Core API 设计：Visitor 链、二遍扫描、常量池序列化、栈帧计算 |
| [doc/tree.md](doc/tree.md) | Tree API 设计：节点结构、指令系统、回放机制 |
| [doc/commons.md](doc/commons.md) | Commons 设计：Remapper 体系、Adapter 继承链 |
| [doc/analysis.md](doc/analysis.md) | Analysis 设计：Analyzer + Interpreter 数据流框架 |
| [doc/util.md](doc/util.md) | Util 设计：Printer/Textifier/ASMifier、Trace、CheckAdapter |
| [doc/safety.md](doc/safety.md) | 安全使用指南：迭代器失效、accept 链契约、线程/异常安全 |
