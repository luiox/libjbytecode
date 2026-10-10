#include <gtest/gtest.h>
#include "test_utils.hpp"
#include "jbytecode/core/frame.hpp"

// B8 clone(arena, clonedLabels) 协议单测：
// - 节点一律在目标 arena 分配（arena.owns 判定）；
// - 携带的 Label 经自由函数 resolveCloneLabel 解析（nullptr 直返 / 同 arena O(1) 直返 /
//   map 命中直返 / 否则建副本并登记，拷贝 {bytecodeOffset, line, flags}）；
// - 本协议为唯一 clone 通道（B8b 已物理退役旧堆 clone(map) 虚函数族，
//   下游 jsr_inliner_adapter 亦已迁移至本协议）。

// -- resolveCloneLabel -------------------------------------------------------

TEST(ResolveCloneLabelTest, nullptrDirectReturn)
{
    garena::Arena                     arena{256};
    std::unordered_map<Label*, Label*> map;
    EXPECT_EQ(nullptr, resolveCloneLabel(arena, map, nullptr));
    EXPECT_TRUE(map.empty());
}

TEST(ResolveCloneLabelTest, foreignLabelCopiedIntoArena)
{
    garena::Arena                     arena{256};
    std::unordered_map<Label*, Label*> map;
    Label                              src;
    src.bytecodeOffset = 42;
    src.line           = 7;
    src.flags          = Label::FLAG_JUMP_TARGET;

    Label* resolved = resolveCloneLabel(arena, map, &src);
    ASSERT_NE(nullptr, resolved);
    EXPECT_NE(&src, resolved);
    EXPECT_TRUE(arena.owns(resolved));
    EXPECT_EQ(&arena, resolved->owner());
    EXPECT_EQ(42, resolved->bytecodeOffset);
    EXPECT_EQ(7, resolved->line);
    EXPECT_TRUE(resolved->flags & Label::FLAG_JUMP_TARGET);     // 其余 flags 保留
    EXPECT_EQ(resolved, map[&src]);                             // 登记映射
}

TEST(ResolveCloneLabelTest, mapHitDirectReturn)
{
    garena::Arena                     arena{256};
    std::unordered_map<Label*, Label*> map;
    Label                              src;
    Label* first  = resolveCloneLabel(arena, map, &src);
    Label* second = resolveCloneLabel(arena, map, &src);
    EXPECT_EQ(first, second);   // 前向跳转与其目标共享同一克隆
}

TEST(ResolveCloneLabelTest, sameArenaDirectReturn)
{
    garena::Arena                     arena{256};
    std::unordered_map<Label*, Label*> map;
    Label* owned = arena.create<Label>();
    owned->setOwner(&arena);

    Label* resolved = resolveCloneLabel(arena, map, owned);
    EXPECT_EQ(owned, resolved);   // 同 arena O(1) 直返，身份不变
    EXPECT_TRUE(map.empty());     // 同域直返不登记 map
}

// -- 各节点 clone(arena, map)：目标 arena 分配 + Label 解析 -------------------

TEST(InsnCloneTest, cloneAllocatesInTargetArena)
{
    garena::Arena                      arena{256};
    std::unordered_map<Label*, Label*> map;
    InsnNode                           insn(Opcodes::ICONST_1);

    AbstractInsnNode* cloned = insn.clone(arena, map);
    ASSERT_NE(nullptr, cloned);
    EXPECT_TRUE(arena.owns(cloned));
    EXPECT_EQ(AbstractInsnNode::INSN, cloned->getType());
    EXPECT_EQ(Opcodes::ICONST_1, cloned->getOpcode());
}

TEST(InsnCloneTest, clonePlainNodesAllocatedInArena)
{
    garena::Arena                      arena{256};
    std::unordered_map<Label*, Label*> map;

    auto check = [&arena](AbstractInsnNode* cloned, int type) {
        ASSERT_NE(nullptr, cloned);
        EXPECT_TRUE(arena.owns(cloned));
        EXPECT_EQ(type, cloned->getType());
    };

    IntInsnNode           intInsn(Opcodes::BIPUSH, 7);
    VarInsnNode           varInsn(Opcodes::ILOAD, 2);
    TypeInsnNode          typeInsn(Opcodes::NEW, ZUtf8StringRef::from_static("java/lang/Object"));
    FieldInsnNode         fieldInsn(Opcodes::GETFIELD, ZUtf8StringRef::from_static("A"),
                            ZUtf8StringRef::from_static("f"), ZUtf8StringRef::from_static("I"));
    MethodInsnNode        methodInsn(Opcodes::INVOKEVIRTUAL, ZUtf8StringRef::from_static("A"),
                             ZUtf8StringRef::from_static("m"), ZUtf8StringRef::from_static("()V"),
                             false);
    InvokeDynamicInsnNode indy(ZUtf8StringRef::from_static("run"),
                               ZUtf8StringRef::from_static("()Ljava/lang/Runnable;"),
                               Handle(Opcodes::H_INVOKESTATIC, ZUtf8StringRef::from_static("B"),
                                      ZUtf8StringRef::from_static("meta"),
                                      ZUtf8StringRef::from_static("()Ljava/lang/invoke/CallSite;")),
                               {});
    LdcInsnNode           ldc(ca::core::Any(ca::i32(1)));
    IincInsnNode          iinc(1, 3);
    MultiANewArrayInsnNode multiANew(ZUtf8StringRef::from_static("[[I"), 2);
    FrameNode              frame(0, {}, {});

    check(intInsn.clone(arena, map), AbstractInsnNode::INT_INSN);
    check(varInsn.clone(arena, map), AbstractInsnNode::VAR_INSN);
    check(typeInsn.clone(arena, map), AbstractInsnNode::TYPE_INSN);
    check(fieldInsn.clone(arena, map), AbstractInsnNode::FIELD_INSN);
    check(methodInsn.clone(arena, map), AbstractInsnNode::METHOD_INSN);
    check(indy.clone(arena, map), AbstractInsnNode::INVOKE_DYNAMIC_INSN);
    check(ldc.clone(arena, map), AbstractInsnNode::LDC_INSN);
    check(iinc.clone(arena, map), AbstractInsnNode::IINC_INSN);
    check(multiANew.clone(arena, map), AbstractInsnNode::MULTIANEWARRAY_INSN);
    check(frame.clone(arena, map), AbstractInsnNode::FRAME);
}

TEST(InsnCloneTest, cloneJumpAndLabelShareResolvedLabel)
{
    garena::Arena                      arena{256};
    std::unordered_map<Label*, Label*> map;
    Label                              target;

    JumpInsnNode jump(Opcodes::GOTO, &target);
    LabelNode    labelNode(&target);

    auto* clonedJump  = static_cast<JumpInsnNode*>(jump.clone(arena, map));
    auto* clonedLabel = static_cast<LabelNode*>(labelNode.clone(arena, map));
    EXPECT_TRUE(arena.owns(clonedJump));
    EXPECT_TRUE(arena.owns(clonedLabel));
    ASSERT_NE(nullptr, clonedJump->label);
    EXPECT_NE(&target, clonedJump->label);
    EXPECT_EQ(&arena, clonedJump->label->owner());
    // 前向跳转与其目标 LabelNode 指向同一克隆 label（ASM getLabelNode 语义）。
    EXPECT_EQ(clonedJump->label, clonedLabel->getLabel());
}

TEST(InsnCloneTest, cloneTableSwitchResolvesAllLabels)
{
    garena::Arena                      arena{256};
    std::unordered_map<Label*, Label*> map;
    Label                              dflt;
    Label                              l0;
    Label                              l1;

    TableSwitchInsnNode ts(0, 1, &dflt, {&l0, &l1});
    auto* cloned = static_cast<TableSwitchInsnNode*>(ts.clone(arena, map));
    EXPECT_TRUE(arena.owns(cloned));
    ASSERT_EQ(2u, cloned->labels.size());
    EXPECT_NE(&dflt, cloned->dflt);
    EXPECT_NE(&l0, cloned->labels[0]);
    EXPECT_NE(&l1, cloned->labels[1]);
    EXPECT_EQ(map[&dflt], cloned->dflt);
    EXPECT_EQ(map[&l0], cloned->labels[0]);
    EXPECT_EQ(map[&l1], cloned->labels[1]);
}

TEST(InsnCloneTest, cloneLookupSwitchResolvesAllLabels)
{
    garena::Arena                      arena{256};
    std::unordered_map<Label*, Label*> map;
    Label                              dflt;
    Label                              l0;

    LookupSwitchInsnNode ls(&dflt, {5}, {&l0});
    auto* cloned = static_cast<LookupSwitchInsnNode*>(ls.clone(arena, map));
    EXPECT_TRUE(arena.owns(cloned));
    EXPECT_NE(&dflt, cloned->dflt);
    ASSERT_EQ(1u, cloned->labels.size());
    EXPECT_NE(&l0, cloned->labels[0]);
    EXPECT_EQ(map[&dflt], cloned->dflt);
    EXPECT_EQ(map[&l0], cloned->labels[0]);
}

TEST(InsnCloneTest, cloneLineNumberResolvesStartLabel)
{
    garena::Arena                      arena{256};
    std::unordered_map<Label*, Label*> map;
    Label                              start;
    start.line = 3;

    LineNumberNode ln(12, &start);
    auto* cloned = static_cast<LineNumberNode*>(ln.clone(arena, map));
    EXPECT_TRUE(arena.owns(cloned));
    EXPECT_EQ(12, cloned->line);
    EXPECT_NE(&start, cloned->start);   // B8：未命中也建 arena 副本，不回退引用原 label
    EXPECT_EQ(map[&start], cloned->start);
    EXPECT_EQ(&arena, cloned->start->owner());
}

// -- MethodNode::resolveCloneLabel 成员（委托自由函数） -----------------------

TEST(MethodNodeResolveCloneLabelTest, delegatesToFreeFunction)
{
    MethodNode m(Opcodes::ACC_PUBLIC,
                 ZUtf8StringRef::from_static("a"),
                 ZUtf8StringRef::from_static("()V"),
                 ZUtf8StringRef::from_static(""),
                 {});
    Label src;
    src.bytecodeOffset = 5;

    std::unordered_map<Label*, Label*> map;
    Label* resolved = m.resolveCloneLabel(map, &src);
    ASSERT_NE(nullptr, resolved);
    EXPECT_NE(&src, resolved);
    EXPECT_TRUE(m.nodeArena_.owns(resolved));
    EXPECT_EQ(&m.nodeArena_, resolved->owner());
    EXPECT_EQ(5, resolved->bytecodeOffset);
    EXPECT_EQ(resolved, m.resolveCloneLabel(map, &src));   // map 命中直返
}

TEST(MethodNodeResolveCloneLabelTest, sameMethodArenaLabelDirectReturn)
{
    MethodNode m(Opcodes::ACC_PUBLIC,
                 ZUtf8StringRef::from_static("a"),
                 ZUtf8StringRef::from_static("()V"),
                 ZUtf8StringRef::from_static(""),
                 {});
    Label* owned = m.createLabel();

    std::unordered_map<Label*, Label*> map;
    EXPECT_EQ(owned, m.resolveCloneLabel(map, owned));   // 同 arena O(1) 直返
    EXPECT_TRUE(map.empty());
}

// -- FrameNode::clone：帧槽 Label 重映射（B8 收编在 clone 侧的对偶，#994 审查 P2）--
//
// FrameNode::clone 曾原样拷贝 local/stack 向量，槽内 Label* 跨 arena 直指源方法域，
// 违背「树只持有方法级 arena 副本」契约（跨方法克隆后源方法先亡即同族悬垂）。

TEST(InsnCloneTest, cloneFrameRemapsLabelSlotsIntoTargetArena)
{
    garena::Arena                      targetArena{256};
    std::unordered_map<Label*, Label*> map;
    Label                              srcLocal;   // 模拟源方法 arena 产物
    srcLocal.bytecodeOffset            = 9;
    srcLocal.line                      = 3;
    srcLocal.flags                     = Label::FLAG_RESOLVED;
    Label                              srcStack;
    srcStack.bytecodeOffset            = 4;
    const Label                        srcStackConst;   // const 形态槽（reader 兼容面）

    std::vector<ca::core::Any> local{ca::core::Any(ca::i32(1)), ca::core::Any(&srcLocal)};
    std::vector<ca::core::Any> stack{ca::core::Any(&srcStack),
                                     ca::core::Any(static_cast<const Label*>(&srcStackConst))};

    FrameNode frame(Opcodes::F_SAME1, local, stack);
    auto*     cloned = static_cast<FrameNode*>(frame.clone(targetArena, map));
    ASSERT_NE(nullptr, cloned);
    EXPECT_TRUE(targetArena.owns(cloned));
    EXPECT_EQ(Opcodes::F_SAME1, cloned->frameType);

    // local：值槽原样、Label 槽换身份入目标 arena（元数据拷贝保留）
    ASSERT_EQ(2u, cloned->local.size());
    auto* val = cloned->local[0].as<int>();
    ASSERT_NE(nullptr, val);
    EXPECT_EQ(1, *val);
    auto* clonedLocal = cloned->local[1].as<Label*>();
    ASSERT_NE(nullptr, clonedLocal);
    EXPECT_NE(&srcLocal, *clonedLocal);
    EXPECT_EQ(&targetArena, (*clonedLocal)->owner());
    EXPECT_EQ(9, (*clonedLocal)->bytecodeOffset);
    EXPECT_EQ(3, (*clonedLocal)->line);
    EXPECT_TRUE((*clonedLocal)->flags & Label::FLAG_RESOLVED);
    EXPECT_EQ(*clonedLocal, map[&srcLocal]);   // 登记映射

    // stack：Label* 槽与 const Label* 槽都收编
    ASSERT_EQ(2u, cloned->stack.size());
    auto* clonedStack = cloned->stack[0].as<Label*>();
    ASSERT_NE(nullptr, clonedStack);
    EXPECT_NE(&srcStack, *clonedStack);
    EXPECT_EQ(&targetArena, (*clonedStack)->owner());
    EXPECT_EQ(4, (*clonedStack)->bytecodeOffset);
    auto* clonedConst = cloned->stack[1].as<Label*>();
    ASSERT_NE(nullptr, clonedConst);
    EXPECT_NE(static_cast<const Label*>(&srcStackConst), *clonedConst);
    EXPECT_EQ(&targetArena, (*clonedConst)->owner());

    // 同 map 再克隆：同一源 Label 命中同一克隆（帧槽与 LabelNode 身份共享语义）
    auto* again      = static_cast<FrameNode*>(frame.clone(targetArena, map));
    auto* againLocal = again->local[1].as<Label*>();
    ASSERT_NE(nullptr, againLocal);
    EXPECT_EQ(*clonedLocal, *againLocal);
}

// 跨方法克隆 + 源方法消亡：帧槽 Label 归属目标 arena，且 roundtrip 字节保真。
// pre-fix：槽内 Label 直指源方法 arena，源亡后序列化即悬垂（MSVC 下读到失效
// flags/offset → 字节不一致；linux ASAN 下 use-after-free）。
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

TEST(FrameNodeCloneLabelTest, crossMethodCloneKeepsFrameLabelsOwnedAndBytesFaithful)
{
    ca::str::Utf8StringArena arena;
    const auto               bytes = buildClassWithUninitializedFrame(arena);
    ASSERT_FALSE(bytes.empty());

    // reader 局部作用域内解析成树（收编后树 Label 归源方法 arena）
    std::unique_ptr<ClassNode> source;
    MethodNode*                pick = nullptr;
    {
        ClassReader reader(arena, bytes.data(), static_cast<ca::i32>(bytes.size()));
        source = std::make_unique<ClassNode>();
        reader.accept(source.get(), 0);
    }
    for (auto* m : source->methods) {
        if (m && m->name.to_std_string() == "pick") {
            pick = m;
            break;
        }
    }
    ASSERT_NE(nullptr, pick);

    // 跨方法克隆：目标方法全新 arena，指令逐个 clone(arena, map)
    std::unique_ptr<MethodNode> target(std::make_unique<MethodNode>(pick->access,
                                                                    pick->name,
                                                                    pick->desc,
                                                                    pick->signature,
                                                                    pick->exceptions));
    std::unordered_map<Label*, Label*> clonedLabels;
    for (auto* insn = pick->instructions.getFirst(); insn; insn = insn->getNext()) {
        target->instructions.add(insn->clone(target->arena(), clonedLabels));
    }
    target->maxStack  = pick->maxStack;
    target->maxLocals = pick->maxLocals;

    // 帧槽 Label 必须归属目标 arena（pre-fix：直指源方法 arena）
    bool sawFrameLabel = false;
    for (auto* insn = target->instructions.getFirst(); insn; insn = insn->getNext()) {
        if (insn->getType() != AbstractInsnNode::FRAME) continue;
        auto* frame = static_cast<FrameNode*>(insn);
        for (auto* slots : {&frame->local, &frame->stack}) {
            for (const auto& slot : *slots) {
                if (auto* lbl = slot.as<Label*>()) {
                    sawFrameLabel = true;
                    ASSERT_EQ(&target->arena(), (*lbl)->owner());
                }
            }
        }
    }
    ASSERT_TRUE(sawFrameLabel);   // 用例确实携带 ITEM_Uninitialized Label 槽

    // 源方法（连同其内容 arena）消亡；pre-fix 此后序列化即悬垂
    source.reset();

    // 目标方法序列化：roundtrip 字节保真
    ClassWriter writer(arena, 0);
    writer.visit(Opcodes::V1_7,
                 Opcodes::ACC_PUBLIC | Opcodes::ACC_SUPER,
                 ZUtf8StringRef::from_static("FrameUninitTarget"),
                 ca::str::Utf8StringRef{},
                 ZUtf8StringRef::from_static("java/lang/Object"),
                 {});
    target->accept(&writer);
    writer.visitEnd();
    EXPECT_EQ(bytes, writer.toByteArray());
}

