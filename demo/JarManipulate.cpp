#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>

#include "test_helper.hpp"
#include "jbytecode/jbytecode.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"
#include "jzip/zip_io.hpp"
#include "jzip/zip_model.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace jbytecode::commons;
using namespace jbytecode::util;
using namespace jzip;
using namespace ca::str;

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

static std::string accessFlagsToString(int access)
{
    std::string s;
    if (access & Opcodes::ACC_PUBLIC) s += "public ";
    if (access & Opcodes::ACC_PRIVATE) s += "private ";
    if (access & Opcodes::ACC_PROTECTED) s += "protected ";
    if (access & Opcodes::ACC_STATIC) s += "static ";
    if (access & Opcodes::ACC_FINAL) s += "final ";
    if (access & Opcodes::ACC_ABSTRACT) s += "abstract ";
    if (access & Opcodes::ACC_SYNTHETIC) s += "synthetic ";
    return s;
}

// --- AdviceAdapter that injects println on method entry ---
class PrintlnInjector : public AdviceAdapter
{
public:
    PrintlnInjector(MethodVisitor* mv, int access, ca::str::Utf8StringRef name,
                    ca::str::Utf8StringRef desc)
        : AdviceAdapter(Opcodes::ASM9, mv, access, name, desc)
    {}

protected:
    void onMethodEnter() override
    {
        mv->visitFieldInsn(Opcodes::GETSTATIC,
                           ZUtf8StringRef::from_static("java/lang/System"),
                           ZUtf8StringRef::from_static("out"),
                           ZUtf8StringRef::from_static("Ljava/io/PrintStream;"));
        mv->visitLdcInsn(std::string("[libjbytecode] Greeter.add() called"));
        mv->visitMethodInsn(Opcodes::INVOKEVIRTUAL,
                            ZUtf8StringRef::from_static("java/io/PrintStream"),
                            ZUtf8StringRef::from_static("println"),
                            ZUtf8StringRef::from_static("(Ljava/lang/String;)V"),
                            false);
    }
};

// --- ClassVisitor that wraps specific methods ---
class InjectorVisitor : public ClassVisitor
{
public:
    explicit InjectorVisitor(ClassVisitor* cv)
        : ClassVisitor(Opcodes::ASM9, cv)
    {}

    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                               ca::str::Utf8StringRef                     signature,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        auto* mv = cv->visitMethod(access, name, desc, signature, exceptions);
        if (name.equals(ZUtf8StringRef::from_static("add")) &&
            desc.equals(ZUtf8StringRef::from_static("(II)I"))) {
            return new PrintlnInjector(mv, access, name, desc);
        }
        return mv;
    }
};

// --- Full pass-through (no modification) for Calculator ---
// Note: We avoid rewriting Calculator because libjbytecode's MethodWriter
// has a known limitation with backward jump label resolution (the
// visitJumpInsn always adds forward refs but visitLabel only resolves
// them once, so backward jumps are corrupted on rewrite).

// --- Dump class info to console via ClassVisitor ---
class DumpVisitor : public ClassVisitor
{
public:
    DumpVisitor()
        : ClassVisitor(Opcodes::ASM9)
    {}

    void visit(int version, int access, ca::str::Utf8StringRef name,
               ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override
    {
        std::string sName(reinterpret_cast<const char*>(name.data()), name.byte_length());
        std::string sSuper(reinterpret_cast<const char*>(superName.data()),
                           superName.byte_length());
        std::cout << "    Class: " << sName << "\n";
        std::cout << "    Super: " << sSuper << "\n";
        if (!interfaces.empty()) {
            std::cout << "    Interfaces:";
            for (auto& iface : interfaces) {
                std::cout << " "
                          << std::string(reinterpret_cast<const char*>(iface.data()),
                                         iface.byte_length());
            }
            std::cout << "\n";
        }
        std::cout << "    Version: " << (version >> 16) << "." << (version & 0xFFFF) << "\n";
    }

    FieldVisitor* visitField(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                             ca::str::Utf8StringRef signature, const ca::core::Any& value) override
    {
        std::string sName(reinterpret_cast<const char*>(name.data()), name.byte_length());
        std::string sDesc(reinterpret_cast<const char*>(desc.data()), desc.byte_length());
        std::cout << "    Field: " << accessFlagsToString(access) << sName << " " << sDesc << "\n";
        return nullptr;
    }

    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                               ca::str::Utf8StringRef                     signature,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        std::string sName(reinterpret_cast<const char*>(name.data()), name.byte_length());
        std::string sDesc(reinterpret_cast<const char*>(desc.data()), desc.byte_length());
        std::cout << "    Method: " << accessFlagsToString(access) << sName << sDesc << "\n";
        return nullptr;
    }
};

// --- Chain: ClassReader -> DumpVisitor (1st pass to print) ---
// Then: ClassReader -> InjectorVisitor -> ClassWriter (2nd pass to modify & write)

struct ClassModResult
{
    std::vector<uint8_t> bytes;
    bool                 modified;
};

static ClassModResult processClassBytes(const std::vector<uint8_t>& classBytes,
                                        const std::string&          className)
{
    Utf8StringArena arena;
    ClassModResult  result = {classBytes, false};

    // Pass 1: Dump class info
    {
        ClassReader reader(arena, classBytes.data(), classBytes.size());
        DumpVisitor dumper;
        reader.accept(&dumper, ClassReader::SKIP_DEBUG | ClassReader::SKIP_FRAMES);
    }

    // Pass 2: Modify Greeter.add (no backward jumps in this class)
    if (className == "com/demo/Greeter") {
        std::cout << "    >>> Modifying Greeter.add...\n";

        DefaultCommonSuperClassProvider typeResolver;
        ClassReader                     reader(arena, classBytes.data(), classBytes.size());
        ClassWriter                     writer(arena, ClassWriter::COMPUTE_FRAMES);
        writer.setTypeResolver(&typeResolver);
        InjectorVisitor injector(&writer);
        reader.accept(&injector, ClassReader::SKIP_DEBUG);
        result.bytes    = writer.toByteArray();
        result.modified = true;
    }

    return result;
}

int main()
{
    test::setup("libjbytecode");
    auto jarPath = test::getResourcePath("javatestdemo.jar").string();

    std::cout << "=== libjbytecode + libjzip JAR Manipulation Demo ===\n\n";

    // Step 1: Read JAR
    std::cout << "[Step 1] Reading JAR: " << jarPath << "\n";
    auto jarBytes = readFile(jarPath);
    if (jarBytes.empty()) {
        std::cerr << "Error: cannot read JAR file\n";
        return 1;
    }

    MemorySegment jarSeg(jarBytes);
    auto          archive = ZipIO::readJvm(jarSeg);

    std::cout << "  Entries: " << archive.getLocalFiles().size() << "\n\n";

    // Step 2: Process each class entry
    std::cout << "[Step 2] Parsing & modifying class entries\n\n";

    bool modified = false;

    for (auto& part : archive.getParts()) {
        if (part->type() != PartType::LOCAL_FILE_HEADER) continue;
        auto*       local     = static_cast<LocalFileHeader*>(part.get());
        const auto& entryName = local->getFileName();

        if (entryName.size() < 6 || entryName.substr(entryName.size() - 6) != ".class") {
            std::cout << "  [SKIP] " << entryName << " (not .class)\n";
            continue;
        }

        std::cout << "  [ENTRY] " << entryName << "\n";

        auto classBytes = ZipIO::decompressLocalFile(*local);
        if (classBytes.empty()) {
            std::cout << "    (empty)\n\n";
            continue;
        }

        // Extract class name from path: "com/demo/Calculator.class"
        std::string className = entryName.substr(0, entryName.size() - 6);
        // Convert path separators (already "/" in JAR) - keep as internal name

        auto result = processClassBytes(classBytes, className);

        if (result.modified) {
            std::cout << "        size: " << classBytes.size() << " -> " << result.bytes.size()
                      << " bytes\n";
            local->setFileData(result.bytes);
            local->setCompressionMethod(0);
            local->setCompressedSize(static_cast<ca::u32>(result.bytes.size()));
            local->setUncompressedSize(static_cast<ca::u32>(result.bytes.size()));
            local->setCrc32(0);
            modified = true;
        }

        std::cout << "\n";
    }

    if (!modified) {
        std::cout << "  (No modifications were made)\n";
    }

    // Step 3: Write modified JAR
    std::cout << "[Step 3] Writing modified JAR\n";

    auto        outputBytes = ZipIO::writeDirect(archive);
    std::string outputPath  = test::getDemoOutPath("modified_javatestdemo.jar").string();
    writeFile(outputPath, outputBytes);
    std::cout << "  Output: " << outputPath << " (" << outputBytes.size() << " bytes)\n\n";

    // Step 4: Verification
    std::cout << "[Step 4] Verification\n";

    MemorySegment outSeg(outputBytes);
    auto          outArchive = ZipIO::readJvm(outSeg);
    std::cout << "  Entries in output: " << outArchive.getLocalFiles().size() << "\n";

    for (const auto* local : outArchive.getLocalFiles()) {
        const auto& name = local->getFileName();
        if (name.find("Greeter.class") != std::string::npos) {
            auto data = ZipIO::decompressLocalFile(*local);
            if (data.empty()) {
                std::cout << "  ERROR: Greeter.class data is empty!\n";
                break;
            }

            std::cout << "  Greeter.class re-read: " << data.size() << " bytes\n";

            Utf8StringArena verifyArena;
            ClassNode       cn2;
            ClassReader     reader2(verifyArena, data.data(), data.size());
            reader2.accept(&cn2, ClassReader::SKIP_DEBUG | ClassReader::SKIP_FRAMES);
            std::cout << "  Class: " << reinterpret_cast<const char*>(cn2.name.data())
                      << " (parse OK)\n";
            std::cout << "  Methods:\n";
            for (auto* mn : cn2.methods) {
                std::cout << "    - " << reinterpret_cast<const char*>(mn->name.data())
                          << reinterpret_cast<const char*>(mn->desc.data()) << " ("
                          << mn->instructions.size() << " insns)\n";
            }
            std::cout << "  >>> Verification PASSED\n";
            break;
        }
    }

    std::cout << "\n=== Demo complete ===\n";
    std::cout << "Run: java -noverify -jar modified_javatestdemo.jar\n";

    return 0;
}
