#pragma once

#include <string>
#include <vector>

#include "jbytecode/core/visitors.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class StaticInitMerger : public jbytecode::core::ClassVisitor
{
public:
    StaticInitMerger(ca::str::Utf8StringRef prefix, jbytecode::core::ClassVisitor* cv);

    void visit(int version, int access, ca::str::Utf8StringRef name,
               ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override;

    jbytecode::core::MethodVisitor* visitMethod(
        int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
        ca::str::Utf8StringRef                     signature,
        const std::vector<ca::str::Utf8StringRef>& exceptions) override;

    void visitEnd() override;

protected:
    StaticInitMerger(int api, ca::str::Utf8StringRef prefix, jbytecode::core::ClassVisitor* cv);

private:
    ca::str::Utf8StringRef          owner_;
    ca::str::Utf8StringRef          renamedClinitMethodPrefix_;
    int                             numClinitMethods_    = 0;
    jbytecode::core::MethodVisitor* mergedClinitVisitor_ = nullptr;
};

}   // namespace jbytecode::commons
