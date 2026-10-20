# libjbytecode 安全使用指南

本文件记录跨模块共用的安全约束：节点生命周期、迭代器失效、accept 链事件契约、线程与异常安全。

## 节点生命周期

Tree API 的指令节点有两种来源：

1. `MethodNode::instructions` 绑定了 owner arena，节点应由 `instructions.create<T>(...)` 分配（或 `garena::Arena::create<T>()` 后挂入同一 arena 的列表），寿命等于方法域，arena 统一析构；
2. 未绑定 arena 的临时列表（scratch / holding）使用 `new` 分配，容器负责 `delete`。

`InsnList` 的删除型操作按节点实际归属分别处理：arena 节点只摘链，堆节点立即 `delete`。判别依据是 `ownerArena_->owns(node)`。跨域挂入（把堆节点放进 arena 列表，或把 arena 节点放进临时列表）会绕过该判别：前者在清理时泄漏，后者会对 bump 内存调用 `delete` 并破坏堆。新增建点一律走 `owner.create<T>()`，不要手工 `new` 后挂入 arena 列表。

`remove(node)` 只摘链，不销毁任何节点：堆节点此后由调用方负责释放或重新挂入，arena 节点无需处理。需要「摘除即弃」时用 `removeAndDelete(node)`。

## InsnList 迭代器失效规则

`InsnList::Iterator` 是原始指针遍历器，release 构建无失效检测。debug 构建中经 `begin()`/`end()` 取得的迭代器会登记活跃计数，`set`、`removeAndDelete`、`clear` 在计数非零时断言失败。各操作对活跃迭代器的影响：

| 操作 | 对 `*it`（当前节点） | 对 `it` 后续遍历 | 对其他迭代器 |
|------|---------------------|-----------------|-------------|
| `remove(node)` | node 仍存活；若 node == `*it`，其 `nextInsn` 已被置空，`++it` 提前结束 | 已摘链节点不会再次访问 | 安全 |
| `set(old, new)` | old 被摘链；堆节点被 `delete`（`*it` 悬垂），arena 节点保留 | 堆节点：`++it` 读已释放内存；arena 节点：`old->nextInsn` 未清空，可继续前进 | 安全 |
| `removeAndDelete(node)` | 堆节点被 `delete`，arena 节点只摘链 | 堆节点：同 `set`；arena 节点：`nextInsn` 已清空，提前结束 | 安全 |
| `insert(before, node)` | node 插在 `*it` 之前时不会被当前遍历访问 | 后续遍历正常 | 安全 |
| `clear()` | 堆节点全部 `delete`，arena 节点全部摘链 | 堆节点：悬垂；arena 节点：链已断，提前结束 | 全部失效 |

结论：任何会删除堆节点的操作都不能与活跃迭代器同时使用；arena 列表虽然不会释放内存，但 debug 断言同样会拦截。遍历中需要增删改时，先取快照。

### 安全迭代模式

```cpp
// 推荐：toArray 快照法（O(n) 内存，安全）
for (auto* node : insnList.toArray()) {
    if (shouldRemove(node)) insnList.removeAndDelete(node);
    else if (shouldReplace(node)) insnList.set(node, makeReplacement(node));
}

// 推荐：双向链表快照法（O(1) 额外内存，安全）
auto* node = insnList.getFirst();
while (node) {
    auto* next = node->getNext(); // 先快照
    if (shouldRemove(node)) insnList.removeAndDelete(node);
    node = next; // 从快照前进
}
```

### 不安全模式

```cpp
// 危险：迭代中 set 当前节点（debug 断言；release 下堆节点 UAF）
for (auto it = list.begin(); it != list.end(); ++it) {
    list.set(*it, newNode);
}

// 危险：remove 后未快照 next（nextInsn 已置空，遍历提前终止）
for (auto it = list.begin(); it != list.end(); ++it) {
    list.remove(*it);
}

// 危险：对 arena 节点手工 delete（堆损坏）
list.remove(node);
delete node; // arena 节点由 arena 析构，remove 后无需也不能 delete
```

## Accept 链契约

### ClassNode::accept(ClassVisitor*) 事件顺序

```
visit(version, access, name, sig, super, interfaces)
  → visitSource                  (if non-empty)
  → moduleNode->accept(cv)       (if present)
  → visitNestHost                (if non-empty)
  → visitOuterClass              (if non-empty)
  → [visible/invisible annotations]     visitAnnotation / visitTypeAnnotation
  → [custom attributes]          visitAttribute
  → [nest members]               visitNestMember
  → [permitted subclasses]       visitPermittedSubclass
  → [inner classes]              visitInnerClass
  → [record components]          rc->accept(cv)
  → visitEnd()
```

**注意：** fields/methods 在构建期由 `ClassVisitor::visit` 基类的 `visitField`/`visitMethod`
回调创建节点，accept 时**不会**重新发射 fields/methods。此顺序与 ClassReader 发射的
顺序不同（ClassReader 的 visitSource/visitOuterClass 在 fields/methods 之前）。对
`ClassWriter` 等顺序不敏感的 visitor，这不是问题。

### MethodNode::accept(ClassVisitor*) / accept(MethodVisitor*) 事件顺序

```
[parameters] visitParameter
[annotationDefault] visitAnnotationDefault
[visibleAnnotations] visitAnnotation
[invisibleAnnotations] visitAnnotation
[visibleTypeAnnotations] visitTypeAnnotation (含 typeRef/typePath)
[invisibleTypeAnnotations] visitTypeAnnotation
[visibleParameterAnnotations] visitParameterAnnotation
[invisibleParameterAnnotations] visitParameterAnnotation
[visibleLocalVariableAnnotations] visitLocalVariableAnnotation
[invisibleLocalVariableAnnotations] visitLocalVariableAnnotation
visitCode()
  → [tryCatchBlocks] visitTryCatchBlock
  → [instructions] InsnList::accept(mv)
  → [localVariables] visitLocalVariable
  → visitMaxs(maxStack, maxLocals)
  → visitEnd()
```

## Round-trip 验证边界

libjbytecode 的默认 reader -> tree -> writer 契约是：重新读取输出后，class metadata、field/method identity、指令类型与 opcode、maxStack/maxLocals、异常表和局部变量表保持一致。

以下内容不属于默认契约：

- 与 ASM Java 相同的常量池条目顺序；
- 等价属性的相同排列；
- StackMapTable 使用完全相同的等价编码；
- 未修改 class 的输出字节与输入逐字节相同。

这些二进制差异不能单独判定为功能缺陷。若新增 byte-exact 场景，必须单独定义比较对象、允许差异和 JVM verify/行为验收，不能弱化现有结构回归断言。测试资源和 ASM 参考回放的生成工具不在本仓库内。

## 已知限制

1. **JSR/RET 子程序指令** — `COMPUTE_FRAMES` 模式遇到 JSR/RET 抛 `std::invalid_argument`（与 ASM 相同限制）。可用 `JsrInlinerAdapter` 先内联消除。
2. **COMPUTE_FRAMES 公共父类精度** — 默认 `DefaultCommonSuperClassProvider` 只内置 JCL 常见类继承表（Number/Throwable/Exception 等），第三方库类层次需注入自定义 `ICommonSuperClassProvider`。详见 [core.md 栈帧计算](core.md#栈帧计算compute_frames)。
3. **`ReferenceAlignmentTest.issue307600`** —— 对输入 class 做结构化 round-trip 对比，当前断言全 0 差异（field/method/instruction/maxStack/handler/localVariable 均须匹配）。若该测试失败，说明 reader↔writer 往返出现结构差异，应优先排查字节码编码层。

## 线程安全

全部 API 均为线程不安全。`ClassNode`、`ClassWriter`、`MethodNode`、`InsnList` 等所有类都不包含同步原语。单线程中使用，或在外部加锁。

## 异常安全

| 组件 | 异常行为 |
|------|---------|
| `ClassReader` 构造 / `accept` | 文件打不开、魔数/版本非法、截断、越界 CP 索引、未知 CP 标签、注解或 ConstantDynamic 嵌套超限时抛 `std::invalid_argument` |
| `ClassWriter::toByteArray` | 常量池条目超过 65535 抛 `ClassTooLargeException`，单个方法 Code 超过 65535 字节抛 `MethodTooLargeException`（两者均派生自 `std::length_error`）；`COMPUTE_FRAMES` 不收敛抛 `FrameComputationException` |
| `SymbolTable::add*` | 常量 MUTF-8 字节数超过 65535 时抛 `std::length_error`（来自 `ByteVector::putMUtf8`） |
| `InsnList` 操作 | 删除型操作不抛异常；`toArray` 等分配操作可能抛 `std::bad_alloc`；debug 断言失败走 `assert` 终止而非异常 |
| 全部组件 | 容器扩容、arena 分配、字符串转换的堆分配失败可能抛 `std::bad_alloc` / `std::length_error`，不转换为库自定义异常 |
