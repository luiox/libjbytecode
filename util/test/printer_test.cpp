#include <gtest/gtest.h>
#include "test_utils.hpp"
#include "jbytecode/util/printer.hpp"

TEST(PrinterTest, testOpcodeNames)
{
    EXPECT_STREQ("NOP", jbytecode::util::Printer::OPCODES[Opcodes::NOP]);
    EXPECT_STREQ("RETURN", jbytecode::util::Printer::OPCODES[Opcodes::RETURN]);
    EXPECT_STREQ("INVOKEVIRTUAL", jbytecode::util::Printer::OPCODES[Opcodes::INVOKEVIRTUAL]);
}
