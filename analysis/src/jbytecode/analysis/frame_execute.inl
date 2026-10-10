#pragma once

// Out-of-line definition of Frame<V>::execute — the dataflow engine.
// Faithful port of ASM org.objectweb.asm.tree.analysis.Frame.execute
// (Frame.java L290-686). Pops operands, invokes interpreter callbacks, pushes
// results, simulating each instruction's effect on the locals/stack frame.

#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/tree/insn_list.hpp"
#include "libca/core/tag_cast.hpp"

namespace jbytecode::analysis {

namespace frame_detail {

template <typename V>
inline bool executeDupX2(Frame<V>& f, const jbytecode::tree::AbstractInsnNode* insn,
                         const V& value1, Interpreter<V>* interp) {
    V value2 = f.pop();
    if (value2.getSize() == 1) {
        V value3 = f.pop();
        if (value3.getSize() == 1) {
            f.push(interp->copyOperation(insn, value1));
            f.push(interp->copyOperation(insn, value3));
            f.push(interp->copyOperation(insn, value2));
            f.push(interp->copyOperation(insn, value1));
            return true;
        }
    } else {
        f.push(interp->copyOperation(insn, value1));
        f.push(interp->copyOperation(insn, value2));
        f.push(interp->copyOperation(insn, value1));
        return true;
    }
    return false;
}

template <typename V>
inline void executeInvokeInsn(Frame<V>& f, const jbytecode::tree::AbstractInsnNode* insn,
                              ca::str::Utf8StringRef desc, Interpreter<V>* interp) {
    int opcode = insn->getOpcode();
    std::vector<V> args(jbytecode::core::Type::getArgumentCount(desc));
    for (int i = (int)args.size() - 1; i >= 0; --i) args[i] = f.pop();
    std::vector<V> valueList;
    if (opcode != jbytecode::core::Opcodes::INVOKESTATIC &&
        opcode != jbytecode::core::Opcodes::INVOKEDYNAMIC) {
        valueList.push_back(f.pop());
    }
    for (auto& a : args) valueList.push_back(a);
    if (jbytecode::core::Type::getReturnType(desc) == jbytecode::core::Type::VOID_TYPE) {
        interp->naryOperation(insn, valueList);
    } else {
        f.push(interp->naryOperation(insn, valueList));
    }
}

} // namespace frame_detail

// EXECUTE_BODY_PLACEHOLDER
template <typename V>
void Frame<V>::execute(const jbytecode::tree::AbstractInsnNode* insn, Interpreter<V>* interpreter) {
    namespace op = jbytecode::core;
    using namespace jbytecode::tree;
    V value1, value2, value3, value4;
    int varIndex;

    switch (insn->getOpcode()) {
        case op::Opcodes::NOP:
            break;
        case op::Opcodes::ACONST_NULL:
        case op::Opcodes::ICONST_M1: case op::Opcodes::ICONST_0: case op::Opcodes::ICONST_1:
        case op::Opcodes::ICONST_2: case op::Opcodes::ICONST_3: case op::Opcodes::ICONST_4:
        case op::Opcodes::ICONST_5:
        case op::Opcodes::LCONST_0: case op::Opcodes::LCONST_1:
        case op::Opcodes::FCONST_0: case op::Opcodes::FCONST_1: case op::Opcodes::FCONST_2:
        case op::Opcodes::DCONST_0: case op::Opcodes::DCONST_1:
        case op::Opcodes::BIPUSH: case op::Opcodes::SIPUSH: case op::Opcodes::LDC:
            push(interpreter->newOperation(insn));
            break;
        case op::Opcodes::ILOAD: case op::Opcodes::LLOAD: case op::Opcodes::FLOAD:
        case op::Opcodes::DLOAD: case op::Opcodes::ALOAD:
            push(interpreter->copyOperation(insn, getLocal(ca::core::tag_cast::cast<VarInsnNode*>(insn)->var)));
            break;
        case op::Opcodes::ISTORE: case op::Opcodes::LSTORE: case op::Opcodes::FSTORE:
        case op::Opcodes::DSTORE: case op::Opcodes::ASTORE:
            value1 = interpreter->copyOperation(insn, pop());
            varIndex = ca::core::tag_cast::cast<VarInsnNode*>(insn)->var;
            setLocal(varIndex, value1);
            if (value1.getSize() == 2)
                setLocal(varIndex + 1, interpreter->newEmptyValue(varIndex + 1));
            if (varIndex > 0) {
                V local = getLocal(varIndex - 1);
                if (local.getSize() == 2)
                    setLocal(varIndex - 1, interpreter->newEmptyValue(varIndex - 1));
            }
            break;
        // EXECUTE_BODY_CONT
        case op::Opcodes::IASTORE: case op::Opcodes::LASTORE: case op::Opcodes::FASTORE:
        case op::Opcodes::DASTORE: case op::Opcodes::AASTORE: case op::Opcodes::BASTORE:
        case op::Opcodes::CASTORE: case op::Opcodes::SASTORE:
            value3 = pop(); value2 = pop(); value1 = pop();
            interpreter->ternaryOperation(insn, value1, value2, value3);
            break;
        case op::Opcodes::POP:
            if (pop().getSize() == 2)
                throw jbytecode::analysis::AnalyzerException("Illegal use of POP");
            break;
        case op::Opcodes::POP2:
            if (pop().getSize() == 1 && pop().getSize() != 1)
                throw jbytecode::analysis::AnalyzerException("Illegal use of POP2");
            break;
        case op::Opcodes::DUP:
            value1 = pop();
            if (value1.getSize() != 1)
                throw jbytecode::analysis::AnalyzerException("Illegal use of DUP");
            push(interpreter->copyOperation(insn, value1));
            push(interpreter->copyOperation(insn, value1));
            break;
        case op::Opcodes::DUP_X1:
            value1 = pop(); value2 = pop();
            if (value1.getSize() != 1 || value2.getSize() != 1)
                throw jbytecode::analysis::AnalyzerException("Illegal use of DUP_X1");
            push(interpreter->copyOperation(insn, value1));
            push(interpreter->copyOperation(insn, value2));
            push(interpreter->copyOperation(insn, value1));
            break;
        case op::Opcodes::DUP_X2:
            value1 = pop();
            if (value1.getSize() == 1 && frame_detail::executeDupX2(*this, insn, value1, interpreter))
                break;
            throw jbytecode::analysis::AnalyzerException("Illegal use of DUP_X2");
        case op::Opcodes::DUP2:
            value1 = pop();
            if (value1.getSize() == 1) {
                value2 = pop();
                if (value2.getSize() == 1) {
                    push(interpreter->copyOperation(insn, value2));
                    push(interpreter->copyOperation(insn, value1));
                    push(interpreter->copyOperation(insn, value2));
                    push(interpreter->copyOperation(insn, value1));
                    break;
                }
            } else {
                push(interpreter->copyOperation(insn, value1));
                push(interpreter->copyOperation(insn, value1));
                break;
            }
            throw jbytecode::analysis::AnalyzerException("Illegal use of DUP2");
        // EXECUTE_BODY_CONT2
        case op::Opcodes::DUP2_X1:
            value1 = pop();
            if (value1.getSize() == 1) {
                value2 = pop();
                if (value2.getSize() == 1) {
                    value3 = pop();
                    if (value3.getSize() == 1) {
                        push(interpreter->copyOperation(insn, value2));
                        push(interpreter->copyOperation(insn, value1));
                        push(interpreter->copyOperation(insn, value3));
                        push(interpreter->copyOperation(insn, value2));
                        push(interpreter->copyOperation(insn, value1));
                        break;
                    }
                }
            } else {
                value2 = pop();
                if (value2.getSize() == 1) {
                    push(interpreter->copyOperation(insn, value1));
                    push(interpreter->copyOperation(insn, value2));
                    push(interpreter->copyOperation(insn, value1));
                    break;
                }
            }
            throw jbytecode::analysis::AnalyzerException("Illegal use of DUP2_X1");
        case op::Opcodes::DUP2_X2:
            value1 = pop();
            if (value1.getSize() == 1) {
                value2 = pop();
                if (value2.getSize() == 1) {
                    value3 = pop();
                    if (value3.getSize() == 1) {
                        value4 = pop();
                        if (value4.getSize() == 1) {
                            push(interpreter->copyOperation(insn, value2));
                            push(interpreter->copyOperation(insn, value1));
                            push(interpreter->copyOperation(insn, value4));
                            push(interpreter->copyOperation(insn, value3));
                            push(interpreter->copyOperation(insn, value2));
                            push(interpreter->copyOperation(insn, value1));
                            break;
                        }
                    } else {
                        push(interpreter->copyOperation(insn, value2));
                        push(interpreter->copyOperation(insn, value1));
                        push(interpreter->copyOperation(insn, value3));
                        push(interpreter->copyOperation(insn, value2));
                        push(interpreter->copyOperation(insn, value1));
                        break;
                    }
                }
            } else if (frame_detail::executeDupX2(*this, insn, value1, interpreter)) {
                break;
            }
            throw jbytecode::analysis::AnalyzerException("Illegal use of DUP2_X2");
        case op::Opcodes::SWAP:
            value2 = pop(); value1 = pop();
            if (value1.getSize() != 1 || value2.getSize() != 1)
                throw jbytecode::analysis::AnalyzerException("Illegal use of SWAP");
            push(interpreter->copyOperation(insn, value2));
            push(interpreter->copyOperation(insn, value1));
            break;
        // EXECUTE_BODY_CONT3
        case op::Opcodes::IALOAD: case op::Opcodes::LALOAD: case op::Opcodes::FALOAD:
        case op::Opcodes::DALOAD: case op::Opcodes::AALOAD: case op::Opcodes::BALOAD:
        case op::Opcodes::CALOAD: case op::Opcodes::SALOAD:
        case op::Opcodes::IADD: case op::Opcodes::LADD: case op::Opcodes::FADD: case op::Opcodes::DADD:
        case op::Opcodes::ISUB: case op::Opcodes::LSUB: case op::Opcodes::FSUB: case op::Opcodes::DSUB:
        case op::Opcodes::IMUL: case op::Opcodes::LMUL: case op::Opcodes::FMUL: case op::Opcodes::DMUL:
        case op::Opcodes::IDIV: case op::Opcodes::LDIV: case op::Opcodes::FDIV: case op::Opcodes::DDIV:
        case op::Opcodes::IREM: case op::Opcodes::LREM: case op::Opcodes::FREM: case op::Opcodes::DREM:
        case op::Opcodes::ISHL: case op::Opcodes::LSHL: case op::Opcodes::ISHR: case op::Opcodes::LSHR:
        case op::Opcodes::IUSHR: case op::Opcodes::LUSHR:
        case op::Opcodes::IAND: case op::Opcodes::LAND: case op::Opcodes::IOR: case op::Opcodes::LOR:
        case op::Opcodes::IXOR: case op::Opcodes::LXOR:
        case op::Opcodes::LCMP: case op::Opcodes::FCMPL: case op::Opcodes::FCMPG:
        case op::Opcodes::DCMPL: case op::Opcodes::DCMPG:
            value2 = pop(); value1 = pop();
            push(interpreter->binaryOperation(insn, value1, value2));
            break;
        case op::Opcodes::INEG: case op::Opcodes::LNEG: case op::Opcodes::FNEG: case op::Opcodes::DNEG:
            push(interpreter->unaryOperation(insn, pop()));
            break;
        case op::Opcodes::IINC:
            varIndex = ca::core::tag_cast::cast<IincInsnNode*>(insn)->var;
            setLocal(varIndex, interpreter->unaryOperation(insn, getLocal(varIndex)));
            break;
        case op::Opcodes::I2L: case op::Opcodes::I2F: case op::Opcodes::I2D:
        case op::Opcodes::L2I: case op::Opcodes::L2F: case op::Opcodes::L2D:
        case op::Opcodes::F2I: case op::Opcodes::F2L: case op::Opcodes::F2D:
        case op::Opcodes::D2I: case op::Opcodes::D2L: case op::Opcodes::D2F:
        case op::Opcodes::I2B: case op::Opcodes::I2C: case op::Opcodes::I2S:
            push(interpreter->unaryOperation(insn, pop()));
            break;
        // EXECUTE_BODY_CONT4
        case op::Opcodes::IFEQ: case op::Opcodes::IFNE: case op::Opcodes::IFLT:
        case op::Opcodes::IFGE: case op::Opcodes::IFGT: case op::Opcodes::IFLE:
            interpreter->unaryOperation(insn, pop());
            break;
        case op::Opcodes::IF_ICMPEQ: case op::Opcodes::IF_ICMPNE: case op::Opcodes::IF_ICMPLT:
        case op::Opcodes::IF_ICMPGE: case op::Opcodes::IF_ICMPGT: case op::Opcodes::IF_ICMPLE:
        case op::Opcodes::IF_ACMPEQ: case op::Opcodes::IF_ACMPNE: case op::Opcodes::PUTFIELD:
            value2 = pop(); value1 = pop();
            interpreter->binaryOperation(insn, value1, value2);
            break;
        case op::Opcodes::GOTO:
            break;
        case op::Opcodes::JSR:
            push(interpreter->newOperation(insn));
            break;
        case op::Opcodes::RET:
            break;
        case op::Opcodes::TABLESWITCH: case op::Opcodes::LOOKUPSWITCH:
            interpreter->unaryOperation(insn, pop());
            break;
        case op::Opcodes::IRETURN: case op::Opcodes::LRETURN: case op::Opcodes::FRETURN:
        case op::Opcodes::DRETURN: case op::Opcodes::ARETURN:
            value1 = pop();
            interpreter->unaryOperation(insn, value1);
            interpreter->returnOperation(insn, value1, returnValue_);
            break;
        case op::Opcodes::RETURN:
            if (hasReturnValue_)
                throw jbytecode::analysis::AnalyzerException("Incompatible return type");
            break;
        case op::Opcodes::GETSTATIC:
            push(interpreter->newOperation(insn));
            break;
        case op::Opcodes::PUTSTATIC:
            interpreter->unaryOperation(insn, pop());
            break;
        case op::Opcodes::GETFIELD:
            push(interpreter->unaryOperation(insn, pop()));
            break;
        case op::Opcodes::INVOKEVIRTUAL: case op::Opcodes::INVOKESPECIAL:
        case op::Opcodes::INVOKESTATIC: case op::Opcodes::INVOKEINTERFACE:
            frame_detail::executeInvokeInsn(*this, insn, ca::core::tag_cast::cast<MethodInsnNode*>(insn)->desc, interpreter);
            break;
        case op::Opcodes::INVOKEDYNAMIC:
            frame_detail::executeInvokeInsn(*this, insn, ca::core::tag_cast::cast<InvokeDynamicInsnNode*>(insn)->desc, interpreter);
            break;
        case op::Opcodes::NEW:
            push(interpreter->newOperation(insn));
            break;
        case op::Opcodes::NEWARRAY: case op::Opcodes::ANEWARRAY: case op::Opcodes::ARRAYLENGTH:
            push(interpreter->unaryOperation(insn, pop()));
            break;
        case op::Opcodes::ATHROW:
            interpreter->unaryOperation(insn, pop());
            break;
        case op::Opcodes::CHECKCAST: case op::Opcodes::INSTANCEOF:
            push(interpreter->unaryOperation(insn, pop()));
            break;
        case op::Opcodes::MONITORENTER: case op::Opcodes::MONITOREXIT:
            interpreter->unaryOperation(insn, pop());
            break;
        case op::Opcodes::MULTIANEWARRAY: {
            std::vector<V> valueList;
            int dims = ca::core::tag_cast::cast<MultiANewArrayInsnNode*>(insn)->dims;
            valueList.resize(dims);
            for (int i = dims - 1; i >= 0; --i) valueList[i] = pop();
            push(interpreter->naryOperation(insn, valueList));
            break;
        }
        case op::Opcodes::IFNULL: case op::Opcodes::IFNONNULL:
            interpreter->unaryOperation(insn, pop());
            break;
        default:
            throw jbytecode::analysis::AnalyzerException("Illegal opcode " + std::to_string(insn->getOpcode()));
    }
}

} // namespace jbytecode::analysis
