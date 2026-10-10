-- libjbytecode commons 子库（ASM commons 等价）
-- 实用类适配器：Remapper、GeneratorAdapter、AdviceAdapter 等

target("libjbytecode_commons")
    set_kind("static")
    add_packages("libca", "libjstr", "libgarena", {public = true})
    add_deps("libjbytecode_core", "libjbytecode_tree")
    add_includedirs("src", {public = true})
    add_files("src/jbytecode/commons/*.cpp")
    add_headerfiles("src/(jbytecode/commons/**.hpp)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

-- commons 子库单元测试
-- gtest 面随 with_tests 门控：默认配置不定义本 target、不拉 gtest。
if has_config("with_tests") then
target("libjbytecode_commons_unittest")
    set_kind("binary")
    set_default(false)

    add_deps("libjbytecode_commons", "libjbytecode_tree", "libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena", "gtest")
    add_files("test/main.cpp", "test/*_test.cpp")
    add_includedirs("test", "../src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
    if is_plat("linux") then
        add_syslinks("pthread")
    end
end
