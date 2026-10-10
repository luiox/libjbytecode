#include <gtest/gtest.h>
#include "test_utils.hpp"
#include "jbytecode/util/check_adapters.hpp"

TEST(CheckAnnotationAdapterTest, testConstructor)
{
    jbytecode::util::CheckAnnotationAdapter caa(nullptr);
    SUCCEED();
}
