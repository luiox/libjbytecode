#include <gtest/gtest.h>
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

TEST(SignatureWriterTest, testReadAndWrite_classOrMethodSignature)
{
    SignatureWriter classSignatureWriter;
    SignatureReader(ZUtf8StringRef::from_static("<T:Ljava/lang/Object;>Ljava/lang/Object;"))
        .accept(&classSignatureWriter);

    EXPECT_EQ("<T:Ljava/lang/Object;>Ljava/lang/Object;", classSignatureWriter.toString());

    SignatureWriter methodSignatureWriter;
    SignatureReader(ZUtf8StringRef::from_static("<T:Ljava/lang/Object;>(TT;)TT;"))
        .accept(&methodSignatureWriter);

    EXPECT_EQ("<T:Ljava/lang/Object;>(TT;)TT;", methodSignatureWriter.toString());
}

TEST(SignatureWriterTest, testReadAndWrite_classSignatures_fromPrecompiledClasses)
{
    auto signatures = collectAvailableSignatures();
    ASSERT_FALSE(signatures.classSignatures.empty());
    for (const auto& signature : signatures.classSignatures) {
        SignatureWriter signatureWriter;
        SignatureReader(Utf8StringRef::from_cstr(signature.c_str())).accept(&signatureWriter);
        EXPECT_EQ(signature, signatureWriter.toString());
    }
}

TEST(SignatureWriterTest, testReadAndWrite_methodSignatures_fromPrecompiledClasses)
{
    auto signatures = collectAvailableSignatures();
    ASSERT_FALSE(signatures.methodSignatures.empty());
    for (const auto& signature : signatures.methodSignatures) {
        SignatureWriter signatureWriter;
        SignatureReader(Utf8StringRef::from_cstr(signature.c_str())).accept(&signatureWriter);
        EXPECT_EQ(signature, signatureWriter.toString());
    }
}

TEST(SignatureWriterTest, testReadAndWrite_fieldSignature)
{
    SignatureWriter signatureWriter;

    SignatureReader(ZUtf8StringRef::from_static("Ljava/util/List<Ljava/lang/String;>;"))
        .acceptType(&signatureWriter);

    EXPECT_EQ("Ljava/util/List<Ljava/lang/String;>;", signatureWriter.toString());
}

TEST(SignatureWriterTest, testReadAndWrite_fieldSignatures_fromPrecompiledClasses)
{
    auto signatures = collectAvailableSignatures();
    ASSERT_FALSE(signatures.fieldSignatures.empty());
    for (const auto& signature : signatures.fieldSignatures) {
        SignatureWriter signatureWriter;
        SignatureReader(Utf8StringRef::from_cstr(signature.c_str())).acceptType(&signatureWriter);
        EXPECT_EQ(signature, signatureWriter.toString());
    }
}
