/// @file        primitives.hpp
/// @brief       Core primitive types for bytecode analysis and classfile manipulation.
///
/// This file consolidates several ASM core classes that have no complex logic
/// but are referenced by most other modules:
///
///   - AbstractType:  Internal abstract type representation for frame computation
///                    (bit-packed encoding: DIM|FLAGS|KIND|VALUE).
///   - Edge:          Control flow graph edge (jump or exception handler).
///   - Label:         Position in method bytecode (jump targets, line numbers).
///   - Handle:        Reference to a field or method (MethodHandle constant).
///   - ConstantDynamic: Runtime-computed constant with bootstrap method.
///   - Attribute:     Non-standard class/file/method/Code attribute.
///   - Symbol:        Base class for constant pool entries (internal to SymbolTable).
///   - TypePath:      Path to a type argument/wildcard bound in type annotations.
///   - Context:       Workspace for ClassReader parsing (reusable buffers).
///   - Handler:       Exception handler table entry.
///
/// Corresponding ASM originals:
///   AbstractType         ← Frame.java (internal encoding)
///   Edge                 ← Edge.java
///   Label                ← Label.java
///   Handle               ← Handle.java
///   ConstantDynamic      ← ConstantDynamic.java
///   Attribute            ← Attribute.java
///   Symbol               ← Symbol.java
///   TypePath             ← TypePath.java
///   Context              ← Context.java
///   Handler              ← Handler.java
#pragma once

#include <string>
#include <vector>
#include "libca/core/any.hpp"
#include <memory>

#include "libca/core/any.hpp"
#include <cstdint>
#include <utility>
#include <stdexcept>
#include <unordered_set>

#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/byte_vector.hpp"
#include "libca/core/datatype.hpp"
#include "libca/core/bytes.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::core {

// ---- AbstractType --------------------------------------------------------

/// AbstractType: internal representation of JVM verification types for frame
/// computation (corresponds to Frame.java's abstract type encoding in ASM).
///
/// Encoding: [DIM(6)][KIND(4)][FLAGS(2)][VALUE(20)] packed in a 32-bit int.
/// Matches ASM Frame.java: DIM_SHIFT = 26, KIND_SHIFT = 22, FLAGS_SHIFT = 20.
///
///   - DIM (bits 31-26):   array dimensions (signed, -32 to 31)
///   - KIND (bits 25-22):  CONSTANT_KIND(1), REFERENCE_KIND(2), UNINITIALIZED_KIND(3),
///                          FORWARD_UNINITIALIZED_KIND(4), LOCAL_KIND(5), STACK_KIND(6)
///   - FLAGS (bits 21-20): flags (TOP_IF_LONG_OR_DOUBLE_FLAG)
///   - VALUE (bits 19-0):  type-specific data
class AbstractType
{
public:
    static constexpr int DIM_SIZE   = 6;
    static constexpr int KIND_SIZE  = 4;
    static constexpr int FLAGS_SIZE = 2;
    // VALUE_SIZE = 32 - 6 - 4 - 2 = 20
    static constexpr int VALUE_SIZE = 32 - DIM_SIZE - KIND_SIZE - FLAGS_SIZE;

    static constexpr int DIM_SHIFT   = KIND_SIZE + FLAGS_SIZE + VALUE_SIZE;   // 26
    static constexpr int KIND_SHIFT  = FLAGS_SIZE + VALUE_SIZE;               // 22
    static constexpr int FLAGS_SHIFT = VALUE_SIZE;                            // 20

    static constexpr int VALUE_MASK = (1 << VALUE_SIZE) - 1;
    static constexpr int KIND_MASK  = ((1 << KIND_SIZE) - 1) << KIND_SHIFT;
    static constexpr int FLAGS_MASK = ((1 << FLAGS_SIZE) - 1) << FLAGS_SHIFT;
    static constexpr int DIM_MASK   = ((1 << DIM_SIZE) - 1) << DIM_SHIFT;

    static constexpr int CONSTANT_KIND              = 1 << KIND_SHIFT;
    static constexpr int REFERENCE_KIND             = 2 << KIND_SHIFT;
    static constexpr int UNINITIALIZED_KIND         = 3 << KIND_SHIFT;
    static constexpr int FORWARD_UNINITIALIZED_KIND = 4 << KIND_SHIFT;
    static constexpr int LOCAL_KIND                 = 5 << KIND_SHIFT;
    static constexpr int STACK_KIND                 = 6 << KIND_SHIFT;

    static constexpr int TOP                = CONSTANT_KIND | 0;
    static constexpr int INTEGER            = CONSTANT_KIND | 1;
    static constexpr int FLOAT              = CONSTANT_KIND | 2;
    static constexpr int DOUBLE             = CONSTANT_KIND | 3;
    static constexpr int LONG               = CONSTANT_KIND | 4;
    static constexpr int NULL_T             = CONSTANT_KIND | 5;
    static constexpr int UNINITIALIZED_THIS = CONSTANT_KIND | 6;
    static constexpr int BOOLEAN            = CONSTANT_KIND | 9;
    static constexpr int BYTE               = CONSTANT_KIND | 10;
    static constexpr int CHAR_T             = CONSTANT_KIND | 11;
    static constexpr int SHORT              = CONSTANT_KIND | 12;

    static constexpr int TOP_IF_LONG_OR_DOUBLE_FLAG = 1 << FLAGS_SHIFT;
    static constexpr int ARRAY_OF                   = 1 << DIM_SHIFT;

    /// The constant to be added to an abstract type to get one with one less array dimension.
    static constexpr int ELEMENT_OF = static_cast<int>(static_cast<unsigned>(-1) << DIM_SHIFT);

    static constexpr int getDim(int t) { return (t & DIM_MASK) >> DIM_SHIFT; }
    static constexpr int getKind(int t) { return t & KIND_MASK; }
    static constexpr int getValue(int t) { return t & VALUE_MASK; }
    static constexpr int getFlags(int t) { return t & FLAGS_MASK; }

    static constexpr bool isConstant(int t) { return getKind(t) == CONSTANT_KIND; }
    static constexpr bool isReference(int t) { return getKind(t) == REFERENCE_KIND; }
    static constexpr bool isUninitialized(int t) { return getKind(t) == UNINITIALIZED_KIND; }
    static constexpr bool isForwardUninit(int t)
    {
        return getKind(t) == FORWARD_UNINITIALIZED_KIND;
    }
    static constexpr bool isLocalRef(int t) { return getKind(t) == LOCAL_KIND; }
    static constexpr bool isStackRef(int t) { return getKind(t) == STACK_KIND; }
    static constexpr bool isForwardRef(int t) { return isLocalRef(t) || isStackRef(t); }

    static constexpr int makeConstant(int value) { return CONSTANT_KIND | (value & VALUE_MASK); }
    static constexpr int makeReference(int cpIdx) { return REFERENCE_KIND | (cpIdx & VALUE_MASK); }
    static constexpr int makeUninitialized(int offset)
    {
        return UNINITIALIZED_KIND | (offset & VALUE_MASK);
    }
    static constexpr int makeLocalRef(int idx) { return LOCAL_KIND | (idx & VALUE_MASK); }
    static constexpr int makeStackRef(int pos) { return STACK_KIND | (pos & VALUE_MASK); }

    static constexpr int addDim(int t) { return (t & ~DIM_MASK) | ((getDim(t) + 1) << DIM_SHIFT); }
    static constexpr int elementOf(int t)
    {
        if (getDim(t) == 0) return TOP;
        return (t & ~DIM_MASK) | ((getDim(t) - 1) << DIM_SHIFT);
    }

    static int getSize(int type)
    {
        if (type == LONG || type == DOUBLE) return 2;
        return 1;
    }
};

/// An edge in the control flow graph of a method. Each node of this graph is a
/// basic block, represented with the Label corresponding to its first instruction.
/// Each edge goes from one node to another, i.e. from one basic block to another
/// (the predecessor and successor blocks). An edge corresponds either to a jump
/// or ret instruction, or to an exception handler.
///
/// @see <a href="https://docs.oracle.com/javase/specs/jvms/se9/html/jvms-2.11.html">JVMS 2.11</a>
/// @author Eric Bruneton

class Label;
class Frame;

class Edge
{
public:
    /// Edge corresponding to a jump or ret instruction (COMPUTE_FRAMES).
    static constexpr int JUMP = 0;

    /// Edge corresponding to an exception handler (COMPUTE_MAXS).
    static constexpr int EXCEPTION = 0x7FFFFFFF;

    /// Info about this control flow graph edge:
    /// - JUMP for jump edges (COMPUTE_FRAMES)
    /// - catch type index for exception edges (COMPUTE_FRAMES)
    /// - stack delta or EXCEPTION for COMPUTE_MAXS
    const int info;

    /// The successor block of this control flow graph edge.
    Label* const successor;

    /// The next edge in the outgoing edge list of a basic block.
    Edge* nextEdge;

    Edge(int info, Label* successor, Edge* nextEdge)
        : info(info)
        , successor(successor)
        , nextEdge(nextEdge)
    {}
};

/// A position in the bytecode of a method. Labels are used for jump, goto, and
/// switch instructions, and for try catch blocks. A label designates the
/// <i>instruction</i> that is just after. Note however that there can be other
/// elements between a label and the instruction it designates (such as other
/// labels, stack map frames, line numbers, etc.).
///
/// @author Eric Bruneton
class Label
{
public:
    /// Flag: label is only used for debug attributes (safe to ignore in CFG).
    static constexpr short FLAG_DEBUG_ONLY = 1 << 0;

    /// Flag: label is a jump target or exception handler start.
    static constexpr short FLAG_JUMP_TARGET = 1 << 1;

    /// Flag: bytecode offset is known.
    static constexpr short FLAG_RESOLVED = 1 << 2;

    /// Flag: basic block is reachable.
    static constexpr short FLAG_REACHABLE = 1 << 3;

    /// Flag: basic block ends with a subroutine call (jsr).
    static constexpr short FLAG_SUBROUTINE_CALLER = 1 << 4;

    /// Flag: basic block is the start of a subroutine.
    static constexpr short FLAG_SUBROUTINE_START = 1 << 5;

    /// Flag: basic block is the end of a subroutine.
    static constexpr short FLAG_SUBROUTINE_END = 1 << 6;

    /// Flag: this label has at least one associated line number.
    static constexpr short FLAG_LINE_NUMBER = 1 << 7;

    // Forward reference types (matching ASM Label.java)
    static constexpr int FORWARD_REFERENCE_TYPE_MASK      = 0xF0000000;
    static constexpr int FORWARD_REFERENCE_TYPE_SHORT     = 0x10000000;
    static constexpr int FORWARD_REFERENCE_TYPE_WIDE      = 0x20000000;
    static constexpr int FORWARD_REFERENCE_TYPE_STACK_MAP = 0x30000000;
    static constexpr int FORWARD_REFERENCE_HANDLE_MASK    = 0x0FFFFFFF;

    int   bytecodeOffset  = -1;
    void* info            = nullptr;
    int   line            = 0;
    short flags           = 0;

    /// 归属锚(B8)：创建方盖章的 owner 句柄——统一取「所属内容 arena 的地址」
    /// （MethodNode::nodeArena_ 等，P2 归属锚同源）。非 null 即 arena 所有，
    /// visitor 边界 canonicalize 直返；null 为外来堆 Label（reader/调用方自建），
    /// 入方法时拷贝收编。裸指针，不引 garena 依赖。
    const void* owner_ = nullptr;
    short inputStackSize  = -1;
    short outputStackSize = -1;
    short outputStackMax  = -1;
    short subroutineId    = 0;

    /// ASM-style forward references: [lastIdx, srcOff, type|handle, srcOff, type|handle, ...]
    std::vector<int> forwardReferences;

    Edge*  outgoingEdges   = nullptr;
    Label* nextListElement = nullptr;
    Label* nextBasicBlock  = nullptr;
    Frame* frame           = nullptr;

    // ---- ASM Label API ----

    /// Register a forward reference (matching ASM Label.addForwardReference).
    void addForwardRef(int sourceInsnOffset, int refType, int handle);

    ///  Write a jump/switch reference — if resolved, compute offset directly; else write -1
    /// placeholder + register forward ref.
    void put(ByteVector& code, int sourceInsnOffset, bool wide);

    /// Write this label offset into the StackMapTable, with forward resolution if needed.
    void put(ByteVector& stackMapTableEntries);

    ///  Called when label is visited: resolve all forward refs, patch bytecode and StackMapTable.
    /// Returns true if any ASM-specific instruction was introduced.
    bool resolve(ca::u8* codeBytes, ByteVector* stackMapTableEntries, int bytecodeOffsetArg);
    bool resolve(ca::u8* codeBytes, int bytecodeOffsetArg)
    {
        return resolve(codeBytes, nullptr, bytecodeOffsetArg);
    }

    // ---- existing helpers ----

    bool isResolved() const { return (flags & FLAG_RESOLVED) != 0; }

    Label* getCanonicalInstance() { return frame == nullptr ? this : frame->owner; }

    int getOffset() const
    {
        if (!isResolved()) {
            throw std::logic_error("Label offset position has not been resolved yet");
        }
        return bytecodeOffset;
    }

    std::string toString() const
    {
        return "L" + std::to_string(reinterpret_cast<std::uintptr_t>(this));
    }

    bool isDebugOnly() const { return (flags & FLAG_DEBUG_ONLY) != 0; }
    void setDebugOnly(bool v)
    {
        flags = v ? (flags | FLAG_DEBUG_ONLY) : (flags & ~FLAG_DEBUG_ONLY);
    }

    bool isJumpTarget() const { return (flags & FLAG_JUMP_TARGET) != 0; }
    void setJumpTarget(bool v)
    {
        flags = v ? (flags | FLAG_JUMP_TARGET) : (flags & ~FLAG_JUMP_TARGET);
    }

    bool isReachable() const { return (flags & FLAG_REACHABLE) != 0; }
    void setReachable(bool v) { flags = v ? (flags | FLAG_REACHABLE) : (flags & ~FLAG_REACHABLE); }

    bool isSubroutineCaller() const { return (flags & FLAG_SUBROUTINE_CALLER) != 0; }
    void setSubroutineCaller(bool v)
    {
        flags = v ? (flags | FLAG_SUBROUTINE_CALLER) : (flags & ~FLAG_SUBROUTINE_CALLER);
    }

    bool isSubroutineStart() const { return (flags & FLAG_SUBROUTINE_START) != 0; }
    void setSubroutineStart(bool v)
    {
        flags = v ? (flags | FLAG_SUBROUTINE_START) : (flags & ~FLAG_SUBROUTINE_START);
    }

    bool isSubroutineEnd() const { return (flags & FLAG_SUBROUTINE_END) != 0; }
    void setSubroutineEnd(bool v)
    {
        flags = v ? (flags | FLAG_SUBROUTINE_END) : (flags & ~FLAG_SUBROUTINE_END);
    }

    bool isSubroutine() const
    {
        return (flags & (FLAG_SUBROUTINE_CALLER | FLAG_SUBROUTINE_START | FLAG_SUBROUTINE_END)) !=
               0;
    }


    /// 归属锚访问(B8)：null = 外来堆 Label（reader/调用方自建），待方法边界收编。
    const void* owner() const { return owner_; }
    void        setOwner(const void* owner) { owner_ = owner; }

    int getOutgoingEdgeCount() const
    {
        int count = 0;
        for (Edge* e = outgoingEdges; e != nullptr; e = e->nextEdge) count++;
        return count;
    }

    void markSubroutine(short subroutineId);
    void addSubroutineRetSuccessors(Label* subroutineCaller);

private:
    void pushSuccessors(std::vector<Label*>&        blocksToProcess,
                        std::unordered_set<Label*>& queuedOrProcessed);
};

/// A reference to a field or a method (corresponds to CONSTANT_MethodHandle_info).
///
/// @author Remi Forax
/// @author Eric Bruneton
/// @see <a
/// href="https://docs.oracle.com/javase/specs/jvms/se9/html/jvms-4.html#jvms-4.4.8">JVMS 4.4.8</a>
class Handle
{
public:
    /// @deprecated Use {@link #Handle(int, String, String, String, boolean)} instead.
    /// isInterface is auto-detected from tag (true iff tag == H_INVOKEINTERFACE).
    Handle(int tag, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
           ca::str::Utf8StringRef descriptor)
        : Handle(tag, owner, name, descriptor, tag == Opcodes::H_INVOKEINTERFACE)
    {}

    Handle(int tag, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
           ca::str::Utf8StringRef descriptor, bool isInterface)
        : tag_(tag)
        , owner_(owner)
        , name_(name)
        , descriptor_(descriptor)
        , isInterface_(isInterface)
    {}

    int                    getTag() const { return tag_; }
    ca::str::Utf8StringRef getOwner() const { return owner_; }
    ca::str::Utf8StringRef getName() const { return name_; }
    ca::str::Utf8StringRef getDesc() const { return descriptor_; }
    bool                   isInterface() const { return isInterface_; }

    bool operator==(const Handle& other) const
    {
        return tag_ == other.tag_ && isInterface_ == other.isInterface_ && owner_ == other.owner_ &&
               name_ == other.name_ && descriptor_ == other.descriptor_;
    }
    bool operator!=(const Handle& other) const { return !(*this == other); }

    size_t hashCode() const
    {
        // Matches ASM Handle.hashCode():
        //   tag + (isInterface ? 64 : 0) + owner.hashCode() * name.hashCode() *
        //   descriptor.hashCode()
        // where String.hashCode() = s[0]*31^(n-1) + ... + s[n-1]
        auto strHash = [](ca::str::Utf8StringRef s) -> size_t {
            size_t h = 0;
            for (ca::usize i = 0; i < s.byte_length(); i++) h = h * 31 + s.byte_at(i);
            return h;
        };
        return static_cast<size_t>(tag_) + (isInterface_ ? static_cast<size_t>(64) : 0) +
               strHash(owner_) * strHash(name_) * strHash(descriptor_);
    }

    std::string toString() const
    {
        std::string s;
        s.append(reinterpret_cast<const char*>(owner_.data()), owner_.byte_length());
        s += '.';
        s.append(reinterpret_cast<const char*>(name_.data()), name_.byte_length());
        s.append(reinterpret_cast<const char*>(descriptor_.data()), descriptor_.byte_length());
        s += " (" + std::to_string(tag_) + (isInterface_ ? " itf)" : ")");
        return s;
    }

private:
    int                    tag_;
    ca::str::Utf8StringRef owner_;
    ca::str::Utf8StringRef name_;
    ca::str::Utf8StringRef descriptor_;
    bool                   isInterface_;
};

/// A constant whose value is computed at runtime, with a bootstrap method.
/// Corresponds to CONSTANT_Dynamic_info in the constant pool.
///
/// @author Remi Forax
class ConstantDynamic
{
public:
    ConstantDynamic(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                    Handle bootstrapMethod, std::vector<ca::core::Any> bootstrapMethodArguments)
        : name_(name)
        , descriptor_(descriptor)
        , bootstrapMethod_(std::move(bootstrapMethod))
        , bootstrapMethodArguments_(std::move(bootstrapMethodArguments))
    {}

    ca::str::Utf8StringRef            getName() const { return name_; }
    ca::str::Utf8StringRef            getDescriptor() const { return descriptor_; }
    const Handle&                     getBootstrapMethod() const { return bootstrapMethod_; }
    const std::vector<ca::core::Any>& getBootstrapMethodArguments() const
    {
        return bootstrapMethodArguments_;
    }

private:
    ca::str::Utf8StringRef     name_;
    ca::str::Utf8StringRef     descriptor_;
    Handle                     bootstrapMethod_;
    std::vector<ca::core::Any> bootstrapMethodArguments_;
};

/// A non-standard class, field, method or Code attribute, as defined in the
/// Java Virtual Machine Specification (JVMS). Attribute instances can be chained
/// via nextAttribute to represent a list of class/file/method/Code attributes.
///
/// @author Eric Bruneton
/// @see <a
/// href="https://docs.oracle.com/javase/specs/jvms/se9/html/jvms-4.html#jvms-4.7">JVMS 4.7</a>
class ByteVector;
class ClassReader;
class ClassWriter;

class Attribute
{
public:
    /// The type of this attribute (its name in the JVMS).
    ca::str::Utf8String type;

    /// The raw content of this attribute (opaque bytes for unknown attributes).
    ca::core::Bytes value;

    /// The next attribute in this attribute list. May be nullptr.
    Attribute* nextAttribute = nullptr;

    Attribute() = default;

    /// Construct with type only (used by custom attribute prototypes).
    explicit Attribute(ca::str::Utf8StringRef type)
        : type(type.data(), type.byte_length())
    {}

    Attribute(ca::str::Utf8StringRef type, ca::core::Bytes value)
        : type(type.data(), type.byte_length())
        , value(std::move(value))
    {}

    Attribute(const Attribute& other)
        : type(other.type.clone())
        , value(other.value)
        , nextAttribute(other.nextAttribute)
    {}

    Attribute& operator=(const Attribute& other)
    {
        if (this != &other) {
            type          = other.type.clone();
            value         = other.value;
            nextAttribute = other.nextAttribute;
        }
        return *this;
    }

    Attribute(Attribute&& other) noexcept            = default;
    Attribute& operator=(Attribute&& other) noexcept = default;

    virtual ~Attribute() = default;

    /// Returns true if this type of attribute is unknown (opaque byte array).
    virtual bool isUnknown() const { return true; }

    /// Returns true if this type of attribute is a Code attribute.
    virtual bool isCodeAttribute() const { return false; }

    virtual std::vector<Label*> getLabels() const { return {}; }

    // ===== read/write 机制 =====

    /// Virtual factory: reads from class file and returns a new Attribute.
    /// Default implementation reads raw bytes as opaque data.
    ///
    /// @param classReader  the class file reader.
    /// @param offset       offset of attribute content (after 6-byte header).
    /// @param length       length of attribute content.
    /// @param charBuffer   buffer for reading CP strings.
    /// @param codeAttributeOffset  offset of enclosing Code attribute, or -1.
    /// @param labels       labels of the method's code, or nullptr.
    /// @return a new Attribute (ownership transferred via unique_ptr).
    virtual std::unique_ptr<Attribute> read(ClassReader& classReader, int offset, int length,
                                            std::vector<char>& charBuffer, int codeAttributeOffset,
                                            Label** labels);

    /// Virtual writer: returns the serialized content (without 6-byte header).
    /// Default implementation returns value wrapped in ByteVector.
    ///
    /// @param classWriter  the class writer.
    /// @param code         the method bytecode, or empty if non-Code attribute.
    /// @param codeLength   length of bytecode, or 0.
    /// @param maxStack     max stack, or -1.
    /// @param maxLocals    max locals, or -1.
    /// @return the serialized attribute content.
    virtual ByteVector write(ClassWriter& classWriter, const std::vector<ca::u8>& code,
                             int codeLength, int maxStack, int maxLocals);

    static ByteVector write(Attribute& attribute, ClassWriter& classWriter,
                            const std::vector<ca::u8>& code, int codeLength, int maxStack,
                            int maxLocals);

    int computeAttributesSize(ClassWriter& classWriter, const std::vector<ca::u8>& code,
                              int codeLength, int maxStack, int maxLocals);

    void putAttributes(ClassWriter& classWriter, ByteVector& output);

    /// Static dispatcher: calls prototype.read() via virtual dispatch.
    /// Used by ClassReader to parse attributes matching a prototype by type name.
    static std::unique_ptr<Attribute> read(Attribute& prototype, ClassReader& classReader,
                                           int offset, int length, std::vector<char>& charBuffer,
                                           int codeAttributeOffset, Label** labels)
    {
        return prototype.read(classReader, offset, length, charBuffer, codeAttributeOffset, labels);
    }
};

/// Symbol (constant pool entries) — defined in symbol_table.hpp.
/// Symbol is no longer defined here to avoid duplicate definition
/// when both primitives.hpp and symbol_table.hpp are included
/// (e.g. via writers.hpp).

/// The path to a type argument, wildcard bound, array element type, or static
/// inner type within an enclosing type. Used for type annotations.
///
/// @author Eric Bruneton
/// @see <a
/// href="https://docs.oracle.com/javase/specs/jvms/se9/html/jvms-4.html#jvms-4.7.20.2">JVMS 4.7.20.2</a>
class TypePath
{
public:
    static constexpr int ARRAY_ELEMENT  = 0;
    static constexpr int INNER_TYPE     = 1;
    static constexpr int WILDCARD_BOUND = 2;
    static constexpr int TYPE_ARGUMENT  = 3;

    TypePath() = default;

    int getLength() const;

    int getStep(int index) const;

    int getStepArgument(int index) const;

    std::string toString() const;

    static TypePath fromString(const ca::str::Utf8StringRef& typePath);
    static TypePath fromBytes(const ByteVector& bytes);

    static void put(const TypePath* typePath, ByteVector& output);

private:
    std::vector<uint8_t> data_;
    friend class ClassReader;
};

/// Information about a class being visited by ClassReader. This is essentially
/// a workspace / scratchpad that holds reusable buffers (charBuffer for string
/// decoding, label arrays, frame analysis state) to minimize allocation.
///
/// @author Eric Bruneton
struct Context
{
    Attribute*        attributePrototypes = nullptr;
    int               parsingOptions      = 0;
    std::vector<char> charBuffer;

    int                    currentMethodAccessFlags = 0;
    ca::str::Utf8StringRef currentMethodName;
    ca::str::Utf8StringRef currentMethodDescriptor;
    std::vector<Label*>    currentMethodLabels;

    int                 currentTypeAnnotationTarget = 0;
    TypePath            currentTypeAnnotationTargetPath;
    std::vector<Label*> currentLocalVariableAnnotationRangeStarts;
    std::vector<Label*> currentLocalVariableAnnotationRangeEnds;
    std::vector<int>    currentLocalVariableAnnotationRangeIndices;

    int                        currentFrameOffset          = 0;
    int                        currentFrameType            = 0;
    int                        currentFrameLocalCount      = 0;
    int                        currentFrameLocalCountDelta = 0;
    std::vector<ca::core::Any> currentFrameLocalTypes;
    int                        currentFrameStackCount = 0;
    std::vector<ca::core::Any> currentFrameStackTypes;
};


/// Information about an exception handler. Corresponds to an element of the
/// exception_table of a Code attribute. Handler instances can be chained via
/// nextHandler to describe a full JVMS exception_table array.
///
/// @author Eric Bruneton
/// @see <a
/// href="https://docs.oracle.com/javase/specs/jvms/se9/html/jvms-4.html#jvms-4.7.3">JVMS 4.7.3</a>
class Handler
{
public:
    /// start_pc: beginning of scope (inclusive).
    Label* startPc;
    /// end_pc: end of scope (exclusive).
    Label* endPc;
    /// handler_pc: beginning of handler code.
    Label* handlerPc;
    /// catch_type: CP index of exception type, or 0 for any.
    int catchType;
    /// catch_type_descriptor: internal name of exception type, empty for any.
    ca::str::Utf8String catchTypeDescriptor;
    /// Next exception handler in the chain.
    Handler* nextHandler;

    Handler(Label* startPc, Label* endPc, Label* handlerPc, int catchType,
            ca::str::Utf8StringRef catchTypeDescriptor)
        : startPc(startPc)
        , endPc(endPc)
        , handlerPc(handlerPc)
        , catchType(catchType)
        , catchTypeDescriptor(catchTypeDescriptor.data() != nullptr
                                  ? ca::str::Utf8String(catchTypeDescriptor.data(),
                                                        catchTypeDescriptor.byte_length())
                                  : ca::str::Utf8String())
        , nextHandler(nullptr)
    {}

    Handler(Label* startPc, Label* endPc, Label* handlerPc, int catchType,
            const char* catchTypeDescriptor)
        : Handler(startPc, endPc, handlerPc, catchType,
                  catchTypeDescriptor != nullptr
                      ? ca::str::Utf8StringRef::from_cstr(catchTypeDescriptor)
                      : ca::str::Utf8StringRef())
    {}

    Handler(const Handler* other, Label* newStartPc, Label* newEndPc)
        : startPc(newStartPc)
        , endPc(newEndPc)
        , handlerPc(other->handlerPc)
        , catchType(other->catchType)
        , catchTypeDescriptor(other->catchTypeDescriptor.clone())
        , nextHandler(other->nextHandler)
    {}

    static Handler* removeRange(Handler* firstHandler, Label* start, Label* end);

    static int getExceptionTableLength(const Handler* firstHandler);

    static int getExceptionTableSize(const Handler* firstHandler);

    static void putExceptionTable(const Handler* firstHandler, ByteVector& output);
};


}   // namespace jbytecode::core
