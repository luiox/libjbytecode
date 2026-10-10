#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "test_utils.hpp"
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"
#include "jbytecode/tree/nodes.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;

namespace {

struct RoundTripResult
{
    ClassNode           first;
    ClassNode           second;
    std::vector<ca::u8> output;
};

std::string toString(ca::str::Utf8StringRef value)
{
    if (!value.data()) return {};
    return value.to_std_string();
}

std::vector<ca::u8> loadClassResource(const std::string& resource)
{
    auto bytes = test::getResource("origin/" + resource);
    if (bytes.empty()) {
        bytes = test::getAsmTestResource("test_resource/origin/" + resource);
    }
    return bytes;
}

std::unique_ptr<RoundTripResult> roundTripViaTree(const std::vector<ca::u8>& input, int writerFlags = 0)
{
    auto result = std::make_unique<RoundTripResult>();

    ClassReader reader(asmArena, input.data(), static_cast<ca::i32>(input.size()));
    reader.accept(&result->first, 0);

    ClassWriter writer(asmArena, writerFlags);
    result->first.accept(&writer);
    result->output = writer.toByteArray();

    ClassReader reload(asmArena, result->output.data(), static_cast<ca::i32>(result->output.size()));
    reload.accept(&result->second, 0);
    return result;
}

MethodNode* findMethod(ClassNode& clazz, ca::str::Utf8StringRef name)
{
    for (auto* method : clazz.methods) {
        if (method && method->name == name) return method;
    }
    return nullptr;
}

bool hasInstructionType(const MethodNode& method, int type)
{
    for (auto* insn = method.instructions.getFirst(); insn; insn = insn->nextInsn) {
        if (insn->getType() == type) return true;
    }
    return false;
}

bool classHasInstructionType(const ClassNode& clazz, int type)
{
    for (auto* method : clazz.methods) {
        if (method && hasInstructionType(*method, type)) return true;
    }
    return false;
}

bool hasTypeAnnotations(const ClassNode& clazz)
{
    if (!clazz.visibleTypeAnnotations.empty() || !clazz.invisibleTypeAnnotations.empty())
        return true;
    for (auto* field : clazz.fields) {
        if (field &&
            (!field->visibleTypeAnnotations.empty() || !field->invisibleTypeAnnotations.empty()))
            return true;
    }
    for (auto* component : clazz.recordComponents) {
        if (component && (!component->visibleTypeAnnotations.empty() ||
                          !component->invisibleTypeAnnotations.empty()))
            return true;
    }
    for (auto* method : clazz.methods) {
        if (!method) continue;
        if (!method->visibleTypeAnnotations.empty() || !method->invisibleTypeAnnotations.empty() ||
            !method->visibleLocalVariableAnnotations.empty() ||
            !method->invisibleLocalVariableAnnotations.empty())
            return true;
        for (auto* insn = method->instructions.getFirst(); insn; insn = insn->nextInsn) {
            if (!insn->visibleTypeAnnotations.empty() || !insn->invisibleTypeAnnotations.empty())
                return true;
        }
    }
    return false;
}

std::size_t instructionCount(const ClassNode& clazz)
{
    std::size_t count = 0;
    for (auto* method : clazz.methods) {
        if (method) count += static_cast<std::size_t>(method->instructions.size());
    }
    return count;
}

std::vector<ca::u8> writeTree(ClassNode& clazz)
{
    ClassWriter writer(asmArena);
    clazz.accept(&writer);
    return writer.toByteArray();
}

void assertSecondRoundTripIsStable(RoundTripResult& result)
{
    auto        secondOutput = writeTree(result.second);
    ClassNode   third;
    ClassReader reader(asmArena, secondOutput.data(), static_cast<ca::i32>(secondOutput.size()));
    reader.accept(&third, 0);
    EXPECT_EQ(secondOutput, writeTree(third));
}

void assertReloadedClassMatchesHeader(const RoundTripResult& result)
{
    ASSERT_FALSE(result.output.empty());
    ASSERT_EQ(0xCAFEBABE, readMagic(result.output));
    EXPECT_EQ(result.second.version, result.first.version);
    EXPECT_EQ(result.second.access, result.first.access);
    EXPECT_EQ(result.second.name, result.first.name);
    EXPECT_EQ(result.second.superName, result.first.superName);
    EXPECT_EQ(result.second.interfaces.size(), result.first.interfaces.size());
    EXPECT_EQ(result.second.fields.size(), result.first.fields.size());
    EXPECT_EQ(result.second.methods.size(), result.first.methods.size());
}

class TreeIdempotencyTest : public ::testing::TestWithParam<const char*>
{};

}   // namespace

TEST(TreeRoundtripContractTest, plainClassReloadsAfterWriter)
{
    auto input = loadClassResource("DefaultPackage.class");
    ASSERT_FALSE(input.empty());

    auto result = roundTripViaTree(input);

    assertReloadedClassMatchesHeader(*result);
    EXPECT_NE(std::string::npos, toString(result->second.name).find("DefaultPackage"));
}

TEST(TreeRoundtripContractTest, tryCatchBlocksSurviveReload)
{
    auto input = loadClassResource("Issue311642.class");
    ASSERT_FALSE(input.empty());

    auto result = roundTripViaTree(input);

    assertReloadedClassMatchesHeader(*result);
    bool foundTryCatch = false;
    for (auto* method : result->second.methods) {
        if (method && !method->tryCatchBlocks.empty()) {
            foundTryCatch = true;
            break;
        }
    }
    EXPECT_TRUE(foundTryCatch);
}

TEST(TreeRoundtripContractTest, annotationsSurviveReload)
{
    auto input = loadClassResource("annotations/VCA.class");
    ASSERT_FALSE(input.empty());

    auto result = roundTripViaTree(input);

    assertReloadedClassMatchesHeader(*result);
    EXPECT_FALSE(result->second.visibleAnnotations.empty());
}

TEST(TreeRoundtripContractTest, stackMapFramesSurviveReload)
{
    auto input = loadClassResource("jdk8/AllFrames.class");
    ASSERT_FALSE(input.empty());

    auto result = roundTripViaTree(input);

    assertReloadedClassMatchesHeader(*result);
    bool foundFrame = false;
    for (auto* method : result->second.methods) {
        if (method && hasInstructionType(*method, AbstractInsnNode::FRAME)) {
            foundFrame = true;
            break;
        }
    }
    EXPECT_TRUE(foundFrame);
}

TEST_P(TreeIdempotencyTest, secondTreeWriteIsByteStable)
{
    auto input = loadClassResource(GetParam());
    ASSERT_FALSE(input.empty());

    auto result = roundTripViaTree(input);

    assertReloadedClassMatchesHeader(*result);
    assertSecondRoundTripIsStable(*result);
}

INSTANTIATE_TEST_SUITE_P(FeatureFixtures, TreeIdempotencyTest,
                         ::testing::Values("jdk8/AllStructures.class", "jdk5/AllStructures.class",
                                           "jdk8/AllLambdas.class", "jdk9/module-info.class",
                                           "jdk14/AllStructures$RecordSubType.class",
                                           "jdk15/AllStructures.class", "jdk3/LargeMethod.class"));

TEST(TreeRoundtripContractTest, typeAnnotationsSurviveReload)
{
    auto input = loadClassResource("jdk8/AllStructures.class");
    ASSERT_FALSE(input.empty());
    auto result = roundTripViaTree(input);
    assertReloadedClassMatchesHeader(*result);
    EXPECT_TRUE(hasTypeAnnotations(result->second));
}

TEST(TreeRoundtripContractTest, genericSignaturesSurviveReload)
{
    auto input = loadClassResource("jdk5/AllStructures.class");
    ASSERT_FALSE(input.empty());
    auto result = roundTripViaTree(input);
    assertReloadedClassMatchesHeader(*result);
    EXPECT_FALSE(result->second.signature.is_empty());
}

TEST(TreeRoundtripContractTest, invokedynamicSurvivesReload)
{
    auto input = loadClassResource("jdk8/AllLambdas.class");
    ASSERT_FALSE(input.empty());
    auto result = roundTripViaTree(input);
    assertReloadedClassMatchesHeader(*result);
    EXPECT_TRUE(classHasInstructionType(result->second, AbstractInsnNode::INVOKE_DYNAMIC_INSN));
}

TEST(TreeRoundtripContractTest, moduleDescriptorSurvivesReload)
{
    auto input = loadClassResource("jdk9/module-info.class");
    ASSERT_FALSE(input.empty());
    auto result = roundTripViaTree(input);
    assertReloadedClassMatchesHeader(*result);
    EXPECT_NE(result->second.moduleNode, nullptr);
}

TEST(TreeRoundtripContractTest, recordComponentsSurviveReload)
{
    auto input = loadClassResource("jdk14/AllStructures$RecordSubType.class");
    ASSERT_FALSE(input.empty());
    auto result = roundTripViaTree(input);
    assertReloadedClassMatchesHeader(*result);
    EXPECT_FALSE(result->second.recordComponents.empty());
}

TEST(TreeRoundtripContractTest, permittedSubclassesSurviveReload)
{
    auto input = loadClassResource("jdk15/AllStructures.class");
    ASSERT_FALSE(input.empty());
    auto result = roundTripViaTree(input);
    assertReloadedClassMatchesHeader(*result);
    EXPECT_FALSE(result->second.permittedSubclasses.empty());
}

TEST(TreeRoundtripContractTest, largeMethodSurvivesReload)
{
    auto input = loadClassResource("jdk3/LargeMethod.class");
    ASSERT_FALSE(input.empty());
    auto result = roundTripViaTree(input);
    assertReloadedClassMatchesHeader(*result);
    EXPECT_GT(instructionCount(result->second), 1000u);
}
