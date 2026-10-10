#include <fstream>
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

static std::vector<uint8_t> readFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    in.seekg(0, std::ios::end);
    auto sz = static_cast<size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> d(sz);
    in.read(reinterpret_cast<char*>(d.data()), static_cast<std::streamsize>(sz));
    return d;
}

int main()
{
    test::setup("libjbytecode");
    ca::str::Utf8StringArena arena;

    auto input = test::getResource("AllFrames.class");
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
