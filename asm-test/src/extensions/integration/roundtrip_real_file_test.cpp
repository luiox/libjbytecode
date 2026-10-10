#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <utility>

#include "jbytecode/jbytecode.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace jbytecode::commons;
using namespace jbytecode::util;

static void expectSameMethodShape(const MethodNode* expected, const MethodNode* actual,
                                  size_t index)
{
    EXPECT_EQ(expected->name, actual->name) << "Method " << index << " name";
    EXPECT_EQ(expected->desc, actual->desc) << "Method " << index << " desc";
    EXPECT_EQ(expected->access, actual->access) << "Method " << index << " access";
    EXPECT_EQ(expected->maxStack, actual->maxStack) << "Method " << index << " maxStack";
    EXPECT_EQ(expected->maxLocals, actual->maxLocals) << "Method " << index << " maxLocals";
    EXPECT_EQ(expected->tryCatchBlocks.size(), actual->tryCatchBlocks.size())
        << "Method " << index << " try-catch count";
    EXPECT_EQ(expected->localVariables.size(), actual->localVariables.size())
        << "Method " << index << " local variable count";
    std::vector<std::pair<int, int>> expectedOpcodes;
    std::vector<std::pair<int, int>> actualOpcodes;
    for (size_t i = 0; i < expected->instructions.size(); ++i) {
        auto* insn = expected->instructions.get(i);
        if (insn->getType() != AbstractInsnNode::LABEL) {
            expectedOpcodes.emplace_back(insn->getType(), insn->getOpcode());
        }
    }
    for (size_t i = 0; i < actual->instructions.size(); ++i) {
        auto* insn = actual->instructions.get(i);
        if (insn->getType() != AbstractInsnNode::LABEL) {
            actualOpcodes.emplace_back(insn->getType(), insn->getOpcode());
        }
    }
    ASSERT_EQ(expectedOpcodes.size(), actualOpcodes.size())
        << "Method " << index << " executable instruction count";
    for (size_t i = 0; i < expectedOpcodes.size(); ++i) {
        EXPECT_EQ(expectedOpcodes[i].first, actualOpcodes[i].first)
            << "Method " << index << " insn " << i << " type";
        EXPECT_EQ(expectedOpcodes[i].second, actualOpcodes[i].second)
            << "Method " << index << " insn " << i << " opcode";
    }
}

// ---------- Phase 1: Parse ----------

TEST(RealFileRoundTripTest, parseIssue307600)
{
    auto path = locateResource("Issue307600.class");
    ASSERT_FALSE(path.empty());
    auto bytes = readFile(path);
    ASSERT_FALSE(bytes.empty());
    ClassNode   cn;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    EXPECT_EQ(reader.getClassName(),
              ZUtf8StringRef::from_static(
                  "org/apache/jsp/secure/replacement/entry/F_005fReplacementEntryForm_jsp"));
    reader.accept(&cn, 0);
    EXPECT_EQ(cn.methods.size(), 85u);
    EXPECT_EQ(cn.fields.size(), 36u);
}

// ---------- Phase 2: Parse + Write ----------

TEST(RealFileRoundTripTest, writeIssue307600)
{
    auto path = locateResource("Issue307600.class");
    ASSERT_FALSE(path.empty());
    auto bytes = readFile(path);
    ASSERT_FALSE(bytes.empty());

    ClassNode   cn;
    ClassReader reader(asmArena, bytes.data(), bytes.size());
    reader.accept(&cn, 0);

    ClassWriter writer(asmArena);
    cn.accept(&writer);
    auto output = writer.toByteArray();
    ASSERT_GT(output.size(), 20u);
    EXPECT_EQ(output[0], 0xCA);
    EXPECT_EQ(output[1], 0xFE);
    EXPECT_EQ(output[2], 0xBA);
    EXPECT_EQ(output[3], 0xBE);
    printf("[OK] Issue307600: input=%zu output=%zu (%.1f%%)\n",
           bytes.size(),
           output.size(),
           100.0 * (1.0 - (double)output.size() / (double)bytes.size()));

    // Idempotency: re-parse the output
    ClassNode   cn2;
    ClassReader reader2(asmArena, output.data(), output.size());
    reader2.accept(&cn2, 0);
    EXPECT_EQ(cn2.methods.size(), cn.methods.size());
    EXPECT_EQ(cn2.fields.size(), cn.fields.size());

    // Write again
    ClassWriter writer2(asmArena);
    cn2.accept(&writer2);
    auto output2 = writer2.toByteArray();
    ASSERT_GT(output2.size(), 20u);

    ClassNode   cn3;
    ClassReader reader3(asmArena, output2.data(), output2.size());
    reader3.accept(&cn3, 0);

    // 结构化对比：两轮输出在语义结构上等价
    EXPECT_EQ(cn2.methods.size(), cn.methods.size()) << "Method count should match";
    EXPECT_EQ(cn2.fields.size(), cn.fields.size()) << "Field count should match";
    EXPECT_EQ(cn3.methods.size(), cn2.methods.size()) << "Second pass method count should match";
    EXPECT_EQ(cn3.fields.size(), cn2.fields.size()) << "Second pass field count should match";
    for (size_t i = 0; i < cn.methods.size() && i < cn2.methods.size(); i++) {
        expectSameMethodShape(cn.methods[i], cn2.methods[i], i);
    }
    for (size_t i = 0; i < cn2.methods.size() && i < cn3.methods.size(); i++) {
        expectSameMethodShape(cn2.methods[i], cn3.methods[i], i);
    }
    printf("[INFO] Structural check passed: %zu methods, %zu fields\n",
           cn2.methods.size(),
           cn2.fields.size());
}
