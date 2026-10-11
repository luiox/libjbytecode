# libjbytecode 安全使用指南

本文件记录跨模块共用的安全约束：迭代器失效、accept 链事件契约、线程/异常安全。

## InsnList 迭代器失效规则

`InsnList::Iterator` 是原始指针遍历器，**无失效检测**。各操作对活跃迭代器的影响：

| 操作 | 对 `*it`（当前节点） | 对 `it` 后续遍历 | 对其他迭代器 |
|------|---------------------|-----------------|-------------|
| `remove(node)` | 若 node == `*it`：`++it` 到达已脱离节点，遍历提前终止 | 已脱离节点不会再次访问 | 安全 |
| `set(old, new)` | 若 old == `*it`：**use-after-free**（set() delete 旧节点，`*it` 悬垂） | 新节点在正确位置，但迭代器指向已释放内存 | 安全 |
| `insert(before, node)` | node 插在 `*it` 之前时不会被当前遍历访问到 | 后续遍历正常 | 安全 |
| `clear()` | **所有迭代器失效**（全部节点 delete） | 同左 | 全部失效 |

### 安全迭代模式

```cpp
// 推荐：toArray 快照法（O(n) 内存，安全）
for (auto* node : insnList.toArray()) {
    if (shouldRemove(node)) { insnList.remove(node); delete node; }
    if (shouldSet(node, newNode)) { insnList.set(node, newNode); } // 迭代的是副本，安全
}

// 推荐：双向链表快照法（O(1) 额外内存，安全）
auto* node = insnList.getFirst();
while (node) {
    auto* next = node->getNext(); // 先快照
    if (shouldRemove(node)) { insnList.remove(node); delete node; }
    node = next; // 从快照前进
}
```

### 不安全模式

```cpp
// 危险：迭代中 set 当前节点
for (auto it = list.begin(); it != list.end(); ++it) {
    list.set(*it, newNode); // BUG: *it 被 delete，++it 读已释放内存
}

// 危险：remove 后未快照 next
for (auto it = list.begin(); it != list.end(); ++it) {
    list.remove(*it); // BUG: remove 将 node->nextInsn 置 null
    // ++it → current_ = nullptr → 循环提前结束
}
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

libjbytecode 的默认 reader -> tree -> writer 契约是：重新读取输出后，class metadata、
field/method identity、指令类型与 opcode、maxStack/maxLocals、异常表和局部变量表保持一致。

以下内容不属于默认契约：

- 与 ASM Java 相同的常量池条目顺序；
- 等价属性的相同排列；
- StackMapTable 使用完全相同的等价编码；
- 未修改 class 的输出字节与输入逐字节相同。

这些二进制差异不能单独判定为功能缺陷。若新增 byte-exact 场景，必须单独定义比较对象、
允许差异和 JVM verify/行为验收，不能弱化现有结构回归断言。测试资源和 ASM 参考回放的
生成工具不在本仓库内。

## 已知限制

1. **JSR/RET 子程序指令** — `COMPUTE_FRAMES` 模式遇到 JSR/RET 抛
   `std::invalid_argument`（与 ASM 相同限制）。可用 `JsrInlinerAdapter` 先内联消除。
2. **COMPUTE_FRAMES 公共父类精度** — 默认 `DefaultCommonSuperClassProvider` 只内置
   JCL 常见类继承表（Number/Throwable/Exception 等），第三方库类层次需注入自定义
   `ICommonSuperClassProvider`。详见 [core.md 栈帧计算](core.md#栈帧计算compute_frames)。
3. **`ReferenceAlignmentTest.issue307600`** —— 对输入 class 做结构化 round-trip 对比，
   当前断言全 0 差异
   （field/method/instruction/maxStack/handler/localVariable 均须匹配）。若该测试失败，
   说明 reader↔writer 往返出现结构差异，应优先排查字节码编码层。

## 线程安全

全部 API 均为线程不安全。`ClassNode`、`ClassWriter`、`MethodNode`、`InsnList` 等所有类
都不包含同步原语。单线程中使用，或在外部加锁。

## 异常安全

| 组件 | 异常行为 |
|------|---------|
| `ClassReader::accept` | 遇到截断属性、越界 CP 索引、未知 CP 标签时抛 `std::runtime_error` |
| `ClassWriter::toByteArray` | 不抛异常（内部 ByteVector 自动扩容） |
| `SymbolTable::add*` | 不抛异常 |
| `InsnList` 操作 | 不抛异常 |
| 构造/析构函数 | 不抛异常 |
