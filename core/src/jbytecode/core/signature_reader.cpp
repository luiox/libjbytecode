/// @file        signature_reader.cpp
/// @brief       SignatureReader 实现 — 从 readers.hpp 内联实现迁移而来。
///              对标 org.objectweb.asm.signature.SignatureReader。
///
///              原本全部内联在 readers.hpp 中，违反"声明/实现分离"约定，
///              现移出以保证头文件只含声明。

#include "jbytecode/core/readers.hpp"

#include <stdexcept>

using namespace ca::str;

namespace jbytecode::core {

void SignatureReader::accept(SignatureVisitor* v)
{
    Utf8StringRef sig    = signatureValue;
    size_t        length = sig.byte_length();
    size_t        offset = 0;

    if (length > 0 && sig.byte_at(0) == '<') {
        offset = 2;
        do {
            size_t classBoundStartOffset = length;
            for (size_t i = offset; i < length; i++) {
                if (sig.byte_at(i) == ':') {
                    classBoundStartOffset = i;
                    break;
                }
            }
            if (classBoundStartOffset >= length || classBoundStartOffset < offset) {
                throw std::invalid_argument(
                    "Malformed signature: expected ':' for type parameter bound");
            }
            v->visitFormalTypeParameter(sig.slice(offset - 1, classBoundStartOffset));
            offset = classBoundStartOffset + 1;
            if (offset >= length)
                throw std::invalid_argument("Malformed signature: unexpected end after ':'");
            char currentChar = static_cast<char>(sig.byte_at(offset));
            if (currentChar == 'L' || currentChar == '[' || currentChar == 'T') {
                offset = parseType(sig, offset, v->visitClassBound());
            }
            while (offset < length &&
                   (currentChar = static_cast<char>(sig.byte_at(offset++))) == ':') {
                offset = parseType(sig, offset, v->visitInterfaceBound());
            }
        } while (offset < length && sig.byte_at(offset - 1) != '>');
    }

    if (offset < length && sig.byte_at(offset) == '(') {
        offset++;
        while (offset < length && sig.byte_at(offset) != ')') {
            offset = parseType(sig, offset, v->visitParameterType());
        }
        offset = parseType(sig, offset + 1, v->visitReturnType());
        while (offset < length) {
            offset = parseType(sig, offset + 1, v->visitExceptionType());
        }
    }
    else {
        offset = parseType(sig, offset, v->visitSuperclass());
        while (offset < length) {
            offset = parseType(sig, offset, v->visitInterface());
        }
    }
}

void SignatureReader::acceptType(SignatureVisitor* v)
{
    parseType(signatureValue, 0, v);
}

size_t SignatureReader::parseType(Utf8StringRef sig, size_t startOffset, SignatureVisitor* v)
{
    size_t offset      = startOffset;
    char   currentChar = static_cast<char>(sig.byte_at(offset++));

    switch (currentChar) {
    case 'Z':
    case 'C':
    case 'B':
    case 'S':
    case 'I':
    case 'F':
    case 'J':
    case 'D':
    case 'V': v->visitBaseType(currentChar); return offset;

    case '[': return parseType(sig, offset, v->visitArrayType());

    case 'T':
    {
        size_t endOffset = sig.byte_length();
        for (size_t i = offset; i < sig.byte_length(); i++) {
            if (sig.byte_at(i) == ';') {
                endOffset = i;
                break;
            }
        }
        if (endOffset >= sig.byte_length()) {
            throw std::invalid_argument("Malformed signature: missing ';' for type variable");
        }
        v->visitTypeVariable(sig.slice(offset, endOffset));
        return endOffset + 1;
    }

    case 'L':
    {
        size_t start   = offset;
        bool   visited = false;
        bool   inner   = false;
        while (true) {
            if (offset >= sig.byte_length()) {
                throw std::invalid_argument("Malformed signature: unexpected end in class type");
            }
            currentChar = static_cast<char>(sig.byte_at(offset++));
            if (currentChar == '.' || currentChar == ';') {
                if (!visited) {
                    auto name = sig.slice(start, offset - 1);
                    if (inner) {
                        v->visitInnerClassType(name);
                    }
                    else {
                        v->visitClassType(name);
                    }
                }
                if (currentChar == ';') {
                    v->visitEnd();
                    break;
                }
                start   = offset;
                visited = false;
                inner   = true;
            }
            else if (currentChar == '<') {
                auto name = sig.slice(start, offset - 1);
                if (inner) {
                    v->visitInnerClassType(name);
                }
                else {
                    v->visitClassType(name);
                }
                visited = true;
                while (true) {
                    if (offset >= sig.byte_length()) {
                        throw std::invalid_argument("Malformed signature: missing '>'");
                    }
                    currentChar = static_cast<char>(sig.byte_at(offset));
                    if (currentChar == '>') break;
                    switch (currentChar) {
                    case '*':
                        ++offset;
                        v->visitTypeArgument();
                        break;
                    case '+':
                    case '-':
                        offset = parseType(sig, offset + 1, v->visitTypeArgument(currentChar));
                        break;
                    default: offset = parseType(sig, offset, v->visitTypeArgument('=')); break;
                    }
                }
                offset++;
            }
        }
        return offset;
    }

    default:
        throw std::invalid_argument("Invalid signature at offset " + std::to_string(startOffset));
    }
}

}   // namespace jbytecode::core
