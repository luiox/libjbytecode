// Core API 诊断工具 — 直接用 ClassReader + ClassWriter (visitor 级别)
// 不依赖树 API (ClassNode)，纯粹测试 core 层的读写+帧计算
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <exception>
#include <cstring>

#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/visitors.hpp"
#include "libca/str/utf8_string_arena.hpp"
#include "jzip/zip_io.hpp"
#include "jzip/zip_model.hpp"

using namespace jbytecode::core;
using namespace jzip;

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

static void writeFile(const std::string& path, const std::vector<uint8_t>& data)
{
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

int main(int argc, char** argv)
{
    std::string inputPath, outputPath;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-i" && i + 1 < argc)
            inputPath = argv[++i];
        else if (arg == "-o" && i + 1 < argc)
            outputPath = argv[++i];
        else if (inputPath.empty())
            inputPath = arg;
        else if (outputPath.empty())
            outputPath = arg;
    }

    if (inputPath.empty() || outputPath.empty()) {
        std::cerr << "Usage: CoreDiag -i <input.jar> -o <output.jar>\n";
        return 1;
    }

    auto jarBytes = readFile(inputPath);
    if (jarBytes.empty()) {
        std::cerr << "Error: cannot read " << inputPath << "\n";
        return 1;
    }

    MemorySegment jarSeg(jarBytes);
    auto          archive   = ZipIO::readJvm(jarSeg);
    int           processed = 0, skipped = 0, failed = 0;
    int           readFail = 0, writeFail = 0, frameFail = 0;

    for (auto& part : archive.getParts()) {
        if (part->type() != PartType::LOCAL_FILE_HEADER) continue;
        auto*       local     = static_cast<LocalFileHeader*>(part.get());
        const auto& entryName = local->getFileName();
        if (entryName.size() < 6 || entryName.substr(entryName.size() - 6) != ".class") {
            skipped++;
            continue;
        }

        auto classBytes = ZipIO::decompressLocalFile(*local);
        if (classBytes.empty()) {
            skipped++;
            continue;
        }

        std::cout << "--- " << entryName << " (" << classBytes.size() << " bytes) ---\n";

        // === Test 1: 纯读 (passthrough, 不修改) ===
        {
            try {
                ca::str::Utf8StringArena arena;
                ClassReader              reader(
                    arena, classBytes.data(), 0, static_cast<ca::i32>(classBytes.size()));
                ClassWriter cw(arena, 0);   // 无标志 = 不重算帧
                reader.accept(&cw, 0);
                auto outBytes = cw.toByteArray();
                std::cout << "  [PASS] Passthrough: " << outBytes.size() << " bytes\n";
            }
            catch (const std::exception& e) {
                std::cout << "  [FAIL] Passthrough read/write: " << e.what() << "\n";
                readFail++;
                failed++;
                continue;   // 如果纯读都失败，后面的测试没意义
            }
        }

        // === Test 2: COMPUTE_FRAMES ===
        {
            try {
                ca::str::Utf8StringArena arena;
                ClassReader              reader(
                    arena, classBytes.data(), 0, static_cast<ca::i32>(classBytes.size()));
                ClassWriter cw(arena, ClassWriter::COMPUTE_FRAMES);
                reader.accept(&cw, ClassReader::EXPAND_FRAMES | ClassReader::SKIP_DEBUG);
                auto outBytes = cw.toByteArray();
                std::cout << "  [PASS] COMPUTE_FRAMES: " << outBytes.size() << " bytes\n";

                // 验证: 读回输出, 确保有效
                try {
                    ClassReader reader2(
                        arena, outBytes.data(), 0, static_cast<ca::i32>(outBytes.size()));
                    ClassWriter cw2(arena, ClassWriter::COMPUTE_FRAMES);
                    reader2.accept(&cw2, ClassReader::EXPAND_FRAMES | ClassReader::SKIP_DEBUG);
                    auto pass2 = cw2.toByteArray();
                    std::cout << "  [PASS] Idempotent: " << pass2.size() << " bytes\n";
                }
                catch (const std::exception& e) {
                    std::cout << "  [FAIL] Idempotent check: " << e.what() << "\n";
                    frameFail++;
                }
            }
            catch (const std::exception& e) {
                std::cout << "  [FAIL] COMPUTE_FRAMES: " << e.what() << "\n";
                frameFail++;
                failed++;
            }
        }

        processed++;
    }

    std::cout << "\n===== SUMMARY =====\n";
    std::cout << "Processed: " << processed << " classes\n";
    std::cout << "Skipped: " << skipped << "\n";
    std::cout << "Failed: " << failed << " (read=" << readFail << ", frame=" << frameFail << ")\n";
    return failed > 0 ? 1 : 0;
}
