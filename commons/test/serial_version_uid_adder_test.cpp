#include <gtest/gtest.h>
#include "test_utils.hpp"


TEST(SerialVersionUidAdderTest, testConstructor)
{
    ASSERT_NO_THROW({ SerialVersionUIDAdder adder(nullptr); });
}
