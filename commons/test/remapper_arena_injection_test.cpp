#include <gtest/gtest.h>
#include <cstdio>
#include <string>
#include <unordered_map>

#include "jbytecode/commons/remapper.hpp"
#include "jbytecode/core/type.hpp"
#include "libca/str/utf8_string_arena.hpp"

// 字符串线示范(总方案 §5.1/§5.2)+ #921 staticArena 退役验收测试:
//   1) 旧兜底路径(无 arena 注入的 remap,intern 进进程级 Type::staticArena())
//      已在 #921 批次1 退役:Remapper/SimpleRemapper 删除无 arena 构造,
//      原 legacyFallbackPathGrowsStaticArena 证据测试随之移除。
//   2) #921 批次2:进程级 staticArena 本体已整体删除——注入是唯一路径,
//      "staticArena 零增长"升级为结构保证(无池可泄),断言改为作业池承载。
//   3) 生命周期:注入产物借用作业 arena,clear 后归还。

namespace {

constexpr int kRounds = 1000;

std::string genName(int i)
{
    // "org/gen/" 前缀命名空间,避免与其它用例的静态内容互相干扰判定。
    return "org/gen/Class" + std::to_string(i);
}

std::unordered_map<std::string, std::string> genMapping(int i)
{
    return {{genName(i), genName(i) + "X"}};
}

// 每轮对四个 intern 点(remapper.cpp 原 57/98/119/179)各触发一次:
// map(改名)、mapDesc(数组/类描述符)、mapMethodDesc(方法描述符)、mapSignature(泛型签名)。
template<typename Remap>
void remapRound(Remap& r, int i)
{
    // 注意:视图不能绑定到语句级临时 std::string;先落名为本轮持有缓冲。
    const std::string nameStr = genName(i);
    const auto&       name    = ca::str::Utf8StringRef::from_cstr(nameStr.c_str());
    (void)r.map(name);
    (void)r.mapDesc(ca::str::Utf8StringRef::from_cstr(("L" + genName(i) + ";").c_str()));
    (void)r.mapMethodDesc(ca::str::Utf8StringRef::from_cstr(("(L" + genName(i) + ";)V").c_str()));
    (void)r.mapSignature(
        ca::str::Utf8StringRef::from_cstr(("L" + genName(i) + "<Ljava/lang/Object;>;").c_str()));
}

}   // namespace

// 注入路径同样负载下 intern 计数落在作业 arena(进程级池已删除,无第二落点)。
TEST(RemapperStaticArena, injectedPathLandsInJobArenaOnly)
{
    ca::str::Utf8StringArena jobArena;
    for (int i = 0; i < kRounds; ++i) {
        jbytecode::commons::SimpleRemapper r(jobArena, genMapping(i));
        remapRound(r, i);
    }

    std::printf("[staticArena|injected] rounds=%d jobArenaInterns=%zu\n",
                kRounds,
                static_cast<size_t>(jobArena.size()));
    EXPECT_GT(jobArena.size(), 0);
}

// 语义验收:注入版对同一负载产出与退役前兜底版一致的改写结果(结果由作业池承载)。
TEST(RemapperArenaInjection, injectedMatchesLegacyResults)
{
    const auto          mapping = genMapping(7);
    const std::string   nameStr = genName(7);
    const auto          name    = ca::str::Utf8StringRef::from_cstr(nameStr.c_str());
    const std::string descStr  = "L" + genName(7) + ";";
    const std::string mdescStr = "(L" + genName(7) + ";)V";
    const std::string sigStr   = "L" + genName(7) + "<Ljava/lang/Object;>;";
    const auto        desc     = ca::str::Utf8StringRef::from_cstr(descStr.c_str());
    const auto        mdesc    = ca::str::Utf8StringRef::from_cstr(mdescStr.c_str());
    const auto        sig      = ca::str::Utf8StringRef::from_cstr(sigStr.c_str());

    ca::str::Utf8StringArena           arena;
    jbytecode::commons::SimpleRemapper injected(arena, mapping);

    const auto renamed = injected.map(name);
    EXPECT_TRUE(
        renamed.equals(ca::str::Utf8StringRef::from_cstr((genName(7) + "X").c_str())));

    // 注入版产物必须落在注入 arena;改名/描述符/签名结果由作业池承载。
    EXPECT_GT(arena.size(), 0);
    const auto descMapped = injected.mapDesc(desc);
    EXPECT_TRUE(descMapped.equals(ca::str::Utf8StringRef::from_cstr(
        ("L" + genName(7) + "X;").c_str())));
    const auto mdescMapped = injected.mapMethodDesc(mdesc);
    EXPECT_TRUE(mdescMapped.equals(ca::str::Utf8StringRef::from_cstr(
        ("(L" + genName(7) + "X;)V").c_str())));
    const auto sigMapped = injected.mapSignature(sig);
    EXPECT_TRUE(sigMapped.equals(ca::str::Utf8StringRef::from_cstr(
        ("L" + genName(7) + "X<Ljava/lang/Object;>;").c_str())));
}

// 生命周期:注入版产物借用作业 arena;arena clear 后池计数归零(借用随作业死亡)。
TEST(RemapperArenaInjection, resultsBorrowInjectedArenaLifetime)
{
    ca::str::Utf8StringArena arena;
    jbytecode::commons::SimpleRemapper r(arena, genMapping(9));
    auto renamed = r.map(ca::str::Utf8StringRef::from_cstr(genName(9).c_str()));
    ASSERT_TRUE(renamed.equals(ca::str::Utf8StringRef::from_cstr((genName(9) + "X").c_str())));
    EXPECT_GT(arena.size(), 0);
    arena.clear();   // 作业结束:renamed 自此失效(不再触碰),池归还
    EXPECT_EQ(0, arena.size());
}
