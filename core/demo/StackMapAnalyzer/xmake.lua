target("demo_15_StackMapAnalyzer")
    set_kind("binary")
    set_group("libs/demo")
    set_default(false)
    add_deps("libjbytecode_core")
    add_files("*.cpp")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
