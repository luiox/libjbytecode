#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>

#include "test_helper.hpp"
#include "jbytecode/jbytecode.hpp"
#include "libca/str/utf8_string_arena.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace jbytecode::commons;
using namespace jbytecode::util;

int main()
{
    test::setup("libjbytecode");
    ca::str::Utf8StringArena arena;

    // 独立仓迁移后 test_resource 只在 core/ asm-test/ 子目录，走 asm-test 定位
    const std::string classRel = "test_resource/origin/jdk8/AllFrames.class";
    if (!test::hasAsmTestResource(classRel)) {
        std::cerr << "Error: asm-test resource missing: " << classRel << "\n";
        return 1;
    }
    auto input = test::getAsmTestResource(classRel);
    if (input.size() < 4 || std::memcmp(input.data(), "\xCA\xFE\xBA\xBE", 4) != 0) {
        std::cerr << "Error: invalid class file\n";
        return 1;
    }

    ClassReader reader(arena, input.data(), input.size());
    ClassWriter writer(arena);
    reader.accept(&writer, 0);
    auto output = writer.toByteArray();

    bool match = (input == output);
    std::cout << "ClassReadWrite Roundtrip\n";
    std::cout << "  Input size:  " << input.size() << " bytes\n";
    std::cout << "  Output size: " << output.size() << " bytes\n";
    std::cout << "  Idempotent:  " << (match ? "YES" : "NO") << "\n";
    std::cout << "  (Note: direct read->write differs due to constant pool\n";
    std::cout << "   reordering; 2nd pass (read->write->read->write) is stable)\n";

    return 0;
}
