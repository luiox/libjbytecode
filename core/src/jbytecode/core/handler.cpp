#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/byte_vector.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/constants.hpp"

namespace jbytecode::core {

// ======================================================================
// Handler — original implementation
// ======================================================================

Handler* Handler::removeRange(Handler* firstHandler, Label* start, Label* end)
{
    if (firstHandler == nullptr) return nullptr;

    firstHandler->nextHandler = removeRange(firstHandler->nextHandler, start, end);

    int handlerStart = firstHandler->startPc->bytecodeOffset;
    int handlerEnd   = firstHandler->endPc->bytecodeOffset;
    int rangeStart   = start->bytecodeOffset;
    int rangeEnd     = end == nullptr ? 0x7FFFFFFF : end->bytecodeOffset;

    if (rangeStart >= handlerEnd || rangeEnd <= handlerStart) {
        return firstHandler;
    }

    if (rangeStart <= handlerStart) {
        if (rangeEnd >= handlerEnd) {
            Handler* next = firstHandler->nextHandler;
            delete firstHandler;
            return next;
        }
        else {
            Handler* replacement = new Handler(firstHandler, end, firstHandler->endPc);
            delete firstHandler;
            return replacement;
        }
    }
    else if (rangeEnd >= handlerEnd) {
        Handler* replacement = new Handler(firstHandler, firstHandler->startPc, start);
        delete firstHandler;
        return replacement;
    }
    else {
        Handler* rightPart     = new Handler(firstHandler, end, firstHandler->endPc);
        rightPart->nextHandler = firstHandler->nextHandler;
        Handler* leftPart      = new Handler(firstHandler, firstHandler->startPc, start);
        leftPart->nextHandler  = rightPart;
        delete firstHandler;
        return leftPart;
    }
}

int Handler::getExceptionTableLength(const Handler* firstHandler)
{
    int            length = 0;
    const Handler* h      = firstHandler;
    while (h != nullptr) {
        length++;
        h = h->nextHandler;
    }
    return length;
}

int Handler::getExceptionTableSize(const Handler* firstHandler)
{
    return 2 + 8 * getExceptionTableLength(firstHandler);
}

void Handler::putExceptionTable(const Handler* firstHandler, ByteVector& output)
{
    output.putShort(getExceptionTableLength(firstHandler));
    const Handler* h = firstHandler;
    while (h != nullptr) {
        output.putShort(h->startPc->bytecodeOffset)
            .putShort(h->endPc->bytecodeOffset)
            .putShort(h->handlerPc->bytecodeOffset)
            .putShort(h->catchType);
        h = h->nextHandler;
    }
}

// ======================================================================
// Label — ASM-style forward reference management
// ======================================================================

void Label::addForwardRef(int sourceInsnOffset, int refType, int handle)
{
    if (forwardReferences.empty()) {
        forwardReferences.push_back(0);   // lastIndex at [0]
    }
    int lastIdx = forwardReferences[0];
    if (lastIdx + 3 >= static_cast<int>(forwardReferences.size())) {
        forwardReferences.resize(forwardReferences.size() + 16);
    }
    forwardReferences[++lastIdx] = sourceInsnOffset;
    forwardReferences[++lastIdx] = refType | handle;
    forwardReferences[0]         = lastIdx;
}

void Label::put(ByteVector& code, int sourceInsnOffset, bool wide)
{
    if (!isResolved()) {
        if (wide) {
            addForwardRef(sourceInsnOffset, FORWARD_REFERENCE_TYPE_WIDE, code.size());
            code.putInt(-1);
        }
        else {
            addForwardRef(sourceInsnOffset, FORWARD_REFERENCE_TYPE_SHORT, code.size());
            code.putShort(-1);
        }
    }
    else {
        int offset = bytecodeOffset - sourceInsnOffset;
        if (wide) {
            code.putInt(offset);
        }
        else {
            code.putShort(offset);
        }
    }
}

void Label::put(ByteVector& stackMapTableEntries)
{
    if (!isResolved()) {
        addForwardRef(0, FORWARD_REFERENCE_TYPE_STACK_MAP, stackMapTableEntries.size());
    }
    stackMapTableEntries.putShort(bytecodeOffset);
}

bool Label::resolve(ca::u8* codeBytes, ByteVector* stackMapTableEntries, int bytecodeOffsetArg)
{
    flags |= FLAG_RESOLVED;
    bytecodeOffset = bytecodeOffsetArg;
    if (forwardReferences.empty()) return false;

    bool hasAsmInstructions = false;
    for (int i = forwardReferences[0]; i > 0; i -= 2) {
        int srcOffset      = forwardReferences[i - 1];
        int ref            = forwardReferences[i];
        int relativeOffset = bytecodeOffset - srcOffset;
        int handle         = ref & FORWARD_REFERENCE_HANDLE_MASK;

        if ((ref & FORWARD_REFERENCE_TYPE_MASK) == FORWARD_REFERENCE_TYPE_SHORT) {
            if (relativeOffset < static_cast<int>(0xFFFF8000) || relativeOffset > 0x7FFF) {
                int opcode = codeBytes[srcOffset] & 0xFF;
                if (opcode < Opcodes::IFNULL) {
                    codeBytes[srcOffset] =
                        static_cast<ca::u8>(opcode + Constants::ASM_OPCODE_DELTA);
                }
                else {
                    codeBytes[srcOffset] =
                        static_cast<ca::u8>(opcode + Constants::ASM_IFNULL_OPCODE_DELTA);
                }
                hasAsmInstructions = true;
            }
            codeBytes[handle++] = static_cast<ca::u8>(relativeOffset >> 8);
            codeBytes[handle]   = static_cast<ca::u8>(relativeOffset);
        }
        else if ((ref & FORWARD_REFERENCE_TYPE_MASK) == FORWARD_REFERENCE_TYPE_WIDE) {
            codeBytes[handle++] = static_cast<ca::u8>(relativeOffset >> 24);
            codeBytes[handle++] = static_cast<ca::u8>(relativeOffset >> 16);
            codeBytes[handle++] = static_cast<ca::u8>(relativeOffset >> 8);
            codeBytes[handle]   = static_cast<ca::u8>(relativeOffset);
        }
        else if ((ref & FORWARD_REFERENCE_TYPE_MASK) == FORWARD_REFERENCE_TYPE_STACK_MAP) {
            if (stackMapTableEntries == nullptr || stackMapTableEntries->mutableData() == nullptr) {
                continue;
            }
            auto* stackMapBytes     = stackMapTableEntries->mutableData();
            stackMapBytes[handle++] = static_cast<ca::u8>(bytecodeOffset >> 8);
            stackMapBytes[handle]   = static_cast<ca::u8>(bytecodeOffset);
        }
    }
    return hasAsmInstructions;
}

void Label::markSubroutine(short id)
{
    std::vector<Label*>        blocksToProcess{this};
    std::unordered_set<Label*> queuedOrProcessed{this};
    while (!blocksToProcess.empty()) {
        Label* basicBlock = blocksToProcess.back();
        blocksToProcess.pop_back();
        if (basicBlock->subroutineId == 0) {
            basicBlock->subroutineId = id;
            basicBlock->pushSuccessors(blocksToProcess, queuedOrProcessed);
        }
    }
}

void Label::addSubroutineRetSuccessors(Label* subroutineCaller)
{
    std::vector<Label*>        blocksToProcess{this};
    std::unordered_set<Label*> queuedOrProcessed{this};
    while (!blocksToProcess.empty()) {
        Label* basicBlock = blocksToProcess.back();
        blocksToProcess.pop_back();
        if ((basicBlock->flags & FLAG_SUBROUTINE_END) != 0 &&
            basicBlock->subroutineId != subroutineCaller->subroutineId &&
            subroutineCaller->outgoingEdges != nullptr) {
            basicBlock->outgoingEdges = new Edge(basicBlock->outputStackSize,
                                                 subroutineCaller->outgoingEdges->successor,
                                                 basicBlock->outgoingEdges);
        }
        basicBlock->pushSuccessors(blocksToProcess, queuedOrProcessed);
    }
}

void Label::pushSuccessors(std::vector<Label*>&        blocksToProcess,
                           std::unordered_set<Label*>& queuedOrProcessed)
{
    Edge* outgoingEdge = outgoingEdges;
    while (outgoingEdge != nullptr) {
        bool isJsrTarget = (flags & FLAG_SUBROUTINE_CALLER) != 0 && outgoingEdges != nullptr &&
                           outgoingEdge == outgoingEdges->nextEdge;
        if (!isJsrTarget && outgoingEdge->successor != nullptr &&
            queuedOrProcessed.insert(outgoingEdge->successor).second) {
            blocksToProcess.push_back(outgoingEdge->successor);
        }
        outgoingEdge = outgoingEdge->nextEdge;
    }
}

}   // namespace jbytecode::core
