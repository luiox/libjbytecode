#include "../Common.hpp"

struct TryCatchMV : MethodVisitor
{
    bool  hasCode = false;
    Label startLabel, endLabel, handlerLabel;

    TryCatchMV(MethodVisitor* mv)
        : MethodVisitor(Opcodes::ASM9, mv)
    {}

    void visitCode() override
    {
        MethodVisitor::visitCode();
        hasCode = true;

        mv->visitLabel(&startLabel);
        mv->visitTryCatchBlock(&startLabel,
                               &endLabel,
                               &handlerLabel,
                               ZUtf8StringRef::from_static("java/lang/Throwable"));
    }

    void visitInsn(int opcode) override
    {
        if (hasCode && (opcode == Opcodes::IRETURN || opcode == Opcodes::LRETURN ||
                        opcode == Opcodes::FRETURN || opcode == Opcodes::DRETURN ||
                        opcode == Opcodes::ARETURN || opcode == Opcodes::RETURN)) {
            mv->visitLabel(&endLabel);
            mv->visitInsn(Opcodes::RETURN);
            mv->visitLabel(&handlerLabel);

            mv->visitVarInsn(Opcodes::ASTORE, 1);
            mv->visitFieldInsn(Opcodes::GETSTATIC,
                               ZUtf8StringRef::from_static("java/lang/System"),
                               ZUtf8StringRef::from_static("err"),
                               ZUtf8StringRef::from_static("Ljava/io/PrintStream;"));
            mv->visitVarInsn(Opcodes::ALOAD, 1);
            mv->visitMethodInsn(Opcodes::INVOKEVIRTUAL,
                                ZUtf8StringRef::from_static("java/io/PrintStream"),
                                ZUtf8StringRef::from_static("println"),
                                ZUtf8StringRef::from_static("(Ljava/lang/Object;)V"),
                                false);
            mv->visitInsn(Opcodes::RETURN);
        }
        MethodVisitor::visitInsn(opcode);
    }
};

struct TryCatchCV : ClassVisitor
{
    TryCatchCV(ClassVisitor* cv)
        : ClassVisitor(Opcodes::ASM9, cv)
    {}

    MethodVisitor* visitMethod(int access, Utf8StringRef name, Utf8StringRef desc,
                               Utf8StringRef sig, const std::vector<Utf8StringRef>& excs) override
    {
        MethodVisitor* mv = cv->visitMethod(access, name, desc, sig, excs);
        if (mv == nullptr) return nullptr;
        auto n = toStr(name);
        if (n == "<init>" || n == "<clinit>") return mv;
        return new TryCatchMV(mv);
    }
};

int main(int argc, char* argv[])
{
    std::string inputPath  = (argc > 1) ? argv[1] : "";
    std::string outputPath = (argc > 2) ? argv[2] : "SampleClass-trycatch.class";

    std::vector<uint8_t> bytes;
    if (!inputPath.empty()) {
        bytes = readFile(inputPath);
    }
    else {
        bytes = createSampleClass();
    }
    if (bytes.empty()) {
        std::cerr << "Error: empty input\n";
        return 1;
    }

    ca::str::Utf8StringArena arena;
    ClassReader              reader(arena, bytes);
    ClassWriter              writer(arena, ClassWriter::COMPUTE_MAXS);
    TryCatchCV               adapter(&writer);
    reader.accept(&adapter, 0);
    auto output = writer.toByteArray();

    writeFile(outputPath, output);
    std::cout << "InsertTryCatch Demo\n";
    std::cout << "  Original: " << bytes.size() << " bytes\n";
    std::cout << "  Modified: " << output.size() << " bytes\n";
    std::cout << "  Added:    " << (output.size() - bytes.size()) << " bytes overhead\n";
    return 0;
}
