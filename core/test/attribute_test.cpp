#include <gtest/gtest.h>
#include "jbytecode/core/primitives.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

namespace {

// 全测试套共享 arena（gtest 串行，intern 累积无害）。
ca::str::Utf8StringArena arena;

class ByteAttribute : public Attribute
{
public:
    explicit ByteAttribute(ca::u8 value)
        : Attribute(ca::str::ZUtf8StringRef::from_static("Byte"))
        , value_(value)
    {}

    ByteVector write(ClassWriter& classWriter, const std::vector<ca::u8>& code, int codeLength,
                     int maxStack, int maxLocals) override
    {
        (void)classWriter;
        (void)code;
        (void)codeLength;
        (void)maxStack;
        (void)maxLocals;
        ByteVector result;
        result.putByte(value_++);
        return result;
    }

private:
    ca::u8 value_;
};

}   // namespace

TEST(AttributeTest, testIsUnknown)
{
    ASSERT_TRUE(Attribute(ca::str::ZUtf8StringRef::from_static("Comment")).isUnknown());
}

TEST(AttributeTest, testCommentIsUnknown)
{
    ASSERT_FALSE(CommentAttribute().isUnknown());
}

TEST(AttributeTest, testCodeCommentIsCodeAttribute)
{
    ASSERT_TRUE(CodeCommentAttribute().isCodeAttribute());
}

TEST(AttributeTest, testCodeCommentWrite)
{
    ClassWriter          classWriter(arena, 0);
    CodeCommentAttribute attr;
    ByteVector           result = attr.write(classWriter, {}, 0, -1, -1);
    ASSERT_EQ(0, result.size());
}

TEST(AttributeTest, testGetLabels)
{
    ASSERT_TRUE(Attribute(ca::str::ZUtf8StringRef::from_static("Comment")).getLabels().empty());
}

TEST(AttributeTest, testStaticWrite)
{
    ClassWriter   classWriter(arena, 0);
    ByteAttribute attribute(42);

    ByteVector content0 = Attribute::write(attribute, classWriter, {}, -1, -1, -1);
    ByteVector content1 = Attribute::write(attribute, classWriter, {}, -1, -1, -1);

    ASSERT_EQ(42, content0.data()[0]);
    ASSERT_EQ(42, content1.data()[0]);
}

TEST(AttributeTest, testCachedContent)
{
    ClassWriter   classWriter(arena, 0);
    ByteAttribute attributes(42);
    ByteAttribute next(123);
    attributes.nextAttribute = &next;
    ByteVector result;

    int size = attributes.computeAttributesSize(classWriter, {}, -1, -1, -1);
    attributes.putAttributes(classWriter, result);

    ASSERT_EQ(14, size);
    ASSERT_EQ(42, result.data()[6]);
    ASSERT_EQ(123, result.data()[13]);
}
