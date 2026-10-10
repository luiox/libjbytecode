#include "jbytecode/util/check_adapters.hpp"
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/opcodes.hpp"
#include <cctype>

using namespace jbytecode::util;
using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;

// ============================================================
// Validation Helpers
// ============================================================

namespace {

bool isValidDescriptor(ca::str::Utf8StringRef desc)
{
    if (desc.is_empty()) return false;
    ca::usize i = 0;
    while (i < desc.byte_length() && desc.byte_at(i) == '[') i++;
    if (i >= desc.byte_length()) return false;
    char c = static_cast<char>(desc.byte_at(i));
    if (c == 'L') {
        if (desc.byte_at(desc.byte_length() - 1) != ';') return false;
        for (ca::usize j = i + 1; j < desc.byte_length() - 1; ++j) {
            char cc = static_cast<char>(desc.byte_at(j));
            if (cc == ';' || cc == '(' || cc == ')') return false;
        }
        return true;
    }
    return c == 'B' || c == 'C' || c == 'D' || c == 'F' || c == 'I' || c == 'J' || c == 'S' ||
           c == 'Z' || c == 'V';
}

bool isValidMethodDescriptor(ca::str::Utf8StringRef desc)
{
    if (desc.is_empty() || desc.byte_at(0) != '(') return false;
    ca::usize end = desc.byte_length();
    for (ca::usize i = 0; i < desc.byte_length(); ++i) {
        if (desc.byte_at(i) == ')') {
            end = i;
            break;
        }
    }
    if (end >= desc.byte_length()) return false;
    auto retDesc = desc.slice(end + 1, desc.byte_length());
    return isValidDescriptor(retDesc);
}

void validateAccessFlags(int access, int allowed)
{
    if ((access & ~allowed) != 0) {
        throw std::invalid_argument("Invalid access flags: 0x" + std::to_string(access));
    }
}

}   // namespace

// ============================================================
// CheckAnnotationAdapter
// ============================================================

CheckAnnotationAdapter::CheckAnnotationAdapter(AnnotationVisitor* av)
    : AnnotationVisitor(Opcodes::ASM4)
    , av_(av)
{}

void CheckAnnotationAdapter::checkEnd()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
}

void CheckAnnotationAdapter::visit(ca::str::Utf8StringRef name, const ca::core::Any& value)
{
    checkEnd();
    if (av_) av_->visit(name, value);
}

void CheckAnnotationAdapter::visitEnum(ca::str::Utf8StringRef name,
                                       ca::str::Utf8StringRef descriptor,
                                       ca::str::Utf8StringRef value)
{
    checkEnd();
    if (!isValidDescriptor(descriptor)) throw std::invalid_argument("Invalid enum descriptor");
    if (av_) av_->visitEnum(name, descriptor, value);
}

AnnotationVisitor* CheckAnnotationAdapter::visitAnnotation(ca::str::Utf8StringRef name,
                                                           ca::str::Utf8StringRef descriptor)
{
    checkEnd();
    if (!isValidDescriptor(descriptor))
        throw std::invalid_argument("Invalid annotation descriptor");
    AnnotationVisitor* next = av_ ? av_->visitAnnotation(name, descriptor) : nullptr;
    return new CheckAnnotationAdapter(next);
}

AnnotationVisitor* CheckAnnotationAdapter::visitArray(ca::str::Utf8StringRef name)
{
    checkEnd();
    AnnotationVisitor* next = av_ ? av_->visitArray(name) : nullptr;
    return new CheckAnnotationAdapter(next);
}

void CheckAnnotationAdapter::visitEnd()
{
    checkEnd();
    endCalled_ = true;
    if (av_) av_->visitEnd();
}

// ============================================================
// CheckFieldAdapter
// ============================================================

CheckFieldAdapter::CheckFieldAdapter(FieldVisitor* fv)
    : FieldVisitor(Opcodes::ASM4)
    , fv_(fv)
{}

void CheckFieldAdapter::checkEnd()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
}

AnnotationVisitor* CheckFieldAdapter::visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                      bool                   visible)
{
    checkEnd();
    if (!isValidDescriptor(descriptor)) throw std::invalid_argument("Invalid descriptor");
    AnnotationVisitor* next = fv_ ? fv_->visitAnnotation(descriptor, visible) : nullptr;
    return new CheckAnnotationAdapter(next);
}

AnnotationVisitor* CheckFieldAdapter::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                          ca::str::Utf8StringRef descriptor,
                                                          bool                   visible)
{
    checkEnd();
    if (!isValidDescriptor(descriptor)) throw std::invalid_argument("Invalid descriptor");
    AnnotationVisitor* next =
        fv_ ? fv_->visitTypeAnnotation(typeRef, typePath, descriptor, visible) : nullptr;
    return new CheckAnnotationAdapter(next);
}

void CheckFieldAdapter::visitAttribute(const Attribute&)
{
    checkEnd();
}
void CheckFieldAdapter::visitEnd()
{
    checkEnd();
    endCalled_ = true;
    if (fv_) fv_->visitEnd();
}

// ============================================================
// CheckModuleAdapter
// ============================================================

CheckModuleAdapter::CheckModuleAdapter(ModuleVisitor* mv)
    : ModuleVisitor(Opcodes::ASM4)
    , mv_(mv)
{}

void CheckModuleAdapter::checkEnd()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
}

void CheckModuleAdapter::visitMainClass(ca::str::Utf8StringRef mainClass)
{
    checkEnd();
    if (mainClassSet_) throw std::invalid_argument("visitMainClass already called");
    mainClassSet_ = true;
    if (mv_) mv_->visitMainClass(mainClass);
}

void CheckModuleAdapter::visitPackage(ca::str::Utf8StringRef packaze)
{
    checkEnd();
    if (packageSet_) throw std::invalid_argument("visitPackage already called");
    packageSet_ = true;
    if (mv_) mv_->visitPackage(packaze);
}

void CheckModuleAdapter::visitRequire(ca::str::Utf8StringRef module, int access,
                                      ca::str::Utf8StringRef version)
{
    checkEnd();
    validateAccessFlags(access,
                        Opcodes::ACC_STATIC | Opcodes::ACC_SYNTHETIC | Opcodes::ACC_MANDATED |
                            Opcodes::ACC_TRANSITIVE | Opcodes::ACC_STATIC_PHASE);
    if (mv_) mv_->visitRequire(module, access, version);
}

void CheckModuleAdapter::visitExport(ca::str::Utf8StringRef packaze, int access,
                                     const std::vector<ca::str::Utf8StringRef>& modules)
{
    checkEnd();
    validateAccessFlags(access, Opcodes::ACC_SYNTHETIC | Opcodes::ACC_MANDATED);
    if (mv_) mv_->visitExport(packaze, access, modules);
}

void CheckModuleAdapter::visitOpen(ca::str::Utf8StringRef packaze, int access,
                                   const std::vector<ca::str::Utf8StringRef>& modules)
{
    checkEnd();
    validateAccessFlags(access, Opcodes::ACC_SYNTHETIC | Opcodes::ACC_MANDATED);
    if (mv_) mv_->visitOpen(packaze, access, modules);
}

void CheckModuleAdapter::visitUse(ca::str::Utf8StringRef service)
{
    checkEnd();
    if (mv_) mv_->visitUse(service);
}
void CheckModuleAdapter::visitProvide(ca::str::Utf8StringRef                     service,
                                      const std::vector<ca::str::Utf8StringRef>& providers)
{
    checkEnd();
    if (mv_) mv_->visitProvide(service, providers);
}
void CheckModuleAdapter::visitEnd()
{
    checkEnd();
    endCalled_ = true;
    if (mv_) mv_->visitEnd();
}

// ============================================================
// CheckRecordComponentAdapter
// ============================================================

CheckRecordComponentAdapter::CheckRecordComponentAdapter(RecordComponentVisitor* rcv)
    : RecordComponentVisitor(Opcodes::ASM4)
    , rcv_(rcv)
{}

void CheckRecordComponentAdapter::checkEnd()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
}

AnnotationVisitor* CheckRecordComponentAdapter::visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                                bool                   visible)
{
    checkEnd();
    AnnotationVisitor* next = rcv_ ? rcv_->visitAnnotation(descriptor, visible) : nullptr;
    return new CheckAnnotationAdapter(next);
}

AnnotationVisitor* CheckRecordComponentAdapter::visitTypeAnnotation(
    int typeRef, const TypePath* typePath, ca::str::Utf8StringRef descriptor, bool visible)
{
    checkEnd();
    AnnotationVisitor* next =
        rcv_ ? rcv_->visitTypeAnnotation(typeRef, typePath, descriptor, visible) : nullptr;
    return new CheckAnnotationAdapter(next);
}

void CheckRecordComponentAdapter::visitAttribute(const Attribute&)
{
    checkEnd();
}
void CheckRecordComponentAdapter::visitEnd()
{
    checkEnd();
    endCalled_ = true;
    if (rcv_) rcv_->visitEnd();
}

// ============================================================
// CheckSignatureAdapter
// ============================================================

CheckSignatureAdapter::CheckSignatureAdapter(int type, SignatureVisitor* sv)
    : SignatureVisitor(Opcodes::ASM4)
    , sv_(sv)
    , type_(type)
    , state_(S_START)
{}

void CheckSignatureAdapter::visitFormalTypeParameter(ca::str::Utf8StringRef name)
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    state_ = S_FORMAL;
    if (sv_) sv_->visitFormalTypeParameter(name);
}

SignatureVisitor* CheckSignatureAdapter::visitClassBound()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    SignatureVisitor* result = sv_ ? sv_->visitClassBound() : nullptr;
    return new CheckSignatureAdapter(type_, result);
}

SignatureVisitor* CheckSignatureAdapter::visitInterfaceBound()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    SignatureVisitor* result = sv_ ? sv_->visitInterfaceBound() : nullptr;
    return new CheckSignatureAdapter(type_, result);
}

SignatureVisitor* CheckSignatureAdapter::visitSuperclass()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    if (type_ != CLASS_SIGNATURE)
        throw std::invalid_argument("visitSuperclass requires CLASS_SIGNATURE");
    SignatureVisitor* result = sv_ ? sv_->visitSuperclass() : nullptr;
    return new CheckSignatureAdapter(type_, result);
}

SignatureVisitor* CheckSignatureAdapter::visitInterface()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    SignatureVisitor* result = sv_ ? sv_->visitInterface() : nullptr;
    return new CheckSignatureAdapter(type_, result);
}

SignatureVisitor* CheckSignatureAdapter::visitParameterType()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    if (type_ != METHOD_SIGNATURE)
        throw std::invalid_argument("visitParameterType requires METHOD_SIGNATURE");
    SignatureVisitor* result = sv_ ? sv_->visitParameterType() : nullptr;
    return new CheckSignatureAdapter(type_, result);
}

SignatureVisitor* CheckSignatureAdapter::visitReturnType()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    SignatureVisitor* result = sv_ ? sv_->visitReturnType() : nullptr;
    return new CheckSignatureAdapter(type_, result);
}

SignatureVisitor* CheckSignatureAdapter::visitExceptionType()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    SignatureVisitor* result = sv_ ? sv_->visitExceptionType() : nullptr;
    return new CheckSignatureAdapter(type_, result);
}

void CheckSignatureAdapter::visitBaseType(char c)
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    if (sv_) sv_->visitBaseType(c);
}

void CheckSignatureAdapter::visitTypeVariable(ca::str::Utf8StringRef name)
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    if (sv_) sv_->visitTypeVariable(name);
}

SignatureVisitor* CheckSignatureAdapter::visitArrayType()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    SignatureVisitor* result = sv_ ? sv_->visitArrayType() : nullptr;
    return new CheckSignatureAdapter(type_, result);
}

void CheckSignatureAdapter::visitClassType(ca::str::Utf8StringRef name)
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    state_ = S_FIELD;
    if (sv_) sv_->visitClassType(name);
}

void CheckSignatureAdapter::visitInnerClassType(ca::str::Utf8StringRef name)
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    if (sv_) sv_->visitInnerClassType(name);
}

void CheckSignatureAdapter::visitTypeArgument()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    if (sv_) sv_->visitTypeArgument();
}

SignatureVisitor* CheckSignatureAdapter::visitTypeArgument(char wildcard)
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
    if (sv_) {
        SignatureVisitor* next = sv_->visitTypeArgument(wildcard);
        return new CheckSignatureAdapter(type_, next);
    }
    return this;
}

void CheckSignatureAdapter::visitEnd()
{
    if (endCalled_) throw std::invalid_argument("visitEnd called twice");
    endCalled_ = true;
    if (sv_) sv_->visitEnd();
}

// ============================================================
// CheckMethodAdapter
// ============================================================

CheckMethodAdapter::CheckMethodAdapter(MethodVisitor* mv)
    : MethodVisitor(Opcodes::ASM4)
    , mv_(mv)
{}

void CheckMethodAdapter::checkEnd()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
}
void CheckMethodAdapter::checkCodeStarted()
{
    if (!codeStarted_) throw std::invalid_argument("visitCode not called yet");
}
bool CheckMethodAdapter::isValidOpcode(int opcode)
{
    return opcode >= 0 && opcode <= 201 && opcode != 186;
}

AnnotationVisitor* CheckMethodAdapter::visitAnnotationDefault()
{
    checkEnd();
    AnnotationVisitor* next = mv_ ? mv_->visitAnnotationDefault() : nullptr;
    return new CheckAnnotationAdapter(next);
}

AnnotationVisitor* CheckMethodAdapter::visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                       bool                   visible)
{
    checkEnd();
    if (!isValidDescriptor(descriptor)) throw std::invalid_argument("Invalid descriptor");
    AnnotationVisitor* next = mv_ ? mv_->visitAnnotation(descriptor, visible) : nullptr;
    return new CheckAnnotationAdapter(next);
}

AnnotationVisitor* CheckMethodAdapter::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                           ca::str::Utf8StringRef descriptor,
                                                           bool                   visible)
{
    checkEnd();
    AnnotationVisitor* next =
        mv_ ? mv_->visitTypeAnnotation(typeRef, typePath, descriptor, visible) : nullptr;
    return new CheckAnnotationAdapter(next);
}

AnnotationVisitor* CheckMethodAdapter::visitParameterAnnotation(int                    parameter,
                                                                ca::str::Utf8StringRef descriptor,
                                                                bool                   visible)
{
    checkEnd();
    if (!isValidDescriptor(descriptor)) throw std::invalid_argument("Invalid descriptor");
    AnnotationVisitor* next =
        mv_ ? mv_->visitParameterAnnotation(parameter, descriptor, visible) : nullptr;
    return new CheckAnnotationAdapter(next);
}

void CheckMethodAdapter::visitCode()
{
    checkEnd();
    codeStarted_ = true;
    if (mv_) mv_->visitCode();
}
void CheckMethodAdapter::visitFrame(int type, int numLocal, const std::vector<ca::core::Any>& local,
                                    int numStack, const std::vector<ca::core::Any>& stack)
{
    checkEnd();
    checkCodeStarted();
    if (type < 0 || type > 5) throw std::invalid_argument("Invalid frame type");
    if (mv_) mv_->visitFrame(type, numLocal, local, numStack, stack);
}
void CheckMethodAdapter::visitInsn(int opcode)
{
    checkEnd();
    checkCodeStarted();
    if (!isValidOpcode(opcode)) throw std::invalid_argument("Invalid opcode");
    if (mv_) mv_->visitInsn(opcode);
}
void CheckMethodAdapter::visitIntInsn(int opcode, int operand)
{
    checkEnd();
    checkCodeStarted();
    if (!isValidOpcode(opcode)) throw std::invalid_argument("Invalid opcode");
    if (mv_) mv_->visitIntInsn(opcode, operand);
}
void CheckMethodAdapter::visitVarInsn(int opcode, int varIndex)
{
    checkEnd();
    checkCodeStarted();
    if (!isValidOpcode(opcode)) throw std::invalid_argument("Invalid opcode");
    if (varIndex < 0) throw std::invalid_argument("Negative var index");
    if (mv_) mv_->visitVarInsn(opcode, varIndex);
}
void CheckMethodAdapter::visitTypeInsn(int opcode, ca::str::Utf8StringRef type)
{
    checkEnd();
    checkCodeStarted();
    if (!isValidOpcode(opcode)) throw std::invalid_argument("Invalid opcode");
    if (type.is_empty()) throw std::invalid_argument("Empty type");
    if (mv_) mv_->visitTypeInsn(opcode, type);
}
void CheckMethodAdapter::visitFieldInsn(int opcode, ca::str::Utf8StringRef owner,
                                        ca::str::Utf8StringRef name,
                                        ca::str::Utf8StringRef descriptor)
{
    checkEnd();
    checkCodeStarted();
    if (!isValidOpcode(opcode)) throw std::invalid_argument("Invalid opcode");
    if (owner.is_empty() || name.is_empty()) throw std::invalid_argument("Empty owner/name");
    if (!isValidDescriptor(descriptor)) throw std::invalid_argument("Invalid descriptor");
    if (mv_) mv_->visitFieldInsn(opcode, owner, name, descriptor);
}
void CheckMethodAdapter::visitMethodInsn(int opcode, ca::str::Utf8StringRef owner,
                                         ca::str::Utf8StringRef name,
                                         ca::str::Utf8StringRef descriptor, bool isInterface)
{
    checkEnd();
    checkCodeStarted();
    if (!isValidOpcode(opcode)) throw std::invalid_argument("Invalid opcode");
    if (owner.is_empty() || name.is_empty()) throw std::invalid_argument("Empty owner/name");
    if (!isValidMethodDescriptor(descriptor))
        throw std::invalid_argument("Invalid method descriptor");
    if (mv_) mv_->visitMethodInsn(opcode, owner, name, descriptor, isInterface);
}
void CheckMethodAdapter::visitInvokeDynamicInsn(
    ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
    const Handle& bootstrapMethodHandle, const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    checkEnd();
    checkCodeStarted();
    if (name.is_empty()) throw std::invalid_argument("Empty name");
    if (!isValidMethodDescriptor(descriptor)) throw std::invalid_argument("Invalid descriptor");
    if (mv_)
        mv_->visitInvokeDynamicInsn(
            name, descriptor, bootstrapMethodHandle, bootstrapMethodArguments);
}
void CheckMethodAdapter::visitJumpInsn(int opcode, Label* label)
{
    checkEnd();
    checkCodeStarted();
    if (!isValidOpcode(opcode)) throw std::invalid_argument("Invalid opcode");
    if (label == nullptr) throw std::invalid_argument("Null label");
    if (mv_) mv_->visitJumpInsn(opcode, label);
}
void CheckMethodAdapter::visitLabel(Label* label)
{
    checkEnd();
    checkCodeStarted();
    if (label == nullptr) throw std::invalid_argument("Null label");
    if (mv_) mv_->visitLabel(label);
}
void CheckMethodAdapter::visitLdcInsn(const ca::core::Any& value)
{
    checkEnd();
    checkCodeStarted();
    if (!value.has_value()) throw std::invalid_argument("Empty LDC value");
    if (mv_) mv_->visitLdcInsn(value);
}
void CheckMethodAdapter::visitIincInsn(int varIndex, int increment)
{
    checkEnd();
    checkCodeStarted();
    if (mv_) mv_->visitIincInsn(varIndex, increment);
}
void CheckMethodAdapter::visitTableSwitchInsn(int min, int max, Label* dflt,
                                              const std::vector<Label*>& labels)
{
    checkEnd();
    checkCodeStarted();
    if (dflt == nullptr) throw std::invalid_argument("Null default label");
    if (mv_) mv_->visitTableSwitchInsn(min, max, dflt, labels);
}
void CheckMethodAdapter::visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                                               const std::vector<Label*>& labels)
{
    checkEnd();
    checkCodeStarted();
    if (dflt == nullptr) throw std::invalid_argument("Null default label");
    if (mv_) mv_->visitLookupSwitchInsn(dflt, keys, labels);
}
void CheckMethodAdapter::visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor,
                                                 int                    numDimensions)
{
    checkEnd();
    checkCodeStarted();
    if (descriptor.is_empty()) throw std::invalid_argument("Empty descriptor");
    if (numDimensions < 1) throw std::invalid_argument("Invalid dimensions");
    if (mv_) mv_->visitMultiANewArrayInsn(descriptor, numDimensions);
}
AnnotationVisitor* CheckMethodAdapter::visitInsnAnnotation(int typeRef, const TypePath* typePath,
                                                           ca::str::Utf8StringRef descriptor,
                                                           bool                   visible)
{
    checkEnd();
    checkCodeStarted();
    AnnotationVisitor* next =
        mv_ ? mv_->visitInsnAnnotation(typeRef, typePath, descriptor, visible) : nullptr;
    return new CheckAnnotationAdapter(next);
}
void CheckMethodAdapter::visitTryCatchBlock(Label* start, Label* end, Label* handler,
                                            ca::str::Utf8StringRef type)
{
    checkEnd();
    checkCodeStarted();
    if (!start || !end || !handler) throw std::invalid_argument("Null label");
    if (mv_) mv_->visitTryCatchBlock(start, end, handler, type);
}
AnnotationVisitor* CheckMethodAdapter::visitTryCatchAnnotation(int                    typeRef,
                                                               const TypePath*        typePath,
                                                               ca::str::Utf8StringRef descriptor,
                                                               bool                   visible)
{
    checkEnd();
    checkCodeStarted();
    AnnotationVisitor* next =
        mv_ ? mv_->visitTryCatchAnnotation(typeRef, typePath, descriptor, visible) : nullptr;
    return new CheckAnnotationAdapter(next);
}
void CheckMethodAdapter::visitLocalVariable(ca::str::Utf8StringRef name,
                                            ca::str::Utf8StringRef descriptor,
                                            ca::str::Utf8StringRef signature, Label* start,
                                            Label* end, int index)
{
    checkEnd();
    checkCodeStarted();
    if (name.is_empty()) throw std::invalid_argument("Empty name");
    if (!isValidDescriptor(descriptor)) throw std::invalid_argument("Invalid descriptor");
    if (mv_) mv_->visitLocalVariable(name, descriptor, signature, start, end, index);
}
AnnotationVisitor* CheckMethodAdapter::visitLocalVariableAnnotation(
    int typeRef, const TypePath* typePath, const std::vector<Label*>& start,
    const std::vector<Label*>& end, const std::vector<int>& index,
    ca::str::Utf8StringRef descriptor, bool visible)
{
    checkEnd();
    checkCodeStarted();
    AnnotationVisitor* next = mv_ ? mv_->visitLocalVariableAnnotation(
                                        typeRef, typePath, start, end, index, descriptor, visible)
                                  : nullptr;
    return new CheckAnnotationAdapter(next);
}
void CheckMethodAdapter::visitLineNumber(int line, Label* start)
{
    checkEnd();
    checkCodeStarted();
    if (start == nullptr) throw std::invalid_argument("Null label");
    if (mv_) mv_->visitLineNumber(line, start);
}
void CheckMethodAdapter::visitMaxs(int maxStack, int maxLocals)
{
    checkEnd();
    if (!codeStarted_) throw std::invalid_argument("visitMaxs before visitCode");
    if (mv_) mv_->visitMaxs(maxStack, maxLocals);
}
void CheckMethodAdapter::visitEnd()
{
    checkEnd();
    endCalled_ = true;
    if (mv_) mv_->visitEnd();
}

// ============================================================
// CheckClassAdapter
// ============================================================

CheckClassAdapter::CheckClassAdapter(ClassVisitor* cv)
    : ClassVisitor(Opcodes::ASM4)
    , cv_(cv)
{}

void CheckClassAdapter::checkVisitCalled()
{
    if (!visitCalled_) throw std::invalid_argument("visit not called yet");
}
void CheckClassAdapter::checkEnd()
{
    if (endCalled_) throw std::invalid_argument("visitEnd already called");
}

void CheckClassAdapter::visit(int version, int access, ca::str::Utf8StringRef name,
                              ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
                              const std::vector<ca::str::Utf8StringRef>& interfaces)
{
    checkEnd();
    if (visitCalled_) throw std::invalid_argument("visit already called");
    if (name.is_empty()) throw std::invalid_argument("Empty class name");
    if (version <= 0) throw std::invalid_argument("Invalid class version");
    visitCalled_ = true;
    if (cv_) cv_->visit(version, access, name, signature, superName, interfaces);
}

void CheckClassAdapter::visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug)
{
    checkEnd();
    checkVisitCalled();
    if (cv_) cv_->visitSource(source, debug);
}
void CheckClassAdapter::visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                        ca::str::Utf8StringRef descriptor)
{
    checkEnd();
    checkVisitCalled();
    if (cv_) cv_->visitOuterClass(owner, name, descriptor);
}
AnnotationVisitor* CheckClassAdapter::visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                      bool                   visible)
{
    checkEnd();
    checkVisitCalled();
    if (!isValidDescriptor(descriptor)) throw std::invalid_argument("Invalid descriptor");
    AnnotationVisitor* next = cv_ ? cv_->visitAnnotation(descriptor, visible) : nullptr;
    return next ? new CheckAnnotationAdapter(next) : nullptr;
}
AnnotationVisitor* CheckClassAdapter::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                          ca::str::Utf8StringRef descriptor,
                                                          bool                   visible)
{
    checkEnd();
    checkVisitCalled();
    AnnotationVisitor* next =
        cv_ ? cv_->visitTypeAnnotation(typeRef, typePath, descriptor, visible) : nullptr;
    return next ? new CheckAnnotationAdapter(next) : nullptr;
}
void CheckClassAdapter::visitAttribute(const Attribute&)
{
    checkEnd();
    checkVisitCalled();
}
void CheckClassAdapter::visitInnerClass(ca::str::Utf8StringRef name,
                                        ca::str::Utf8StringRef outerName,
                                        ca::str::Utf8StringRef innerName, int access)
{
    checkEnd();
    checkVisitCalled();
    if (cv_) cv_->visitInnerClass(name, outerName, innerName, access);
}
void CheckClassAdapter::visitNestHost(ca::str::Utf8StringRef nestHost)
{
    checkEnd();
    checkVisitCalled();
    if (cv_) cv_->visitNestHost(nestHost);
}
void CheckClassAdapter::visitNestMember(ca::str::Utf8StringRef nestMember)
{
    checkEnd();
    checkVisitCalled();
    if (cv_) cv_->visitNestMember(nestMember);
}
void CheckClassAdapter::visitPermittedSubclass(ca::str::Utf8StringRef subclass)
{
    checkEnd();
    checkVisitCalled();
    if (cv_) cv_->visitPermittedSubclass(subclass);
}
ModuleVisitor* CheckClassAdapter::visitModule(ca::str::Utf8StringRef name, int access,
                                              ca::str::Utf8StringRef version)
{
    checkEnd();
    checkVisitCalled();
    ModuleVisitor* next = cv_ ? cv_->visitModule(name, access, version) : nullptr;
    return new CheckModuleAdapter(next);
}
FieldVisitor* CheckClassAdapter::visitField(int access, ca::str::Utf8StringRef name,
                                            ca::str::Utf8StringRef descriptor,
                                            ca::str::Utf8StringRef signature,
                                            const ca::core::Any&   value)
{
    checkEnd();
    checkVisitCalled();
    if (name.is_empty()) throw std::invalid_argument("Empty field name");
    if (!isValidDescriptor(descriptor)) throw std::invalid_argument("Invalid field descriptor");
    FieldVisitor* next =
        cv_ ? cv_->visitField(access, name, descriptor, signature, value) : nullptr;
    return new CheckFieldAdapter(next);
}
MethodVisitor* CheckClassAdapter::visitMethod(int access, ca::str::Utf8StringRef name,
                                              ca::str::Utf8StringRef                     descriptor,
                                              ca::str::Utf8StringRef                     signature,
                                              const std::vector<ca::str::Utf8StringRef>& exceptions)
{
    checkEnd();
    checkVisitCalled();
    if (name.is_empty()) throw std::invalid_argument("Empty method name");
    if (!isValidMethodDescriptor(descriptor))
        throw std::invalid_argument("Invalid method descriptor");
    MethodVisitor* next =
        cv_ ? cv_->visitMethod(access, name, descriptor, signature, exceptions) : nullptr;
    return new CheckMethodAdapter(next);
}
void CheckClassAdapter::visitEnd()
{
    checkEnd();
    if (!visitCalled_) throw std::invalid_argument("visit not called before visitEnd");
    endCalled_ = true;
    if (cv_) cv_->visitEnd();
}

// ============================================================
// CheckFrameAnalyzer
// ============================================================

CheckFrameAnalyzer::CheckFrameAnalyzer(BasicVerifier* verifier)
    : Analyzer<BasicValue>(verifier)
    , interpreter_(verifier)
    , insnList_(nullptr)
    , currentLocals_(0)
{}

std::vector<jbytecode::analysis::Frame<BasicValue>*> CheckFrameAnalyzer::analyze(
    const std::string& owner, MethodNode* method)
{
    if (method == nullptr) throw std::invalid_argument("Null MethodNode");
    return Analyzer<BasicValue>::analyze(
        ca::str::Utf8StringRef(
            reinterpret_cast<const ca::u8*>(owner.data()), owner.size(), owner.size()),
        method);
}

void CheckFrameAnalyzer::analyzeClass(ca::str::Utf8StringRef owner, ClassReader* classReader)
{
    if (classReader == nullptr) throw std::invalid_argument("Null ClassReader");
    auto owner_ = std::string(reinterpret_cast<const char*>(owner.data()), owner.byte_length());
    ClassNode cn;
    classReader->accept(&cn, ClassReader::EXPAND_FRAMES);
    for (auto* method : cn.methods) {
        if ((method->access & (Opcodes::ACC_ABSTRACT | Opcodes::ACC_NATIVE)) != 0) continue;
        analyze(owner_, method);
    }
}

void CheckFrameAnalyzer::init(ca::str::Utf8StringRef owner, MethodNode* method)
{
    insnList_      = &method->instructions;
    int argsSize   = Type::getArgumentsAndReturnSizes(method->desc) >> 2;
    currentLocals_ = ((method->access & Opcodes::ACC_STATIC) != 0) ? argsSize - 1 : argsSize;

    auto&                                  frames = getFrames();
    jbytecode::analysis::Frame<BasicValue> currentFrame(*frames[0]);
    expandFrames(owner, method, currentFrame);

    int insnListSize = insnList_->size();
    for (int insnIndex = 0; insnIndex < insnListSize; ++insnIndex) {
        jbytecode::analysis::Frame<BasicValue>* oldFrame = frames[insnIndex];
        if (oldFrame == nullptr) continue;

        AbstractInsnNode* insnNode = insnList_->get(insnIndex);
        int               insnType = insnNode->getType();

        if (insnType == AbstractInsnNode::LABEL || insnType == AbstractInsnNode::LINE ||
            insnType == AbstractInsnNode::FRAME) {
            checkFrame(insnIndex + 1, *oldFrame, false);
        }
        else {
            currentFrame = *oldFrame;
            int opcode   = insnNode->getOpcode();

            if (insnNode->getType() == AbstractInsnNode::JUMP_INSN) {
                if (opcode == Opcodes::JSR) {
                    throw AnalyzerException(insnNode, "JSR instructions are unsupported");
                }
                auto* jumpInsn  = ca::core::tag_cast::cast<JumpInsnNode*>(insnNode);
                auto* labelNode = findLabelNode(jumpInsn->label);
                if (!labelNode) throw AnalyzerException(insnNode, "Cannot find label node");
                int targetInsnIndex = insnList_->indexOf(labelNode);
                checkFrame(targetInsnIndex, currentFrame, true);
                if (opcode == Opcodes::GOTO) {
                    endControlFlow(insnIndex);
                }
                else {
                    checkFrame(insnIndex + 1, currentFrame, false);
                }
            }
            else if (insnNode->getType() == AbstractInsnNode::LOOKUPSWITCH_INSN) {
                auto* ls              = ca::core::tag_cast::cast<LookupSwitchInsnNode*>(insnNode);
                int   targetInsnIndex = insnList_->indexOf(findLabelNode(ls->dflt));
                checkFrame(targetInsnIndex, currentFrame, true);
                for (auto* label : ls->labels) {
                    targetInsnIndex = insnList_->indexOf(findLabelNode(label));
                    checkFrame(targetInsnIndex, currentFrame, true);
                }
                endControlFlow(insnIndex);
            }
            else if (insnNode->getType() == AbstractInsnNode::TABLESWITCH_INSN) {
                auto* ts              = ca::core::tag_cast::cast<TableSwitchInsnNode*>(insnNode);
                int   targetInsnIndex = insnList_->indexOf(findLabelNode(ts->dflt));
                checkFrame(targetInsnIndex, currentFrame, true);
                for (auto* label : ts->labels) {
                    targetInsnIndex = insnList_->indexOf(findLabelNode(label));
                    checkFrame(targetInsnIndex, currentFrame, true);
                }
                endControlFlow(insnIndex);
            }
            else if (opcode == Opcodes::RET) {
                throw AnalyzerException(insnNode, "RET instructions are unsupported");
            }
            else if (opcode != Opcodes::ATHROW &&
                     (opcode < Opcodes::IRETURN || opcode > Opcodes::RETURN)) {
                checkFrame(insnIndex + 1, currentFrame, false);
            }
            else {
                endControlFlow(insnIndex);
            }
        }

        if (!hasNextJvmInsnOrFrame(insnIndex)) {
            break;
        }
    }
}

void CheckFrameAnalyzer::expandFrames(ca::str::Utf8StringRef owner, MethodNode* method,
                                      jbytecode::analysis::Frame<BasicValue>& initialFrame)
{
    int                                    lastJvmOrFrameInsnIndex = -1;
    jbytecode::analysis::Frame<BasicValue> currentFrame(initialFrame);
    int                                    currentInsnIndex = 0;
    auto&                                  frames           = getFrames();

    for (auto* insnNode : method->instructions) {
        if (insnNode->getType() == AbstractInsnNode::FRAME) {
            auto* fn = ca::core::tag_cast::cast<FrameNode*>(insnNode);
            try {
                currentFrame = expandFrame(owner, currentFrame, fn);
            }
            catch (const AnalyzerException& e) {
                throw AnalyzerException(
                    insnNode,
                    "Error at instruction " + std::to_string(currentInsnIndex) + ": " + e.what());
            }
            for (int index = lastJvmOrFrameInsnIndex + 1; index <= currentInsnIndex; ++index) {
                if (frames[index] == nullptr) {
                    addNewFrame(index);
                }
                *frames[index] = currentFrame;
            }
        }
        if (isJvmInsnNode(insnNode) || insnNode->getType() == AbstractInsnNode::FRAME) {
            lastJvmOrFrameInsnIndex = currentInsnIndex;
        }
        currentInsnIndex += 1;
    }
}

jbytecode::analysis::Frame<BasicValue> CheckFrameAnalyzer::expandFrame(
    ca::str::Utf8StringRef owner, const jbytecode::analysis::Frame<BasicValue>& previousFrame,
    FrameNode* frameNode)
{
    jbytecode::analysis::Frame<BasicValue> frame(previousFrame);
    auto&                                  locals = frameNode->local;
    auto&                                  stack  = frameNode->stack;

    int currentLocal = currentLocals_;
    switch (frameNode->frameType) {
    case Opcodes::F_NEW:
    case Opcodes::F_FULL:
        currentLocal = 0;
        for (auto& type : locals) {
            BasicValue value = newFrameValue(owner, frameNode, type);
            if (currentLocal + value.getSize() > frame.getLocals()) {
                throw AnalyzerException(frameNode, "Cannot append more locals than maxLocals");
            }
            frame.setLocal(currentLocal++, value);
            if (value.getSize() == 2) {
                frame.setLocal(currentLocal++, interpreter_->newValue(nullptr));
            }
        }
        break;
    case Opcodes::F_APPEND:
        for (auto& type : locals) {
            BasicValue value = newFrameValue(owner, frameNode, type);
            if (currentLocal + value.getSize() > frame.getLocals()) {
                throw AnalyzerException(frameNode, "Cannot append more locals than maxLocals");
            }
            frame.setLocal(currentLocal++, value);
            if (value.getSize() == 2) {
                frame.setLocal(currentLocal++, interpreter_->newValue(nullptr));
            }
        }
        break;
    case Opcodes::F_CHOP:
    {
        int chopCount = 251 - frameNode->frameType;
        for (int i = 0; i < chopCount; i++) {
            if (currentLocal <= 0) {
                throw AnalyzerException(frameNode, "Cannot chop more locals than defined");
            }
            if (currentLocal > 1 && frame.getLocal(currentLocal - 2).getSize() == 2) {
                currentLocal -= 2;
            }
            else {
                currentLocal -= 1;
            }
        }
        break;
    }
    case Opcodes::F_SAME:
    case Opcodes::F_SAME1: break;
    default:
        throw AnalyzerException(frameNode,
                                "Illegal frame type " + std::to_string(frameNode->frameType));
    }
    currentLocals_ = currentLocal;
    while (currentLocal < frame.getLocals()) {
        frame.setLocal(currentLocal++, interpreter_->newValue(nullptr));
    }

    frame.clearStack();
    for (auto& type : stack) {
        frame.push(newFrameValue(owner, frameNode, type));
    }
    return frame;
}

BasicValue CheckFrameAnalyzer::newFrameValue(ca::str::Utf8StringRef owner, FrameNode* frameNode,
                                             const ca::core::Any& type)
{
    if (!type.has_value()) {
        return interpreter_->newValue(nullptr);
    }
    if (auto* p = type.as<int>()) {
        int val = *p;
        switch (val) {
        case 0: return interpreter_->newValue(nullptr);
        case 1: return interpreter_->newValue(&Type::INT_TYPE);
        case 2: return interpreter_->newValue(&Type::FLOAT_TYPE);
        case 3: return interpreter_->newValue(&Type::LONG_TYPE);
        case 4: return interpreter_->newValue(&Type::DOUBLE_TYPE);
        case 5:
        {
            InsnNode insn(Opcodes::ACONST_NULL);
            return interpreter_->newOperation(&insn);
        }
        case 6:
        {
            ownedTypes_.push_back(Type::getObjectType(owner));
            return interpreter_->newValue(&ownedTypes_.back());
        }
        default:
        {
            if (val <= 7) {
                throw AnalyzerException(frameNode,
                                        "Illegal stack map frame value " + std::to_string(val));
            }
            return interpreter_->newValue(nullptr);
        }
        }
    }
    if (auto* p = type.as<ca::str::Utf8StringRef>()) {
        ownedTypes_.push_back(Type::getObjectType(*p));
        return interpreter_->newValue(&ownedTypes_.back());
    }
    throw AnalyzerException(frameNode, "Illegal stack map frame value type");
}

void CheckFrameAnalyzer::checkFrame(int                                           insnIndex,
                                    const jbytecode::analysis::Frame<BasicValue>& frame,
                                    bool                                          requireFrame)
{
    if (insnIndex < 0 || insnIndex >= (int)getFrames().size()) return;
    jbytecode::analysis::Frame<BasicValue>* oldFrame = getFrames()[insnIndex];
    if (oldFrame == nullptr) {
        if (requireFrame) {
            throw AnalyzerException(
                nullptr, "Expected stack map frame at instruction " + std::to_string(insnIndex));
        }
        addNewFrame(insnIndex);
        *getFrames()[insnIndex] = frame;
    }
    else {
        std::string error = checkMerge(frame, *oldFrame);
        if (!error.empty()) {
            throw AnalyzerException(nullptr,
                                    "Stack map frame incompatible with frame at instruction " +
                                        std::to_string(insnIndex) + " (" + error + ")");
        }
    }
}

std::string CheckFrameAnalyzer::checkMerge(const jbytecode::analysis::Frame<BasicValue>& srcFrame,
                                           const jbytecode::analysis::Frame<BasicValue>& dstFrame)
{
    int numLocals = srcFrame.getLocals();
    if (numLocals != dstFrame.getLocals()) {
        return "incompatible local counts";
    }
    for (int i = 0; i < numLocals; ++i) {
        BasicValue v = interpreter_->merge(srcFrame.getLocal(i), dstFrame.getLocal(i));
        if (!v.equals(dstFrame.getLocal(i))) {
            return "incompatible types at local " + std::to_string(i);
        }
    }
    int numStack = srcFrame.getStackSize();
    if (numStack != dstFrame.getStackSize()) {
        return "incompatible stack heights";
    }
    for (int i = 0; i < numStack; ++i) {
        BasicValue v = interpreter_->merge(srcFrame.getStack(i), dstFrame.getStack(i));
        if (!v.equals(dstFrame.getStack(i))) {
            return "incompatible types at stack item " + std::to_string(i);
        }
    }
    return "";
}

void CheckFrameAnalyzer::endControlFlow(int insnIndex)
{
    if (hasNextJvmInsnOrFrame(insnIndex) && getFrames()[insnIndex + 1] == nullptr) {
        throw AnalyzerException(
            nullptr, "Expected stack map frame at instruction " + std::to_string(insnIndex + 1));
    }
}

bool CheckFrameAnalyzer::hasNextJvmInsnOrFrame(int insnIndex)
{
    if (insnIndex < 0 || insnIndex + 1 >= (int)getFrames().size()) return false;
    for (int i = insnIndex + 1; i < (int)getFrames().size(); i++) {
        AbstractInsnNode* insn = insnList_->get(i);
        if (isJvmInsnNode(insn) || insn->getType() == AbstractInsnNode::FRAME) {
            return true;
        }
    }
    return false;
}

bool CheckFrameAnalyzer::isJvmInsnNode(AbstractInsnNode* insnNode)
{
    return insnNode->getOpcode() >= 0;
}

void checkClassFrames(ca::str::Utf8StringRef owner, ClassReader* classReader)
{
    if (classReader == nullptr) throw std::invalid_argument("Null ClassReader");
    auto owner_ = std::string(reinterpret_cast<const char*>(owner.data()), owner.byte_length());
    ClassNode cn;
    classReader->accept(&cn, ClassReader::EXPAND_FRAMES);
    BasicVerifier      verifier;
    CheckFrameAnalyzer analyzer(&verifier);
    for (auto* method : cn.methods) {
        if ((method->access & (Opcodes::ACC_ABSTRACT | Opcodes::ACC_NATIVE)) != 0) continue;
        analyzer.analyze(owner_, method);
    }
}
