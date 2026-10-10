#include <gtest/gtest.h>
#include "test_utils.hpp"
#include "jbytecode/util/check_adapters.hpp"

TEST(CheckModuleAdapterTest, testConstructor)
{
    jbytecode::util::CheckModuleAdapter cma(nullptr);
    SUCCEED();
}
