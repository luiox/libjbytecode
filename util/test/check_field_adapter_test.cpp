#include <gtest/gtest.h>
#include "test_utils.hpp"
#include "jbytecode/util/check_adapters.hpp"

TEST(CheckFieldAdapterTest, testConstructor)
{
    jbytecode::util::CheckFieldAdapter cfa(nullptr);
    SUCCEED();
}

TEST(CheckFieldAdapterTest, testVisitEnd)
{
    jbytecode::util::CheckFieldAdapter cfa(nullptr);
    EXPECT_NO_THROW(cfa.visitEnd());
}
