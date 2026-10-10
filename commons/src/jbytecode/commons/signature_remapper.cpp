#include "jbytecode/commons/signature_remapper.hpp"
#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/type.hpp"

#include <cstring>

using namespace jbytecode::commons;
using namespace jbytecode::core;
using namespace ca::str;
using ca::usize;

SignatureRemapper::SignatureRemapper(SignatureVisitor* sv, Remapper* remapper)
    : SignatureVisitor(Opcodes::ASM4)
    , sv_(sv)
    , remapper_(remapper)
{}

void SignatureRemapper::visitClassType(Utf8StringRef name)
{
    classNames_.emplace_back(name.data(), name.byte_length());
    sv_->visitClassType(remapper_->mapType(name));
}

void SignatureRemapper::visitInnerClassType(Utf8StringRef name)
{
    Utf8String outerClassName = std::move(classNames_.back());
    classNames_.pop_back();

    // 构建完整限定名 Outer$Inner
    Utf8StringBuilder builder;
    builder.append(outerClassName.ref());
    builder.append("$");
    builder.append(name);
    Utf8String className = builder.build();
    classNames_.push_back(std::move(className));

    // 映射后的内层类名：先 map outer 和完整名，再取出 "$" 后的后缀
    Utf8StringRef remappedOuter = remapper_->mapType(outerClassName.ref());
    Utf8StringRef remappedFull  = remapper_->mapType(classNames_.back().ref());

    usize innerByteStart;
    usize outerByteLen = remappedOuter.byte_length();
    if (remappedFull.byte_length() > outerByteLen + 1 && remappedFull.starts_with(remappedOuter) &&
        remappedFull.byte_at(outerByteLen) == '$') {
        innerByteStart = outerByteLen + 1;
    }
    else {
        // 回退：从末尾找最后一个 '$'
        innerByteStart = remappedFull.byte_length();
        while (innerByteStart > 0 && remappedFull.byte_at(innerByteStart - 1) != '$')
            innerByteStart--;
    }
    sv_->visitInnerClassType(remappedFull.slice(innerByteStart, remappedFull.byte_length()));
}

void SignatureRemapper::visitFormalTypeParameter(Utf8StringRef name)
{
    sv_->visitFormalTypeParameter(remapper_->mapType(name));
}

void SignatureRemapper::visitTypeVariable(Utf8StringRef name)
{
    sv_->visitTypeVariable(remapper_->mapType(name));
}

SignatureVisitor* SignatureRemapper::visitArrayType()
{
    sv_->visitArrayType();
    return this;
}

void SignatureRemapper::visitBaseType(char descriptor)
{
    sv_->visitBaseType(descriptor);
}

SignatureVisitor* SignatureRemapper::visitClassBound()
{
    sv_->visitClassBound();
    return this;
}

SignatureVisitor* SignatureRemapper::visitExceptionType()
{
    sv_->visitExceptionType();
    return this;
}

SignatureVisitor* SignatureRemapper::visitInterface()
{
    sv_->visitInterface();
    return this;
}

SignatureVisitor* SignatureRemapper::visitInterfaceBound()
{
    sv_->visitInterfaceBound();
    return this;
}

SignatureVisitor* SignatureRemapper::visitParameterType()
{
    sv_->visitParameterType();
    return this;
}

SignatureVisitor* SignatureRemapper::visitReturnType()
{
    sv_->visitReturnType();
    return this;
}

SignatureVisitor* SignatureRemapper::visitSuperclass()
{
    sv_->visitSuperclass();
    return this;
}

void SignatureRemapper::visitTypeArgument()
{
    sv_->visitTypeArgument();
}

SignatureVisitor* SignatureRemapper::visitTypeArgument(char wildcard)
{
    sv_->visitTypeArgument(wildcard);
    return this;
}

void SignatureRemapper::visitEnd()
{
    sv_->visitEnd();
    classNames_.pop_back();
}
