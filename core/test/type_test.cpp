#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "jbytecode/core/type.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

TEST(TypeTest, testGetSort_typeConstants)
{
    ASSERT_EQ(Type::VOID, Type::VOID_TYPE.getSort());
    ASSERT_EQ(Type::BOOLEAN, Type::BOOLEAN_TYPE.getSort());
    ASSERT_EQ(Type::CHAR, Type::CHAR_TYPE.getSort());
    ASSERT_EQ(Type::BYTE, Type::BYTE_TYPE.getSort());
    ASSERT_EQ(Type::SHORT, Type::SHORT_TYPE.getSort());
    ASSERT_EQ(Type::INT, Type::INT_TYPE.getSort());
    ASSERT_EQ(Type::FLOAT, Type::FLOAT_TYPE.getSort());
    ASSERT_EQ(Type::LONG, Type::LONG_TYPE.getSort());
    ASSERT_EQ(Type::DOUBLE, Type::DOUBLE_TYPE.getSort());
}

TEST(TypeTest, testGetSize_typeConstants)
{
    ASSERT_EQ(0, Type::VOID_TYPE.getSize());
    ASSERT_EQ(1, Type::BOOLEAN_TYPE.getSize());
    ASSERT_EQ(1, Type::CHAR_TYPE.getSize());
    ASSERT_EQ(1, Type::BYTE_TYPE.getSize());
    ASSERT_EQ(1, Type::SHORT_TYPE.getSize());
    ASSERT_EQ(1, Type::INT_TYPE.getSize());
    ASSERT_EQ(1, Type::FLOAT_TYPE.getSize());
    ASSERT_EQ(2, Type::LONG_TYPE.getSize());
    ASSERT_EQ(2, Type::DOUBLE_TYPE.getSize());
}

TEST(TypeTest, testGetDescriptor_typeConstants)
{
    // #921:无参 getDescriptor() 已删除,统一走 getDescriptor(arena) 注入版。
    ca::str::Utf8StringArena arena;
    ASSERT_STREQ("V", Type::VOID_TYPE.getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ("Z", Type::BOOLEAN_TYPE.getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ("C", Type::CHAR_TYPE.getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ("B", Type::BYTE_TYPE.getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ("S", Type::SHORT_TYPE.getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ("I", Type::INT_TYPE.getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ("F", Type::FLOAT_TYPE.getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ("J", Type::LONG_TYPE.getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ("J", Type::LONG_TYPE.getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ("D", Type::DOUBLE_TYPE.getDescriptor(arena).to_std_string().c_str());
}

TEST(TypeTest, testGetTypeFromDescriptor)
{
    const char* descriptors[] = {"I",
                                 "V",
                                 "Z",
                                 "B",
                                 "C",
                                 "S",
                                 "D",
                                 "F",
                                 "J",
                                 "LI;",
                                 "LV;",
                                 "Ljava/lang/Object;",
                                 "[I",
                                 "[LI;",
                                 "[[Ljava/lang/Object;",
                                 "(IZBCSDFJLI;LV;Ljava/lang/Object;[I[LI;[[Ljava/lang/Object;)V",
                                 "()I",
                                 "()LI;",
                                 "()Ljava/lang/Object;",
                                 "()[I",
                                 "()[LI;",
                                 "()[[Ljava/lang/Object;"};
    ca::str::Utf8StringArena arena;   // #921:注入版替换已删除的无参重载
    for (const char* desc : descriptors) {
        Type t = Type::getType(Utf8StringRef::from_cstr(desc));
        ASSERT_STREQ(desc, t.getDescriptor(arena).to_std_string().c_str());
        ASSERT_STREQ(desc, t.toString().c_str());
    }
}

TEST(TypeTest, testGetTypeFromDescriptor_invalid)
{
    try {
        Type::getType(ZUtf8StringRef::from_static("-"));
        FAIL() << "Expected std::invalid_argument";
    }
    catch (const std::invalid_argument& e) {
        ASSERT_STREQ("Invalid descriptor: -", e.what());
    }
}

TEST(TypeTest, testGetObjectType)
{
    ca::str::Utf8StringArena arena;   // #921:注入版替换已删除的无参重载
    Type t1 = Type::getObjectType(ZUtf8StringRef::from_static("I"));
    ASSERT_STREQ("I", t1.getInternalName(arena).to_std_string().c_str());
    ASSERT_EQ(Type::OBJECT, t1.getSort());
    Type t2 = Type::getObjectType(ZUtf8StringRef::from_static("java/lang/Object"));
    ASSERT_STREQ("java/lang/Object", t2.getInternalName(arena).to_std_string().c_str());
    ASSERT_EQ(Type::OBJECT, t2.getSort());
    Type t3 = Type::getObjectType(ZUtf8StringRef::from_static("[I"));
    ASSERT_STREQ("[I", t3.getInternalName(arena).to_std_string().c_str());
    ASSERT_EQ(Type::ARRAY, t3.getSort());
    Type t4 = Type::getObjectType(ZUtf8StringRef::from_static("[LI;"));
    ASSERT_STREQ("[LI;", t4.getInternalName(arena).to_std_string().c_str());
    ASSERT_EQ(Type::ARRAY, t4.getSort());
    Type t5 = Type::getObjectType(ZUtf8StringRef::from_static("[[Ljava/lang/Object;"));
    ASSERT_STREQ("[[Ljava/lang/Object;", t5.getInternalName(arena).to_std_string().c_str());
    ASSERT_EQ(Type::ARRAY, t5.getSort());
}

TEST(TypeTest, testGetMethodTypeFromDescriptor)
{
    const char* mds[] = {"(IZBCSDFJLI;LV;Ljava/lang/Object;[I[LI;[[Ljava/lang/Object;)V",
                         "()I",
                         "()LI;",
                         "()Ljava/lang/Object;",
                         "()[I",
                         "()[LI;",
                         "()[[Ljava/lang/Object;"};
    ca::str::Utf8StringArena arena;   // #921:注入版替换已删除的无参重载
    for (const char* md : mds) {
        Type t = Type::getMethodType(Utf8StringRef::from_cstr(md));
        ASSERT_EQ(Type::METHOD, t.getSort());
        ASSERT_STREQ(md, t.getDescriptor(arena).to_std_string().c_str());
        ASSERT_STREQ(md, t.toString().c_str());
    }
}

TEST(TypeTest, testGetArgumentTypesGetReturnTypeAndGetMethodType)
{
    const char* mds[] = {"(IZBCSDFJLI;LV;Ljava/lang/Object;[I[LI;[[Ljava/lang/Object;)V",
                         "()I",
                         "()LI;",
                         "()Ljava/lang/Object;",
                         "()[I",
                         "()[LI;",
                         "()[[Ljava/lang/Object;"};
    for (const char* md : mds) {
        std::vector<Type> argTypes   = Type::getArgumentTypes(Utf8StringRef::from_cstr(md));
        Type              retType    = Type::getReturnType(Utf8StringRef::from_cstr(md));
        ca::str::Utf8StringArena arena;   // #921:注入版替换已删除的无参兜底重载
        Type methodType = Type::getMethodType(arena, retType, argTypes);
        ASSERT_EQ(Type::METHOD, methodType.getSort());
        ASSERT_STREQ(md, methodType.getDescriptor(arena).to_std_string().c_str());
        ASSERT_EQ(argTypes, methodType.getArgumentTypes());
        ASSERT_EQ(retType, methodType.getReturnType());
    }
}

TEST(TypeTest, testGetArgumentTypesInvalidMethodDescriptor)
{
    ASSERT_THROW(Type::getArgumentTypes(ZUtf8StringRef::from_static("(Ljava/lang/String")),
                 std::invalid_argument);
}

TEST(TypeTest, testGetArgumentTypesInvalidArgumentDescriptor)
{
    try {
        Type::getArgumentTypes(ZUtf8StringRef::from_static("(Ljava/lang/String;-)V"));
        FAIL() << "Expected std::invalid_argument";
    }
    catch (const std::invalid_argument& e) {
        ASSERT_STREQ("Invalid descriptor: (Ljava/lang/String;-)V", e.what());
    }
}

TEST(TypeTest, testGetReturnTypeFromDescriptor)
{
    ASSERT_EQ(Type::INT_TYPE, Type::getReturnType(ZUtf8StringRef::from_static("()I")));
    ASSERT_EQ(Type::INT_TYPE,
              Type::getReturnType(ZUtf8StringRef::from_static("(Lpkg/classMethod();)I")));
}

TEST(TypeTest, testGetTypeFromClass)
{
    ca::str::Utf8StringArena arena;   // #921:注入版替换已删除的无参重载
    ASSERT_EQ(Type::VOID_TYPE, Type::getType(ZUtf8StringRef::from_static("V")));
    ASSERT_EQ(Type::BOOLEAN_TYPE, Type::getType(ZUtf8StringRef::from_static("Z")));
    ASSERT_EQ(Type::CHAR_TYPE, Type::getType(ZUtf8StringRef::from_static("C")));
    ASSERT_EQ(Type::BYTE_TYPE, Type::getType(ZUtf8StringRef::from_static("B")));
    ASSERT_EQ(Type::SHORT_TYPE, Type::getType(ZUtf8StringRef::from_static("S")));
    ASSERT_EQ(Type::INT_TYPE, Type::getType(ZUtf8StringRef::from_static("I")));
    ASSERT_EQ(Type::FLOAT_TYPE, Type::getType(ZUtf8StringRef::from_static("F")));
    ASSERT_EQ(Type::LONG_TYPE, Type::getType(ZUtf8StringRef::from_static("J")));
    ASSERT_EQ(Type::DOUBLE_TYPE, Type::getType(ZUtf8StringRef::from_static("D")));
    ASSERT_STREQ("Ljava/lang/Object;",
                 Type::getType(ZUtf8StringRef::from_static("Ljava/lang/Object;"))
                     .getDescriptor(arena)
                     .to_std_string()
                     .c_str());
    ASSERT_STREQ("[Ljava/lang/Object;",
                 Type::getType(ZUtf8StringRef::from_static("[Ljava/lang/Object;"))
                     .getDescriptor(arena)
                     .to_std_string()
                     .c_str());
}

TEST(TypeTest, testGetTypeFromConstructor)
{
    ca::str::Utf8StringArena arena;   // #921:注入版替换已删除的无参重载
    Type type = Type::getType(ZUtf8StringRef::from_static("([BII)V"));
    ASSERT_STREQ("([BII)V", type.getDescriptor(arena).to_std_string().c_str());
}

TEST(TypeTest, testGetTypeFromMethod)
{
    ca::str::Utf8StringArena arena;   // #921:注入版替换已删除的无参重载
    Type type = Type::getType(ZUtf8StringRef::from_static("([BB)I"));
    ASSERT_STREQ("([BB)I", type.getDescriptor(arena).to_std_string().c_str());
}

TEST(TypeTest, testGetArgumentTypesFromMethod)
{
    std::vector<Type> argumentTypes = Type::getArgumentTypes(ZUtf8StringRef::from_static("([BB)I"));
    ASSERT_EQ(2u, argumentTypes.size());
    ASSERT_EQ(Type::getType(ZUtf8StringRef::from_static("[B")), argumentTypes[0]);
    ASSERT_EQ(Type::BYTE_TYPE, argumentTypes[1]);
}

TEST(TypeTest, testGetReturnTypeFromMethod)
{
    Type returnType = Type::getReturnType(ZUtf8StringRef::from_static("([BB)I"));
    ASSERT_EQ(Type::INT_TYPE, returnType);
}

TEST(TypeTest, testGetArgumentCountFromType)
{
    ASSERT_EQ(
        14,
        Type::getMethodType(ZUtf8StringRef::from_static(
                                "(IZBCSDFJLI;LV;Ljava/lang/Object;[I[LI;[[Ljava/lang/Object;)V"))
            .getArgumentCount());
}

TEST(TypeTest, testGetArgumentCountFromDescriptor)
{
    ASSERT_EQ(14,
              Type::getArgumentCount(ZUtf8StringRef::from_static(
                  "(IZBCSDFJLI;LV;Ljava/lang/Object;[I[LI;[[Ljava/lang/Object;)V")));
    ASSERT_EQ(0, Type::getArgumentCount(ZUtf8StringRef::from_static("()I")));
}

TEST(TypeTest, testGetArgumentsAndReturnSizeFromType)
{
    ASSERT_EQ(
        17 << 2,
        Type::getMethodType(ZUtf8StringRef::from_static(
                                "(IZBCSDFJLI;LV;Ljava/lang/Object;[I[LI;[[Ljava/lang/Object;)V"))
            .getArgumentsAndReturnSizes());
    ASSERT_EQ(1 << 2 | 1,
              Type::getMethodType(ZUtf8StringRef::from_static("()I")).getArgumentsAndReturnSizes());
    ASSERT_EQ(1 << 2 | 1,
              Type::getMethodType(ZUtf8StringRef::from_static("()F")).getArgumentsAndReturnSizes());
    ASSERT_EQ(1 << 2 | 2,
              Type::getMethodType(ZUtf8StringRef::from_static("()J")).getArgumentsAndReturnSizes());
    ASSERT_EQ(1 << 2 | 2,
              Type::getMethodType(ZUtf8StringRef::from_static("()D")).getArgumentsAndReturnSizes());
    ASSERT_EQ(
        1 << 2 | 1,
        Type::getMethodType(ZUtf8StringRef::from_static("()LD;")).getArgumentsAndReturnSizes());
}

TEST(TypeTest, testGetArgumentsAndReturnSizeFromDescriptor)
{
    ASSERT_EQ(17 << 2,
              Type::getArgumentsAndReturnSizes(ZUtf8StringRef::from_static(
                  "(IZBCSDFJLI;LV;Ljava/lang/Object;[I[LI;[[Ljava/lang/Object;)V")));
    ASSERT_EQ(1 << 2 | 1, Type::getArgumentsAndReturnSizes(ZUtf8StringRef::from_static("()I")));
    ASSERT_EQ(1 << 2 | 1, Type::getArgumentsAndReturnSizes(ZUtf8StringRef::from_static("()F")));
    ASSERT_EQ(1 << 2 | 2, Type::getArgumentsAndReturnSizes(ZUtf8StringRef::from_static("()J")));
    ASSERT_EQ(1 << 2 | 2, Type::getArgumentsAndReturnSizes(ZUtf8StringRef::from_static("()D")));
    ASSERT_EQ(1 << 2 | 1, Type::getArgumentsAndReturnSizes(ZUtf8StringRef::from_static("()LD;")));
}

TEST(TypeTest, testGetArgumentsAndReturnSize)
{
    Type type = Type::getType(ZUtf8StringRef::from_static(
        "(IZBCSDFJLI;LV;Ljava/lang/Object;[I[LI;[[Ljava/lang/Object;)V"));

    ASSERT_EQ(17 << 2, type.getArgumentsAndReturnSizes());
}

TEST(TypeTest, testGetSort)
{
    ASSERT_EQ(Type::ARRAY, Type::getType(ZUtf8StringRef::from_static("[LI;")).getSort());
    ASSERT_EQ(Type::ARRAY, Type::getObjectType(ZUtf8StringRef::from_static("[LI;")).getSort());
    ASSERT_EQ(Type::OBJECT, Type::getType(ZUtf8StringRef::from_static("LI;")).getSort());
    ASSERT_EQ(Type::OBJECT, Type::getObjectType(ZUtf8StringRef::from_static("I")).getSort());
}

TEST(TypeTest, testGetDimensions)
{
    ASSERT_EQ(1, Type::getType(ZUtf8StringRef::from_static("[I")).getDimensions());
    ASSERT_EQ(3, Type::getType(ZUtf8StringRef::from_static("[[[LI;")).getDimensions());
}

TEST(TypeTest, testGetElementType)
{
    ASSERT_EQ(Type::INT_TYPE, Type::getType(ZUtf8StringRef::from_static("[I")).getElementType());
    ASSERT_EQ(Type::getObjectType(ZUtf8StringRef::from_static("I")),
              Type::getType(ZUtf8StringRef::from_static("[[[LI;")).getElementType());
}

TEST(TypeTest, testGetClassName)
{
    // #921:getClassName() 无参兜底重载已删除,统一走 getClassName(arena) 注入版。
    ca::str::Utf8StringArena arena;
    ASSERT_STREQ("void", Type::VOID_TYPE.getClassName(arena).to_std_string().c_str());
    ASSERT_STREQ("boolean", Type::BOOLEAN_TYPE.getClassName(arena).to_std_string().c_str());
    ASSERT_STREQ("char", Type::CHAR_TYPE.getClassName(arena).to_std_string().c_str());
    ASSERT_STREQ("byte", Type::BYTE_TYPE.getClassName(arena).to_std_string().c_str());
    ASSERT_STREQ("short", Type::SHORT_TYPE.getClassName(arena).to_std_string().c_str());
    ASSERT_STREQ("int", Type::INT_TYPE.getClassName(arena).to_std_string().c_str());
    ASSERT_STREQ("float", Type::FLOAT_TYPE.getClassName(arena).to_std_string().c_str());
    ASSERT_STREQ("long", Type::LONG_TYPE.getClassName(arena).to_std_string().c_str());
    ASSERT_STREQ("double", Type::DOUBLE_TYPE.getClassName(arena).to_std_string().c_str());
    ASSERT_STREQ("I[]",
                 Type::getObjectType(ZUtf8StringRef::from_static("[LI;"))
                     .getClassName(arena)
                     .to_std_string()
                     .c_str());
    ASSERT_STREQ("java.lang.Object",
                 Type::getObjectType(ZUtf8StringRef::from_static("java/lang/Object"))
                     .getClassName(arena)
                     .to_std_string()
                     .c_str());
    ASSERT_STREQ("java.lang.Object",
                 Type::getType(ZUtf8StringRef::from_static("Ljava/lang/Object;"))
                     .getClassName(arena)
                     .to_std_string()
                     .c_str());
}

TEST(TypeTest, testGetInternalName)
{
    ca::str::Utf8StringArena arena;   // #921:注入版替换已删除的无参重载
    ASSERT_STREQ("[LI;",
                 Type::getObjectType(ZUtf8StringRef::from_static("[LI;"))
                     .getInternalName(arena)
                     .to_std_string()
                     .c_str());
    ASSERT_STREQ("java/lang/Object",
                 Type::getObjectType(ZUtf8StringRef::from_static("java/lang/Object"))
                     .getInternalName(arena)
                     .to_std_string()
                     .c_str());
    ASSERT_STREQ("java/lang/Object",
                 Type::getType(ZUtf8StringRef::from_static("Ljava/lang/Object;"))
                     .getInternalName(arena)
                     .to_std_string()
                     .c_str());
}

TEST(TypeTest, testGetDescriptor)
{
    ca::str::Utf8StringArena arena;   // #921:注入版替换已删除的无参重载
    ASSERT_STREQ("[LI;",
                 Type::getObjectType(ZUtf8StringRef::from_static("[LI;"))
                     .getDescriptor(arena)
                     .to_std_string()
                     .c_str());
    ASSERT_STREQ("Ljava/lang/Object;",
                 Type::getObjectType(ZUtf8StringRef::from_static("java/lang/Object"))
                     .getDescriptor(arena)
                     .to_std_string()
                     .c_str());
    ASSERT_STREQ("Ljava/lang/Object;",
                 Type::getType(ZUtf8StringRef::from_static("Ljava/lang/Object;"))
                     .getDescriptor(arena)
                     .to_std_string()
                     .c_str());
}

TEST(TypeTest, testGetMethodDescriptor)
{
    ASSERT_EQ("(IJ)V",
              Type::getMethodDescriptor(Type::VOID_TYPE, {Type::INT_TYPE, Type::LONG_TYPE}));
    ASSERT_EQ(
        "(Ljava/lang/Object;)V",
        Type::getMethodDescriptor(
            Type::VOID_TYPE, {Type::getType(ZUtf8StringRef::from_static("Ljava/lang/Object;"))}));
    ASSERT_EQ("(Ljava/lang/Object;)V",
              Type::getMethodDescriptor(
                  Type::VOID_TYPE,
                  {Type::getObjectType(ZUtf8StringRef::from_static("java/lang/Object"))}));
}

TEST(TypeTest, testGetInternalNameFromClass)
{
    ASSERT_EQ("java/lang/Object",
              Type::getInternalName(ZUtf8StringRef::from_static("java.lang.Object")));
    ca::str::Utf8StringArena arena;   // #921:注入版替换已删除的无参重载
    ASSERT_STREQ("[Ljava/lang/Object;",
                 Type::getObjectType(ZUtf8StringRef::from_static("[Ljava/lang/Object;"))
                     .getInternalName(arena)
                     .to_std_string()
                     .c_str());
}

TEST(TypeTest, testGetDescriptorFromClass)
{
    ca::str::Utf8StringArena arena;   // #921:注入版替换已删除的无参重载
    ASSERT_STREQ(
        "V",
        Type::getType(ZUtf8StringRef::from_static("V")).getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ(
        "Z",
        Type::getType(ZUtf8StringRef::from_static("Z")).getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ(
        "C",
        Type::getType(ZUtf8StringRef::from_static("C")).getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ(
        "B",
        Type::getType(ZUtf8StringRef::from_static("B")).getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ(
        "S",
        Type::getType(ZUtf8StringRef::from_static("S")).getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ(
        "I",
        Type::getType(ZUtf8StringRef::from_static("I")).getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ(
        "F",
        Type::getType(ZUtf8StringRef::from_static("F")).getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ(
        "J",
        Type::getType(ZUtf8StringRef::from_static("J")).getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ(
        "D",
        Type::getType(ZUtf8StringRef::from_static("D")).getDescriptor(arena).to_std_string().c_str());
    ASSERT_STREQ("Ljava/lang/Object;",
                 Type::getType(ZUtf8StringRef::from_static("Ljava/lang/Object;"))
                     .getDescriptor(arena)
                     .to_std_string()
                     .c_str());
    ASSERT_STREQ("[Ljava/lang/Object;",
                 Type::getType(ZUtf8StringRef::from_static("[Ljava/lang/Object;"))
                     .getDescriptor(arena)
                     .to_std_string()
                     .c_str());
    ASSERT_STREQ("[[Ljava/lang/Object;",
                 Type::getType(ZUtf8StringRef::from_static("[[Ljava/lang/Object;"))
                     .getDescriptor(arena)
                     .to_std_string()
                     .c_str());
    ASSERT_STREQ(
        "[[I",
        Type::getType(ZUtf8StringRef::from_static("[[I")).getDescriptor(arena).to_std_string().c_str());
}

TEST(TypeTest, testGetOpcode_ialoadOrStore)
{
    ASSERT_EQ(Opcodes::BALOAD, Type::BOOLEAN_TYPE.getOpcode(Opcodes::IALOAD));
    ASSERT_EQ(Opcodes::BALOAD, Type::BYTE_TYPE.getOpcode(Opcodes::IALOAD));
    ASSERT_EQ(Opcodes::CALOAD, Type::CHAR_TYPE.getOpcode(Opcodes::IALOAD));
    ASSERT_EQ(Opcodes::SALOAD, Type::SHORT_TYPE.getOpcode(Opcodes::IALOAD));
    ASSERT_EQ(Opcodes::IALOAD, Type::INT_TYPE.getOpcode(Opcodes::IALOAD));
    ASSERT_EQ(Opcodes::FALOAD, Type::FLOAT_TYPE.getOpcode(Opcodes::IALOAD));
    ASSERT_EQ(Opcodes::LALOAD, Type::LONG_TYPE.getOpcode(Opcodes::IALOAD));
    ASSERT_EQ(Opcodes::DALOAD, Type::DOUBLE_TYPE.getOpcode(Opcodes::IALOAD));
    ASSERT_EQ(Opcodes::AALOAD,
              Type::getType(ZUtf8StringRef::from_static("Ljava/lang/Object;"))
                  .getOpcode(Opcodes::IALOAD));
    ASSERT_EQ(Opcodes::AALOAD,
              Type::getObjectType(ZUtf8StringRef::from_static("java/lang/Object"))
                  .getOpcode(Opcodes::IALOAD));
    ASSERT_EQ(Opcodes::AASTORE,
              Type::getType(ZUtf8StringRef::from_static("Ljava/lang/Object;"))
                  .getOpcode(Opcodes::IASTORE));
    ASSERT_EQ(Opcodes::AASTORE,
              Type::getObjectType(ZUtf8StringRef::from_static("java/lang/Object"))
                  .getOpcode(Opcodes::IASTORE));
    ASSERT_EQ(Opcodes::AASTORE,
              Type::getType(ZUtf8StringRef::from_static("[I")).getOpcode(Opcodes::IASTORE));
}

TEST(TypeTest, testGetOpcode_return)
{
    ASSERT_EQ(Opcodes::RETURN, Type::VOID_TYPE.getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::IRETURN, Type::BOOLEAN_TYPE.getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::IRETURN, Type::BYTE_TYPE.getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::IRETURN, Type::CHAR_TYPE.getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::IRETURN, Type::SHORT_TYPE.getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::IRETURN, Type::INT_TYPE.getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::FRETURN, Type::FLOAT_TYPE.getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::LRETURN, Type::LONG_TYPE.getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::DRETURN, Type::DOUBLE_TYPE.getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::ARETURN,
              Type::getType(ZUtf8StringRef::from_static("Ljava/lang/Object;"))
                  .getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::ARETURN,
              Type::getObjectType(ZUtf8StringRef::from_static("java/lang/Object"))
                  .getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::ARETURN,
              Type::getType(ZUtf8StringRef::from_static("Ljava/lang/Object;"))
                  .getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::ARETURN,
              Type::getObjectType(ZUtf8StringRef::from_static("java/lang/Object"))
                  .getOpcode(Opcodes::IRETURN));
    ASSERT_EQ(Opcodes::ARETURN,
              Type::getType(ZUtf8StringRef::from_static("[I")).getOpcode(Opcodes::IRETURN));
}

TEST(TypeTest, testGetOpcode_iadd)
{
    ASSERT_EQ(Opcodes::IADD, Type::BOOLEAN_TYPE.getOpcode(Opcodes::IADD));
    ASSERT_EQ(Opcodes::IADD, Type::BYTE_TYPE.getOpcode(Opcodes::IADD));
    ASSERT_EQ(Opcodes::IADD, Type::CHAR_TYPE.getOpcode(Opcodes::IADD));
    ASSERT_EQ(Opcodes::IADD, Type::SHORT_TYPE.getOpcode(Opcodes::IADD));
    ASSERT_EQ(Opcodes::IADD, Type::INT_TYPE.getOpcode(Opcodes::IADD));
    ASSERT_EQ(Opcodes::FADD, Type::FLOAT_TYPE.getOpcode(Opcodes::IADD));
    ASSERT_EQ(Opcodes::LADD, Type::LONG_TYPE.getOpcode(Opcodes::IADD));
    ASSERT_EQ(Opcodes::DADD, Type::DOUBLE_TYPE.getOpcode(Opcodes::IADD));
}

TEST(TypeTest, testGetOpcode_unsupported)
{
    ASSERT_THROW(Type::VOID_TYPE.getOpcode(Opcodes::IADD), std::invalid_argument);
    ASSERT_THROW(Type::VOID_TYPE.getOpcode(Opcodes::ILOAD), std::invalid_argument);
    ASSERT_THROW(Type::VOID_TYPE.getOpcode(Opcodes::IALOAD), std::invalid_argument);
    ASSERT_THROW(Type::getType(ZUtf8StringRef::from_static("LI;")).getOpcode(Opcodes::IADD),
                 std::invalid_argument);
    ASSERT_THROW(Type::getType(ZUtf8StringRef::from_static("[I")).getOpcode(Opcodes::IADD),
                 std::invalid_argument);
    ASSERT_THROW(Type::getObjectType(ZUtf8StringRef::from_static("I")).getOpcode(Opcodes::IADD),
                 std::invalid_argument);
    ASSERT_THROW(Type::getMethodType(ZUtf8StringRef::from_static("()V")).getOpcode(Opcodes::IADD),
                 std::invalid_argument);
    ASSERT_THROW(Type::getMethodType(ZUtf8StringRef::from_static("()V")).getOpcode(Opcodes::IALOAD),
                 std::invalid_argument);
}

TEST(TypeTest, testEquals)
{
    Type objectType                = Type::getObjectType(ZUtf8StringRef::from_static("I"));
    bool equalsDifferentTypeSort   = objectType == Type::INT_TYPE;
    bool equalsDifferentObjectType = Type::getObjectType(ZUtf8StringRef::from_static("I")) ==
                                     Type::getObjectType(ZUtf8StringRef::from_static("HI"));
    bool equalsOtherDifferentObjectType = Type::getObjectType(ZUtf8StringRef::from_static("I")) ==
                                          Type::getObjectType(ZUtf8StringRef::from_static("J"));
    bool equalsSameType = Type::getObjectType(ZUtf8StringRef::from_static("I")) ==
                          Type::getType(ZUtf8StringRef::from_static("LI;"));
    bool equalsSameObjectType = Type::getType(ZUtf8StringRef::from_static("LI;")) ==
                                Type::getObjectType(ZUtf8StringRef::from_static("I"));
    bool equalsSamePrimitiveType =
        Type::INT_TYPE == Type::getType(ZUtf8StringRef::from_static("I"));
    ASSERT_FALSE(equalsDifferentTypeSort);
    ASSERT_FALSE(equalsDifferentObjectType);
    ASSERT_FALSE(equalsOtherDifferentObjectType);
    ASSERT_TRUE(equalsSameType);
    ASSERT_TRUE(equalsSameObjectType);
    ASSERT_TRUE(equalsSamePrimitiveType);
}

TEST(TypeTest, testHashcode)
{
    ASSERT_EQ(Type::getType(ZUtf8StringRef::from_static("Ljava/lang/Object;")),
              Type::getObjectType(ZUtf8StringRef::from_static("java/lang/Object")));
    ASSERT_NE(Type::INT_TYPE, Type::getObjectType(ZUtf8StringRef::from_static("I")));
}
