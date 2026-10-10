#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>

#include "test_helper.hpp"
#include "jbytecode/jbytecode.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace jbytecode::commons;
using namespace jbytecode::util;
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

class TimingAdapter : public AdviceAdapter
{
public:
    TimingAdapter(MethodVisitor* mv, int access, ca::str::Utf8StringRef name,
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
        mv->visitLdcInsn(std::string("[enter] method"));
        mv->visitMethodInsn(Opcodes::INVOKEVIRTUAL,
                            ZUtf8StringRef::from_static("java/io/PrintStream"),
                            ZUtf8StringRef::from_static("println"),
                            ZUtf8StringRef::from_static("(Ljava/lang/String;)V"),
                            false);
    }

    void onMethodExit(int opcode) override
    {
        (void)opcode;
        mv->visitFieldInsn(Opcodes::GETSTATIC,
                           ZUtf8StringRef::from_static("java/lang/System"),
                           ZUtf8StringRef::from_static("out"),
                           ZUtf8StringRef::from_static("Ljava/io/PrintStream;"));
        mv->visitLdcInsn(std::string("[exit] method"));
        mv->visitMethodInsn(Opcodes::INVOKEVIRTUAL,
                            ZUtf8StringRef::from_static("java/io/PrintStream"),
                            ZUtf8StringRef::from_static("println"),
                            ZUtf8StringRef::from_static("(Ljava/lang/String;)V"),
                            false);
    }
};

class Instrumentor : public ClassVisitor
{
public:
    explicit Instrumentor(ClassVisitor* cv)
        : ClassVisitor(Opcodes::ASM9, cv)
    {}

    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                               ca::str::Utf8StringRef                     signature,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        auto* methodVisitor = cv->visitMethod(access, name, desc, signature, exceptions);
        if (name.equals(ZUtf8StringRef::from_static("<init>")) ||
            name.equals(ZUtf8StringRef::from_static("<clinit>")))
            return methodVisitor;
        return new TimingAdapter(methodVisitor, access, name, desc);
    }
};

int main()
{
    test::setup("libjbytecode");
    Utf8StringArena arena;

    auto input = test::getResource("AllFrames.class");
    if (input.empty()) {
        std::cerr << "Error: cannot read file\n";
        return 1;
    }

    ClassReader  reader(arena, input.data(), input.size());
    ClassWriter  writer(arena);
    Instrumentor instrumentor(&writer);
    reader.accept(&instrumentor, 0);
    auto output = writer.toByteArray();

    bool match = (input == output);
    std::cout << "MethodInstrument Demo\n";
    std::cout << "  Input size:   " << input.size() << " bytes\n";
    std::cout << "  Output size:  " << output.size() << " bytes\n";
    std::cout << "  Instrumented: " << (match ? "NO (unmodified)" : "YES") << "\n";
    std::cout << "  Output differs from input by "
              << (output.size() > input.size() ? output.size() - input.size()
                                               : input.size() - output.size())
              << " bytes\n";

    return 0;
}
