#include <gtest/gtest.h>
#include "test_utils.hpp"
#include "jbytecode/util/check_adapters.hpp"

TEST(CheckSignatureAdapterTest, testConstructor)
{
    jbytecode::util::CheckSignatureAdapter csa(
        jbytecode::util::CheckSignatureAdapter::CLASS_SIGNATURE, nullptr);
    SUCCEED();
}

TEST(CheckSignatureAdapterTest, testClassSignature)
{
    jbytecode::util::CheckSignatureAdapter csa(
        jbytecode::util::CheckSignatureAdapter::CLASS_SIGNATURE, nullptr);
    EXPECT_NO_THROW(csa.visitClassType(ZUtf8StringRef::from_static("java/lang/Object")));
    EXPECT_NO_THROW(csa.visitEnd());
}
