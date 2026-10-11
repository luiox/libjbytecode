# libjbytecode

OW2 ASM 的 C++ 移植，提供 Java `.class` 字节码的解析、生成、分析与变换能力。

- **语言**：C++17，规避 RTTI（向下转型用 libca 的 `ca::core::tag_cast::cast<T>`，不用 `dynamic_cast`）
- **构建系统**：Xmake（≥ 3.0）
- **命名空间**：`jbytecode`（子模块 `jbytecode::core` / `::tree` / `::commons` / `::analysis` / `::util`）
- **统一入口**：`#include <jbytecode/jbytecode.hpp>`（聚合头文件，可按需单独包含子模块）
- **线程安全**：所有 API 均线程不安全，单线程使用或外部加锁

## 架构

```
libjbytecode (聚合静态库)
├── core/      Core API —— Visitor 模式读写 .class（ClassReader/Writer、Opcodes、Type、SymbolTable、ClassFileSanitizer 输入净化 #963）
├── tree/      Tree API —— 内存中 class 树形表示（ClassNode/MethodNode/InsnList）
├── commons/   实用适配器（Remapper、GeneratorAdapter、AdviceAdapter 等）
├── analysis/  数据流分析引擎（Analyzer、Interpreter、Frame、Value）
└── util/      调试/追踪/验证工具（Printer、Textifier、ASMifier、CheckAdapter、TraceClassVisitor）
```

依赖链：`core`（依赖 libca / libjstr / libgarena 包）→ `tree`/`commons`/`analysis`/`util`（依赖 core）。
聚合库 `libjbytecode` 串联全部子库并对外暴露统一头文件。

## 构建

xmake ≥ 3.0。libca / libjstr / libgarena / libjzip 经
[luiox-repo](https://github.com/luiox/luiox-repo) git 直连包解析，gtest 走 xrepo。
首次构建先注册包仓库：

```sh
xmake repo --add luiox-repo https://github.com/luiox/luiox-repo.git
xmake f --with_tests=y -y     # 配置（gtest 仅测试需要；demo 另加 --with_demo=y）
xmake build libjbytecode      # 聚合库
xmake build libjbytecode_asm_core_unittest
xmake run libjbytecode_asm_core_unittest
```

## 构建目标

| 目标 | 类型 | 说明 |
|------|------|------|
| `libjbytecode_core` / `_tree` / `_commons` / `_analysis` / `_util` | 静态库 | 各子模块独立子库 |
| `libjbytecode` | 静态库 | 聚合库（统一头 + 串联子库） |
| `libjbytecode_asm_core_unittest` / `_tree_unittest` / `_commons_unittest` / `_analysis_unittest` / `_util_unittest` | 测试 | 子模块单元测试（默认关闭，`--with_tests=y`） |
| `libjbytecode_origin_unittest` | 测试 | core 单元测试（同 asm_core 源，独立 target） |
| `demo_*` | 可执行 | 各类用法示例（默认关闭，`--with_demo=y`） |

```bash
# 最小构建（libca / libjstr / libgarena 由 luiox-repo 包自动拉取）
xmake build libjbytecode_core

# 完整库 + 测试
xmake f --with_tests=y
xmake build libjbytecode && xmake run libjbytecode_asm_core_unittest
```

## 测试纪律

- 测试资源由仓库外的生成脚本（ASM 参考回放）产生，生成过程只写
  `test/`，不得直接写入 `test_resource/`。
- 对齐型测试的期望值必须来自参考实现、预编译样本，或可自证的生成型 probe。
- 默认 round-trip 契约是结构与指令语义一致；常量池顺序、属性布局和等价帧编码不要求
  与 ASM Java 逐字节一致。具体边界见 [doc/safety.md](doc/safety.md#round-trip-验证边界)。
- 仓库内理应提交的 `test_resource/` / `test_resource/origin/` fixture 若缺失，测试应直接失败，不能用 `skip` 掩盖资源不完整。
- `skip` 只允许用于明确的外部环境前提不满足，并且必须打印具体原因；“路径不存在”“样本未提交”不属于合法 `skip` 理由。
- 占位断言、弱化断言、或“找不到合适资源就跳过”的测试，都应视为待清理技术债，而不是可接受现状。

## 依赖

| 库 | 用于 | 用途 |
|----|------|------|
| libca | 全部子库 | `ca::i32` / `ca::u8` 基础类型、`Utf8StringRef` 语义字符串、`ca::core::tag_cast` 向下转型、SHA-1（`SerialVersionUIDAdder`）、log 宏、`ca::test` 测试布局 |
| libjstr | core | MUTF-8 / WTF-8 字符串转换 |
| libgarena | core | arena 内存底座 |
| libjzip | demo | Jar 读写（仅 `--with_demo=y`） |
| gtest | 测试 | 单元测试（仅 `--with_tests=y`，走 xrepo） |

## 快速开始

```cpp
#include <jbytecode/jbytecode.hpp>
using namespace jbytecode;
using namespace jbytecode::core;

// arena 借宿常量池解码出的 UTF-8 字符串，生命周期需覆盖 reader/writer
ca::str::Utf8StringArena arena;

// 直通回放：读取 → 写出
std::vector<ca::u8> bytes = /* 读 .class */;
ClassReader reader(arena, bytes.data(), (ca::i32)bytes.size());
ClassWriter writer(arena);
reader.accept(&writer, /*parsingOptions=*/0);
std::vector<ca::u8> out = writer.toByteArray();

// Tree API：读取 → 修改 → 写回
tree::ClassNode cn;
reader.accept(&cn, 0);
auto* m = cn.methods[0];
m->instructions.insert(m->instructions.getFirst(), new tree::InsnNode(Opcodes::ACONST_NULL));
cn.accept(&writer);
```

## 文档

| 文档 | 内容 |
|------|------|
| [doc/design.md](doc/design.md) | **总体设计总览（新接手者先读这篇）**：库定位、五模块分层、Visitor 链核心思想、两条主线数据流、「想做某事去哪找 API」导航 |
| [doc/core.md](doc/core.md) | Core API 设计：Visitor 链、二遍扫描、常量池序列化、栈帧计算 |
| [doc/tree.md](doc/tree.md) | Tree API 设计：节点结构、指令系统、回放机制 |
| [doc/commons.md](doc/commons.md) | Commons 设计：Remapper 体系、Adapter 继承链 |
| [doc/analysis.md](doc/analysis.md) | Analysis 设计：Analyzer + Interpreter 数据流框架 |
| [doc/util.md](doc/util.md) | Util 设计：Printer/Textifier/ASMifier、Trace、CheckAdapter |
| [doc/safety.md](doc/safety.md) | 安全使用指南：迭代器失效、accept 链契约、线程/异常安全 |
