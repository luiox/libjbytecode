#include <gtest/gtest.h>
#include "garena/arena.hpp"
#include "test_utils.hpp"

namespace {

class RecordingAnnotationVisitor : public AnnotationVisitor
{
public:
    RecordingAnnotationVisitor()
        : AnnotationVisitor(Opcodes::ASM4)
    {}

    void visit(Utf8StringRef name, const ca::core::Any& value) override
    {
        lastName = name;
        if (const auto* type = value.as<Type>()) {
            // #921 staticArena 退役：无参 getDescriptor() 已删除，检视走注入版（局部 arena）。
            ca::str::Utf8StringArena scratch;
            lastTypeDescriptor = type->getDescriptor(scratch).to_std_string();
        }
    }

    void visitEnum(Utf8StringRef name, Utf8StringRef descriptor, Utf8StringRef value) override
    {
        lastEnumDesc  = descriptor;
        lastEnumValue = value;
    }

    Utf8StringRef lastName;
    std::string   lastTypeDescriptor;
    Utf8StringRef lastEnumDesc;
    Utf8StringRef lastEnumValue;
};

}   // namespace

TEST(AnnotationNodeTest, testConstructor)
{
    garena::Arena  nodeArena{256};
    AnnotationNode annotationNode(nodeArena, ZUtf8StringRef::from_static("LI;"));
    ASSERT_EQ(ZUtf8StringRef::from_static("LI;"), annotationNode.desc);
}

TEST(AnnotationNodeTest, preservesClassLiteralElement)
{
    garena::Arena  nodeArena{256};
    AnnotationNode annotationNode(nodeArena, ZUtf8StringRef::from_static("Lpkg/Anno;"));
    annotationNode.visit(ZUtf8StringRef::from_static("value"),
                         Type::getType(ZUtf8StringRef::from_static("Lpkg/Old;")));

    ASSERT_EQ(1u, annotationNode.elements.size());
    EXPECT_EQ('c', annotationNode.elements[0].tag);
    EXPECT_TRUE(
        annotationNode.elements[0].sval.ref().equals(ZUtf8StringRef::from_static("Lpkg/Old;")));

    RecordingAnnotationVisitor visitor;
    annotationNode.accept(&visitor);

    EXPECT_TRUE(visitor.lastName.equals(ZUtf8StringRef::from_static("value")));
    EXPECT_EQ("Lpkg/Old;", visitor.lastTypeDescriptor);
}

// 枚举值含非 UTF-8 字节（WTF-8 孤立代理 ED A0 80）时按原始字节保留：visitEnum
// 与 visit 字符串面同款字节直存，校验失败路径不得静默清空载荷；accept 侧按
// 首个空格字节切分，desc/value 无损往返。
TEST(AnnotationNodeTest, visitEnumPreservesNonUtf8ValueBytes)
{
    garena::Arena  nodeArena{256};
    AnnotationNode annotationNode(nodeArena, ZUtf8StringRef::from_static("LI;"));

    const unsigned char       lone[] = {0xED, 0xA0, 0x80};
    const ca::str::Utf8String loneStr =
        ca::str::Utf8String::from_data_unchecked(lone, sizeof(lone));
    annotationNode.visitEnum(ZUtf8StringRef::from_static("value"),
                             ZUtf8StringRef::from_static("LFoo;"), loneStr.ref());

    ASSERT_EQ(1u, annotationNode.elements.size());
    EXPECT_EQ('e', annotationNode.elements[0].tag);
    const auto stored = annotationNode.elements[0].sval.ref();
    ASSERT_EQ(9u, stored.byte_length());   // "LFoo; " 6 字节 + 值 3 字节
    EXPECT_EQ(0x3B, stored.byte_at(4));    // ';'
    EXPECT_EQ(0x20, stored.byte_at(5));    // 切分空格
    EXPECT_EQ(0xED, stored.byte_at(6));
    EXPECT_EQ(0xA0, stored.byte_at(7));
    EXPECT_EQ(0x80, stored.byte_at(8));

    RecordingAnnotationVisitor visitor;
    annotationNode.accept(&visitor);

    EXPECT_TRUE(visitor.lastEnumDesc.equals(ZUtf8StringRef::from_static("LFoo;")));
    ASSERT_EQ(3u, visitor.lastEnumValue.byte_length());
    EXPECT_EQ(0xED, visitor.lastEnumValue.byte_at(0));
    EXPECT_EQ(0xA0, visitor.lastEnumValue.byte_at(1));
    EXPECT_EQ(0x80, visitor.lastEnumValue.byte_at(2));
}
