#include <gtest/gtest.h>
#include "jbytecode/core/visitors.hpp"

using namespace jbytecode::core;

TEST(SignatureVisitorTest, testConstructor_invalidApi)
{
    try {
        SignatureVisitor sv(0);
        FAIL() << "Expected std::invalid_argument";
    }
    catch (const std::invalid_argument& e) {
        ASSERT_STREQ("Unsupported api 0", e.what());
    }
}
