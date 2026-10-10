/// 局部变量类型注解 reader→tree→writer 往返（#994 审查 P2/P3，ASM 兼容性缺口）：
/// MethodNode 此前未覆写 visitLocalVariableAnnotation（vector<Label*> 形态），
/// 基类默认实现转发 mv=nullptr 直接丢弃事件——注解在树中无存储、写回自然缺失。
/// 修复：MethodNode 覆写存储（起止范围 Label 经 canonicalizeLabel 收编，B8），
/// replayMethodBody 原序重发（既有代码），MethodWriter 新增 Code 属性
/// Runtime(In)VisibleTypeAnnotations 写出。
/// 构造携带 LOCAL_VARIABLE 目标类型注解（visible + invisible 各一条，含起止
/// Label 落点）的类，断言：
///   a) 树存储：LocalVariableAnnotationNode 入列，起止 Label 与指令侧 LabelNode
///      同一且归属方法 arena（B8），注解元素保真；
///   b) 往返字节保真（Code 属性携带 Runtime(In)VisibleTypeAnnotations）。
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "garena/arena.hpp"
#include "libca/core/tag_cast.hpp"
#include "test_utils.hpp"
#include "jbytecode/core/frame.hpp"
#include "jbytecode/core/type_reference.hpp"

namespace {

using ca::core::tag_cast::cast;
using ZUtf8StringRef = ca::str::ZUtf8StringRef;

/// 生成带局部变量类型注解的类字节（writer API 合成，javac 亦可产出该形态）：
///   static String m() { String s = null; return s; }
/// 变量槽 index=1 的类型注解 @I(42)/@I(7)（visible/invisible）覆盖
/// [start(ACONST_NULL), end(ASTORE 完成)]。
std::vector<ca::u8> buildClassWithLocalVariableTypeAnnotation(ca::str::Utf8StringArena& arena)
{
    ClassWriter writer(arena, 0);
    writer.visit(Opcodes::V1_8,
                 Opcodes::ACC_PUBLIC | Opcodes::ACC_SUPER,
                 ZUtf8StringRef::from_static("LvtaTarget"),
                 ca::str::Utf8StringRef{},
                 ZUtf8StringRef::from_static("java/lang/Object"),
                 {});
    auto* mv = writer.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()Ljava/lang/String;"),
                                  ca::str::Utf8StringRef{},
                                  {});
    Label start;
    Label end;
    // 注解事件先于 visitCode（与 MethodNode 重放顺序一致，前向引用形态）
    auto* visible = mv->visitLocalVariableAnnotation(TypeReference::LOCAL_VARIABLE << 24,
                                                     nullptr,
                                                     {&start},
                                                     {&end},
                                                     {1},
                                                     ZUtf8StringRef::from_static("Ljava/lang/String;"),
                                                     true);
    if (visible) {
        visible->visit(ZUtf8StringRef::from_static("value"), ca::core::Any(ca::i32(42)));
        visible->visitEnd();
    }
    auto* invisible =
        mv->visitLocalVariableAnnotation(TypeReference::LOCAL_VARIABLE << 24,
                                         nullptr,
                                         {&start},
                                         {&end},
                                         {1},
                                         ZUtf8StringRef::from_static("Ljava/lang/String;"),
                                         false);
    if (invisible) {
        invisible->visit(ZUtf8StringRef::from_static("value"), ca::core::Any(ca::i32(7)));
        invisible->visitEnd();
    }
    mv->visitCode();
    mv->visitLabel(&start);
    mv->visitInsn(Opcodes::ACONST_NULL);
    mv->visitVarInsn(Opcodes::ASTORE, 1);
    mv->visitLabel(&end);
    mv->visitVarInsn(Opcodes::ALOAD, 1);
    mv->visitInsn(Opcodes::ARETURN);
    mv->visitMaxs(1, 2);
    mv->visitEnd();
    writer.visitEnd();
    return writer.toByteArray();
}

}   // namespace

TEST(LocalVariableTypeAnnotationTest, roundtripPreservesAnnotationsAndLabelSpans)
{
    ca::str::Utf8StringArena arena;
    const auto               bytes = buildClassWithLocalVariableTypeAnnotation(arena);
    ASSERT_FALSE(bytes.empty());

    // ── reader 局部作用域内解析成树，随后 reader（labelArena_）析构 ──
    auto node = std::make_unique<jbytecode::tree::ClassNode>();
    {
        jbytecode::core::ClassReader reader(arena, bytes.data(),
                                            static_cast<ca::i32>(bytes.size()));
        reader.accept(node.get(), 0);
    }

    // ── 定位 m 方法 ──
    jbytecode::tree::MethodNode* m = nullptr;
    for (auto* method : node->methods) {
        if (method && method->name.to_std_string() == "m") {
            m = method;
            break;
        }
    }
    ASSERT_NE(m, nullptr);

    // ── 断言 a)：树存储 + Label 落点 ──
    // pre-fix：两个列表为空（事件在树边界被基类默认实现丢弃）。
    ASSERT_EQ(1u, m->visibleLocalVariableAnnotations.size());
    ASSERT_EQ(1u, m->invisibleLocalVariableAnnotations.size());
    auto* lva = m->visibleLocalVariableAnnotations[0];
    EXPECT_EQ(TypeReference::LOCAL_VARIABLE << 24, lva->typeRef);
    EXPECT_EQ("Ljava/lang/String;", lva->desc.to_std_string());
    // reader 形态：非空指针 + 空 path（无嵌套路径）
    ASSERT_NE(nullptr, lva->typePath);
    EXPECT_EQ(0, lva->typePath->getLength());
    ASSERT_EQ(1u, lva->start.size());
    ASSERT_EQ(1u, lva->end.size());
    ASSERT_EQ(1u, lva->index.size());
    EXPECT_EQ(1, lva->index[0]);
    // 注解元素保真（@I(42)）
    ASSERT_EQ(1u, lva->elements.size());
    EXPECT_EQ('I', lva->elements[0].tag);
    EXPECT_EQ(42, lva->elements[0].ival);
    // invisible 侧仅可见性不同
    auto* lvaInv = m->invisibleLocalVariableAnnotations[0];
    ASSERT_EQ(1u, lvaInv->elements.size());
    EXPECT_EQ(7, lvaInv->elements[0].ival);

    // Label 归属方法 arena（B8：收编副本，非 reader 原对象——reader 已析构）
    EXPECT_EQ(static_cast<const void*>(&m->arena()), lva->start[0]->owner());
    EXPECT_EQ(static_cast<const void*>(&m->arena()), lva->end[0]->owner());
    // Label 落点：起止与指令侧对应 LabelNode 同一（身份共享，写回偏移一致）
    std::vector<jbytecode::core::Label*> insnLabels;
    for (auto* insn = m->instructions.getFirst(); insn; insn = insn->getNext()) {
        if (insn->getType() == jbytecode::tree::AbstractInsnNode::LABEL) {
            insnLabels.push_back(cast<jbytecode::tree::LabelNode*>(insn)->label);
        }
    }
    ASSERT_EQ(2u, insnLabels.size());
    EXPECT_TRUE(std::find(insnLabels.begin(), insnLabels.end(), lva->start[0]) !=
                insnLabels.end());
    EXPECT_TRUE(std::find(insnLabels.begin(), insnLabels.end(), lva->end[0]) != insnLabels.end());

    // ── 断言 b)：reader 死亡后 tree→writer(0) 往返逐字保真 ──
    jbytecode::core::ClassWriter writer2(arena, 0);
    node->accept(&writer2);
    const auto out = writer2.toByteArray();
    EXPECT_EQ(bytes, out);
}

// accept 重放到下游 MethodVisitor：注解事件原序重发（此处重放进另一棵树，
// 第二棵树同样存储 + 再写回，验证多重放稳定性与可见性分组不变）。
TEST(LocalVariableTypeAnnotationTest, replayIntoSecondTreeStaysFaithful)
{
    ca::str::Utf8StringArena arena;
    const auto               bytes = buildClassWithLocalVariableTypeAnnotation(arena);

    auto first = std::make_unique<jbytecode::tree::ClassNode>();
    {
        jbytecode::core::ClassReader reader(arena, bytes.data(),
                                            static_cast<ca::i32>(bytes.size()));
        reader.accept(first.get(), 0);
    }
    auto second = std::make_unique<jbytecode::tree::ClassNode>();
    first->accept(second.get());
    ASSERT_EQ(1u, second->methods.size());
    EXPECT_EQ(1u, second->methods[0]->visibleLocalVariableAnnotations.size());
    EXPECT_EQ(1u, second->methods[0]->invisibleLocalVariableAnnotations.size());
    // 第二棵树的起止 Label 归属第二棵树的方法 arena（visitor 边界再收编）
    auto* lva = second->methods[0]->visibleLocalVariableAnnotations[0];
    EXPECT_EQ(static_cast<const void*>(&second->methods[0]->arena()), lva->start[0]->owner());

    jbytecode::core::ClassWriter writer(arena, 0);
    second->accept(&writer);
    writer.visitEnd();
    EXPECT_EQ(bytes, writer.toByteArray());
}
