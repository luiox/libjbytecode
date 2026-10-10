#include <gtest/gtest.h>
#include <set>
#include <vector>
#include <string>
#include <algorithm>
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/constants.hpp"

using namespace jbytecode::core;

TEST(ConstantsTest, testAsmVersions)
{
    std::vector<int> vals = {Opcodes::ASM4,
                             Opcodes::ASM5,
                             Opcodes::ASM6,
                             Opcodes::ASM7,
                             Opcodes::ASM8,
                             Opcodes::ASM9,
                             Opcodes::ASM10_EXPERIMENTAL};
    std::set<int>    s(vals.begin(), vals.end());
    ASSERT_EQ(vals.size(), s.size());
}

TEST(ConstantsTest, testClassVersions)
{
    std::vector<int> vals = {Opcodes::V_PREVIEW, Opcodes::V1_1, Opcodes::V1_2, Opcodes::V1_3,
                             Opcodes::V1_4,      Opcodes::V1_5, Opcodes::V1_6, Opcodes::V1_7,
                             Opcodes::V1_8,      Opcodes::V9,   Opcodes::V10,  Opcodes::V11,
                             Opcodes::V12,       Opcodes::V13,  Opcodes::V14,  Opcodes::V15,
                             Opcodes::V16,       Opcodes::V17,  Opcodes::V18,  Opcodes::V19,
                             Opcodes::V20,       Opcodes::V21,  Opcodes::V22,  Opcodes::V23,
                             Opcodes::V24,       Opcodes::V25,  Opcodes::V26,  Opcodes::V27};
    std::set<int>    s(vals.begin(), vals.end());
    ASSERT_EQ(vals.size(), s.size());
}

TEST(ConstantsTest, testAccessFlags)
{
    struct
    {
        const char* n;
        int         v;
    } flags[] = {{"ACC_PUBLIC", Opcodes::ACC_PUBLIC},
                 {"ACC_PRIVATE", Opcodes::ACC_PRIVATE},
                 {"ACC_PROTECTED", Opcodes::ACC_PROTECTED},
                 {"ACC_STATIC", Opcodes::ACC_STATIC},
                 {"ACC_FINAL", Opcodes::ACC_FINAL},
                 {"ACC_SUPER", Opcodes::ACC_SUPER},
                 {"ACC_SYNCHRONIZED", Opcodes::ACC_SYNCHRONIZED},
                 {"ACC_OPEN", Opcodes::ACC_OPEN},
                 {"ACC_TRANSITIVE", Opcodes::ACC_TRANSITIVE},
                 {"ACC_VOLATILE", Opcodes::ACC_VOLATILE},
                 {"ACC_BRIDGE", Opcodes::ACC_BRIDGE},
                 {"ACC_STATIC_PHASE", Opcodes::ACC_STATIC_PHASE},
                 {"ACC_VARARGS", Opcodes::ACC_VARARGS},
                 {"ACC_TRANSIENT", Opcodes::ACC_TRANSIENT},
                 {"ACC_NATIVE", Opcodes::ACC_NATIVE},
                 {"ACC_INTERFACE", Opcodes::ACC_INTERFACE},
                 {"ACC_ABSTRACT", Opcodes::ACC_ABSTRACT},
                 {"ACC_STRICT", Opcodes::ACC_STRICT},
                 {"ACC_SYNTHETIC", Opcodes::ACC_SYNTHETIC},
                 {"ACC_ANNOTATION", Opcodes::ACC_ANNOTATION},
                 {"ACC_ENUM", Opcodes::ACC_ENUM},
                 {"ACC_MANDATED", Opcodes::ACC_MANDATED},
                 {"ACC_MODULE", Opcodes::ACC_MODULE}};
    for (auto& f : flags) {
        ASSERT_EQ(0, f.v & ~0xFFFF) << f.n;
        bool singleBit = (f.v != 0) && ((f.v & (f.v - 1)) == 0);
        ASSERT_TRUE(singleBit) << f.n << " is not a single bit";
    }
}

TEST(ConstantsTest, testAsmAccessFlags)
{
    struct
    {
        const char* n;
        int         v;
    } flags[] = {{"ACC_SEALED", Opcodes::ACC_SEALED},
                 {"ACC_RECORD", Opcodes::ACC_RECORD},
                 {"ACC_DEPRECATED", Opcodes::ACC_DEPRECATED},
                 {"ACC_CONSTRUCTOR", Constants::ACC_CONSTRUCTOR}};
    for (auto& f : flags) {
        ASSERT_EQ(0, f.v & 0xFFFF) << f.n << " overlaps standard flags";
        bool singleBit = (f.v != 0) && ((f.v & (f.v - 1)) == 0);
        ASSERT_TRUE(singleBit) << f.n << " is not a single bit";
    }
}

TEST(ConstantsTest, testNewArrayTypes)
{
    int           arr[] = {Opcodes::T_BOOLEAN,
                           Opcodes::T_CHAR,
                           Opcodes::T_FLOAT,
                           Opcodes::T_DOUBLE,
                           Opcodes::T_BYTE,
                           Opcodes::T_SHORT,
                           Opcodes::T_INT,
                           Opcodes::T_LONG};
    std::set<int> s(arr, arr + 8);
    ASSERT_EQ(8u, s.size());
    for (int t : s) ASSERT_EQ(0, t & ~0xFF);
}

TEST(ConstantsTest, testReferenceKinds)
{
    int           arr[] = {Opcodes::H_GETFIELD,
                           Opcodes::H_GETSTATIC,
                           Opcodes::H_PUTFIELD,
                           Opcodes::H_PUTSTATIC,
                           Opcodes::H_INVOKEVIRTUAL,
                           Opcodes::H_INVOKESTATIC,
                           Opcodes::H_INVOKESPECIAL,
                           Opcodes::H_NEWINVOKESPECIAL,
                           Opcodes::H_INVOKEINTERFACE};
    std::set<int> s(arr, arr + 9);
    ASSERT_EQ(9u, s.size());
    for (int t : s) ASSERT_EQ(0, t & ~0xFF);
}

TEST(ConstantsTest, testFrameTypes)
{
    // Original ASM ConstantsTest groups F_INSERT (Constants) with F_NEW..F_SAME1 (Opcodes)
    int           arr[] = {Opcodes::F_NEW,
                           Opcodes::F_FULL,
                           Opcodes::F_APPEND,
                           Opcodes::F_CHOP,
                           Opcodes::F_SAME,
                           Opcodes::F_SAME1,
                           Constants::F_INSERT};
    std::set<int> s(arr, arr + 7);
    ASSERT_EQ(7u, s.size());
}

TEST(ConstantsTest, testVerificationTypeInfoTags)
{
    // Original ASM Opcodes.java uses NULL (C++ ITEM_NULL avoids macro conflict)
    int           arr[] = {Opcodes::TOP,
                           Opcodes::INTEGER,
                           Opcodes::FLOAT,
                           Opcodes::DOUBLE,
                           Opcodes::LONG,
                           Opcodes::ITEM_NULL,
                           Opcodes::UNINITIALIZED_THIS};
    std::set<int> s(arr, arr + 7);
    ASSERT_EQ(7u, s.size());
    for (int t : s) ASSERT_EQ(0, t & ~0xFF);
}

TEST(ConstantsTest, testOpcodes)
{
    // Original ASM ConstantsTest collects opcodes from BOTH Opcodes.class and Constants.class
    // via reflection. Here we hardcode all opcodes. Constants.java has: LDC_W, LDC2_W,
    // ILOAD_0..ALOAD_3, ISTORE_0..ASTORE_3, WIDE, GOTO_W, JSR_W.
    int           arr[] = {Opcodes::NOP,
                           Opcodes::ACONST_NULL,
                           Opcodes::ICONST_M1,
                           Opcodes::ICONST_0,
                           Opcodes::ICONST_1,
                           Opcodes::ICONST_2,
                           Opcodes::ICONST_3,
                           Opcodes::ICONST_4,
                           Opcodes::ICONST_5,
                           Opcodes::LCONST_0,
                           Opcodes::LCONST_1,
                           Opcodes::FCONST_0,
                           Opcodes::FCONST_1,
                           Opcodes::FCONST_2,
                           Opcodes::DCONST_0,
                           Opcodes::DCONST_1,
                           Opcodes::BIPUSH,
                           Opcodes::SIPUSH,
                           Opcodes::LDC,
                           Constants::LDC_W,
                           Constants::LDC2_W,
                           Opcodes::ILOAD,
                           Opcodes::LLOAD,
                           Opcodes::FLOAD,
                           Opcodes::DLOAD,
                           Opcodes::ALOAD,
                           Constants::ILOAD_0,
                           Constants::ILOAD_1,
                           Constants::ILOAD_2,
                           Constants::ILOAD_3,
                           Constants::LLOAD_0,
                           Constants::LLOAD_1,
                           Constants::LLOAD_2,
                           Constants::LLOAD_3,
                           Constants::FLOAD_0,
                           Constants::FLOAD_1,
                           Constants::FLOAD_2,
                           Constants::FLOAD_3,
                           Constants::DLOAD_0,
                           Constants::DLOAD_1,
                           Constants::DLOAD_2,
                           Constants::DLOAD_3,
                           Constants::ALOAD_0,
                           Constants::ALOAD_1,
                           Constants::ALOAD_2,
                           Constants::ALOAD_3,
                           Opcodes::IALOAD,
                           Opcodes::LALOAD,
                           Opcodes::FALOAD,
                           Opcodes::DALOAD,
                           Opcodes::AALOAD,
                           Opcodes::BALOAD,
                           Opcodes::CALOAD,
                           Opcodes::SALOAD,
                           Opcodes::ISTORE,
                           Opcodes::LSTORE,
                           Opcodes::FSTORE,
                           Opcodes::DSTORE,
                           Opcodes::ASTORE,
                           Constants::ISTORE_0,
                           Constants::ISTORE_1,
                           Constants::ISTORE_2,
                           Constants::ISTORE_3,
                           Constants::LSTORE_0,
                           Constants::LSTORE_1,
                           Constants::LSTORE_2,
                           Constants::LSTORE_3,
                           Constants::FSTORE_0,
                           Constants::FSTORE_1,
                           Constants::FSTORE_2,
                           Constants::FSTORE_3,
                           Constants::DSTORE_0,
                           Constants::DSTORE_1,
                           Constants::DSTORE_2,
                           Constants::DSTORE_3,
                           Constants::ASTORE_0,
                           Constants::ASTORE_1,
                           Constants::ASTORE_2,
                           Constants::ASTORE_3,
                           Opcodes::IASTORE,
                           Opcodes::LASTORE,
                           Opcodes::FASTORE,
                           Opcodes::DASTORE,
                           Opcodes::AASTORE,
                           Opcodes::BASTORE,
                           Opcodes::CASTORE,
                           Opcodes::SASTORE,
                           Opcodes::POP,
                           Opcodes::POP2,
                           Opcodes::DUP,
                           Opcodes::DUP_X1,
                           Opcodes::DUP_X2,
                           Opcodes::DUP2,
                           Opcodes::DUP2_X1,
                           Opcodes::DUP2_X2,
                           Opcodes::SWAP,
                           Opcodes::IADD,
                           Opcodes::LADD,
                           Opcodes::FADD,
                           Opcodes::DADD,
                           Opcodes::ISUB,
                           Opcodes::LSUB,
                           Opcodes::FSUB,
                           Opcodes::DSUB,
                           Opcodes::IMUL,
                           Opcodes::LMUL,
                           Opcodes::FMUL,
                           Opcodes::DMUL,
                           Opcodes::IDIV,
                           Opcodes::LDIV,
                           Opcodes::FDIV,
                           Opcodes::DDIV,
                           Opcodes::IREM,
                           Opcodes::LREM,
                           Opcodes::FREM,
                           Opcodes::DREM,
                           Opcodes::INEG,
                           Opcodes::LNEG,
                           Opcodes::FNEG,
                           Opcodes::DNEG,
                           Opcodes::ISHL,
                           Opcodes::LSHL,
                           Opcodes::ISHR,
                           Opcodes::LSHR,
                           Opcodes::IUSHR,
                           Opcodes::LUSHR,
                           Opcodes::IAND,
                           Opcodes::LAND,
                           Opcodes::IOR,
                           Opcodes::LOR,
                           Opcodes::IXOR,
                           Opcodes::LXOR,
                           Opcodes::IINC,
                           Opcodes::I2L,
                           Opcodes::I2F,
                           Opcodes::I2D,
                           Opcodes::L2I,
                           Opcodes::L2F,
                           Opcodes::L2D,
                           Opcodes::F2I,
                           Opcodes::F2L,
                           Opcodes::F2D,
                           Opcodes::D2I,
                           Opcodes::D2L,
                           Opcodes::D2F,
                           Opcodes::I2B,
                           Opcodes::I2C,
                           Opcodes::I2S,
                           Opcodes::LCMP,
                           Opcodes::FCMPL,
                           Opcodes::FCMPG,
                           Opcodes::DCMPL,
                           Opcodes::DCMPG,
                           Opcodes::IFEQ,
                           Opcodes::IFNE,
                           Opcodes::IFLT,
                           Opcodes::IFGE,
                           Opcodes::IFGT,
                           Opcodes::IFLE,
                           Opcodes::IF_ICMPEQ,
                           Opcodes::IF_ICMPNE,
                           Opcodes::IF_ICMPLT,
                           Opcodes::IF_ICMPGE,
                           Opcodes::IF_ICMPGT,
                           Opcodes::IF_ICMPLE,
                           Opcodes::IF_ACMPEQ,
                           Opcodes::IF_ACMPNE,
                           Opcodes::GOTO,
                           Opcodes::JSR,
                           Opcodes::RET,
                           Opcodes::TABLESWITCH,
                           Opcodes::LOOKUPSWITCH,
                           Opcodes::IRETURN,
                           Opcodes::LRETURN,
                           Opcodes::FRETURN,
                           Opcodes::DRETURN,
                           Opcodes::ARETURN,
                           Opcodes::RETURN,
                           Opcodes::GETSTATIC,
                           Opcodes::PUTSTATIC,
                           Opcodes::GETFIELD,
                           Opcodes::PUTFIELD,
                           Opcodes::INVOKEVIRTUAL,
                           Opcodes::INVOKESPECIAL,
                           Opcodes::INVOKESTATIC,
                           Opcodes::INVOKEINTERFACE,
                           Opcodes::INVOKEDYNAMIC,
                           Opcodes::NEW,
                           Opcodes::NEWARRAY,
                           Opcodes::ANEWARRAY,
                           Opcodes::ARRAYLENGTH,
                           Opcodes::ATHROW,
                           Opcodes::CHECKCAST,
                           Opcodes::INSTANCEOF,
                           Opcodes::MONITORENTER,
                           Opcodes::MONITOREXIT,
                           Constants::WIDE,
                           Opcodes::MULTIANEWARRAY,
                           Opcodes::IFNULL,
                           Opcodes::IFNONNULL,
                           Constants::GOTO_W,
                           Constants::JSR_W,
                           Constants::ASM_IFEQ,
                           Constants::ASM_IFNE,
                           Constants::ASM_IFLT,
                           Constants::ASM_IFGE,
                           Constants::ASM_IFGT,
                           Constants::ASM_IFLE,
                           Constants::ASM_IF_ICMPEQ,
                           Constants::ASM_IF_ICMPNE,
                           Constants::ASM_IF_ICMPLT,
                           Constants::ASM_IF_ICMPGE,
                           Constants::ASM_IF_ICMPGT,
                           Constants::ASM_IF_ICMPLE,
                           Constants::ASM_IF_ACMPEQ,
                           Constants::ASM_IF_ACMPNE,
                           Constants::ASM_GOTO,
                           Constants::ASM_JSR,
                           Constants::ASM_IFNULL,
                           Constants::ASM_IFNONNULL,
                           Constants::ASM_GOTO_W};
    std::set<int> s(arr, arr + sizeof(arr) / sizeof(arr[0]));
    ASSERT_EQ(sizeof(arr) / sizeof(arr[0]), s.size());
    for (int op : s) {
        ASSERT_EQ(0, op & ~0xFF);
        ASSERT_EQ(0, op & Opcodes::SOURCE_MASK);
    }
}

TEST(ConstantsTest, testIsWhitelisted)
{
    ASSERT_FALSE(Constants::isWhitelisted("org/jacoco/core/internal/flow/ClassProbesVisitor"));
    ASSERT_FALSE(Constants::isWhitelisted("org/objectweb/asm/ClassWriter"));
    ASSERT_FALSE(Constants::isWhitelisted("org/objectweb/asm/util/CheckClassVisitor"));
    ASSERT_FALSE(Constants::isWhitelisted("org/objectweb/asm/ClassWriterTest"));
    ASSERT_TRUE(Constants::isWhitelisted("org/objectweb/asm/ClassWriterTest$DeadCodeInserter"));
    ASSERT_TRUE(Constants::isWhitelisted("org/objectweb/asm/util/TraceClassVisitor"));
    ASSERT_TRUE(Constants::isWhitelisted("org/objectweb/asm/util/CheckClassAdapter"));
}

TEST(ConstantsTest, testCheckIsPreview_nullStream)
{
    ASSERT_THROW(Constants::checkIsPreview(nullptr, 0), std::runtime_error);
}

TEST(ConstantsTest, testCheckIsPreview_invalidStream)
{
    unsigned char invalidData[4] = {0, 0, 0, 0};
    ASSERT_THROW(Constants::checkIsPreview(invalidData, 4), std::runtime_error);
}

TEST(ConstantsTest, testCheckIsPreview_nonPreviewClass)
{
    unsigned char nonPreviewData[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    ASSERT_THROW(Constants::checkIsPreview(nonPreviewData, 8), std::runtime_error);
}

TEST(ConstantsTest, testCheckIsPreview_previewClass)
{
    unsigned char previewData[6] = {0, 0, 0, 0, 0xFF, 0xFF};
    ASSERT_NO_THROW(Constants::checkIsPreview(previewData, 6));
}
