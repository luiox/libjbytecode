#pragma once

#include "jbytecode/core/visitors.hpp"
#include "jbytecode/util/printer.hpp"

namespace jbytecode::util {

class TraceMethodVisitor;

class TraceClassVisitor : public jbytecode::core::ClassVisitor
{
public:
    TraceClassVisitor(jbytecode::core::ClassVisitor* cv, Printer* printer);

    void visit(int version, int access, ca::str::Utf8StringRef name,
               ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override;
    void visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug) override;
    void visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor) override;
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    void visitAttribute(const jbytecode::core::Attribute& attribute) override;
    void visitInnerClass(ca::str::Utf8StringRef name, ca::str::Utf8StringRef outerName,
                         ca::str::Utf8StringRef innerName, int access) override;
    void visitNestHost(ca::str::Utf8StringRef nestHost) override;
    void visitNestMember(ca::str::Utf8StringRef nestMember) override;
    void visitPermittedSubclass(ca::str::Utf8StringRef subclass) override;
    jbytecode::core::ModuleVisitor*          visitModule(ca::str::Utf8StringRef name, int access,
                                                         ca::str::Utf8StringRef version) override;
    jbytecode::core::RecordComponentVisitor* visitRecordComponent(
        ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
        ca::str::Utf8StringRef signature) override;
    jbytecode::core::FieldVisitor*  visitField(int access, ca::str::Utf8StringRef name,
                                               ca::str::Utf8StringRef descriptor,
                                               ca::str::Utf8StringRef signature,
                                               const ca::core::Any&   value) override;
    jbytecode::core::MethodVisitor* visitMethod(
        int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
        ca::str::Utf8StringRef                     signature,
        const std::vector<ca::str::Utf8StringRef>& exceptions) override;
    void visitEnd() override;

    static void mergeText(Printer* parent, Printer* child);

private:
    jbytecode::core::ClassVisitor* cv_;
    Printer*                       printer_;
};

class TraceAnnotationVisitor : public jbytecode::core::AnnotationVisitor
{
public:
    TraceAnnotationVisitor(jbytecode::core::AnnotationVisitor* av, Printer* printer);
    void visit(ca::str::Utf8StringRef name, const ca::core::Any& value) override;
    void visitEnum(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                   ca::str::Utf8StringRef value) override;
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef name,
                                                        ca::str::Utf8StringRef descriptor) override;
    jbytecode::core::AnnotationVisitor* visitArray(ca::str::Utf8StringRef name) override;
    void                                visitEnd() override;

private:
    jbytecode::core::AnnotationVisitor* av_;
    Printer*                            printer_;
};

class TraceFieldVisitor : public jbytecode::core::FieldVisitor
{
public:
    TraceFieldVisitor(jbytecode::core::FieldVisitor* fv, Printer* printer,
                      Printer* parentPrinter = nullptr);
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    void visitAttribute(const jbytecode::core::Attribute& attribute) override;
    void visitEnd() override;

private:
    jbytecode::core::FieldVisitor* fv_;
    Printer*                       printer_;
    Printer*                       parentPrinter_;
};

class TraceModuleVisitor : public jbytecode::core::ModuleVisitor
{
public:
    TraceModuleVisitor(jbytecode::core::ModuleVisitor* mv, Printer* printer,
                       Printer* parentPrinter = nullptr);
    void visitMainClass(ca::str::Utf8StringRef mainClass) override;
    void visitPackage(ca::str::Utf8StringRef packaze) override;
    void visitRequire(ca::str::Utf8StringRef module, int access,
                      ca::str::Utf8StringRef version) override;
    void visitExport(ca::str::Utf8StringRef packaze, int access,
                     const std::vector<ca::str::Utf8StringRef>& modules) override;
    void visitOpen(ca::str::Utf8StringRef packaze, int access,
                   const std::vector<ca::str::Utf8StringRef>& modules) override;
    void visitUse(ca::str::Utf8StringRef service) override;
    void visitProvide(ca::str::Utf8StringRef                     service,
                      const std::vector<ca::str::Utf8StringRef>& providers) override;
    void visitEnd() override;

private:
    jbytecode::core::ModuleVisitor* mv_;
    Printer*                        printer_;
    Printer*                        parentPrinter_;
};

class TraceRecordComponentVisitor : public jbytecode::core::RecordComponentVisitor
{
public:
    TraceRecordComponentVisitor(jbytecode::core::RecordComponentVisitor* rcv, Printer* printer,
                                Printer* parentPrinter = nullptr);
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    void visitAttribute(const jbytecode::core::Attribute& attribute) override;
    void visitEnd() override;

private:
    jbytecode::core::RecordComponentVisitor* rcv_;
    Printer*                                 printer_;
    Printer*                                 parentPrinter_;
};

class TraceMethodVisitor : public jbytecode::core::MethodVisitor
{
public:
    TraceMethodVisitor(jbytecode::core::MethodVisitor* mv, Printer* printer,
                       Printer* parentPrinter = nullptr);

    jbytecode::core::AnnotationVisitor* visitAnnotationDefault() override;
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    jbytecode::core::AnnotationVisitor* visitParameterAnnotation(int                    parameter,
                                                                 ca::str::Utf8StringRef descriptor,
                                                                 bool visible) override;
    void                                visitCode() override;
    void visitFrame(int type, int numLocal, const std::vector<ca::core::Any>& local, int numStack,
                    const std::vector<ca::core::Any>& stack) override;
    void visitInsn(int opcode) override;
    void visitIntInsn(int opcode, int operand) override;
    void visitVarInsn(int opcode, int varIndex) override;
    void visitTypeInsn(int opcode, ca::str::Utf8StringRef type) override;
    void visitFieldInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef descriptor) override;
    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor, bool isInterface) override;
    void visitInvokeDynamicInsn(
        ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
        const jbytecode::core::Handle&    bootstrapMethodHandle,
        const std::vector<ca::core::Any>& bootstrapMethodArguments) override;
    void visitJumpInsn(int opcode, jbytecode::core::Label* label) override;
    void visitLabel(jbytecode::core::Label* label) override;
    void visitLdcInsn(const ca::core::Any& value) override;
    void visitIincInsn(int varIndex, int increment) override;
    void visitTableSwitchInsn(int min, int max, jbytecode::core::Label* dflt,
                              const std::vector<jbytecode::core::Label*>& labels) override;
    void visitLookupSwitchInsn(jbytecode::core::Label* dflt, const std::vector<int>& keys,
                               const std::vector<jbytecode::core::Label*>& labels) override;
    void visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor, int numDimensions) override;
    jbytecode::core::AnnotationVisitor* visitInsnAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    void visitTryCatchBlock(jbytecode::core::Label* start, jbytecode::core::Label* end,
                            jbytecode::core::Label* handler, ca::str::Utf8StringRef type) override;
    jbytecode::core::AnnotationVisitor* visitTryCatchAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                            ca::str::Utf8StringRef signature, jbytecode::core::Label* start,
                            jbytecode::core::Label* end, int index) override;
    jbytecode::core::AnnotationVisitor* visitLocalVariableAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath,
        const std::vector<jbytecode::core::Label*>& start,
        const std::vector<jbytecode::core::Label*>& end, const std::vector<int>& index,
        ca::str::Utf8StringRef descriptor, bool visible) override;
    void visitLineNumber(int line, jbytecode::core::Label* start) override;
    void visitMaxs(int maxStack, int maxLocals) override;
    void visitEnd() override;

private:
    jbytecode::core::MethodVisitor* mv_;
    Printer*                        printer_;
    Printer*                        parentPrinter_;
};

}   // namespace jbytecode::util
