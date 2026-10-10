/// @file        signature_writer.cpp
/// @brief       SignatureWriter 实现 — 从 writers.hpp 内联实现迁移而来。
///              对标 org.objectweb.asm.signature.SignatureWriter。
///

#include "jbytecode/core/writers.hpp"
#include "libca/str/utf8_string.hpp"

using namespace ca::str;

namespace jbytecode::core {

void SignatureWriter::visitFormalTypeParameter(Utf8StringRef name)
{
    if (!hasFormals) {
        hasFormals = true;
        stringBuilder.append("<");
    }
    stringBuilder.append(name);
    stringBuilder.append(":");
}

SignatureVisitor* SignatureWriter::visitInterfaceBound()
{
    stringBuilder.append(":");
    return this;
}

SignatureVisitor* SignatureWriter::visitSuperclass()
{
    endFormals();
    return this;
}

SignatureVisitor* SignatureWriter::visitParameterType()
{
    endFormals();
    if (!hasParameters) {
        hasParameters = true;
        stringBuilder.append("(");
    }
    return this;
}

SignatureVisitor* SignatureWriter::visitReturnType()
{
    endFormals();
    if (!hasParameters) stringBuilder.append("(");
    stringBuilder.append(")");
    return this;
}

SignatureVisitor* SignatureWriter::visitExceptionType()
{
    stringBuilder.append("^");
    return this;
}

void SignatureWriter::visitBaseType(char descriptor)
{
    char buf[2] = {descriptor, '\0'};
    stringBuilder.append(buf);
}

void SignatureWriter::visitTypeVariable(Utf8StringRef name)
{
    stringBuilder.append("T");
    stringBuilder.append(name);
    stringBuilder.append(";");
}

SignatureVisitor* SignatureWriter::visitArrayType()
{
    stringBuilder.append("[");
    return this;
}

void SignatureWriter::visitClassType(Utf8StringRef name)
{
    stringBuilder.append("L");
    stringBuilder.append(name);
    argumentStack <<= 1;
}

void SignatureWriter::visitInnerClassType(Utf8StringRef name)
{
    endArguments();
    stringBuilder.append(".");
    stringBuilder.append(name);
    argumentStack <<= 1;
}

void SignatureWriter::visitTypeArgument()
{
    if ((argumentStack & 1) == 0) {
        argumentStack |= 1;
        stringBuilder.append("<");
    }
    stringBuilder.append("*");
}

SignatureVisitor* SignatureWriter::visitTypeArgument(char wildcard)
{
    if ((argumentStack & 1) == 0) {
        argumentStack |= 1;
        stringBuilder.append("<");
    }
    if (wildcard != '=') {
        char buf[2] = {wildcard, '\0'};
        stringBuilder.append(buf);
    }
    return this;
}

void SignatureWriter::visitEnd()
{
    endArguments();
    stringBuilder.append(";");
}

std::string SignatureWriter::toString() const
{
    return stringBuilder.build().to_std_string();
}

void SignatureWriter::endFormals()
{
    if (hasFormals) {
        hasFormals = false;
        stringBuilder.append(">");
    }
}

void SignatureWriter::endArguments()
{
    if ((argumentStack & 1) == 1) stringBuilder.append(">");
    argumentStack >>= 1;
}

}   // namespace jbytecode::core
