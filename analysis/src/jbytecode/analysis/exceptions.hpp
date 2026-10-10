#pragma once

#include <string>
#include <sstream>
#include <exception>

// 构造函数内联调用 insn->getOpcode()，需要完整类型；自包含头，
// 不再依赖包含顺序。
#include "jbytecode/tree/nodes.hpp"

namespace jbytecode::analysis {

class AnalyzerException : public std::exception
{
public:
    AnalyzerException()
        : msg_("")
    {}
    explicit AnalyzerException(const std::string& message)
        : msg_(message)
    {}
    AnalyzerException(const jbytecode::tree::AbstractInsnNode* insn, const std::string& message)
    {
        std::ostringstream oss;
        if (insn) oss << "At instruction opcode=" << insn->getOpcode() << ": ";
        oss << message;
        msg_ = oss.str();
    }
    const char* what() const noexcept override { return msg_.c_str(); }

private:
    std::string msg_;
};

}   // namespace jbytecode::analysis
