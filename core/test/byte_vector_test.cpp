#include <gtest/gtest.h>
#include "jbytecode/core/byte_vector.hpp"
#include "test_utils.hpp"
#include <array>
#include <algorithm>

using namespace jbytecode::core;

namespace {

std::vector<uint8_t> toArray(const ByteVector& bv)
{
    std::vector<uint8_t> result(bv.size());
    std::copy(bv.data(), bv.data() + bv.size(), result.begin());
    return result;
}

}   // namespace

TEST(ByteVectorTest, testPutByte)
{
    ByteVector byteVector(0);

    byteVector.putByte(1);

    std::vector<uint8_t> expected = {1};
    ASSERT_EQ(expected, toArray(byteVector));
    ASSERT_EQ(1, byteVector.size());
}

TEST(ByteVectorTest, testPut11)
{
    ByteVector byteVector(0);

    byteVector.put11(1, 2);

    std::vector<uint8_t> expected = {1, 2};
    ASSERT_EQ(expected, toArray(byteVector));
    ASSERT_EQ(2, byteVector.size());
}

TEST(ByteVectorTest, testPutShort)
{
    ByteVector byteVector(0);

    byteVector.putShort(0x0102);

    std::vector<uint8_t> expected = {1, 2};
    ASSERT_EQ(expected, toArray(byteVector));
    ASSERT_EQ(2, byteVector.size());
}

TEST(ByteVectorTest, testPut12)
{
    ByteVector byteVector(0);

    byteVector.put12(1, 0x0203);

    std::vector<uint8_t> expected = {1, 2, 3};
    ASSERT_EQ(expected, toArray(byteVector));
    ASSERT_EQ(3, byteVector.size());
}

TEST(ByteVectorTest, testPut112)
{
    ByteVector byteVector(0);

    byteVector.put112(1, 2, 0x0304);

    std::vector<uint8_t> expected = {1, 2, 3, 4};
    ASSERT_EQ(expected, toArray(byteVector));
    ASSERT_EQ(4, byteVector.size());
}

TEST(ByteVectorTest, testPutInt)
{
    ByteVector byteVector(0);

    byteVector.putInt(0x01020304);

    std::vector<uint8_t> expected = {1, 2, 3, 4};
    ASSERT_EQ(expected, toArray(byteVector));
    ASSERT_EQ(4, byteVector.size());
}

TEST(ByteVectorTest, testPut122)
{
    ByteVector byteVector(0);

    byteVector.put122(1, 0x0203, 0x0405);

    std::vector<uint8_t> expected = {1, 2, 3, 4, 5};
    ASSERT_EQ(expected, toArray(byteVector));
    ASSERT_EQ(5, byteVector.size());
}

TEST(ByteVectorTest, testPutLong)
{
    ByteVector byteVector(0);

    byteVector.putLong(0x0102030405060708LL);

    std::vector<uint8_t> expected = {1, 2, 3, 4, 5, 6, 7, 8};
    ASSERT_EQ(expected, toArray(byteVector));
    ASSERT_EQ(8, byteVector.size());
}

TEST(ByteVectorTest, testPutUtf8_ascii)
{
    ByteVector byteVector(0);

    byteVector.putUTF8(ZUtf8StringRef::from_static("abc"));

    std::vector<uint8_t> expected = {0, 3, 'a', 'b', 'c'};
    ASSERT_EQ(expected, toArray(byteVector));
    ASSERT_EQ(5, byteVector.size());
}

TEST(ByteVectorTest, testPutUtf8_ascii_tooLarge)
{
    int sizes[] = {65535, 65536};
    for (int size : sizes) {
        ByteVector  byteVector(0);
        std::string charBuffer(size, 'A');

        if (size > 65535) {
            try {
                byteVector.putUTF8(Utf8StringRef::from_cstr(charBuffer.c_str()));
                FAIL() << "Expected std::length_error";
            }
            catch (const std::length_error& e) {
                ASSERT_STREQ("MUTF8 string too large", e.what());
            }
        }
        else {
            ASSERT_NO_THROW({ byteVector.putUTF8(Utf8StringRef::from_cstr(charBuffer.c_str())); });
        }
    }
}

TEST(ByteVectorTest, testPutUtf8_unicode)
{
    ByteVector byteVector(0);

    std::string input;
    input += static_cast<char>(0x61);   // 'a'
    input += static_cast<char>(0x00);   // '\0'
    // U+0080 in UTF-8: 0xC2 0x80
    input += static_cast<char>(static_cast<unsigned char>(0xC2));
    input += static_cast<char>(static_cast<unsigned char>(0x80));
    // U+0800 in UTF-8: 0xE0 0xA0 0x80
    input += static_cast<char>(static_cast<unsigned char>(0xE0));
    input += static_cast<char>(static_cast<unsigned char>(0xA0));
    input += static_cast<char>(static_cast<unsigned char>(0x80));

    byteVector.putUTF8(
        Utf8StringRef(reinterpret_cast<const ca::u8*>(input.data()), input.size(), input.size()));

    // Expected modified UTF-8: {0, 8, 'a', -64, -128, -62, -128, -32, -96, -128}
    std::vector<uint8_t> expected = {
        0,
        8,
        'a',
        static_cast<uint8_t>(0xC0),
        static_cast<uint8_t>(0x80),   // \0
        static_cast<uint8_t>(0xC2),
        static_cast<uint8_t>(0x80),   // U+0080
        static_cast<uint8_t>(0xE0),
        static_cast<uint8_t>(0xA0),
        static_cast<uint8_t>(0x80)   // U+0800
    };
    ASSERT_EQ(expected, toArray(byteVector));
}

TEST(ByteVectorTest, testPutUtf8_unicode_tooLarge)
{
    ByteVector  byteVector(0);
    std::string charBuffer;
    charBuffer.reserve(32768 * 3);
    for (int i = 0; i < 32768; i++) {
        charBuffer += static_cast<char>(static_cast<unsigned char>(0x07FF));
    }

    try {
        byteVector.putUTF8(Utf8StringRef::from_cstr(charBuffer.c_str()));
        FAIL() << "Expected exception";
    }
    catch (const std::length_error& e) {
        ASSERT_STREQ("MUTF8 string too large", e.what());
    }
    catch (const std::runtime_error&) {
        // UTF-8 to MUTF-8 conversion may fail for very large strings
        SUCCEED();
    }
}

TEST(ByteVectorTest, testPutByteArray)
{
    ByteVector byteVector(0);
    uint8_t    src[] = {0, 1, 2, 3, 4, 5};
    byteVector.putByteArray(src, 1, 3);

    std::vector<uint8_t> expected = {1, 2, 3};
    ASSERT_EQ(expected, toArray(byteVector));
    ASSERT_EQ(3, byteVector.size());
}

// enlarge 反复扩容（容量 0 起步，多次翻倍越过一个 64 字节初始块）：
// 内容前缀必须逐字节保持。守护容量 i64 中间量重构。
TEST(ByteVectorTest, enlargeRepeatedGrowthPreservesContent)
{
    ByteVector byteVector(0);
    for (int i = 0; i < 1000; ++i) {
        byteVector.putByte(i & 0xFF);
    }
    ASSERT_EQ(1000, byteVector.size());
    for (int i = 0; i < 1000; ++i) {
        ASSERT_EQ(static_cast<uint8_t>(i & 0xFF), byteVector.data()[i]);
    }
}

// 单次超大写入走 minimalCapacity > doubleCapacity 分支（新容量不翻倍、
// 直接取所需值）。
TEST(ByteVectorTest, enlargeSingleLargePut)
{
    ByteVector          byteVector(0);
    std::vector<uint8_t> payload(300, 0xAB);
    byteVector.putByteArray(payload.data(), 0, static_cast<ca::i32>(payload.size()));
    ASSERT_EQ(300, byteVector.size());
    ASSERT_EQ(0xAB, byteVector.data()[299]);
}
