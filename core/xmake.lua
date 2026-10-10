-- libjbytecode core 子库（ASM core 等价）+ 其原版对齐测试
-- include 根 = src（即 core/src），源码 include "jbytecode/core/xxx.hpp" 零改动

target("libjbytecode_core")
    set_kind("static")
    add_packages("libca", "libjstr", "libgarena", {public = true})
    add_includedirs("src", {public = true})
    add_files("src/jbytecode/core/*.cpp")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

local unittest_files = {
    "test/main.cpp",
    "test/*_test.cpp",
    "test/signature/*_test.cpp",
}
local unittest_incs = {
    "test",
    "src",
    "$(projectdir)/test_helper",
}

-- gtest 面随 with_tests 门控：默认配置不定义本 target、不拉 gtest。
if has_config("with_tests") then
target("libjbytecode_asm_core_unittest")
    set_kind("binary")
    set_default(false)

    add_deps("libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena", "gtest")
    add_files(unittest_files)
    add_includedirs(unittest_incs)
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
    if is_plat("linux") then
        add_syslinks("pthread")
    end
end
if has_config("with_tests") then
target("libjbytecode_origin_unittest")
    set_kind("binary")
    set_default(false)

    add_deps("libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena", "gtest")
    add_files(unittest_files)
    add_includedirs(unittest_incs)
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
    if is_plat("linux") then
        add_syslinks("pthread")
    end
end

includes("test/crossval")
