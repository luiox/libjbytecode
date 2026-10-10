#pragma once

#include <unordered_map>
#include "jbytecode/util/printer.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::util {

class ASMifier : public Printer
{
public:
    ASMifier();
    ~ASMifier() override;

    void     visit(int version, int access, ca::str::Utf8StringRef name,
                   ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
                   const std::vector<ca::str::Utf8StringRef>& interfaces) override;
    void     visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug) override;
    void     visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                             ca::str::Utf8StringRef descriptor) override;
    Printer* visitClassAnnotation(ca::str::Utf8StringRef descriptor, bool visible) override;
    void     visitInnerClass(ca::str::Utf8StringRef name, ca::str::Utf8StringRef outerName,
                             ca::str::Utf8StringRef innerName, int access) override;
    Printer* visitField(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                        ca::str::Utf8StringRef signature, const ca::core::Any& value) override;
    Printer* visitMethod(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                         ca::str::Utf8StringRef                     signature,
                         const std::vector<ca::str::Utf8StringRef>& exceptions) override;
    void     visitClassEnd() override;

    void visitCode() override;
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
    void visitTryCatchBlock(jbytecode::core::Label* start, jbytecode::core::Label* end,
                            jbytecode::core::Label* handler, ca::str::Utf8StringRef type) override;
    void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                            ca::str::Utf8StringRef signature, jbytecode::core::Label* start,
                            jbytecode::core::Label* end, int index) override;
    void visitLineNumber(int line, jbytecode::core::Label* start) override;
    void visitMaxs(int maxStack, int maxLocals) override;
    void visitMethodEnd() override;

    void     visitAnnotationValue(ca::str::Utf8StringRef name, const ca::core::Any& value) override;
    void     visitAnnotationEnum(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                 ca::str::Utf8StringRef value) override;
    Printer* visitAnnotationAnnotation(ca::str::Utf8StringRef name,
                                       ca::str::Utf8StringRef descriptor) override;
    Printer* visitAnnotationArray(ca::str::Utf8StringRef name) override;
    void     visitAnnotationEnd() override;

private:
    static constexpr const char* TAB = "    ";
    struct SharedState
    {
        int varCounter = 0;
    };
    SharedState*                                             state_;
    bool                                                     ownsState_ = false;
    std::string                                              varName_;
    std::unordered_map<jbytecode::core::Label*, std::string> labelNames_;
    int                                                      nextLabelId_ = 0;

    std::string nextVar() { return "mv" + std::to_string(state_->varCounter++); }
    std::string getLabelName(jbytecode::core::Label* label);
    void        line(ca::str::Utf8StringRef s);
    void        line();
    ASMifier*   subASMifier();
    std::string toASMValue(const ca::core::Any& value);
};

}   // namespace jbytecode::util
