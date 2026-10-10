#pragma once

#include "libca/core/any.hpp"
#include <unordered_map>
#include <vector>

#include "jbytecode/core/primitives.hpp"
#include "jbytecode/commons/generator_adapter.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class AdviceAdapter : public GeneratorAdapter
{
public:
    void visitCode() override;
    void visitLabel(jbytecode::core::Label* label) override;
    void visitInsn(int opcode) override;
    void visitVarInsn(int opcode, int varIndex) override;
    void visitFieldInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef descriptor) override;
    void visitIntInsn(int opcode, int operand) override;
    void visitLdcInsn(const ca::core::Any& value) override;
    void visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor, int numDimensions) override;
    void visitTypeInsn(int opcode, ca::str::Utf8StringRef type) override;
    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor, bool isInterface) override;
    void visitInvokeDynamicInsn(
        ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
        const jbytecode::core::Handle&    bootstrapMethodHandle,
        const std::vector<ca::core::Any>& bootstrapMethodArguments) override;
    void visitJumpInsn(int opcode, jbytecode::core::Label* label) override;
    void visitLookupSwitchInsn(jbytecode::core::Label* dflt, const std::vector<int>& keys,
                               const std::vector<jbytecode::core::Label*>& labels) override;
    void visitTableSwitchInsn(int min, int max, jbytecode::core::Label* dflt,
                              const std::vector<jbytecode::core::Label*>& labels) override;
    void visitTryCatchBlock(jbytecode::core::Label* start, jbytecode::core::Label* end,
                            jbytecode::core::Label* handler, ca::str::Utf8StringRef type) override;

protected:
    AdviceAdapter(int api, jbytecode::core::MethodVisitor* mv, int access,
                  ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor);

    virtual void onMethodEnter() {}
    virtual void onMethodExit(int opcode) {}

    int                 methodAccess_;
    ca::str::Utf8String methodDesc_;
    bool                isConstructor_;

private:
    enum Sentinel
    {
        UNINITIALIZED_THIS = 0,
        OTHER
    };
    bool             superClassConstructorCalled_ = false;
    std::vector<int> stackFrame_;
    std::unordered_map<jbytecode::core::Label*, std::vector<int>> forwardJumpStackFrames_;

    void doVisitMethodInsn(int opcode, ca::str::Utf8StringRef name,
                           ca::str::Utf8StringRef descriptor);
    void pushValue(int value);
    int  popValue();
    int  peekValue();
    void addForwardJump(jbytecode::core::Label* label);
    void addForwardJumps(jbytecode::core::Label*                     dflt,
                         const std::vector<jbytecode::core::Label*>& labels);
    void endConstructorBasicBlockWithoutSuccessor();
};

}   // namespace jbytecode::commons
