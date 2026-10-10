#include <gtest/gtest.h>
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/visitors.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

TEST(SignatureReaderTest, testAccept_validClassOrMethodSignature)
{
    SignatureReader classSignatureReader(
        ZUtf8StringRef::from_static("<T:Ljava/lang/Object;>Ljava/lang/Object;"));
    SignatureReader methodSignatureReader(
        ZUtf8StringRef::from_static("<T:Ljava/lang/Object;>(TT;)TT;"));
    SignatureVisitor signatureVisitor(Opcodes::ASM10_EXPERIMENTAL);

    EXPECT_NO_THROW(classSignatureReader.accept(&signatureVisitor));
    EXPECT_NO_THROW(methodSignatureReader.accept(&signatureVisitor));
}

TEST(SignatureReaderTest, testAccept_validClassSignatures_fromPrecompiledClasses)
{
    auto signatures = collectAvailableSignatures();
    ASSERT_FALSE(signatures.classSignatures.empty());
    SignatureVisitor signatureVisitor(Opcodes::ASM10_EXPERIMENTAL);
    for (const auto& signature : signatures.classSignatures) {
        EXPECT_NO_THROW(
            SignatureReader(Utf8StringRef::from_cstr(signature.c_str())).accept(&signatureVisitor))
            << signature;
    }
}

TEST(SignatureReaderTest, testAccept_validMethodSignatures_fromPrecompiledClasses)
{
    auto signatures = collectAvailableSignatures();
    ASSERT_FALSE(signatures.methodSignatures.empty());
    SignatureVisitor signatureVisitor(Opcodes::ASM10_EXPERIMENTAL);
    for (const auto& signature : signatures.methodSignatures) {
        EXPECT_NO_THROW(
            SignatureReader(Utf8StringRef::from_cstr(signature.c_str())).accept(&signatureVisitor))
            << signature;
    }
}

TEST(SignatureReaderTest, testAccept_validFieldSignature)
{
    SignatureReader signatureReader(
        ZUtf8StringRef::from_static("Ljava/util/List<Ljava/lang/String;>;"));
    SignatureVisitor signatureVisitor(Opcodes::ASM10_EXPERIMENTAL);

    EXPECT_NO_THROW(signatureReader.acceptType(&signatureVisitor));
}

TEST(SignatureReaderTest, testAccept_validFieldSignatures_fromPrecompiledClasses)
{
    auto signatures = collectAvailableSignatures();
    ASSERT_FALSE(signatures.fieldSignatures.empty());
    SignatureVisitor signatureVisitor(Opcodes::ASM10_EXPERIMENTAL);
    for (const auto& signature : signatures.fieldSignatures) {
        EXPECT_NO_THROW(SignatureReader(Utf8StringRef::from_cstr(signature.c_str()))
                            .acceptType(&signatureVisitor))
            << signature;
    }
}

TEST(SignatureReaderTest, testAccept_invalidSignature)
{
    SignatureReader  signatureReader(ZUtf8StringRef::from_static("-"));
    SignatureVisitor signatureVisitor(Opcodes::ASM10_EXPERIMENTAL);

    EXPECT_THROW(signatureReader.accept(&signatureVisitor), std::invalid_argument);
}
