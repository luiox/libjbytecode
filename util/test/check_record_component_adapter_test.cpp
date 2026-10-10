#include <gtest/gtest.h>
#include "test_utils.hpp"
#include "jbytecode/util/check_adapters.hpp"

TEST(CheckRecordComponentAdapterTest, testConstructor)
{
    jbytecode::util::CheckRecordComponentAdapter crca(nullptr);
    SUCCEED();
}
