/// 回归测试（野外罐全管线段错误根因，main 基线 c913d0528 发现；移植自
/// fix/zkm-indyref-segfault 专项，补 #994/63a6acde5 契约直测之外的两个维度）：
/// MethodNode::visitFrame 曾漏收编展开帧中 ITEM_UNINITIALIZED 条目的 reader 侧
/// Label*（其余 visit* 入口均经 canonicalizeLabel 收编）：
///   1) 悬垂：ClassReader 析构（labelArena_ 释放）后 FrameNode 仍持已释放
///      Label*，tree→ClassWriter 往返经 putVerificationTypeInfo → Label::put
///      → isResolved() 读释放内存（linux glibc/arena 回收后页脱映射 →
///      SIGSEGV；MSVC 小块不复用不清零长期掩盖）；
///   2) 身份：帧内 Label 与指令侧 LabelNode 的收编副本非同一对象，写栈图时
///      isResolved() 恒假，Item_Uninitialized offset 落前向占位且无人回补，
///      StackMapTable 损坏（VerifyError）。
/// 构造「构造器实参分支」形态使栈图携带 ITEM_UNINITIALIZED（new Integer 的
/// 未初始化引用横跨跳转汇合帧），走真实 ClassReader 解析路径，断言：
///   a) 帧内 Label 与某条指令 LabelNode 的 label 同一（收编身份归位）；
///   b) reader 出作用域后 tree→writer(0) 逐字保真（寿命收编归位；
///      pre-fix 形态本断言在 linux ASAN 下必崩、MSVC 下字节必不一致）。
#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "garena/arena.hpp"
#include "libca/core/tag_cast.hpp"
#include "test_utils.hpp"
#include "jbytecode/core/frame.hpp"

namespace {

using ca::core::tag_cast::cast;
using ZUtf8StringRef = ca::str::ZUtf8StringRef;

/// 生成带 ITEM_UNINITIALIZED 栈图项的类字节：
///   static Object pick(boolean z) { return new Integer(z ? 1 : 0); }
/// 汇合点帧的栈 = [Uninitialized, int]，展开后即 ITEM_UNINITIALIZED 条目。
std::vector<ca::u8> buildClassWithUninitializedFrame(ca::str::Utf8StringArena& arena)
{
    ClassWriter writer(arena, ClassWriter::COMPUTE_FRAMES);
    writer.setTypeResolver(&jbytecode::core::defaultProvider);
    writer.visit(Opcodes::V1_7,
                 Opcodes::ACC_PUBLIC | Opcodes::ACC_SUPER,
                 ZUtf8StringRef::from_static("FrameUninitTarget"),
                 ca::str::Utf8StringRef{},
                 ZUtf8StringRef::from_static("java/lang/Object"),
                 {});
    auto* mv = writer.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                                  ZUtf8StringRef::from_static("pick"),
                                  ZUtf8StringRef::from_static("(Z)Ljava/lang/Object;"),
                                  ca::str::Utf8StringRef{},
                                  {});
    mv->visitCode();
    mv->visitTypeInsn(Opcodes::NEW, ZUtf8StringRef::from_static("java/lang/Integer"));
    mv->visitInsn(Opcodes::DUP);
    mv->visitVarInsn(Opcodes::ILOAD, 0);
    Label elseLabel;
    Label endLabel;
    mv->visitJumpInsn(Opcodes::IFEQ, &elseLabel);
    mv->visitInsn(Opcodes::ICONST_1);
    mv->visitJumpInsn(Opcodes::GOTO, &endLabel);
    mv->visitLabel(&elseLabel);
    mv->visitInsn(Opcodes::ICONST_0);
    mv->visitLabel(&endLabel);
    mv->visitMethodInsn(Opcodes::INVOKESPECIAL,
                        ZUtf8StringRef::from_static("java/lang/Integer"),
                        ZUtf8StringRef::from_static("<init>"),
                        ZUtf8StringRef::from_static("(I)V"),
                        false);
    mv->visitInsn(Opcodes::ARETURN);
    mv->visitMaxs(0, 0);
    mv->visitEnd();
    writer.visitEnd();
    return writer.toByteArray();
}

bool containsLabel(const std::vector<jbytecode::core::Label*>& labels,
                   jbytecode::core::Label*                    label)
{
    return std::find(labels.begin(), labels.end(), label) != labels.end();
}

}   // namespace

TEST(MethodNodeFrameLabelTest, visitFrameCanonicalizesUninitializedLabels)
{
    ca::str::Utf8StringArena arena;
    const auto bytes = buildClassWithUninitializedFrame(arena);
    ASSERT_FALSE(bytes.empty());

    // ── reader 局部作用域内解析成树，随后 reader（labelArena_）析构 ──
    auto node = std::make_unique<jbytecode::tree::ClassNode>();
    {
        jbytecode::core::ClassReader reader(arena, bytes.data(),
                                            static_cast<ca::i32>(bytes.size()));
        reader.accept(node.get(), 0);
    }

    // ── 定位 pick 方法 ──
    jbytecode::tree::MethodNode* pick = nullptr;
    for (auto* m : node->methods) {
        if (m && m->name.to_std_string() == "pick") {
            pick = m;
            break;
        }
    }
    ASSERT_NE(pick, nullptr);

    // ── 收集指令侧 LabelNode 的收编副本 ──
    std::vector<jbytecode::core::Label*> insnLabels;
    int                                  frameCount = 0;
    for (auto* insn = pick->instructions.getFirst(); insn; insn = insn->getNext()) {
        if (insn->getType() == jbytecode::tree::AbstractInsnNode::LABEL) {
            insnLabels.push_back(cast<jbytecode::tree::LabelNode*>(insn)->label);
        }
    }
    ASSERT_FALSE(insnLabels.empty());

    // ── 断言 a)：帧内（local/stack）每个 Label 均与某指令 LabelNode 同一 ──
    bool allFrameLabelsCanonical = true;
    bool sawUninitializedEntry   = false;
    for (auto* insn = pick->instructions.getFirst(); insn; insn = insn->getNext()) {
        if (insn->getType() != jbytecode::tree::AbstractInsnNode::FRAME) continue;
        auto* frame = cast<jbytecode::tree::FrameNode*>(insn);
        ++frameCount;
        for (const auto& v : frame->local) {
            if (auto* lbl = v.as<jbytecode::core::Label*>()) {
                sawUninitializedEntry = true;
                if (!containsLabel(insnLabels, *lbl)) allFrameLabelsCanonical = false;
            }
        }
        for (const auto& v : frame->stack) {
            if (auto* lbl = v.as<jbytecode::core::Label*>()) {
                sawUninitializedEntry = true;
                if (!containsLabel(insnLabels, *lbl)) allFrameLabelsCanonical = false;
            }
        }
    }
    EXPECT_GT(frameCount, 0);              // 汇合点帧在树中
    EXPECT_TRUE(sawUninitializedEntry);    // 用例确实携带 ITEM_UNINITIALIZED
    EXPECT_TRUE(allFrameLabelsCanonical);  // pre-fix：帧内持 reader 原 Label ≠ 收编副本

    // ── 断言 b)：reader 死亡后 tree→writer(0) 往返逐字保真 ──
    // pre-fix：帧内 Label 已释放——linux ASAN 下此段必报 heap-use-after-free /
    // heap-buffer-overflow；MSVC 下读到失效 flags/offset → 字节必不一致。
    jbytecode::core::ClassWriter writer2(arena, 0);
    node->accept(&writer2);
    const auto out = writer2.toByteArray();
    EXPECT_EQ(bytes, out);
}
