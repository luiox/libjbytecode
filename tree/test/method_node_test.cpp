#include <gtest/gtest.h>
#include "test_utils.hpp"


TEST(MethodNodeTest, testConstructor)
{
    MethodNode methodNode(Opcodes::ACC_PUBLIC,
                          ZUtf8StringRef::from_static("method"),
                          ZUtf8StringRef::from_static("()V"),
                          ZUtf8StringRef::from_static(""),
                          {});
    ASSERT_EQ(Opcodes::ACC_PUBLIC, methodNode.access);
    ASSERT_EQ(ZUtf8StringRef::from_static("method"), methodNode.name);
    ASSERT_EQ(ZUtf8StringRef::from_static("()V"), methodNode.desc);
}

TEST(MethodNodeTest, testClone)
{
    MethodNode methodNode(Opcodes::ACC_PUBLIC,
                          ZUtf8StringRef::from_static("m"),
                          ZUtf8StringRef::from_static("()V"),
                          ZUtf8StringRef::from_static(""),
                          {});
    methodNode.visitCode();
    methodNode.visitLabel(new Label());
    methodNode.visitInsn(Opcodes::NOP);
    ASSERT_EQ(2, methodNode.instructions.size());
}

// B8 收编：visitor 边界把外来堆 Label 拷贝为方法 arena 副本（owner 盖章），原对象不动。
TEST(MethodNodeTest, visitorCanonicalizesForeignLabels)
{
    MethodNode mn(Opcodes::ACC_PUBLIC,
                  ZUtf8StringRef::from_static("m"),
                  ZUtf8StringRef::from_static("()V"),
                  ZUtf8StringRef::from_static(""),
                  {});
    auto*      l1 = new Label();
    auto*      l2 = new Label();
    mn.visitJumpInsn(Opcodes::GOTO, l1);
    mn.visitLabel(l2);
    mn.visitInsn(Opcodes::RETURN);
    // 原对象归调用方（B8b：taken 机制已随旧池删除，无状态可断言）。
    // 树内存的是收编副本：身份换新、归属盖章、边界幂等（映射表直返同副本）。
    ASSERT_EQ(3, mn.instructions.size());
    auto* jump = static_cast<JumpInsnNode*>(mn.instructions.get(0));
    auto* ln   = static_cast<LabelNode*>(mn.instructions.get(1));
    EXPECT_NE(l1, jump->label);
    EXPECT_EQ(mn.canonicalizeLabel(l1), jump->label);
    EXPECT_EQ(static_cast<const void*>(&mn.arena()), jump->label->owner());
    EXPECT_NE(l2, ln->label);
    EXPECT_EQ(static_cast<const void*>(&mn.arena()), ln->label->owner());
    delete l1;
    delete l2;
}

// visitFrame 帧槽收编：帧 local/stack 中 ITEM_Uninitialized 形态的 Label* 与跳转
// 目标同属外来 Label，必须随方法边界拷贝收编（B8；否则 ClassReader 析构释放
// labelArena_ 后 FrameNode 持悬垂指针，flags=0 写回的 visitFrame→Label::put→
// isResolved 踩 use-after-free）。值类型帧槽（int 编码的验证类型等）原样保留。
TEST(MethodNodeTest, visitFrameCanonicalizesForeignLabelInFrameTypes)
{
    MethodNode mn(Opcodes::ACC_PUBLIC,
                  ZUtf8StringRef::from_static("m"),
                  ZUtf8StringRef::from_static("()V"),
                  ZUtf8StringRef::from_static(""),
                  {});
    auto* foreign = new Label();   // 模拟 reader labelArena_ 产物（外来堆 Label）
    foreign->bytecodeOffset = 0;
    foreign->flags |= Label::FLAG_RESOLVED;
    std::vector<ca::core::Any> stack{ca::core::Any(foreign)};
    mn.visitFrame(Opcodes::F_SAME1, 0, {}, 1, stack);

    ASSERT_EQ(1, mn.instructions.size());
    auto* frame = static_cast<FrameNode*>(mn.instructions.get(0));
    ASSERT_EQ(1u, frame->stack.size());
    auto* slot = frame->stack[0].as<Label*>();
    ASSERT_NE(slot, nullptr);
    EXPECT_NE(*slot, foreign);   // 身份换新：收编副本，非外来原对象
    EXPECT_EQ(static_cast<const void*>(&mn.arena()), (*slot)->owner());
    EXPECT_EQ(0, (*slot)->bytecodeOffset);   // 收编拷贝保留控制流元数据
    EXPECT_EQ(mn.canonicalizeLabel(foreign), *slot);   // 边界幂等：映射直返同副本

    // 值类型帧槽不收编、不变形：int 编码验证类型原样保留。
    std::vector<ca::core::Any> locals{ca::core::Any(Opcodes::TOP)};
    mn.visitFrame(Opcodes::F_APPEND, 1, locals, 0, {});
    auto* frame2 = static_cast<FrameNode*>(mn.instructions.get(1));
    ASSERT_EQ(1u, frame2->local.size());
    auto* val = frame2->local[0].as<int>();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(Opcodes::TOP, *val);
    delete foreign;
}

// 析构：arena 内容物（指令/收编 Label 副本等）统一回收；外来 Label 只拷贝收编，
// 原对象归创建方（不泄漏、不 double-free）。
TEST(MethodNodeTest, destructorReleasesArenaContent)
{
    Label* l1;
    Label* l2;
    {
        MethodNode mn(Opcodes::ACC_PUBLIC,
                      ZUtf8StringRef::from_static("m"),
                      ZUtf8StringRef::from_static("()V"),
                      ZUtf8StringRef::from_static(""),
                      {});
        l1 = new Label();
        l2 = new Label();
        mn.visitLabel(l1);   // 收编为 arena 副本，原对象归测试
        mn.visitJumpInsn(Opcodes::GOTO, l2);
        mn.visitInsn(Opcodes::RETURN);
        delete l1;
        delete l2;
    }
    // 作用域结束，MethodNode 析构回收 arena 副本。无法直接断言已释放，
    // 但若 double-free/use-after-free 会被 ASAN/堆检查捕获。此处主要验证不崩溃。
    SUCCEED();
}

// accept replay 到下游 MethodNode：下游 visitor 边界把外来 Label 收编为自己的
// arena 副本（owner 隔离），源方法与其 Label 不受影响。
TEST(MethodNodeTest, acceptDoesNotDoubleOwn)
{
    MethodNode src(Opcodes::ACC_PUBLIC,
                   ZUtf8StringRef::from_static("m"),
                   ZUtf8StringRef::from_static("()V"),
                   ZUtf8StringRef::from_static(""),
                   {});
    auto*      l = new Label();
    src.visitLabel(l);   // src 收编为自己的 arena 副本
    src.visitInsn(Opcodes::RETURN);
    auto* srcCopy = static_cast<LabelNode*>(src.instructions.get(0))->label;
    EXPECT_EQ(static_cast<const void*>(&src.arena()), srcCopy->owner());

    MethodNode dst(Opcodes::ACC_PUBLIC,
                   ZUtf8StringRef::from_static("m"),
                   ZUtf8StringRef::from_static("()V"),
                   ZUtf8StringRef::from_static(""),
                   {});
    src.accept(&dst);   // replay：dst 边界把 src 的副本再收编为自己的副本
    auto* dstCopy = static_cast<LabelNode*>(dst.instructions.get(0))->label;
    EXPECT_NE(srcCopy, dstCopy);   // 各方法各持副本，身份隔离
    EXPECT_EQ(static_cast<const void*>(&dst.arena()), dstCopy->owner());
    delete l;
}
