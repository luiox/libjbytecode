#include <gtest/gtest.h>
#include "test_utils.hpp"
#include "jbytecode/util/textifier.hpp"

TEST(TextifierTest, testConstructor)
{
    jbytecode::util::Textifier textifier;
    EXPECT_TRUE(textifier.text.empty());
}

TEST(TextifierTest, testVisitProducesText)
{
    jbytecode::util::Textifier textifier;
    textifier.visit(Opcodes::V1_8,
                    Opcodes::ACC_PUBLIC,
                    ZUtf8StringRef::from_static("pkg/Class"),
                    Utf8StringRef(),
                    ZUtf8StringRef::from_static("java/lang/Object"),
                    {});
    EXPECT_FALSE(textifier.text.empty());
}
