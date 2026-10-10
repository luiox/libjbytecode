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

// ============================================================
// Empty class roundtrip
// ============================================================

TEST(EdgeCaseTest, emptyClass)
{
    ClassNode cn;
    cn.version   = 52;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("Empty");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    ClassWriter cw(asmArena);
    ASSERT_NO_THROW(cn.accept(&cw));
    auto bytes = cw.toByteArray();
    ASSERT_GT(bytes.size(), 20u);

    ClassNode   cn2;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    ASSERT_NO_THROW(reader.accept(&cn2, 0));
    EXPECT_EQ(cn.name, cn2.name);
    checkIdempotent(bytes);
}

TEST(EdgeCaseTest, emptyClassWithInterface)
{
    ClassNode cn;
    cn.version    = 52;
    cn.access     = Opcodes::ACC_PUBLIC;
    cn.name       = ZUtf8StringRef::from_static("Empty");
    cn.superName  = ZUtf8StringRef::from_static("java/lang/Object");
    cn.interfaces = {ZUtf8StringRef::from_static("java/io/Serializable")};

    ClassWriter cw(asmArena);
    ASSERT_NO_THROW(cn.accept(&cw));
    auto bytes = cw.toByteArray();
    ASSERT_GT(bytes.size(), 20u);

    ClassNode   cn2;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    ASSERT_NO_THROW(reader.accept(&cn2, 0));
    ASSERT_EQ(cn2.interfaces.size(), 1u)
        << "Expected java/io/Serializable but got " << cn2.interfaces.size() << " interfaces";
    EXPECT_EQ(cn2.interfaces[0], ZUtf8StringRef::from_static("java/io/Serializable"));
    checkIdempotent(bytes);
}

TEST(EdgeCaseTest, classWithStaticField)
{
    ClassNode cn;
    cn.version   = 52;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("HasField");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* fn = new FieldNode(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                             ZUtf8StringRef::from_static("count"),
                             ZUtf8StringRef::from_static("I"),
                             ZUtf8StringRef::from_static(""),
                             ca::core::Any());
    cn.fields.push_back(fn);

    ClassWriter cw(asmArena);
    ASSERT_NO_THROW(cn.accept(&cw));
    auto bytes = cw.toByteArray();

    ClassNode   cn2;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    ASSERT_NO_THROW(reader.accept(&cn2, 0));
    ASSERT_EQ(cn2.fields.size(), 1u);
    EXPECT_EQ(cn2.fields[0]->name, ZUtf8StringRef::from_static("count"));
    EXPECT_EQ(cn2.fields[0]->desc, ZUtf8StringRef::from_static("I"));
    checkIdempotent(bytes);
}

// ============================================================
// Method edge cases
// ============================================================

TEST(EdgeCaseTest, emptyConstructor)
{
    ClassNode cn;
    cn.version   = 52;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("CtorTest");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

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
    ASSERT_EQ(cn2.methods.size(), 1u);
    ASSERT_EQ(cn2.methods[0]->name, ZUtf8StringRef::from_static("<init>"));
    checkIdempotent(bytes);
}

TEST(EdgeCaseTest, abstractMethod)
{
    ClassNode cn;
    cn.version   = 52;
    cn.access    = Opcodes::ACC_PUBLIC | Opcodes::ACC_ABSTRACT;
    cn.name      = ZUtf8StringRef::from_static("AbstractTest");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* mn = new MethodNode(Opcodes::ACC_PUBLIC | Opcodes::ACC_ABSTRACT,
                              ZUtf8StringRef::from_static("foo"),
                              ZUtf8StringRef::from_static("()V"),
                              ZUtf8StringRef::from_static(""),
                              {});
    cn.methods.push_back(mn);

    ClassWriter cw(asmArena);
    ASSERT_NO_THROW(cn.accept(&cw));
    auto bytes = cw.toByteArray();

    ClassNode   cn2;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    ASSERT_NO_THROW(reader.accept(&cn2, 0));
    ASSERT_EQ(cn2.methods.size(), 1u);
    EXPECT_TRUE(cn2.methods[0]->access & Opcodes::ACC_ABSTRACT);
    checkIdempotent(bytes);
}

TEST(EdgeCaseTest, methodWithMaxStack)
{
    ClassNode cn;
    cn.version   = 52;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("MaxTest");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* mn = new MethodNode(Opcodes::ACC_PUBLIC,
                              ZUtf8StringRef::from_static("add"),
                              ZUtf8StringRef::from_static("(II)I"),
                              ZUtf8StringRef::from_static(""),
                              {});
    mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));
    mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 1));
    mn->instructions.add(new InsnNode(Opcodes::IADD));
    mn->instructions.add(new InsnNode(Opcodes::IRETURN));
    mn->maxStack  = 2;
    mn->maxLocals = 2;
    cn.methods.push_back(mn);

    ClassWriter cw(asmArena);
    ASSERT_NO_THROW(cn.accept(&cw));
    auto bytes = cw.toByteArray();

    ClassNode   cn2;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    ASSERT_NO_THROW(reader.accept(&cn2, 0));
    ASSERT_EQ(cn2.methods.size(), 1u);
    EXPECT_EQ(cn2.methods[0]->maxStack, 2);
    EXPECT_EQ(cn2.methods[0]->maxLocals, 2);
    checkIdempotent(bytes);
}
