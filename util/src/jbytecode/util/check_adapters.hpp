#pragma once

#include <list>

#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/analysis/analyzer.hpp"
#include "jbytecode/analysis/basic_verifier.hpp"
#include "jbytecode/tree/nodes.hpp"

namespace jbytecode::util {

// ---- CheckAnnotationAdapter ----
class CheckAnnotationAdapter : public jbytecode::core::AnnotationVisitor
{
public:
    CheckAnnotationAdapter(jbytecode::core::AnnotationVisitor* av);
    void visit(ca::str::Utf8StringRef name, const ca::core::Any& value) override;
    void visitEnum(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                   ca::str::Utf8StringRef value) override;
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef name,
                                                        ca::str::Utf8StringRef descriptor) override;
    jbytecode::core::AnnotationVisitor* visitArray(ca::str::Utf8StringRef name) override;
    void                                visitEnd() override;

private:
    jbytecode::core::AnnotationVisitor* av_;
    bool                                endCalled_ = false;
    void                                checkEnd();
};

// ---- CheckFieldAdapter ----
class CheckFieldAdapter : public jbytecode::core::FieldVisitor
{
public:
    CheckFieldAdapter(jbytecode::core::FieldVisitor* fv);
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    void visitAttribute(const jbytecode::core::Attribute& attribute) override;
    void visitEnd() override;

private:
    jbytecode::core::FieldVisitor* fv_;
    bool                           endCalled_ = false;
    void                           checkEnd();
};

// ---- CheckModuleAdapter ----
class CheckModuleAdapter : public jbytecode::core::ModuleVisitor
{
public:
    CheckModuleAdapter(jbytecode::core::ModuleVisitor* mv);
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
    bool                            endCalled_    = false;
    bool                            mainClassSet_ = false;
    bool                            packageSet_   = false;
    void                            checkEnd();
};

// ---- CheckRecordComponentAdapter ----
class CheckRecordComponentAdapter : public jbytecode::core::RecordComponentVisitor
{
public:
    CheckRecordComponentAdapter(jbytecode::core::RecordComponentVisitor* rcv);
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    void visitAttribute(const jbytecode::core::Attribute& attribute) override;
    void visitEnd() override;

private:
    jbytecode::core::RecordComponentVisitor* rcv_;
    bool                                     endCalled_ = false;
    void                                     checkEnd();
};

// ---- CheckSignatureAdapter ----
class CheckSignatureAdapter : public jbytecode::core::SignatureVisitor
{
public:
    static constexpr int CLASS_SIGNATURE  = 0;
    static constexpr int METHOD_SIGNATURE = 1;
    static constexpr int TYPE_SIGNATURE   = 2;

    CheckSignatureAdapter(int type, jbytecode::core::SignatureVisitor* sv);
    void visitFormalTypeParameter(ca::str::Utf8StringRef name) override;
    jbytecode::core::SignatureVisitor* visitClassBound() override;
    jbytecode::core::SignatureVisitor* visitInterfaceBound() override;
    jbytecode::core::SignatureVisitor* visitSuperclass() override;
    jbytecode::core::SignatureVisitor* visitInterface() override;
    jbytecode::core::SignatureVisitor* visitParameterType() override;
    jbytecode::core::SignatureVisitor* visitReturnType() override;
    jbytecode::core::SignatureVisitor* visitExceptionType() override;
    void                               visitBaseType(char descriptor) override;
    void                               visitTypeVariable(ca::str::Utf8StringRef name) override;
    jbytecode::core::SignatureVisitor* visitArrayType() override;
    void                               visitClassType(ca::str::Utf8StringRef name) override;
    void                               visitInnerClassType(ca::str::Utf8StringRef name) override;
    void                               visitTypeArgument() override;
    jbytecode::core::SignatureVisitor* visitTypeArgument(char wildcard) override;
    void                               visitEnd() override;

private:
    enum State
    {
        S_START,
        S_FORMAL,
        S_SUPER,
        S_PARAM,
        S_RETURN,
        S_FIELD,
        S_END
    };

    jbytecode::core::SignatureVisitor* sv_;
    int                                type_;
    State                              state_;
    bool                               endCalled_ = false;
};

// ---- CheckMethodAdapter ----
class CheckMethodAdapter : public jbytecode::core::MethodVisitor
{
public:
    CheckMethodAdapter(jbytecode::core::MethodVisitor* mv);
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
    bool                            codeStarted_ = false;
    bool                            endCalled_   = false;
    void                            checkEnd();
    void                            checkCodeStarted();
    static bool                     isValidOpcode(int opcode);
};

// ---- CheckClassAdapter ----
class CheckClassAdapter : public jbytecode::core::ClassVisitor
{
public:
    CheckClassAdapter(jbytecode::core::ClassVisitor* cv);
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
    jbytecode::core::ModuleVisitor* visitModule(ca::str::Utf8StringRef name, int access,
                                                ca::str::Utf8StringRef version) override;
    jbytecode::core::FieldVisitor*  visitField(int access, ca::str::Utf8StringRef name,
                                               ca::str::Utf8StringRef descriptor,
                                               ca::str::Utf8StringRef signature,
                                               const ca::core::Any&   value) override;
    jbytecode::core::MethodVisitor* visitMethod(
        int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
        ca::str::Utf8StringRef                     signature,
        const std::vector<ca::str::Utf8StringRef>& exceptions) override;
    void visitEnd() override;

private:
    jbytecode::core::ClassVisitor* cv_;
    bool                           visitCalled_ = false;
    bool                           endCalled_   = false;
    void                           checkVisitCalled();
    void                           checkEnd();
};

// ---- CheckFrameAnalyzer ----
class CheckFrameAnalyzer : public jbytecode::analysis::Analyzer<jbytecode::analysis::BasicValue>
{
public:
    explicit CheckFrameAnalyzer(jbytecode::analysis::BasicVerifier* verifier);

    void analyzeClass(ca::str::Utf8StringRef owner, jbytecode::core::ClassReader* classReader);
    // Wrapper that checks for null before delegating to base Analyzer::analyze
    std::vector<jbytecode::analysis::Frame<jbytecode::analysis::BasicValue>*> analyze(
        const std::string& owner, jbytecode::tree::MethodNode* method);

protected:
    void init(ca::str::Utf8StringRef owner, jbytecode::tree::MethodNode* method) override;

private:
    void expandFrames(ca::str::Utf8StringRef owner, jbytecode::tree::MethodNode* method,
                      jbytecode::analysis::Frame<jbytecode::analysis::BasicValue>& initialFrame);
    jbytecode::analysis::Frame<jbytecode::analysis::BasicValue> expandFrame(
        ca::str::Utf8StringRef                                             owner,
        const jbytecode::analysis::Frame<jbytecode::analysis::BasicValue>& previousFrame,
        jbytecode::tree::FrameNode*                                        frameNode);
    jbytecode::analysis::BasicValue newFrameValue(ca::str::Utf8StringRef      owner,
                                                  jbytecode::tree::FrameNode* frameNode,
                                                  const ca::core::Any&        type);
    void                            checkFrame(int                                                                insnIndex,
                                               const jbytecode::analysis::Frame<jbytecode::analysis::BasicValue>& frame,
                                               bool requireFrame);
    std::string                     checkMerge(
                            const jbytecode::analysis::Frame<jbytecode::analysis::BasicValue>& srcFrame,
                            const jbytecode::analysis::Frame<jbytecode::analysis::BasicValue>& dstFrame);
    void        endControlFlow(int insnIndex);
    bool        hasNextJvmInsnOrFrame(int insnIndex);
    static bool isJvmInsnNode(jbytecode::tree::AbstractInsnNode* insnNode);

    jbytecode::analysis::Interpreter<jbytecode::analysis::BasicValue>* interpreter_;
    jbytecode::tree::InsnList*                                         insnList_;
    int                                                                currentLocals_;
    std::list<jbytecode::core::Type>                                   ownedTypes_;
};

}   // namespace jbytecode::util
