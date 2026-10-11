// class_file_sanitizer 单测（#963）。
//
// 本文件只覆盖净化器自身的基础行为（合法零误报 / 结构诊断 / join 契约），
// 全量恶意样本族由 mtest_set malformedFixtureManifest 承载，消费测试在
// 上游反混淆项目的 classfile_sanitize_test.cpp。

#include <vector>

#include <gtest/gtest.h>

#include "jbytecode/core/class_file_sanitizer.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/writers.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

namespace {

using ca::str::Utf8StringArena;

/// Fieldref.class_index 指向 Utf8 的最小畸形类（#953 形态 1，手拼字节）。
std::vector<ca::u8> malformedFieldrefToUtf8()
{
    return {
        0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x00, 0x00, 0x34, 0x00, 0x08,   // header, cp_count=8
        0x01, 0x00, 0x01, 0x41,                                       // 1: Utf8 "A"
        0x07, 0x00, 0x01,                                             // 2: Class -> 1
        0x01, 0x00, 0x10, 'j',  'a',  'v',  'a',  '/',  'l',  'a',  'n', 'g',  '/',
        'O',  'b',  'j',  'e',  'c',  't',                            // 3: Utf8 java/lang/Object
        0x07, 0x00, 0x03,                                             // 4: Class -> 3
        0x01, 0x00, 0x01, 0x78,                                       // 5: Utf8 "x"
        0x0C, 0x00, 0x05, 0x00, 0x05,                                 // 6: NAT(5,5)
        0x09, 0x00, 0x05, 0x00, 0x06,                                 // 7: Fieldref(class=5 Utf8!, nat=6)
        0x00, 0x21,                                                   // ACC_PUBLIC | ACC_SUPER
        0x00, 0x02,                                                   // this = 2
        0x00, 0x04,                                                   // super = 4
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                           // ifaces/fields/methods = 0
        0x00, 0x00,                                                   // attributes = 0
    };
}

}   // namespace

TEST(ClassFileSanitizerTest, validClassFromWriterHasNoIssues)
{
    Utf8StringArena arena;
    jbytecode::core::ClassWriter writer(arena, 0);
    writer.visit(jbytecode::core::Opcodes::V1_8, jbytecode::core::Opcodes::ACC_PUBLIC,
                 ca::str::Utf8StringRef::from_cstr("A"), ca::str::Utf8StringRef::from_cstr(""),
                 ca::str::Utf8StringRef::from_cstr("java/lang/Object"), {});
    writer.visitEnd();
    auto bytes  = writer.toByteArray();
    auto issues = jbytecode::core::sanitizeClassFile(bytes);
    EXPECT_TRUE(issues.empty()) << jbytecode::core::joinSanitizeIssues(issues).c_str();
}

TEST(ClassFileSanitizerTest, detectsRefTagMismatchWithCpIndex)
{
    auto issues = jbytecode::core::sanitizeClassFile(malformedFieldrefToUtf8());
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].kind, jbytecode::core::SanitizeIssueKind::RefTagMismatch);
    EXPECT_EQ(issues[0].cpIndex, 7u);
    const std::string message = issues[0].message.c_str();
    EXPECT_NE(message.find("Fieldref.class_index=5"), std::string::npos) << "actual: " << message;
    EXPECT_NE(message.find("CONSTANT_Utf8"), std::string::npos) << "actual: " << message;
}

TEST(ClassFileSanitizerTest, detectsBadMagic)
{
    std::vector<ca::u8> bytes(32, 0);
    bytes[0] = 0xCA;
    bytes[1] = 0xFE;
    bytes[2] = 0xBA;
    bytes[3] = 0xBF;   // 非 CAFEBABE
    auto issues = jbytecode::core::sanitizeClassFile(bytes);
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].kind, jbytecode::core::SanitizeIssueKind::BadHeader);
}

TEST(ClassFileSanitizerTest, detectsTruncatedUtf8)
{
    std::vector<ca::u8> bytes = {
        0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x00, 0x00, 0x34, 0x00, 0x02,
        0x01, 0xFF, 0xFF, 0x41, 0x41,   // 1: Utf8 声明长度 65535，实际只剩 2 字节
    };
    auto issues = jbytecode::core::sanitizeClassFile(bytes);
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].kind, jbytecode::core::SanitizeIssueKind::LengthOutOfRange);
    EXPECT_EQ(issues[0].cpIndex, 1u);
}

TEST(ClassFileSanitizerTest, emptyInputReportsBadHeader)
{
    auto issues = jbytecode::core::sanitizeClassFile(std::vector<ca::u8>{});
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].kind, jbytecode::core::SanitizeIssueKind::BadHeader);
}

TEST(ClassFileSanitizerTest, joinProducesSingleLineWithSeparator)
{
    std::vector<jbytecode::core::SanitizeIssue> issues;
    issues.push_back({jbytecode::core::SanitizeIssueKind::BadHeader, 0, 0,
                      ca::str::Utf8String::from_cstr("issue one")});
    issues.push_back({jbytecode::core::SanitizeIssueKind::BadCpTag, 5, 12,
                      ca::str::Utf8String::from_cstr("issue two")});
    auto joined = jbytecode::core::joinSanitizeIssues(issues);
    // 单行契约：PassFaultReport::toLine 把 detail 放同一行。
    const std::string text = joined.c_str();
    EXPECT_EQ(text.find('\n'), std::string::npos) << text;
    EXPECT_NE(text.find("2 issue(s): issue one; issue two"), std::string::npos) << text;
}
