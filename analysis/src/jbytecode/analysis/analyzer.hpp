#pragma once

#include <vector>
#include <deque>
#include <memory>
#include <algorithm>

#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/tree/insn_list.hpp"
#include "jbytecode/tree/nodes.hpp"
#include "jbytecode/tree/insn_type_traits.hpp"
#include "jbytecode/analysis/interpreter.hpp"
#include "jbytecode/analysis/subroutine.hpp"
#include "jbytecode/analysis/exceptions.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::analysis {

template<typename V>
class Analyzer
{
public:
    explicit Analyzer(Interpreter<V>* interpreter)
        : interpreter_(interpreter)
    {}

    virtual ~Analyzer() = default;

    std::vector<Frame<V>*> analyze(ca::str::Utf8StringRef       owner,
                                   jbytecode::tree::MethodNode* method)
    {
        (void)owner;
        if ((method->access & (jbytecode::core::Opcodes::ACC_ABSTRACT |
                               jbytecode::core::Opcodes::ACC_NATIVE)) != 0) {
            return {};
        }

        insnList_     = &method->instructions;
        insnListSize_ = insnList_->size();
        handlers_.assign(insnListSize_, std::vector<jbytecode::tree::TryCatchBlockNode*>());

        frameData_.clear();
        framesPtr_.clear();
        framesPtr_.resize(insnListSize_, nullptr);

        // 不能用 assign(n, nullptr)：MSVC 会实例化 std::fill 要求 unique_ptr
        // 可拷贝赋值。clear+resize 只需默认构造+移动插入，语义等价。
        subroutines_.clear();
        subroutines_.resize(insnListSize_);
        inInstructionsToProcess_.assign(insnListSize_, false);
        instructionsToProcess_.resize(insnListSize_);
        numInstructionsToProcess_ = 0;

        // Build handler lists
        for (auto* handler : method->tryCatchBlocks) {
            int startIdx = insnList_->indexOf(const_cast<jbytecode::tree::AbstractInsnNode*>(
                static_cast<const jbytecode::tree::AbstractInsnNode*>(
                    findLabelNode(handler->start))));
            int endIdx   = insnList_->indexOf(const_cast<jbytecode::tree::AbstractInsnNode*>(
                static_cast<const jbytecode::tree::AbstractInsnNode*>(
                    findLabelNode(handler->end))));
            if (startIdx < 0 || endIdx < 0) continue;
            for (int j = startIdx; j < endIdx; j++) {
                handlers_[j].push_back(handler);
            }
        }

        // Find initial frame size. Prefer Code attribute's maxLocals (authoritative
        // for real bytecode) over optional debug info or descriptor heuristic.
        int maxLocals = method->maxLocals;
        if (maxLocals == 0) {
            for (auto* local : method->localVariables) {
                bool isWide =
                    local->descriptor.byte_length() == 1 &&
                    (local->descriptor.byte_at(0) == 'J' || local->descriptor.byte_at(0) == 'D');
                int idx = local->index + (isWide ? 1 : 0);
                if (idx > maxLocals) maxLocals = idx;
            }
            if (maxLocals == 0) {
                maxLocals = jbytecode::core::Type::getArgumentsAndReturnSizes(method->desc) >> 2;
                if ((method->access & jbytecode::core::Opcodes::ACC_STATIC) != 0) maxLocals--;
            }
        }

        Frame<V> initialFrame = Frame<V>::newFrame(maxLocals, method->maxStack);
        initFrame(initialFrame, owner, method);

        int insnIdx = 0;
        merge(insnIdx, initialFrame);

        initFrame(insnIdx, nullptr);

        // Virtual hook for subclasses (e.g. CheckFrameAnalyzer)
        this->init(owner, method);

        while (numInstructionsToProcess_ > 0) {
            int idx                       = instructionsToProcess_[--numInstructionsToProcess_];
            inInstructionsToProcess_[idx] = false;
            jbytecode::tree::AbstractInsnNode* insn = insnList_->get(idx);
            try {
                Frame<V> frame = *framesPtr_[idx];
                switch (insn->getOpcode()) {
                case jbytecode::core::Opcodes::JSR: jsr(idx, insn); break;
                default: simulate(insn, frame, idx); break;
                }
            }
            catch (const jbytecode::analysis::AnalyzerException& e) {
                throw jbytecode::analysis::AnalyzerException("Error at instruction " +
                                                             std::to_string(idx) + ": " + e.what());
            }
        }

        // Clean up owned frames
        std::vector<Frame<V>*> result;
        result.swap(framesPtr_);
        return result;
    }

protected:
    virtual void init(ca::str::Utf8StringRef owner, jbytecode::tree::MethodNode* method)
    {
        (void)owner;
        (void)method;
    }

    std::vector<Frame<V>*>&     getFrames() { return framesPtr_; }
    jbytecode::tree::InsnList*  getInsnList() const { return insnList_; }
    void                        addNewFrame(int insnIdx) { newFrame(insnIdx); }
    jbytecode::tree::LabelNode* findLabelNode(jbytecode::core::Label* label) const
    {
        return findLabelNodeImpl(label);
    }

private:
    Interpreter<V>*                                               interpreter_;
    jbytecode::tree::InsnList*                                    insnList_;
    int                                                           insnListSize_;
    std::vector<std::vector<jbytecode::tree::TryCatchBlockNode*>> handlers_;

    std::deque<std::unique_ptr<Frame<V>>> frameData_;
    std::vector<Frame<V>*>                framesPtr_;
    // JSR/RET 子例程由本 Analyzer 独占持有；analyze() 开头 assign 清空，
    // 上一轮的 Subroutine 在此析构，不再泄漏。
    std::vector<std::unique_ptr<Subroutine>> subroutines_;

    std::vector<bool> inInstructionsToProcess_;
    std::vector<int>  instructionsToProcess_;
    int               numInstructionsToProcess_;

    void newFrame(int insnIdx)
    {
        frameData_.push_back(std::make_unique<Frame<V>>());
        framesPtr_[insnIdx] = frameData_.back().get();
    }

    void initFrame(Frame<V>& frame, ca::str::Utf8StringRef owner,
                   jbytecode::tree::MethodNode* method)
    {
        bool isStatic = (method->access & jbytecode::core::Opcodes::ACC_STATIC) != 0;
        jbytecode::core::Type methodType = jbytecode::core::Type::getMethodType(method->desc);
        std::vector<jbytecode::core::Type> argTypes = methodType.getArgumentTypes();

        int local = 0;
        if (!isStatic) {
            auto type = jbytecode::core::Type::getObjectType(owner);
            frame.setLocal(local++, interpreter_->newParameterValue(false, 0, type));
        }
        for (size_t i = 0; i < argTypes.size(); i++) {
            jbytecode::core::Type t   = argTypes[i];
            V                     val = interpreter_->newParameterValue(!isStatic, local, t);
            frame.setLocal(local++, val);
            if (t.getSize() == 2) {
                frame.setLocal(local++, interpreter_->newEmptyValue(local - 1));
            }
        }

        // Initialize remaining locals
        while (local < frame.getLocals()) {
            frame.setLocal(local++, interpreter_->newEmptyValue(local));
        }

        // Set return value (null for void methods, matching ASM computeInitialFrame).
        jbytecode::core::Type returnType = methodType.getReturnType();
        if (returnType == jbytecode::core::Type::VOID_TYPE) {
            frame.clearReturn();
        }
        else {
            frame.setReturn(interpreter_->newReturnTypeValue(returnType));
        }
    }

    void initFrame(int insnIdx, Subroutine* subroutine)
    {
        if (subroutine == nullptr) return;
        // JSR subroutine initialization
    }

    int merge(int insnIdx, const Frame<V>& frame)
    {
        if (insnIdx < 0 || insnIdx >= insnListSize_) return 0;
        bool      changed  = false;
        Frame<V>* oldFrame = framesPtr_[insnIdx];
        if (oldFrame == nullptr) {
            newFrame(insnIdx);
            *framesPtr_[insnIdx] = frame;
            changed              = true;
        }
        else {
            changed = oldFrame->merge(frame, interpreter_);
        }

        if (changed && !inInstructionsToProcess_[insnIdx]) {
            inInstructionsToProcess_[insnIdx]                   = true;
            instructionsToProcess_[numInstructionsToProcess_++] = insnIdx;
        }
        return changed ? 1 : 0;
    }

    void simulate(jbytecode::tree::AbstractInsnNode* insn, Frame<V>& frame, int insnIdx)
    {
        Frame<V> handlerFrame = frame;
        int      opcode       = insn->getOpcode();
        int      nextInsnIdx  = insnIdx + 1;
        int      jumpInsnIdx  = 0;

        // Execute the instruction's dataflow effect (pop operands, invoke
        // interpreter callbacks, push results) BEFORE propagating the resulting
        // frame to successors. Pseudo-instructions (LABEL/LINE/FRAME, opcode -1)
        // have no effect. JSR is routed to jsr() by the caller; RET's execute is
        // a no-op. (Mirrors ASM Analyzer: currentFrame.init(old).execute(insn).)
        if (opcode >= 0) {
            frame.execute(insn, interpreter_);
        }

        // Handle different instruction types
        switch (opcode) {
        case jbytecode::core::Opcodes::RET:
        {
            int var = ca::core::tag_cast::cast<jbytecode::tree::VarInsnNode*>(insn)->var;
            // Find subroutine for this RET
            Subroutine* sub = subroutines_[insnIdx].get();
            if (sub == nullptr) return;
            for (auto* caller : sub->callers) {
                int callerIdx = insnList_->indexOf(caller);
                merge(callerIdx + 1, frame);
            }
            return;
        }
        case jbytecode::core::Opcodes::JSR:
        {
            // Handled in jsr()
            return;
        }
        case jbytecode::core::Opcodes::GOTO:
        {
            auto* target    = ca::core::tag_cast::cast<jbytecode::tree::JumpInsnNode*>(insn)->label;
            auto* labelNode = findLabelNode(target);
            jumpInsnIdx     = insnList_->indexOf(labelNode);
            merge(jumpInsnIdx, frame);
            return;
        }
        case jbytecode::core::Opcodes::IFEQ:
        case jbytecode::core::Opcodes::IFNE:
        case jbytecode::core::Opcodes::IFLT:
        case jbytecode::core::Opcodes::IFGE:
        case jbytecode::core::Opcodes::IFGT:
        case jbytecode::core::Opcodes::IFLE:
        case jbytecode::core::Opcodes::IFNONNULL:
        case jbytecode::core::Opcodes::IFNULL:
        {
            auto* target    = ca::core::tag_cast::cast<jbytecode::tree::JumpInsnNode*>(insn)->label;
            auto* labelNode = findLabelNode(target);
            jumpInsnIdx     = insnList_->indexOf(labelNode);
            merge(jumpInsnIdx, frame);
            merge(nextInsnIdx, frame);
            return;
        }
        case jbytecode::core::Opcodes::IF_ICMPEQ:
        case jbytecode::core::Opcodes::IF_ICMPNE:
        case jbytecode::core::Opcodes::IF_ICMPLT:
        case jbytecode::core::Opcodes::IF_ICMPGE:
        case jbytecode::core::Opcodes::IF_ICMPGT:
        case jbytecode::core::Opcodes::IF_ICMPLE:
        case jbytecode::core::Opcodes::IF_ACMPEQ:
        case jbytecode::core::Opcodes::IF_ACMPNE:
        {
            auto* target    = ca::core::tag_cast::cast<jbytecode::tree::JumpInsnNode*>(insn)->label;
            auto* labelNode = findLabelNode(target);
            jumpInsnIdx     = insnList_->indexOf(labelNode);
            merge(jumpInsnIdx, frame);
            merge(nextInsnIdx, frame);
            return;
        }
        case jbytecode::core::Opcodes::TABLESWITCH:
        {
            auto* ts   = ca::core::tag_cast::cast<jbytecode::tree::TableSwitchInsnNode*>(insn);
            auto* dflt = findLabelNode(ts->dflt);
            merge(insnList_->indexOf(dflt), frame);
            for (auto* label : ts->labels) {
                merge(insnList_->indexOf(findLabelNode(label)), frame);
            }
            return;
        }
        case jbytecode::core::Opcodes::LOOKUPSWITCH:
        {
            auto* ls   = ca::core::tag_cast::cast<jbytecode::tree::LookupSwitchInsnNode*>(insn);
            auto* dflt = findLabelNode(ls->dflt);
            merge(insnList_->indexOf(dflt), frame);
            for (auto* label : ls->labels) {
                merge(insnList_->indexOf(findLabelNode(label)), frame);
            }
            return;
        }
        case jbytecode::core::Opcodes::ATHROW:
            // No normal successor; only exception handlers (handled below).
            propagateHandlers(insnIdx, handlerFrame, frame);
            return;
        case jbytecode::core::Opcodes::IRETURN:
        case jbytecode::core::Opcodes::LRETURN:
        case jbytecode::core::Opcodes::FRETURN:
        case jbytecode::core::Opcodes::DRETURN:
        case jbytecode::core::Opcodes::ARETURN:
        case jbytecode::core::Opcodes::RETURN:
            propagateHandlers(insnIdx, handlerFrame, frame);
            return;   // No normal successors
        default: break;
        }

        // For normal instructions, propagate to next instruction
        merge(nextInsnIdx, frame);

        // Propagate to exception handlers
        propagateHandlers(insnIdx, handlerFrame, frame);
    }

    // Merge exception-handler entry frames per ASM: for each handler covering
    // insnIdx, clear the stack and push a single exception value, using both the
    // frame *before* the instruction (beforeFrame) and *after* it (afterFrame).
    void propagateHandlers(int insnIdx, const Frame<V>& beforeFrame, const Frame<V>& afterFrame)
    {
        if (insnIdx < 0 || insnIdx >= insnListSize_) return;
        for (auto* handler : handlers_[insnIdx]) {
            auto* handlerNode = findLabelNode(handler->handler);
            int   handlerIdx  = insnList_->indexOf(handlerNode);
            if (handlerIdx < 0) continue;

            jbytecode::core::Type catchType =
                handler->type.is_empty()
                    ? jbytecode::core::Type::getObjectType(
                          ca::str::ZUtf8StringRef::from_static("java/lang/Throwable"))
                    : jbytecode::core::Type::getObjectType(handler->type);

            Frame<V> before = beforeFrame;
            before.clearStack();
            V exceptionValue = interpreter_->newExceptionValue(handler, &before, catchType);
            before.push(exceptionValue);
            merge(handlerIdx, before);

            Frame<V> after = afterFrame;
            after.clearStack();
            after.push(exceptionValue);
            merge(handlerIdx, after);
        }
    }

    void jsr(int insnIdx, jbytecode::tree::AbstractInsnNode* insn)
    {
        auto*     target     = ca::core::tag_cast::cast<jbytecode::tree::JumpInsnNode*>(insn)->label;
        auto*     targetNode = findLabelNode(target);
        int       jumpIdx    = insnList_->indexOf(targetNode);
        Frame<V>* frame      = framesPtr_[insnIdx];
        if (frame == nullptr) return;

        Subroutine* subroutine = subroutines_[jumpIdx].get();
        if (subroutine == nullptr) {
            auto owned = std::make_unique<Subroutine>(target, insn, std::vector<jbytecode::core::Label*>{});
            subroutine            = owned.get();
            subroutines_[jumpIdx] = std::move(owned);
        }
        else if (!subroutine->callers.empty()) {
            // Already processed, might need merge
        }
        merge(jumpIdx, *frame);
        merge(insnIdx + 1, *frame);
    }

    jbytecode::tree::LabelNode* findLabelNodeImpl(jbytecode::core::Label* label) const
    {
        for (auto* node : *insnList_) {
            if (node->getType() == jbytecode::tree::AbstractInsnNode::LABEL) {
                auto* ln = ca::core::tag_cast::cast<jbytecode::tree::LabelNode*>(node);
                if (ln && ln->label == label) return ln;
            }
        }
        return nullptr;
    }
};

}   // namespace jbytecode::analysis
