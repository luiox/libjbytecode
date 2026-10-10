#pragma once

#include <string>
#include <vector>
#include <list>
#include <sstream>
#include <memory>

#include "jbytecode/core/visitors.hpp"

namespace jbytecode::util {

class TraceSignatureVisitor : public jbytecode::core::SignatureVisitor
{
public:
    TraceSignatureVisitor(int accessFlags);
    ~TraceSignatureVisitor() override;

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

    std::string getDeclaration() const { return declaration_.str(); }
    std::string getReturnType() const { return returnType_ ? returnType_->str() : ""; }
    std::string getExceptions() const { return exceptions_ ? exceptions_->str() : ""; }

private:
    enum class Task
    {
        END_ARRAY_TYPE,
        END_EMPTY_TYPE_ARGUMENTS,
        END_NON_EMPTY_TYPE_ARGUMENTS
    };

    TraceSignatureVisitor(std::ostringstream* ss);

    void               endFormals();
    void               endTypeArguments();
    void               endType();
    static std::string baseTypeName(char descriptor);

    bool                                                isInterface_;
    std::ostringstream                                  declaration_;
    std::ostringstream*                                 returnType_ = nullptr;
    std::ostringstream*                                 exceptions_ = nullptr;
    std::ostringstream*                                 activeSb_   = nullptr;
    std::vector<std::unique_ptr<TraceSignatureVisitor>> ownedSubVisitors_;

    bool              formalTypeParameterVisited_ = false;
    bool              interfaceBoundVisited_      = false;
    bool              parameterTypeVisited_       = false;
    bool              interfaceVisited_           = false;
    std::vector<Task> pendingTasks_;
    std::string       separator_;
};

}   // namespace jbytecode::util
