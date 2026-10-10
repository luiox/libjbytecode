#pragma once

#include <vector>

#include "jbytecode/core/visitors.hpp"
#include "jbytecode/commons/remapper.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class SignatureRemapper : public jbytecode::core::SignatureVisitor
{
public:
    SignatureRemapper(jbytecode::core::SignatureVisitor* sv, Remapper* remapper);

    void visitClassType(ca::str::Utf8StringRef name) override;
    void visitInnerClassType(ca::str::Utf8StringRef name) override;
    void visitFormalTypeParameter(ca::str::Utf8StringRef name) override;
    void visitTypeVariable(ca::str::Utf8StringRef name) override;
    jbytecode::core::SignatureVisitor* visitArrayType() override;
    void                               visitBaseType(char descriptor) override;
    jbytecode::core::SignatureVisitor* visitClassBound() override;
    jbytecode::core::SignatureVisitor* visitExceptionType() override;
    jbytecode::core::SignatureVisitor* visitInterface() override;
    jbytecode::core::SignatureVisitor* visitInterfaceBound() override;
    jbytecode::core::SignatureVisitor* visitParameterType() override;
    jbytecode::core::SignatureVisitor* visitReturnType() override;
    jbytecode::core::SignatureVisitor* visitSuperclass() override;
    void                               visitTypeArgument() override;
    jbytecode::core::SignatureVisitor* visitTypeArgument(char wildcard) override;
    void                               visitEnd() override;

private:
    jbytecode::core::SignatureVisitor* sv_;
    Remapper*                          remapper_;
    std::vector<ca::str::Utf8String>   classNames_;
};

}   // namespace jbytecode::commons
