-- libjbytecode analysis 子库（ASM analysis 等价）
-- 字节码分析框架：Analyzer、Interpreter、BasicVerifier 等

target("libjbytecode_analysis")
    set_kind("static")
    add_packages("libca", "libjstr", "libgarena", {public = true})
    add_deps("libjbytecode_core", "libjbytecode_tree")
    add_includedirs("src", {public = true})
    add_files("src/jbytecode/analysis/*.cpp")
    add_headerfiles("src/(jbytecode/analysis/**.hpp)", "src/(jbytecode/analysis/**.inl)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

if has_config("with_tests") then
target("libjbytecode_analysis_unittest")
    set_kind("binary")
    set_default(false)

    add_deps("libjbytecode_analysis", "libjbytecode_tree", "libjbytecode_core")
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
