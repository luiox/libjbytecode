-- asm-test: extension tests for libjbytecode (ASM alignment/integration testing)
-- gtest 面随 with_tests 门控：默认配置不定义本 target、不拉 gtest。
if has_config("with_tests") then
target("libjbytecode_core_unittest")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_util", "libjbytecode_commons", "libjbytecode_analysis",
             "libjbytecode_tree", "libjbytecode_core", "libjbytecode")
    add_packages("libca", "libjstr", "libgarena", "gtest")
    add_files("src/main.cpp")
    add_files("src/extensions/core/*_test.cpp")
    add_files("src/extensions/integration/*_test.cpp")
    add_includedirs("src", "$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
    if is_plat("linux") then
        add_syslinks("pthread")
    end
end
