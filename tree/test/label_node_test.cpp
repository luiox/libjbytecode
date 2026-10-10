#include <gtest/gtest.h>
#include "garena/arena.hpp"
#include "test_utils.hpp"


TEST(LabelNodeTest, testConstructor)
{
    garena::Arena arena{256};
    LabelNode     labelNode1(arena.create<Label>());   // B8b：默认构造已退役，Label 走 arena
    ASSERT_EQ(AbstractInsnNode::LABEL, labelNode1.getType());
    ASSERT_NE(nullptr, labelNode1.getLabel());

    Label*    label = new Label();
    LabelNode labelNode2(label);
    ASSERT_EQ(label, labelNode2.getLabel());
    delete label;
}
