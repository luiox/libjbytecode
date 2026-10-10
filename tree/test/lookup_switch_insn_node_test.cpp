#include <gtest/gtest.h>
#include "test_utils.hpp"


TEST(LookupSwitchInsnNodeTest, testConstructor)
{
    Label*               dflt   = new Label();
    std::vector<int>     keys   = {1};
    std::vector<Label*>  labels = {new Label()};
    LookupSwitchInsnNode lookupSwitchInsnNode(dflt, keys, labels);
    ASSERT_EQ(AbstractInsnNode::LOOKUPSWITCH_INSN, lookupSwitchInsnNode.getType());
    ASSERT_EQ(dflt, lookupSwitchInsnNode.dflt);
    ASSERT_EQ(1, lookupSwitchInsnNode.keys.size());
    ASSERT_EQ(1, lookupSwitchInsnNode.keys[0]);
    ASSERT_EQ(1, lookupSwitchInsnNode.labels.size());
}
