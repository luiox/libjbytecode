#include "jbytecode/commons/advice_adapter.hpp"

#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/type.hpp"

using namespace ca::str;
using namespace jbytecode::commons;
using namespace jbytecode::core;

AdviceAdapter::AdviceAdapter(int api, MethodVisitor* mv, int access, ca::str::Utf8StringRef name,
                             ca::str::Utf8StringRef descriptor)
    : GeneratorAdapter(api, mv, access, name, descriptor)
    , methodAccess_(access)
    , methodDesc_(descriptor.data(), descriptor.byte_length())
    , isConstructor_(name == ZUtf8StringRef::from_static("<init>"))
{}

void AdviceAdapter::visitCode()
{
    GeneratorAdapter::visitCode();
    if (isConstructor_) {
        stackFrame_.clear();
        forwardJumpStackFrames_.clear();
    }
    else {
        onMethodEnter();
    }
}

void AdviceAdapter::visitLabel(Label* label)
{
    GeneratorAdapter::visitLabel(label);
    if (isConstructor_ && !forwardJumpStackFrames_.empty()) {
        auto it = forwardJumpStackFrames_.find(label);
        if (it != forwardJumpStackFrames_.end()) {
            stackFrame_                  = it->second;
            superClassConstructorCalled_ = false;
            forwardJumpStackFrames_.erase(it);
        }
    }
}

void AdviceAdapter::visitInsn(int opcode)
{
    if (isConstructor_ && !superClassConstructorCalled_) {
        switch (opcode) {
        case Opcodes::IRETURN:
        case Opcodes::FRETURN:
        case Opcodes::ARETURN:
        case Opcodes::LRETURN:
        case Opcodes::DRETURN: throw std::runtime_error("Invalid return in constructor");
        case Opcodes::RETURN:
            onMethodExit(opcode);
            endConstructorBasicBlockWithoutSuccessor();
            break;
        case Opcodes::ATHROW:
            popValue();
            onMethodExit(opcode);
            endConstructorBasicBlockWithoutSuccessor();
            break;
        case Opcodes::NOP:
        case Opcodes::LALOAD:
        case Opcodes::DALOAD:
        case Opcodes::LNEG:
        case Opcodes::DNEG:
        case Opcodes::FNEG:
        case Opcodes::INEG:
        case Opcodes::L2D:
        case Opcodes::D2L:
        case Opcodes::F2I:
        case Opcodes::I2B:
        case Opcodes::I2C:
        case Opcodes::I2S:
        case Opcodes::I2F:
        case Opcodes::ARRAYLENGTH: break;
        case Opcodes::ACONST_NULL:
        case Opcodes::ICONST_M1:
        case Opcodes::ICONST_0:
        case Opcodes::ICONST_1:
        case Opcodes::ICONST_2:
        case Opcodes::ICONST_3:
        case Opcodes::ICONST_4:
        case Opcodes::ICONST_5:
        case Opcodes::FCONST_0:
        case Opcodes::FCONST_1:
        case Opcodes::FCONST_2:
        case Opcodes::F2L:
        case Opcodes::F2D:
        case Opcodes::I2L:
        case Opcodes::I2D: pushValue(OTHER); break;
        case Opcodes::LCONST_0:
        case Opcodes::LCONST_1:
        case Opcodes::DCONST_0:
        case Opcodes::DCONST_1:
            pushValue(OTHER);
            pushValue(OTHER);
            break;
        case Opcodes::IALOAD:
        case Opcodes::FALOAD:
        case Opcodes::AALOAD:
        case Opcodes::BALOAD:
        case Opcodes::CALOAD:
        case Opcodes::SALOAD:
        case Opcodes::POP:
        case Opcodes::IADD:
        case Opcodes::FADD:
        case Opcodes::ISUB:
        case Opcodes::LSHL:
        case Opcodes::LSHR:
        case Opcodes::LUSHR:
        case Opcodes::L2I:
        case Opcodes::L2F:
        case Opcodes::D2I:
        case Opcodes::D2F:
        case Opcodes::FSUB:
        case Opcodes::FMUL:
        case Opcodes::FDIV:
        case Opcodes::FREM:
        case Opcodes::FCMPL:
        case Opcodes::FCMPG:
        case Opcodes::IMUL:
        case Opcodes::IDIV:
        case Opcodes::IREM:
        case Opcodes::ISHL:
        case Opcodes::ISHR:
        case Opcodes::IUSHR:
        case Opcodes::IAND:
        case Opcodes::IOR:
        case Opcodes::IXOR:
        case Opcodes::MONITORENTER:
        case Opcodes::MONITOREXIT: popValue(); break;
        case Opcodes::POP2:
        case Opcodes::LSUB:
        case Opcodes::LMUL:
        case Opcodes::LDIV:
        case Opcodes::LREM:
        case Opcodes::LADD:
        case Opcodes::LAND:
        case Opcodes::LOR:
        case Opcodes::LXOR:
        case Opcodes::DADD:
        case Opcodes::DMUL:
        case Opcodes::DSUB:
        case Opcodes::DDIV:
        case Opcodes::DREM:
            popValue();
            popValue();
            break;
        case Opcodes::IASTORE:
        case Opcodes::FASTORE:
        case Opcodes::AASTORE:
        case Opcodes::BASTORE:
        case Opcodes::CASTORE:
        case Opcodes::SASTORE:
        case Opcodes::LCMP:
        case Opcodes::DCMPL:
        case Opcodes::DCMPG:
            popValue();
            popValue();
            popValue();
            break;
        case Opcodes::LASTORE:
        case Opcodes::DASTORE:
            popValue();
            popValue();
            popValue();
            popValue();
            break;
        case Opcodes::DUP: pushValue(peekValue()); break;
        case Opcodes::DUP_X1:
        {
            int sz = (int)stackFrame_.size();
            stackFrame_.insert(stackFrame_.end() - 2, stackFrame_.back());
            break;
        }
        case Opcodes::DUP_X2:
        {
            int sz = (int)stackFrame_.size();
            stackFrame_.insert(stackFrame_.end() - 3, stackFrame_.back());
            break;
        }
        case Opcodes::DUP2:
        {
            int sz = (int)stackFrame_.size();
            stackFrame_.insert(stackFrame_.end() - 2, stackFrame_.back());
            stackFrame_.insert(stackFrame_.end() - 2, stackFrame_.back());
            break;
        }
        case Opcodes::DUP2_X1:
        {
            int sz = (int)stackFrame_.size();
            stackFrame_.insert(stackFrame_.end() - 3, stackFrame_.back());
            stackFrame_.insert(stackFrame_.end() - 3, stackFrame_.back());
            break;
        }
        case Opcodes::DUP2_X2:
        {
            int sz = (int)stackFrame_.size();
            stackFrame_.insert(stackFrame_.end() - 4, stackFrame_.back());
            stackFrame_.insert(stackFrame_.end() - 4, stackFrame_.back());
            break;
        }
        case Opcodes::SWAP:
        {
            int sz = (int)stackFrame_.size();
            stackFrame_.insert(stackFrame_.end() - 2, stackFrame_.back());
            stackFrame_.erase(stackFrame_.end() - 1);
            break;
        }
        default: throw std::runtime_error("Invalid opcode " + std::to_string(opcode));
        }
    }
    else {
        switch (opcode) {
        case Opcodes::RETURN:
        case Opcodes::IRETURN:
        case Opcodes::FRETURN:
        case Opcodes::ARETURN:
        case Opcodes::LRETURN:
        case Opcodes::DRETURN:
        case Opcodes::ATHROW: onMethodExit(opcode); break;
        default: break;
        }
    }
    GeneratorAdapter::visitInsn(opcode);
}

void AdviceAdapter::visitVarInsn(int opcode, int varIndex)
{
    GeneratorAdapter::visitVarInsn(opcode, varIndex);
    if (isConstructor_ && !superClassConstructorCalled_) {
        switch (opcode) {
        case Opcodes::ILOAD:
        case Opcodes::FLOAD: pushValue(OTHER); break;
        case Opcodes::LLOAD:
        case Opcodes::DLOAD:
            pushValue(OTHER);
            pushValue(OTHER);
            break;
        case Opcodes::ALOAD: pushValue(varIndex == 0 ? UNINITIALIZED_THIS : OTHER); break;
        case Opcodes::ASTORE:
        case Opcodes::ISTORE:
        case Opcodes::FSTORE: popValue(); break;
        case Opcodes::LSTORE:
        case Opcodes::DSTORE:
            popValue();
            popValue();
            break;
        case Opcodes::RET: endConstructorBasicBlockWithoutSuccessor(); break;
        default: break;
        }
    }
}

void AdviceAdapter::visitFieldInsn(int opcode, ca::str::Utf8StringRef owner,
                                   ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor)
{
    GeneratorAdapter::visitFieldInsn(opcode, owner, name, descriptor);
    if (isConstructor_ && !superClassConstructorCalled_) {
        bool longOrDouble = descriptor.byte_length() == 1 &&
                            (descriptor.byte_at(0) == 'J' || descriptor.byte_at(0) == 'D');
        switch (opcode) {
        case Opcodes::GETSTATIC:
            pushValue(OTHER);
            if (longOrDouble) pushValue(OTHER);
            break;
        case Opcodes::PUTSTATIC:
            popValue();
            if (longOrDouble) popValue();
            break;
        case Opcodes::PUTFIELD:
            popValue();
            popValue();
            if (longOrDouble) popValue();
            break;
        case Opcodes::GETFIELD:
            if (longOrDouble) pushValue(OTHER);
            break;
        default: break;
        }
    }
}

void AdviceAdapter::visitIntInsn(int opcode, int operand)
{
    GeneratorAdapter::visitIntInsn(opcode, operand);
    if (isConstructor_ && !superClassConstructorCalled_ && opcode != Opcodes::NEWARRAY) {
        pushValue(OTHER);
    }
}

void AdviceAdapter::visitLdcInsn(const ca::core::Any& value)
{
    GeneratorAdapter::visitLdcInsn(value);
    if (isConstructor_ && !superClassConstructorCalled_) {
        pushValue(OTHER);
        if (value.is<double>() || value.is<ca::i64>()) {
            pushValue(OTHER);
        }
    }
}

void AdviceAdapter::visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor, int numDimensions)
{
    GeneratorAdapter::visitMultiANewArrayInsn(descriptor, numDimensions);
    if (isConstructor_ && !superClassConstructorCalled_) {
        for (int i = 0; i < numDimensions; i++) popValue();
        pushValue(OTHER);
    }
}

void AdviceAdapter::visitTypeInsn(int opcode, ca::str::Utf8StringRef type)
{
    GeneratorAdapter::visitTypeInsn(opcode, type);
    if (isConstructor_ && !superClassConstructorCalled_ && opcode == Opcodes::NEW) {
        pushValue(OTHER);
    }
}

void AdviceAdapter::visitMethodInsn(int opcode, ca::str::Utf8StringRef owner,
                                    ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                    bool isInterface)
{
    GeneratorAdapter::visitMethodInsn(opcode, owner, name, descriptor, isInterface);
    doVisitMethodInsn(opcode & ~Opcodes::SOURCE_MASK, name, descriptor);
}

void AdviceAdapter::visitInvokeDynamicInsn(
    ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
    const Handle& bootstrapMethodHandle, const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    GeneratorAdapter::visitInvokeDynamicInsn(
        name, descriptor, bootstrapMethodHandle, bootstrapMethodArguments);
    doVisitMethodInsn(Opcodes::INVOKEDYNAMIC, name, descriptor);
}

void AdviceAdapter::doVisitMethodInsn(int opcode, ca::str::Utf8StringRef name,
                                      ca::str::Utf8StringRef descriptor)
{
    if (isConstructor_ && !superClassConstructorCalled_) {
        auto argTypes = Type::getArgumentTypes(descriptor);
        for (size_t i = 0; i < argTypes.size(); i++) {
            popValue();
            if (argTypes[i].getSize() == 2) popValue();
        }
        switch (opcode) {
        case Opcodes::INVOKEINTERFACE:
        case Opcodes::INVOKEVIRTUAL: popValue(); break;
        case Opcodes::INVOKESPECIAL:
        {
            int value = popValue();
            if (value == UNINITIALIZED_THIS && name == Utf8StringRef::from_cstr("<init>")) {
                superClassConstructorCalled_ = true;
                onMethodEnter();
            }
            break;
        }
        default: break;
        }
        Type returnType = Type::getReturnType(descriptor);
        if (returnType.getSort() != Type::VOID) {
            pushValue(OTHER);
            if (returnType.getSize() == 2) pushValue(OTHER);
        }
    }
}

void AdviceAdapter::visitJumpInsn(int opcode, Label* label)
{
    GeneratorAdapter::visitJumpInsn(opcode, label);
    if (isConstructor_ && !superClassConstructorCalled_) {
        switch (opcode) {
        case Opcodes::IFEQ:
        case Opcodes::IFNE:
        case Opcodes::IFLT:
        case Opcodes::IFGE:
        case Opcodes::IFGT:
        case Opcodes::IFLE:
        case Opcodes::IFNULL:
        case Opcodes::IFNONNULL: popValue(); break;
        case Opcodes::IF_ICMPEQ:
        case Opcodes::IF_ICMPNE:
        case Opcodes::IF_ICMPLT:
        case Opcodes::IF_ICMPGE:
        case Opcodes::IF_ICMPGT:
        case Opcodes::IF_ICMPLE:
        case Opcodes::IF_ACMPEQ:
        case Opcodes::IF_ACMPNE:
            popValue();
            popValue();
            break;
        case Opcodes::JSR: pushValue(OTHER); break;
        case Opcodes::GOTO: endConstructorBasicBlockWithoutSuccessor(); break;
        default: break;
        }
        addForwardJump(label);
    }
}

void AdviceAdapter::visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                                          const std::vector<Label*>& labels)
{
    GeneratorAdapter::visitLookupSwitchInsn(dflt, keys, labels);
    if (isConstructor_ && !superClassConstructorCalled_) {
        popValue();
        addForwardJumps(dflt, labels);
        endConstructorBasicBlockWithoutSuccessor();
    }
}

void AdviceAdapter::visitTableSwitchInsn(int min, int max, Label* dflt,
                                         const std::vector<Label*>& labels)
{
    GeneratorAdapter::visitTableSwitchInsn(min, max, dflt, labels);
    if (isConstructor_ && !superClassConstructorCalled_) {
        popValue();
        addForwardJumps(dflt, labels);
        endConstructorBasicBlockWithoutSuccessor();
    }
}

void AdviceAdapter::visitTryCatchBlock(Label* start, Label* end, Label* handler,
                                       ca::str::Utf8StringRef type)
{
    GeneratorAdapter::visitTryCatchBlock(start, end, handler, type);
    if (isConstructor_ && forwardJumpStackFrames_.count(handler) == 0) {
        forwardJumpStackFrames_[handler] = {OTHER};
    }
}

void AdviceAdapter::pushValue(int value)
{
    stackFrame_.push_back(value);
}

int AdviceAdapter::popValue()
{
    int v = stackFrame_.back();
    stackFrame_.pop_back();
    return v;
}

int AdviceAdapter::peekValue()
{
    return stackFrame_.back();
}

void AdviceAdapter::addForwardJump(Label* label)
{
    if (forwardJumpStackFrames_.count(label) > 0) return;
    forwardJumpStackFrames_[label] = stackFrame_;
}

void AdviceAdapter::addForwardJumps(Label* dflt, const std::vector<Label*>& labels)
{
    addForwardJump(dflt);
    for (auto* label : labels) addForwardJump(label);
}

void AdviceAdapter::endConstructorBasicBlockWithoutSuccessor()
{
    superClassConstructorCalled_ = true;
}
