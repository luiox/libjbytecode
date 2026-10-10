/// @file        byte_vector.hpp
/// @author      canrad (1517807724@qq.com)
/// @brief       大端字节写入器，ca::u8* 原始缓冲区 (ASM 风格)
/// @version     0.2
/// @date        2026-05-31
/// @copyright   Copyright (c) 2026
///
/// 提供 JVM class 文件格式所需的大端序写入方法。
/// 所有公共方法返回 ByteVector& 以支持链式调用。
/// 实现全部位于 byte_vector.cpp。
///
/// 内部存储对标 Java ASM ByteVector：
///   ca::u8* data_   ↔ byte[] data
///   ca::i32 length_ ↔ int length
///   ca::i32 capacity_ (隐式: data_.length)
#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include "jbytecode/core/byte_array.hpp"
#include "libca/core/datatype.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::core {

class ByteVector
{
public:
    /// 内部缓冲区
    ca::core::ByteArray data_;

    /// 当前已写入字节数
    ca::i32 length_ = 0;

    /// 当前缓冲区容量
    ca::i32 capacity_ = 0;

    /// @brief 构造空向量，预分配 64 字节
    ByteVector();

    /// @brief 构造空向量，指定初始容量
    explicit ByteVector(ca::i32 initialCapacity);

    /// @brief 获得已初始化的数据并接管所有权（不拷贝）
    /// Analogous to Java ByteVector(byte[] data) which sets length = data.length
    /// and uses the array as-is (no remaining capacity).
    ByteVector(ca::u8* data, ca::i32 length);

    ~ByteVector();

    ByteVector(const ByteVector& other)            = delete;
    ByteVector& operator=(const ByteVector& other) = delete;
    ByteVector(ByteVector&& other) noexcept;
    ByteVector& operator=(ByteVector&& other) noexcept;

    /// @return 当前已写入的字节数
    ca::i32 size() const { return length_; }

    /// @return 内部缓冲区指针（非拥有）
    ca::u8* data() const;

    /// @return 内部缓冲区可变指针
    ca::u8* mutableData() const;

    // ---- put* 方法 ----

    /// @brief 写入 1 字节
    ByteVector& putByte(int byteValue);

    /// @brief 连续写入 2 字节（Java: put11）
    ByteVector& put11(int byteValue1, int byteValue2);

    /// @brief 写入 1 个 short（大端 2 字节）
    ByteVector& putShort(int shortValue);

    /// @brief 写入 1 字节 + 1 short（Java: put12）
    ByteVector& put12(int byteValue, int shortValue);

    /// @brief 写入 2 字节 + 1 short（Java: put112）
    ByteVector& put112(int byteValue1, int byteValue2, int shortValue);

    /// @brief 写入 1 个 int（大端 4 字节）
    ByteVector& putInt(int intValue);

    /// @brief 写入 1 字节 + 2 short（Java: put122）
    ByteVector& put122(int byteValue, int shortValue1, int shortValue2);

    /// @brief 写入 1 个 long（大端 8 字节）
    ByteVector& putLong(ca::i64 longValue);

    /// @brief 写入 modified UTF-8 字符串
    ByteVector& putUTF8(const ca::str::Utf8StringRef& str);

    /// @brief 写入字节数组
    ByteVector& putByteArray(const ca::u8* src, ca::i32 offset, ca::i32 byteLength);

    /// @brief 写入 std::vector<ca::u8> 全部内容（C++ 便利方法，原版 Java 无此重载）
    ByteVector& putByteArray(const std::vector<ca::u8>& bytes);

private:
    void enlarge(ca::i32 size);

    // Helper: write raw MUTF-8 bytes (not in original Java ByteVector)
    ByteVector& putMUtf8(const ca::u8* data, ca::i32 byteLength);
};

}   // namespace jbytecode::core
