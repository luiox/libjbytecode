#include <gtest/gtest.h>
#include "test_utils.hpp"

TEST(FieldNodeTest, testConstructor)
{
    FieldNode fieldNode(123,
                        ZUtf8StringRef::from_static("field"),
                        ZUtf8StringRef::from_static("I"),
                        ZUtf8StringRef::from_static(""),
                        ca::core::Any{});
    ASSERT_EQ(123, fieldNode.access);
    ASSERT_EQ(ZUtf8StringRef::from_static("field"), fieldNode.name);
    ASSERT_EQ(ZUtf8StringRef::from_static("I"), fieldNode.desc);
}
