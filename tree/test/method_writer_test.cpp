#include <gtest/gtest.h>
#include <vector>
#include <cstdio>
#include "test_utils.hpp"

// 测试基类：构造一个类，里面有一个包含 N 条指定模式指令的方法
struct MethodWriterTest : ::testing::Test
{
    ClassNode* makeBase()
    {
        auto* cn = new ClassNode();
        cn->visit(52 << 16,
                  Opcodes::ACC_PUBLIC,
                  ZUtf8StringRef::from_static("Test"),
                  ZUtf8StringRef::from_static(""),
                  ZUtf8StringRef::from_static("java/lang/Object"),
                  {});
        cn->visitSource(ZUtf8StringRef::from_static("Test.java"), ZUtf8StringRef::from_static(""));
        return cn;
    }
    void addMethod(ClassNode* cn, const char* name, const char* desc, int maxStack, int maxLocals)
    {
        mn            = new MethodNode(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                            Utf8StringRef::from_cstr(name),
                            Utf8StringRef::from_cstr(desc),
                            ZUtf8StringRef::from_static(""),
                                       {});
        mn->maxStack  = maxStack;
        mn->maxLocals = maxLocals;
        cn->methods.push_back(mn);
    }
    void verify(ClassNode* cn)
    {
        ClassWriter w(tArena);
        cn->accept(&w);
        auto        out = w.toByteArray();
        ClassNode   cn2;
        ClassReader r(tArena, out.data(), out.size());
        r.accept(&cn2, 0);
        EXPECT_EQ(cn2.methods.size(), cn->methods.size());

        // 诊断：检查 re-parse 后的方法状态
        for (size_t i = 0; i < cn->methods.size() && i < cn2.methods.size(); i++) {
            auto* m1 = cn->methods[i];
            auto* m2 = cn2.methods[i];
            EXPECT_EQ(m1->instructions.size(), m2->instructions.size())
                << "method[" << i << "] " << m1->name.to_std_string() << ": insn count "
                << m1->instructions.size() << " vs " << m2->instructions.size();
        }

        ClassWriter w2(tArena);
        cn2.accept(&w2);
        auto out2 = w2.toByteArray();
        if (out != out2) {
            size_t fd = 0;
            while (fd < out.size() && fd < out2.size() && out[fd] == out2[fd]) fd++;
            ADD_FAILURE() << "Idempotency failed, first diff at " << fd << " (size " << out.size()
                          << " vs " << out2.size() << ")";
        }
        delete cn;
    }
    MethodNode* mn = nullptr;
};

TEST_F(MethodWriterTest, manyNops)
{
    auto* cn = makeBase();
    addMethod(cn, "test", "()V", 0, 0);
    for (int i = 0; i < 5000; i++) mn->instructions.add(new InsnNode(Opcodes::NOP));
    mn->instructions.add(new InsnNode(Opcodes::RETURN));
    verify(cn);
}

TEST_F(MethodWriterTest, manyIconst)
{
    auto* cn = makeBase();
    addMethod(cn, "test", "()I", 2, 0);
    for (int i = 0; i < 2000; i++) {
        mn->instructions.add(new InsnNode(Opcodes::ICONST_0));
        mn->instructions.add(new InsnNode(Opcodes::ICONST_1));
        mn->instructions.add(new InsnNode(Opcodes::IADD));
    }
    mn->instructions.add(new InsnNode(Opcodes::IRETURN));
    verify(cn);
}

TEST_F(MethodWriterTest, singleInvokeStatic)
{
    auto* cn = makeBase();
    addMethod(cn, "test", "()I", 3, 3);
    mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));
    mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 1));
    mn->instructions.add(new MethodInsnNode(Opcodes::INVOKESTATIC,
                                            ZUtf8StringRef::from_static("java/lang/Math"),
                                            ZUtf8StringRef::from_static("max"),
                                            ZUtf8StringRef::from_static("(II)I"),
                                            false));
    mn->instructions.add(new InsnNode(Opcodes::IRETURN));
    verify(cn);
}

TEST_F(MethodWriterTest, manyInvokes)
{
    auto* cn = makeBase();
    addMethod(cn, "test", "()I", 3, 3);
    for (int i = 0; i < 1000; i++) {
        mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));
        mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 1));
        mn->instructions.add(new MethodInsnNode(Opcodes::INVOKESTATIC,
                                                ZUtf8StringRef::from_static("java/lang/Math"),
                                                ZUtf8StringRef::from_static("max"),
                                                ZUtf8StringRef::from_static("(II)I"),
                                                false));
        mn->instructions.add(new VarInsnNode(Opcodes::ISTORE, 2));
    }
    mn->instructions.add(new InsnNode(Opcodes::IRETURN));
    verify(cn);
}

TEST_F(MethodWriterTest, manyLdc)
{
    auto* cn = makeBase();
    addMethod(cn, "test", "()V", 2, 0);
    for (int i = 0; i < 1000; i++) {
        mn->instructions.add(new LdcInsnNode(std::string("hello")));
        mn->instructions.add(new InsnNode(Opcodes::POP));
    }
    mn->instructions.add(new InsnNode(Opcodes::RETURN));
    verify(cn);
}

TEST_F(MethodWriterTest, manyFields)
{
    auto* cn = makeBase();
    // 加一些静态字段给 getstatic 用
    for (int i = 0; i < 100; i++) {
        auto fname = "f" + std::to_string(i);
        // fname 是语句级 std::string：字段名必须 intern 进测试 arena 取得池内视图，
        // 否则 ClassNode 存下悬垂视图，写回时 ASan 报 stack-use-after-scope。
        cn->visitField(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                       tArena.intern(fname.c_str()),
                       ZUtf8StringRef::from_static("I"),
                       ZUtf8StringRef::from_static(""),
                       ca::core::Any())
            ->visitEnd();
    }
    addMethod(cn, "test", "()I", 2, 0);
    for (int i = 0; i < 500; i++) {
        auto fname = "f" + std::to_string(i % 100);
        // 同上：语句级 std::string 不得以裸视图存入 ClassNode，intern 进测试 arena。
        mn->instructions.add(new FieldInsnNode(Opcodes::GETSTATIC,
                                               ZUtf8StringRef::from_static("Test"),
                                               tArena.intern(fname.c_str()),
                                               ZUtf8StringRef::from_static("I")));
        mn->instructions.add(new InsnNode(Opcodes::ICONST_1));
        mn->instructions.add(new InsnNode(Opcodes::IADD));
    }
    mn->instructions.add(new InsnNode(Opcodes::IRETURN));
    verify(cn);
}

TEST_F(MethodWriterTest, manyBranches)
{
    auto* cn = makeBase();
    addMethod(cn, "test", "()V", 2, 2);
    Label* l = new Label();
    for (int i = 0; i < 1000; i++) {
        mn->instructions.add(new InsnNode(Opcodes::ICONST_0));
        mn->instructions.add(new JumpInsnNode(Opcodes::IFEQ, l));
        mn->instructions.add(new LabelNode(l));
        l = new Label();
    }
    mn->instructions.add(new InsnNode(Opcodes::RETURN));
    verify(cn);
}
