#pragma once
#include "libca/core/any.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/constants.hpp"

using namespace jbytecode::core;
using namespace ca::str;

inline std::vector<uint8_t> readFile(const std::string& path)
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

inline void writeFile(const std::string& path, const std::vector<uint8_t>& data)
{
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

// 演示用显示辅助：Utf8StringRef -> std::string（直接用原生 to_std_string，避免手写 reinterpret_cast
// 胶水）。
inline std::string toStr(Utf8StringRef ref)
{
    return ref.to_std_string();
}

inline std::string accessFlagsToString(int access)
{
    std::string s;
    if (access & Opcodes::ACC_PUBLIC) s += "public ";
    if (access & Opcodes::ACC_PRIVATE) s += "private ";
    if (access & Opcodes::ACC_PROTECTED) s += "protected ";
    if (access & Opcodes::ACC_STATIC) s += "static ";
    if (access & Opcodes::ACC_FINAL) s += "final ";
    if (access & Opcodes::ACC_SUPER) s += "super ";
    if (access & Opcodes::ACC_SYNCHRONIZED) s += "synchronized ";
    if (access & Opcodes::ACC_VOLATILE) s += "volatile ";
    if (access & Opcodes::ACC_BRIDGE) s += "bridge ";
    if (access & Opcodes::ACC_VARARGS) s += "varargs ";
    if (access & Opcodes::ACC_TRANSIENT) s += "transient ";
    if (access & Opcodes::ACC_NATIVE) s += "native ";
    if (access & Opcodes::ACC_INTERFACE) s += "interface ";
    if (access & Opcodes::ACC_ABSTRACT) s += "abstract ";
    if (access & Opcodes::ACC_STRICT) s += "strict ";
    if (access & Opcodes::ACC_SYNTHETIC) s += "synthetic ";
    if (access & Opcodes::ACC_ANNOTATION) s += "annotation ";
    if (access & Opcodes::ACC_ENUM) s += "enum ";
    if (access & Opcodes::ACC_RECORD) s += "record ";
    return s;
}

inline std::vector<uint8_t> createSampleClass()
{
    ca::str::Utf8StringArena arena;
    ClassWriter              cw(arena, ClassWriter::COMPUTE_FRAMES);
    cw.visit(Opcodes::V17,
             Opcodes::ACC_PUBLIC,
             ZUtf8StringRef::from_static("SampleClass"),
             Utf8StringRef{},
             ZUtf8StringRef::from_static("java/lang/Object"),
             {ZUtf8StringRef::from_static("java/io/Serializable")});

    cw.visitField(Opcodes::ACC_PRIVATE,
                  ZUtf8StringRef::from_static("count"),
                  ZUtf8StringRef::from_static("I"),
                  Utf8StringRef{},
                  ca::core::Any{});
    cw.visitField(Opcodes::ACC_PRIVATE,
                  ZUtf8StringRef::from_static("name"),
                  ZUtf8StringRef::from_static("Ljava/lang/String;"),
                  Utf8StringRef{},
                  ca::core::Any{});

    {
        MethodVisitor* mv = cw.visitMethod(Opcodes::ACC_PUBLIC,
                                           ZUtf8StringRef::from_static("<init>"),
                                           ZUtf8StringRef::from_static("(Ljava/lang/String;)V"),
                                           Utf8StringRef{},
                                           {});
        mv->visitCode();
        mv->visitVarInsn(Opcodes::ALOAD, 0);
        mv->visitMethodInsn(Opcodes::INVOKESPECIAL,
                            ZUtf8StringRef::from_static("java/lang/Object"),
                            ZUtf8StringRef::from_static("<init>"),
                            ZUtf8StringRef::from_static("()V"),
                            false);
        mv->visitVarInsn(Opcodes::ALOAD, 0);
        mv->visitVarInsn(Opcodes::ALOAD, 1);
        mv->visitFieldInsn(Opcodes::PUTFIELD,
                           ZUtf8StringRef::from_static("SampleClass"),
                           ZUtf8StringRef::from_static("name"),
                           ZUtf8StringRef::from_static("Ljava/lang/String;"));
        mv->visitVarInsn(Opcodes::ALOAD, 0);
        mv->visitInsn(Opcodes::ICONST_0);
        mv->visitFieldInsn(Opcodes::PUTFIELD,
                           ZUtf8StringRef::from_static("SampleClass"),
                           ZUtf8StringRef::from_static("count"),
                           ZUtf8StringRef::from_static("I"));
        mv->visitInsn(Opcodes::RETURN);
        mv->visitMaxs(0, 0);
        mv->visitEnd();
    }

    {
        MethodVisitor* mv = cw.visitMethod(Opcodes::ACC_PUBLIC,
                                           ZUtf8StringRef::from_static("getCount"),
                                           ZUtf8StringRef::from_static("()I"),
                                           Utf8StringRef{},
                                           {});
        mv->visitCode();
        mv->visitVarInsn(Opcodes::ALOAD, 0);
        mv->visitFieldInsn(Opcodes::GETFIELD,
                           ZUtf8StringRef::from_static("SampleClass"),
                           ZUtf8StringRef::from_static("count"),
                           ZUtf8StringRef::from_static("I"));
        mv->visitInsn(Opcodes::IRETURN);
        mv->visitMaxs(0, 0);
        mv->visitEnd();
    }

    {
        MethodVisitor* mv = cw.visitMethod(Opcodes::ACC_PUBLIC,
                                           ZUtf8StringRef::from_static("setCount"),
                                           ZUtf8StringRef::from_static("(I)V"),
                                           Utf8StringRef{},
                                           {});
        mv->visitCode();
        mv->visitVarInsn(Opcodes::ALOAD, 0);
        mv->visitVarInsn(Opcodes::ILOAD, 1);
        mv->visitFieldInsn(Opcodes::PUTFIELD,
                           ZUtf8StringRef::from_static("SampleClass"),
                           ZUtf8StringRef::from_static("count"),
                           ZUtf8StringRef::from_static("I"));
        mv->visitInsn(Opcodes::RETURN);
        mv->visitMaxs(0, 0);
        mv->visitEnd();
    }

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

    {
        MethodVisitor* mv =
            cw.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                           ZUtf8StringRef::from_static("greet"),
                           ZUtf8StringRef::from_static("(Ljava/lang/String;)Ljava/lang/String;"),
                           Utf8StringRef{},
                           {});
        mv->visitCode();
        Label notNull;
        mv->visitVarInsn(Opcodes::ALOAD, 0);
        mv->visitJumpInsn(Opcodes::IFNONNULL, &notNull);
        mv->visitLdcInsn(ca::core::Any(std::string("Hello, World")));
        mv->visitInsn(Opcodes::ARETURN);
        mv->visitLabel(&notNull);
        mv->visitVarInsn(Opcodes::ALOAD, 0);
        mv->visitInsn(Opcodes::ARETURN);
        mv->visitMaxs(0, 0);
        mv->visitEnd();
    }

    {
        MethodVisitor* mv = cw.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                                           ZUtf8StringRef::from_static("run"),
                                           ZUtf8StringRef::from_static("()V"),
                                           Utf8StringRef{},
                                           {});
        mv->visitCode();
        mv->visitFieldInsn(Opcodes::GETSTATIC,
                           ZUtf8StringRef::from_static("java/lang/System"),
                           ZUtf8StringRef::from_static("out"),
                           ZUtf8StringRef::from_static("Ljava/io/PrintStream;"));
        mv->visitLdcInsn(ca::core::Any(std::string("Hello from SampleClass!")));
        mv->visitMethodInsn(Opcodes::INVOKEVIRTUAL,
                            ZUtf8StringRef::from_static("java/io/PrintStream"),
                            ZUtf8StringRef::from_static("println"),
                            ZUtf8StringRef::from_static("(Ljava/lang/String;)V"),
                            false);
        mv->visitInsn(Opcodes::RETURN);
        mv->visitMaxs(0, 0);
        mv->visitEnd();
    }

    cw.visitEnd();
    return cw.toByteArray();
}
