#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>

#include "jbytecode/jbytecode.hpp"
#include "libca/str/utf8_string_arena.hpp"
#include "jzip/zip_io.hpp"
#include "jzip/zip_model.hpp"
#include "libca/zip/checksum.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace jbytecode::commons;
using namespace jbytecode::util;
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
    std::string jdkPath;

    // Parse args: support both positional and flag-style
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-i" && i + 1 < argc)
            inputPath = argv[++i];
        else if (arg == "-o" && i + 1 < argc)
            outputPath = argv[++i];
        else if (arg.compare(0, 5, "--jdk") == 0) {
            jdkPath = arg.substr(arg.find('=') + 1);
        }
        else if (inputPath.empty())
            inputPath = arg;
        else if (outputPath.empty())
            outputPath = arg;
    }

    if (inputPath.empty() || outputPath.empty()) {
        std::cerr << "Usage: FrameRecomputeDemo [--jdk=<path>] -i <input.jar> -o <output.jar>\n";
        std::cerr << "   or: FrameRecomputeDemo <input.jar> <output.jar>\n";
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

    for (auto& part : archive.getParts()) {
        if (part->type() != PartType::LOCAL_FILE_HEADER) continue;
        auto*       local     = static_cast<LocalFileHeader*>(part.get());
        const auto& entryName = local->getFileName();
        local->setGeneralPurposeBitFlag(
            static_cast<ca::u16>(local->getGeneralPurposeBitFlag() & ~0x0008u));

        if (entryName.size() < 6 || entryName.substr(entryName.size() - 6) != ".class") {
            skipped++;
            continue;
        }

        auto classBytes = ZipIO::decompressLocalFile(*local);
        if (classBytes.empty()) {
            skipped++;
            continue;
        }

        try {
            ca::str::Utf8StringArena arena;
            ClassNode                cn;
            ClassReader              reader(arena, classBytes.data(), classBytes.size());
            reader.accept(&cn, ClassReader::EXPAND_FRAMES | ClassReader::SKIP_DEBUG);

            // Delete all FrameNode entries
            for (auto* mn : cn.methods) {
                auto& insns = mn->instructions;
                for (auto* n = insns.getFirst(); n;) {
                    auto* next = n->nextInsn;
                    if (n->getType() == AbstractInsnNode::FRAME) {
                        insns.remove(n);
                        delete n;
                    }
                    n = next;
                }
            }

            // Rewrite with COMPUTE_FRAMES
            ClassWriter writer(arena, ClassWriter::COMPUTE_FRAMES);
            cn.accept(&writer);
            auto outBytes = writer.toByteArray();

            local->setFileData(outBytes);
            local->setCompressionMethod(0);
            local->setCompressedSize(static_cast<ca::u32>(outBytes.size()));
            local->setUncompressedSize(static_cast<ca::u32>(outBytes.size()));
            {
                ca::zip::Crc32 crc;
                crc.update(outBytes.data(), outBytes.size());
                local->setCrc32(crc.value());
            }
            processed++;
        }
        catch (const std::exception& e) {
            std::cerr << "  FAIL: " << entryName << " - " << e.what() << "\n";
            failed++;
        }
    }

    std::cout << "Processed: " << processed << " classes, " << skipped << " skipped, " << failed
              << " failed\n";

    auto outputBytes = ZipIO::writeDirect(archive);
    writeFile(outputPath, outputBytes);
    std::cout << "Output: " << outputPath << " (" << outputBytes.size() << " bytes)\n";

    if (processed == 0) {
        std::cerr << "No classes processed. Output JAR may be invalid.\n";
        return 1;
    }
    std::cout << "Try: java -jar " << outputPath << "\n";
    return failed > 0 ? 1 : 0;
}
