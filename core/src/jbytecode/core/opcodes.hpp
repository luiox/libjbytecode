#pragma once

#include "libca/core/datatype.hpp"
#include "jbytecode/core/frame.hpp"

namespace jbytecode::core {

///  @brief ASM-level opcode and access-flag constants.
///
///  These are the logical opcodes used when calling MethodVisitor
///  (e.g. visitInsn(Opcodes::ALOAD)). The framework auto-expands
///  shorthand forms (ILOAD_0 → ALOAD_3, LDC_W, GOTO_W) when writing.
/// For raw classfile byte values see Constants.
class Opcodes
{
public:
    // ASM API versions
    static constexpr ca::i32 ASM4 = 4 << 16 | 0 << 8;
    static constexpr ca::i32 ASM5 = 5 << 16 | 0 << 8;
    static constexpr ca::i32 ASM6 = 6 << 16 | 0 << 8;
    static constexpr ca::i32 ASM7 = 7 << 16 | 0 << 8;
    static constexpr ca::i32 ASM8 = 8 << 16 | 0 << 8;
    static constexpr ca::i32 ASM9 = 9 << 16 | 0 << 8;

    // 实验性版本 （原 Java 中的 @Deprecated 标记，C++ 中我们可以用注释说明）
    static constexpr ca::i32 ASM10_EXPERIMENTAL = 1 << 24 | 10 << 16 | 0 << 8;

    // 内部标志
    static constexpr ca::i32 SOURCE_DEPRECATED = 0x100;
    static constexpr ca::i32 SOURCE_MASK       = SOURCE_DEPRECATED;

    // Java ClassFile versions （小版本在高16位，大版本在低16位）
    static constexpr ca::i32 V1_1 = 3 << 16 | 45;
    static constexpr ca::i32 V1_2 = 0 << 16 | 46;
    static constexpr ca::i32 V1_3 = 0 << 16 | 47;
    static constexpr ca::i32 V1_4 = 0 << 16 | 48;
    static constexpr ca::i32 V1_5 = 0 << 16 | 49;
    static constexpr ca::i32 V1_6 = 0 << 16 | 50;
    static constexpr ca::i32 V1_7 = 0 << 16 | 51;
    static constexpr ca::i32 V1_8 = 0 << 16 | 52;
    static constexpr ca::i32 V9   = 0 << 16 | 53;
    static constexpr ca::i32 V10  = 0 << 16 | 54;
    static constexpr ca::i32 V11  = 0 << 16 | 55;
    static constexpr ca::i32 V12  = 0 << 16 | 56;
    static constexpr ca::i32 V13  = 0 << 16 | 57;
    static constexpr ca::i32 V14  = 0 << 16 | 58;
    static constexpr ca::i32 V15  = 0 << 16 | 59;
    static constexpr ca::i32 V16  = 0 << 16 | 60;
    static constexpr ca::i32 V17  = 0 << 16 | 61;
    static constexpr ca::i32 V18  = 0 << 16 | 62;
    static constexpr ca::i32 V19  = 0 << 16 | 63;
    static constexpr ca::i32 V20  = 0 << 16 | 64;
    static constexpr ca::i32 V21  = 0 << 16 | 65;
    static constexpr ca::i32 V22  = 0 << 16 | 66;
    static constexpr ca::i32 V23  = 0 << 16 | 67;
    static constexpr ca::i32 V24  = 0 << 16 | 68;
    static constexpr ca::i32 V25  = 0 << 16 | 69;
    static constexpr ca::i32 V26  = 0 << 16 | 70;
    static constexpr ca::i32 V27  = 0 << 16 | 71;

    static constexpr ca::i32 V_PREVIEW = 0xFFFF0000;

    // Access flags
    static constexpr ca::i32 ACC_PUBLIC       = 0x0001;
    static constexpr ca::i32 ACC_PRIVATE      = 0x0002;
    static constexpr ca::i32 ACC_PROTECTED    = 0x0004;
    static constexpr ca::i32 ACC_STATIC       = 0x0008;
    static constexpr ca::i32 ACC_FINAL        = 0x0010;
    static constexpr ca::i32 ACC_SUPER        = 0x0020;
    static constexpr ca::i32 ACC_SYNCHRONIZED = 0x0020;
    static constexpr ca::i32 ACC_OPEN         = 0x0020;
    static constexpr ca::i32 ACC_TRANSITIVE   = 0x0020;
    static constexpr ca::i32 ACC_VOLATILE     = 0x0040;
    static constexpr ca::i32 ACC_BRIDGE       = 0x0040;
    static constexpr ca::i32 ACC_STATIC_PHASE = 0x0040;
    static constexpr ca::i32 ACC_VARARGS      = 0x0080;
    static constexpr ca::i32 ACC_TRANSIENT    = 0x0080;
    static constexpr ca::i32 ACC_NATIVE       = 0x0100;
    static constexpr ca::i32 ACC_INTERFACE    = 0x0200;
    static constexpr ca::i32 ACC_ABSTRACT     = 0x0400;
    static constexpr ca::i32 ACC_STRICT       = 0x0800;
    static constexpr ca::i32 ACC_SYNTHETIC    = 0x1000;
    static constexpr ca::i32 ACC_ANNOTATION   = 0x2000;
    static constexpr ca::i32 ACC_ENUM         = 0x4000;
    static constexpr ca::i32 ACC_MANDATED     = 0x8000;
    static constexpr ca::i32 ACC_MODULE       = 0x8000;

    // ASM specific access flags (bits 16+)
    static constexpr ca::i32 ACC_RECORD     = 0x10000;
    static constexpr ca::i32 ACC_DEPRECATED = 0x20000;
    static constexpr ca::i32 ACC_SEALED     = 0x40000;

    // NEWARRAY type operands
    static constexpr ca::i32 T_BOOLEAN = 4;
    static constexpr ca::i32 T_CHAR    = 5;
    static constexpr ca::i32 T_FLOAT   = 6;
    static constexpr ca::i32 T_DOUBLE  = 7;
    static constexpr ca::i32 T_BYTE    = 8;
    static constexpr ca::i32 T_SHORT   = 9;
    static constexpr ca::i32 T_INT     = 10;
    static constexpr ca::i32 T_LONG    = 11;

    // MethodHandle reference kinds
    static constexpr ca::i32 H_GETFIELD         = 1;
    static constexpr ca::i32 H_GETSTATIC        = 2;
    static constexpr ca::i32 H_PUTFIELD         = 3;
    static constexpr ca::i32 H_PUTSTATIC        = 4;
    static constexpr ca::i32 H_INVOKEVIRTUAL    = 5;
    static constexpr ca::i32 H_INVOKESTATIC     = 6;
    static constexpr ca::i32 H_INVOKESPECIAL    = 7;
    static constexpr ca::i32 H_NEWINVOKESPECIAL = 8;
    static constexpr ca::i32 H_INVOKEINTERFACE  = 9;

    // Stack map frame types
    static constexpr ca::i32 F_NEW    = -1;
    static constexpr ca::i32 F_FULL   = 0;
    static constexpr ca::i32 F_APPEND = 1;
    static constexpr ca::i32 F_CHOP   = 2;
    static constexpr ca::i32 F_SAME   = 3;
    static constexpr ca::i32 F_SAME1  = 4;

    // Standard stack map frame element types, used in {@link ClassVisitor#visitFrame}.
    // Original ASM:  Opcodes.java defines these as Integer boxing Frame.ITEM_* from Frame.java.
    // C++ adaptation: static constexpr ca::i32 with values 0-6.
    // C++ name ITEM_NULL instead of NULL (NULL is a C++ macro).
    static constexpr ca::i32 TOP                = Frame::ITEM_TOP;
    static constexpr ca::i32 INTEGER            = Frame::ITEM_INTEGER;
    static constexpr ca::i32 FLOAT              = Frame::ITEM_FLOAT;
    static constexpr ca::i32 DOUBLE             = Frame::ITEM_DOUBLE;
    static constexpr ca::i32 LONG               = Frame::ITEM_LONG;
    static constexpr ca::i32 ITEM_NULL          = Frame::ITEM_NULL;
    static constexpr ca::i32 UNINITIALIZED_THIS = Frame::ITEM_UNINITIALIZED_THIS;

    // JVM opcodes
    static constexpr ca::i32 NOP         = 0;
    static constexpr ca::i32 ACONST_NULL = 1;
    static constexpr ca::i32 ICONST_M1   = 2;
    static constexpr ca::i32 ICONST_0    = 3;
    static constexpr ca::i32 ICONST_1    = 4;
    static constexpr ca::i32 ICONST_2    = 5;
    static constexpr ca::i32 ICONST_3    = 6;
    static constexpr ca::i32 ICONST_4    = 7;
    static constexpr ca::i32 ICONST_5    = 8;
    static constexpr ca::i32 LCONST_0    = 9;
    static constexpr ca::i32 LCONST_1    = 10;
    static constexpr ca::i32 FCONST_0    = 11;
    static constexpr ca::i32 FCONST_1    = 12;
    static constexpr ca::i32 FCONST_2    = 13;
    static constexpr ca::i32 DCONST_0    = 14;
    static constexpr ca::i32 DCONST_1    = 15;
    static constexpr ca::i32 BIPUSH      = 16;
    static constexpr ca::i32 SIPUSH      = 17;
    static constexpr ca::i32 LDC         = 18;
    // LDC_W = 19, LDC2_W = 20: defined in Constants (not in original ASM Opcodes.java)
    static constexpr ca::i32 ILOAD = 21;
    static constexpr ca::i32 LLOAD = 22;
    static constexpr ca::i32 FLOAD = 23;
    static constexpr ca::i32 DLOAD = 24;
    static constexpr ca::i32 ALOAD = 25;
    // ILOAD_0..ALOAD_3 (26-45): defined in Constants (not in original ASM Opcodes.java)
    static constexpr ca::i32 IALOAD = 46;
    static constexpr ca::i32 LALOAD = 47;
    static constexpr ca::i32 FALOAD = 48;
    static constexpr ca::i32 DALOAD = 49;
    static constexpr ca::i32 AALOAD = 50;
    static constexpr ca::i32 BALOAD = 51;
    static constexpr ca::i32 CALOAD = 52;
    static constexpr ca::i32 SALOAD = 53;
    static constexpr ca::i32 ISTORE = 54;
    static constexpr ca::i32 LSTORE = 55;
    static constexpr ca::i32 FSTORE = 56;
    static constexpr ca::i32 DSTORE = 57;
    static constexpr ca::i32 ASTORE = 58;
    // ISTORE_0..ASTORE_3 (59-78): defined in Constants (not in original ASM Opcodes.java)
    static constexpr ca::i32 IASTORE   = 79;
    static constexpr ca::i32 LASTORE   = 80;
    static constexpr ca::i32 FASTORE   = 81;
    static constexpr ca::i32 DASTORE   = 82;
    static constexpr ca::i32 AASTORE   = 83;
    static constexpr ca::i32 BASTORE   = 84;
    static constexpr ca::i32 CASTORE   = 85;
    static constexpr ca::i32 SASTORE   = 86;
    static constexpr ca::i32 POP       = 87;
    static constexpr ca::i32 POP2      = 88;
    static constexpr ca::i32 DUP       = 89;
    static constexpr ca::i32 DUP_X1    = 90;
    static constexpr ca::i32 DUP_X2    = 91;
    static constexpr ca::i32 DUP2      = 92;
    static constexpr ca::i32 DUP2_X1   = 93;
    static constexpr ca::i32 DUP2_X2   = 94;
    static constexpr ca::i32 SWAP      = 95;
    static constexpr ca::i32 IADD      = 96;
    static constexpr ca::i32 LADD      = 97;
    static constexpr ca::i32 FADD      = 98;
    static constexpr ca::i32 DADD      = 99;
    static constexpr ca::i32 ISUB      = 100;
    static constexpr ca::i32 LSUB      = 101;
    static constexpr ca::i32 FSUB      = 102;
    static constexpr ca::i32 DSUB      = 103;
    static constexpr ca::i32 IMUL      = 104;
    static constexpr ca::i32 LMUL      = 105;
    static constexpr ca::i32 FMUL      = 106;
    static constexpr ca::i32 DMUL      = 107;
    static constexpr ca::i32 IDIV      = 108;
    static constexpr ca::i32 LDIV      = 109;
    static constexpr ca::i32 FDIV      = 110;
    static constexpr ca::i32 DDIV      = 111;
    static constexpr ca::i32 IREM      = 112;
    static constexpr ca::i32 LREM      = 113;
    static constexpr ca::i32 FREM      = 114;
    static constexpr ca::i32 DREM      = 115;
    static constexpr ca::i32 INEG      = 116;
    static constexpr ca::i32 LNEG      = 117;
    static constexpr ca::i32 FNEG      = 118;
    static constexpr ca::i32 DNEG      = 119;
    static constexpr ca::i32 ISHL      = 120;
    static constexpr ca::i32 LSHL      = 121;
    static constexpr ca::i32 ISHR      = 122;
    static constexpr ca::i32 LSHR      = 123;
    static constexpr ca::i32 IUSHR     = 124;
    static constexpr ca::i32 LUSHR     = 125;
    static constexpr ca::i32 IAND      = 126;
    static constexpr ca::i32 LAND      = 127;
    static constexpr ca::i32 IOR       = 128;
    static constexpr ca::i32 LOR       = 129;
    static constexpr ca::i32 IXOR      = 130;
    static constexpr ca::i32 LXOR      = 131;
    static constexpr ca::i32 IINC      = 132;
    static constexpr ca::i32 I2L       = 133;
    static constexpr ca::i32 I2F       = 134;
    static constexpr ca::i32 I2D       = 135;
    static constexpr ca::i32 L2I       = 136;
    static constexpr ca::i32 L2F       = 137;
    static constexpr ca::i32 L2D       = 138;
    static constexpr ca::i32 F2I       = 139;
    static constexpr ca::i32 F2L       = 140;
    static constexpr ca::i32 F2D       = 141;
    static constexpr ca::i32 D2I       = 142;
    static constexpr ca::i32 D2L       = 143;
    static constexpr ca::i32 D2F       = 144;
    static constexpr ca::i32 I2B       = 145;
    static constexpr ca::i32 I2C       = 146;
    static constexpr ca::i32 I2S       = 147;
    static constexpr ca::i32 LCMP      = 148;
    static constexpr ca::i32 FCMPL     = 149;
    static constexpr ca::i32 FCMPG     = 150;
    static constexpr ca::i32 DCMPL     = 151;
    static constexpr ca::i32 DCMPG     = 152;
    static constexpr ca::i32 IFEQ      = 153;
    static constexpr ca::i32 IFNE      = 154;
    static constexpr ca::i32 IFLT      = 155;
    static constexpr ca::i32 IFGE      = 156;
    static constexpr ca::i32 IFGT      = 157;
    static constexpr ca::i32 IFLE      = 158;
    static constexpr ca::i32 IF_ICMPEQ = 159;
    static constexpr ca::i32 IF_ICMPNE = 160;
    static constexpr ca::i32 IF_ICMPLT = 161;
    static constexpr ca::i32 IF_ICMPGE = 162;
    static constexpr ca::i32 IF_ICMPGT = 163;
    static constexpr ca::i32 IF_ICMPLE = 164;
    static constexpr ca::i32 IF_ACMPEQ = 165;
    static constexpr ca::i32 IF_ACMPNE = 166;
    static constexpr ca::i32 GOTO      = 167;
    static constexpr ca::i32 JSR       = 168;
    static constexpr ca::i32 RET       = 169;
    // GOTO_W = 200, JSR_W = 201: defined in Constants (not in original ASM Opcodes.java)
    static constexpr ca::i32 TABLESWITCH     = 170;
    static constexpr ca::i32 LOOKUPSWITCH    = 171;
    static constexpr ca::i32 IRETURN         = 172;
    static constexpr ca::i32 LRETURN         = 173;
    static constexpr ca::i32 FRETURN         = 174;
    static constexpr ca::i32 DRETURN         = 175;
    static constexpr ca::i32 ARETURN         = 176;
    static constexpr ca::i32 RETURN          = 177;
    static constexpr ca::i32 GETSTATIC       = 178;
    static constexpr ca::i32 PUTSTATIC       = 179;
    static constexpr ca::i32 GETFIELD        = 180;
    static constexpr ca::i32 PUTFIELD        = 181;
    static constexpr ca::i32 INVOKEVIRTUAL   = 182;
    static constexpr ca::i32 INVOKESPECIAL   = 183;
    static constexpr ca::i32 INVOKESTATIC    = 184;
    static constexpr ca::i32 INVOKEINTERFACE = 185;
    static constexpr ca::i32 INVOKEDYNAMIC   = 186;
    static constexpr ca::i32 NEW             = 187;
    static constexpr ca::i32 NEWARRAY        = 188;
    static constexpr ca::i32 ANEWARRAY       = 189;
    static constexpr ca::i32 ARRAYLENGTH     = 190;
    static constexpr ca::i32 ATHROW          = 191;
    static constexpr ca::i32 CHECKCAST       = 192;
    static constexpr ca::i32 INSTANCEOF      = 193;
    static constexpr ca::i32 MONITORENTER    = 194;
    static constexpr ca::i32 MONITOREXIT     = 195;
    // WIDE = 196: defined in Constants (not in original ASM Opcodes.java)
    static constexpr ca::i32 MULTIANEWARRAY = 197;
    static constexpr ca::i32 IFNULL         = 198;
    static constexpr ca::i32 IFNONNULL      = 199;

private:
    Opcodes() = delete;   // forbid instantiation (analogous to Java interface)
};

}   // namespace jbytecode::core
