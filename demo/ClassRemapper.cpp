#include <iostream>

#include "jbytecode/jbytecode.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace jbytecode::commons;
using namespace jbytecode::util;
using namespace ca::str;

int main()
{
    Utf8StringArena arena;

    std::cout << "ClassRemapper Demo\n\n";

    // SimpleRemapper string API -- rename types in descriptors
    // (字符串线示范:经作业级 arena 注入,intern 落作业 arena;进程级 staticArena
    // 已随 #921 批次2 整体退役,注入是唯一路径,#699/#921)
    SimpleRemapper r(arena,
                     Utf8StringRef::from_cstr("java/lang/Object"),
                     Utf8StringRef::from_cstr("java/lang/Cloneable"));
    std::cout << "[SimpleRemapper string API]\n";
    std::cout << "  mapDesc('Ljava/lang/Object;'):     "
              << r.mapDesc(Utf8StringRef::from_cstr("Ljava/lang/Object;")) << "\n";
    std::cout << "  mapDesc('[Ljava/lang/Object;'):    "
              << r.mapDesc(Utf8StringRef::from_cstr("[Ljava/lang/Object;")) << "\n";
    std::cout << "  mapType('java/lang/Object'):       "
              << r.mapType(Utf8StringRef::from_cstr("java/lang/Object")) << "\n";
    std::cout << "  map('java/lang/Object'):           "
              << r.map(Utf8StringRef::from_cstr("java/lang/Object")) << "\n";
    std::cout << "  mapMethodDesc('(Ljava/lang/Object;)V'): "
              << r.mapMethodDesc(Utf8StringRef::from_cstr("(Ljava/lang/Object;)V")) << "\n\n";

    // Multi-mapping usage
    std::unordered_map<std::string, std::string> mappings;
    mappings["com/foo/A"] = "com/bar/X";
    mappings["com/foo/B"] = "com/bar/Y";
    SimpleRemapper mr(arena, mappings);
    std::cout << "[Multi-mapping]\n";
    std::cout << "  map('com/foo/A') -> " << mr.map(Utf8StringRef::from_cstr("com/foo/A")) << "\n";
    std::cout << "  map('com/foo/B') -> " << mr.map(Utf8StringRef::from_cstr("com/foo/B")) << "\n";
    std::cout << "  map('com/foo/C') -> " << mr.map(Utf8StringRef::from_cstr("com/foo/C"))
              << "\n\n";

    // Demonstrate ClassRemapper on a generated class (no outer class attr)
    ClassNode cn;
    cn.version    = 52 << 16;
    cn.access     = Opcodes::ACC_PUBLIC;
    cn.name       = ZUtf8StringRef::from_static("demo/Hello");
    cn.superName  = ZUtf8StringRef::from_static("java/lang/Object");
    auto* mn      = new MethodNode(Opcodes::ACC_PUBLIC,
                              ZUtf8StringRef::from_static("<init>"),
                              ZUtf8StringRef::from_static("()V"),
                              Utf8StringRef(),
                                   {});
    mn->maxStack  = 1;
    mn->maxLocals = 1;
    cn.methods.push_back(mn);

    ClassWriter    cw(arena);
    SimpleRemapper classRemap(arena,
                              Utf8StringRef::from_cstr("java/lang/Object"),
                              Utf8StringRef::from_cstr("java/lang/Cloneable"));
    ClassRemapper  cr(&cw, &classRemap);
    cn.accept(&cr);
    auto bytes = cw.toByteArray();

    ClassNode   cn2;
    ClassReader rd(arena, bytes.data(), bytes.size());
    rd.accept(&cn2, 0);

    std::cout << "[ClassRemapper via generated class]\n";
    std::cout << "  Original super: java/lang/Object\n";
    std::cout << "  Remapped super: " << reinterpret_cast<const char*>(cn2.superName.data())
              << "\n";
    std::cout << "  Output size: " << bytes.size() << " bytes\n";

    return 0;
}
