#pragma once

#include "libca/core/any.hpp"
#include <string>
#include <vector>

#include "jbytecode/core/visitors.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class CodeSizeEvaluator : public jbytecode::core::MethodVisitor
{
public:
    explicit CodeSizeEvaluator(jbytecode::core::MethodVisitor* methodVisitor);

    int getMinSize() const { return minSize_; }
    int getMaxSize() const { return maxSize_; }

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
    void visitLdcInsn(const ca::core::Any& value) override;
    void visitIincInsn(int varIndex, int increment) override;
    void visitTableSwitchInsn(int min, int max, jbytecode::core::Label* dflt,
                              const std::vector<jbytecode::core::Label*>& labels) override;
    void visitLookupSwitchInsn(jbytecode::core::Label* dflt, const std::vector<int>& keys,
                               const std::vector<jbytecode::core::Label*>& labels) override;
    void visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor, int numDimensions) override;

private:
    CodeSizeEvaluator(int api, jbytecode::core::MethodVisitor* methodVisitor);

    int minSize_ = 0;
    int maxSize_ = 0;
};

}   // namespace jbytecode::commons
