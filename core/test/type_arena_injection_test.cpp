#include <gtest/gtest.h>

#include "jbytecode/core/type.hpp"
#include "libca/str/utf8_string_arena.hpp"

// 字符串线示范(总方案 §5.1)验收测试:Type 注入重载。
//   - 语义等价:getDescriptor/getInternalName 注入版产物与字面量一致;
//     getClassName()/getMethodType(ret,args) 旧无参版已随 staticArena 退役
//     批次1(#921)删除;无参 getDescriptor()/getInternalName() 与
//     Type::staticArena() 进程级兜底池已随批次2(#921)整体删除——注入版是唯一
//     入口,本文件不再存在旧版比对基准;
//   - 归零:进程级池已不存在,任何路径都不可能再落 staticArena(结构保证,
//     见 script/gates 台账 staticArena 硬门禁);
//   - 生命周期:产物借用注入的作业 arena,arena clear 后随作业死亡。

using namespace jbytecode::core;
using ca::str::Utf8StringArena;
using ca::str::Utf8StringRef;

static const char* kCls = "com/foo/Bar";

// arena 内的 ref 不以 \0 结尾:统一按字节长度比较,避免读越界。
static std::string toStr(const Utf8StringRef& r)
{
    return std::string(reinterpret_cast<const char*>(r.data()), r.byte_length());
}

TEST(TypeArenaInjection, getDescriptorInjectedProducesExpectedString)
{
    Type            t = Type::getObjectType(Utf8StringRef::from_cstr(kCls));
    Utf8StringArena arena;

    const auto injected = t.getDescriptor(arena);
    EXPECT_EQ("Lcom/foo/Bar;", toStr(injected));
}

// 无参 getClassName() 已删除(#921 批次1),基准为字面量。
TEST(TypeArenaInjection, getClassNameInjectedProducesExpectedString)
{
    const Type obj  = Type::getObjectType(Utf8StringRef::from_cstr(kCls));
    const Type arr  = Type::getType(Utf8StringRef::from_cstr("[[Lcom/foo/Bar;"));
    Utf8StringArena arena;

    EXPECT_EQ("com.foo.Bar", toStr(obj.getClassName(arena)));
    EXPECT_EQ("com.foo.Bar[][]", toStr(arr.getClassName(arena)));
}

TEST(TypeArenaInjection, getInternalNameInjectedProducesExpectedString)
{
    const Type arr = Type::getType(Utf8StringRef::from_cstr("[Lcom/foo/Bar;"));
    const Type obj = Type::getObjectType(Utf8StringRef::from_cstr(kCls));
    Utf8StringArena arena;

    // ARRAY 排序 intern 的是 value_ 本身(原始数组描述符,ASM getInternalName 语义,
    // 不展开元素名);与已删无参转发版产出逐字节相同,只是 intern 落点改注入 arena。
    EXPECT_EQ("[Lcom/foo/Bar;", toStr(arr.getInternalName(arena)));
    // OBJECT/INTERNAL 分支本就直接返回 value_,不 intern:注入版语义不变。
    EXPECT_EQ(kCls, toStr(obj.getInternalName(arena)));
}

// getMethodType(ret,args) 旧无参-arena 版已删除(#921 批次1),基准为字面量。
TEST(TypeArenaInjection, getMethodTypeInjectedProducesExpectedDescriptor)
{
    std::vector<Type> args{Type::getObjectType(Utf8StringRef::from_cstr(kCls)),
                           Type::INT_TYPE};
    Utf8StringArena   arena;

    const Type injected = Type::getMethodType(arena, Type::VOID_TYPE, args);
    EXPECT_EQ("(Lcom/foo/Bar;I)V", toStr(injected.getDescriptor(arena)));
}

TEST(TypeArenaInjection, injectedInternsLandInCallerArenaOnly)
{
    Utf8StringArena arena;

    const Type obj = Type::getObjectType(Utf8StringRef::from_cstr("com/foo/Only"));
    const Type arr = Type::getType(Utf8StringRef::from_cstr("[Lcom/foo/Only;"));
    std::vector<Type> args{Type::getObjectType(Utf8StringRef::from_cstr("com/foo/Only"))};

    (void)obj.getDescriptor(arena);
    (void)obj.getClassName(arena);
    (void)obj.getInternalName(arena);
    (void)arr.getDescriptor(arena);
    (void)arr.getClassName(arena);
    (void)arr.getInternalName(arena);
    (void)Type::getMethodType(arena, Type::VOID_TYPE, args);

    // intern 全部落到注入 arena;进程级池已随 #921 批次2 删除,无第二落点可泄。
    EXPECT_GT(arena.size(), 0);
}

TEST(TypeArenaInjection, descriptorLifetimeFollowsInjectedArena)
{
    Utf8StringArena arena;

    Type        t  = Type::getObjectType(Utf8StringRef::from_cstr("com/foo/Life"));
    const auto desc = t.getDescriptor(arena);
    EXPECT_EQ(1, arena.size());
    EXPECT_TRUE(arena.intern("x").data() != nullptr);   // arena 仍可用

    arena.clear();   // 作业结束:desc 自此失效(其后不再触碰),借用随 arena 死亡
    EXPECT_EQ(0, arena.size());
}
