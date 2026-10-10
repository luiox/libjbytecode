-- crossval match tests: core reader dumper cross-validation against ASM reference
-- 静态闭包按「消费者在前、被依赖在后」显式声明（linux/windows 同口径）。
target("cppdump")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena")
    add_files("cppdump.cpp")
    add_includedirs("$(projectdir)/core/test", "$(projectdir)/core/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("debug_dump")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena")
    add_files("debug_dump.cpp")
    add_includedirs("$(projectdir)/core/test", "$(projectdir)/asm-test/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("cppdump_full")
    set_kind("binary")
    set_default(false)
    add_deps("libjbytecode_core")
    add_packages("libca", "libjstr", "libgarena")
    add_files("cppdump_full.cpp")
    add_includedirs("$(projectdir)/core/test", "$(projectdir)/asm-test/src", "$(projectdir)/test_helper")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
