#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>

#include "jbytecode/jbytecode.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::commons;
using namespace jbytecode::util;
using namespace ca::str;


static int testCount = 0, passCount = 0;

static void check(bool cond, const char* msg)
{
    testCount++;
    if (cond) {
        passCount++;
        printf("  PASS: %s\n", msg);
    }
    else {
        printf("  FAIL: %s\n", msg);
    }
}

int main()
{
    Utf8StringArena arena;

    printf("=== Test 1: simple branch ===\n");
    {
        ClassWriter cw(arena, ClassWriter::COMPUTE_FRAMES);
        cw.visit(Opcodes::V1_8,
                 Opcodes::ACC_PUBLIC,
                 ZUtf8StringRef::from_static("Test"),
                 Utf8StringRef(),
                 ZUtf8StringRef::from_static("java/lang/Object"),
                 {});
        auto* mv = cw.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                                  ZUtf8StringRef::from_static("test"),
                                  ZUtf8StringRef::from_static("(I)V"),
                                  Utf8StringRef(),
                                  {});
        mv->visitCode();
        Label elseLabel;
        mv->visitVarInsn(Opcodes::ILOAD, 0);
        mv->visitJumpInsn(Opcodes::IFLE, &elseLabel);
        mv->visitInsn(Opcodes::RETURN);
        mv->visitLabel(&elseLabel);
        mv->visitInsn(Opcodes::RETURN);
        mv->visitMaxs(1, 1);
        mv->visitEnd();
        try {
            auto bytes = cw.toByteArray();
            check(bytes.size() > 50, "toByteArray OK");
            ClassReader cr(arena, bytes.data(), bytes.size());
            ClassNode   cn;
            cr.accept(&cn, ClassReader::SKIP_FRAMES);
            check(true, "round-trip OK");
        }
        catch (const std::exception& e) {
            check(false, e.what());
        }
    }

    printf("=== Test 2: new + init ===\n");
    {
        ClassWriter cw(arena, ClassWriter::COMPUTE_FRAMES);
        cw.visit(Opcodes::V1_8,
                 Opcodes::ACC_PUBLIC,
                 ZUtf8StringRef::from_static("Test2"),
                 Utf8StringRef(),
                 ZUtf8StringRef::from_static("java/lang/Object"),
                 {});
        auto* mv = cw.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                                  ZUtf8StringRef::from_static("make"),
                                  ZUtf8StringRef::from_static("()Ljava/lang/String;"),
                                  Utf8StringRef(),
                                  {});
        mv->visitCode();
        mv->visitTypeInsn(Opcodes::NEW, ZUtf8StringRef::from_static("java/lang/String"));
        mv->visitInsn(Opcodes::DUP);
        mv->visitLdcInsn(std::string("hi"));
        mv->visitMethodInsn(Opcodes::INVOKESPECIAL,
                            ZUtf8StringRef::from_static("java/lang/String"),
                            ZUtf8StringRef::from_static("<init>"),
                            ZUtf8StringRef::from_static("(Ljava/lang/String;)V"),
                            false);
        mv->visitInsn(Opcodes::ARETURN);
        mv->visitMaxs(3, 0);
        mv->visitEnd();
        try {
            auto bytes = cw.toByteArray();
            check(bytes.size() > 50, "toByteArray OK");
            ClassReader cr(arena, bytes.data(), bytes.size());
            ClassNode   cn;
            cr.accept(&cn, ClassReader::EXPAND_FRAMES);
            check(true, "round-trip OK");
        }
        catch (const std::exception& e) {
            check(false, e.what());
        }
    }

    printf("=== Test 3: loop with merge ===\n");
    {
        ClassWriter cw(arena, ClassWriter::COMPUTE_FRAMES);
        cw.visit(Opcodes::V1_8,
                 Opcodes::ACC_PUBLIC,
                 ZUtf8StringRef::from_static("Test3"),
                 Utf8StringRef(),
                 ZUtf8StringRef::from_static("java/lang/Object"),
                 {});
        auto* mv = cw.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                                  ZUtf8StringRef::from_static("sum"),
                                  ZUtf8StringRef::from_static("(I)I"),
                                  Utf8StringRef(),
                                  {});
        mv->visitCode();
        Label loop, end;
        mv->visitInsn(Opcodes::ICONST_0);
        mv->visitVarInsn(Opcodes::ISTORE, 1);
        mv->visitInsn(Opcodes::ICONST_0);
        mv->visitVarInsn(Opcodes::ISTORE, 2);
        mv->visitLabel(&loop);
        mv->visitVarInsn(Opcodes::ILOAD, 2);
        mv->visitVarInsn(Opcodes::ILOAD, 0);
        mv->visitJumpInsn(Opcodes::IF_ICMPGE, &end);
        mv->visitVarInsn(Opcodes::ILOAD, 1);
        mv->visitVarInsn(Opcodes::ILOAD, 2);
        mv->visitInsn(Opcodes::IADD);
        mv->visitVarInsn(Opcodes::ISTORE, 1);
        mv->visitIincInsn(2, 1);
        mv->visitJumpInsn(Opcodes::GOTO, &loop);
        mv->visitLabel(&end);
        mv->visitVarInsn(Opcodes::ILOAD, 1);
        mv->visitInsn(Opcodes::IRETURN);
        mv->visitMaxs(2, 3);
        mv->visitEnd();
        try {
            auto bytes = cw.toByteArray();
            check(bytes.size() > 80, "toByteArray OK");
            ClassReader cr(arena, bytes.data(), bytes.size());
            ClassNode   cn;
            cr.accept(&cn, ClassReader::SKIP_FRAMES);
            check(true, "round-trip OK");
        }
        catch (const std::exception& e) {
            check(false, e.what());
        }
    }

    printf("\n%d/%d passed\n", passCount, testCount);
    return passCount == testCount ? 0 : 1;
}
