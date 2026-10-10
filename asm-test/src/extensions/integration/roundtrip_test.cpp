#include <gtest/gtest.h>
#include <vector>
#include <cstdint>

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

TEST(RoundTripTest, testFileAccess)
{
    auto readFile = [](const std::string& name) {
        auto          path = locateResource(name);
        std::ifstream in(path, std::ios::binary);
        if (!in) return std::vector<uint8_t>();
        in.seekg(0, std::ios::end);
        auto fileSize = in.tellg();
        if (fileSize <= 0) return std::vector<uint8_t>();
        auto sz = static_cast<size_t>(fileSize);
        in.seekg(0, std::ios::beg);
        std::vector<uint8_t> d(sz);
        in.read(reinterpret_cast<char*>(d.data()), static_cast<std::streamsize>(sz));
        return d;
    };

    auto d1 = readFile("Issue307600.class");
    ASSERT_FALSE(d1.empty());
    ASSERT_EQ(0xCAFEBABE, readMagic(d1));

    auto d2 = readFile("Issue311642.class");
    ASSERT_FALSE(d2.empty());
    ASSERT_EQ(0xCAFEBABE, readMagic(d2));
}

TEST(RoundTripTest, minimalRoundTrip)
{
    ClassNode cn;
    cn.version   = 52;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("Test");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    auto* mn      = new MethodNode(Opcodes::ACC_PUBLIC,
                              ZUtf8StringRef::from_static("<init>"),
                              ZUtf8StringRef::from_static("()V"),
                              ZUtf8StringRef::from_static(""),
                                   {});
    mn->maxStack  = 1;
    mn->maxLocals = 1;
    cn.methods.push_back(mn);

    ClassWriter cw(asmArena);
    ASSERT_NO_THROW(cn.accept(&cw));
    auto bytes = cw.toByteArray();
    ASSERT_GT(bytes.size(), 20);
    ASSERT_EQ(0xCAFEBABE, readMagic(bytes));

    // 读回验证
    ClassNode   cn2;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    ASSERT_NO_THROW(reader.accept(&cn2, 0));
    ASSERT_EQ(cn.name, cn2.name);
    ASSERT_EQ(cn.superName, cn2.superName);
    ASSERT_EQ(cn.methods.size(), cn2.methods.size());
    ASSERT_EQ(cn.methods[0]->name, cn2.methods[0]->name);

    checkIdempotent(bytes);
}
