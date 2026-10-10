#include <gtest/gtest.h>
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/writers.hpp"

using namespace jbytecode::core;
using namespace ca::str;

TEST(LabelTest, testGetOffset)
{
    Utf8StringArena arena;
    ClassWriter     classWriter(arena, 0);
    MethodVisitor*  methodVisitor = classWriter.visitMethod(Opcodes::ACC_PUBLIC,
                                                           ZUtf8StringRef::from_static("m"),
                                                           ZUtf8StringRef::from_static("()V"),
                                                           ZUtf8StringRef::from_static(""),
                                                            {});
    methodVisitor->visitCode();
    methodVisitor->visitVarInsn(Opcodes::ALOAD, 0);
    Label label;
    methodVisitor->visitLabel(&label);
    methodVisitor->visitMaxs(1, 1);
    methodVisitor->visitEnd();

    ASSERT_EQ(1, label.getOffset());
}

TEST(LabelTest, testGetOffset_illegalState)
{
    try {
        Label().getOffset();
        FAIL() << "Expected std::logic_error";
    }
    catch (const std::logic_error& e) {
        ASSERT_STREQ("Label offset position has not been resolved yet", e.what());
    }
}

TEST(LabelTest, testToString)
{
    std::string value = Label().toString();

    ASSERT_FALSE(value.empty());
    ASSERT_EQ('L', value[0]);
}
