#include "jbytecode/commons/local_variables_sorter.hpp"

#include "jbytecode/core/opcodes.hpp"

using namespace jbytecode::commons;
using namespace jbytecode::core;
using ca::str::Utf8StringRef;

LocalVariablesSorter::LocalVariablesSorter(int access, Utf8StringRef descriptor, MethodVisitor* mv)
    : LocalVariablesSorter(Opcodes::ASM9, access, descriptor, mv)
{}

LocalVariablesSorter::LocalVariablesSorter(int api, int access, Utf8StringRef descriptor,
                                           MethodVisitor* mv)
    : MethodVisitor(api, mv)
{
    Type              methodType = Type::getMethodType(descriptor);
    std::vector<Type> argTypes   = methodType.getArgumentTypes();

    int offset = 0;
    if ((access & Opcodes::ACC_STATIC) == 0) {
        offset = 1;
    }

    firstLocal_ = offset;
    for (size_t i = 0; i < argTypes.size(); i++) {
        offset += argTypes[i].getSize();
    }
    nextLocal_ = offset;
}

int LocalVariablesSorter::newLocal(const Type& type)
{
    int local = nextLocal_;
    nextLocal_ += type.getSize();
    if (remappedVariableIndices_.size() <= (size_t)local) {
        remappedVariableIndices_.resize(local + 40, -1);
    }
    return local;
}

void LocalVariablesSorter::visitVarInsn(int opcode, int varIndex)
{
    ca::core::Any type;
    switch (opcode) {
    case Opcodes::ILOAD:
    case Opcodes::ISTORE: type = Opcodes::INTEGER; break;
    case Opcodes::FLOAD:
    case Opcodes::FSTORE: type = Opcodes::FLOAT; break;
    case Opcodes::LLOAD:
    case Opcodes::LSTORE: type = Opcodes::LONG; break;
    case Opcodes::DLOAD:
    case Opcodes::DSTORE: type = Opcodes::DOUBLE; break;
    case Opcodes::ALOAD:
    case Opcodes::ASTORE: type = Opcodes::UNINITIALIZED_THIS; break;
    case Opcodes::RET: break;
    default: break;
    }
    int  remappedIndex = remap(varIndex, type);
    auto isDoubleSlot  = [](const ca::core::Any& t) -> bool {
        auto* p = t.as<int>();
        return p && *p >= Opcodes::LONG;
    };
    int newMax = remappedIndex + (isDoubleSlot(type) ? 2 : 1);
    if (newMax > nextLocal_) nextLocal_ = newMax;
    if (mv) mv->visitVarInsn(opcode, remappedIndex);
}

void LocalVariablesSorter::visitIincInsn(int varIndex, int increment)
{
    int remappedIndex = remap(varIndex, Opcodes::INTEGER);
    if (mv) mv->visitIincInsn(remappedIndex, increment);
}

void LocalVariablesSorter::visitMaxs(int maxStack, int maxLocals)
{
    if (mv) mv->visitMaxs(maxStack, nextLocal_);
}

void LocalVariablesSorter::visitLocalVariable(Utf8StringRef name, Utf8StringRef descriptor,
                                              Utf8StringRef signature, Label* start, Label* end,
                                              int index)
{
    // remap 仅读取 typeid(int)；descriptor 以 Utf8StringRef 存入 ca::core::Any（可拷贝，借用调用方
    // arena 生命周期）。
    int remappedIndex = remap(index, ca::core::Any(descriptor));
    if (mv) mv->visitLocalVariable(name, descriptor, signature, start, end, remappedIndex);
}

void LocalVariablesSorter::visitFrame(int type, int nLocal, const std::vector<ca::core::Any>& local,
                                      int nStack, const std::vector<ca::core::Any>& stack)
{
    if (type != Opcodes::F_NEW && type != Opcodes::F_FULL) {
        if (mv) mv->visitFrame(type, nLocal, local, nStack, stack);
        return;
    }

    int maxIndex = 0;
    for (int i = 0; i < nLocal; i++) {
        int newIndex = remap(i, local[i]);
        if (newIndex > maxIndex) maxIndex = newIndex;
    }
    int                        newLocalCount = (std::max)(nextLocal_, maxIndex + 1);
    std::vector<ca::core::Any> finalLocal(newLocalCount, Opcodes::TOP);
    for (int i = 0; i < nLocal; i++) {
        int newIndex = remap(i, local[i]);
        if (newIndex < newLocalCount) {
            finalLocal[newIndex] = local[i];
        }
    }

    if (mv) mv->visitFrame(Opcodes::F_NEW, newLocalCount, finalLocal, nStack, stack);
}

int LocalVariablesSorter::getRemappedLocal(int oldIndex) const
{
    int idx = oldIndex * 2;
    if (idx < (int)remappedVariableIndices_.size()) {
        return remappedVariableIndices_[idx];
    }
    return -1;
}

void LocalVariablesSorter::setRemappedLocal(int oldIndex, const ca::core::Any& type)
{
    int idx = oldIndex * 2;
    while (idx >= (int)remappedVariableIndices_.size()) {
        remappedVariableIndices_.push_back(-1);
    }
    while ((int)remappedLocalTypes_.size() <= idx / 2) {
        remappedLocalTypes_.push_back(ca::core::Any{});
    }
    remappedLocalTypes_[oldIndex] = type;
}

int LocalVariablesSorter::remap(int oldIndex, const ca::core::Any& type)
{
    int newIndex = getRemappedLocal(oldIndex);
    if (newIndex < 0) {
        int idx = oldIndex * 2;
        if (idx >= (int)remappedVariableIndices_.size()) {
            remappedVariableIndices_.resize(idx + 40, -1);
        }
        newIndex                      = nextLocal_++;
        remappedVariableIndices_[idx] = newIndex;
        setRemappedLocal(oldIndex, type);

        // Handle double-slot types
        bool isDoubleSlot = false;
        if (auto* p = type.as<int>()) {
            isDoubleSlot = (*p == Opcodes::LONG || *p == Opcodes::DOUBLE);
        }
        if (isDoubleSlot) {
            int idx2 = oldIndex * 2 + 1;
            if (idx2 >= (int)remappedVariableIndices_.size()) {
                remappedVariableIndices_.resize(idx2 + 40, -1);
            }
            remappedVariableIndices_[idx2] = newIndex + 1;
            nextLocal_++;
        }
    }
    return newIndex;
}
