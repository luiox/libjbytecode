#include <gtest/gtest.h>
#include "test_utils.hpp"
#include "jbytecode/util/trace_signature_visitor.hpp"

TEST(TraceSignatureVisitorTest, testConstructor)
{
    jbytecode::util::TraceSignatureVisitor tsv(Opcodes::ACC_PUBLIC);
    SUCCEED();
}

TEST(TraceSignatureVisitorTest, testClassSignature)
{
    jbytecode::util::TraceSignatureVisitor tsv(Opcodes::ACC_PUBLIC);
    tsv.visitSuperclass();
    tsv.visitClassType(ZUtf8StringRef::from_static("pkg/Base"));
    tsv.visitEnd();
    std::string decl = tsv.getDeclaration();
    EXPECT_EQ(" extends pkg.Base", decl);
}
