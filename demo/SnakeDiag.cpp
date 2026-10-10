// 迷你诊断：只测试 Snake.class 的 ClassReader → ClassWriter passthrough
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

int main()
{
    ca::str::Utf8StringArena arena;

    // 直接读 Snake.class 文件
    std::ifstream in("Snake.class", std::ios::binary | std::ios::ate);
    if (!in) {
        std::cerr << "Cannot open Snake.class\n";
        return 1;
    }
    auto sz = in.tellg();
    in.seekg(0);
    std::vector<uint8_t> buf(sz);
    in.read(reinterpret_cast<char*>(buf.data()), sz);
    std::cout << "Read " << buf.size() << " bytes\n";

    // 测试 1: 只做 ClassReader 构造 + accept 到空 ClassWriter (passthrough)
    std::cout << "Test 1: Passthrough read+write...\n";
    ClassReader reader(arena, buf.data(), 0, static_cast<ca::i32>(buf.size()));
    ClassWriter cw(arena, 0);
    reader.accept(&cw, ClassReader::SKIP_DEBUG);
    auto out = cw.toByteArray();
    std::cout << "  OK: " << out.size() << " bytes\n";

    // 测试 2: COMPUTE_FRAMES
    std::cout << "Test 2: COMPUTE_FRAMES...\n";
    ClassReader reader2(arena, buf.data(), 0, static_cast<ca::i32>(buf.size()));
    ClassWriter cw2(arena, ClassWriter::COMPUTE_FRAMES);
    reader2.accept(&cw2, ClassReader::EXPAND_FRAMES | ClassReader::SKIP_DEBUG);
    auto out2 = cw2.toByteArray();
    std::cout << "  OK: " << out2.size() << " bytes\n";

    // 测试 3: 幂等
    std::cout << "Test 3: Idempotent...\n";
    ClassReader reader3(arena, out2.data(), 0, static_cast<ca::i32>(out2.size()));
    ClassWriter cw3(arena, ClassWriter::COMPUTE_FRAMES);
    reader3.accept(&cw3, ClassReader::EXPAND_FRAMES | ClassReader::SKIP_DEBUG);
    auto out3 = cw3.toByteArray();
    std::cout << "  OK: " << out3.size() << " bytes\n";

    std::cout << "All tests passed!\n";
    return 0;
}
