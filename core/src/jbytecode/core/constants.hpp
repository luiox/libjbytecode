#pragma once

#include "libca/core/datatype.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "libca/str/utf8_string.hpp"
#include <set>
#include <vector>

namespace jbytecode::core {

///  @brief JVM classfile attribute names and raw bytecode constant values.
///
///  Attribute names are strings like "Code", "LineNumberTable".
///  Raw bytecode values are the specific shorthand opcodes
///  (ILOAD_0…ALOAD_3, ISTORE_0…ASTORE_3, WIDE, GOTO_W, JSR_W, LDC_W, LDC2_W).
///  These are the actual values that appear in .class byte streams;
///  they are distinct from Opcodes ASM-level abstract opcodes.
///
///  IMPORTANT: When adding a new attribute name constant, add it to the
///  FOREACH_ATTR_NAME macro below AND update the expected set in
///  MethodWriterTest::testCanCopyMethodAttributesUpdated (and
///  MethodWriter::canCopyMethodAttributes if needed).
class Constants
{
public:
    inline static const ca::str::ZUtf8StringRef CONSTANT_VALUE =
        ca::str::ZUtf8StringRef::from_static("ConstantValue");
    inline static const ca::str::ZUtf8StringRef CODE = ca::str::ZUtf8StringRef::from_static("Code");
    inline static const ca::str::ZUtf8StringRef STACK_MAP_TABLE =
        ca::str::ZUtf8StringRef::from_static("StackMapTable");
    inline static const ca::str::ZUtf8StringRef EXCEPTIONS =
        ca::str::ZUtf8StringRef::from_static("Exceptions");
    inline static const ca::str::ZUtf8StringRef INNER_CLASSES =
        ca::str::ZUtf8StringRef::from_static("InnerClasses");
    inline static const ca::str::ZUtf8StringRef ENCLOSING_METHOD =
        ca::str::ZUtf8StringRef::from_static("EnclosingMethod");
    inline static const ca::str::ZUtf8StringRef SYNTHETIC =
        ca::str::ZUtf8StringRef::from_static("Synthetic");
    inline static const ca::str::ZUtf8StringRef SIGNATURE =
        ca::str::ZUtf8StringRef::from_static("Signature");
    inline static const ca::str::ZUtf8StringRef SOURCE_FILE =
        ca::str::ZUtf8StringRef::from_static("SourceFile");
    inline static const ca::str::ZUtf8StringRef SOURCE_DEBUG_EXTENSION =
        ca::str::ZUtf8StringRef::from_static("SourceDebugExtension");
    inline static const ca::str::ZUtf8StringRef LINE_NUMBER_TABLE =
        ca::str::ZUtf8StringRef::from_static("LineNumberTable");
    inline static const ca::str::ZUtf8StringRef LOCAL_VARIABLE_TABLE =
        ca::str::ZUtf8StringRef::from_static("LocalVariableTable");
    inline static const ca::str::ZUtf8StringRef LOCAL_VARIABLE_TYPE_TABLE =
        ca::str::ZUtf8StringRef::from_static("LocalVariableTypeTable");
    inline static const ca::str::ZUtf8StringRef DEPRECATED =
        ca::str::ZUtf8StringRef::from_static("Deprecated");
    inline static const ca::str::ZUtf8StringRef RUNTIME_VISIBLE_ANNOTATIONS =
        ca::str::ZUtf8StringRef::from_static("RuntimeVisibleAnnotations");
    inline static const ca::str::ZUtf8StringRef RUNTIME_INVISIBLE_ANNOTATIONS =
        ca::str::ZUtf8StringRef::from_static("RuntimeInvisibleAnnotations");
    inline static const ca::str::ZUtf8StringRef RUNTIME_VISIBLE_PARAMETER_ANNOTATIONS =
        ca::str::ZUtf8StringRef::from_static("RuntimeVisibleParameterAnnotations");
    inline static const ca::str::ZUtf8StringRef RUNTIME_INVISIBLE_PARAMETER_ANNOTATIONS =
        ca::str::ZUtf8StringRef::from_static("RuntimeInvisibleParameterAnnotations");
    inline static const ca::str::ZUtf8StringRef RUNTIME_VISIBLE_TYPE_ANNOTATIONS =
        ca::str::ZUtf8StringRef::from_static("RuntimeVisibleTypeAnnotations");
    inline static const ca::str::ZUtf8StringRef RUNTIME_INVISIBLE_TYPE_ANNOTATIONS =
        ca::str::ZUtf8StringRef::from_static("RuntimeInvisibleTypeAnnotations");
    inline static const ca::str::ZUtf8StringRef ANNOTATION_DEFAULT =
        ca::str::ZUtf8StringRef::from_static("AnnotationDefault");
    inline static const ca::str::ZUtf8StringRef BOOTSTRAP_METHODS =
        ca::str::ZUtf8StringRef::from_static("BootstrapMethods");
    inline static const ca::str::ZUtf8StringRef METHOD_PARAMETERS =
        ca::str::ZUtf8StringRef::from_static("MethodParameters");
    inline static const ca::str::ZUtf8StringRef MODULE =
        ca::str::ZUtf8StringRef::from_static("Module");
    inline static const ca::str::ZUtf8StringRef MODULE_PACKAGES =
        ca::str::ZUtf8StringRef::from_static("ModulePackages");
    inline static const ca::str::ZUtf8StringRef MODULE_MAIN_CLASS =
        ca::str::ZUtf8StringRef::from_static("ModuleMainClass");
    inline static const ca::str::ZUtf8StringRef NEST_HOST =
        ca::str::ZUtf8StringRef::from_static("NestHost");
    inline static const ca::str::ZUtf8StringRef NEST_MEMBERS =
        ca::str::ZUtf8StringRef::from_static("NestMembers");
    inline static const ca::str::ZUtf8StringRef PERMITTED_SUBCLASSES =
        ca::str::ZUtf8StringRef::from_static("PermittedSubclasses");
    inline static const ca::str::ZUtf8StringRef RECORD =
        ca::str::ZUtf8StringRef::from_static("Record");

    // ASM 特定访问标志
    inline static constexpr ca::i32 ACC_CONSTRUCTOR = 0x40000;   // method access flag

    // ASM 特定栈映射帧类型，用于 ClassVisitor::visitFrame
    inline static constexpr ca::i32 F_INSERT = 256;

    // JVM 操作码值（classfile 字节值，与 ASM 抽象 Opcodes 区分）
    // The JVM opcode values which are not part of the ASM public API.
    // See https://docs.oracle.com/javase/specs/jvms/se9/html/jvms-6.html.

    inline static constexpr ca::i32 LDC_W    = 19;
    inline static constexpr ca::i32 LDC2_W   = 20;
    inline static constexpr ca::i32 ILOAD_0  = 26;
    inline static constexpr ca::i32 ILOAD_1  = 27;
    inline static constexpr ca::i32 ILOAD_2  = 28;
    inline static constexpr ca::i32 ILOAD_3  = 29;
    inline static constexpr ca::i32 LLOAD_0  = 30;
    inline static constexpr ca::i32 LLOAD_1  = 31;
    inline static constexpr ca::i32 LLOAD_2  = 32;
    inline static constexpr ca::i32 LLOAD_3  = 33;
    inline static constexpr ca::i32 FLOAD_0  = 34;
    inline static constexpr ca::i32 FLOAD_1  = 35;
    inline static constexpr ca::i32 FLOAD_2  = 36;
    inline static constexpr ca::i32 FLOAD_3  = 37;
    inline static constexpr ca::i32 DLOAD_0  = 38;
    inline static constexpr ca::i32 DLOAD_1  = 39;
    inline static constexpr ca::i32 DLOAD_2  = 40;
    inline static constexpr ca::i32 DLOAD_3  = 41;
    inline static constexpr ca::i32 ALOAD_0  = 42;
    inline static constexpr ca::i32 ALOAD_1  = 43;
    inline static constexpr ca::i32 ALOAD_2  = 44;
    inline static constexpr ca::i32 ALOAD_3  = 45;
    inline static constexpr ca::i32 ISTORE_0 = 59;
    inline static constexpr ca::i32 ISTORE_1 = 60;
    inline static constexpr ca::i32 ISTORE_2 = 61;
    inline static constexpr ca::i32 ISTORE_3 = 62;
    inline static constexpr ca::i32 LSTORE_0 = 63;
    inline static constexpr ca::i32 LSTORE_1 = 64;
    inline static constexpr ca::i32 LSTORE_2 = 65;
    inline static constexpr ca::i32 LSTORE_3 = 66;
    inline static constexpr ca::i32 FSTORE_0 = 67;
    inline static constexpr ca::i32 FSTORE_1 = 68;
    inline static constexpr ca::i32 FSTORE_2 = 69;
    inline static constexpr ca::i32 FSTORE_3 = 70;
    inline static constexpr ca::i32 DSTORE_0 = 71;
    inline static constexpr ca::i32 DSTORE_1 = 72;
    inline static constexpr ca::i32 DSTORE_2 = 73;
    inline static constexpr ca::i32 DSTORE_3 = 74;
    inline static constexpr ca::i32 ASTORE_0 = 75;
    inline static constexpr ca::i32 ASTORE_1 = 76;
    inline static constexpr ca::i32 ASTORE_2 = 77;
    inline static constexpr ca::i32 ASTORE_3 = 78;
    inline static constexpr ca::i32 WIDE     = 196;
    inline static constexpr ca::i32 GOTO_W   = 200;
    inline static constexpr ca::i32 JSR_W    = 201;

    // 普通跳转与宽跳转之间的转换常量
    inline static constexpr ca::i32 WIDE_JUMP_OPCODE_DELTA = GOTO_W - Opcodes::GOTO;

    // JVM 操作码与 ASM 特定操作码之间的转换常量
    inline static constexpr ca::i32 ASM_OPCODE_DELTA        = 49;
    inline static constexpr ca::i32 ASM_IFNULL_OPCODE_DELTA = 20;

    // ASM specific opcodes, used for long forward jump instructions.
    inline static constexpr ca::i32 ASM_IFEQ      = Opcodes::IFEQ + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IFNE      = Opcodes::IFNE + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IFLT      = Opcodes::IFLT + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IFGE      = Opcodes::IFGE + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IFGT      = Opcodes::IFGT + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IFLE      = Opcodes::IFLE + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IF_ICMPEQ = Opcodes::IF_ICMPEQ + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IF_ICMPNE = Opcodes::IF_ICMPNE + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IF_ICMPLT = Opcodes::IF_ICMPLT + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IF_ICMPGE = Opcodes::IF_ICMPGE + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IF_ICMPGT = Opcodes::IF_ICMPGT + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IF_ICMPLE = Opcodes::IF_ICMPLE + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IF_ACMPEQ = Opcodes::IF_ACMPEQ + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IF_ACMPNE = Opcodes::IF_ACMPNE + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_GOTO      = Opcodes::GOTO + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_JSR       = Opcodes::JSR + ASM_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IFNULL    = Opcodes::IFNULL + ASM_IFNULL_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_IFNONNULL = Opcodes::IFNONNULL + ASM_IFNULL_OPCODE_DELTA;
    inline static constexpr ca::i32 ASM_GOTO_W    = 220;

    /// Returns all attribute name constants defined in this class.
    /// Used by MethodWriterTest::testCanCopyMethodAttributesUpdated
    /// as a dynamic source of truth (equivalent to Java reflection).
    struct ZRefCompare
    {
        bool operator()(const ca::str::ZUtf8StringRef& a, const ca::str::ZUtf8StringRef& b) const
        {
            return a.compare(b) < 0;
        }
    };
    using ZRefSet = std::set<ca::str::ZUtf8StringRef, ZRefCompare>;
    static ZRefSet allAttributeNames()
    {
        static const ZRefSet names = {
            CONSTANT_VALUE,
            CODE,
            STACK_MAP_TABLE,
            EXCEPTIONS,
            INNER_CLASSES,
            ENCLOSING_METHOD,
            SYNTHETIC,
            SIGNATURE,
            SOURCE_FILE,
            SOURCE_DEBUG_EXTENSION,
            LINE_NUMBER_TABLE,
            LOCAL_VARIABLE_TABLE,
            LOCAL_VARIABLE_TYPE_TABLE,
            DEPRECATED,
            RUNTIME_VISIBLE_ANNOTATIONS,
            RUNTIME_INVISIBLE_ANNOTATIONS,
            RUNTIME_VISIBLE_PARAMETER_ANNOTATIONS,
            RUNTIME_INVISIBLE_PARAMETER_ANNOTATIONS,
            RUNTIME_VISIBLE_TYPE_ANNOTATIONS,
            RUNTIME_INVISIBLE_TYPE_ANNOTATIONS,
            ANNOTATION_DEFAULT,
            BOOTSTRAP_METHODS,
            METHOD_PARAMETERS,
            MODULE,
            MODULE_PACKAGES,
            MODULE_MAIN_CLASS,
            NEST_HOST,
            NEST_MEMBERS,
            PERMITTED_SUBCLASSES,
            RECORD,
        };
        return names;
    }

private:
    Constants() = delete;
};

}   // namespace jbytecode::core
