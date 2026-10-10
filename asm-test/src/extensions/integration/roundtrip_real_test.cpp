#include <gtest/gtest.h>
#include <vector>
#include "jbytecode/jbytecode.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace jbytecode::commons;
using namespace jbytecode::util;

// 幂等性检查：写出→读回→再写出，两轮输出必须一致
static void checkIdempotent(const std::vector<ca::u8>& firstOutput)
{
    ClassNode   cn;
    ClassReader reader(asmArena, firstOutput.data(), firstOutput.size());
    reader.accept(&cn, 0);
    ClassWriter writer(asmArena);
    cn.accept(&writer);
    auto secondOutput = writer.toByteArray();
    EXPECT_EQ(firstOutput, secondOutput) << "Idempotency violation: round 1 ≠ round 2";
}

// Inline: build a class node, serialize, re-parse, verify
static ClassNode* makeTestClass()
{
    auto* cn = new ClassNode();
    cn->visit(61 << 16,
              Opcodes::ACC_PUBLIC,
              ZUtf8StringRef::from_static("pkg/Test"),
              ZUtf8StringRef::from_static(""),
              ZUtf8StringRef::from_static("java/lang/Object"),
              {});

    // Add a simple method: public static int add(int a, int b) { return a + b; }
    MethodNode* mn = new MethodNode(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
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
    cn->methods.push_back(mn);

    cn->visitEnd();
    return cn;
}

TEST(RoundTripTest, inlineSmoke)
{
    ClassNode*  cn = makeTestClass();
    ClassWriter writer(asmArena);
    cn->accept(&writer);
    auto out = writer.toByteArray();
    ASSERT_FALSE(out.empty());
    ASSERT_EQ(0xCA, out[0]);
    ASSERT_EQ(0xFE, out[1]);
    ASSERT_EQ(0xBA, out[2]);
    ASSERT_EQ(0xBE, out[3]);

    // Re-parse
    ClassReader r2(asmArena, out.data(), out.size());
    ClassNode   cn2;
    ASSERT_NO_THROW(r2.accept(&cn2, 0));
    EXPECT_EQ(cn2.name, ZUtf8StringRef::from_static("pkg/Test"));
    EXPECT_EQ(cn2.methods.size(), 1);
    EXPECT_EQ(cn2.methods[0]->name, ZUtf8StringRef::from_static("add"));
    EXPECT_EQ(cn2.methods[0]->instructions.size(), 4);

    checkIdempotent(out);

    delete cn;
}

TEST(RoundTripTest, computeMaxsFlag)
{
    ClassNode*  cn = makeTestClass();
    ClassWriter writer(asmArena, ClassWriter::COMPUTE_MAXS);
    cn->accept(&writer);
    std::vector<ca::u8> out;
    ASSERT_NO_THROW(out = writer.toByteArray());
    ASSERT_FALSE(out.empty());

    ClassReader r2(asmArena, out.data(), out.size());
    ClassNode   cn2;
    ASSERT_NO_THROW(r2.accept(&cn2, 0));
    EXPECT_EQ(cn2.name, ZUtf8StringRef::from_static("pkg/Test"));

    checkIdempotent(out);

    delete cn;
}
