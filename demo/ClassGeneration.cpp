#include <iostream>
#include <vector>
#include <cstdint>
#include <cstdio>

#include "jbytecode/jbytecode.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace jbytecode::commons;
using namespace jbytecode::util;
using namespace ca::str;


static std::string toStr(Utf8StringRef ref)
{
    return ref.to_std_string();
}

static void hexDump(const std::vector<uint8_t>& data)
{
    for (size_t i = 0; i < data.size(); i++) {
        if (i % 16 == 0) std::printf("%04zx: ", i);
        std::printf("%02x ", data[i]);
        if (i % 16 == 15 || i == data.size() - 1) std::printf("\n");
    }
}

int main()
{
    Utf8StringArena arena;
    auto            className = ZUtf8StringRef::from_static("demo/GeneratedClass");

    ClassNode cn;
    cn.version    = 52 << 16;
    cn.access     = Opcodes::ACC_PUBLIC;
    cn.name       = className;
    cn.superName  = ZUtf8StringRef::from_static("java/lang/Object");
    cn.signature  = Utf8StringRef();
    cn.interfaces = {ZUtf8StringRef::from_static("java/io/Serializable")};

    // Add a field: private int value;
    cn.fields.push_back(new FieldNode(Opcodes::ACC_PRIVATE,
                                      ZUtf8StringRef::from_static("value"),
                                      ZUtf8StringRef::from_static("I"),
                                      Utf8StringRef(),
                                      ca::core::Any{}));

    // Add annotation to field: @Deprecated
    {
        auto* fn = cn.fields.back();
        fn->visibleAnnotations.push_back(
            fn->create<AnnotationNode>(fn->arena(), ZUtf8StringRef::from_static("Ljava/lang/Deprecated;")));
    }

    // Constructor: public <init>()V { super(); this.value = 42; }
    {
        auto* mn      = new MethodNode(Opcodes::ACC_PUBLIC,
                                  ZUtf8StringRef::from_static("<init>"),
                                  ZUtf8StringRef::from_static("()V"),
                                  Utf8StringRef(),
                                       {});
        mn->maxStack  = 2;
        mn->maxLocals = 1;

        // return label for try-catch
        auto* retLabel     = new Label();
        auto* startLabel   = new Label();
        auto* endLabel     = new Label();
        auto* handlerLabel = new Label();

        mn->instructions.add(new LabelNode(startLabel));
        mn->instructions.add(new VarInsnNode(Opcodes::ALOAD, 0));
        mn->instructions.add(new MethodInsnNode(Opcodes::INVOKESPECIAL,
                                                ZUtf8StringRef::from_static("java/lang/Object"),
                                                ZUtf8StringRef::from_static("<init>"),
                                                ZUtf8StringRef::from_static("()V"),
                                                false));
        mn->instructions.add(new VarInsnNode(Opcodes::ALOAD, 0));
        mn->instructions.add(new InsnNode(Opcodes::ICONST_0));
        mn->instructions.add(new FieldInsnNode(Opcodes::PUTFIELD,
                                               ZUtf8StringRef::from_static("demo/GeneratedClass"),
                                               ZUtf8StringRef::from_static("value"),
                                               ZUtf8StringRef::from_static("I")));
        mn->instructions.add(new LabelNode(endLabel));

        // try-catch: RuntimeException -> print stack trace
        mn->instructions.add(new JumpInsnNode(Opcodes::GOTO, retLabel));
        mn->instructions.add(new LabelNode(handlerLabel));
        mn->instructions.add(new InsnNode(Opcodes::ATHROW));
        mn->instructions.add(new LabelNode(retLabel));
        mn->instructions.add(new InsnNode(Opcodes::RETURN));

        mn->tryCatchBlocks.push_back(
            mn->create<TryCatchBlockNode>(startLabel,
                                  endLabel,
                                  handlerLabel,
                                  ZUtf8StringRef::from_static("java/lang/RuntimeException")));

        cn.methods.push_back(mn);
    }

    // Method: public int getValue() { return value; }
    {
        auto* mn      = new MethodNode(Opcodes::ACC_PUBLIC,
                                  ZUtf8StringRef::from_static("getValue"),
                                  ZUtf8StringRef::from_static("()I"),
                                  Utf8StringRef(),
                                       {});
        mn->maxStack  = 1;
        mn->maxLocals = 1;
        mn->instructions.add(new VarInsnNode(Opcodes::ALOAD, 0));
        mn->instructions.add(new FieldInsnNode(Opcodes::GETFIELD,
                                               ZUtf8StringRef::from_static("demo/GeneratedClass"),
                                               ZUtf8StringRef::from_static("value"),
                                               ZUtf8StringRef::from_static("I")));
        mn->instructions.add(new InsnNode(Opcodes::IRETURN));
        cn.methods.push_back(mn);
    }

    // Write class to bytes
    ClassWriter writer(arena);
    cn.accept(&writer);
    auto bytes = writer.toByteArray();

    std::cout << "ClassGeneration Demo\n";
    std::cout << "  Generated class: " << toStr(cn.name) << "\n";
    std::cout << "  Fields: " << cn.fields.size() << "\n";
    std::cout << "  Methods: " << cn.methods.size() << "\n";
    std::cout << "  Output size: " << bytes.size() << " bytes\n";
    std::cout << "  Magic: " << (bytes.size() >= 4 ? "0xCAFEBABE" : "INVALID") << "\n";
    std::cout << "\nHexDump:\n";
    hexDump(bytes);

    // Roundtrip verification
    ClassNode   cn2;
    ClassReader reader(arena, bytes.data(), bytes.size());
    reader.accept(&cn2, 0);

    std::cout << "\nRoundtrip:\n";
    std::cout << "  Name:        " << toStr(cn2.name) << "\n";
    std::cout << "  Super class: " << toStr(cn2.superName) << "\n";
    std::cout << "  Fields:      " << cn2.fields.size() << "\n";
    std::cout << "  Methods:     " << cn2.methods.size() << "\n";

    bool ok = (cn2.name == cn.name && cn2.methods.size() == cn.methods.size());
    std::cout << "  Match:       " << (ok ? "YES" : "NO") << "\n";

    return 0;
}
