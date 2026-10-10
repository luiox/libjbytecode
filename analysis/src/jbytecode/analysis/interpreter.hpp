#pragma once

#include <vector>
#include <algorithm>

#include "jbytecode/core/type.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/tree/insn_list.hpp"
#include "jbytecode/tree/nodes.hpp"
#include "jbytecode/analysis/exceptions.hpp"
#include "libca/core/tag_cast.hpp"

namespace jbytecode::analysis {

class Value
{
public:
    virtual ~Value()            = default;
    virtual int getSize() const = 0;
};

template<typename V>
class Interpreter;

// Execution stack frame for dataflow analysis. Mirrors ASM's
// org.objectweb.asm.tree.analysis.Frame: values_ holds locals in [0,numLocals_)
// and the operand stack in [numLocals_, numLocals_+numStack_). Long/double take
// two local slots but a single stack slot.
template<typename V>
class Frame
{
public:
    static constexpr int MAX_STACK_SIZE = 1 << 16;

    Frame() = default;

    Frame(int numLocals, int maxStack) { reset(numLocals, maxStack); }

    Frame(const Frame& other) { copyFrom(other); }

    Frame& operator=(const Frame& other)
    {
        if (this != &other) copyFrom(other);
        return *this;
    }

    // ---- accessors (public API relied on by analyzer + CheckFrameAnalyzer) ----

    int getLocals() const { return numLocals_; }
    int getMaxStackSize() const { return maxStack_; }

    V getLocal(int i) const
    {
        if (i < 0 || i >= (int)values_.size()) return V{};
        return values_[i];
    }

    void setLocal(int i, const V& value)
    {
        if (i < 0) return;
        if (i >= (int)values_.size()) values_.resize(i + 1);
        if (i >= numLocals_) numLocals_ = i + 1;
        values_[i] = value;
    }

    int getStackSize() const { return numStack_; }

    V getStack(int i) const
    {
        int idx = numLocals_ + i;
        if (i < 0 || idx >= (int)values_.size()) return V{};
        return values_[idx];
    }

    void setStack(int i, const V& value)
    {
        int idx = numLocals_ + i;
        if (i < 0) return;
        if (idx >= (int)values_.size()) values_.resize(idx + 1);
        values_[idx] = value;
    }

    void clearStack() { numStack_ = 0; }

    void push(const V& value)
    {
        if (numLocals_ + numStack_ >= (int)values_.size()) {
            if (numLocals_ + numStack_ >= maxStack_)
                throw jbytecode::analysis::AnalyzerException("Insufficient maximum stack size.");
            values_.resize((std::max)((int)values_.size() * 2, numLocals_ + numStack_ + 1));
        }
        values_[numLocals_ + (numStack_++)] = value;
    }

    V pop()
    {
        if (numStack_ == 0)
            throw jbytecode::analysis::AnalyzerException("Cannot pop operand off an empty stack.");
        return values_[numLocals_ + (--numStack_)];
    }

    void setReturn(const V& value)
    {
        returnValue_    = value;
        hasReturnValue_ = true;
    }
    void clearReturn() { hasReturnValue_ = false; }

    // Copies the state of the given frame into this frame (ASM Frame.init).
    Frame& init(const Frame& frame)
    {
        copyFrom(frame);
        return *this;
    }

    // Hook for branch-sensitive analyses; no-op by default (ASM initJumpTarget).
    void initJumpTarget(int /*opcode*/, jbytecode::tree::LabelNode* /*target*/) {}

    static Frame newFrame(int nLocals, int maxStack) { return Frame(nLocals, maxStack); }

    // Strict merge matching ASM Frame.merge: stack heights must be equal; each
    // value slot (locals + stack) is merged via the interpreter.
    bool merge(const Frame& frame, Interpreter<V>* interpreter)
    {
        if (numStack_ != frame.numStack_)
            throw jbytecode::analysis::AnalyzerException("Incompatible stack heights");
        bool changed = false;
        int  n       = numLocals_ + numStack_;
        for (int i = 0; i < n; i++) {
            V v = interpreter->merge(values_[i], frame.values_[i]);
            if (!(v == values_[i])) {
                values_[i] = v;
                changed    = true;
            }
        }
        return changed;
    }

    void execute(const jbytecode::tree::AbstractInsnNode* insn, Interpreter<V>* interpreter);

private:
    std::vector<V> values_;
    int            numLocals_ = 0;
    int            numStack_  = 0;
    int            maxStack_  = 0;
    V              returnValue_{};
    bool           hasReturnValue_ = false;

    void reset(int numLocals, int maxStack)
    {
        values_.assign(numLocals + (maxStack >= 0 ? maxStack : 4), V{});
        numLocals_ = numLocals;
        numStack_  = 0;
        maxStack_  = maxStack >= 0 ? maxStack : MAX_STACK_SIZE;
    }

    void copyFrom(const Frame& other)
    {
        values_         = other.values_;
        numLocals_      = other.numLocals_;
        numStack_       = other.numStack_;
        maxStack_       = other.maxStack_;
        returnValue_    = other.returnValue_;
        hasReturnValue_ = other.hasReturnValue_;
    }
};

template<typename V>
class Interpreter
{
public:
    virtual ~Interpreter() = default;

    virtual V newValue(const jbytecode::core::Type* type) = 0;

    virtual V newParameterValue(bool isInstanceMethod, int local, const jbytecode::core::Type& type)
    {
        (void)isInstanceMethod;
        (void)local;
        return newValue(&type);
    }

    virtual V newReturnTypeValue(const jbytecode::core::Type& type) { return newValue(&type); }

    virtual V newEmptyValue(int local)
    {
        (void)local;
        return newValue(nullptr);
    }

    virtual V newExceptionValue(const jbytecode::tree::TryCatchBlockNode* tryCatchBlockNode,
                                const Frame<V>*                           handlerFrame,
                                const jbytecode::core::Type&              exceptionType)
    {
        (void)tryCatchBlockNode;
        (void)handlerFrame;
        return newValue(&exceptionType);
    }

    virtual V newOperation(const jbytecode::tree::AbstractInsnNode* insn)                   = 0;
    virtual V copyOperation(const jbytecode::tree::AbstractInsnNode* insn, const V& value)  = 0;
    virtual V unaryOperation(const jbytecode::tree::AbstractInsnNode* insn, const V& value) = 0;
    virtual V binaryOperation(const jbytecode::tree::AbstractInsnNode* insn, const V& value1,
                              const V& value2)                                              = 0;
    virtual V ternaryOperation(const jbytecode::tree::AbstractInsnNode* insn, const V& value1,
                               const V& value2, const V& value3)                            = 0;
    virtual V naryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                            const std::vector<V>&                    values)                                   = 0;

    virtual void returnOperation(const jbytecode::tree::AbstractInsnNode* insn, const V& value,
                                 const V& expected) = 0;

    virtual V merge(const V& value1, const V& value2) = 0;
};

}   // namespace jbytecode::analysis

#include "jbytecode/analysis/frame_execute.inl"
