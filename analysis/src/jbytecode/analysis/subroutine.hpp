#pragma once

#include <vector>
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/tree/insn_list.hpp"

namespace jbytecode::analysis {

class Subroutine
{
public:
    jbytecode::core::Label*                         start = nullptr;
    std::vector<jbytecode::core::Label*>            successors;
    std::vector<jbytecode::tree::AbstractInsnNode*> callers;

    Subroutine() = default;

    explicit Subroutine(jbytecode::core::Label* start, jbytecode::tree::AbstractInsnNode* caller,
                        std::vector<jbytecode::core::Label*> successors)
        : start(start)
        , successors(std::move(successors))
    {
        callers.push_back(caller);
    }
};

}   // namespace jbytecode::analysis
