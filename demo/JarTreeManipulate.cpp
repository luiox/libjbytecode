#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
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


static std::string toStr(Utf8StringRef ref)
{
    return ref.to_std_string();
}

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

static std::string internalName(const std::string& jarEntryName)
{
    return jarEntryName.substr(0, jarEntryName.size() - 6);
}

struct ClassCounter
{
    int classId = 0;
};

static bool isInstrumentable(const Utf8StringRef& name)
{
    return name != ZUtf8StringRef::from_static("<init>") &&
           name != ZUtf8StringRef::from_static("<clinit>") &&
           name != ZUtf8StringRef::from_static("testPrint");
}

// Insert nodes before `anchor` that call:
//   testPrint(className.identifierM * 100 + methodIdx, "prefix name desc")
static void emitCall(InsnList& insns, AbstractInsnNode* anchor, const Utf8StringRef& cn,
                     int methodIdx, const std::string& msg)
{
    insns.insertBefore(anchor,
                       new FieldInsnNode(Opcodes::GETSTATIC,
                                         cn,
                                         ZUtf8StringRef::from_static("identifierM"),
                                         ZUtf8StringRef::from_static("I")));
    insns.insertBefore(anchor, new IntInsnNode(Opcodes::BIPUSH, 100));
    insns.insertBefore(anchor, new InsnNode(Opcodes::IMUL));
    if (methodIdx <= 127) {
        insns.insertBefore(anchor, new IntInsnNode(Opcodes::BIPUSH, methodIdx));
    }
    else {
        insns.insertBefore(anchor, new IntInsnNode(Opcodes::SIPUSH, methodIdx));
    }
    insns.insertBefore(anchor, new InsnNode(Opcodes::IADD));
    insns.insertBefore(anchor, new LdcInsnNode(std::string(msg)));
    insns.insertBefore(anchor,
                       new MethodInsnNode(Opcodes::INVOKESTATIC,
                                          cn,
                                          ZUtf8StringRef::from_static("testPrint"),
                                          ZUtf8StringRef::from_static("(ILjava/lang/String;)V"),
                                          false));
}

static void buildTestPrintBody(MethodNode* mn)
{
    auto& insns = mn->instructions;
    insns.add(new FieldInsnNode(Opcodes::GETSTATIC,
                                ZUtf8StringRef::from_static("java/lang/System"),
                                ZUtf8StringRef::from_static("out"),
                                ZUtf8StringRef::from_static("Ljava/io/PrintStream;")));
    insns.add(
        new TypeInsnNode(Opcodes::NEW, ZUtf8StringRef::from_static("java/lang/StringBuilder")));
    insns.add(new InsnNode(Opcodes::DUP));
    insns.add(new MethodInsnNode(Opcodes::INVOKESPECIAL,
                                 ZUtf8StringRef::from_static("java/lang/StringBuilder"),
                                 ZUtf8StringRef::from_static("<init>"),
                                 ZUtf8StringRef::from_static("()V"),
                                 false));
    insns.add(new LdcInsnNode(std::string("[Demo-")));
    insns.add(new MethodInsnNode(
        Opcodes::INVOKEVIRTUAL,
        ZUtf8StringRef::from_static("java/lang/StringBuilder"),
        ZUtf8StringRef::from_static("append"),
        ZUtf8StringRef::from_static("(Ljava/lang/String;)Ljava/lang/StringBuilder;"),
        false));
    insns.add(new VarInsnNode(Opcodes::ILOAD, 0));
    insns.add(new MethodInsnNode(Opcodes::INVOKEVIRTUAL,
                                 ZUtf8StringRef::from_static("java/lang/StringBuilder"),
                                 ZUtf8StringRef::from_static("append"),
                                 ZUtf8StringRef::from_static("(I)Ljava/lang/StringBuilder;"),
                                 false));
    insns.add(new LdcInsnNode(std::string("] ")));
    insns.add(new MethodInsnNode(
        Opcodes::INVOKEVIRTUAL,
        ZUtf8StringRef::from_static("java/lang/StringBuilder"),
        ZUtf8StringRef::from_static("append"),
        ZUtf8StringRef::from_static("(Ljava/lang/String;)Ljava/lang/StringBuilder;"),
        false));
    insns.add(new VarInsnNode(Opcodes::ALOAD, 1));
    insns.add(new MethodInsnNode(
        Opcodes::INVOKEVIRTUAL,
        ZUtf8StringRef::from_static("java/lang/StringBuilder"),
        ZUtf8StringRef::from_static("append"),
        ZUtf8StringRef::from_static("(Ljava/lang/String;)Ljava/lang/StringBuilder;"),
        false));
    insns.add(new MethodInsnNode(Opcodes::INVOKEVIRTUAL,
                                 ZUtf8StringRef::from_static("java/lang/StringBuilder"),
                                 ZUtf8StringRef::from_static("toString"),
                                 ZUtf8StringRef::from_static("()Ljava/lang/String;"),
                                 false));
    insns.add(new MethodInsnNode(Opcodes::INVOKEVIRTUAL,
                                 ZUtf8StringRef::from_static("java/io/PrintStream"),
                                 ZUtf8StringRef::from_static("println"),
                                 ZUtf8StringRef::from_static("(Ljava/lang/String;)V"),
                                 false));
    insns.add(new InsnNode(Opcodes::RETURN));
}

static std::vector<uint8_t> transformClass(const std::vector<uint8_t>& inBytes,
                                           ClassCounter& counter, const std::string& jarEntryName)
{
    Utf8StringArena arena;
    std::string     cn = internalName(jarEntryName);
    auto            cnRef =
        ca::str::Utf8StringRef(reinterpret_cast<const ca::u8*>(cn.data()), cn.size(), cn.size());
    int classId = counter.classId++;

    // Read into ClassNode (Tree API)
    ClassNode cnNode;
    {
        ClassReader reader(arena, inBytes.data(), inBytes.size());
        reader.accept(&cnNode, ClassReader::EXPAND_FRAMES | ClassReader::SKIP_DEBUG);
    }

    // Add field: private static int identifierM
    cnNode.fields.push_back(new FieldNode(Opcodes::ACC_PRIVATE | Opcodes::ACC_STATIC,
                                          ZUtf8StringRef::from_static("identifierM"),
                                          ZUtf8StringRef::from_static("I"),
                                          Utf8StringRef(),
                                          ca::core::Any{}));

    // Add method: private static void testPrint(int, String)
    auto* tp      = new MethodNode(Opcodes::ACC_PRIVATE | Opcodes::ACC_STATIC,
                              ZUtf8StringRef::from_static("testPrint"),
                              ZUtf8StringRef::from_static("(ILjava/lang/String;)V"),
                              Utf8StringRef(),
                                   {});
    tp->maxStack  = 4;
    tp->maxLocals = 2;
    buildTestPrintBody(tp);
    cnNode.methods.push_back(tp);

    // <clinit>: identifierM = classId
    bool hasClinit = false;
    for (auto* mn : cnNode.methods) {
        if (mn->name == ZUtf8StringRef::from_static("<clinit>")) {
            auto* last = mn->instructions.getLast();
            if (last && last->getType() == AbstractInsnNode::INSN &&
                static_cast<InsnNode*>(last)->opcode == Opcodes::RETURN) {
                mn->instructions.insertBefore(
                    last,
                    new IntInsnNode(classId <= 127 ? Opcodes::BIPUSH : Opcodes::SIPUSH, classId));
                mn->instructions.insertBefore(
                    last,
                    new FieldInsnNode(Opcodes::PUTSTATIC,
                                      cnRef,
                                      ZUtf8StringRef::from_static("identifierM"),
                                      ZUtf8StringRef::from_static("I")));
            }
            hasClinit = true;
            break;
        }
    }
    if (!hasClinit) {
        auto* clinit      = new MethodNode(Opcodes::ACC_STATIC,
                                      ZUtf8StringRef::from_static("<clinit>"),
                                      ZUtf8StringRef::from_static("()V"),
                                      Utf8StringRef(),
                                           {});
        clinit->maxStack  = 1;
        clinit->maxLocals = 0;
        auto& ci          = clinit->instructions;
        ci.add(new IntInsnNode(classId <= 127 ? Opcodes::BIPUSH : Opcodes::SIPUSH, classId));
        ci.add(new FieldInsnNode(Opcodes::PUTSTATIC,
                                 cnRef,
                                 ZUtf8StringRef::from_static("identifierM"),
                                 ZUtf8StringRef::from_static("I")));
        ci.add(new InsnNode(Opcodes::RETURN));
        cnNode.methods.push_back(clinit);
    }

    // ClassNode roundtrip corrupts StackMapTable for methods with GOTO
    // (FrameNode objects are lost during accept -> ClassWriter).
    // Skip rewriting for such classes entirely.
    for (auto* mn : cnNode.methods) {
        if (!isInstrumentable(mn->name)) continue;
        for (auto* n = mn->instructions.getFirst(); n; n = n->nextInsn) {
            if (n->getType() == AbstractInsnNode::JUMP_INSN &&
                static_cast<JumpInsnNode*>(n)->opcode == Opcodes::GOTO) {
                std::cout << "         (has GOTO -- skip)\n";
                return inBytes;
            }
        }
    }

    // Inject enter/exit calls
    int methodIdx = 0;
    for (auto* mn : cnNode.methods) {
        if (!isInstrumentable(mn->name)) continue;
        auto* first = mn->instructions.getFirst();
        if (first) {
            emitCall(mn->instructions,
                     first,
                     cnRef,
                     methodIdx,
                     "[enter] " + toStr(mn->name) + toStr(mn->desc));
        }
        std::vector<AbstractInsnNode*> returns;
        for (auto* n = mn->instructions.getFirst(); n; n = n->nextInsn) {
            if (n->getType() == AbstractInsnNode::INSN) {
                int op = static_cast<InsnNode*>(n)->opcode;
                if (op == Opcodes::RETURN || op == Opcodes::IRETURN || op == Opcodes::ARETURN) {
                    returns.push_back(n);
                }
            }
        }
        for (auto* ret : returns) {
            emitCall(mn->instructions,
                     ret,
                     cnRef,
                     methodIdx,
                     "[exit] " + toStr(mn->name) + toStr(mn->desc));
        }
        methodIdx++;
    }

    // Write back
    ClassWriter writer(arena, ClassWriter::COMPUTE_MAXS);
    cnNode.accept(&writer);
    return writer.toByteArray();
}

int main()
{
    test::setup("libjbytecode");
    auto jarPath = test::getResourcePath("javatestdemo.jar").string();

    std::cout << "=== Tree-API Direct Node Manipulation Demo ===\n\n";

    auto          jarBytes = readFile(jarPath);
    MemorySegment jarSeg(jarBytes);
    auto          archive = ZipIO::readJvm(jarSeg);
    std::cout << "Entries: " << archive.getLocalFiles().size() << "\n\n";

    ClassCounter counter{0};

    for (auto& part : archive.getParts()) {
        if (part->type() != PartType::LOCAL_FILE_HEADER) continue;
        auto*       local     = static_cast<LocalFileHeader*>(part.get());
        const auto& entryName = local->getFileName();

        if (entryName.size() < 6 || entryName.substr(entryName.size() - 6) != ".class") {
            std::cout << "  [SKIP] " << entryName << " (not .class)\n";
            continue;
        }

        std::cout << "  [PROC] " << entryName << "\n";

        auto classBytes = ZipIO::decompressLocalFile(*local);
        if (classBytes.empty()) continue;

        auto modified = transformClass(classBytes, counter, entryName);
        std::cout << "         " << classBytes.size() << " -> " << modified.size() << " bytes\n";

        local->setFileData(modified);
        local->setCompressionMethod(0);
        local->setCompressedSize(static_cast<ca::u32>(modified.size()));
        local->setUncompressedSize(static_cast<ca::u32>(modified.size()));
        local->setCrc32(0);
    }

    std::cout << "\nWriting modified JAR...\n";
    auto        outputBytes = ZipIO::writeDirect(archive);
    std::string outputPath  = test::getDemoOutPath("modified_tree_demo.jar").string();
    writeFile(outputPath, outputBytes);
    std::cout << "Output: " << outputPath << " (" << outputBytes.size() << " bytes)\n";
    std::cout << "Run: java -noverify -jar " << outputPath << "\n";
    return 0;
}
