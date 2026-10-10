// 逐步诊断: 定位 Snake.class crash 的确切位置
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>

#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "libca/str/utf8_string_arena.hpp"

using namespace jbytecode::core;

static std::vector<uint8_t> readFile(const char* path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    auto sz = in.tellg();
    in.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    in.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

int main()
{
    ca::str::Utf8StringArena arena;

    auto buf = readFile("Snake.class");
    std::cout << "Step 1: Read " << buf.size() << " bytes\n";

    std::cout << "Step 2: Construct ClassReader...\n";
    ClassReader reader(arena, buf.data(), 0, static_cast<ca::i32>(buf.size()));
    std::cout << "Step 3: ClassReader constructed OK\n";

    std::cout << "Step 4: Get class name...\n";
    auto cn = reader.getClassName();
    std::cout << "  className = " << cn.data() << "\n";

    std::cout << "Step 5: Get super name...\n";
    auto sn = reader.getSuperName();
    std::cout << "  superName = " << sn.data() << "\n";

    std::cout << "Step 6: Get interfaces...\n";
    auto ifs = reader.getInterfaces();
    std::cout << "  interfaces count = " << ifs.size() << "\n";

    std::cout << "Step 7: Construct ClassWriter...\n";
    ClassWriter cw(arena, 0);
    std::cout << "Step 8: Accept (read → write)...\n";
    reader.accept(&cw, ClassReader::SKIP_DEBUG);
    std::cout << "Step 9: toByteArray...\n";
    auto out = cw.toByteArray();
    std::cout << "Final: " << out.size() << " bytes\n";
    return 0;
}
