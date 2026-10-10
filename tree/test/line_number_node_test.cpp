#include <gtest/gtest.h>
#include "test_utils.hpp"


TEST(LineNumberNodeTest, testConstructor)
{
    Label*         label = new Label();
    LabelNode      labelNode(label);
    LineNumberNode lineNumberNode(42, label);
    ASSERT_EQ(42, lineNumberNode.line);
    ASSERT_EQ(label, lineNumberNode.start);
    ASSERT_EQ(AbstractInsnNode::LINE, lineNumberNode.getType());
}
