#include "jbytecode/core/byte_vector.hpp"

#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <limits>

#include "jstr/conversion.hpp"

using namespace ca;


namespace jbytecode::core {

// ===== 构造 / 析构 =====

ByteVector::ByteVector()
{
    data_.data_   = new u8[64];
    data_.length_ = 64;
    capacity_     = 64;
    length_       = 0;
}

ByteVector::ByteVector(i32 initialCapacity)
{
    data_.data_   = new u8[initialCapacity];
    data_.length_ = initialCapacity;
    capacity_     = initialCapacity;
    length_       = 0;
}

// Corresponds to Java package-private ByteVector(byte[] data) — wraps existing
// byte array with length = data.length. In C++ we use length param directly.
ByteVector::ByteVector(u8* data, i32 length)
{
    data_.data_   = data;
    data_.length_ = length;
    capacity_     = length;
    length_       = length;   // match Java: length = data.length
}

ByteVector::ByteVector(ByteVector&& other) noexcept
    : data_(other.data_)
    , length_(other.length_)
    , capacity_(other.capacity_)
{
    other.data_.data_   = nullptr;
    other.data_.length_ = 0;
    other.length_       = 0;
    other.capacity_     = 0;
}

ByteVector& ByteVector::operator=(ByteVector&& other) noexcept
{
    if (this != &other) {
        delete[] data_.data_;
        data_               = other.data_;
        length_             = other.length_;
        capacity_           = other.capacity_;
        other.data_.data_   = nullptr;
        other.data_.length_ = 0;
        other.length_       = 0;
        other.capacity_     = 0;
    }
    return *this;
}

ByteVector::~ByteVector()
{
    delete[] data_.data_;
}

// ===== 公开查询方法 =====

u8* ByteVector::data() const
{
    return data_.data_;
}
u8* ByteVector::mutableData() const
{
    return data_.data_;
}

// ===== put* 方法 =====

ByteVector& ByteVector::putByte(int byteValue)
{
    if (length_ + 1 > capacity_) {
        enlarge(1);
    }
    data_.data_[length_++] = static_cast<u8>(byteValue);
    return *this;
}

ByteVector& ByteVector::put11(int byteValue1, int byteValue2)
{
    if (length_ + 2 > capacity_) {
        enlarge(2);
    }
    data_.data_[length_++] = static_cast<u8>(byteValue1);
    data_.data_[length_++] = static_cast<u8>(byteValue2);
    return *this;
}

ByteVector& ByteVector::putShort(int shortValue)
{
    i32 currentLength = length_;
    if (currentLength + 2 > capacity_) {
        enlarge(2);
        currentLength = length_;
    }
    data_.data_[currentLength++] = static_cast<u8>(shortValue >> 8);
    data_.data_[currentLength++] = static_cast<u8>(shortValue);
    length_                      = currentLength;
    return *this;
}

ByteVector& ByteVector::put12(int byteValue, int shortValue)
{
    i32 currentLength = length_;
    if (currentLength + 3 > capacity_) {
        enlarge(3);
        currentLength = length_;
    }
    data_.data_[currentLength++] = static_cast<u8>(byteValue);
    data_.data_[currentLength++] = static_cast<u8>(shortValue >> 8);
    data_.data_[currentLength++] = static_cast<u8>(shortValue);
    length_                      = currentLength;
    return *this;
}

ByteVector& ByteVector::put112(int byteValue1, int byteValue2, int shortValue)
{
    i32 currentLength = length_;
    if (currentLength + 4 > capacity_) {
        enlarge(4);
        currentLength = length_;
    }
    data_.data_[currentLength++] = static_cast<u8>(byteValue1);
    data_.data_[currentLength++] = static_cast<u8>(byteValue2);
    data_.data_[currentLength++] = static_cast<u8>(shortValue >> 8);
    data_.data_[currentLength++] = static_cast<u8>(shortValue);
    length_                      = currentLength;
    return *this;
}

ByteVector& ByteVector::putInt(int intValue)
{
    i32 currentLength = length_;
    if (currentLength + 4 > capacity_) {
        enlarge(4);
        currentLength = length_;
    }
    data_.data_[currentLength++] = static_cast<u8>(intValue >> 24);
    data_.data_[currentLength++] = static_cast<u8>(intValue >> 16);
    data_.data_[currentLength++] = static_cast<u8>(intValue >> 8);
    data_.data_[currentLength++] = static_cast<u8>(intValue);
    length_                      = currentLength;
    return *this;
}

ByteVector& ByteVector::put122(int byteValue, int shortValue1, int shortValue2)
{
    i32 currentLength = length_;
    if (currentLength + 5 > capacity_) {
        enlarge(5);
        currentLength = length_;
    }
    data_.data_[currentLength++] = static_cast<u8>(byteValue);
    data_.data_[currentLength++] = static_cast<u8>(shortValue1 >> 8);
    data_.data_[currentLength++] = static_cast<u8>(shortValue1);
    data_.data_[currentLength++] = static_cast<u8>(shortValue2 >> 8);
    data_.data_[currentLength++] = static_cast<u8>(shortValue2);
    length_                      = currentLength;
    return *this;
}

ByteVector& ByteVector::putLong(i64 longValue)
{
    i32 currentLength = length_;
    if (currentLength + 8 > capacity_) {
        enlarge(8);
        currentLength = length_;
    }
    u32 high                     = static_cast<u32>(longValue >> 32);
    u32 low                      = static_cast<u32>(longValue);
    data_.data_[currentLength++] = static_cast<u8>(high >> 24);
    data_.data_[currentLength++] = static_cast<u8>(high >> 16);
    data_.data_[currentLength++] = static_cast<u8>(high >> 8);
    data_.data_[currentLength++] = static_cast<u8>(high);
    data_.data_[currentLength++] = static_cast<u8>(low >> 24);
    data_.data_[currentLength++] = static_cast<u8>(low >> 16);
    data_.data_[currentLength++] = static_cast<u8>(low >> 8);
    data_.data_[currentLength++] = static_cast<u8>(low);
    length_                      = currentLength;
    return *this;
}

ByteVector& ByteVector::putMUtf8(const u8* data, i32 byteLength)
{
    if (byteLength > 65535) throw std::length_error("MUTF8 string too large");
    if (length_ + 2 + byteLength > capacity_) {
        enlarge(2 + byteLength);
    }
    data_.data_[length_++] = static_cast<u8>(byteLength >> 8);
    data_.data_[length_++] = static_cast<u8>(byteLength);
    std::memcpy(data_.data_ + length_, data, byteLength);
    length_ += byteLength;
    return *this;
}

ByteVector& ByteVector::putUTF8(const str::Utf8StringRef& str)
{
    // 先尝试按 UTF-8 转 MUTF-8（正常字符串路径）。
    std::string utf8_str(reinterpret_cast<const char*>(str.data()), str.byte_length());
    auto        result = jstr::utf8_to_mutf8(utf8_str);
    if (result.is_err()) {
        auto wtf8 = jstr::wtf8_to_mutf8(utf8_str);
        if (wtf8.is_ok()) {
            return putMUtf8(reinterpret_cast<const u8*>(wtf8.unwrap().data()),
                            static_cast<i32>(wtf8.unwrap().size()));
        }
        // 非 UTF-8 字节（如字节流载体）：按 Latin-1 每 code point 转 MUTF-8。
        // 唯一失败条件是空指针配非零长度；Utf8StringRef 正常构造不会出现，保留防御分支。
        auto latin1 = jstr::latin1_to_mutf8(str.data(), str.byte_length());
        if (latin1.is_err()) {
            throw std::runtime_error("Failed to convert to MUTF-8 (both utf8 and latin1 paths)");
        }
        jstr::MUtf8String mutf8 = latin1.unwrap();
        return putMUtf8(reinterpret_cast<const u8*>(mutf8.data()), static_cast<i32>(mutf8.size()));
    }
    jstr::MUtf8String mutf8 = result.unwrap();
    return putMUtf8(reinterpret_cast<const u8*>(mutf8.data()), static_cast<i32>(mutf8.size()));
}

ByteVector& ByteVector::putByteArray(const u8* src, i32 offset, i32 byteLength)
{
    if (byteLength == 0) return *this;
    if (length_ + byteLength > capacity_) {
        enlarge(byteLength);
    }
    if (src != nullptr) {
        std::memcpy(data_.data_ + length_, src + offset, byteLength);
    }
    else {
        std::memset(data_.data_ + length_, 0, byteLength);
    }
    length_ += byteLength;
    return *this;
}

ByteVector& ByteVector::putByteArray(const std::vector<u8>& bytes)
{
    return putByteArray(bytes.data(), 0, static_cast<i32>(bytes.size()));
}

// ===== 私有方法 =====

void ByteVector::enlarge(i32 size)
{
    if (length_ > capacity_) {
        throw std::runtime_error("Internal error: ByteVector: length_ > capacity_");
    }
    if (size < 0) {
        throw std::length_error("Internal error: ByteVector: negative enlarge size");
    }
    // 容量运算提升到 i64 中间量：2 * capacity_ 与 length_ + size 在 i32 域
    // 会符号溢出成负数，new u8[负数] 再转 size_t 即天文分配。
    i64 doubleCapacity  = static_cast<i64>(capacity_) * 2;
    i64 minimalCapacity = static_cast<i64>(length_) + static_cast<i64>(size);
    i64 newCapacity     = doubleCapacity > minimalCapacity ? doubleCapacity : minimalCapacity;
    // length_/capacity_ 与 size() 均为 i32：超过可表示范围直接拒绝，不让容量回绕。
    if (newCapacity > static_cast<i64>(std::numeric_limits<i32>::max())) {
        throw std::length_error("ByteVector capacity overflow");
    }
    u8* newData = new u8[static_cast<usize>(newCapacity)];
    std::memcpy(newData, data_.data_, length_);
    delete[] data_.data_;
    data_.data_   = newData;
    data_.length_ = static_cast<usize>(newCapacity);
    capacity_     = static_cast<i32>(newCapacity);
}

}   // namespace jbytecode::core
