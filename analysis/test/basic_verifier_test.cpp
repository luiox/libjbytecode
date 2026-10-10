#include <gtest/gtest.h>
#include <string>

#include "jbytecode/analysis/analyzer.hpp"
#include "jbytecode/analysis/basic_verifier.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace ca::str;

TEST(BasicVerifierTest, copyOperationValidInt)
{
    BasicVerifier v;
    InsnNode      iload(Opcodes::ILOAD);
    EXPECT_NO_THROW(v.copyOperation(&iload, BasicValue::INT_VALUE));
}

TEST(BasicVerifierTest, copyOperationRejectsWrongType)
{
    BasicVerifier v;
    InsnNode      iload(Opcodes::ILOAD);
    EXPECT_THROW(v.copyOperation(&iload, BasicValue::FLOAT_VALUE), AnalyzerException);
}

TEST(BasicVerifierTest, copyOperationAcceptsReferenceForALOAD)
{
    BasicVerifier v;
    InsnNode      aload(Opcodes::ALOAD);
    EXPECT_NO_THROW(v.copyOperation(&aload, BasicValue::REFERENCE_VALUE));
}

TEST(BasicVerifierTest, unaryOperationIntOps)
{
    BasicVerifier v;
    InsnNode      ineg(Opcodes::INEG);
    EXPECT_NO_THROW(v.unaryOperation(&ineg, BasicValue::INT_VALUE));
    EXPECT_THROW(v.unaryOperation(&ineg, BasicValue::FLOAT_VALUE), AnalyzerException);
}

TEST(BasicVerifierTest, unaryOperationLongOps)
{
    BasicVerifier v;
    InsnNode      lneg(Opcodes::LNEG);
    EXPECT_NO_THROW(v.unaryOperation(&lneg, BasicValue::LONG_VALUE));
}

TEST(BasicVerifierTest, unaryOperationChecksReferences)
{
    BasicVerifier v;
    InsnNode      athrow(Opcodes::ATHROW);
    EXPECT_NO_THROW(v.unaryOperation(&athrow, BasicValue::REFERENCE_VALUE));
    EXPECT_THROW(v.unaryOperation(&athrow, BasicValue::INT_VALUE), AnalyzerException);
}

TEST(BasicVerifierTest, binaryOperationIntInt)
{
    BasicVerifier v;
    InsnNode      iadd(Opcodes::IADD);
    EXPECT_NO_THROW(v.binaryOperation(&iadd, BasicValue::INT_VALUE, BasicValue::INT_VALUE));
    EXPECT_THROW(v.binaryOperation(&iadd, BasicValue::INT_VALUE, BasicValue::FLOAT_VALUE),
                 AnalyzerException);
}

TEST(BasicVerifierTest, returnOperationTypeMatch)
{
    BasicVerifier v;
    InsnNode      ireturn(Opcodes::IRETURN);
    EXPECT_NO_THROW(v.returnOperation(&ireturn, BasicValue::INT_VALUE, BasicValue::INT_VALUE));
    EXPECT_THROW(v.returnOperation(&ireturn, BasicValue::INT_VALUE, BasicValue::FLOAT_VALUE),
                 AnalyzerException);
}

TEST(BasicVerifierTest, naryOperationMethodArgs)
{
    BasicVerifier           v;
    MethodInsnNode          invoke(Opcodes::INVOKESTATIC,
                          ZUtf8StringRef::from_static("Foo"),
                          ZUtf8StringRef::from_static("bar"),
                          ZUtf8StringRef::from_static("(I)V"),
                          false);
    std::vector<BasicValue> args = {BasicValue::INT_VALUE};
    EXPECT_NO_THROW(v.naryOperation(&invoke, args));
}

TEST(BasicVerifierTest, naryOperationMethodArgsWrong)
{
    BasicVerifier           v;
    MethodInsnNode          invoke(Opcodes::INVOKESTATIC,
                          ZUtf8StringRef::from_static("Foo"),
                          ZUtf8StringRef::from_static("bar"),
                          ZUtf8StringRef::from_static("(I)V"),
                          false);
    std::vector<BasicValue> args = {BasicValue::FLOAT_VALUE};
    EXPECT_THROW(v.naryOperation(&invoke, args), AnalyzerException);
}
