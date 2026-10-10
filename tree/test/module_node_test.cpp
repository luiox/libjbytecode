#include <gtest/gtest.h>
#include "test_utils.hpp"


TEST(ModuleNodeTest, testConstructor)
{
    ModuleNode moduleNode(
        ZUtf8StringRef::from_static("module1"), 123, ZUtf8StringRef::from_static("1.0"));
    ASSERT_EQ(ZUtf8StringRef::from_static("module1"), moduleNode.name);
    ASSERT_EQ(123, moduleNode.access);
    ASSERT_EQ(ZUtf8StringRef::from_static("1.0"), moduleNode.version);
}
