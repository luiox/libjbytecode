#include <gtest/gtest.h>
#include "libca/core/any.hpp"
#include <string>
#include <vector>

#include "jbytecode/jbytecode.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace jbytecode::commons;
using namespace jbytecode::util;

// 幂等性检查：写出→读回→再写出，两轮输出必须一致
static void checkIdempotent(const std::vector<uint8_t>& firstOutput)
{
    ClassNode   cn;
    ClassReader reader(asmArena, firstOutput.data(), firstOutput.size());
    reader.accept(&cn, 0);
    ClassWriter writer(asmArena);
    cn.accept(&writer);
    auto secondOutput = writer.toByteArray();
    EXPECT_EQ(firstOutput, secondOutput) << "Idempotency violation: round 1 ≠ round 2";
}

TEST(AnnotationRoundtripTest, classVisibleAnnotation)
{
    ClassNode cn;
    cn.version   = 52;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("AnnTest");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* an = cn.create<AnnotationNode>(cn.arena(), ZUtf8StringRef::from_static("Ljava/lang/Deprecated;"));
    cn.visibleAnnotations.push_back(an);

    ClassWriter cw(asmArena);
    ASSERT_NO_THROW(cn.accept(&cw));
    auto bytes = cw.toByteArray();
    ASSERT_GT(bytes.size(), 30u);

    ClassNode   cn2;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    ASSERT_NO_THROW(reader.accept(&cn2, 0));
    ASSERT_EQ(cn2.visibleAnnotations.size(), 1u);
    EXPECT_EQ(cn2.visibleAnnotations[0]->desc,
              ZUtf8StringRef::from_static("Ljava/lang/Deprecated;"));
    checkIdempotent(bytes);
}

TEST(AnnotationRoundtripTest, classWithAnnotationAndMethod)
{
    ClassNode cn;
    cn.version   = 52;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("Test2");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* an = cn.create<AnnotationNode>(cn.arena(), ZUtf8StringRef::from_static("Ljava/lang/Deprecated;"));
    cn.visibleAnnotations.push_back(an);

    auto* mn = new MethodNode(Opcodes::ACC_PUBLIC,
                              ZUtf8StringRef::from_static("<init>"),
                              ZUtf8StringRef::from_static("()V"),
                              ZUtf8StringRef::from_static(""),
                              {});
    mn->instructions.add(new VarInsnNode(Opcodes::ALOAD, 0));
    mn->instructions.add(new MethodInsnNode(Opcodes::INVOKESPECIAL,
                                            ZUtf8StringRef::from_static("java/lang/Object"),
                                            ZUtf8StringRef::from_static("<init>"),
                                            ZUtf8StringRef::from_static("()V"),
                                            false));
    mn->instructions.add(new InsnNode(Opcodes::RETURN));
    mn->maxStack  = 1;
    mn->maxLocals = 1;
    cn.methods.push_back(mn);

    ClassWriter cw(asmArena);
    ASSERT_NO_THROW(cn.accept(&cw));
    auto bytes = cw.toByteArray();

    ClassNode   cn2;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    ASSERT_NO_THROW(reader.accept(&cn2, 0));
    ASSERT_EQ(cn2.visibleAnnotations.size(), 1u);
    EXPECT_EQ(cn2.visibleAnnotations[0]->desc,
              ZUtf8StringRef::from_static("Ljava/lang/Deprecated;"));
    ASSERT_EQ(cn2.methods.size(), 1u);
    checkIdempotent(bytes);
}

TEST(AnnotationRoundtripTest, methodAnnotation)
{
    ClassNode cn;
    cn.version   = 52;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("MethAnn");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* mn  = new MethodNode(Opcodes::ACC_PUBLIC,
                              ZUtf8StringRef::from_static("foo"),
                              ZUtf8StringRef::from_static("()V"),
                              ZUtf8StringRef::from_static(""),
                               {});
    auto* man = mn->create<AnnotationNode>(mn->arena(), ZUtf8StringRef::from_static("Ljava/lang/Deprecated;"));
    mn->visibleAnnotations.push_back(man);
    mn->instructions.add(new VarInsnNode(Opcodes::ALOAD, 0));
    mn->instructions.add(new MethodInsnNode(Opcodes::INVOKESPECIAL,
                                            ZUtf8StringRef::from_static("java/lang/Object"),
                                            ZUtf8StringRef::from_static("<init>"),
                                            ZUtf8StringRef::from_static("()V"),
                                            false));
    mn->instructions.add(new InsnNode(Opcodes::RETURN));
    mn->maxStack  = 1;
    mn->maxLocals = 1;
    cn.methods.push_back(mn);

    ClassWriter cw(asmArena);
    ASSERT_NO_THROW(cn.accept(&cw));
    auto bytes = cw.toByteArray();

    ClassNode   cn2;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    ASSERT_NO_THROW(reader.accept(&cn2, 0));
    ASSERT_EQ(cn2.methods.size(), 1u);
    EXPECT_EQ(cn2.methods[0]->visibleAnnotations.size(), 1u)
        << "method-level @Deprecated annotation lost in round-trip";
    checkIdempotent(bytes);
}

TEST(AnnotationRoundtripTest, methodParameterAnnotation)
{
    ClassNode cn;
    cn.version   = 52;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("ParamAnn");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* mn  = new MethodNode(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                              ZUtf8StringRef::from_static("greet"),
                              ZUtf8StringRef::from_static("(Ljava/lang/String;)V"),
                              ZUtf8StringRef::from_static(""),
                               {});
    auto* pan = mn->create<AnnotationNode>(mn->arena(), ZUtf8StringRef::from_static("Ljava/lang/Deprecated;"));
    mn->visibleParameterAnnotations.push_back(pan);
    mn->visibleParameterIndices.push_back(0);
    mn->instructions.add(new InsnNode(Opcodes::RETURN));
    mn->maxStack  = 0;
    mn->maxLocals = 1;
    cn.methods.push_back(mn);

    ClassWriter cw(asmArena);
    ASSERT_NO_THROW(cn.accept(&cw));
    auto bytes = cw.toByteArray();

    ClassNode   cn2;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    ASSERT_NO_THROW(reader.accept(&cn2, 0));
    ASSERT_EQ(cn2.methods.size(), 1u);
    EXPECT_EQ(cn2.methods[0]->visibleParameterAnnotations.size(), 1u)
        << "method parameter annotation lost in round-trip";
    checkIdempotent(bytes);
}

TEST(AnnotationRoundtripTest, fieldAnnotation)
{
    ClassNode cn;
    cn.version   = 52;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("FieldAnn");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* fn = new FieldNode(Opcodes::ACC_PUBLIC,
                             ZUtf8StringRef::from_static("x"),
                             ZUtf8StringRef::from_static("I"),
                             ZUtf8StringRef::from_static(""),
                             ca::core::Any());
    auto* an = cn.create<AnnotationNode>(cn.arena(), ZUtf8StringRef::from_static("Ljava/lang/Deprecated;"));
    fn->visibleAnnotations.push_back(an);
    cn.fields.push_back(fn);

    ClassWriter cw(asmArena);
    ASSERT_NO_THROW(cn.accept(&cw));
    auto bytes = cw.toByteArray();

    ClassNode   cn2;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    ASSERT_NO_THROW(reader.accept(&cn2, 0));
    ASSERT_EQ(cn2.fields.size(), 1u);
    EXPECT_EQ(cn2.fields[0]->name, ZUtf8StringRef::from_static("x"));
    checkIdempotent(bytes);
}
