-- demo：libjbytecode 用法示例（--with_demo=y 启用；Jar 系依赖 libjzip 包）。

target("demo_ClassReadWrite")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_util", "libjbytecode_commons", "libjbytecode_analysis", "libjbytecode_tree", "libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena")
    add_files("ClassReadWrite.cpp")
    add_includedirs("$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("demo_MethodInstrument")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_util", "libjbytecode_commons", "libjbytecode_analysis", "libjbytecode_tree", "libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena")
    add_files("MethodInstrument.cpp")
    add_includedirs("$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("demo_ClassRemapper")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_util", "libjbytecode_commons", "libjbytecode_analysis", "libjbytecode_tree", "libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena")
    add_files("ClassRemapper.cpp")
    add_includedirs("$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("demo_FrameAnalysis")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_util", "libjbytecode_commons", "libjbytecode_analysis", "libjbytecode_tree", "libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena")
    add_files("FrameAnalysis.cpp")
    add_includedirs("$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("demo_ClassGeneration")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_util", "libjbytecode_commons", "libjbytecode_analysis", "libjbytecode_tree", "libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena")
    add_files("ClassGeneration.cpp")
    add_includedirs("$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("demo_JarManipulate")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_util", "libjbytecode_commons", "libjbytecode_analysis", "libjbytecode_tree", "libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena", "libjzip")
    add_files("JarManipulate.cpp")
    add_includedirs("$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("demo_JarTreeManipulate")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_util", "libjbytecode_commons", "libjbytecode_analysis", "libjbytecode_tree", "libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena", "libjzip")
    add_files("JarTreeManipulate.cpp")
    add_includedirs("$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("demo_FrameRecompute")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_util", "libjbytecode_commons", "libjbytecode_analysis", "libjbytecode_tree", "libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena", "libjzip")
    add_files("FrameRecomputeDemo.cpp")
    add_includedirs("$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("demo_MinFrameTest")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_util", "libjbytecode_commons", "libjbytecode_analysis", "libjbytecode_tree", "libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena")
    add_files("MinFrameTest.cpp")
    add_includedirs("$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("demo_CoreDiag")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena", "libjzip")
    add_files("CoreDiag.cpp")
    add_includedirs("$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("demo_SnakeDiag")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena")
    add_files("SnakeDiag.cpp")
    add_includedirs("$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("demo_SnakeDiag2")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena")
    add_files("SnakeDiag2.cpp")
    add_includedirs("$(projectdir)/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

