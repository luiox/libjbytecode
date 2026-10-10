#include <gtest/gtest.h>
#include "jbytecode/core/primitives.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

TEST(HandleTest, testDeprecatedConstructor)
{
    Handle handle1(Opcodes::H_INVOKEINTERFACE,
                   ZUtf8StringRef::from_static("owner"),
                   ZUtf8StringRef::from_static("name"),
                   ZUtf8StringRef::from_static("descriptor"));
    Handle handle2(Opcodes::H_INVOKESPECIAL,
                   ZUtf8StringRef::from_static("owner"),
                   ZUtf8StringRef::from_static("name"),
                   ZUtf8StringRef::from_static("descriptor"));

    ASSERT_TRUE(handle1.isInterface());
    ASSERT_FALSE(handle2.isInterface());
    ASSERT_EQ("owner.namedescriptor (9 itf)", handle1.toString());
    ASSERT_EQ("owner.namedescriptor (7)", handle2.toString());
}

TEST(HandleTest, testConstructor)
{
    Handle handle(Opcodes::H_GETFIELD,
                  ZUtf8StringRef::from_static("owner"),
                  ZUtf8StringRef::from_static("name"),
                  ZUtf8StringRef::from_static("descriptor"),
                  false);
    ASSERT_EQ(Opcodes::H_GETFIELD, handle.getTag());
    ASSERT_EQ(ZUtf8StringRef::from_static("owner"), handle.getOwner());
    ASSERT_EQ(ZUtf8StringRef::from_static("name"), handle.getName());
    ASSERT_EQ(ZUtf8StringRef::from_static("descriptor"), handle.getDesc());
    ASSERT_FALSE(handle.isInterface());
    ASSERT_EQ("owner.namedescriptor (1)", handle.toString());
}

TEST(HandleTest, testEquals)
{
    Handle handle1(Opcodes::H_GETFIELD,
                   ZUtf8StringRef::from_static("owner"),
                   ZUtf8StringRef::from_static("name"),
                   ZUtf8StringRef::from_static("descriptor"),
                   false);
    Handle handle2(Opcodes::H_GETFIELD,
                   ZUtf8StringRef::from_static("owner"),
                   ZUtf8StringRef::from_static("name"),
                   ZUtf8StringRef::from_static("descriptor"),
                   false);

    ASSERT_TRUE(handle1 == handle2);
    ASSERT_EQ(handle1, handle2);
    ASSERT_EQ(handle1.getTag(), Opcodes::H_GETFIELD);
    ASSERT_EQ(handle1.getOwner(), ZUtf8StringRef::from_static("owner"));
    ASSERT_EQ(handle1.getName(), ZUtf8StringRef::from_static("name"));
    ASSERT_EQ(handle1.getDesc(), ZUtf8StringRef::from_static("descriptor"));
    ASSERT_FALSE(handle1.isInterface());
    ASSERT_FALSE((handle1 == Handle(Opcodes::H_PUTFIELD,
                                    ZUtf8StringRef::from_static("owner"),
                                    ZUtf8StringRef::from_static("name"),
                                    ZUtf8StringRef::from_static("descriptor"),
                                    false)));
    ASSERT_FALSE((handle1 == Handle(Opcodes::H_GETFIELD,
                                    ZUtf8StringRef::from_static("o"),
                                    ZUtf8StringRef::from_static("name"),
                                    ZUtf8StringRef::from_static("descriptor"),
                                    false)));
    ASSERT_FALSE((handle1 == Handle(Opcodes::H_GETFIELD,
                                    ZUtf8StringRef::from_static("owner"),
                                    ZUtf8StringRef::from_static("n"),
                                    ZUtf8StringRef::from_static("descriptor"),
                                    false)));
    ASSERT_FALSE((handle1 == Handle(Opcodes::H_GETFIELD,
                                    ZUtf8StringRef::from_static("owner"),
                                    ZUtf8StringRef::from_static("name"),
                                    ZUtf8StringRef::from_static("d"),
                                    false)));
    ASSERT_FALSE((handle1 == Handle(Opcodes::H_GETFIELD,
                                    ZUtf8StringRef::from_static("owner"),
                                    ZUtf8StringRef::from_static("n"),
                                    ZUtf8StringRef::from_static("descriptor"),
                                    true)));
}

TEST(HandleTest, testHashCode)
{
    Handle handle1(Opcodes::H_INVOKESTATIC,
                   ZUtf8StringRef::from_static("owner"),
                   ZUtf8StringRef::from_static("name"),
                   ZUtf8StringRef::from_static("descriptor"),
                   false);
    Handle handle2(Opcodes::H_INVOKESTATIC,
                   ZUtf8StringRef::from_static("owner"),
                   ZUtf8StringRef::from_static("name"),
                   ZUtf8StringRef::from_static("descriptor"),
                   true);

    ASSERT_NE(0u, handle1.hashCode());
    ASSERT_NE(0u, handle2.hashCode());
}
