#include <gtest/gtest.h>
#include "test_utils.hpp"


TEST(TableSwitchInsnNodeTest, testConstructor)
{
    Label*              dflt   = new Label();
    std::vector<Label*> labels = {new Label()};
    TableSwitchInsnNode tableSwitchInsnNode(0, 1, dflt, labels);
    ASSERT_EQ(AbstractInsnNode::TABLESWITCH_INSN, tableSwitchInsnNode.getType());
    ASSERT_EQ(0, tableSwitchInsnNode.min);
    ASSERT_EQ(1, tableSwitchInsnNode.max);
    ASSERT_EQ(dflt, tableSwitchInsnNode.dflt);
    ASSERT_EQ(1, tableSwitchInsnNode.labels.size());
}
