#include <gtest/gtest.h>
#include "test_utils.hpp"

TEST(MethodTest, testConstructor_fromDescriptor)
{
    Method method(ZUtf8StringRef::from_static("name"), ZUtf8StringRef::from_static("(I)J"));
    ASSERT_EQ("name", method.getName().to_std_string());
    ASSERT_EQ(1, (int)method.getArgumentTypes().size());
    ASSERT_EQ(Type::LONG_TYPE, method.getReturnType());
}

TEST(MethodTest, testConstructor_fromType)
{
    std::vector<Type> args = {Type::INT_TYPE};
    Method            method(ZUtf8StringRef::from_static("name"), Type::LONG_TYPE, args);
    ASSERT_EQ("name", method.getName().to_std_string());
    ASSERT_EQ(1, (int)method.getArgumentTypes().size());
    ASSERT_EQ(Type::LONG_TYPE, method.getReturnType());
}

TEST(MethodTest, testConstructor_getDescriptor_roundtrip)
{
    Method method(ZUtf8StringRef::from_static("name"), ZUtf8StringRef::from_static("(I)J"));
    ASSERT_EQ("(I)J", method.getDescriptor().to_std_string());
}

TEST(MethodTest, testEquals)
{
    ASSERT_TRUE(Method(ZUtf8StringRef::from_static("name"), ZUtf8StringRef::from_static("()V")) ==
                Method(ZUtf8StringRef::from_static("name"), ZUtf8StringRef::from_static("()V")));
    ASSERT_FALSE(Method(ZUtf8StringRef::from_static("name"), ZUtf8StringRef::from_static("()V")) ==
                 Method(ZUtf8StringRef::from_static("other"), ZUtf8StringRef::from_static("()V")));
    ASSERT_FALSE(Method(ZUtf8StringRef::from_static("name"), ZUtf8StringRef::from_static("()V")) ==
                 Method(ZUtf8StringRef::from_static("name"), ZUtf8StringRef::from_static("(I)J")));
}
