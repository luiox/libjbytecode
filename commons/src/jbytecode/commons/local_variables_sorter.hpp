#pragma once

#include "libca/core/any.hpp"
#include <string>
#include <vector>

#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/type.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class LocalVariablesSorter : public jbytecode::core::MethodVisitor
{
public:
    LocalVariablesSorter(int access, ca::str::Utf8StringRef descriptor,
                         jbytecode::core::MethodVisitor* mv);

    void visitVarInsn(int opcode, int varIndex) override;
    void visitIincInsn(int varIndex, int increment) override;
    void visitMaxs(int maxStack, int maxLocals) override;
    void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                            ca::str::Utf8StringRef signature, jbytecode::core::Label* start,
                            jbytecode::core::Label* end, int index) override;
    void visitFrame(int type, int nLocal, const std::vector<ca::core::Any>& local, int nStack,
                    const std::vector<ca::core::Any>& stack) override;

    int newLocal(const jbytecode::core::Type& type);

protected:
    LocalVariablesSorter(int api, int access, ca::str::Utf8StringRef descriptor,
                         jbytecode::core::MethodVisitor* mv);

    int firstLocal_;
    int nextLocal_;

private:
    std::vector<int>           remappedVariableIndices_;
    std::vector<ca::core::Any> remappedLocalTypes_;

    int  getRemappedLocal(int oldIndex) const;
    void setRemappedLocal(int oldIndex, const ca::core::Any& type);
    int  remap(int oldIndex, const ca::core::Any& type);
};

}   // namespace jbytecode::commons
