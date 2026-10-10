#include "garena/arena.hpp"
#include <gtest/gtest.h>
#include "jbytecode/analysis/analyzer.hpp"
#include "jbytecode/analysis/basic_interpreter.hpp"

// B8b：测试侧 Label 统一归宿（进程寿命 scratch arena）——LabelNode() 默认构造
// 已随旧池制度退役，Label 一律 arena 分配。
static garena::Arena& labelScratchArena()
{
    static garena::Arena arena;
    return arena;
}


using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace ca::str;

namespace {

// 构造 public static int add(int a, int b) { return a + b; }
// MethodNode 析构会 delete 持有的指令，故栈上对象即可自管理。
std::unique_ptr<jbytecode::tree::MethodNode> makeAddMethod()
{
    auto m = std::make_unique<MethodNode>(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                 ZUtf8StringRef::from_static("add"),
                 ZUtf8StringRef::from_static("(II)I"),
                 Utf8StringRef(),
                 std::vector<ca::str::Utf8StringRef>{});
    m->maxStack  = 2;
    m->maxLocals = 2;
    m->instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));   // 0
    m->instructions.add(new VarInsnNode(Opcodes::ILOAD, 1));   // 1
    m->instructions.add(new InsnNode(Opcodes::IADD));          // 2
    m->instructions.add(new InsnNode(Opcodes::IRETURN));       // 3
    return m;
}

}   // namespace

// 注意：这些是首次覆盖 Analyzer::analyze() 端到端的测试（此前 22 个“真实”
// 测试全部只直驱 interpreter 回调，从未跑过整条帧计算路径）。结构性不变量
// 见下方各 TEST；帧内容精确断言（参数槽型/双槽语义/合并/handler 帧）见
// 文件末尾 frameContent* 系列——原 dev_plan 待查项已由此关闭。

TEST(AnalyzerTest, frameCountMatchesInstructionCount)
{
    auto                 m = makeAddMethod();
    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), m.get());
    EXPECT_EQ(m->instructions.size(), static_cast<int>(frames.size()));
}

TEST(AnalyzerTest, linearCodeAllReachable)
{
    auto                 m = makeAddMethod();
    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), m.get());
    // 直线代码每条指令都应可达（非 nullptr）
    for (auto* f : frames) {
        EXPECT_NE(nullptr, f);
    }
}

TEST(AnalyzerTest, localsCountAtLeastMethodLocals)
{
    auto                 m = makeAddMethod();
    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), m.get());
    // 入口帧应至少容纳 maxLocals 个局部变量槽
    ASSERT_NE(nullptr, frames[0]);
    EXPECT_GE(frames[0]->getLocals(), m->maxLocals);
}

TEST(AnalyzerTest, emptyMethodAnalyzable)
{
    // 仅一条 RETURN 的 void 方法，验证 analyze 不崩、帧数对齐
    MethodNode m(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                 ZUtf8StringRef::from_static("noop"),
                 ZUtf8StringRef::from_static("()V"),
                 Utf8StringRef(),
                 {});
    m.maxStack  = 0;
    m.maxLocals = 0;
    m.instructions.add(new InsnNode(Opcodes::RETURN));
    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), &m);
    EXPECT_EQ(1, static_cast<int>(frames.size()));
    EXPECT_NE(nullptr, frames[0]);
}

// 帧内容精确断言：验证 Frame::execute 数据流引擎真正计算了栈/局部，
// 而非退化传播。add(II)I = ILOAD0 / ILOAD1 / IADD / IRETURN。
// frames[i] 是执行第 i 条指令“之前”的帧状态。
TEST(AnalyzerTest, addMethodFrameContents)
{
    auto                 m = makeAddMethod();
    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), m.get());
    ASSERT_EQ(4, static_cast<int>(frames.size()));
    for (auto* f : frames) ASSERT_NE(nullptr, f);

    // 入口帧：locals[0]=INT, locals[1]=INT, 栈空
    EXPECT_EQ(2, frames[0]->getLocals());
    EXPECT_TRUE(frames[0]->getLocal(0) == BasicValue::INT_VALUE);
    EXPECT_TRUE(frames[0]->getLocal(1) == BasicValue::INT_VALUE);
    EXPECT_EQ(0, frames[0]->getStackSize());

    // ILOAD0 之后（ILOAD1 之前）：栈 = [INT]
    EXPECT_EQ(1, frames[1]->getStackSize());
    EXPECT_TRUE(frames[1]->getStack(0) == BasicValue::INT_VALUE);

    // ILOAD1 之后（IADD 之前）：栈 = [INT, INT]
    EXPECT_EQ(2, frames[2]->getStackSize());
    EXPECT_TRUE(frames[2]->getStack(0) == BasicValue::INT_VALUE);
    EXPECT_TRUE(frames[2]->getStack(1) == BasicValue::INT_VALUE);

    // IADD 之后（IRETURN 之前）：栈 = [INT]
    EXPECT_EQ(1, frames[3]->getStackSize());
    EXPECT_TRUE(frames[3]->getStack(0) == BasicValue::INT_VALUE);
}

// 分支：验证跳转目标处的帧 merge。abs(I)I:
//   ILOAD0 / IFGE L / ILOAD0 / INEG / IRETURN / L: ILOAD0 / IRETURN
TEST(AnalyzerTest, branchMergeFrameContents)
{
    MethodNode m(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                 ZUtf8StringRef::from_static("abs"),
                 ZUtf8StringRef::from_static("(I)I"),
                 Utf8StringRef(),
                 {});
    m.maxStack  = 1;
    m.maxLocals = 1;
    auto* L     = new LabelNode(labelScratchArena().create<Label>());
    m.instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));          // 0
    m.instructions.add(new JumpInsnNode(Opcodes::IFGE, L->label));   // 1
    m.instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));          // 2
    m.instructions.add(new InsnNode(Opcodes::INEG));                 // 3
    m.instructions.add(new InsnNode(Opcodes::IRETURN));              // 4
    m.instructions.add(L);                                           // 5 (label)
    m.instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));          // 6
    m.instructions.add(new InsnNode(Opcodes::IRETURN));              // 7

    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), &m);
    ASSERT_EQ(8, static_cast<int>(frames.size()));
    // 所有指令可达（分支两路都被处理）
    for (int i = 0; i < 8; i++) EXPECT_NE(nullptr, frames[i]) << "insn " << i;
    // 入口与跳转目标 label 处：locals[0]=INT，栈空（IFGE 已弹出条件）
    EXPECT_TRUE(frames[0]->getLocal(0) == BasicValue::INT_VALUE);
    ASSERT_NE(nullptr, frames[5]);
    EXPECT_EQ(0, frames[5]->getStackSize());
    EXPECT_TRUE(frames[5]->getLocal(0) == BasicValue::INT_VALUE);
}

// 实例方法：locals[0] 应为 this 引用（execute 不影响，但验证 initFrame 正确）。
TEST(AnalyzerTest, instanceMethodThisLocal)
{
    MethodNode m(Opcodes::ACC_PUBLIC,
                 ZUtf8StringRef::from_static("get"),
                 ZUtf8StringRef::from_static("()I"),
                 Utf8StringRef(),
                 {});
    m.maxStack  = 1;
    m.maxLocals = 1;
    m.instructions.add(new InsnNode(Opcodes::ICONST_0));   // 0
    m.instructions.add(new InsnNode(Opcodes::IRETURN));    // 1
    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), &m);
    ASSERT_EQ(2, static_cast<int>(frames.size()));
    ASSERT_NE(nullptr, frames[0]);
    EXPECT_GE(frames[0]->getLocals(), 1);
    // this 槽是引用类型（非 UNINITIALIZED）
    EXPECT_TRUE(frames[0]->getLocal(0).isReference());
    // ICONST_0 之后栈 = [INT]
    EXPECT_EQ(1, frames[1]->getStackSize());
    EXPECT_TRUE(frames[1]->getStack(0) == BasicValue::INT_VALUE);
}

// ============================================================================
// 帧内容精确断言（合法输入）
//
// 此前 analyzer 测试只断言结构性质（帧数、可达性、栈大小），移植自 ASM 的
// 帧计算从未对「局部变量/操作数栈里到底是什么值」精确断言过（dev_plan 曾
// 挂 analyzer-frame-model-suspect.md 待查项，文件并未落地）。以下用合法
// 输入把 BasicInterpreter 的帧语义钉死：参数槽型、long/double 双槽语义、
// 未用槽填充、同型合并不变、异型合并退化为 UNINITIALIZED、异常 handler
// 帧清栈压引用。
// ============================================================================

TEST(AnalyzerTest, frameContentLinearAddExactLocalsAndStack)
{
    auto                 m = makeAddMethod();
    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), m.get());
    ASSERT_EQ(4, static_cast<int>(frames.size()));

    // frames[i] 是「执行第 i 条指令前」的状态
    ASSERT_NE(nullptr, frames[0]);
    EXPECT_EQ(0, frames[0]->getStackSize());
    EXPECT_TRUE(frames[0]->getLocal(0) == BasicValue::INT_VALUE);
    EXPECT_TRUE(frames[0]->getLocal(1) == BasicValue::INT_VALUE);

    ASSERT_NE(nullptr, frames[1]);   // ILOAD_1 之前：栈 = [INT]
    EXPECT_EQ(1, frames[1]->getStackSize());
    EXPECT_TRUE(frames[1]->getStack(0) == BasicValue::INT_VALUE);

    ASSERT_NE(nullptr, frames[2]);   // IADD 之前：栈 = [INT, INT]
    EXPECT_EQ(2, frames[2]->getStackSize());
    EXPECT_TRUE(frames[2]->getStack(0) == BasicValue::INT_VALUE);
    EXPECT_TRUE(frames[2]->getStack(1) == BasicValue::INT_VALUE);

    ASSERT_NE(nullptr, frames[3]);   // IRETURN 之前：栈 = [INT]（IADD 结果）
    EXPECT_EQ(1, frames[3]->getStackSize());
    EXPECT_TRUE(frames[3]->getStack(0) == BasicValue::INT_VALUE);
}

TEST(AnalyzerTest, frameContentUnusedLocalsAreUninitialized)
{
    auto m = makeAddMethod();
    m->maxLocals  = 4;   // 参数只占 0/1，2/3 未用
    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), m.get());
    ASSERT_NE(nullptr, frames[0]);
    EXPECT_GE(frames[0]->getLocals(), 4);
    EXPECT_TRUE(frames[0]->getLocal(0) == BasicValue::INT_VALUE);
    EXPECT_TRUE(frames[0]->getLocal(1) == BasicValue::INT_VALUE);
    EXPECT_TRUE(frames[0]->getLocal(2) == BasicValue::UNINITIALIZED_VALUE);
    EXPECT_TRUE(frames[0]->getLocal(3) == BasicValue::UNINITIALIZED_VALUE);
}

// long 参数占两个 local 槽（第二槽 UNINITIALIZED），但在操作数栈上只占一个槽。
TEST(AnalyzerTest, frameContentLongTwoLocalSlotsOneStackSlot)
{
    MethodNode m(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                 ZUtf8StringRef::from_static("id"),
                 ZUtf8StringRef::from_static("(J)J"),
                 Utf8StringRef(),
                 {});
    m.maxStack  = 2;
    m.maxLocals = 2;
    m.instructions.add(new VarInsnNode(Opcodes::LLOAD, 0));   // 0
    m.instructions.add(new InsnNode(Opcodes::LRETURN));       // 1

    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), &m);
    ASSERT_EQ(2, static_cast<int>(frames.size()));

    ASSERT_NE(nullptr, frames[0]);
    EXPECT_EQ(0, frames[0]->getStackSize());
    EXPECT_TRUE(frames[0]->getLocal(0) == BasicValue::LONG_VALUE);
    EXPECT_TRUE(frames[0]->getLocal(1) == BasicValue::UNINITIALIZED_VALUE);   // 第二槽

    ASSERT_NE(nullptr, frames[1]);   // LRETURN 之前：栈 = [LONG]（单栈槽）
    EXPECT_EQ(1, frames[1]->getStackSize());
    EXPECT_TRUE(frames[1]->getStack(0) == BasicValue::LONG_VALUE);
}

// 同型合并：两路都汇入 INT → join 处仍是 INT。
TEST(AnalyzerTest, frameContentJoinOfSameTypeKeepsValue)
{
    MethodNode m(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                 ZUtf8StringRef::from_static("abs"),
                 ZUtf8StringRef::from_static("(I)I"),
                 Utf8StringRef(),
                 {});
    m.maxStack  = 1;
    m.maxLocals = 1;
    auto* L     = new LabelNode(labelScratchArena().create<Label>());
    auto* E     = new LabelNode(labelScratchArena().create<Label>());
    m.instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));          // 0
    m.instructions.add(new JumpInsnNode(Opcodes::IFLT, L->label));   // 1
    m.instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));          // 2
    m.instructions.add(new InsnNode(Opcodes::INEG));                 // 3
    m.instructions.add(new JumpInsnNode(Opcodes::GOTO, E->label));   // 4
    m.instructions.add(L);                                           // 5
    m.instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));          // 6
    m.instructions.add(E);                                           // 7
    m.instructions.add(new InsnNode(Opcodes::IRETURN));              // 8

    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), &m);
    ASSERT_EQ(9, static_cast<int>(frames.size()));

    ASSERT_NE(nullptr, frames[8]);   // join 点之后的 IRETURN
    EXPECT_EQ(1, frames[8]->getStackSize());
    EXPECT_TRUE(frames[8]->getStack(0) == BasicValue::INT_VALUE);
}

// 异型合并：INT 与 FLOAT 汇合 → BasicInterpreter::merge 退化为 UNINITIALIZED。
TEST(AnalyzerTest, frameContentJoinOfDifferentTypesYieldsUninitialized)
{
    MethodNode m(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                 ZUtf8StringRef::from_static("poly"),
                 ZUtf8StringRef::from_static("(I)I"),
                 Utf8StringRef(),
                 {});
    m.maxStack  = 1;
    m.maxLocals = 1;
    auto* L     = new LabelNode(labelScratchArena().create<Label>());
    auto* E     = new LabelNode(labelScratchArena().create<Label>());
    m.instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));          // 0
    m.instructions.add(new JumpInsnNode(Opcodes::IFLT, L->label));   // 1
    m.instructions.add(new InsnNode(Opcodes::ICONST_1));             // 2  INT
    m.instructions.add(new JumpInsnNode(Opcodes::GOTO, E->label));   // 3
    m.instructions.add(L);                                           // 4
    m.instructions.add(new InsnNode(Opcodes::FCONST_1));             // 5  FLOAT
    m.instructions.add(E);                                           // 6
    m.instructions.add(new InsnNode(Opcodes::IRETURN));              // 7

    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), &m);
    ASSERT_EQ(8, static_cast<int>(frames.size()));

    ASSERT_NE(nullptr, frames[7]);
    EXPECT_EQ(1, frames[7]->getStackSize());
    EXPECT_TRUE(frames[7]->getStack(0) == BasicValue::UNINITIALIZED_VALUE);
}

// 异常 handler 入口帧：无论 try 区残留什么栈，handler 帧只压一个异常引用
// 且 locals 与方法入口一致。try 区里先压两个 INT 再抛出，证明栈被清空。
TEST(AnalyzerTest, frameContentHandlerFrameClearsStackAndPushesExceptionRef)
{
    MethodNode m(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                 ZUtf8StringRef::from_static("guarded"),
                 ZUtf8StringRef::from_static("(I)I"),
                 Utf8StringRef(),
                 {});
    m.maxStack  = 3;
    m.maxLocals = 2;
    auto* start   = new LabelNode(labelScratchArena().create<Label>());
    auto* end     = new LabelNode(labelScratchArena().create<Label>());
    auto* handler = new LabelNode(labelScratchArena().create<Label>());
    m.instructions.add(start);                                        // 0
    m.instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));           // 1 栈 [INT]
    m.instructions.add(new InsnNode(Opcodes::ICONST_2));              // 2 栈 [INT, INT]
    m.instructions.add(new InsnNode(Opcodes::POP));                   // 3 栈 [INT]
    m.instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));           // 4 栈 [INT, INT]
    m.instructions.add(new InsnNode(Opcodes::IDIV));                  // 5 栈 [INT]
    m.instructions.add(new InsnNode(Opcodes::IRETURN));               // 6
    m.instructions.add(end);                                          // 7
    m.instructions.add(handler);                                      // 8
    m.instructions.add(new VarInsnNode(Opcodes::ISTORE, 1));          // 9
    m.instructions.add(new InsnNode(Opcodes::ICONST_M1));             // 10
    m.instructions.add(new InsnNode(Opcodes::IRETURN));               // 11
    m.tryCatchBlocks.push_back(m.create<TryCatchBlockNode>(
        start->label, end->label, handler->label, ZUtf8StringRef::from_static("java/lang/Exception")));

    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);
    auto                 frames = analyzer.analyze(ZUtf8StringRef::from_static("Demo"), &m);
    ASSERT_EQ(12, static_cast<int>(frames.size()));

    ASSERT_NE(nullptr, frames[8]);   // handler 入口
    EXPECT_EQ(1, frames[8]->getStackSize());   // 残留栈被清空，只留异常引用
    EXPECT_TRUE(frames[8]->getStack(0).isReference());
    EXPECT_TRUE(frames[8]->getLocal(0) == BasicValue::INT_VALUE);   // locals 不受影响
}
