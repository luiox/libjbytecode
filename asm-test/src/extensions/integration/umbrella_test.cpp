#include <gtest/gtest.h>

// 只包含聚合头：本文件不引入子模块头或测试辅助头，编译通过即证明
// jbytecode/jbytecode.hpp 对全部公共 API 自足。
#include "jbytecode/jbytecode.hpp"

TEST(UmbrellaHeaderTest, exposesPublicTypes)
{
    EXPECT_NE(nullptr, jbytecode::get_lib_version());

    auto typeRef = jbytecode::core::TypeReference::newTypeReference(
        jbytecode::core::TypeReference::CLASS_EXTENDS);
    EXPECT_EQ(jbytecode::core::TypeReference::CLASS_EXTENDS, typeRef.getSort());

    jbytecode::commons::ModuleTargetAttribute target("linux");
    EXPECT_EQ("linux", target.platform);

    auto empty = jbytecode::util::Util::asArrayList<ca::i32>(nullptr, 0);
    EXPECT_TRUE(empty.empty());

    auto issues = jbytecode::core::sanitizeClassFile(nullptr, 0);
    ASSERT_EQ(1u, issues.size());
    EXPECT_EQ(jbytecode::core::SanitizeIssueKind::BadHeader, issues[0].kind);

    jbytecode::core::BoundedCommonSuperClassProvider bounded(nullptr, 8);
    EXPECT_EQ(0, bounded.calls());

    jbytecode::util::ASMifier asmifier;
    EXPECT_TRUE(asmifier.text.empty());

    EXPECT_THROW(throw jbytecode::core::ClassTooLargeException("C", 70000),
                 std::length_error);
    EXPECT_THROW(throw jbytecode::analysis::AnalyzerException("boom"),
                 jbytecode::analysis::AnalyzerException);
}
