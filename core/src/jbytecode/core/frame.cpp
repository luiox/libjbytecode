#include "jbytecode/core/frame.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/constants.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/symbol_table.hpp"
#include "jbytecode/core/writers.hpp"
#include <cstring>
#include <string>

using namespace ca::str;

namespace jbytecode::core {

// ======================================================================
// DefaultCommonSuperClassProvider (from frame.hpp)
// ======================================================================

Utf8StringRef DefaultCommonSuperClassProvider::getCommonSuperClass(const Utf8StringRef& type1,
                                                                   const Utf8StringRef& type2)
{
    auto directSuperClass = [](const std::string& type) -> const char* {
        if (type == "java/lang/Object") return nullptr;
        if (type == "java/lang/Number") return "java/lang/Object";
        if (type == "java/lang/Byte" || type == "java/lang/Short" || type == "java/lang/Integer" ||
            type == "java/lang/Long" || type == "java/lang/Float" || type == "java/lang/Double") {
            return "java/lang/Number";
        }
        if (type == "java/lang/Boolean" || type == "java/lang/Character" ||
            type == "java/lang/String") {
            return "java/lang/Object";
        }
        if (type == "java/lang/Throwable") return "java/lang/Object";
        if (type == "java/lang/Exception" || type == "java/lang/Error") {
            return "java/lang/Throwable";
        }
        if (type == "java/lang/RuntimeException") return "java/lang/Exception";
        if (type == "java/lang/IndexOutOfBoundsException") return "java/lang/RuntimeException";
        if (type == "java/lang/AssertionError") return "java/lang/Error";
        return "java/lang/Object";
    };
    auto refForName = [](const std::string& type) {
        if (type == "java/lang/Number") return Utf8StringRef::from_cstr("java/lang/Number");
        if (type == "java/lang/Throwable") return Utf8StringRef::from_cstr("java/lang/Throwable");
        if (type == "java/lang/Exception") return Utf8StringRef::from_cstr("java/lang/Exception");
        if (type == "java/lang/Error") return Utf8StringRef::from_cstr("java/lang/Error");
        if (type == "java/lang/RuntimeException")
            return Utf8StringRef::from_cstr("java/lang/RuntimeException");
        return Utf8StringRef::from_cstr("java/lang/Object");
    };
    auto isAssignableFrom = [&](const std::string& target, std::string source) {
        while (!source.empty()) {
            if (target == source) return true;
            const char* parent = directSuperClass(source);
            if (parent == nullptr) break;
            source = parent;
        }
        return false;
    };

    std::string t1 = type1.to_std_string();
    std::string t2 = type2.to_std_string();
    if (t1 == t2) return type1;
    if (isAssignableFrom(t1, t2)) return type1;
    if (isAssignableFrom(t2, t1)) return type2;
    for (std::string parent = t1;;) {
        const char* next = directSuperClass(parent);
        if (next == nullptr) break;
        parent = next;
        if (isAssignableFrom(parent, t2)) {
            return refForName(parent);
        }
    }
    return Utf8StringRef::from_cstr("java/lang/Object");
}

DefaultCommonSuperClassProvider defaultProvider;

// ======================================================================
// Constructor + copyFrom (ASM Frame.java L246-267)
// ======================================================================

Frame::Frame(Label* owner)
    : owner(owner)
{}

void Frame::copyFrom(const Frame& source)
{
    inputLocals         = source.inputLocals;
    inputStack          = source.inputStack;
    outputStackStart    = 0;
    outputLocals        = source.outputLocals;
    outputStack         = source.outputStack;
    outputStackTop      = source.outputStackTop;
    initializationCount = source.initializationCount;
    initializations     = source.initializations;
}

// ======================================================================
// Static converters (ASM Frame.java L284-385)
// ======================================================================

int Frame::getAbstractTypeFromApiFormat(SymbolTable& symbolTable, const ca::core::Any& type)
{
    Label* label = nullptr;
    if (auto* p = type.as<int>()) {
        return CONSTANT_KIND | *p;
    }
    else if (auto* p = type.as<Label*>()) {
        label = *p;
    }
    else if (auto* p = type.as<const Label*>()) {
        label = const_cast<Label*>(*p);
    }
    if (label) {
        if ((label->flags & Label::FLAG_RESOLVED) != 0) {
            return UNINITIALIZED_KIND | symbolTable.addUninitializedType(
                                            Utf8StringRef::from_cstr(""), label->bytecodeOffset);
        }
        else {
            return FORWARD_UNINITIALIZED_KIND |
                   symbolTable.addForwardUninitializedType(Utf8StringRef::from_cstr(""), label);
        }
    }
    // String type for OBJECT (internal name)
    if (auto* p = type.as<Utf8StringRef>()) {
        std::string desc = "L";
        desc.append(reinterpret_cast<const char*>(p->data()), p->byte_length());
        desc.push_back(';');
        Utf8StringRef descRef(
            reinterpret_cast<const ca::u8*>(desc.data()), desc.size(), desc.size());
        return getAbstractTypeFromDescriptor(symbolTable, descRef, 0);
    }
    return TOP;
}

int Frame::getAbstractTypeFromInternalName(SymbolTable&         symbolTable,
                                           const Utf8StringRef& internalName)
{
    return REFERENCE_KIND | symbolTable.addType(internalName);
}

int Frame::getAbstractTypeFromDescriptor(SymbolTable& symbolTable, const Utf8StringRef& buffer,
                                         int offset)
{
    if (offset >= static_cast<int>(buffer.byte_length())) return TOP;
    switch (buffer.byte_at(offset)) {
    case 'V': return 0;
    case 'Z':
    case 'C':
    case 'B':
    case 'S':
    case 'I': return INTEGER;
    case 'F': return FLOAT;
    case 'J': return LONG;
    case 'D': return DOUBLE;
    case 'L':
    {
        ca::usize end = offset + 1;
        while (end < buffer.byte_length() && buffer.byte_at(end) != ';') ++end;
        auto internalName = buffer.slice(offset + 1, end);
        return REFERENCE_KIND | symbolTable.addType(internalName);
    }
    case '[':
    {
        int elemOff = offset + 1;
        while (elemOff < static_cast<int>(buffer.byte_length()) && buffer.byte_at(elemOff) == '[')
            ++elemOff;
        int typeValue;
        int elemKind = CONSTANT_KIND;
        switch (buffer.byte_at(elemOff)) {
        case 'Z': typeValue = ITEM_ASM_BOOLEAN; break;
        case 'C': typeValue = ITEM_ASM_CHAR; break;
        case 'B': typeValue = ITEM_ASM_BYTE; break;
        case 'S': typeValue = ITEM_ASM_SHORT; break;
        case 'I': typeValue = ITEM_INTEGER; break;
        case 'F': typeValue = ITEM_FLOAT; break;
        case 'J': typeValue = ITEM_LONG; break;
        case 'D': typeValue = ITEM_DOUBLE; break;
        case 'L':
        {
            ca::usize end = elemOff + 1;
            while (end < buffer.byte_length() && buffer.byte_at(end) != ';') ++end;
            auto internalName = buffer.slice(elemOff + 1, end);
            typeValue         = symbolTable.addType(internalName);
            elemKind          = REFERENCE_KIND;
            break;
        }
        default: return TOP;
        }
        int arrayDim = elemOff - offset;
        return (arrayDim << DIM_SHIFT) | elemKind | typeValue;
    }
    default: return TOP;
    }
}

// ======================================================================
// Input frame initialization (ASM Frame.java L401-473)
// ======================================================================

void Frame::setInputFrameFromDescriptor(SymbolTable& symbolTable, int access,
                                        const Utf8StringRef& descriptor, int maxLocals)
{
    inputLocals.assign(maxLocals, 0);
    inputStack.clear();

    int idx = 0;
    if ((access & Opcodes::ACC_STATIC) == 0) {
        if ((access & Constants::ACC_CONSTRUCTOR) != 0) {
            inputLocals[idx++] = UNINITIALIZED_THIS;
        }
        else {
            inputLocals[idx++] = REFERENCE_KIND | symbolTable.addType(symbolTable.getClassName());
        }
    }

    // Parse method descriptor parameter types
    ca::usize i = 1;   // skip '('
    while (i < descriptor.byte_length() && descriptor.byte_at(i) != ')') {
        int start          = static_cast<int>(i);
        int at             = getAbstractTypeFromDescriptor(symbolTable, descriptor, start);
        inputLocals[idx++] = at;
        if (at == LONG || at == DOUBLE) inputLocals[idx++] = TOP;
        // Advance through the descriptor
        if (descriptor.byte_at(i) == 'L') {
            while (i < descriptor.byte_length() && descriptor.byte_at(i) != ';') ++i;
            if (i < descriptor.byte_length()) ++i;
        }
        else if (descriptor.byte_at(i) == '[') {
            while (i < descriptor.byte_length() && descriptor.byte_at(i) == '[') ++i;
            if (i < descriptor.byte_length() && descriptor.byte_at(i) == 'L') {
                while (i < descriptor.byte_length() && descriptor.byte_at(i) != ';') ++i;
                if (i < descriptor.byte_length()) ++i;
            }
            else {
                ++i;
            }
        }
        else {
            ++i;
        }
    }
    while (idx < maxLocals) inputLocals[idx++] = TOP;
}

void Frame::setInputFrameFromApiFormat(SymbolTable& symbolTable, int numLocal,
                                       const std::vector<ca::core::Any>& local, int numStack,
                                       const std::vector<ca::core::Any>& stack)
{
    inputLocals.assign(numLocal, 0);
    int idx = 0;
    for (int i = 0; i < numLocal; ++i) {
        if (static_cast<size_t>(i) >= local.size()) break;
        int at             = getAbstractTypeFromApiFormat(symbolTable, local[i]);
        inputLocals[idx++] = at;
        if ((at == LONG || at == DOUBLE) || ([&]() -> bool {
                auto* p = local[i].as<int>();
                return p && (*p == Opcodes::LONG || *p == Opcodes::DOUBLE);
            })()) {
            if (idx < numLocal) inputLocals[idx++] = TOP;
        }
    }

    int numStackTop = 0;
    for (int i = 0; i < numStack && static_cast<size_t>(i) < stack.size(); ++i) {
        int at = getAbstractTypeFromApiFormat(symbolTable, stack[i]);
        if (at == LONG || at == DOUBLE) ++numStackTop;
    }
    inputStack.resize(numStack + numStackTop);
    int stackIdx = 0;
    for (int i = 0; i < numStack && static_cast<size_t>(i) < stack.size(); ++i) {
        int at                 = getAbstractTypeFromApiFormat(symbolTable, stack[i]);
        inputStack[stackIdx++] = at;
        if (at == LONG || at == DOUBLE) inputStack[stackIdx++] = TOP;
    }
    outputStackTop      = 0;
    initializationCount = 0;
}

// ======================================================================
// Output frame helpers (ASM Frame.java L489-615)
// ======================================================================

int Frame::getLocal(int localIndex)
{
    if (outputLocals.empty() || localIndex >= static_cast<int>(outputLocals.size()))
        return LOCAL_KIND | localIndex;
    int abstractType = outputLocals[localIndex];
    if (abstractType == 0) {
        abstractType             = LOCAL_KIND | localIndex;
        outputLocals[localIndex] = abstractType;
    }
    return abstractType;
}

void Frame::setLocal(int localIndex, int abstractType)
{
    if (outputLocals.empty()) outputLocals.resize(10, 0);
    int outputLocalsLength = static_cast<int>(outputLocals.size());
    if (localIndex >= outputLocalsLength) {
        outputLocals.resize(std::max(localIndex + 1, 2 * outputLocalsLength), 0);
    }
    outputLocals[localIndex] = abstractType;
}

void Frame::push(int abstractType)
{
    if (outputStack.empty()) outputStack.resize(10, 0);
    int outputStackLength = static_cast<int>(outputStack.size());
    if (outputStackTop >= outputStackLength) {
        outputStack.resize(std::max(outputStackTop + 1, 2 * outputStackLength), 0);
    }
    outputStack[outputStackTop++] = abstractType;
    if (owner) {
        short outputStackSize = static_cast<short>(outputStackStart + outputStackTop);
        if (outputStackSize > owner->outputStackMax) owner->outputStackMax = outputStackSize;
    }
}

void Frame::push(SymbolTable& symbolTable, const Utf8StringRef& descriptor)
{
    int typeDescriptorOffset = 0;
    if (descriptor.byte_length() > 0 && descriptor.byte_at(0) == '(') {
        // Find return type offset
        ca::usize pos = 1;
        while (pos < descriptor.byte_length() && descriptor.byte_at(pos) != ')') ++pos;
        if (pos < descriptor.byte_length()) pos++;   // skip ')'
        typeDescriptorOffset = static_cast<int>(pos);
    }
    if (typeDescriptorOffset >= static_cast<int>(descriptor.byte_length())) return;
    if (descriptor.byte_at(typeDescriptorOffset) == 'V') return;
    int abstractType = getAbstractTypeFromDescriptor(symbolTable, descriptor, typeDescriptorOffset);
    if (abstractType != 0) {
        push(abstractType);
        if (abstractType == LONG || abstractType == DOUBLE) push(TOP);
    }
}

int Frame::pop()
{
    if (outputStackTop > 0) return outputStack[--outputStackTop];
    --outputStackStart;
    return STACK_KIND | (-outputStackStart);
}

void Frame::pop(int elements)
{
    if (outputStackTop >= elements) {
        outputStackTop -= elements;
    }
    else {
        outputStackStart -= elements - outputStackTop;
        outputStackTop = 0;
    }
}

void Frame::pop(const Utf8StringRef& descriptor)
{
    if (descriptor.byte_length() == 0) return;
    ca::u8 first = descriptor.byte_at(0);
    if (first == '(') {
        // Count argument slots
        int slots = 0;
        for (ca::usize i = 1; i < descriptor.byte_length() && descriptor.byte_at(i) != ')';) {
            if (descriptor.byte_at(i) == 'J' || descriptor.byte_at(i) == 'D') {
                slots += 2;
                ++i;
            }
            else if (descriptor.byte_at(i) == 'L') {
                slots += 1;
                while (i < descriptor.byte_length() && descriptor.byte_at(i) != ';') ++i;
                if (i < descriptor.byte_length()) ++i;
            }
            else if (descriptor.byte_at(i) == '[') {
                slots += 1;
                while (i < descriptor.byte_length() && descriptor.byte_at(i) == '[') ++i;
                if (i < descriptor.byte_length() && descriptor.byte_at(i) == 'L') {
                    while (i < descriptor.byte_length() && descriptor.byte_at(i) != ';') ++i;
                    if (i < descriptor.byte_length()) ++i;
                }
                else if (i < descriptor.byte_length()) {
                    ++i;
                }
            }
            else {
                slots += 1;
                ++i;
            }
        }
        pop(slots);
    }
    else if (first == 'J' || first == 'D') {
        pop(2);
    }
    else {
        pop(1);
    }
}

// ======================================================================
// Uninitialized type handling (ASM Frame.java L627-678)
// ======================================================================

void Frame::addInitializedType(int abstractType)
{
    if (initializations.empty()) initializations.resize(2, 0);
    int len = static_cast<int>(initializations.size());
    if (initializationCount >= len) {
        initializations.resize(std::max(initializationCount + 1, 2 * len), 0);
    }
    initializations[initializationCount++] = abstractType;
}

int Frame::getInitializedType(SymbolTable& symbolTable, int abstractType)
{
    if (abstractType == UNINITIALIZED_THIS ||
        (abstractType & (DIM_MASK | KIND_MASK)) == UNINITIALIZED_KIND ||
        (abstractType & (DIM_MASK | KIND_MASK)) == FORWARD_UNINITIALIZED_KIND) {
        for (int i = 0; i < initializationCount; ++i) {
            int initializedType = initializations[i];
            int dim             = initializedType & DIM_MASK;
            int kind            = initializedType & KIND_MASK;
            int value           = initializedType & VALUE_MASK;
            if (kind == LOCAL_KIND) {
                initializedType = dim + inputLocals[value];
            }
            else if (kind == STACK_KIND) {
                initializedType = dim + inputStack[static_cast<int>(inputStack.size()) - value];
            }
            if (abstractType == initializedType) {
                if (abstractType == UNINITIALIZED_THIS) {
                    return REFERENCE_KIND | symbolTable.addType(symbolTable.getClassName());
                }
                else {
                    return REFERENCE_KIND |
                           symbolTable.addType(
                               symbolTable.getType(abstractType & VALUE_MASK)->value);
                }
            }
        }
    }
    return abstractType;
}

// ======================================================================
// Concrete type resolution (ASM Frame.java L1134-1160)
// ======================================================================

int Frame::getConcreteOutputType(int abstractOutputType, int numStack)
{
    int dim  = abstractOutputType & DIM_MASK;
    int kind = abstractOutputType & KIND_MASK;
    if (kind == LOCAL_KIND) {
        int idx = abstractOutputType & VALUE_MASK;
        int concreteOutputType =
            dim + (idx < static_cast<int>(inputLocals.size()) ? inputLocals[idx] : TOP);
        if ((abstractOutputType & TOP_IF_LONG_OR_DOUBLE_FLAG) != 0 &&
            (concreteOutputType == LONG || concreteOutputType == DOUBLE)) {
            concreteOutputType = TOP;
        }
        return concreteOutputType;
    }
    else if (kind == STACK_KIND) {
        int pos = numStack - (abstractOutputType & VALUE_MASK);
        int concreteOutputType =
            dim + (pos >= 0 && pos < static_cast<int>(inputStack.size()) ? inputStack[pos] : TOP);
        if ((abstractOutputType & TOP_IF_LONG_OR_DOUBLE_FLAG) != 0 &&
            (concreteOutputType == LONG || concreteOutputType == DOUBLE)) {
            concreteOutputType = TOP;
        }
        return concreteOutputType;
    }
    else {
        return abstractOutputType;
    }
}

// ======================================================================
// execute() — Instruction simulation (ASM Frame.java L692-1120)
// ======================================================================

void Frame::execute(int opcode, int arg, Symbol* argSymbol, SymbolTable& symbolTable)
{
    int abstractType1;
    int abstractType2;
    int abstractType3;
    int abstractType4;

    switch (opcode) {
    case Opcodes::NOP:
    case Opcodes::INEG:
    case Opcodes::LNEG:
    case Opcodes::FNEG:
    case Opcodes::DNEG:
    case Opcodes::I2B:
    case Opcodes::I2C:
    case Opcodes::I2S:
    case Opcodes::GOTO:
    case Opcodes::RETURN: break;

    case Opcodes::ACONST_NULL: push(NULL_T); break;

    case Opcodes::ICONST_M1:
    case Opcodes::ICONST_0:
    case Opcodes::ICONST_1:
    case Opcodes::ICONST_2:
    case Opcodes::ICONST_3:
    case Opcodes::ICONST_4:
    case Opcodes::ICONST_5:
    case Opcodes::BIPUSH:
    case Opcodes::SIPUSH:
    case Opcodes::ILOAD: push(INTEGER); break;

    case Opcodes::LCONST_0:
    case Opcodes::LCONST_1:
    case Opcodes::LLOAD:
        push(LONG);
        push(TOP);
        break;

    case Opcodes::FCONST_0:
    case Opcodes::FCONST_1:
    case Opcodes::FCONST_2:
    case Opcodes::FLOAD: push(FLOAT); break;

    case Opcodes::DCONST_0:
    case Opcodes::DCONST_1:
    case Opcodes::DLOAD:
        push(DOUBLE);
        push(TOP);
        break;

    case Opcodes::LDC:
    {
        if (!argSymbol) {
            push(TOP);
            break;
        }
        switch (argSymbol->tag) {
        case Symbol::CONSTANT_INTEGER_TAG: push(INTEGER); break;
        case Symbol::CONSTANT_LONG_TAG:
            push(LONG);
            push(TOP);
            break;
        case Symbol::CONSTANT_FLOAT_TAG: push(FLOAT); break;
        case Symbol::CONSTANT_DOUBLE_TAG:
            push(DOUBLE);
            push(TOP);
            break;
        case Symbol::CONSTANT_CLASS_TAG:
            push(REFERENCE_KIND | symbolTable.addType(Utf8StringRef::from_cstr("java/lang/Class")));
            break;
        case Symbol::CONSTANT_STRING_TAG:
            push(REFERENCE_KIND |
                 symbolTable.addType(Utf8StringRef::from_cstr("java/lang/String")));
            break;
        case Symbol::CONSTANT_METHOD_TYPE_TAG:
            push(REFERENCE_KIND |
                 symbolTable.addType(Utf8StringRef::from_cstr("java/lang/invoke/MethodType")));
            break;
        case Symbol::CONSTANT_METHOD_HANDLE_TAG:
            push(REFERENCE_KIND |
                 symbolTable.addType(Utf8StringRef::from_cstr("java/lang/invoke/MethodHandle")));
            break;
        case Symbol::CONSTANT_DYNAMIC_TAG:
        case Symbol::CONSTANT_INVOKE_DYNAMIC_TAG: push(symbolTable, argSymbol->value); break;
        default: break;
        }
        break;
    }

    case Opcodes::ALOAD: push(getLocal(arg)); break;

    case Opcodes::LALOAD:
    case Opcodes::D2L:
        pop(2);
        push(LONG);
        push(TOP);
        break;

    case Opcodes::DALOAD:
    case Opcodes::L2D:
        pop(2);
        push(DOUBLE);
        push(TOP);
        break;

    case Opcodes::AALOAD:
        pop(1);
        abstractType1 = pop();
        push(abstractType1 == NULL_T ? abstractType1 : ELEMENT_OF + abstractType1);
        break;

    case Opcodes::ISTORE:
    case Opcodes::FSTORE:
    case Opcodes::ASTORE:
        abstractType1 = pop();
        setLocal(arg, abstractType1);
        if (arg > 0) {
            int previousLocalType = getLocal(arg - 1);
            if (previousLocalType == LONG || previousLocalType == DOUBLE) {
                setLocal(arg - 1, TOP);
            }
            else if ((previousLocalType & KIND_MASK) == LOCAL_KIND ||
                     (previousLocalType & KIND_MASK) == STACK_KIND) {
                setLocal(arg - 1, previousLocalType | TOP_IF_LONG_OR_DOUBLE_FLAG);
            }
        }
        break;

    case Opcodes::LSTORE:
    case Opcodes::DSTORE:
        pop(1);
        abstractType1 = pop();
        setLocal(arg, abstractType1);
        setLocal(arg + 1, TOP);
        if (arg > 0) {
            int previousLocalType = getLocal(arg - 1);
            if (previousLocalType == LONG || previousLocalType == DOUBLE) {
                setLocal(arg - 1, TOP);
            }
            else if ((previousLocalType & KIND_MASK) == LOCAL_KIND ||
                     (previousLocalType & KIND_MASK) == STACK_KIND) {
                setLocal(arg - 1, previousLocalType | TOP_IF_LONG_OR_DOUBLE_FLAG);
            }
        }
        break;

    case Opcodes::IASTORE:
    case Opcodes::BASTORE:
    case Opcodes::CASTORE:
    case Opcodes::SASTORE:
    case Opcodes::FASTORE:
    case Opcodes::AASTORE: pop(3); break;

    case Opcodes::LASTORE:
    case Opcodes::DASTORE: pop(4); break;

    case Opcodes::POP:
    case Opcodes::IFEQ:
    case Opcodes::IFNE:
    case Opcodes::IFLT:
    case Opcodes::IFGE:
    case Opcodes::IFGT:
    case Opcodes::IFLE:
    case Opcodes::IRETURN:
    case Opcodes::FRETURN:
    case Opcodes::ARETURN:
    case Opcodes::TABLESWITCH:
    case Opcodes::LOOKUPSWITCH:
    case Opcodes::ATHROW:
    case Opcodes::MONITORENTER:
    case Opcodes::MONITOREXIT:
    case Opcodes::IFNULL:
    case Opcodes::IFNONNULL: pop(1); break;

    case Opcodes::POP2:
    case Opcodes::IF_ICMPEQ:
    case Opcodes::IF_ICMPNE:
    case Opcodes::IF_ICMPLT:
    case Opcodes::IF_ICMPGE:
    case Opcodes::IF_ICMPGT:
    case Opcodes::IF_ICMPLE:
    case Opcodes::IF_ACMPEQ:
    case Opcodes::IF_ACMPNE:
    case Opcodes::LRETURN:
    case Opcodes::DRETURN: pop(2); break;

    case Opcodes::DUP:
        abstractType1 = pop();
        push(abstractType1);
        push(abstractType1);
        break;

    case Opcodes::DUP_X1:
        abstractType1 = pop();
        abstractType2 = pop();
        push(abstractType1);
        push(abstractType2);
        push(abstractType1);
        break;

    case Opcodes::DUP_X2:
        abstractType1 = pop();
        abstractType2 = pop();
        abstractType3 = pop();
        push(abstractType1);
        push(abstractType3);
        push(abstractType2);
        push(abstractType1);
        break;

    case Opcodes::DUP2:
        abstractType1 = pop();
        abstractType2 = pop();
        push(abstractType2);
        push(abstractType1);
        push(abstractType2);
        push(abstractType1);
        break;

    case Opcodes::DUP2_X1:
        abstractType1 = pop();
        abstractType2 = pop();
        abstractType3 = pop();
        push(abstractType2);
        push(abstractType1);
        push(abstractType3);
        push(abstractType2);
        push(abstractType1);
        break;

    case Opcodes::DUP2_X2:
        abstractType1 = pop();
        abstractType2 = pop();
        abstractType3 = pop();
        abstractType4 = pop();
        push(abstractType2);
        push(abstractType1);
        push(abstractType4);
        push(abstractType3);
        push(abstractType2);
        push(abstractType1);
        break;

    case Opcodes::SWAP:
        abstractType1 = pop();
        abstractType2 = pop();
        push(abstractType1);
        push(abstractType2);
        break;

    case Opcodes::IALOAD:
    case Opcodes::BALOAD:
    case Opcodes::CALOAD:
    case Opcodes::SALOAD:
    case Opcodes::IADD:
    case Opcodes::ISUB:
    case Opcodes::IMUL:
    case Opcodes::IDIV:
    case Opcodes::IREM:
    case Opcodes::IAND:
    case Opcodes::IOR:
    case Opcodes::IXOR:
    case Opcodes::ISHL:
    case Opcodes::ISHR:
    case Opcodes::IUSHR:
    case Opcodes::L2I:
    case Opcodes::D2I:
    case Opcodes::FCMPL:
    case Opcodes::FCMPG:
        pop(2);
        push(INTEGER);
        break;

    case Opcodes::LADD:
    case Opcodes::LSUB:
    case Opcodes::LMUL:
    case Opcodes::LDIV:
    case Opcodes::LREM:
    case Opcodes::LAND:
    case Opcodes::LOR:
    case Opcodes::LXOR:
        pop(4);
        push(LONG);
        push(TOP);
        break;

    case Opcodes::FALOAD:
    case Opcodes::FADD:
    case Opcodes::FSUB:
    case Opcodes::FMUL:
    case Opcodes::FDIV:
    case Opcodes::FREM:
    case Opcodes::L2F:
    case Opcodes::D2F:
        pop(2);
        push(FLOAT);
        break;

    case Opcodes::DADD:
    case Opcodes::DSUB:
    case Opcodes::DMUL:
    case Opcodes::DDIV:
    case Opcodes::DREM:
        pop(4);
        push(DOUBLE);
        push(TOP);
        break;

    case Opcodes::LSHL:
    case Opcodes::LSHR:
    case Opcodes::LUSHR:
        pop(3);
        push(LONG);
        push(TOP);
        break;

    case Opcodes::IINC: setLocal(arg, INTEGER); break;

    case Opcodes::I2L:
    case Opcodes::F2L:
        pop(1);
        push(LONG);
        push(TOP);
        break;

    case Opcodes::I2F:
        pop(1);
        push(FLOAT);
        break;

    case Opcodes::I2D:
    case Opcodes::F2D:
        pop(1);
        push(DOUBLE);
        push(TOP);
        break;

    case Opcodes::F2I:
    case Opcodes::ARRAYLENGTH:
    case Opcodes::INSTANCEOF:
        pop(1);
        push(INTEGER);
        break;

    case Opcodes::LCMP:
    case Opcodes::DCMPL:
    case Opcodes::DCMPG:
        pop(4);
        push(INTEGER);
        break;

    case Opcodes::JSR:
    case Opcodes::RET: break;

    case Opcodes::GETSTATIC:
        if (argSymbol) push(symbolTable, argSymbol->value);
        break;

    case Opcodes::PUTSTATIC:
        if (argSymbol) pop(argSymbol->value);
        break;

    case Opcodes::GETFIELD:
        pop(1);
        if (argSymbol) push(symbolTable, argSymbol->value);
        break;

    case Opcodes::PUTFIELD:
        if (argSymbol) pop(argSymbol->value);
        pop();
        break;

    case Opcodes::INVOKEVIRTUAL:
    case Opcodes::INVOKESPECIAL:
    case Opcodes::INVOKESTATIC:
    case Opcodes::INVOKEINTERFACE:
        if (argSymbol) pop(argSymbol->value);
        if (opcode != Opcodes::INVOKESTATIC) {
            abstractType1 = pop();
            if (opcode == Opcodes::INVOKESPECIAL && argSymbol &&
                argSymbol->name.byte_length() > 0 && argSymbol->name.byte_at(0) == '<') {
                addInitializedType(abstractType1);
            }
        }
        if (argSymbol) push(symbolTable, argSymbol->value);
        break;

    case Opcodes::INVOKEDYNAMIC:
        if (argSymbol) pop(argSymbol->value);
        if (argSymbol) push(symbolTable, argSymbol->value);
        break;

    case Opcodes::NEW:
        if (argSymbol) {
            push(UNINITIALIZED_KIND | symbolTable.addUninitializedType(argSymbol->value, arg));
        }
        break;

    case Opcodes::NEWARRAY:
        pop();
        switch (arg) {
        case Opcodes::T_BOOLEAN: push(ARRAY_OF | BOOLEAN); break;
        case Opcodes::T_CHAR: push(ARRAY_OF | CHAR); break;
        case Opcodes::T_BYTE: push(ARRAY_OF | BYTE); break;
        case Opcodes::T_SHORT: push(ARRAY_OF | SHORT); break;
        case Opcodes::T_INT: push(ARRAY_OF | INTEGER); break;
        case Opcodes::T_FLOAT: push(ARRAY_OF | FLOAT); break;
        case Opcodes::T_DOUBLE: push(ARRAY_OF | DOUBLE); break;
        case Opcodes::T_LONG: push(ARRAY_OF | LONG); break;
        default: break;
        }
        break;

    case Opcodes::ANEWARRAY:
        pop();
        if (argSymbol) {
            Utf8StringRef arrayElementType = argSymbol->value;
            if (arrayElementType.byte_length() > 0 && arrayElementType.byte_at(0) == '[') {
                std::string arrDesc = "[";
                arrDesc.append(reinterpret_cast<const char*>(arrayElementType.data()),
                               arrayElementType.byte_length());
                Utf8StringRef arrRef(reinterpret_cast<const ca::u8*>(arrDesc.data()),
                                     arrDesc.size(),
                                     arrDesc.size());
                push(symbolTable, arrRef);
            }
            else {
                push(ARRAY_OF | REFERENCE_KIND | symbolTable.addType(arrayElementType));
            }
        }
        break;

    case Opcodes::CHECKCAST:
        pop();
        if (argSymbol) {
            Utf8StringRef castType = argSymbol->value;
            if (castType.byte_length() > 0 && castType.byte_at(0) == '[') {
                push(symbolTable, castType);
            }
            else {
                push(REFERENCE_KIND | symbolTable.addType(castType));
            }
        }
        break;

    case Opcodes::MULTIANEWARRAY:
        pop(arg);
        if (argSymbol) push(symbolTable, argSymbol->value);
        break;

    default: break;
    }
}

// ======================================================================
// Frame merging (ASM Frame.java L1174-1352)
// ======================================================================

bool Frame::merge(SymbolTable& symbolTable, Frame& dstFrame, int catchTypeIndex,
                  ICommonSuperClassProvider* typeResolver)
{
    bool frameChanged = false;

    int numLocal = static_cast<int>(inputLocals.size());
    int numStack = static_cast<int>(inputStack.size());

    if (dstFrame.inputLocals.empty()) {
        dstFrame.inputLocals.resize(numLocal, 0);
        frameChanged = true;
    }

    for (int i = 0; i < numLocal; ++i) {
        int concreteOutputType;
        if (!outputLocals.empty() && i < static_cast<int>(outputLocals.size())) {
            int abstractOutputType = outputLocals[i];
            if (abstractOutputType == 0) {
                concreteOutputType = inputLocals[i];
            }
            else {
                concreteOutputType = getConcreteOutputType(abstractOutputType, numStack);
            }
        }
        else {
            concreteOutputType = inputLocals[i];
        }
        if (!initializations.empty()) {
            concreteOutputType = getInitializedType(symbolTable, concreteOutputType);
        }
        if (i >= static_cast<int>(dstFrame.inputLocals.size())) {
            dstFrame.inputLocals.push_back(0);
        }
        frameChanged |=
            merge(symbolTable, concreteOutputType, dstFrame.inputLocals, i, typeResolver);
    }

    // Exception handler: merge input locals + stack = catch type
    if (catchTypeIndex > 0) {
        for (int i = 0; i < numLocal; ++i) {
            if (i >= static_cast<int>(dstFrame.inputLocals.size())) {
                dstFrame.inputLocals.push_back(0);
            }
            frameChanged |=
                merge(symbolTable, inputLocals[i], dstFrame.inputLocals, i, typeResolver);
        }
        if (dstFrame.inputStack.empty()) {
            dstFrame.inputStack.resize(1, 0);
            frameChanged = true;
        }
        frameChanged |= merge(symbolTable, catchTypeIndex, dstFrame.inputStack, 0, typeResolver);
        return frameChanged;
    }

    // Normal successor: merge stack
    // outputStackStart/outputStackTop are 'short' and can overflow when the
    // simulation consumes many items from inputStack (stack underflow).
    // Clamp sizes to prevent:
    //   1) negative totalStackSize → huge size_t → vector::resize crash
    //   2) numInputStack overflow → out-of-bounds inputStack access
    int numInputStack  = static_cast<int>(inputStack.size()) + static_cast<int>(outputStackStart);
    int totalStackSize = numInputStack + static_cast<int>(outputStackTop);
    if (numInputStack < 0)
        numInputStack = 0;
    else if (numInputStack > static_cast<int>(inputStack.size()))
        numInputStack = static_cast<int>(inputStack.size());
    if (totalStackSize < 0) totalStackSize = 0;
    if (dstFrame.inputStack.empty()) {
        if (totalStackSize > 0) {
            dstFrame.inputStack.resize(totalStackSize, 0);
            frameChanged = true;
        }
    }
    else if (static_cast<int>(dstFrame.inputStack.size()) < totalStackSize) {
        dstFrame.inputStack.resize(totalStackSize, 0);
        frameChanged = true;
    }

    for (int i = 0; i < numInputStack; ++i) {
        if (i >= static_cast<int>(dstFrame.inputStack.size())) break;
        int concreteOutputType = inputStack[i];
        if (!initializations.empty()) {
            concreteOutputType = getInitializedType(symbolTable, concreteOutputType);
        }
        frameChanged |=
            merge(symbolTable, concreteOutputType, dstFrame.inputStack, i, typeResolver);
    }

    for (int i = 0; i < outputStackTop; ++i) {
        int idx = numInputStack + i;
        if (idx >= static_cast<int>(dstFrame.inputStack.size())) break;
        int abstractOutputType = outputStack[i];
        int concreteOutputType = getConcreteOutputType(abstractOutputType, numStack);
        if (!initializations.empty()) {
            concreteOutputType = getInitializedType(symbolTable, concreteOutputType);
        }
        frameChanged |=
            merge(symbolTable, concreteOutputType, dstFrame.inputStack, idx, typeResolver);
    }

    return frameChanged;
}

bool Frame::merge(SymbolTable& symbolTable, int sourceType, std::vector<int>& dstTypes,
                  int dstIndex, ICommonSuperClassProvider* typeResolver)
{
    if (dstIndex >= static_cast<int>(dstTypes.size())) return false;
    int dstType = dstTypes[dstIndex];
    if (dstType == sourceType) {
        return false;
    }
    int srcType = sourceType;
    if ((sourceType & ~DIM_MASK) == NULL_T) {
        if (dstType == NULL_T) {
            return false;
        }
        srcType = NULL_T;
    }
    if (dstType == 0) {
        dstTypes[dstIndex] = srcType;
        return true;
    }
    int mergedType;
    if ((dstType & DIM_MASK) != 0 || (dstType & KIND_MASK) == REFERENCE_KIND) {
        if (srcType == NULL_T) {
            return false;
        }
        else if ((srcType & (DIM_MASK | KIND_MASK)) == (dstType & (DIM_MASK | KIND_MASK))) {
            if ((dstType & KIND_MASK) == REFERENCE_KIND) {
                mergedType = (srcType & DIM_MASK) | REFERENCE_KIND |
                             symbolTable.addMergedType(
                                 srcType & VALUE_MASK, dstType & VALUE_MASK, typeResolver);
            }
            else {
                int mergedDim = (dstType & DIM_MASK) - (1 << DIM_SHIFT);
                mergedType    = mergedDim | REFERENCE_KIND |
                             symbolTable.addType(Utf8StringRef::from_cstr("java/lang/Object"));
            }
        }
        else if ((srcType & DIM_MASK) != 0 || (srcType & KIND_MASK) == REFERENCE_KIND) {
            int srcDim = srcType & DIM_MASK;
            if (srcDim != 0 && (srcType & KIND_MASK) != REFERENCE_KIND) {
                srcDim -= (1 << DIM_SHIFT);
            }
            int dstDim = dstType & DIM_MASK;
            if (dstDim != 0 && (dstType & KIND_MASK) != REFERENCE_KIND) {
                dstDim -= (1 << DIM_SHIFT);
            }
            mergedType = (srcDim < dstDim ? srcDim : dstDim) | REFERENCE_KIND |
                         symbolTable.addType(Utf8StringRef::from_cstr("java/lang/Object"));
        }
        else {
            mergedType = TOP;
        }
    }
    else if (dstType == NULL_T) {
        mergedType =
            (srcType & DIM_MASK) != 0 || (srcType & KIND_MASK) == REFERENCE_KIND ? srcType : TOP;
    }
    else {
        mergedType = TOP;
    }
    if (mergedType != dstType) {
        dstTypes[dstIndex] = mergedType;
        return true;
    }
    return false;
}

// ======================================================================
// StackMapTable output (ASM Frame.java L1366-1490)
// ======================================================================

void Frame::accept(MethodWriter& methodWriter)
{
    // Compute number of locals, ignoring TOP just after LONG/DOUBLE and trailing TOPs
    int numLocal       = 0;
    int numTrailingTop = 0;
    int i              = 0;
    while (i < static_cast<int>(inputLocals.size())) {
        int localType = inputLocals[i];
        i += (localType == LONG || localType == DOUBLE) ? 2 : 1;
        if (localType == TOP) {
            ++numTrailingTop;
        }
        else {
            numLocal += numTrailingTop + 1;
            numTrailingTop = 0;
        }
    }
    // Compute stack size, ignoring TOP just after LONG/DOUBLE
    int numStack = 0;
    i            = 0;
    while (i < static_cast<int>(inputStack.size())) {
        int stackType = inputStack[i];
        i += (stackType == LONG || stackType == DOUBLE) ? 2 : 1;
        ++numStack;
    }

    int frameIndex     = methodWriter.visitFrameStart(owner->bytecodeOffset, numLocal, numStack);
    i                  = 0;
    int remainingLocal = numLocal;
    while (remainingLocal-- > 0) {
        int localType = inputLocals[i];
        i += (localType == LONG || localType == DOUBLE) ? 2 : 1;
        methodWriter.visitAbstractType(frameIndex++, localType);
    }
    i                  = 0;
    int remainingStack = numStack;
    while (remainingStack-- > 0) {
        int stackType = inputStack[i];
        i += (stackType == LONG || stackType == DOUBLE) ? 2 : 1;
        methodWriter.visitAbstractType(frameIndex++, stackType);
    }
    methodWriter.visitFrameEnd();
}

void Frame::putAbstractType(SymbolTable& symbolTable, int abstractType, ByteVector& output)
{
    int arrayDimensions = (abstractType & DIM_MASK) >> DIM_SHIFT;
    if (arrayDimensions == 0) {
        int typeValue = abstractType & VALUE_MASK;
        switch (abstractType & KIND_MASK) {
        case CONSTANT_KIND: output.putByte(typeValue); break;
        case REFERENCE_KIND:
        {
            auto* typeSym = symbolTable.getType(typeValue);
            if (typeSym) {
                auto* classSym = symbolTable.addConstantClass(typeSym->value);
                output.putByte(ITEM_OBJECT).putShort(classSym ? classSym->index : 0);
            }
            else {
                output.putByte(ITEM_OBJECT).putShort(0);
            }
            break;
        }
        case UNINITIALIZED_KIND:
        {
            auto* typeSym = symbolTable.getType(typeValue);
            int   offset  = typeSym ? static_cast<int>(typeSym->data) : 0;
            output.putByte(ITEM_UNINITIALIZED).putShort(offset);
            break;
        }
        case FORWARD_UNINITIALIZED_KIND:
        {
            auto* label = symbolTable.getForwardUninitializedLabel(typeValue);
            output.putByte(ITEM_UNINITIALIZED);
            if (label)
                label->put(output);
            else
                output.putShort(0);
            break;
        }
        default: output.putByte(ITEM_TOP); break;
        }
    }
    else {
        // Array type: build descriptor
        std::string typeDescriptor;
        typeDescriptor.reserve(32);
        for (int d = 0; d < arrayDimensions; ++d) typeDescriptor.push_back('[');
        if ((abstractType & KIND_MASK) == REFERENCE_KIND) {
            auto* typeSym = symbolTable.getType(abstractType & VALUE_MASK);
            typeDescriptor.push_back('L');
            if (typeSym) {
                typeDescriptor.append(reinterpret_cast<const char*>(typeSym->value.data()),
                                      typeSym->value.byte_length());
            }
            typeDescriptor.push_back(';');
        }
        else {
            switch (abstractType & VALUE_MASK) {
            case ITEM_ASM_BOOLEAN: typeDescriptor.push_back('Z'); break;
            case ITEM_ASM_BYTE: typeDescriptor.push_back('B'); break;
            case ITEM_ASM_CHAR: typeDescriptor.push_back('C'); break;
            case ITEM_ASM_SHORT: typeDescriptor.push_back('S'); break;
            case ITEM_INTEGER: typeDescriptor.push_back('I'); break;
            case ITEM_FLOAT: typeDescriptor.push_back('F'); break;
            case ITEM_LONG: typeDescriptor.push_back('J'); break;
            case ITEM_DOUBLE: typeDescriptor.push_back('D'); break;
            default: typeDescriptor.push_back('Z'); break;
            }
        }
        Utf8StringRef descRef(reinterpret_cast<const ca::u8*>(typeDescriptor.data()),
                              typeDescriptor.size(),
                              typeDescriptor.size());
        auto*         classSym = symbolTable.addConstantClass(descRef);
        output.putByte(ITEM_OBJECT).putShort(classSym ? classSym->index : 0);
    }
}

// ======================================================================
// CurrentFrame (ASM CurrentFrame.java L49-55)
// ======================================================================

void CurrentFrame::execute(int opcode, int arg, Symbol* argSymbol, SymbolTable& symbolTable)
{
    Frame::execute(opcode, arg, argSymbol, symbolTable);
    Frame successor(nullptr);
    merge(symbolTable, successor, 0);
    copyFrom(successor);
}

}   // namespace jbytecode::core
