#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "jbytecode/analysis/basic_interpreter.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/tree/insn_list.hpp"
#include "libca/str/utf8_string_arena.hpp"

// 字符串线示范(总方案 §5.1)+ #921 staticArena 退役:BasicInterpreter 注入构造 ——
// ANEWARRAY 合成描述符 intern 进注入的作业 arena;兜底构造(无注入)intern 回落
// 实例私有 fallbackArena_,随解释器析构回收。进程级 staticArena 本体已随批次2
// 整体删除——注入/实例池是仅有的落点(结构保证,无池可泄)。
//
// 观测口径:BasicValue 折叠到 REFERENCE_VALUE 单例,不保留具体数组型,故注入效果
// 以 intern 落点(arena.size())判定;测试体内不得调用旧无参 getDescriptor 检视
// (该重载已随 #921 批次2 删除)。

using namespace jbytecode::analysis;
using namespace jbytecode::core;
using namespace jbytecode::tree;

namespace {

ca::str::ZUtf8StringRef ref(const char* value)
{
    return ca::str::ZUtf8StringRef::from_static(value);
}

}   // namespace

TEST(BasicInterpreterArena, anewArrayDescriptorInternsIntoInjectedArena)
{
    ca::str::Utf8StringArena arena;
    BasicInterpreter         interpreter(arena);

    TypeInsnNode anewArray(Opcodes::ANEWARRAY, ref("com/foo/Only"));
    const auto   value = interpreter.unaryOperation(&anewArray, BasicValue::INT_VALUE);
    EXPECT_TRUE(value.isReference());

    // 合成路径 intern 两笔:"Lcom/foo/Only;"(元素描述符)+"[Lcom/foo/Only;"(完整描述符),
    // 全部落到注入 arena。
    EXPECT_EQ(2, arena.size());
}

TEST(BasicInterpreterArena, fallbackCtorKeepsLegacyBehavior)
{
    BasicInterpreter interpreter;   // 兜底:无注入(intern 落实例私有 fallbackArena_,#921)
    TypeInsnNode     anewArray(Opcodes::ANEWARRAY, ref("java/lang/StringBuilder"));
    const auto       value = interpreter.unaryOperation(&anewArray, BasicValue::INT_VALUE);
    EXPECT_TRUE(value.isReference());
}
