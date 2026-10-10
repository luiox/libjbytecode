#include <gtest/gtest.h>
#include "test_utils.hpp"

TEST(ClassNodeTest, testConstructor)
{
    ClassNode classNode;
    ASSERT_TRUE(classNode.interfaces.empty());
    ASSERT_TRUE(classNode.innerClasses.empty());
    ASSERT_TRUE(classNode.fields.empty());
    ASSERT_TRUE(classNode.methods.empty());
}
