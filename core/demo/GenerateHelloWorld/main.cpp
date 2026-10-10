#include "../Common.hpp"

int main()
{
    ca::str::Utf8StringArena arena;
    ClassWriter              cw(arena, ClassWriter::COMPUTE_FRAMES);

    cw.visit(Opcodes::V17,
             Opcodes::ACC_PUBLIC,
             ZUtf8StringRef::from_static("HelloWorld"),
             Utf8StringRef{},
             ZUtf8StringRef::from_static("java/lang/Object"),
             {});

    // Constructor
    {
        MethodVisitor* mv = cw.visitMethod(Opcodes::ACC_PUBLIC,
                                           ZUtf8StringRef::from_static("<init>"),
                                           ZUtf8StringRef::from_static("()V"),
                                           Utf8StringRef{},
                                           {});
        mv->visitCode();
        mv->visitVarInsn(Opcodes::ALOAD, 0);
        mv->visitMethodInsn(Opcodes::INVOKESPECIAL,
                            ZUtf8StringRef::from_static("java/lang/Object"),
                            ZUtf8StringRef::from_static("<init>"),
                            ZUtf8StringRef::from_static("()V"),
                            false);
        mv->visitInsn(Opcodes::RETURN);
        mv->visitMaxs(0, 0);
        mv->visitEnd();
    }

    // main method
    {
        MethodVisitor* mv = cw.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                                           ZUtf8StringRef::from_static("main"),
                                           ZUtf8StringRef::from_static("([Ljava/lang/String;)V"),
                                           Utf8StringRef{},
                                           {});
        mv->visitCode();
        mv->visitFieldInsn(Opcodes::GETSTATIC,
                           ZUtf8StringRef::from_static("java/lang/System"),
                           ZUtf8StringRef::from_static("out"),
                           ZUtf8StringRef::from_static("Ljava/io/PrintStream;"));
        mv->visitLdcInsn(ca::core::Any(std::string("Hello, World!")));
        mv->visitMethodInsn(Opcodes::INVOKEVIRTUAL,
                            ZUtf8StringRef::from_static("java/io/PrintStream"),
                            ZUtf8StringRef::from_static("println"),
                            ZUtf8StringRef::from_static("(Ljava/lang/String;)V"),
                            false);
        mv->visitInsn(Opcodes::RETURN);
        mv->visitMaxs(0, 0);
        mv->visitEnd();
    }

    // add method
    {
        MethodVisitor* mv = cw.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                                           ZUtf8StringRef::from_static("add"),
                                           ZUtf8StringRef::from_static("(II)I"),
                                           Utf8StringRef{},
                                           {});
        mv->visitCode();
        mv->visitVarInsn(Opcodes::ILOAD, 0);
        mv->visitVarInsn(Opcodes::ILOAD, 1);
        mv->visitInsn(Opcodes::IADD);
        mv->visitInsn(Opcodes::IRETURN);
        mv->visitMaxs(0, 0);
        mv->visitEnd();
    }

    // fact method
    {
        MethodVisitor* mv = cw.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                                           ZUtf8StringRef::from_static("fact"),
                                           ZUtf8StringRef::from_static("(I)I"),
                                           Utf8StringRef{},
                                           {});
        mv->visitCode();
        mv->visitVarInsn(Opcodes::ILOAD, 0);
        Label ifne;
        mv->visitJumpInsn(Opcodes::IFNE, &ifne);
        mv->visitInsn(Opcodes::ICONST_1);
        mv->visitInsn(Opcodes::IRETURN);
        mv->visitLabel(&ifne);
        mv->visitVarInsn(Opcodes::ILOAD, 0);
        mv->visitVarInsn(Opcodes::ILOAD, 0);
        mv->visitInsn(Opcodes::ICONST_1);
        mv->visitInsn(Opcodes::ISUB);
        mv->visitMethodInsn(Opcodes::INVOKESTATIC,
                            ZUtf8StringRef::from_static("HelloWorld"),
                            ZUtf8StringRef::from_static("fact"),
                            ZUtf8StringRef::from_static("(I)I"),
                            false);
        mv->visitInsn(Opcodes::IMUL);
        mv->visitInsn(Opcodes::IRETURN);
        mv->visitMaxs(0, 0);
        mv->visitEnd();
    }

    cw.visitEnd();
    auto bytes = cw.toByteArray();

    writeFile("HelloWorld.class", bytes);

    ClassReader verify(arena, bytes);
    std::cout << "GenerateHelloWorld Demo\n";
    std::cout << "  Class: " << toStr(verify.getClassName()) << "\n";
    std::cout << "  Size:  " << bytes.size() << " bytes\n";
    std::cout << "  Valid: "
              << (bytes.size() >= 4 && bytes[0] == 0xCA && bytes[1] == 0xFE && bytes[2] == 0xBA &&
                          bytes[3] == 0xBE
                      ? "YES"
                      : "NO")
              << "\n";
    return 0;
}
