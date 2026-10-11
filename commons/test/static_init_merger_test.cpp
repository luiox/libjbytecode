#include <gtest/gtest.h>
#include <set>
#include <string>
#include <vector>
#include "test_utils.hpp"
#include "libca/str/utf8_string_arena.hpp"

// 对齐上游 ASM StaticInitMergerTest#testAllMethods_multipleStaticInitBlocks：
// 上游把合并产物载入 JVM 实例化并断言 counter==5；C++ 侧无执行环境，改用结构
// 等价验证（合并产物 = 1 个 <clinit> 顺序 INVOKESTATIC 5 个重命名块 + <init>
// + 各重命名块保留 counter 自增指令流）。此前移植只构建输入类即返回（零断言空跑）。
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

    // ── 应用 StaticInitMerger（上游前缀 "$clinit$"，COMPUTE_MAXS） ──
    ca::str::Utf8StringArena arena;
    ClassWriter              classWriter(arena, ClassWriter::COMPUTE_MAXS);
    StaticInitMerger         merger(ZUtf8StringRef::from_static("$clinit$"), &classWriter);
    classNode.accept(&merger);

    auto bytes = classWriter.toByteArray();
    ASSERT_GT(bytes.size(), 4u);
    EXPECT_EQ(0xCA, bytes[0]);
    EXPECT_EQ(0xFE, bytes[1]);
    EXPECT_EQ(0xBA, bytes[2]);
    EXPECT_EQ(0xBE, bytes[3]);

    // ── 重解析合并产物，做结构等价断言 ──
    ClassNode   merged;
    ClassReader reader(arena, bytes);
    reader.accept(&merged, 0);

    MethodNode*           clinit = nullptr;
    MethodNode*           init   = nullptr;
    std::set<std::string> renamed;
    for (auto* method : merged.methods) {
        std::string name = method->name.to_std_string();
        if (name == "<clinit>") {
            clinit = method;
        } else if (name == "<init>") {
            init = method;
        } else if (name.rfind("$clinit$", 0) == 0) {
            renamed.insert(name);
            EXPECT_EQ(Opcodes::ACC_PRIVATE | Opcodes::ACC_STATIC, method->access) << name;
        }
    }
    ASSERT_NE(nullptr, clinit);
    ASSERT_NE(nullptr, init);
    ASSERT_EQ(5u, renamed.size());
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(1u, renamed.count("$clinit$" + std::to_string(i)));
    }

    // 合并 <clinit>：按序 INVOKESTATIC 5 个重命名块（等价于执行后 counter==5）
    std::vector<std::string> invoked;
    for (auto* insn = clinit->instructions.getFirst(); insn; insn = insn->getNext()) {
        if (insn->getType() == AbstractInsnNode::METHOD_INSN) {
            invoked.push_back(static_cast<MethodInsnNode*>(insn)->name.to_std_string());
        }
    }
    EXPECT_EQ((std::vector<std::string>{
                  "$clinit$0", "$clinit$1", "$clinit$2", "$clinit$3", "$clinit$4"}),
              invoked);

    // 各重命名块保留原 <clinit> 的 counter 自增指令流
    for (auto* method : merged.methods) {
        std::string name = method->name.to_std_string();
        if (name.rfind("$clinit$", 0) != 0) continue;
        std::vector<int> opcodes;
        for (auto* insn = method->instructions.getFirst(); insn; insn = insn->getNext()) {
            if (insn->getType() == AbstractInsnNode::LABEL) continue;
            opcodes.push_back(insn->getOpcode());
        }
        EXPECT_EQ((std::vector<int>{
                      Opcodes::GETSTATIC, Opcodes::ICONST_1, Opcodes::IADD, Opcodes::PUTSTATIC,
                      Opcodes::RETURN}),
                  opcodes)
            << name;
    }
}
