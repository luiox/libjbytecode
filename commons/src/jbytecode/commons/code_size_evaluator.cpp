#include "jbytecode/commons/code_size_evaluator.hpp"
#include "jbytecode/core/opcodes.hpp"

using namespace jbytecode::commons;
using namespace jbytecode::core;
using ca::str::Utf8StringRef;

CodeSizeEvaluator::CodeSizeEvaluator(MethodVisitor* methodVisitor)
    : CodeSizeEvaluator(Opcodes::ASM9, methodVisitor)
{}

CodeSizeEvaluator::CodeSizeEvaluator(int api, MethodVisitor* methodVisitor)
    : MethodVisitor(api, methodVisitor)
{}

void CodeSizeEvaluator::visitInsn(int opcode)
{
    minSize_ += 1;
    maxSize_ += 1;
    MethodVisitor::visitInsn(opcode);
}

void CodeSizeEvaluator::visitIntInsn(int opcode, int operand)
{
    if (opcode == Opcodes::SIPUSH) {
        minSize_ += 3;
        maxSize_ += 3;
    }
    else {
        minSize_ += 2;
        maxSize_ += 2;
    }
    MethodVisitor::visitIntInsn(opcode, operand);
}

void CodeSizeEvaluator::visitVarInsn(int opcode, int varIndex)
{
    if (varIndex < 4 && opcode != Opcodes::RET) {
        minSize_ += 1;
        maxSize_ += 1;
    }
    else if (varIndex >= 256) {
        minSize_ += 4;
        maxSize_ += 4;
    }
    else {
        minSize_ += 2;
        maxSize_ += 2;
    }
    MethodVisitor::visitVarInsn(opcode, varIndex);
}

void CodeSizeEvaluator::visitTypeInsn(int opcode, Utf8StringRef type)
{
    minSize_ += 3;
    maxSize_ += 3;
    MethodVisitor::visitTypeInsn(opcode, type);
}

void CodeSizeEvaluator::visitFieldInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                                       Utf8StringRef descriptor)
{
    minSize_ += 3;
    maxSize_ += 3;
    MethodVisitor::visitFieldInsn(opcode, owner, name, descriptor);
}

void CodeSizeEvaluator::visitMethodInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                                        Utf8StringRef descriptor, bool isInterface)
{
    if (opcode == Opcodes::INVOKEINTERFACE) {
        minSize_ += 5;
        maxSize_ += 5;
    }
    else {
        minSize_ += 3;
        maxSize_ += 3;
    }
    MethodVisitor::visitMethodInsn(opcode, owner, name, descriptor, isInterface);
}

void CodeSizeEvaluator::visitInvokeDynamicInsn(
    Utf8StringRef name, Utf8StringRef descriptor, const Handle& bootstrapMethodHandle,
    const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    minSize_ += 5;
    maxSize_ += 5;
    MethodVisitor::visitInvokeDynamicInsn(
        name, descriptor, bootstrapMethodHandle, bootstrapMethodArguments);
}

void CodeSizeEvaluator::visitJumpInsn(int opcode, Label* label)
{
    minSize_ += 3;
    if (opcode == Opcodes::GOTO || opcode == Opcodes::JSR) {
        maxSize_ += 5;
    }
    else {
        maxSize_ += 8;
    }
    MethodVisitor::visitJumpInsn(opcode, label);
}

void CodeSizeEvaluator::visitLdcInsn(const ca::core::Any& value)
{
    if (value.is<ca::i64>() || value.is<double>()) {
        minSize_ += 3;
        maxSize_ += 3;
    }
    else {
        minSize_ += 2;
        maxSize_ += 3;
    }
    MethodVisitor::visitLdcInsn(value);
}

void CodeSizeEvaluator::visitIincInsn(int varIndex, int increment)
{
    if (varIndex > 255 || increment > 127 || increment < -128) {
        minSize_ += 6;
        maxSize_ += 6;
    }
    else {
        minSize_ += 3;
        maxSize_ += 3;
    }
    MethodVisitor::visitIincInsn(varIndex, increment);
}

void CodeSizeEvaluator::visitTableSwitchInsn(int min, int max, Label* dflt,
                                             const std::vector<Label*>& labels)
{
    minSize_ += 13 + static_cast<int>(labels.size()) * 4;
    maxSize_ += 16 + static_cast<int>(labels.size()) * 4;
    MethodVisitor::visitTableSwitchInsn(min, max, dflt, labels);
}

void CodeSizeEvaluator::visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                                              const std::vector<Label*>& labels)
{
    minSize_ += 9 + static_cast<int>(keys.size()) * 8;
    maxSize_ += 12 + static_cast<int>(keys.size()) * 8;
    MethodVisitor::visitLookupSwitchInsn(dflt, keys, labels);
}

void CodeSizeEvaluator::visitMultiANewArrayInsn(Utf8StringRef descriptor, int numDimensions)
{
    minSize_ += 4;
    maxSize_ += 4;
    MethodVisitor::visitMultiANewArrayInsn(descriptor, numDimensions);
}
