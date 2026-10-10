/// @file        frame.hpp
/// @brief       Stack map frame computation engine.
///
/// Corresponds to ASM's Frame.java, CurrentFrame.java, and the
/// ICommonSuperClassProvider interface (C++ adaptation).
///
/// In ASM, Frame.java contains:
///   - ITEM_* constants (used by Opcodes.java as Frame.ITEM_*)
///   - AbstractType bit encoding (DIM/KIND/FLAGS/VALUE)
///   - StackMapTable frame type constants
///   - Frame class (input/output frame, execute, merge)
///   - CurrentFrame class
///
/// C++ adaptation splits AbstractType into primitives.hpp (so it can be
/// used independently of the full Frame class).
///
/// @author Eric Bruneton
#pragma once

#include "libca/core/datatype.hpp"
#include "libca/str/utf8_string.hpp"
#include <string>
#include <vector>
#include "libca/core/any.hpp"

namespace jbytecode::core {

class Label;
class SymbolTable;
class Symbol;
class MethodWriter;
class ByteVector;

// ---- StackMapTable frame type constants (from Frame.java) ----

inline constexpr int SAME_FRAME                              = 0;
inline constexpr int SAME_LOCALS_1_STACK_ITEM_FRAME          = 64;
inline constexpr int RESERVED                                = 128;
inline constexpr int SAME_LOCALS_1_STACK_ITEM_FRAME_EXTENDED = 247;
inline constexpr int CHOP_FRAME                              = 248;
inline constexpr int SAME_FRAME_EXTENDED                     = 251;
inline constexpr int APPEND_FRAME                            = 252;
inline constexpr int FULL_FRAME                              = 255;

// ---- Type resolver interface (C++ adaptation) ----

class ICommonSuperClassProvider
{
public:
    virtual ~ICommonSuperClassProvider() = default;
    virtual ca::str::Utf8StringRef getCommonSuperClass(const ca::str::Utf8StringRef& type1,
                                                       const ca::str::Utf8StringRef& type2) = 0;
};

class DefaultCommonSuperClassProvider : public ICommonSuperClassProvider
{
public:
    ca::str::Utf8StringRef getCommonSuperClass(const ca::str::Utf8StringRef& type1,
                                               const ca::str::Utf8StringRef& type2) override;
};

extern DefaultCommonSuperClassProvider defaultProvider;

// ---- Frame (from Frame.java) ----
// ASM reference: ref_samples/asm/asm/src/main/java/org/objectweb/asm/Frame.java

class Frame
{
public:
    // ---- Constants used in the StackMapTable attribute ----
    static constexpr ca::i32 ITEM_TOP                = 0;
    static constexpr ca::i32 ITEM_INTEGER            = 1;
    static constexpr ca::i32 ITEM_FLOAT              = 2;
    static constexpr ca::i32 ITEM_DOUBLE             = 3;
    static constexpr ca::i32 ITEM_LONG               = 4;
    static constexpr ca::i32 ITEM_NULL               = 5;
    static constexpr ca::i32 ITEM_UNINITIALIZED_THIS = 6;
    static constexpr ca::i32 ITEM_OBJECT             = 7;
    static constexpr ca::i32 ITEM_UNINITIALIZED      = 8;
    static constexpr ca::i32 ITEM_ASM_BOOLEAN        = 9;
    static constexpr ca::i32 ITEM_ASM_BYTE           = 10;
    static constexpr ca::i32 ITEM_ASM_CHAR           = 11;
    static constexpr ca::i32 ITEM_ASM_SHORT          = 12;

    // ---- ASM AbstractType bit encoding constants (from Frame.java L137-190) ----
    static constexpr int DIM_SIZE   = 6;
    static constexpr int KIND_SIZE  = 4;
    static constexpr int FLAGS_SIZE = 2;
    static constexpr int VALUE_SIZE = 32 - DIM_SIZE - KIND_SIZE - FLAGS_SIZE;

    static constexpr int DIM_SHIFT   = KIND_SIZE + FLAGS_SIZE + VALUE_SIZE;
    static constexpr int KIND_SHIFT  = FLAGS_SIZE + VALUE_SIZE;
    static constexpr int FLAGS_SHIFT = VALUE_SIZE;

    static constexpr int DIM_MASK   = ((1 << DIM_SIZE) - 1) << DIM_SHIFT;
    static constexpr int KIND_MASK  = ((1 << KIND_SIZE) - 1) << KIND_SHIFT;
    static constexpr int VALUE_MASK = (1 << VALUE_SIZE) - 1;
    static constexpr int FLAGS_MASK = ((1 << FLAGS_SIZE) - 1) << FLAGS_SHIFT;

    static constexpr int CONSTANT_KIND              = 1 << KIND_SHIFT;
    static constexpr int REFERENCE_KIND             = 2 << KIND_SHIFT;
    static constexpr int UNINITIALIZED_KIND         = 3 << KIND_SHIFT;
    static constexpr int FORWARD_UNINITIALIZED_KIND = 4 << KIND_SHIFT;
    static constexpr int LOCAL_KIND                 = 5 << KIND_SHIFT;
    static constexpr int STACK_KIND                 = 6 << KIND_SHIFT;

    static constexpr int TOP                = CONSTANT_KIND | ITEM_TOP;
    static constexpr int BOOLEAN            = CONSTANT_KIND | ITEM_ASM_BOOLEAN;
    static constexpr int BYTE               = CONSTANT_KIND | ITEM_ASM_BYTE;
    static constexpr int CHAR               = CONSTANT_KIND | ITEM_ASM_CHAR;
    static constexpr int SHORT              = CONSTANT_KIND | ITEM_ASM_SHORT;
    static constexpr int INTEGER            = CONSTANT_KIND | ITEM_INTEGER;
    static constexpr int FLOAT              = CONSTANT_KIND | ITEM_FLOAT;
    static constexpr int LONG               = CONSTANT_KIND | ITEM_LONG;
    static constexpr int DOUBLE             = CONSTANT_KIND | ITEM_DOUBLE;
    static constexpr int NULL_T             = CONSTANT_KIND | ITEM_NULL;
    static constexpr int UNINITIALIZED_THIS = CONSTANT_KIND | ITEM_UNINITIALIZED_THIS;

    static constexpr int TOP_IF_LONG_OR_DOUBLE_FLAG = 1 << FLAGS_SHIFT;
    static constexpr int ARRAY_OF                   = 1 << DIM_SHIFT;
    static constexpr int ELEMENT_OF = static_cast<int>(static_cast<unsigned>(-1) << DIM_SHIFT);

    // ---- Instance fields (matching ASM Frame.java L196-235) ----
    Label*           owner = nullptr;
    std::vector<int> inputLocals;
    std::vector<int> inputStack;
    std::vector<int> outputLocals;
    std::vector<int> outputStack;
    short            outputStackStart    = 0;
    short            outputStackTop      = 0;
    int              initializationCount = 0;
    std::vector<int> initializations;

    // ---- Constructor ----
    Frame() = default;
    explicit Frame(Label* owner);

    // ---- Copy (ASM Frame.copyFrom, L258-267) ----
    void copyFrom(const Frame& source);

    // ---- Input frame initialization (ASM L401-473) ----
    void setInputFrameFromDescriptor(SymbolTable& symbolTable, int access,
                                     const ca::str::Utf8StringRef& descriptor, int maxLocals);
    void setInputFrameFromApiFormat(SymbolTable& symbolTable, int numLocal,
                                    const std::vector<ca::core::Any>& local, int numStack,
                                    const std::vector<ca::core::Any>& stack);

    // ---- Instruction simulation (ASM L692-1120) ----
    // Signature matches ASM: (opcode, arg, Symbol, SymbolTable)
    virtual void execute(int opcode, int arg, Symbol* argSymbol, SymbolTable& symbolTable);

    // ---- Frame merging (ASM L1174-1352) ----
    bool merge(SymbolTable& symbolTable, Frame& dstFrame, int catchTypeIndex,
               ICommonSuperClassProvider* typeResolver = nullptr);

    // ---- StackMapTable output (ASM L1366-1490) ----
    void        accept(MethodWriter& methodWriter);
    static void putAbstractType(SymbolTable& symbolTable, int abstractType, ByteVector& output);

    int getInputStackSize() const { return static_cast<int>(inputStack.size()); }

    // ---- Static converters (ASM L284-385) ----
    static int getAbstractTypeFromApiFormat(SymbolTable& symbolTable, const ca::core::Any& type);
    static int getAbstractTypeFromInternalName(SymbolTable&                  symbolTable,
                                               const ca::str::Utf8StringRef& internalName);

private:
    // ---- Output frame helpers (ASM L489-615) ----
    int  getLocal(int localIndex);
    void setLocal(int localIndex, int abstractType);
    void push(int abstractType);
    void push(SymbolTable& symbolTable, const ca::str::Utf8StringRef& descriptor);
    int  pop();
    void pop(int elements);
    void pop(const ca::str::Utf8StringRef& descriptor);

    // ---- Uninitialized type handling (ASM L627-678) ----
    void addInitializedType(int abstractType);
    int  getInitializedType(SymbolTable& symbolTable, int abstractType);

    // ---- Concrete type resolution (ASM L1134-1160) ----
    int getConcreteOutputType(int abstractOutputType, int numStack);

    // ---- Descriptor parsing (ASM L321-385) ----
    static int getAbstractTypeFromDescriptor(SymbolTable&                  symbolTable,
                                             const ca::str::Utf8StringRef& buffer, int offset);

    // ---- Single type merge (ASM L1276-1352) ----
    static bool merge(SymbolTable& symbolTable, int sourceType, std::vector<int>& dstTypes,
                      int dstIndex, ICommonSuperClassProvider* typeResolver = nullptr);
};

// ---- CurrentFrame (from CurrentFrame.java) ----

class CurrentFrame : public Frame
{
public:
    explicit CurrentFrame(Label* owner)
        : Frame(owner)
    {}
    void execute(int opcode, int arg, Symbol* argSymbol, SymbolTable& symbolTable) override;
};

}   // namespace jbytecode::core
