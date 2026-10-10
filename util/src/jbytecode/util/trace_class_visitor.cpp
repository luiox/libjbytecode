#include "jbytecode/util/trace_class_visitor.hpp"
#include "jbytecode/util/textifier.hpp"

using namespace jbytecode::util;
using namespace jbytecode::core;

// ============================================================
// TraceClassVisitor
// ============================================================

TraceClassVisitor::TraceClassVisitor(ClassVisitor* cv, Printer* printer)
    : ClassVisitor(Opcodes::ASM9)
    , cv_(cv)
    , printer_(printer)
{}

void TraceClassVisitor::visit(int version, int access, ca::str::Utf8StringRef name,
                              ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
                              const std::vector<ca::str::Utf8StringRef>& interfaces)
{
    printer_->visit(version, access, name, signature, superName, interfaces);
    if (cv_) cv_->visit(version, access, name, signature, superName, interfaces);
}

void TraceClassVisitor::visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug)
{
    printer_->visitSource(source, debug);
    if (cv_) cv_->visitSource(source, debug);
}

void TraceClassVisitor::visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                        ca::str::Utf8StringRef descriptor)
{
    printer_->visitOuterClass(owner, name, descriptor);
    if (cv_) cv_->visitOuterClass(owner, name, descriptor);
}

AnnotationVisitor* TraceClassVisitor::visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                      bool                   visible)
{
    printer_->visitClassAnnotation(descriptor, visible);
    AnnotationVisitor* next = cv_ ? cv_->visitAnnotation(descriptor, visible) : nullptr;
    if (next == nullptr) return nullptr;
    return new TraceAnnotationVisitor(next, printer_);
}

AnnotationVisitor* TraceClassVisitor::visitTypeAnnotation(int, const TypePath*,
                                                          ca::str::Utf8StringRef, bool)
{
    return cv_ ? cv_->visitTypeAnnotation(0, nullptr, ca::str::Utf8StringRef(), false) : nullptr;
}

void TraceClassVisitor::visitAttribute(const Attribute& attribute)
{
    if (cv_) cv_->visitAttribute(attribute);
}

void TraceClassVisitor::visitInnerClass(ca::str::Utf8StringRef name,
                                        ca::str::Utf8StringRef outerName,
                                        ca::str::Utf8StringRef innerName, int access)
{
    printer_->visitInnerClass(name, outerName, innerName, access);
    if (cv_) cv_->visitInnerClass(name, outerName, innerName, access);
}

void TraceClassVisitor::visitNestHost(ca::str::Utf8StringRef nestHost)
{
    if (cv_) cv_->visitNestHost(nestHost);
}

void TraceClassVisitor::visitNestMember(ca::str::Utf8StringRef nestMember)
{
    if (cv_) cv_->visitNestMember(nestMember);
}

void TraceClassVisitor::visitPermittedSubclass(ca::str::Utf8StringRef subclass)
{
    if (cv_) cv_->visitPermittedSubclass(subclass);
}

ModuleVisitor* TraceClassVisitor::visitModule(ca::str::Utf8StringRef name, int access,
                                              ca::str::Utf8StringRef version)
{
    ModuleVisitor* next = cv_ ? cv_->visitModule(name, access, version) : nullptr;
    return new TraceModuleVisitor(next, printer_);
}

RecordComponentVisitor* TraceClassVisitor::visitRecordComponent(ca::str::Utf8StringRef name,
                                                                ca::str::Utf8StringRef descriptor,
                                                                ca::str::Utf8StringRef signature)
{
    RecordComponentVisitor* next =
        cv_ ? cv_->visitRecordComponent(name, descriptor, signature) : nullptr;
    return new TraceRecordComponentVisitor(next, printer_);
}

FieldVisitor* TraceClassVisitor::visitField(int access, ca::str::Utf8StringRef name,
                                            ca::str::Utf8StringRef descriptor,
                                            ca::str::Utf8StringRef signature,
                                            const ca::core::Any&   value)
{
    Printer*      subPrinter = printer_->visitField(access, name, descriptor, signature, value);
    FieldVisitor* next =
        cv_ ? cv_->visitField(access, name, descriptor, signature, value) : nullptr;
    if (next == nullptr && subPrinter == nullptr) return nullptr;
    return new TraceFieldVisitor(
        next, subPrinter ? subPrinter : printer_, subPrinter ? printer_ : nullptr);
}

MethodVisitor* TraceClassVisitor::visitMethod(int access, ca::str::Utf8StringRef name,
                                              ca::str::Utf8StringRef                     descriptor,
                                              ca::str::Utf8StringRef                     signature,
                                              const std::vector<ca::str::Utf8StringRef>& exceptions)
{
    Printer* subPrinter = printer_->visitMethod(access, name, descriptor, signature, exceptions);
    MethodVisitor* next =
        cv_ ? cv_->visitMethod(access, name, descriptor, signature, exceptions) : nullptr;
    return new TraceMethodVisitor(
        next, subPrinter ? subPrinter : printer_, subPrinter ? printer_ : nullptr);
}

void TraceClassVisitor::visitEnd()
{
    printer_->visitClassEnd();
    if (cv_) cv_->visitEnd();
}

// ============================================================
// TraceAnnotationVisitor
// ============================================================

TraceAnnotationVisitor::TraceAnnotationVisitor(AnnotationVisitor* av, Printer* printer)
    : AnnotationVisitor(Opcodes::ASM9)
    , av_(av)
    , printer_(printer)
{}

void TraceAnnotationVisitor::visit(ca::str::Utf8StringRef name, const ca::core::Any& value)
{
    printer_->visitAnnotationValue(name, value);
    if (av_) av_->visit(name, value);
}

void TraceAnnotationVisitor::visitEnum(ca::str::Utf8StringRef name,
                                       ca::str::Utf8StringRef descriptor,
                                       ca::str::Utf8StringRef value)
{
    printer_->visitAnnotationEnum(name, descriptor, value);
    if (av_) av_->visitEnum(name, descriptor, value);
}

AnnotationVisitor* TraceAnnotationVisitor::visitAnnotation(ca::str::Utf8StringRef name,
                                                           ca::str::Utf8StringRef descriptor)
{
    Printer*           sub  = printer_->visitAnnotationAnnotation(name, descriptor);
    AnnotationVisitor* next = av_ ? av_->visitAnnotation(name, descriptor) : nullptr;
    if (next == nullptr && sub == nullptr) return nullptr;
    return new TraceAnnotationVisitor(next, sub ? sub : printer_);
}

AnnotationVisitor* TraceAnnotationVisitor::visitArray(ca::str::Utf8StringRef name)
{
    Printer*           sub  = printer_->visitAnnotationArray(name);
    AnnotationVisitor* next = av_ ? av_->visitArray(name) : nullptr;
    if (next == nullptr && sub == nullptr) return nullptr;
    return new TraceAnnotationVisitor(next, sub ? sub : printer_);
}

void TraceAnnotationVisitor::visitEnd()
{
    printer_->visitAnnotationEnd();
    if (av_) av_->visitEnd();
}

// ============================================================
// TraceFieldVisitor
// ============================================================

TraceFieldVisitor::TraceFieldVisitor(FieldVisitor* fv, Printer* printer, Printer* parentPrinter)
    : FieldVisitor(Opcodes::ASM9)
    , fv_(fv)
    , printer_(printer)
    , parentPrinter_(parentPrinter)
{}

AnnotationVisitor* TraceFieldVisitor::visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                      bool                   visible)
{
    AnnotationVisitor* next = fv_ ? fv_->visitAnnotation(descriptor, visible) : nullptr;
    if (next == nullptr) return nullptr;
    return new TraceAnnotationVisitor(next, printer_);
}

AnnotationVisitor* TraceFieldVisitor::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                          ca::str::Utf8StringRef descriptor,
                                                          bool                   visible)
{
    return fv_ ? fv_->visitTypeAnnotation(typeRef, typePath, descriptor, visible) : nullptr;
}

void TraceFieldVisitor::visitAttribute(const Attribute& attribute)
{
    if (fv_) fv_->visitAttribute(attribute);
}

void TraceFieldVisitor::visitEnd()
{
    printer_->visitFieldEnd();
    if (parentPrinter_) {
        for (auto& ref : printer_->text)
            parentPrinter_->text.push_back(parentPrinter_->textArena.intern(ref));
    }
    if (fv_) fv_->visitEnd();
}

// ============================================================
// TraceMethodVisitor
// ============================================================

TraceMethodVisitor::TraceMethodVisitor(MethodVisitor* mv, Printer* printer, Printer* parentPrinter)
    : MethodVisitor(Opcodes::ASM9)
    , mv_(mv)
    , printer_(printer)
    , parentPrinter_(parentPrinter)
{}

AnnotationVisitor* TraceMethodVisitor::visitAnnotationDefault()
{
    AnnotationVisitor* next = mv_ ? mv_->visitAnnotationDefault() : nullptr;
    return next ? new TraceAnnotationVisitor(next, printer_) : nullptr;
}

AnnotationVisitor* TraceMethodVisitor::visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                       bool                   visible)
{
    AnnotationVisitor* next = mv_ ? mv_->visitAnnotation(descriptor, visible) : nullptr;
    return next ? new TraceAnnotationVisitor(next, printer_) : nullptr;
}

AnnotationVisitor* TraceMethodVisitor::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                           ca::str::Utf8StringRef descriptor,
                                                           bool                   visible)
{
    return mv_ ? mv_->visitTypeAnnotation(typeRef, typePath, descriptor, visible) : nullptr;
}

AnnotationVisitor* TraceMethodVisitor::visitParameterAnnotation(int                    parameter,
                                                                ca::str::Utf8StringRef descriptor,
                                                                bool                   visible)
{
    return mv_ ? mv_->visitParameterAnnotation(parameter, descriptor, visible) : nullptr;
}

void TraceMethodVisitor::visitCode()
{
    printer_->visitCode();
    if (mv_) mv_->visitCode();
}

void TraceMethodVisitor::visitFrame(int type, int numLocal, const std::vector<ca::core::Any>& local,
                                    int numStack, const std::vector<ca::core::Any>& stack)
{
    printer_->visitFrame(type, numLocal, local, numStack, stack);
    if (mv_) mv_->visitFrame(type, numLocal, local, numStack, stack);
}

void TraceMethodVisitor::visitInsn(int opcode)
{
    printer_->visitInsn(opcode);
    if (mv_) mv_->visitInsn(opcode);
}

void TraceMethodVisitor::visitIntInsn(int opcode, int operand)
{
    printer_->visitIntInsn(opcode, operand);
    if (mv_) mv_->visitIntInsn(opcode, operand);
}

void TraceMethodVisitor::visitVarInsn(int opcode, int varIndex)
{
    printer_->visitVarInsn(opcode, varIndex);
    if (mv_) mv_->visitVarInsn(opcode, varIndex);
}

void TraceMethodVisitor::visitTypeInsn(int opcode, ca::str::Utf8StringRef type)
{
    printer_->visitTypeInsn(opcode, type);
    if (mv_) mv_->visitTypeInsn(opcode, type);
}

void TraceMethodVisitor::visitFieldInsn(int opcode, ca::str::Utf8StringRef owner,
                                        ca::str::Utf8StringRef name,
                                        ca::str::Utf8StringRef descriptor)
{
    printer_->visitFieldInsn(opcode, owner, name, descriptor);
    if (mv_) mv_->visitFieldInsn(opcode, owner, name, descriptor);
}

void TraceMethodVisitor::visitMethodInsn(int opcode, ca::str::Utf8StringRef owner,
                                         ca::str::Utf8StringRef name,
                                         ca::str::Utf8StringRef descriptor, bool isInterface)
{
    printer_->visitMethodInsn(opcode, owner, name, descriptor, isInterface);
    if (mv_) mv_->visitMethodInsn(opcode, owner, name, descriptor, isInterface);
}

void TraceMethodVisitor::visitInvokeDynamicInsn(
    ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
    const Handle& bootstrapMethodHandle, const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    printer_->visitInvokeDynamicInsn(
        name, descriptor, bootstrapMethodHandle, bootstrapMethodArguments);
    if (mv_)
        mv_->visitInvokeDynamicInsn(
            name, descriptor, bootstrapMethodHandle, bootstrapMethodArguments);
}

void TraceMethodVisitor::visitJumpInsn(int opcode, Label* label)
{
    printer_->visitJumpInsn(opcode, label);
    if (mv_) mv_->visitJumpInsn(opcode, label);
}

void TraceMethodVisitor::visitLabel(Label* label)
{
    printer_->visitLabel(label);
    if (mv_) mv_->visitLabel(label);
}

void TraceMethodVisitor::visitLdcInsn(const ca::core::Any& value)
{
    printer_->visitLdcInsn(value);
    if (mv_) mv_->visitLdcInsn(value);
}

void TraceMethodVisitor::visitIincInsn(int varIndex, int increment)
{
    printer_->visitIincInsn(varIndex, increment);
    if (mv_) mv_->visitIincInsn(varIndex, increment);
}

void TraceMethodVisitor::visitTableSwitchInsn(int min, int max, Label* dflt,
                                              const std::vector<Label*>& labels)
{
    printer_->visitTableSwitchInsn(min, max, dflt, labels);
    if (mv_) mv_->visitTableSwitchInsn(min, max, dflt, labels);
}

void TraceMethodVisitor::visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                                               const std::vector<Label*>& labels)
{
    printer_->visitLookupSwitchInsn(dflt, keys, labels);
    if (mv_) mv_->visitLookupSwitchInsn(dflt, keys, labels);
}

void TraceMethodVisitor::visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor,
                                                 int                    numDimensions)
{
    printer_->visitMultiANewArrayInsn(descriptor, numDimensions);
    if (mv_) mv_->visitMultiANewArrayInsn(descriptor, numDimensions);
}

AnnotationVisitor* TraceMethodVisitor::visitInsnAnnotation(int typeRef, const TypePath* typePath,
                                                           ca::str::Utf8StringRef descriptor,
                                                           bool                   visible)
{
    return mv_ ? mv_->visitInsnAnnotation(typeRef, typePath, descriptor, visible) : nullptr;
}

void TraceMethodVisitor::visitTryCatchBlock(Label* start, Label* end, Label* handler,
                                            ca::str::Utf8StringRef type)
{
    printer_->visitTryCatchBlock(start, end, handler, type);
    if (mv_) mv_->visitTryCatchBlock(start, end, handler, type);
}

AnnotationVisitor* TraceMethodVisitor::visitTryCatchAnnotation(int                    typeRef,
                                                               const TypePath*        typePath,
                                                               ca::str::Utf8StringRef descriptor,
                                                               bool                   visible)
{
    return mv_ ? mv_->visitTryCatchAnnotation(typeRef, typePath, descriptor, visible) : nullptr;
}

void TraceMethodVisitor::visitLocalVariable(ca::str::Utf8StringRef name,
                                            ca::str::Utf8StringRef descriptor,
                                            ca::str::Utf8StringRef signature, Label* start,
                                            Label* end, int index)
{
    printer_->visitLocalVariable(name, descriptor, signature, start, end, index);
    if (mv_) mv_->visitLocalVariable(name, descriptor, signature, start, end, index);
}

AnnotationVisitor* TraceMethodVisitor::visitLocalVariableAnnotation(
    int typeRef, const TypePath* typePath, const std::vector<Label*>& start,
    const std::vector<Label*>& end, const std::vector<int>& index,
    ca::str::Utf8StringRef descriptor, bool visible)
{
    return mv_ ? mv_->visitLocalVariableAnnotation(
                     typeRef, typePath, start, end, index, descriptor, visible)
               : nullptr;
}

void TraceMethodVisitor::visitLineNumber(int line, Label* start)
{
    printer_->visitLineNumber(line, start);
    if (mv_) mv_->visitLineNumber(line, start);
}

void TraceMethodVisitor::visitMaxs(int maxStack, int maxLocals)
{
    printer_->visitMaxs(maxStack, maxLocals);
    if (mv_) mv_->visitMaxs(maxStack, maxLocals);
}

void TraceMethodVisitor::visitEnd()
{
    printer_->visitMethodEnd();
    if (parentPrinter_) {
        for (auto& ref : printer_->text)
            parentPrinter_->text.push_back(parentPrinter_->textArena.intern(ref));
    }
    if (mv_) mv_->visitEnd();
}

// ============================================================
// TraceModuleVisitor
// ============================================================

TraceModuleVisitor::TraceModuleVisitor(ModuleVisitor* mv, Printer* printer, Printer* parentPrinter)
    : ModuleVisitor(Opcodes::ASM9)
    , mv_(mv)
    , printer_(printer)
    , parentPrinter_(parentPrinter)
{}

void TraceModuleVisitor::visitMainClass(ca::str::Utf8StringRef mainClass)
{
    printer_->visitMainClass(mainClass);
    if (mv_) mv_->visitMainClass(mainClass);
}

void TraceModuleVisitor::visitPackage(ca::str::Utf8StringRef packaze)
{
    printer_->visitPackage(packaze);
    if (mv_) mv_->visitPackage(packaze);
}

void TraceModuleVisitor::visitRequire(ca::str::Utf8StringRef module, int access,
                                      ca::str::Utf8StringRef version)
{
    printer_->visitRequire(module, access, version);
    if (mv_) mv_->visitRequire(module, access, version);
}

void TraceModuleVisitor::visitExport(ca::str::Utf8StringRef packaze, int access,
                                     const std::vector<ca::str::Utf8StringRef>& modules)
{
    printer_->visitExport(packaze, access, modules);
    if (mv_) mv_->visitExport(packaze, access, modules);
}

void TraceModuleVisitor::visitOpen(ca::str::Utf8StringRef packaze, int access,
                                   const std::vector<ca::str::Utf8StringRef>& modules)
{
    printer_->visitOpen(packaze, access, modules);
    if (mv_) mv_->visitOpen(packaze, access, modules);
}

void TraceModuleVisitor::visitUse(ca::str::Utf8StringRef service)
{
    printer_->visitUse(service);
    if (mv_) mv_->visitUse(service);
}

void TraceModuleVisitor::visitProvide(ca::str::Utf8StringRef                     service,
                                      const std::vector<ca::str::Utf8StringRef>& providers)
{
    printer_->visitProvide(service, providers);
    if (mv_) mv_->visitProvide(service, providers);
}

void TraceModuleVisitor::visitEnd()
{
    printer_->visitModuleEnd();
    if (parentPrinter_) {
        for (auto& ref : printer_->text)
            parentPrinter_->text.push_back(parentPrinter_->textArena.intern(ref));
    }
    if (mv_) mv_->visitEnd();
}

// ============================================================
// TraceRecordComponentVisitor
// ============================================================

TraceRecordComponentVisitor::TraceRecordComponentVisitor(RecordComponentVisitor* rcv,
                                                         Printer* printer, Printer* parentPrinter)
    : RecordComponentVisitor(Opcodes::ASM9)
    , rcv_(rcv)
    , printer_(printer)
    , parentPrinter_(parentPrinter)
{}

AnnotationVisitor* TraceRecordComponentVisitor::visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                                bool                   visible)
{
    printer_->visitRecordComponentAnnotation(descriptor, visible);
    AnnotationVisitor* next = rcv_ ? rcv_->visitAnnotation(descriptor, visible) : nullptr;
    if (next == nullptr) return nullptr;
    return new TraceAnnotationVisitor(next, printer_);
}

AnnotationVisitor* TraceRecordComponentVisitor::visitTypeAnnotation(
    int typeRef, const TypePath* typePath, ca::str::Utf8StringRef descriptor, bool visible)
{
    std::string typePathStr;
    if (typePath) typePathStr = typePath->toString();
    printer_->visitRecordComponentTypeAnnotation(
        typeRef,
        ca::str::Utf8StringRef(reinterpret_cast<const ca::u8*>(typePathStr.data()),
                               typePathStr.size(),
                               typePathStr.size()),
        descriptor,
        visible);
    AnnotationVisitor* next =
        rcv_ ? rcv_->visitTypeAnnotation(typeRef, typePath, descriptor, visible) : nullptr;
    if (next == nullptr) return nullptr;
    return new TraceAnnotationVisitor(next, printer_);
}

void TraceRecordComponentVisitor::visitAttribute(const Attribute& attribute)
{
    printer_->visitRecordComponentAttribute(attribute);
    if (rcv_) rcv_->visitAttribute(attribute);
}

void TraceRecordComponentVisitor::visitEnd()
{
    printer_->visitRecordComponentEnd();
    if (parentPrinter_) {
        for (auto& ref : printer_->text)
            parentPrinter_->text.push_back(parentPrinter_->textArena.intern(ref));
    }
    if (rcv_) rcv_->visitEnd();
}
