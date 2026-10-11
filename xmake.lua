-- libjbytecode：OW2 ASM 的 C++ 移植（JVM 字节码读写/树 API/分析框架，独立构建根）。
--
-- 依赖：
--   - libca（luiox-repo git 直连包）：core/str/log/crypto 为运行依赖，test 为
--     测试门面依赖；版本锁固定 0.0.14，升级须显式换版。
--   - libjstr / libgarena（luiox-repo git 直连包）：core 子库的编码与 arena 底座。
--   - libjzip（luiox-repo git 直连包）：仅 --with_demo=y（Jar 系 demo）。
--   - gtest（xrepo，仅 --with_tests=y）。
--
-- 仓库注册：本文件不声明 add_repositories——包沙箱会继承宿主的仓库注册，
-- 在工程内重复注册同名仓库会导致嵌套包构建时 remote 路径错乱。独立开发/CI
-- 先执行：
--   xmake repo --add luiox-repo https://github.com/luiox/luiox-repo.git

set_languages("cxx17")
add_rules("mode.debug", "mode.release")

add_requires("libca 0.0.14", {configs = {modules = "core,str,log,crypto,test"}})
add_requires("libjstr 0.0.2")
add_requires("libgarena 0.0.2")

option("with_tests")
    set_default(false)
    set_showmenu(true)
    set_description("Enable unit tests (pulls gtest)")
option_end()

option("with_demo")
    set_default(false)
    set_showmenu(true)
    set_description("Enable demo targets (pulls libjzip)")
option_end()

if has_config("with_tests") then
    add_requires("gtest")
end

if has_config("with_demo") then
    add_requires("libjzip 0.0.2")
end

-- libjbytecode 聚合库：各子模块拆为独立子库，此 target 聚合全部子库及统一入口头文件。
-- 各子库头与包依赖经 public 透传给消费方（demo / 外部工程）。
target("libjbytecode")
    set_kind("static")
    add_deps("libjbytecode_core", "libjbytecode_tree", "libjbytecode_analysis",
             "libjbytecode_commons", "libjbytecode_util")
    add_packages("libca", "libjstr", "libgarena", {public = true})
    -- xmake 3.x 不把静态 target 依赖传进 binary 链接：windows 下显式给全闭包，
    -- 按「消费者在前、被依赖在后」排序（与 Linux 同口径）。
    if is_plat("windows") then
        add_deps("libjbytecode_util", "libjbytecode_commons", "libjbytecode_analysis",
                 "libjbytecode_tree", "libjbytecode_core", {public = true})
        add_links("libjbytecode_util", "libjbytecode_commons", "libjbytecode_analysis",
                  "libjbytecode_tree", "libjbytecode_core", {public = true})
    end
    add_includedirs("src", {public = true})
    -- 聚合库自带一个查版本号的接口，使子库形态落成真 lib 而非纯聚合。
    add_files("src/jbytecode/*.cpp")
    add_headerfiles("src/(jbytecode/*.hpp)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

includes("core")
includes("tree")
includes("analysis")
includes("commons")
includes("util")

if has_config("with_demo") then
    includes("demo")
    -- core/demo：core 子库示例（此前无 include 链注册，target 永不生效）
    includes("core/demo")
end

-- asm-test：ASM 对齐/集成扩展测试（含 test_resource 预编译样本）。
includes("asm-test")
