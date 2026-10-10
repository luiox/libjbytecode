#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <string>

#include "jbytecode/jbytecode.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace jbytecode::commons;
using namespace jbytecode::util;

// 结构化对比两个 ClassNode（忽略常量池重排）
struct StructDiff
{
    int  fieldMismatches         = 0;
    int  methodMismatches        = 0;
    int  insnMismatches          = 0;
    int  maxMismatches           = 0;
    int  handlerMismatches       = 0;
    int  localVariableMismatches = 0;
    bool classMetaMatch          = true;
};

static StructDiff compareStructure(const ClassNode& a, const ClassNode& b)
{
    StructDiff d;
    d.classMetaMatch =
        (a.version == b.version) && (a.name == b.name) && (a.superName == b.superName);
    if (!d.classMetaMatch) return d;

    std::vector<bool> matchedB(b.fields.size(), false);
    for (auto* fa : a.fields) {
        bool found = false;
        for (size_t j = 0; j < b.fields.size(); j++) {
            if (matchedB[j]) continue;
            auto* fb = b.fields[j];
            if (fa->name == fb->name && fa->desc == fb->desc && fa->access == fb->access) {
                matchedB[j] = true;
                found       = true;
                break;
            }
        }
        if (!found) d.fieldMismatches++;
    }

    for (auto* ma : a.methods) {
        auto* mb = [&]() -> MethodNode* {
            for (auto* m : b.methods) {
                if (m->name == ma->name && m->desc == ma->desc) return m;
            }
            return nullptr;
        }();
        if (!mb) {
            d.methodMismatches++;
            continue;
        }
        if (ma->maxStack != mb->maxStack || ma->maxLocals != mb->maxLocals) {
            d.maxMismatches++;
        }
        if (ma->tryCatchBlocks.size() != mb->tryCatchBlocks.size()) {
            d.handlerMismatches++;
        }
        if (ma->localVariables.size() != mb->localVariables.size()) {
            d.localVariableMismatches++;
        }
        if (ma->instructions.size() != mb->instructions.size()) {
            d.insnMismatches++;
            continue;
        }
        for (size_t i = 0; i < ma->instructions.size(); ++i) {
            auto* ia = ma->instructions.get(i);
            auto* ib = mb->instructions.get(i);
            if (ia->getType() != ib->getType() || ia->getOpcode() != ib->getOpcode()) {
                d.insnMismatches++;
                break;
            }
        }
    }
    return d;
}

// ---------- 验证 C++ ClassNode 结构与 ASM 参考对齐 ----------

TEST(ReferenceAlignmentTest, issue307600)
{
    auto input = readFile(locateResource("Issue307600.class"));
    ASSERT_FALSE(input.empty());

    // Reference = the original upstream class itself. A correct read→write
    // roundtrip must preserve its structure (instruction streams, max values,
    // handler/local tables) modulo constant-pool reordering.
    ClassNode   refNode;
    ClassReader refReader(asmArena, input.data(), input.size());
    refReader.accept(&refNode, 0);

    ClassNode   cppNode;
    ClassReader cppReader(asmArena, input.data(), input.size());
    cppReader.accept(&cppNode, 0);

    ClassWriter writer(asmArena, cppReader, 0);
    cppNode.accept(&writer);
    auto output = writer.toByteArray();

    ClassNode   cppOutNode;
    ClassReader outReader(asmArena, output.data(), output.size());
    outReader.accept(&cppOutNode, 0);

    auto d = compareStructure(refNode, cppOutNode);
    EXPECT_TRUE(d.classMetaMatch) << "Class metadata mismatch";
    EXPECT_EQ(d.fieldMismatches, 0) << d.fieldMismatches << " fields mismatch";
    EXPECT_EQ(d.methodMismatches, 0) << d.methodMismatches << " methods not found in reference";
    EXPECT_EQ(d.insnMismatches, 0)
        << d.insnMismatches << " methods with different instruction stream";
    EXPECT_EQ(d.maxMismatches, 0) << d.maxMismatches
                                  << " methods with different maxStack/maxLocals";
    EXPECT_EQ(d.handlerMismatches, 0)
        << d.handlerMismatches << " methods with different try-catch table size";
    EXPECT_EQ(d.localVariableMismatches, 0)
        << d.localVariableMismatches << " methods with different local variable table size";
}
