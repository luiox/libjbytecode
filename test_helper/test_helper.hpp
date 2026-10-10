#pragma once

// test_helper：libjbytecode 独立仓测试门面。
//
// 薄封装 libca.test（header-only）的多项目测试布局机制：资源定位 / 输出根。
// API 形态保持 namespace test:: 与 camelCase 命名，只保留本库测试用到的面。
//
// asm-test 特例定位：asm-test/ 目录有自己的 .project_root_file（项目名
// "libjbytecode_asm_test"），跨项目定位经 ca::test 项目注册解析；若注册不可达
// （如独立运行单个测试可执行文件），回落 <top>/asm-test/<filename>。
//
// 使用约定：测试 target 须 set_rundir("$(projectdir)")，
// main 里先 test::setup("<子项目名>")。

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "libca/test/test.hpp"

namespace test {

inline void setup(const std::string& subproject)
{
    ca::test::setup(subproject);
}

inline std::filesystem::path getTopProjectPath()
{
    return ca::test::top_project_path();
}
inline std::filesystem::path getCurrentProjectPath()
{
    return ca::test::current_project_path();
}

inline std::filesystem::path getResourcePath(const std::string& filename)
{
    return ca::test::resource_path(filename);
}
inline bool hasResource(const std::string& filename)
{
    return ca::test::has_resource(filename);
}
inline std::vector<uint8_t> getResource(const std::string& filename)
{
    return ca::test::resource(filename);
}

// ---- asm-test 特例：预编译样本定位（test_utils.hpp 的 readClassFile 用） ----

inline std::filesystem::path getAsmTestResourcePath(const std::string& filename)
{
    constexpr const char* asmTestProject = "libjbytecode_asm_test";
    if (ca::test::has_project(asmTestProject)) {
        return ca::test::project_path(asmTestProject) / filename;
    }
    return ca::test::top_project_path() / "asm-test" / filename;
}

inline bool hasAsmTestResource(const std::string& filename)
{
    return std::filesystem::exists(getAsmTestResourcePath(filename));
}

inline std::vector<uint8_t> getAsmTestResource(const std::string& filename)
{
    const auto path = getAsmTestResourcePath(filename);
    std::ifstream   in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "test_helper: cannot open asm-test resource: %s\n",
                     path.string().c_str());
        std::abort();
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                                std::istreambuf_iterator<char>());
}

inline std::filesystem::path getDemoOutPath(const std::string& filename)
{
    return ca::test::demo_out_path(filename);
}
inline std::filesystem::path getOutPath(const std::string& filename)
{
    return ca::test::out_path(filename);
}
inline std::filesystem::path getTempOutPath(const std::string& filename)
{
    return ca::test::temp_out_path(filename);
}

}   // namespace test
