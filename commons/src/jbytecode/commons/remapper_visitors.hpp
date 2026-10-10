#pragma once

#include <memory>
#include <string>
#include <vector>
#include "libca/core/any.hpp"

#include "jbytecode/core/visitors.hpp"
#include "jbytecode/commons/remapper.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class AnnotationRemapper : public jbytecode::core::AnnotationVisitor
{
public:
    AnnotationRemapper(jbytecode::core::AnnotationVisitor* av, Remapper* remapper);
    void visit(ca::str::Utf8StringRef name, const ca::core::Any& value) override;
    void visitEnum(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                   ca::str::Utf8StringRef value) override;
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef name,
                                                        ca::str::Utf8StringRef descriptor) override;
    jbytecode::core::AnnotationVisitor* visitArray(ca::str::Utf8StringRef name) override;
    void                                visitEnd() override;

private:
    jbytecode::core::AnnotationVisitor* av_;
    Remapper*                           remapper_;
};

class FieldRemapper : public jbytecode::core::FieldVisitor
{
public:
    FieldRemapper(jbytecode::core::FieldVisitor* fv, Remapper* remapper);
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    void visitEnd() override;

private:
    jbytecode::core::FieldVisitor* fv_;
    Remapper*                      remapper_;
};

class MethodRemapper : public jbytecode::core::MethodVisitor
{
public:
    MethodRemapper(jbytecode::core::MethodVisitor* mv, Remapper* remapper);
    jbytecode::core::AnnotationVisitor* visitAnnotationDefault() override;
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    jbytecode::core::AnnotationVisitor* visitParameterAnnotation(int                    parameter,
                                                                 ca::str::Utf8StringRef descriptor,
                                                                 bool visible) override;
    void visitFrame(int type, int nLocal, const std::vector<ca::core::Any>& local, int nStack,
                    const std::vector<ca::core::Any>& stack) override;
    void visitFieldInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef descriptor) override;
    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor, bool isInterface) override;
    void visitInvokeDynamicInsn(
        ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
        const jbytecode::core::Handle&    bootstrapMethodHandle,
        const std::vector<ca::core::Any>& bootstrapMethodArguments) override;
    void visitLdcInsn(const ca::core::Any& value) override;
    void visitTypeInsn(int opcode, ca::str::Utf8StringRef type) override;
    void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                            ca::str::Utf8StringRef signature, jbytecode::core::Label* start,
                            jbytecode::core::Label* end, int index) override;
    void visitMaxs(int maxStack, int maxLocals) override;
    void visitEnd() override;

private:
    jbytecode::core::MethodVisitor* mv_;
    Remapper*                       remapper_;
};

class ClassRemapper : public jbytecode::core::ClassVisitor
{
public:
    ClassRemapper(jbytecode::core::ClassVisitor* cv, Remapper* remapper);
    void visit(int version, int access, ca::str::Utf8StringRef name,
               ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override;
    void visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug) override;
    void visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef desc) override;
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    void visitInnerClass(ca::str::Utf8StringRef name, ca::str::Utf8StringRef outerName,
                         ca::str::Utf8StringRef innerName, int access) override;
    void visitNestHost(ca::str::Utf8StringRef nestHost) override;
    void visitNestMember(ca::str::Utf8StringRef nestMember) override;
    void visitPermittedSubclass(ca::str::Utf8StringRef subclass) override;
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
    jbytecode::core::ModuleVisitor* visitModule(ca::str::Utf8StringRef name, int access,
                                                ca::str::Utf8StringRef version) override;
    void                            visitEnd() override;

private:
    jbytecode::core::ClassVisitor* cv_;
    Remapper*                      remapper_;
    ca::str::Utf8StringRef         className_;
};

class ModuleRemapper : public jbytecode::core::ModuleVisitor
{
public:
    ModuleRemapper(jbytecode::core::ModuleVisitor* mv, Remapper* remapper);
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
    Remapper*                       remapper_;
};

class RecordComponentRemapper : public jbytecode::core::RecordComponentVisitor
{
public:
    RecordComponentRemapper(jbytecode::core::RecordComponentVisitor* rcv, Remapper* remapper);
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    void visitEnd() override;

private:
    jbytecode::core::RecordComponentVisitor* rcv_;
    Remapper*                                remapper_;
};

}   // namespace jbytecode::commons
