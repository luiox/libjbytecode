#include <gtest/gtest.h>
#include "jbytecode/core/symbol_table.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

// Tests matching ASM SymbolTable behavior.
// ASM tests SymbolTable indirectly through ClassReader/ClassWriter tests.
// Here we test the public API directly.

TEST(SymbolTableTest, ADDconstantUtf8dedup)
{
    SymbolTable st(asmArena);
    int         idx1 = st.addConstantUtf8(ZUtf8StringRef::from_static("Hello"));
    int         idx2 = st.addConstantUtf8(ZUtf8StringRef::from_static("Hello"));
    ASSERT_EQ(idx1, idx2);
    ASSERT_GE(idx1, 1);
    ASSERT_EQ(st.getConstantPoolCount(), 2);   // constantPoolCount starts at 1
}

TEST(SymbolTableTest, addConstantClass)
{
    SymbolTable st(asmArena);
    Symbol*     cls1 = st.addConstantClass(ZUtf8StringRef::from_static("java/lang/Object"));
    Symbol*     cls2 = st.addConstantClass(ZUtf8StringRef::from_static("java/lang/Object"));
    ASSERT_EQ(cls1->index, cls2->index);
    ASSERT_GE(cls1->index, 1);
    ASSERT_EQ(cls1->tag, Symbol::CONSTANT_CLASS_TAG);
}

TEST(SymbolTableTest, addConstantInteger)
{
    SymbolTable st(asmArena);
    Symbol*     s1 = st.addConstantInteger(42);
    Symbol*     s2 = st.addConstantInteger(42);
    ASSERT_EQ(s1->index, s2->index);
    ASSERT_EQ(s1->tag, Symbol::CONSTANT_INTEGER_TAG);
    ASSERT_EQ(s1->data, 42);
}

TEST(SymbolTableTest, addConstantFloat)
{
    SymbolTable st(asmArena);
    Symbol*     s1 = st.addConstantFloat(3.14f);
    Symbol*     s2 = st.addConstantFloat(3.14f);
    ASSERT_EQ(s1->index, s2->index);
    ASSERT_EQ(s1->tag, Symbol::CONSTANT_FLOAT_TAG);
}

TEST(SymbolTableTest, addConstantLong)
{
    SymbolTable st(asmArena);
    Symbol*     s1 = st.addConstantLong(123456789012LL);
    Symbol*     s2 = st.addConstantLong(123456789012LL);
    ASSERT_EQ(s1->index, s2->index);
    ASSERT_EQ(s1->tag, Symbol::CONSTANT_LONG_TAG);
    // Long takes 2 CP slots
    ASSERT_EQ(st.getConstantPoolCount(), 3);   // start 1, +2
}

TEST(SymbolTableTest, addConstantDouble)
{
    SymbolTable st(asmArena);
    Symbol*     s1 = st.addConstantDouble(2.71828);
    Symbol*     s2 = st.addConstantDouble(2.71828);
    ASSERT_EQ(s1->index, s2->index);
    ASSERT_EQ(s1->tag, Symbol::CONSTANT_DOUBLE_TAG);
    // Double takes 2 CP slots
    ASSERT_EQ(st.getConstantPoolCount(), 3);
}

TEST(SymbolTableTest, addConstantString)
{
    SymbolTable st(asmArena);
    Symbol*     s1 = st.addConstantString(ZUtf8StringRef::from_static("Hello"));
    Symbol*     s2 = st.addConstantString(ZUtf8StringRef::from_static("Hello"));
    ASSERT_EQ(s1->index, s2->index);
    ASSERT_EQ(s1->tag, Symbol::CONSTANT_STRING_TAG);
}

TEST(SymbolTableTest, addConstantNameAndType)
{
    SymbolTable st(asmArena);
    int         nat = st.addConstantNameAndType(ZUtf8StringRef::from_static("foo"),
                                        ZUtf8StringRef::from_static("()V"));
    ASSERT_GT(nat, 0);
    // Dedup
    int nat2 = st.addConstantNameAndType(ZUtf8StringRef::from_static("foo"),
                                         ZUtf8StringRef::from_static("()V"));
    ASSERT_EQ(nat, nat2);
}

TEST(SymbolTableTest, addConstantFieldref)
{
    SymbolTable st(asmArena);
    Symbol*     ref1 = st.addConstantFieldref(ZUtf8StringRef::from_static("java/lang/System"),
                                          ZUtf8StringRef::from_static("out"),
                                          ZUtf8StringRef::from_static("Ljava/io/PrintStream;"));
    Symbol*     ref2 = st.addConstantFieldref(ZUtf8StringRef::from_static("java/lang/System"),
                                          ZUtf8StringRef::from_static("out"),
                                          ZUtf8StringRef::from_static("Ljava/io/PrintStream;"));
    ASSERT_EQ(ref1->index, ref2->index);
    ASSERT_EQ(ref1->tag, Symbol::CONSTANT_FIELDREF_TAG);
}

TEST(SymbolTableTest, addConstantMethodref)
{
    SymbolTable st(asmArena);
    Symbol*     ref = st.addConstantMethodref(ZUtf8StringRef::from_static("java/lang/String"),
                                          ZUtf8StringRef::from_static("length"),
                                          ZUtf8StringRef::from_static("()I"),
                                          false);
    ASSERT_GT(ref->index, 0);
    ASSERT_EQ(ref->tag, Symbol::CONSTANT_METHODREF_TAG);
}

TEST(SymbolTableTest, addConstantInterfaceMethodref)
{
    SymbolTable st(asmArena);
    Symbol*     ref = st.addConstantMethodref(ZUtf8StringRef::from_static("java/util/List"),
                                          ZUtf8StringRef::from_static("size"),
                                          ZUtf8StringRef::from_static("()I"),
                                          true);
    ASSERT_GT(ref->index, 0);
    ASSERT_EQ(ref->tag, Symbol::CONSTANT_INTERFACE_METHODREF_TAG);
}

TEST(SymbolTableTest, addType)
{
    SymbolTable st(asmArena);
    int         t1 = st.addType(ZUtf8StringRef::from_static("java/lang/Object"));
    int         t2 = st.addType(ZUtf8StringRef::from_static("java/lang/String"));
    int         t3 = st.addType(ZUtf8StringRef::from_static("java/lang/Object"));
    ASSERT_EQ(t1, t3);   // dedup
    ASSERT_NE(t1, t2);
    Symbol* s = st.getType(t1);
    ASSERT_NE(s, nullptr);
    ASSERT_TRUE(s->value.equals(ZUtf8StringRef::from_static("java/lang/Object")));
}

TEST(SymbolTableTest, addUninitializedType)
{
    SymbolTable st(asmArena);
    int         t1 = st.addUninitializedType(ZUtf8StringRef::from_static(""), 42);
    int         t2 = st.addUninitializedType(ZUtf8StringRef::from_static(""), 42);
    ASSERT_EQ(t1, t2);
    ASSERT_GE(t1, 0);
}

TEST(SymbolTableTest, addConstantMethodHandle)
{
    SymbolTable st(asmArena);
    Symbol*     mh = st.addConstantMethodHandle(6,   // H_INVOKESTATIC
                                            ZUtf8StringRef::from_static("java/lang/Math"),
                                            ZUtf8StringRef::from_static("max"),
                                            ZUtf8StringRef::from_static("(II)I"),
                                            false);
    ASSERT_GT(mh->index, 0);
    ASSERT_EQ(mh->tag, Symbol::CONSTANT_METHOD_HANDLE_TAG);
}

TEST(SymbolTableTest, addForwardUninitializedType)
{
    SymbolTable st(asmArena);
    Label       label;
    label.bytecodeOffset = 42;
    int t1 =
        st.addForwardUninitializedType(ZUtf8StringRef::from_static("java/lang/Object"), &label);
    int t2 =
        st.addForwardUninitializedType(ZUtf8StringRef::from_static("java/lang/Object"), &label);
    ASSERT_EQ(t1, t2);
    ASSERT_GE(t1, 0);
    Label* retrieved = st.getForwardUninitializedLabel(t1);
    ASSERT_EQ(retrieved, &label);
}

TEST(SymbolTableTest, addConstantDynamic)
{
    SymbolTable                st(asmArena);
    Handle                     bsm(Opcodes::H_INVOKESTATIC,
               ZUtf8StringRef::from_static("java/lang/invoke/LambdaMetafactory"),
               ZUtf8StringRef::from_static("metafactory"),
               ZUtf8StringRef::from_static(
                   "(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/"
                                       "MethodType;Ljava/lang/invoke/MethodType;Ljava/lang/invoke/MethodHandle;Ljava/"
                                       "lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"),
               false);
    std::vector<ca::core::Any> args;
    args.push_back(std::string("()V"));
    Symbol* dyn = st.addConstantDynamic(
        ZUtf8StringRef::from_static("run"), ZUtf8StringRef::from_static("()V"), bsm, args);
    ASSERT_GT(dyn->index, 0);
    ASSERT_EQ(dyn->tag, Symbol::CONSTANT_DYNAMIC_TAG);
}


TEST(SymbolTableTest, addConstantInvokeDynamic)
{
    SymbolTable                st(asmArena);
    Handle                     bsm(Opcodes::H_INVOKESTATIC,
               ZUtf8StringRef::from_static("java/lang/invoke/LambdaMetafactory"),
               ZUtf8StringRef::from_static("metafactory"),
               ZUtf8StringRef::from_static(
                   "(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/"
                                       "MethodType;Ljava/lang/invoke/MethodType;Ljava/lang/invoke/MethodHandle;Ljava/"
                                       "lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"),
               false);
    std::vector<ca::core::Any> args;
    args.push_back(std::string("()V"));
    Symbol* indy = st.addConstantInvokeDynamic(
        ZUtf8StringRef::from_static("run"), ZUtf8StringRef::from_static("()V"), bsm, args);
    ASSERT_GT(indy->index, 0);
    ASSERT_EQ(indy->tag, Symbol::CONSTANT_INVOKE_DYNAMIC_TAG);
}
