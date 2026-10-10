#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/core/constants.hpp"
#include "jbytecode/core/exceptions.hpp"
#include "jbytecode/core/frame.hpp"
#include "jbytecode/core/symbol_table.hpp"
#include "jbytecode/core/bounded_common_super_class_provider.hpp"
#include "libca/log/log_macros.hpp"
#include "libca/str/utf8_string.hpp"
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_set>

using namespace ca::str;
using namespace ca::core;

namespace jbytecode::core {

namespace {

Utf8StringRef constantClassName(const Type& type, ca::str::Utf8StringArena& arena)
{
    if (type.getSort() == Type::ARRAY || type.getSort() == Type::OBJECT ||
        type.getSort() == Type::INTERNAL) {
        // #921 staticArena 退役：类名 intern 落点必选注入（SymbolTable 的作业 arena）。
        return type.getInternalName(arena);
    }
    return type.getDescriptor(arena);
}

// Compute argumentsAndReturnSizes from a method descriptor.
// Return value: (argumentSlots << 2) | returnSize
int argsAndReturnSize(const ca::str::Utf8StringRef& desc)
{
    int       args = 1;   // skip '('
    ca::usize i    = 1;
    while (i < desc.byte_length() && desc.byte_at(i) != ')') {
        if (desc.byte_at(i) == 'J' || desc.byte_at(i) == 'D') {
            args += 2;
            i++;
        }
        else if (desc.byte_at(i) == 'L') {
            args++;
            while (i < desc.byte_length() && desc.byte_at(i) != ';') i++;
            if (i < desc.byte_length()) i++;
        }
        else if (desc.byte_at(i) == '[') {
            args++;
            while (i < desc.byte_length() && desc.byte_at(i) == '[') i++;
            if (i < desc.byte_length() && desc.byte_at(i) == 'L') {
                while (i < desc.byte_length() && desc.byte_at(i) != ';') i++;
                if (i < desc.byte_length()) i++;
            }
            else if (i < desc.byte_length())
                i++;
        }
        else {
            args++;
            i++;
        }
    }
    i++;   // ')'
    int retSize = 0;
    if (i < desc.byte_length()) {
        if (desc.byte_at(i) == 'V')
            retSize = 0;
        else if (desc.byte_at(i) == 'J' || desc.byte_at(i) == 'D')
            retSize = 2;
        else
            retSize = 1;
    }
    return (args << 2) | retSize;
}

// 下标必须与 JVM opcode 一一对应。表项缺失会让后续所有指令的栈增减整体错位。
constexpr int NA                 = 0;
constexpr int STACK_SIZE_DELTA[] = {
    0,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  1,  1,  1,  2,  2,  1,  1,  1,  NA, NA, 1,  2,
    1,  2,  1,  NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    -1, 0,  -1, 0,  -1, -1, -1, -1, -1, -2, -1, -2, -1, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, -3, -4, -3, -4, -3, -3, -3, -3, -1, -2, 1,  1,  1,
    2,  2,  2,  0,  -1, -2, -1, -2, -1, -2, -1, -2, -1, -2, -1, -2, -1, -2, -1, -2, -1, -2, -1,
    -2, 0,  0,  0,  0,  -1, -1, -1, -1, -1, -1, -1, -2, -1, -2, -1, -2, 0,  1,  0,  1,  -1, -1,
    0,  0,  1,  1,  -1, 0,  -1, 0,  0,  0,  -3, -1, -1, -3, -3, -1, -1, -1, -1, -1, -1, -2, -2,
    -2, -2, -2, -2, -2, -2, 0,  1,  0,  -1, -1, -1, -2, -1, -2, -1, 0,  NA, NA, NA, NA, NA, NA,
    NA, NA, NA, 1,  0,  0,  0,  NA, 0,  0,  -1, -1, NA, NA, -1, -1, NA, NA};
static_assert(sizeof(STACK_SIZE_DELTA) / sizeof(STACK_SIZE_DELTA[0]) == 202);
static_assert(STACK_SIZE_DELTA[Opcodes::IASTORE] == -3);
static_assert(STACK_SIZE_DELTA[Opcodes::DUP] == 1);
static_assert(STACK_SIZE_DELTA[Opcodes::IFNULL] == -1);

void requireStackSizeDeltaOpcode(int opcode)
{
    constexpr int opcodeCount = sizeof(STACK_SIZE_DELTA) / sizeof(STACK_SIZE_DELTA[0]);
    if (opcode < 0 || opcode >= opcodeCount) {
        throw std::invalid_argument("opcode 超出 JVM 指令表范围: " + std::to_string(opcode));
    }
}

void putVerificationTypeInfo(SymbolTable& symbols, ByteVector& output, const ca::core::Any& type)
{
    if (auto* p = type.as<int>()) {
        output.putByte(*p);
    }
    else if (auto* p = type.as<ca::str::Utf8StringRef>()) {
        auto* sym = symbols.addConstantClass(*p);
        output.putByte(Frame::ITEM_OBJECT).putShort(sym ? sym->index : 0);
    }
    else if (auto* p = type.as<Label*>()) {
        output.putByte(Frame::ITEM_UNINITIALIZED);
        (*p)->put(output);
    }
    else {
        output.putByte(Frame::ITEM_TOP);
    }
}

}   // namespace

// ======================================================================
// Constructor (ASM MethodWriter L589-628)
// ======================================================================

MethodWriter::MethodWriter(SymbolTable& symbols, ClassWriter& classWriter, ca::i32 access,
                           ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                           ca::str::Utf8StringRef                     signature,
                           const std::vector<ca::str::Utf8StringRef>& exceptions, int compute,
                           ICommonSuperClassProvider* typeResolver)
    : MethodVisitor(Opcodes::ASM9)
    , symbols_(symbols)
    , classWriter_(classWriter)
    , typeResolver_(typeResolver)
    , compute_(compute)
    , access_(name.byte_length() >= 6 && name.byte_at(0) == '<' && name.byte_at(1) == 'i' &&
                      name.byte_at(2) == 'n' && name.byte_at(3) == 'i' && name.byte_at(4) == 't' &&
                      name.byte_at(5) == '>'
                  ? access | Constants::ACC_CONSTRUCTOR
                  : access)
    , nameIndex_(symbols.addConstantUtf8(name))
    , descIndex_(symbols.addConstantUtf8(descriptor))
    , descStr_(descriptor)
{
    nameStr_ = name;
    signatureIndex_ =
        (signature.data() && signature.byte_length() > 0) ? symbols.addConstantUtf8(signature) : 0;

    if (!exceptions.empty()) {
        exceptionIndices_.reserve(exceptions.size());
        for (auto& ex : exceptions) {
            auto* sym = symbols.addConstantClass(ex);
            exceptionIndices_.push_back(sym ? sym->index : 0);
        }
    }

    if (compute_ != COMPUTE_NOTHING) {
        int argumentsSize = argsAndReturnSize(descriptor) >> 2;
        if ((access & Opcodes::ACC_STATIC) != 0) --argumentsSize;
        maxLocals_       = argumentsSize;
        currentLocals_   = argumentsSize;
        firstBasicBlock_ = new Label();
        visitLabel(firstBasicBlock_);
    }
}

// ======================================================================
// Annotation visitors (ASM L651-713)
// ======================================================================

void MethodWriter::visitParameter(ca::str::Utf8StringRef name, int access)
{
    parameters_.putShort(name.data() ? symbols_.addConstantUtf8(name) : 0).putShort(access);
    ++parametersCount_;
}

AnnotationVisitor* MethodWriter::visitAnnotationDefault()
{
    annotationDefault_ = new AnnotationWriter(symbols_, ca::str::Utf8StringRef(), false);
    ownedAnns_.push_back(annotationDefault_);
    return annotationDefault_;
}

AnnotationVisitor* MethodWriter::visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible)
{
    auto* ann = new AnnotationWriter(symbols_, descriptor);
    ownedAnns_.push_back(ann);
    if (visible) {
        visibleAnnotations_.push_back(ann);
        lastRuntimeVisibleAnnotation_ = ann;
    }
    else {
        invisibleAnnotations_.push_back(ann);
        lastRuntimeInvisibleAnnotation_ = ann;
    }
    return ann;
}

AnnotationVisitor* MethodWriter::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                     ca::str::Utf8StringRef descriptor,
                                                     bool                   visible)
{
    auto* ann = new AnnotationWriter(symbols_, descriptor);
    ann->setTypeAnnotation(typeRef, typePath);
    ownedAnns_.push_back(ann);
    if (visible) {
        visibleTypeAnnotations_.push_back(ann);
        lastRuntimeVisibleTypeAnnotation_ = ann;
    }
    else {
        invisibleTypeAnnotations_.push_back(ann);
        lastRuntimeInvisibleTypeAnnotation_ = ann;
    }
    return ann;
}

AnnotationVisitor* MethodWriter::visitParameterAnnotation(int                    parameter,
                                                          ca::str::Utf8StringRef descriptor,
                                                          bool                   visible)
{
    auto* ann = new AnnotationWriter(symbols_, descriptor);
    ownedAnns_.push_back(ann);
    if (visible) {
        visibleParameterAnnotations_.emplace_back(parameter, ann);
    }
    else {
        invisibleParameterAnnotations_.emplace_back(parameter, ann);
    }
    return ann;
}

AnnotationVisitor* MethodWriter::visitLocalVariableAnnotation(int typeRef, const TypePath* typePath,
                                                              const std::vector<Label*>& start,
                                                              const std::vector<Label*>& end,
                                                              const std::vector<int>&    index,
                                                              ca::str::Utf8StringRef descriptor,
                                                              bool                   visible)
{
    // 此前未覆写：事件被基类默认实现（mv=nullptr）丢弃，Code 级局部变量类型注解
    // 在写出时静默消失。目标表随注解收集，start_pc/length 在 put 阶段读 Label
    // 解析值（visit 可先于 visitCode 到达，属 ASM 前向引用形态）。
    auto* ann = new AnnotationWriter(symbols_, descriptor);
    ann->setTypeAnnotation(typeRef, typePath);
    ann->setLocalVariableTarget(start, end, index);
    ownedAnns_.push_back(ann);
    if (visible) {
        codeVisibleTypeAnnotations_.push_back(ann);
    }
    else {
        codeInvisibleTypeAnnotations_.push_back(ann);
    }
    return ann;
}

// ======================================================================
// Code lifecycle (ASM L728-730, L1546-1557, L1769-1771)
// ======================================================================

void MethodWriter::visitCode()
{ /* Nothing to do in ASM */
}

void MethodWriter::visitMaxs(int maxStack, int maxLocals)
{
    if (compute_ == COMPUTE_ALL_FRAMES) {
        computeAllFrames();
    }
    else if (compute_ == COMPUTE_MAX_STACK_AND_LOCAL) {
        computeMaxStackAndLocal();
    }
    else if (compute_ == COMPUTE_MAX_STACK_AND_LOCAL_FROM_FRAMES) {
        this->maxStack_ = maxRelativeStackSize_;
    }
    else {
        this->maxStack_  = maxStack;
        this->maxLocals_ = maxLocals;
    }
}

void MethodWriter::visitEnd()
{ /* Nothing to do in ASM */
}

// ======================================================================
// Instruction visitors (ASM L862-1404)
// ======================================================================

void MethodWriter::visitInsn(int opcode)
{
    requireStackSizeDeltaOpcode(opcode);
    lastBytecodeOffset_ = code_.size();
    code_.putByte(opcode);
    if (currentBasicBlock_ != nullptr) {
        if (compute_ == COMPUTE_ALL_FRAMES || compute_ == COMPUTE_INSERTED_FRAMES) {
            currentBasicBlock_->frame->execute(opcode, 0, nullptr, symbols_);
        }
        else {
            int size = relativeStackSize_ + STACK_SIZE_DELTA[opcode];
            if (size > maxRelativeStackSize_) maxRelativeStackSize_ = size;
            relativeStackSize_ = size;
        }
        if ((opcode >= Opcodes::IRETURN && opcode <= Opcodes::RETURN) || opcode == Opcodes::ATHROW)
            endCurrentBasicBlockWithNoSuccessor();
    }
}

void MethodWriter::visitIntInsn(int opcode, int operand)
{
    lastBytecodeOffset_ = code_.size();
    if (opcode == Opcodes::SIPUSH)
        code_.put12(opcode, operand);
    else
        code_.put11(opcode, operand);
    if (currentBasicBlock_ != nullptr) {
        if (compute_ == COMPUTE_ALL_FRAMES || compute_ == COMPUTE_INSERTED_FRAMES) {
            currentBasicBlock_->frame->execute(opcode, operand, nullptr, symbols_);
        }
        else if (opcode != Opcodes::NEWARRAY) {
            int size = relativeStackSize_ + 1;
            if (size > maxRelativeStackSize_) maxRelativeStackSize_ = size;
            relativeStackSize_ = size;
        }
    }
}

void MethodWriter::visitVarInsn(int opcode, int varIndex)
{
    requireStackSizeDeltaOpcode(opcode);
    lastBytecodeOffset_ = code_.size();
    if (varIndex < 4 && opcode != Opcodes::RET) {
        int optimizedOpcode;
        if (opcode < Opcodes::ISTORE)
            optimizedOpcode = Constants::ILOAD_0 + ((opcode - Opcodes::ILOAD) << 2) + varIndex;
        else
            optimizedOpcode = Constants::ISTORE_0 + ((opcode - Opcodes::ISTORE) << 2) + varIndex;
        code_.putByte(optimizedOpcode);
    }
    else if (varIndex >= 256) {
        code_.putByte(Constants::WIDE).put12(opcode, varIndex);
    }
    else {
        code_.put11(opcode, varIndex);
    }
    if (currentBasicBlock_ != nullptr) {
        if (compute_ == COMPUTE_ALL_FRAMES || compute_ == COMPUTE_INSERTED_FRAMES) {
            if (opcode == Opcodes::RET && compute_ == COMPUTE_ALL_FRAMES) {
                throw std::invalid_argument("JSR/RET are not supported with computeFrames option");
            }
            currentBasicBlock_->frame->execute(opcode, varIndex, nullptr, symbols_);
        }
        else {
            if (opcode == Opcodes::RET) {
                currentBasicBlock_->flags |= Label::FLAG_SUBROUTINE_END;
                currentBasicBlock_->outputStackSize = static_cast<short>(relativeStackSize_);
                endCurrentBasicBlockWithNoSuccessor();
            }
            else {
                int size = relativeStackSize_ + STACK_SIZE_DELTA[opcode];
                if (size > maxRelativeStackSize_) maxRelativeStackSize_ = size;
                relativeStackSize_ = size;
            }
        }
    }
    if (compute_ != COMPUTE_NOTHING) {
        int currentMaxLocals =
            varIndex + (opcode == Opcodes::LLOAD || opcode == Opcodes::DLOAD ||
                                opcode == Opcodes::LSTORE || opcode == Opcodes::DSTORE
                            ? 2
                            : 1);
        if (currentMaxLocals > maxLocals_) maxLocals_ = currentMaxLocals;
    }
    if (opcode >= Opcodes::ISTORE && compute_ == COMPUTE_ALL_FRAMES && firstHandler_ != nullptr)
        visitLabel(new Label());
}

void MethodWriter::visitTypeInsn(int opcode, ca::str::Utf8StringRef type)
{
    lastBytecodeOffset_ = code_.size();
    Symbol* typeSymbol  = symbols_.addConstantClass(type);
    code_.put12(opcode, typeSymbol ? typeSymbol->index : 0);
    if (currentBasicBlock_ != nullptr) {
        if (compute_ == COMPUTE_ALL_FRAMES || compute_ == COMPUTE_INSERTED_FRAMES) {
            currentBasicBlock_->frame->execute(opcode, lastBytecodeOffset_, typeSymbol, symbols_);
        }
        else if (opcode == Opcodes::NEW) {
            int size = relativeStackSize_ + 1;
            if (size > maxRelativeStackSize_) maxRelativeStackSize_ = size;
            relativeStackSize_ = size;
        }
    }
}

void MethodWriter::visitFieldInsn(int opcode, ca::str::Utf8StringRef owner,
                                  ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor)
{
    lastBytecodeOffset_    = code_.size();
    Symbol* fieldrefSymbol = symbols_.addConstantFieldref(owner, name, descriptor);
    code_.put12(opcode, fieldrefSymbol ? fieldrefSymbol->index : 0);
    if (currentBasicBlock_ != nullptr) {
        if (compute_ == COMPUTE_ALL_FRAMES || compute_ == COMPUTE_INSERTED_FRAMES) {
            currentBasicBlock_->frame->execute(opcode, 0, fieldrefSymbol, symbols_);
        }
        else {
            int  size;
            char firstDescChar =
                static_cast<char>(descriptor.byte_length() > 0 ? descriptor.byte_at(0) : 'I');
            switch (opcode) {
            case Opcodes::GETSTATIC:
                size = relativeStackSize_ + (firstDescChar == 'D' || firstDescChar == 'J' ? 2 : 1);
                break;
            case Opcodes::PUTSTATIC:
                size =
                    relativeStackSize_ + (firstDescChar == 'D' || firstDescChar == 'J' ? -2 : -1);
                break;
            case Opcodes::GETFIELD:
                size = relativeStackSize_ + (firstDescChar == 'D' || firstDescChar == 'J' ? 1 : 0);
                break;
            default:   // PUTFIELD
                size =
                    relativeStackSize_ + (firstDescChar == 'D' || firstDescChar == 'J' ? -3 : -2);
                break;
            }
            if (size > maxRelativeStackSize_) maxRelativeStackSize_ = size;
            relativeStackSize_ = size;
        }
    }
}

void MethodWriter::visitMethodInsn(int opcode, ca::str::Utf8StringRef owner,
                                   ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                   bool isInterface)
{
    lastBytecodeOffset_     = code_.size();
    Symbol* methodrefSymbol = symbols_.addConstantMethodref(owner, name, descriptor, isInterface);
    if (opcode == Opcodes::INVOKEINTERFACE) {
        int argsAndSize = argsAndReturnSize(descriptor);
        code_.put12(Opcodes::INVOKEINTERFACE, methodrefSymbol ? methodrefSymbol->index : 0)
            .put11(argsAndSize >> 2, 0);
    }
    else {
        code_.put12(opcode, methodrefSymbol ? methodrefSymbol->index : 0);
    }
    if (currentBasicBlock_ != nullptr) {
        if (compute_ == COMPUTE_ALL_FRAMES || compute_ == COMPUTE_INSERTED_FRAMES) {
            currentBasicBlock_->frame->execute(opcode, 0, methodrefSymbol, symbols_);
        }
        else {
            int argsAndSize    = argsAndReturnSize(descriptor);
            int stackSizeDelta = (argsAndSize & 3) - (argsAndSize >> 2);
            int size;
            if (opcode == Opcodes::INVOKESTATIC)
                size = relativeStackSize_ + stackSizeDelta + 1;
            else
                size = relativeStackSize_ + stackSizeDelta;
            if (size > maxRelativeStackSize_) maxRelativeStackSize_ = size;
            relativeStackSize_ = size;
        }
    }
}

void MethodWriter::visitInvokeDynamicInsn(ca::str::Utf8StringRef name,
                                          ca::str::Utf8StringRef descriptor, const Handle& bsm,
                                          const std::vector<ca::core::Any>& bsmArgs)
{
    // Convert ca::core::Any args → ca::core::Any so they work with new-API symbol table.
    // This path is legacy (issue #339, frame/invokedynamic pass) and will be
    // migrated fully in a follow-up.
    std::vector<ca::core::Any> convertedArgs;
    convertedArgs.reserve(bsmArgs.size());
    for (const auto& a : bsmArgs) {
        if (auto* p = a.as<int>())
            convertedArgs.push_back(ca::core::Any(*p));
        else if (auto* p = a.as<ca::i64>())
            convertedArgs.push_back(ca::core::Any(*p));
        else if (auto* p = a.as<float>())
            convertedArgs.push_back(ca::core::Any(*p));
        else if (auto* p = a.as<double>())
            convertedArgs.push_back(ca::core::Any(*p));
        else if (auto* p = a.as<Utf8StringRef>())
            convertedArgs.push_back(ca::core::Any(*p));
        else if (auto* p = a.as<Type>())
            convertedArgs.push_back(ca::core::Any(*p));
        else if (auto* p = a.as<Handle>())
            convertedArgs.push_back(ca::core::Any(*p));
        else if (auto* p = a.as<ConstantDynamic>())
            convertedArgs.push_back(ca::core::Any(*p));
        else
            convertedArgs.push_back(ca::core::Any());
    }
    lastBytecodeOffset_ = code_.size();
    Symbol* invokeDynamicSymbol =
        symbols_.addConstantInvokeDynamic(name, descriptor, bsm, convertedArgs);
    code_.put12(Opcodes::INVOKEDYNAMIC, invokeDynamicSymbol ? invokeDynamicSymbol->index : 0);
    code_.putShort(0);
    if (currentBasicBlock_ != nullptr) {
        if (compute_ == COMPUTE_ALL_FRAMES || compute_ == COMPUTE_INSERTED_FRAMES) {
            currentBasicBlock_->frame->execute(
                Opcodes::INVOKEDYNAMIC, 0, invokeDynamicSymbol, symbols_);
        }
        else {
            int argsAndSize    = argsAndReturnSize(descriptor);
            int stackSizeDelta = (argsAndSize & 3) - (argsAndSize >> 2) + 1;
            int size           = relativeStackSize_ + stackSizeDelta;
            if (size > maxRelativeStackSize_) maxRelativeStackSize_ = size;
            relativeStackSize_ = size;
        }
    }
}

void MethodWriter::visitJumpInsn(int opcode, Label* label)
{
    int baseOpcode =
        (opcode >= Constants::GOTO_W) ? opcode - Constants::WIDE_JUMP_OPCODE_DELTA : opcode;
    requireStackSizeDeltaOpcode(baseOpcode);
    lastBytecodeOffset_       = code_.size();
    bool nextInsnIsJumpTarget = false;

    int relativeOffset = label->bytecodeOffset - static_cast<int>(code_.size());
    if ((label->flags & Label::FLAG_RESOLVED) != 0 &&
        (relativeOffset < static_cast<int>(0xFFFF8000) || relativeOffset > 0x7FFF)) {
        if (baseOpcode == Opcodes::GOTO)
            code_.putByte(Constants::GOTO_W);
        else if (baseOpcode == Opcodes::JSR)
            code_.putByte(Constants::JSR_W);
        else {
            code_.putByte(baseOpcode >= Opcodes::IFNULL ? baseOpcode ^ 1
                                                        : ((baseOpcode + 1) ^ 1) - 1);
            code_.putShort(8);
            code_.putByte(Constants::ASM_GOTO_W);
            hasAsmInstructions_  = true;
            nextInsnIsJumpTarget = true;
        }
        label->put(code_, code_.size() - 1, true);
    }
    else if (baseOpcode != opcode) {
        code_.putByte(opcode);
        label->put(code_, code_.size() - 1, true);
    }
    else {
        code_.putByte(baseOpcode);
        label->put(code_, code_.size() - 1, false);
    }

    if (currentBasicBlock_ != nullptr) {
        Label* nextBasicBlock = nullptr;
        if (compute_ == COMPUTE_ALL_FRAMES) {
            if (baseOpcode == Opcodes::JSR) {
                throw std::invalid_argument("JSR/RET are not supported with computeFrames option");
            }
            currentBasicBlock_->frame->execute(baseOpcode, 0, nullptr, symbols_);
            label->getCanonicalInstance()->flags |= Label::FLAG_JUMP_TARGET;
            addSuccessorToCurrentBasicBlock(Edge::JUMP, label);
            if (baseOpcode != Opcodes::GOTO) nextBasicBlock = new Label();
        }
        else if (compute_ == COMPUTE_INSERTED_FRAMES) {
            currentBasicBlock_->frame->execute(baseOpcode, 0, nullptr, symbols_);
        }
        else if (compute_ == COMPUTE_MAX_STACK_AND_LOCAL_FROM_FRAMES) {
            relativeStackSize_ += STACK_SIZE_DELTA[baseOpcode];
        }
        else {
            if (baseOpcode == Opcodes::JSR) {
                if ((label->flags & Label::FLAG_SUBROUTINE_START) == 0) {
                    label->flags |= Label::FLAG_SUBROUTINE_START;
                    hasSubroutines_ = true;
                }
                currentBasicBlock_->flags |= Label::FLAG_SUBROUTINE_CALLER;
                addSuccessorToCurrentBasicBlock(relativeStackSize_ + 1, label);
                nextBasicBlock = new Label();
            }
            else {
                relativeStackSize_ += STACK_SIZE_DELTA[baseOpcode];
                addSuccessorToCurrentBasicBlock(relativeStackSize_, label);
            }
        }
        if (nextBasicBlock != nullptr) {
            if (nextInsnIsJumpTarget) nextBasicBlock->flags |= Label::FLAG_JUMP_TARGET;
            visitLabel(nextBasicBlock);
        }
        if (baseOpcode == Opcodes::GOTO) endCurrentBasicBlockWithNoSuccessor();
    }
}

void MethodWriter::visitLabel(Label* label)
{
    hasAsmInstructions_ |=
        label->resolve(code_.mutableData(), &stackMapTableEntries_, code_.size());
    if ((label->flags & Label::FLAG_DEBUG_ONLY) != 0) return;

    if (compute_ == COMPUTE_ALL_FRAMES) {
        if (currentBasicBlock_ != nullptr) {
            if (label->bytecodeOffset == currentBasicBlock_->bytecodeOffset) {
                currentBasicBlock_->flags |= (label->flags & Label::FLAG_JUMP_TARGET);
                label->frame = currentBasicBlock_->frame;
                return;
            }
            addSuccessorToCurrentBasicBlock(Edge::JUMP, label);
        }
        if (lastBasicBlock_ != nullptr) {
            if (label->bytecodeOffset == lastBasicBlock_->bytecodeOffset) {
                lastBasicBlock_->flags |= (label->flags & Label::FLAG_JUMP_TARGET);
                label->frame       = lastBasicBlock_->frame;
                currentBasicBlock_ = lastBasicBlock_;
                return;
            }
            lastBasicBlock_->nextBasicBlock = label;
        }
        lastBasicBlock_    = label;
        currentBasicBlock_ = label;
        label->frame       = new Frame(label);
    }
    else if (compute_ == COMPUTE_INSERTED_FRAMES) {
        if (currentBasicBlock_ == nullptr)
            currentBasicBlock_ = label;
        else if (currentBasicBlock_->frame)
            currentBasicBlock_->frame->owner = label;
    }
    else if (compute_ == COMPUTE_MAX_STACK_AND_LOCAL) {
        if (currentBasicBlock_ != nullptr) {
            currentBasicBlock_->outputStackMax = static_cast<short>(maxRelativeStackSize_);
            addSuccessorToCurrentBasicBlock(relativeStackSize_, label);
        }
        currentBasicBlock_    = label;
        relativeStackSize_    = 0;
        maxRelativeStackSize_ = 0;
        if (lastBasicBlock_ != nullptr) lastBasicBlock_->nextBasicBlock = label;
        lastBasicBlock_ = label;
    }
    else if (compute_ == COMPUTE_MAX_STACK_AND_LOCAL_FROM_FRAMES && currentBasicBlock_ == nullptr) {
        currentBasicBlock_ = label;
    }
}

void MethodWriter::visitLdcInsn(const Any& value)
{
    lastBytecodeOffset_    = code_.size();
    Symbol* constantSymbol = nullptr;
    if (value.is<int>())
        constantSymbol = symbols_.addConstantInteger(value.cast<int>());
    else if (value.is<float>())
        constantSymbol = symbols_.addConstantFloat(value.cast<float>());
    else if (value.is<ca::i64>())
        constantSymbol = symbols_.addConstantLong(value.cast<ca::i64>());
    else if (value.is<double>())
        constantSymbol = symbols_.addConstantDouble(value.cast<double>());
    else if (value.is<ca::str::Utf8StringRef>())
        constantSymbol = symbols_.addConstantString(value.cast<ca::str::Utf8StringRef>());
    else if (value.is<std::string>()) {
#ifndef NDEBUG
        // 字符串线护栏(总方案 §5.2-3):本分支保留作兼容兜底,但新代码应经作业级
        // arena intern 成 Utf8StringRef 再装箱。debug 构建下首次触发即报告,
        // 让残留违规可见 —— 这是字符串线的"dangle_check 时刻"。
        static std::atomic<bool> fallbackWarned{false};
        if (!fallbackWarned.exchange(true)) {
            CA_LOG_WARN("[garena] writer fallback intern fired: Any(std::string) reached "
                        "MethodWriter::visitLdcInsn; callers should intern into the job "
                        "Utf8StringArena and box Utf8StringRef");
        }
#endif
        const auto& s  = value.cast<std::string>();
        constantSymbol = symbols_.addConstantString(
            ca::str::Utf8StringRef(reinterpret_cast<const ca::u8*>(s.data()),
                                   static_cast<ca::usize>(s.size()),
                                   ca::str::Utf8StringRef::npos));
    }
    else if (value.is<const char*>()) {
        const char* s  = value.cast<const char*>();
        constantSymbol = symbols_.addConstantString(
            ca::str::Utf8StringRef(reinterpret_cast<const ca::u8*>(s),
                                   static_cast<ca::usize>(std::strlen(s)),
                                   ca::str::Utf8StringRef::npos));
    }
    else if (value.is<Type>()) {
        auto type      = value.cast<Type>();
        constantSymbol = type.getSort() == Type::METHOD
                             ? symbols_.addConstantMethodType(type.getDescriptor(symbols_.arena()))
                             : symbols_.addConstantClass(constantClassName(type, symbols_.arena()));
    }
    else if (value.is<Handle>()) {
        auto h         = value.cast<Handle>();
        constantSymbol = symbols_.addConstantMethodHandle(
            h.getTag(), h.getOwner(), h.getName(), h.getDesc(), h.isInterface());
    }
    else if (value.is<ConstantDynamic>()) {
        auto cd        = value.cast<ConstantDynamic>();
        constantSymbol = symbols_.addConstantDynamic(cd.getName(),
                                                     cd.getDescriptor(),
                                                     cd.getBootstrapMethod(),
                                                     cd.getBootstrapMethodArguments());
    }

    if (!constantSymbol) {
        code_.putByte(Opcodes::ACONST_NULL);
        lastBytecodeOffset_++;
        return;
    }

    int  constantIndex = constantSymbol->index;
    bool isLongOrDouble =
        constantSymbol->tag == Symbol::CONSTANT_LONG_TAG ||
        constantSymbol->tag == Symbol::CONSTANT_DOUBLE_TAG ||
        (constantSymbol->tag == Symbol::CONSTANT_DYNAMIC_TAG &&
         constantSymbol->value.byte_length() > 0 &&
         (constantSymbol->value.byte_at(0) == 'J' || constantSymbol->value.byte_at(0) == 'D'));

    if (isLongOrDouble)
        code_.put12(Constants::LDC2_W, constantIndex);
    else if (constantIndex >= 256)
        code_.put12(Constants::LDC_W, constantIndex);
    else
        code_.put11(Opcodes::LDC, constantIndex);

    if (currentBasicBlock_ != nullptr) {
        if (compute_ == COMPUTE_ALL_FRAMES || compute_ == COMPUTE_INSERTED_FRAMES) {
            currentBasicBlock_->frame->execute(Opcodes::LDC, 0, constantSymbol, symbols_);
        }
        else {
            int size = relativeStackSize_ + (isLongOrDouble ? 2 : 1);
            if (size > maxRelativeStackSize_) maxRelativeStackSize_ = size;
            relativeStackSize_ = size;
        }
    }
}

void MethodWriter::visitIincInsn(int varIndex, int increment)
{
    lastBytecodeOffset_ = code_.size();
    if (varIndex > 255 || increment > 127 || increment < -128) {
        code_.putByte(Constants::WIDE).put12(Opcodes::IINC, varIndex).putShort(increment);
    }
    else {
        code_.putByte(Opcodes::IINC).put11(varIndex, increment);
    }
    if (currentBasicBlock_ != nullptr &&
        (compute_ == COMPUTE_ALL_FRAMES || compute_ == COMPUTE_INSERTED_FRAMES)) {
        currentBasicBlock_->frame->execute(Opcodes::IINC, varIndex, nullptr, symbols_);
    }
    if (compute_ != COMPUTE_NOTHING) {
        int currentMaxLocals = varIndex + 1;
        if (currentMaxLocals > maxLocals_) maxLocals_ = currentMaxLocals;
    }
}

void MethodWriter::visitTableSwitchInsn(int min, int max, Label* dflt,
                                        const std::vector<Label*>& labels)
{
    lastBytecodeOffset_ = code_.size();
    code_.putByte(Opcodes::TABLESWITCH);
    int pad = (4 - code_.size() % 4) % 4;
    for (int i = 0; i < pad; i++) code_.putByte(0);
    dflt->put(code_, lastBytecodeOffset_, true);
    code_.putInt(min).putInt(max);
    for (auto* l : labels) l->put(code_, lastBytecodeOffset_, true);

    // visitSwitchInsn equivalent
    if (currentBasicBlock_ != nullptr) {
        if (compute_ == COMPUTE_ALL_FRAMES) {
            currentBasicBlock_->frame->execute(Opcodes::LOOKUPSWITCH, 0, nullptr, symbols_);
            addSuccessorToCurrentBasicBlock(Edge::JUMP, dflt);
            dflt->getCanonicalInstance()->flags |= Label::FLAG_JUMP_TARGET;
            for (auto* l : labels) {
                addSuccessorToCurrentBasicBlock(Edge::JUMP, l);
                l->getCanonicalInstance()->flags |= Label::FLAG_JUMP_TARGET;
            }
        }
        else if (compute_ == COMPUTE_MAX_STACK_AND_LOCAL) {
            --relativeStackSize_;
            addSuccessorToCurrentBasicBlock(relativeStackSize_, dflt);
            for (auto* l : labels) addSuccessorToCurrentBasicBlock(relativeStackSize_, l);
        }
        endCurrentBasicBlockWithNoSuccessor();
    }
}

void MethodWriter::visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                                         const std::vector<Label*>& labels)
{
    lastBytecodeOffset_ = code_.size();
    code_.putByte(Opcodes::LOOKUPSWITCH);
    int pad = (4 - code_.size() % 4) % 4;
    for (int i = 0; i < pad; i++) code_.putByte(0);
    dflt->put(code_, lastBytecodeOffset_, true);
    code_.putInt(static_cast<int>(labels.size()));
    for (size_t i = 0; i < labels.size(); i++) {
        code_.putInt(keys[i]);
        labels[i]->put(code_, lastBytecodeOffset_, true);
    }

    if (currentBasicBlock_ != nullptr) {
        if (compute_ == COMPUTE_ALL_FRAMES) {
            currentBasicBlock_->frame->execute(Opcodes::LOOKUPSWITCH, 0, nullptr, symbols_);
            addSuccessorToCurrentBasicBlock(Edge::JUMP, dflt);
            dflt->getCanonicalInstance()->flags |= Label::FLAG_JUMP_TARGET;
            for (auto* l : labels) {
                addSuccessorToCurrentBasicBlock(Edge::JUMP, l);
                l->getCanonicalInstance()->flags |= Label::FLAG_JUMP_TARGET;
            }
        }
        else if (compute_ == COMPUTE_MAX_STACK_AND_LOCAL) {
            --relativeStackSize_;
            addSuccessorToCurrentBasicBlock(relativeStackSize_, dflt);
            for (auto* l : labels) addSuccessorToCurrentBasicBlock(relativeStackSize_, l);
        }
        endCurrentBasicBlockWithNoSuccessor();
    }
}

void MethodWriter::visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor, int numDimensions)
{
    lastBytecodeOffset_ = code_.size();
    Symbol* descSymbol  = symbols_.addConstantClass(descriptor);
    code_.put12(Opcodes::MULTIANEWARRAY, descSymbol ? descSymbol->index : 0).putByte(numDimensions);
    if (currentBasicBlock_ != nullptr) {
        if (compute_ == COMPUTE_ALL_FRAMES || compute_ == COMPUTE_INSERTED_FRAMES) {
            currentBasicBlock_->frame->execute(
                Opcodes::MULTIANEWARRAY, numDimensions, descSymbol, symbols_);
        }
        else {
            relativeStackSize_ += 1 - numDimensions;
        }
    }
}

void MethodWriter::visitFrame(int type, int numLocal, const std::vector<ca::core::Any>& local,
                              int numStack, const std::vector<ca::core::Any>& stack)
{
    if (compute_ == COMPUTE_ALL_FRAMES) return;

    if (compute_ == COMPUTE_INSERTED_FRAMES) {
        if (currentBasicBlock_->frame == nullptr) {
            currentBasicBlock_->frame = new CurrentFrame(currentBasicBlock_);
            currentBasicBlock_->frame->setInputFrameFromDescriptor(
                symbols_, access_, descStr_, numLocal);
            currentBasicBlock_->frame->accept(*this);
        }
        else {
            if (type == Opcodes::F_NEW) {
                currentBasicBlock_->frame->setInputFrameFromApiFormat(
                    symbols_, numLocal, local, numStack, stack);
            }
            currentBasicBlock_->frame->accept(*this);
        }
    }
    else if (type == Opcodes::F_NEW) {
        if (previousFrame_.empty()) {
            int   argumentsSize = argsAndReturnSize(descStr_) >> 2;
            Frame implicitFirstFrame(new Label());
            implicitFirstFrame.setInputFrameFromDescriptor(
                symbols_, access_, descStr_, argumentsSize);
            implicitFirstFrame.accept(*this);
        }
        currentLocals_ = numLocal;
        int frameIndex = visitFrameStart(code_.size(), numLocal, numStack);
        for (int i = 0; i < numLocal; ++i)
            currentFrame_[frameIndex++] = Frame::getAbstractTypeFromApiFormat(symbols_, local[i]);
        for (int i = 0; i < numStack; ++i)
            currentFrame_[frameIndex++] = Frame::getAbstractTypeFromApiFormat(symbols_, stack[i]);
        visitFrameEnd();
    }
    else {
        if ((classWriter_.version() & 0xFFFF) < (Opcodes::V1_6 & 0xFFFF)) {
            throw std::invalid_argument("Class versions V1_5 or less must use F_NEW frames.");
        }
        int offsetDelta;
        if (stackMapTableEntries_.size() == 0) {
            offsetDelta = static_cast<int>(code_.size());
        }
        else {
            offsetDelta = static_cast<int>(code_.size()) - previousFrame_[0] - 1;
            if (offsetDelta < 0) {
                if (type == Opcodes::F_SAME)
                    return;
                else
                    throw std::logic_error("Invalid frame offset");
            }
        }

        switch (type) {
        case Opcodes::F_FULL:
            currentLocals_ = numLocal;
            stackMapTableEntries_.putByte(FULL_FRAME).putShort(offsetDelta).putShort(numLocal);
            for (int i = 0; i < numLocal; ++i) {
                putVerificationTypeInfo(symbols_, stackMapTableEntries_, local[i]);
            }
            stackMapTableEntries_.putShort(numStack);
            for (int i = 0; i < numStack; ++i) {
                putVerificationTypeInfo(symbols_, stackMapTableEntries_, stack[i]);
            }
            break;
        case Opcodes::F_APPEND:
            currentLocals_ += numLocal;
            stackMapTableEntries_.putByte(SAME_FRAME_EXTENDED + numLocal).putShort(offsetDelta);
            for (int i = 0; i < numLocal; ++i) {
                putVerificationTypeInfo(symbols_, stackMapTableEntries_, local[i]);
            }
            break;
        case Opcodes::F_CHOP:
            currentLocals_ -= numLocal;
            stackMapTableEntries_.putByte(static_cast<ca::u8>(SAME_FRAME_EXTENDED - numLocal))
                .putShort(offsetDelta);
            break;
        case Opcodes::F_SAME:
            if (offsetDelta < 64)
                stackMapTableEntries_.putByte(static_cast<ca::u8>(offsetDelta));
            else
                stackMapTableEntries_.putByte(SAME_FRAME_EXTENDED).putShort(offsetDelta);
            break;
        case Opcodes::F_SAME1:
            if (offsetDelta < 64)
                stackMapTableEntries_.putByte(
                    static_cast<ca::u8>(SAME_LOCALS_1_STACK_ITEM_FRAME + offsetDelta));
            else
                stackMapTableEntries_.putByte(SAME_LOCALS_1_STACK_ITEM_FRAME_EXTENDED)
                    .putShort(offsetDelta);
            if (stack.size() > 0) {
                putVerificationTypeInfo(symbols_, stackMapTableEntries_, stack[0]);
            }
            break;
        default: throw std::logic_error("Unknown frame type");
        }

        ++stackMapTableNumberOfEntries_;
        previousFrame_.assign({static_cast<int>(code_.size()), currentLocals_, numStack});
    }

    if (compute_ == COMPUTE_MAX_STACK_AND_LOCAL_FROM_FRAMES) {
        relativeStackSize_ = numStack;
        for (int i = 0; i < numStack; ++i) {
            if (i < static_cast<int>(stack.size())) {
                auto* p = stack[i].as<int>();
                if (!p) continue;
                int val = *p;
                if (val == Opcodes::LONG || val == Opcodes::DOUBLE) relativeStackSize_++;
            }
        }
        if (relativeStackSize_ > maxRelativeStackSize_) maxRelativeStackSize_ = relativeStackSize_;
    }
    maxStack_  = (std::max)(static_cast<int>(maxStack_), numStack);
    maxLocals_ = (std::max)(static_cast<int>(maxLocals_), currentLocals_);
}

void MethodWriter::visitTryCatchBlock(Label* start, Label* end, Label* handler,
                                      ca::str::Utf8StringRef type)
{
    auto* newHandler = new Handler(
        start,
        end,
        handler,
        (type.data() && type.byte_length() > 0) ? symbols_.addConstantClass(type)->index : 0,
        type);
    if (firstHandler_ == nullptr)
        firstHandler_ = newHandler;
    else
        lastHandler_->nextHandler = newHandler;
    lastHandler_ = newHandler;
}

void MethodWriter::visitLocalVariable(ca::str::Utf8StringRef name,
                                      ca::str::Utf8StringRef descriptor,
                                      ca::str::Utf8StringRef signature, Label* start, Label* end,
                                      int index)
{
    if (signature.data() && signature.byte_length() > 0) {
        if (localVariableTypeTable_.size() == 0) localVariableTypeTable_ = ByteVector();
        ++localVariableTypeTableLength_;
        localVariableTypeTable_.putShort(start->bytecodeOffset)
            .putShort(end->bytecodeOffset - start->bytecodeOffset)
            .putShort(symbols_.addConstantUtf8(name))
            .putShort(symbols_.addConstantUtf8(signature))
            .putShort(index);
    }
    if (descriptor.data() && descriptor.byte_length() > 0) {
        if (localVariableTable_.size() == 0) localVariableTable_ = ByteVector();
        ++localVariableTableLength_;
        localVariableTable_.putShort(start->bytecodeOffset)
            .putShort(end->bytecodeOffset - start->bytecodeOffset)
            .putShort(symbols_.addConstantUtf8(name))
            .putShort(symbols_.addConstantUtf8(descriptor))
            .putShort(index);
    }
    if (compute_ != COMPUTE_NOTHING && descriptor.data() && descriptor.byte_length() > 0) {
        char firstDescChar =
            static_cast<char>(descriptor.byte_length() > 0 ? descriptor.byte_at(0) : 'I');
        int currentMaxLocals = index + (firstDescChar == 'J' || firstDescChar == 'D' ? 2 : 1);
        if (currentMaxLocals > maxLocals_) maxLocals_ = currentMaxLocals;
    }
}

void MethodWriter::visitLineNumber(int line, Label* start)
{
    if (lineNumberTable_.size() == 0) lineNumberTable_ = ByteVector();
    ++lineNumberTableLength_;
    lineNumberTable_.putShort(start->bytecodeOffset).putShort(line);
}

// ======================================================================
// Frame emit API (ASM L1821-1857)
// ======================================================================

int MethodWriter::visitFrameStart(int offset, int numLocal, int numStack)
{
    int frameLength = 3 + numLocal + numStack;
    if (static_cast<int>(currentFrame_.size()) < frameLength) currentFrame_.resize(frameLength, 0);
    currentFrame_[0] = offset;
    currentFrame_[1] = numLocal;
    currentFrame_[2] = numStack;
    return 3;
}

void MethodWriter::visitAbstractType(int frameIndex, int abstractType)
{
    if (frameIndex >= 0 && frameIndex < static_cast<int>(currentFrame_.size()))
        currentFrame_[frameIndex] = abstractType;
}

void MethodWriter::visitFrameEnd()
{
    if (!previousFrame_.empty()) {
        if (stackMapTableEntries_.size() == 0) stackMapTableEntries_ = ByteVector();
        putFrame();
        ++stackMapTableNumberOfEntries_;
    }
    previousFrame_ = currentFrame_;
    currentFrame_.clear();
}

// ======================================================================
// putFrame — compress and write currentFrame to stackMapTableEntries (ASM L1860-1950)
// ======================================================================

void MethodWriter::putFrame()
{
    int numLocal         = currentFrame_[1];
    int numStack         = currentFrame_[2];
    int offsetDelta      = stackMapTableNumberOfEntries_ == 0 ? currentFrame_[0]
                                                              : currentFrame_[0] - previousFrame_[0] - 1;
    int previousNumlocal = previousFrame_[1];
    int numLocalDelta    = numLocal - previousNumlocal;

    int type = FULL_FRAME;
    if (numStack == 0) {
        if (numLocalDelta >= -3 && numLocalDelta <= -1)
            type = CHOP_FRAME;
        else if (numLocalDelta == 0)
            type = offsetDelta < 64 ? SAME_FRAME : SAME_FRAME_EXTENDED;
        else if (numLocalDelta >= 1 && numLocalDelta <= 3)
            type = APPEND_FRAME;
    }
    else if (numLocalDelta == 0 && numStack == 1) {
        type = offsetDelta < 63 ? SAME_LOCALS_1_STACK_ITEM_FRAME
                                : SAME_LOCALS_1_STACK_ITEM_FRAME_EXTENDED;
    }

    if (type != FULL_FRAME) {
        int frameIndex = 3;
        int maxCheck   = (std::min)(previousNumlocal, numLocal);
        for (int i = 0; i < maxCheck; i++) {
            if (currentFrame_[frameIndex] != previousFrame_[frameIndex]) {
                type = FULL_FRAME;
                break;
            }
            frameIndex++;
        }
    }

    switch (type) {
    case SAME_FRAME: stackMapTableEntries_.putByte(static_cast<ca::u8>(offsetDelta)); break;
    case SAME_LOCALS_1_STACK_ITEM_FRAME:
        stackMapTableEntries_.putByte(
            static_cast<ca::u8>(SAME_LOCALS_1_STACK_ITEM_FRAME + offsetDelta));
        putAbstractTypes(3 + numLocal, 4 + numLocal);
        break;
    case SAME_LOCALS_1_STACK_ITEM_FRAME_EXTENDED:
        stackMapTableEntries_.putByte(SAME_LOCALS_1_STACK_ITEM_FRAME_EXTENDED)
            .putShort(offsetDelta);
        putAbstractTypes(3 + numLocal, 4 + numLocal);
        break;
    case SAME_FRAME_EXTENDED:
        stackMapTableEntries_.putByte(SAME_FRAME_EXTENDED).putShort(offsetDelta);
        break;
    case CHOP_FRAME:
        stackMapTableEntries_.putByte(static_cast<ca::u8>(SAME_FRAME_EXTENDED + numLocalDelta))
            .putShort(offsetDelta);
        break;
    case APPEND_FRAME:
        stackMapTableEntries_.putByte(static_cast<ca::u8>(SAME_FRAME_EXTENDED + numLocalDelta))
            .putShort(offsetDelta);
        putAbstractTypes(3 + previousNumlocal, 3 + numLocal);
        break;
    case FULL_FRAME:
    default:
        stackMapTableEntries_.putByte(FULL_FRAME).putShort(offsetDelta).putShort(numLocal);
        putAbstractTypes(3, 3 + numLocal);
        stackMapTableEntries_.putShort(numStack);
        putAbstractTypes(3 + numLocal, 3 + numLocal + numStack);
        break;
    }
}

void MethodWriter::putAbstractTypes(int start, int end)
{
    for (int i = start; i < end; ++i)
        Frame::putAbstractType(symbols_, currentFrame_[i], stackMapTableEntries_);
}

// ======================================================================
// CFG helpers (ASM L1783-1807)
// ======================================================================

void MethodWriter::addSuccessorToCurrentBasicBlock(int info, Label* successor)
{
    currentBasicBlock_->outgoingEdges =
        new Edge(info, successor, currentBasicBlock_->outgoingEdges);
}

void MethodWriter::endCurrentBasicBlockWithNoSuccessor()
{
    if (compute_ == COMPUTE_ALL_FRAMES) {
        Label* nextBasicBlock = new Label();
        nextBasicBlock->frame = new Frame(nextBasicBlock);
        nextBasicBlock->resolve(code_.mutableData(), &stackMapTableEntries_, code_.size());
        lastBasicBlock_->nextBasicBlock = nextBasicBlock;
        lastBasicBlock_                 = nextBasicBlock;
        currentBasicBlock_              = nullptr;
    }
    else if (compute_ == COMPUTE_MAX_STACK_AND_LOCAL) {
        currentBasicBlock_->outputStackMax = static_cast<short>(maxRelativeStackSize_);
        currentBasicBlock_                 = nullptr;
    }
}

void MethodWriter::computeMaxStackAndLocal()
{
    Handler* handler = firstHandler_;
    while (handler != nullptr) {
        Label* handlerBlock      = handler->handlerPc;
        Label* handlerRangeBlock = handler->startPc;
        Label* handlerRangeEnd   = handler->endPc;
        while (handlerRangeBlock != nullptr && handlerRangeBlock != handlerRangeEnd) {
            if ((handlerRangeBlock->flags & Label::FLAG_SUBROUTINE_CALLER) == 0) {
                handlerRangeBlock->outgoingEdges =
                    new Edge(Edge::EXCEPTION, handlerBlock, handlerRangeBlock->outgoingEdges);
            }
            else if (handlerRangeBlock->outgoingEdges != nullptr &&
                     handlerRangeBlock->outgoingEdges->nextEdge != nullptr) {
                Edge* afterJsrEdges = handlerRangeBlock->outgoingEdges->nextEdge;
                afterJsrEdges->nextEdge =
                    new Edge(Edge::EXCEPTION, handlerBlock, afterJsrEdges->nextEdge);
            }
            handlerRangeBlock = handlerRangeBlock->nextBasicBlock;
        }
        handler = handler->nextHandler;
    }

    if (hasSubroutines_) {
        short numSubroutines = 1;
        if (firstBasicBlock_) firstBasicBlock_->markSubroutine(numSubroutines);
        for (short currentSubroutine = 1; currentSubroutine <= numSubroutines;
             ++currentSubroutine) {
            Label* basicBlock = firstBasicBlock_;
            while (basicBlock != nullptr) {
                if ((basicBlock->flags & Label::FLAG_SUBROUTINE_CALLER) != 0 &&
                    basicBlock->subroutineId == currentSubroutine &&
                    basicBlock->outgoingEdges != nullptr &&
                    basicBlock->outgoingEdges->nextEdge != nullptr) {
                    Label* jsrTarget = basicBlock->outgoingEdges->nextEdge->successor;
                    if (jsrTarget != nullptr && jsrTarget->subroutineId == 0) {
                        jsrTarget->markSubroutine(++numSubroutines);
                    }
                }
                basicBlock = basicBlock->nextBasicBlock;
            }
        }
        Label* basicBlock = firstBasicBlock_;
        while (basicBlock != nullptr) {
            if ((basicBlock->flags & Label::FLAG_SUBROUTINE_CALLER) != 0 &&
                basicBlock->outgoingEdges != nullptr &&
                basicBlock->outgoingEdges->nextEdge != nullptr) {
                Label* subroutine = basicBlock->outgoingEdges->nextEdge->successor;
                if (subroutine != nullptr) subroutine->addSubroutineRetSuccessors(basicBlock);
            }
            basicBlock = basicBlock->nextBasicBlock;
        }
    }

    std::vector<Label*>        blocksToProcess;
    std::unordered_set<Label*> queued;
    std::unordered_set<Label*> processed;
    if (firstBasicBlock_ != nullptr) {
        firstBasicBlock_->inputStackSize = 0;
        blocksToProcess.push_back(firstBasicBlock_);
        queued.insert(firstBasicBlock_);
    }

    int maxStackSize = maxStack_;
    while (!blocksToProcess.empty()) {
        Label* basicBlock = blocksToProcess.back();
        blocksToProcess.pop_back();
        queued.erase(basicBlock);
        processed.insert(basicBlock);
        int inputStackTop     = basicBlock->inputStackSize < 0 ? 0 : basicBlock->inputStackSize;
        int outputStackMax    = basicBlock->outputStackMax < 0 ? 0 : basicBlock->outputStackMax;
        int maxBlockStackSize = inputStackTop + outputStackMax;
        if (maxBlockStackSize > maxStackSize) maxStackSize = maxBlockStackSize;

        Edge* outgoingEdge = basicBlock->outgoingEdges;
        if ((basicBlock->flags & Label::FLAG_SUBROUTINE_CALLER) != 0 && outgoingEdge != nullptr) {
            outgoingEdge = outgoingEdge->nextEdge;
        }
        while (outgoingEdge != nullptr) {
            Label* successorBlock = outgoingEdge->successor;
            if (successorBlock != nullptr && processed.find(successorBlock) == processed.end()) {
                int successorInputStackSize =
                    outgoingEdge->info == Edge::EXCEPTION ? 1 : inputStackTop + outgoingEdge->info;
                if (queued.insert(successorBlock).second) {
                    successorBlock->inputStackSize = static_cast<short>(successorInputStackSize);
                    blocksToProcess.push_back(successorBlock);
                }
                else if (successorBlock->inputStackSize < 0 ||
                         successorInputStackSize < successorBlock->inputStackSize) {
                    successorBlock->inputStackSize = static_cast<short>(successorInputStackSize);
                }
            }
            outgoingEdge = outgoingEdge->nextEdge;
        }
    }
    maxStack_ = maxStackSize;
}

// ======================================================================
// computeAllFrames (ASM L1560-1658)
// ======================================================================

void MethodWriter::computeAllFrames()
{
    // Complete the control flow graph with exception handler blocks
    Handler* handler = firstHandler_;
    while (handler != nullptr) {
        ca::str::Utf8StringRef catchTypeDescriptor =
            handler->catchTypeDescriptor.is_empty()
                ? ZUtf8StringRef::from_static("java/lang/Throwable")
                : handler->catchTypeDescriptor.ref();
        int    catchType    = Frame::getAbstractTypeFromInternalName(symbols_, catchTypeDescriptor);
        Label* handlerBlock = handler->handlerPc->getCanonicalInstance();
        handlerBlock->flags |= Label::FLAG_JUMP_TARGET;
        Label* handlerRangeBlock = handler->startPc->getCanonicalInstance();
        Label* handlerRangeEnd   = handler->endPc->getCanonicalInstance();
        while (handlerRangeBlock != nullptr && handlerRangeBlock != handlerRangeEnd) {
            handlerRangeBlock->outgoingEdges =
                new Edge(catchType, handlerBlock, handlerRangeBlock->outgoingEdges);
            handlerRangeBlock = handlerRangeBlock->nextBasicBlock;
        }
        handler = handler->nextHandler;
    }

    // Create and visit the first (implicit) frame
    Frame* firstFrame = firstBasicBlock_->frame;
    firstFrame->setInputFrameFromDescriptor(symbols_, access_, descStr_, maxLocals_);
    firstFrame->accept(*this);

    // 统计基本块数量，用于推导 fixpoint 迭代上限。
    // 单调数据流框架（引用类型只能向上走到 Object）下，每个基本块的每个栈/local
    // 槽在收敛前最多变化有限次。上界 ≈ 块数 × 槽数 × K。给 K=4 的余量，避免误伤
    // 合法的大方法；真正不收敛（getCommonSuperClass 非单调/类型组合爆炸）的极端
    // 场景会超限，由 FrameComputationException 兜底，供调用方降级而非进程卡死。
    int   basicBlockCount = 0;
    for (Label* bb = firstBasicBlock_; bb != nullptr; bb = bb->nextBasicBlock) ++basicBlockCount;
    int   slotBound  = maxLocals_ + 16;   // 16 作为栈深度的保守估计（maxStack_ 此时未定）
    if (slotBound < 16) slotBound = 16;
    long long maxIterations =
        static_cast<long long>(basicBlockCount) * slotBound * 4 + 1024;   // +1024 兜住小方法

    // Fixpoint algorithm
    Label*    listOfBlocksToProcess           = firstBasicBlock_;
    listOfBlocksToProcess->nextListElement    = nullptr;
    int       maxStackSize                     = 0;
    long long iterations                       = 0;
    while (listOfBlocksToProcess != nullptr) {
        if (++iterations > maxIterations) {
            throw FrameComputationException(
                classWriter_.getClassName().to_std_string(), nameStr_.to_std_string(),
                descStr_.to_std_string(), iterations);
        }
        Label* basicBlock           = listOfBlocksToProcess;
        listOfBlocksToProcess       = listOfBlocksToProcess->nextListElement;
        basicBlock->nextListElement = nullptr;
        basicBlock->flags |= Label::FLAG_REACHABLE;

        int maxBlockStackSize = basicBlock->frame->getInputStackSize() +
                                (std::max)(0, static_cast<int>(basicBlock->outputStackMax));
        if (maxBlockStackSize > maxStackSize) maxStackSize = maxBlockStackSize;

        Edge* outgoingEdge = basicBlock->outgoingEdges;
        while (outgoingEdge != nullptr) {
            Label* successorBlock = outgoingEdge->successor->getCanonicalInstance();
            if (successorBlock->frame == nullptr) {
                // 防御（#575 崩溃复现）：跳转目标从未 visitLabel（调用方畸形结构，
                // 如 blockShuffle 重排丢失 label 节点）——补空 frame 让 fixpoint
                // 可处理，而不是解引用 null 崩溃。
                successorBlock->frame = new Frame(successorBlock);
            }
            bool successorBlockChanged = false;
            try {
                successorBlockChanged = basicBlock->frame->merge(
                    symbols_, *successorBlock->frame, outgoingEdge->info, typeResolver_);
            }
            catch (const CommonSuperClassBudgetExceeded&) {
                // 装饰器预算超限：provider 内部死循环（如继承环）的最后防线——
                // 转 FrameComputationException 让调用方按 frame_not_converged 降级。
                throw FrameComputationException(
                    classWriter_.getClassName().to_std_string(), nameStr_.to_std_string(),
                    descStr_.to_std_string(), iterations);
            }
            if (successorBlockChanged && successorBlock->nextListElement == nullptr) {
                successorBlock->nextListElement = listOfBlocksToProcess;
                listOfBlocksToProcess           = successorBlock;
            }
            outgoingEdge = outgoingEdge->nextEdge;
        }
    }

    // Visit stack map frames for all reachable jump targets, replace unreachable code.
    // Start from firstBasicBlock_ itself: if the first basic block is also a jump target
    // (e.g. a backwards switch/branch to offset 0), JVMS 4.7.4 requires an explicit frame
    // at offset 0. Non-jump-target first blocks are filtered by FLAG_JUMP_TARGET below.
    Label* basicBlock = firstBasicBlock_;
    while (basicBlock != nullptr) {
        if ((basicBlock->flags & (Label::FLAG_JUMP_TARGET | Label::FLAG_REACHABLE)) ==
            (Label::FLAG_JUMP_TARGET | Label::FLAG_REACHABLE)) {
            if (basicBlock->frame) basicBlock->frame->accept(*this);
        }
        if ((basicBlock->flags & Label::FLAG_REACHABLE) == 0) {
            Label* nextBasicBlock = basicBlock->nextBasicBlock;
            int    startOffset    = basicBlock->bytecodeOffset;
            int    endOffset      = (nextBasicBlock == nullptr ? static_cast<int>(code_.size())
                                                               : nextBasicBlock->bytecodeOffset) -
                            1;
            // 防御（#575 崩溃复现）：未 visitLabel 的跳转目标块 bytecodeOffset 为
            // 默认 -1，若直接写 NOP/ATHROW 会写越界（data[-1] → 访问冲突）。
            // 这类畸形结构跳过改写，由跳转序列化路径抛明确异常。
            if (startOffset >= 0 && endOffset >= startOffset) {
                auto* data = code_.mutableData();
                for (int i = startOffset; i < endOffset; ++i)
                    data[i] = static_cast<ca::u8>(Opcodes::NOP);
                data[endOffset]           = static_cast<ca::u8>(Opcodes::ATHROW);
                int frameIndex            = visitFrameStart(startOffset, 0, 1);
                currentFrame_[frameIndex] = Frame::getAbstractTypeFromInternalName(
                    symbols_, ZUtf8StringRef::from_static("java/lang/Throwable"));
                visitFrameEnd();
                if (firstHandler_)
                    firstHandler_ = Handler::removeRange(firstHandler_, basicBlock, nextBasicBlock);
                maxStackSize = (std::max)(maxStackSize, 1);
            }
        }
        basicBlock = basicBlock->nextBasicBlock;
    }
    maxStack_ = maxStackSize;
}

// ======================================================================
// Two-phase serialization (ASM L2076-2173, L2181-2383)
// ======================================================================

void MethodWriter::collectConstants()
{
    if (annotationDefault_) annotationDefault_->collectConstants();
    for (auto* a : visibleAnnotations_)
        if (a) a->collectConstants();
    for (auto* a : invisibleAnnotations_)
        if (a) a->collectConstants();
    for (auto* a : visibleTypeAnnotations_)
        if (a) a->collectConstants();
    for (auto* a : invisibleTypeAnnotations_)
        if (a) a->collectConstants();
    for (auto* a : codeVisibleTypeAnnotations_)
        if (a) a->collectConstants();
    for (auto* a : codeInvisibleTypeAnnotations_)
        if (a) a->collectConstants();
    for (auto& item : visibleParameterAnnotations_)
        if (item.second) item.second->collectConstants();
    for (auto& item : invisibleParameterAnnotations_)
        if (item.second) item.second->collectConstants();
    if (parametersCount_ > 0) symbols_.addConstantUtf8(Constants::METHOD_PARAMETERS.ref());
}

int MethodWriter::computeMethodInfoSize()
{
    int  size                  = 8;
    bool useSyntheticAttribute = (classWriter_.version() & 0xFFFF) < (Opcodes::V1_5 & 0xFFFF);
    if (code_.size() > 0) {
        if (code_.size() > 0xFFFF) {
            throw MethodTooLargeException(classWriter_.getClassName().to_std_string(),
                                          nameStr_.to_std_string(),
                                          descStr_.to_std_string(),
                                          code_.size());
        }
        symbols_.addConstantUtf8(Constants::CODE.ref());
        size += 16 + code_.size() + Handler::getExceptionTableSize(firstHandler_);
        if (stackMapTableEntries_.size() > 0) {
            symbols_.addConstantUtf8(Constants::STACK_MAP_TABLE.ref());
            size += 8 + stackMapTableEntries_.size();
        }
        if (lineNumberTable_.size() > 0) {
            symbols_.addConstantUtf8(Constants::LINE_NUMBER_TABLE.ref());
            size += 8 + lineNumberTable_.size();
        }
        if (localVariableTable_.size() > 0) {
            symbols_.addConstantUtf8(Constants::LOCAL_VARIABLE_TABLE.ref());
            size += 8 + localVariableTable_.size();
        }
        if (localVariableTypeTable_.size() > 0) {
            symbols_.addConstantUtf8(Constants::LOCAL_VARIABLE_TYPE_TABLE.ref());
            size += 8 + localVariableTypeTable_.size();
        }
        if (!codeVisibleTypeAnnotations_.empty()) {
            symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_TYPE_ANNOTATIONS.ref());
            ByteVector tmp;
            AnnotationWriter::putTypeAnnotations(tmp, codeVisibleTypeAnnotations_);
            size += 6 + tmp.size();
        }
        if (!codeInvisibleTypeAnnotations_.empty()) {
            symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_TYPE_ANNOTATIONS.ref());
            ByteVector tmp;
            AnnotationWriter::putTypeAnnotations(tmp, codeInvisibleTypeAnnotations_);
            size += 6 + tmp.size();
        }
    }
    if (!exceptionIndices_.empty()) {
        symbols_.addConstantUtf8(Constants::EXCEPTIONS.ref());
        size += 8 + 2 * static_cast<int>(exceptionIndices_.size());
    }
    if (signatureIndex_ != 0) {
        symbols_.addConstantUtf8(Constants::SIGNATURE.ref());
        size += 8;
    }
    // Synthetic
    if ((access_ & Opcodes::ACC_SYNTHETIC) != 0 && useSyntheticAttribute) {
        symbols_.addConstantUtf8(Constants::SYNTHETIC.ref());
        size += 6;
    }

    // Deprecated
    if ((access_ & Opcodes::ACC_DEPRECATED) != 0) {
        symbols_.addConstantUtf8(Constants::DEPRECATED.ref());
        size += 6;
    }

    // MethodParameters
    if (parametersCount_ > 0) {
        symbols_.addConstantUtf8(Constants::METHOD_PARAMETERS.ref());
        size += 7 + parameters_.size();
    }

    // AnnotationDefault
    if (annotationDefault_ != nullptr) {
        symbols_.addConstantUtf8(ZUtf8StringRef::from_static("AnnotationDefault"));
        ByteVector tmp;
        annotationDefault_->putAnnotationDefault(tmp);
        size += 6 + tmp.size();
    }

    // Annotations
    if (!visibleAnnotations_.empty()) {
        symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_ANNOTATIONS.ref());
        ByteVector tmp;
        AnnotationWriter::putAnnotations(tmp, visibleAnnotations_);
        size += 6 + tmp.size();
    }
    if (!invisibleAnnotations_.empty()) {
        symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_ANNOTATIONS.ref());
        ByteVector tmp;
        AnnotationWriter::putAnnotations(tmp, invisibleAnnotations_);
        size += 6 + tmp.size();
    }
    if (!visibleTypeAnnotations_.empty()) {
        symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_TYPE_ANNOTATIONS.ref());
        ByteVector tmp;
        AnnotationWriter::putTypeAnnotations(tmp, visibleTypeAnnotations_);
        size += 6 + tmp.size();
    }
    if (!invisibleTypeAnnotations_.empty()) {
        symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_TYPE_ANNOTATIONS.ref());
        ByteVector tmp;
        AnnotationWriter::putTypeAnnotations(tmp, invisibleTypeAnnotations_);
        size += 6 + tmp.size();
    }
    if (!visibleParameterAnnotations_.empty()) {
        symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_PARAMETER_ANNOTATIONS.ref());
        ByteVector tmp;
        int        maxParam = 0;
        for (auto& [param, ann] : visibleParameterAnnotations_)
            if (param > maxParam) maxParam = param;
        int                                         numParams = maxParam + 1;
        std::vector<std::vector<AnnotationWriter*>> paramAnns(numParams);
        for (auto& [param, ann] : visibleParameterAnnotations_)
            if (param < numParams) paramAnns[param].push_back(ann);
        tmp.putByte(static_cast<ca::u8>(numParams));
        for (int i = 0; i < numParams; i++) AnnotationWriter::putAnnotations(tmp, paramAnns[i]);
        size += 6 + tmp.size();
    }
    if (!invisibleParameterAnnotations_.empty()) {
        symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_PARAMETER_ANNOTATIONS.ref());
        ByteVector tmp;
        int        maxParam = 0;
        for (auto& [param, ann] : invisibleParameterAnnotations_)
            if (param > maxParam) maxParam = param;
        int                                         numParams = maxParam + 1;
        std::vector<std::vector<AnnotationWriter*>> paramAnns(numParams);
        for (auto& [param, ann] : invisibleParameterAnnotations_)
            if (param < numParams) paramAnns[param].push_back(ann);
        tmp.putByte(static_cast<ca::u8>(numParams));
        for (int i = 0; i < numParams; i++) AnnotationWriter::putAnnotations(tmp, paramAnns[i]);
        size += 6 + tmp.size();
    }
    for (const auto& attr : attrs_) {
        symbols_.addConstantUtf8(attr.type.ref());
        size += 6 + static_cast<int>(attr.value.len());
    }
    return size;
}

void MethodWriter::putMethodInfo(ByteVector& out)
{
    bool useSyntheticAttribute = (classWriter_.version() & 0xFFFF) < (Opcodes::V1_5 & 0xFFFF);
    int  accessFlags           = access_ & ~Opcodes::ACC_DEPRECATED;
    if (useSyntheticAttribute) accessFlags &= ~Opcodes::ACC_SYNTHETIC;
    out.putShort(accessFlags).putShort(nameIndex_).putShort(descIndex_);

    int attributeCount = 0;
    if (code_.size() > 0) ++attributeCount;
    if (!exceptionIndices_.empty()) ++attributeCount;
    if (signatureIndex_ != 0) ++attributeCount;
    if ((access_ & Opcodes::ACC_SYNTHETIC) != 0 && useSyntheticAttribute) ++attributeCount;
    if ((access_ & Opcodes::ACC_DEPRECATED) != 0) ++attributeCount;
    if (parametersCount_ > 0) ++attributeCount;
    if (annotationDefault_ != nullptr) ++attributeCount;
    if (!visibleAnnotations_.empty()) ++attributeCount;
    if (!invisibleAnnotations_.empty()) ++attributeCount;
    if (!visibleTypeAnnotations_.empty()) ++attributeCount;
    if (!invisibleTypeAnnotations_.empty()) ++attributeCount;
    if (!visibleParameterAnnotations_.empty()) ++attributeCount;
    if (!invisibleParameterAnnotations_.empty()) ++attributeCount;
    attributeCount += static_cast<int>(attrs_.size());

    out.putShort(attributeCount);

    // Code attribute
    if (code_.size() > 0) {
        int size               = 10 + code_.size() + Handler::getExceptionTableSize(firstHandler_);
        int codeAttributeCount = 0;
        if (stackMapTableEntries_.size() > 0) {
            size += 8 + stackMapTableEntries_.size();
            ++codeAttributeCount;
        }
        if (lineNumberTable_.size() > 0) {
            size += 8 + lineNumberTable_.size();
            ++codeAttributeCount;
        }
        if (localVariableTable_.size() > 0) {
            size += 8 + localVariableTable_.size();
            ++codeAttributeCount;
        }
        if (localVariableTypeTable_.size() > 0) {
            size += 8 + localVariableTypeTable_.size();
            ++codeAttributeCount;
        }
        if (!codeVisibleTypeAnnotations_.empty()) {
            ByteVector tmp;
            AnnotationWriter::putTypeAnnotations(tmp, codeVisibleTypeAnnotations_);
            size += 6 + tmp.size();
            ++codeAttributeCount;
        }
        if (!codeInvisibleTypeAnnotations_.empty()) {
            ByteVector tmp;
            AnnotationWriter::putTypeAnnotations(tmp, codeInvisibleTypeAnnotations_);
            size += 6 + tmp.size();
            ++codeAttributeCount;
        }

        out.putShort(symbols_.addConstantUtf8(Constants::CODE.ref()))
            .putInt(size)
            .putShort(maxStack_)
            .putShort(maxLocals_)
            .putInt(code_.size())
            .putByteArray(code_.data(), 0, code_.size());

        Handler::putExceptionTable(firstHandler_, out);
        out.putShort(codeAttributeCount);

        if (stackMapTableEntries_.size() > 0) {
            out.putShort(symbols_.addConstantUtf8(Constants::STACK_MAP_TABLE.ref()))
                .putInt(2 + stackMapTableEntries_.size())
                .putShort(stackMapTableNumberOfEntries_)
                .putByteArray(stackMapTableEntries_.data(), 0, stackMapTableEntries_.size());
        }
        if (lineNumberTable_.size() > 0) {
            out.putShort(symbols_.addConstantUtf8(Constants::LINE_NUMBER_TABLE.ref()))
                .putInt(2 + lineNumberTable_.size())
                .putShort(lineNumberTableLength_)
                .putByteArray(lineNumberTable_.data(), 0, lineNumberTable_.size());
        }
        if (localVariableTable_.size() > 0) {
            out.putShort(symbols_.addConstantUtf8(Constants::LOCAL_VARIABLE_TABLE.ref()))
                .putInt(2 + localVariableTable_.size())
                .putShort(localVariableTableLength_)
                .putByteArray(localVariableTable_.data(), 0, localVariableTable_.size());
        }
        if (localVariableTypeTable_.size() > 0) {
            out.putShort(symbols_.addConstantUtf8(Constants::LOCAL_VARIABLE_TYPE_TABLE.ref()))
                .putInt(2 + localVariableTypeTable_.size())
                .putShort(localVariableTypeTableLength_)
                .putByteArray(localVariableTypeTable_.data(), 0, localVariableTypeTable_.size());
        }
        if (!codeVisibleTypeAnnotations_.empty()) {
            ByteVector tmp;
            AnnotationWriter::putTypeAnnotations(tmp, codeVisibleTypeAnnotations_);
            out.putShort(symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_TYPE_ANNOTATIONS.ref()))
                .putInt(tmp.size())
                .putByteArray(tmp.data(), 0, tmp.size());
        }
        if (!codeInvisibleTypeAnnotations_.empty()) {
            ByteVector tmp;
            AnnotationWriter::putTypeAnnotations(tmp, codeInvisibleTypeAnnotations_);
            out.putShort(
                  symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_TYPE_ANNOTATIONS.ref()))
                .putInt(tmp.size())
                .putByteArray(tmp.data(), 0, tmp.size());
        }
    }

    // Exceptions
    if (!exceptionIndices_.empty()) {
        out.putShort(symbols_.addConstantUtf8(Constants::EXCEPTIONS.ref()))
            .putInt(2 + 2 * static_cast<int>(exceptionIndices_.size()))
            .putShort(static_cast<int>(exceptionIndices_.size()));
        for (int idx : exceptionIndices_) out.putShort(idx);
    }

    // Signature
    if (signatureIndex_ != 0) {
        out.putShort(symbols_.addConstantUtf8(Constants::SIGNATURE.ref()))
            .putInt(2)
            .putShort(signatureIndex_);
    }

    // Synthetic
    if ((access_ & Opcodes::ACC_SYNTHETIC) != 0 && useSyntheticAttribute) {
        out.putShort(symbols_.addConstantUtf8(Constants::SYNTHETIC.ref())).putInt(0);
    }

    // Deprecated
    if ((access_ & Opcodes::ACC_DEPRECATED) != 0) {
        out.putShort(symbols_.addConstantUtf8(Constants::DEPRECATED.ref())).putInt(0);
    }

    // MethodParameters
    if (parametersCount_ > 0) {
        out.putShort(symbols_.addConstantUtf8(Constants::METHOD_PARAMETERS.ref()))
            .putInt(1 + parameters_.size())
            .putByte(parametersCount_)
            .putByteArray(parameters_.data(), 0, parameters_.size());
    }

    // AnnotationDefault
    if (annotationDefault_ != nullptr) {
        ByteVector tmp;
        annotationDefault_->putAnnotationDefault(tmp);
        out.putShort(symbols_.addConstantUtf8(ZUtf8StringRef::from_static("AnnotationDefault")))
            .putInt(tmp.size())
            .putByteArray(tmp.data(), 0, tmp.size());
    }

    // RuntimeVisibleAnnotations
    if (!visibleAnnotations_.empty()) {
        int        nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_ANNOTATIONS.ref());
        ByteVector tmp;
        AnnotationWriter::putAnnotations(tmp, visibleAnnotations_);
        out.putShort(nameIdx).putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }

    // RuntimeInvisibleAnnotations
    if (!invisibleAnnotations_.empty()) {
        int nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_ANNOTATIONS.ref());
        ByteVector tmp;
        AnnotationWriter::putAnnotations(tmp, invisibleAnnotations_);
        out.putShort(nameIdx).putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }

    // RuntimeVisibleTypeAnnotations
    if (!visibleTypeAnnotations_.empty()) {
        int nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_TYPE_ANNOTATIONS.ref());
        ByteVector tmp;
        AnnotationWriter::putTypeAnnotations(tmp, visibleTypeAnnotations_);
        out.putShort(nameIdx).putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }

    // RuntimeInvisibleTypeAnnotations
    if (!invisibleTypeAnnotations_.empty()) {
        int nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_TYPE_ANNOTATIONS.ref());
        ByteVector tmp;
        AnnotationWriter::putTypeAnnotations(tmp, invisibleTypeAnnotations_);
        out.putShort(nameIdx).putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }

    // Parameter annotations (grouped by parameter index)
    if (!visibleParameterAnnotations_.empty()) {
        int nameIdx =
            symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_PARAMETER_ANNOTATIONS.ref());
        ByteVector tmp;
        int        maxParam = 0;
        for (auto& [param, ann] : visibleParameterAnnotations_)
            if (param > maxParam) maxParam = param;
        int                                         numParams = maxParam + 1;
        std::vector<std::vector<AnnotationWriter*>> paramAnns(numParams);
        for (auto& [param, ann] : visibleParameterAnnotations_)
            if (param < numParams) paramAnns[param].push_back(ann);
        tmp.putByte(static_cast<ca::u8>(numParams));
        for (int i = 0; i < numParams; i++) AnnotationWriter::putAnnotations(tmp, paramAnns[i]);
        out.putShort(nameIdx).putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }

    if (!invisibleParameterAnnotations_.empty()) {
        int nameIdx =
            symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_PARAMETER_ANNOTATIONS.ref());
        ByteVector tmp;
        int        maxParam = 0;
        for (auto& [param, ann] : invisibleParameterAnnotations_)
            if (param > maxParam) maxParam = param;
        int                                         numParams = maxParam + 1;
        std::vector<std::vector<AnnotationWriter*>> paramAnns(numParams);
        for (auto& [param, ann] : invisibleParameterAnnotations_)
            if (param < numParams) paramAnns[param].push_back(ann);
        tmp.putByte(static_cast<ca::u8>(numParams));
        for (int i = 0; i < numParams; i++) AnnotationWriter::putAnnotations(tmp, paramAnns[i]);
        out.putShort(nameIdx).putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }

    for (const auto& attr : attrs_) {
        int nameIdx = symbols_.addConstantUtf8(attr.type.ref());
        out.putShort(nameIdx);
        out.putInt(static_cast<ca::i32>(attr.value.len()));
        if (!attr.value.is_empty()) {
            out.putByteArray(attr.value.as_ptr(), 0, static_cast<ca::i32>(attr.value.len()));
        }
    }
}

}   // namespace jbytecode::core
