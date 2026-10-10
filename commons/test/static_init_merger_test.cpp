#include <gtest/gtest.h>
#include "test_utils.hpp"

TEST(StaticInitMergerTest, testAllMethods_multipleStaticInitBlocks)
{
    ClassNode classNode;
    classNode.visit(Opcodes::V1_1,
                    Opcodes::ACC_PUBLIC,
                    ZUtf8StringRef::from_static("A"),
                    ZUtf8StringRef::from_static(""),
                    ZUtf8StringRef::from_static("java/lang/Object"),
                    {});
    classNode.visitField(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                         ZUtf8StringRef::from_static("counter"),
                         ZUtf8StringRef::from_static("I"),
                         ZUtf8StringRef::from_static(""),
                         ca::core::Any{});
    for (int i = 0; i < 5; i++) {
        MethodVisitor* mv = classNode.visitMethod(Opcodes::ACC_PUBLIC,
                                                  ZUtf8StringRef::from_static("<clinit>"),
                                                  ZUtf8StringRef::from_static("()V"),
                                                  ZUtf8StringRef::from_static(""),
                                                  {});
        mv->visitCode();
        mv->visitFieldInsn(Opcodes::GETSTATIC,
                           ZUtf8StringRef::from_static("A"),
                           ZUtf8StringRef::from_static("counter"),
                           ZUtf8StringRef::from_static("I"));
        mv->visitInsn(Opcodes::ICONST_1);
        mv->visitInsn(Opcodes::IADD);
        mv->visitFieldInsn(Opcodes::PUTSTATIC,
                           ZUtf8StringRef::from_static("A"),
                           ZUtf8StringRef::from_static("counter"),
                           ZUtf8StringRef::from_static("I"));
        mv->visitInsn(Opcodes::RETURN);
        mv->visitMaxs(2, 0);
        mv->visitEnd();
    }
    MethodVisitor* mv = classNode.visitMethod(Opcodes::ACC_PUBLIC,
                                              ZUtf8StringRef::from_static("<init>"),
                                              ZUtf8StringRef::from_static("()V"),
                                              ZUtf8StringRef::from_static(""),
                                              {});
    mv->visitCode();
    mv->visitVarInsn(Opcodes::ALOAD, 0);
    mv->visitMethodInsn(Opcodes::INVOKESPECIAL,
                        ZUtf8StringRef::from_static("java/lang/Object"),
                        ZUtf8StringRef::from_static("<init>"),
                        ZUtf8StringRef::from_static("()V"),
                        false);
    mv->visitInsn(Opcodes::RETURN);
    mv->visitMaxs(1, 1);
    mv->visitEnd();
}
