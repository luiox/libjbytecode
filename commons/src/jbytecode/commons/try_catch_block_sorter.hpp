#pragma once

#include <algorithm>
#include <vector>

#include "jbytecode/core/visitors.hpp"
#include "jbytecode/tree/nodes.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class TryCatchBlockSorter : public jbytecode::tree::MethodNode
{
public:
    TryCatchBlockSorter(jbytecode::core::MethodVisitor* mv, int access, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                        const std::vector<ca::str::Utf8StringRef>& exceptions);

    void visitEnd() override;
};

}   // namespace jbytecode::commons
