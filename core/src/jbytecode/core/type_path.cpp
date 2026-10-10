#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/byte_vector.hpp"
#include <stdexcept>

namespace jbytecode::core {

int TypePath::getLength() const
{
    if (data_.empty()) return 0;
    return data_[0];
}

int TypePath::getStep(int index) const
{
    if (index < 0 || index >= getLength()) return -1;
    return data_[1 + 2 * index];
}

int TypePath::getStepArgument(int index) const
{
    if (index < 0 || index >= getLength()) return -1;
    return data_[1 + 2 * index + 1];
}

std::string TypePath::toString() const
{
    int         length = getLength();
    std::string result;
    result.reserve(length * 2);
    for (int i = 0; i < length; ++i) {
        switch (getStep(i)) {
        case ARRAY_ELEMENT: result += '['; break;
        case INNER_TYPE: result += '.'; break;
        case WILDCARD_BOUND: result += '*'; break;
        case TYPE_ARGUMENT: result += std::to_string(getStepArgument(i)) + ';'; break;
        default: break;
        }
    }
    return result;
}

TypePath TypePath::fromBytes(const ByteVector& bytes)
{
    TypePath tp;
    tp.data_.resize(bytes.size());
    for (ca::i32 i = 0; i < bytes.size(); i++) {
        tp.data_[i] = bytes.data()[i];
    }
    return tp;
}

TypePath TypePath::fromString(const ca::str::Utf8StringRef& typePath)
{
    TypePath tp;
    if (typePath.is_empty()) return tp;

    std::vector<uint8_t> buf;
    buf.reserve(1 + typePath.byte_length());
    buf.push_back(0);
    size_t idx = 0;
    while (idx < typePath.byte_length()) {
        char c = static_cast<char>(typePath.byte_at(idx++));
        if (c == '[') {
            buf.push_back(ARRAY_ELEMENT);
            buf.push_back(0);
        }
        else if (c == '.') {
            buf.push_back(INNER_TYPE);
            buf.push_back(0);
        }
        else if (c == '*') {
            buf.push_back(WILDCARD_BOUND);
            buf.push_back(0);
        }
        else if (c >= '0' && c <= '9') {
            int typeArg = c - '0';
            while (idx < typePath.byte_length()) {
                c = static_cast<char>(typePath.byte_at(idx));
                if (c >= '0' && c <= '9') {
                    typeArg = typeArg * 10 + (c - '0');
                    ++idx;
                }
                else if (c == ';') {
                    ++idx;
                    break;
                }
                else {
                    throw std::invalid_argument("Invalid type path string");
                }
            }
            if (typeArg > 255) throw std::invalid_argument("Type path: argument index > 255");
            buf.push_back(TYPE_ARGUMENT);
            buf.push_back(static_cast<uint8_t>(typeArg));
        }
        else {
            throw std::invalid_argument("Invalid type path string");
        }
    }
    size_t stepCount = (buf.size() - 1) / 2;
    if (stepCount > 255) throw std::invalid_argument("Type path: too many steps");
    buf[0]   = static_cast<uint8_t>(stepCount);
    tp.data_ = std::move(buf);
    return tp;
}

void TypePath::put(const TypePath* typePath, ByteVector& output)
{
    if (typePath == nullptr || typePath->data_.empty()) {
        output.putByte(0);
    }
    else {
        // size_t→ca::i32 显式收窄（消 MSVC C4267）：data_ 写入面全部受 u1 值域
        // 约束——fromString 限 stepCount ≤ 255（:100 抛出），尺寸 = 1+2×steps ≤ 511；
        // ClassReader 三处 friend 写入均按 u1 pathLength 读入，尺寸同 ≤ 511；
        // fromBytes 全仓无调用点。size() ≤ 511 远小于 ca::i32 上限，收窄值恒等。
        output.putByteArray(typePath->data_.data(), 0,
                            static_cast<ca::i32>(typePath->data_.size()));
    }
}

}   // namespace jbytecode::core
