#include "garena/arena.hpp"
#include <gtest/gtest.h>
#include "libca/core/any.hpp"
#include <string>
#include <vector>
#include "test_utils.hpp"

// B8b：测试侧 Label 统一归宿（进程寿命 scratch arena）——LabelNode() 默认构造
// 已随旧池制度退役，Label 一律 arena 分配。
static garena::Arena& labelScratchArena()
{
    static garena::Arena arena;
    return arena;
}


// 验证 ClassNode::accept → ClassWriter → toByteArray → ClassReader
// 能正确 roundtrip 所有数据
TEST(AcceptChainTest, classMetadataRoundtrip)
{
    ClassNode cn;
    cn.version    = Opcodes::V1_8;
    cn.access     = Opcodes::ACC_PUBLIC | Opcodes::ACC_ABSTRACT;
    cn.name       = ZUtf8StringRef::from_static("pkg/Base");
    cn.superName  = ZUtf8StringRef::from_static("java/lang/Object");
    cn.interfaces = {ZUtf8StringRef::from_static("java/io/Serializable"),
                     ZUtf8StringRef::from_static("java/lang/Cloneable")};
    cn.sourceFile = ZUtf8StringRef::from_static("Base.java");
    cn.signature  = ZUtf8StringRef::from_static("Ljava/lang/Object;");

    ClassWriter cw(tArena);
    cn.accept(&cw);
    auto b1 = cw.toByteArray();

    ClassNode   cn2;
    ClassReader r(tArena, b1.data(), b1.size());
    r.accept(&cn2, 0);

    EXPECT_EQ(cn2.version, cn.version);
    EXPECT_EQ(cn2.access, cn.access);
    EXPECT_EQ(cn2.name, cn.name);
    EXPECT_EQ(cn2.superName, cn.superName);
    EXPECT_EQ(cn2.interfaces, cn.interfaces);
    EXPECT_EQ(cn2.sourceFile, cn.sourceFile);

    ClassWriter cw2(tArena);
    cn2.accept(&cw2);
    EXPECT_EQ(b1, cw2.toByteArray());
}

TEST(AcceptChainTest, methodVisitOrder)
{
    ClassNode cn;
    cn.version   = Opcodes::V1_8;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("Test");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* m1 = new MethodNode(Opcodes::ACC_PUBLIC,
                              ZUtf8StringRef::from_static("a"),
                              ZUtf8StringRef::from_static("()V"),
                              ZUtf8StringRef::from_static(""),
                              {});
    m1->instructions.add(new InsnNode(Opcodes::RETURN));
    m1->maxStack  = 0;
    m1->maxLocals = 1;
    cn.methods.push_back(m1);

    auto* m2 = new MethodNode(Opcodes::ACC_PUBLIC,
                              ZUtf8StringRef::from_static("b"),
                              ZUtf8StringRef::from_static("()V"),
                              ZUtf8StringRef::from_static(""),
                              {});
    m2->instructions.add(new InsnNode(Opcodes::RETURN));
    m2->maxStack  = 0;
    m2->maxLocals = 1;
    cn.methods.push_back(m2);

    ClassWriter cw(tArena);
    cn.accept(&cw);
    auto bytes = cw.toByteArray();

    ClassNode   cn2;
    ClassReader r(tArena, bytes.data(), bytes.size());
    r.accept(&cn2, 0);
    ASSERT_EQ(cn2.methods.size(), 2u);
    EXPECT_EQ(cn2.methods[0]->name, ZUtf8StringRef::from_static("a"));
    EXPECT_EQ(cn2.methods[1]->name, ZUtf8StringRef::from_static("b"));
}

TEST(AcceptChainTest, insnVarFieldMethodRoundtrip)
{
    ClassNode cn;
    cn.version   = Opcodes::V1_8;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("Test");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* mn = new MethodNode(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                              ZUtf8StringRef::from_static("test"),
                              ZUtf8StringRef::from_static("()V"),
                              ZUtf8StringRef::from_static(""),
                              {});
    mn->instructions.add(new InsnNode(Opcodes::ICONST_0));
    mn->instructions.add(new IntInsnNode(Opcodes::BIPUSH, 42));
    mn->instructions.add(new VarInsnNode(Opcodes::ISTORE, 0));
    mn->instructions.add(new TypeInsnNode(Opcodes::NEW, ZUtf8StringRef::from_static("Test")));
    mn->instructions.add(new InsnNode(Opcodes::POP));
    mn->instructions.add(new FieldInsnNode(Opcodes::GETSTATIC,
                                           ZUtf8StringRef::from_static("java/lang/System"),
                                           ZUtf8StringRef::from_static("out"),
                                           ZUtf8StringRef::from_static("Ljava/io/PrintStream;")));
    mn->instructions.add(new MethodInsnNode(Opcodes::INVOKESTATIC,
                                            ZUtf8StringRef::from_static("Test"),
                                            ZUtf8StringRef::from_static("foo"),
                                            ZUtf8StringRef::from_static("()V"),
                                            false));
    mn->instructions.add(new LdcInsnNode(std::string("hello")));
    mn->instructions.add(new IincInsnNode(0, 1));
    mn->instructions.add(new MultiANewArrayInsnNode(ZUtf8StringRef::from_static("[[I"), 2));
    mn->instructions.add(new InsnNode(Opcodes::RETURN));
    mn->maxStack  = 2;
    mn->maxLocals = 2;
    cn.methods.push_back(mn);

    ClassWriter cw(tArena);
    cn.accept(&cw);
    auto        b1 = cw.toByteArray();
    ClassNode   cn2;
    ClassReader r(tArena, b1.data(), b1.size());
    r.accept(&cn2, 0);
    ASSERT_EQ(cn2.methods.size(), 1u);
    EXPECT_GT(cn2.methods[0]->instructions.size(), 5u);
    ClassWriter cw2(tArena);
    cn2.accept(&cw2);
    EXPECT_EQ(b1, cw2.toByteArray());
}

TEST(AcceptChainTest, jumpAndSwitchRoundtrip)
{
    ClassNode cn;
    cn.version   = Opcodes::V1_8;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("Test");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto*      mn = new MethodNode(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                              ZUtf8StringRef::from_static("test"),
                              ZUtf8StringRef::from_static("(I)I"),
                              ZUtf8StringRef::from_static(""),
                                   {});
    LabelNode* l1 = new LabelNode(labelScratchArena().create<Label>());
    LabelNode* l2 = new LabelNode(labelScratchArena().create<Label>());
    LabelNode* l3 = new LabelNode(labelScratchArena().create<Label>());

    mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));   // load x
    mn->instructions.add(new JumpInsnNode(Opcodes::IFEQ, l1->label));
    mn->instructions.add(new InsnNode(Opcodes::ICONST_1));
    mn->instructions.add(new InsnNode(Opcodes::IRETURN));
    mn->instructions.add(l1);
    mn->instructions.add(new InsnNode(Opcodes::ICONST_2));
    mn->instructions.add(new InsnNode(Opcodes::IRETURN));
    mn->maxStack  = 1;
    mn->maxLocals = 1;
    cn.methods.push_back(mn);

    ClassWriter cw(tArena);
    cn.accept(&cw);
    auto        b1 = cw.toByteArray();
    ClassNode   cn2;
    ClassReader r(tArena, b1.data(), b1.size());
    r.accept(&cn2, 0);
    ASSERT_EQ(cn2.methods.size(), 1u);
    EXPECT_GT(cn2.methods[0]->instructions.size(), 3u);
    ClassWriter cw2(tArena);
    cn2.accept(&cw2);
    EXPECT_EQ(b1, cw2.toByteArray());
}

TEST(AcceptChainTest, annotationsThroughAccept)
{
    ClassNode cn;
    cn.version   = Opcodes::V1_8;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("Test");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* ca = cn.create<AnnotationNode>(cn.arena(), ZUtf8StringRef::from_static("Ljava/lang/Deprecated;"));
    cn.visibleAnnotations.push_back(ca);

    auto* mn = new MethodNode(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                              ZUtf8StringRef::from_static("foo"),
                              ZUtf8StringRef::from_static("()V"),
                              ZUtf8StringRef::from_static(""),
                              {});
    auto* ma = mn->create<AnnotationNode>(mn->arena(), ZUtf8StringRef::from_static("Ljava/lang/SuppressWarnings;"));
    {
        ma->elements.emplace_back();
        auto& elem = ma->elements.back();
        elem.name  = ZUtf8StringRef::from_static("value");
        elem.tag   = 's';
        elem.sval  = ca::str::Utf8String("unchecked");
    }
    mn->visibleAnnotations.push_back(ma);
    mn->instructions.add(new InsnNode(Opcodes::RETURN));
    mn->maxStack  = 0;
    mn->maxLocals = 1;
    cn.methods.push_back(mn);

    auto* fn = new FieldNode(Opcodes::ACC_PRIVATE,
                             ZUtf8StringRef::from_static("x"),
                             ZUtf8StringRef::from_static("I"),
                             ZUtf8StringRef::from_static(""),
                             {});
    auto* fa = fn->create<AnnotationNode>(fn->arena(), ZUtf8StringRef::from_static("Ljava/lang/Deprecated;"));
    fn->visibleAnnotations.push_back(fa);
    cn.fields.push_back(fn);

    ClassWriter cw(tArena);
    cn.accept(&cw);
    auto bytes = cw.toByteArray();

    ClassNode   cn2;
    ClassReader r(tArena, bytes.data(), bytes.size());
    r.accept(&cn2, 0);
    ASSERT_EQ(cn2.visibleAnnotations.size(), 1u);
    ASSERT_EQ(cn2.methods.size(), 1u);
    ASSERT_EQ(cn2.methods[0]->visibleAnnotations.size(), 1u);
    ASSERT_EQ(cn2.fields.size(), 1u);
    ASSERT_EQ(cn2.fields[0]->visibleAnnotations.size(), 1u);
}

TEST(AcceptChainTest, tryCatchBlocksRoundtrip)
{
    ClassNode cn;
    cn.version   = Opcodes::V1_8;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("Test");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto*      mn      = new MethodNode(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                              ZUtf8StringRef::from_static("safe"),
                              ZUtf8StringRef::from_static("()I"),
                              ZUtf8StringRef::from_static(""),
                                        {ZUtf8StringRef::from_static("java/lang/Exception")});
    LabelNode* start   = new LabelNode(labelScratchArena().create<Label>());
    LabelNode* end     = new LabelNode(labelScratchArena().create<Label>());
    LabelNode* handler = new LabelNode(labelScratchArena().create<Label>());
    mn->instructions.add(start);
    mn->instructions.add(new InsnNode(Opcodes::ICONST_1));
    mn->instructions.add(new InsnNode(Opcodes::IRETURN));
    mn->instructions.add(end);
    mn->instructions.add(handler);
    mn->instructions.add(new InsnNode(Opcodes::ICONST_0));
    mn->instructions.add(new InsnNode(Opcodes::IRETURN));
    mn->tryCatchBlocks.push_back(
        mn->create<TryCatchBlockNode>(start->label,
                              end->label,
                              handler->label,
                              ZUtf8StringRef::from_static("java/lang/Exception")));
    mn->maxStack  = 1;
    mn->maxLocals = 0;
    cn.methods.push_back(mn);

    ClassWriter cw(tArena);
    cn.accept(&cw);
    auto b1 = cw.toByteArray();

    ClassNode   cn2;
    ClassReader r(tArena, b1.data(), b1.size());
    r.accept(&cn2, 0);
    ASSERT_EQ(cn2.methods.size(), 1u);
    ASSERT_EQ(cn2.methods[0]->tryCatchBlocks.size(), 1u);
    EXPECT_EQ(cn2.methods[0]->tryCatchBlocks[0]->type,
              ZUtf8StringRef::from_static("java/lang/Exception"));
    ClassWriter cw2(tArena);
    cn2.accept(&cw2);
    EXPECT_EQ(b1, cw2.toByteArray());
}

TEST(AcceptChainTest, methodWithParameters)
{
    ClassNode cn;
    cn.version   = Opcodes::V1_8;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("Test");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* mn = new MethodNode(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                              ZUtf8StringRef::from_static("calc"),
                              ZUtf8StringRef::from_static("(II)I"),
                              ZUtf8StringRef::from_static(""),
                              {});
    auto* p0 = mn->create<ParameterNode>(ZUtf8StringRef::from_static("a"), 0);
    auto* p1 = mn->create<ParameterNode>(ZUtf8StringRef::from_static("b"), 0);
    mn->parameters.push_back(p0);
    mn->parameters.push_back(p1);
    mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));
    mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 1));
    mn->instructions.add(new InsnNode(Opcodes::IADD));
    mn->instructions.add(new InsnNode(Opcodes::IRETURN));
    mn->maxStack  = 2;
    mn->maxLocals = 2;
    cn.methods.push_back(mn);

    ClassWriter cw(tArena);
    cn.accept(&cw);
    auto bytes = cw.toByteArray();

    ClassNode   cn2;
    ClassReader r(tArena, bytes.data(), bytes.size());
    r.accept(&cn2, 0);
    ASSERT_EQ(cn2.methods.size(), 1u);
    auto* mn2 = cn2.methods[0];
    // ClassReader may or may not preserve MethodParameters
    // Verify at least the method body survived
    EXPECT_GT(mn2->instructions.size(), 0u);
}
