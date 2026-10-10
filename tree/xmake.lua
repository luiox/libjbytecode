-- libjbytecode tree 子库（ASM tree 等价）
-- Tree API：ClassNode、MethodNode、InsnList 等

target("libjbytecode_tree")
    set_kind("static")
    add_packages("libca", "libjstr", "libgarena", {public = true})
    add_deps("libjbytecode_core")
    add_includedirs("src", {public = true})
    add_files("src/jbytecode/tree/*.cpp")
    add_headerfiles("src/(jbytecode/tree/**.hpp)", "src/(jbytecode/tree/**.inl)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

-- tree 子库单元测试
-- gtest 面随 with_tests 门控：默认配置不定义本 target、不拉 gtest。
if has_config("with_tests") then
target("libjbytecode_tree_unittest")
    set_kind("binary")
    set_default(false)

    add_deps("libjbytecode_tree", "libjbytecode_core")
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
