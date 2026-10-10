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

    Utf8StringRef lastName;
    std::string   lastTypeDescriptor;
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
