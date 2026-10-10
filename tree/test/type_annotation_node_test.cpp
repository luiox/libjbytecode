#include <gtest/gtest.h>
#include "garena/arena.hpp"
#include "test_utils.hpp"


TEST(TypeAnnotationNodeTest, testConstructor)
{
    garena::Arena      contentArena{256};
    TypePath           typePath = TypePath::fromString(ZUtf8StringRef::from_static("["));
    TypeAnnotationNode typeAnnotationNode(contentArena, 123, &typePath,
                                          ZUtf8StringRef::from_static("LI;"));
    ASSERT_EQ(123, typeAnnotationNode.typeRef);
    ASSERT_EQ(ZUtf8StringRef::from_static("LI;"), typeAnnotationNode.desc);
}

TEST(TypeAnnotationNodeTest, ownsTypePathCopy)
{
    garena::Arena      contentArena{256};
    TypePath           typePath = TypePath::fromString(ZUtf8StringRef::from_static("["));
    TypeAnnotationNode typeAnnotationNode(contentArena, 123, &typePath,
                                          ZUtf8StringRef::from_static("LI;"));

    typePath = TypePath::fromString(ZUtf8StringRef::from_static("."));

    ASSERT_NE(nullptr, typeAnnotationNode.typePath);
    EXPECT_EQ("[", typeAnnotationNode.typePath->toString());
}

TEST(TypeAnnotationNodeTest, nullTypePathStaysNull)
{
    garena::Arena      contentArena{256};
    TypeAnnotationNode typeAnnotationNode(contentArena, 123, nullptr,
                                          ZUtf8StringRef::from_static("LI;"));

    EXPECT_EQ(nullptr, typeAnnotationNode.typePath);
}

