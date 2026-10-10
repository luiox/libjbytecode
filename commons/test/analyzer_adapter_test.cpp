#include <gtest/gtest.h>
#include <stdexcept>
#include "test_utils.hpp"

TEST(AnalyzerAdapterTest, testConstructor)
{
    AnalyzerAdapter aa(ZUtf8StringRef::from_static("pkg/Class"),
                       Opcodes::ACC_PUBLIC,
                       ZUtf8StringRef::from_static("name"),
                       ZUtf8StringRef::from_static("()V"),
                       nullptr);
    EXPECT_EQ(ZUtf8StringRef::from_static("pkg/Class"), aa.owner_);
}

TEST(AnalyzerAdapterTest, testVisitFrame_emptyFrame)
{
    AnalyzerAdapter aa(ZUtf8StringRef::from_static("pkg/Class"),
                       Opcodes::ACC_PUBLIC,
                       ZUtf8StringRef::from_static("name"),
                       ZUtf8StringRef::from_static("()V"),
                       nullptr);
    EXPECT_NO_THROW(aa.visitFrame(jbytecode::core::Opcodes::F_NEW, 0, {}, 0, {}));
}

TEST(AnalyzerAdapterTest, testVisitFrame_invalidFrameType)
{
    AnalyzerAdapter aa(ZUtf8StringRef::from_static("pkg/Class"),
                       Opcodes::ACC_PUBLIC,
                       ZUtf8StringRef::from_static("name"),
                       ZUtf8StringRef::from_static("()V"),
                       nullptr);
    EXPECT_THROW(aa.visitFrame(jbytecode::core::Opcodes::F_FULL, 0, {}, 0, {}),
                 std::invalid_argument);
}

// 字符串线示范(总方案 §5.1):AnalyzerAdapter 的 arena 注入 —— ANEWARRAY 合成
// 描述符 intern 进注入的作业 arena;进程级 staticArena 已随 #921 批次2 删除,
// 注入是唯一 intern 落点(结构保证,无池可泄)。
TEST(AnalyzerAdapterTest, anewArrayInternsIntoInjectedArena)
{
    Utf8StringArena arena;
    AnalyzerAdapter aa(ZUtf8StringRef::from_static("pkg/Class"),
                       Opcodes::ACC_PUBLIC,
                       ZUtf8StringRef::from_static("name"),
                       ZUtf8StringRef::from_static("()V"),
                       nullptr,
                       &arena);
    aa.stack_.emplace_back(ca::core::Any(1));   // push 是 private:直接压操作数(count)
    aa.visitTypeInsn(Opcodes::ANEWARRAY, ZUtf8StringRef::from_static("com/foo/Only"));

    ASSERT_EQ(1, aa.stack_.size());
    const auto* desc = aa.stack_.front().as<Utf8StringRef>();
    ASSERT_NE(nullptr, desc);
    const std::string got(reinterpret_cast<const char*>(desc->data()), desc->byte_length());
    EXPECT_EQ("[Lcom/foo/Only;", got);
    EXPECT_GT(arena.size(), 0);
}
