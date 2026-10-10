#include <gtest/gtest.h>
#include "garena/arena.hpp"
#include "test_utils.hpp"


TEST(RecordComponentNodeTest, testConstructor)
{
    garena::Arena       contentArena{256};
    RecordComponentNode componentNode(contentArena,
                                      ZUtf8StringRef::from_static("component"),
                                      ZUtf8StringRef::from_static("I"),
                                      ZUtf8StringRef::from_static(""));
    ASSERT_EQ(ZUtf8StringRef::from_static("component"), componentNode.name);
    ASSERT_EQ(ZUtf8StringRef::from_static("I"), componentNode.descriptor);
}
