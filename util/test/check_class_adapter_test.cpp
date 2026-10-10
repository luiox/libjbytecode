#include <gtest/gtest.h>
#include "test_utils.hpp"
#include "jbytecode/util/check_adapters.hpp"

TEST(CheckClassAdapterTest, testConstructor)
{
    jbytecode::util::CheckClassAdapter cca(nullptr);
    SUCCEED();
}
