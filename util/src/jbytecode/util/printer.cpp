#include "jbytecode/util/printer.hpp"
#include "jbytecode/core/opcodes.hpp"
#include <cstring>

using namespace jbytecode::util;
using namespace jbytecode::core;
using namespace ca::str;

const char* Printer::OPCODES[200] = {"NOP",           "ACONST_NULL",  "ICONST_M1",
                                     "ICONST_0",      "ICONST_1",     "ICONST_2",
                                     "ICONST_3",      "ICONST_4",     "ICONST_5",
                                     "LCONST_0",      "LCONST_1",     "FCONST_0",
                                     "FCONST_1",      "FCONST_2",     "DCONST_0",
                                     "DCONST_1",      "BIPUSH",       "SIPUSH",
                                     "LDC",           "LDC_W",        "LDC2_W",
                                     "ILOAD",         "LLOAD",        "FLOAD",
                                     "DLOAD",         "ALOAD",        "ILOAD_0",
                                     "ILOAD_1",       "ILOAD_2",      "ILOAD_3",
                                     "LLOAD_0",       "LLOAD_1",      "LLOAD_2",
                                     "LLOAD_3",       "FLOAD_0",      "FLOAD_1",
                                     "FLOAD_2",       "FLOAD_3",      "DLOAD_0",
                                     "DLOAD_1",       "DLOAD_2",      "DLOAD_3",
                                     "ALOAD_0",       "ALOAD_1",      "ALOAD_2",
                                     "ALOAD_3",       "IALOAD",       "LALOAD",
                                     "FALOAD",        "DALOAD",       "AALOAD",
                                     "BALOAD",        "CALOAD",       "SALOAD",
                                     "ISTORE",        "LSTORE",       "FSTORE",
                                     "DSTORE",        "ASTORE",       "ISTORE_0",
                                     "ISTORE_1",      "ISTORE_2",     "ISTORE_3",
                                     "LSTORE_0",      "LSTORE_1",     "LSTORE_2",
                                     "LSTORE_3",      "FSTORE_0",     "FSTORE_1",
                                     "FSTORE_2",      "FSTORE_3",     "DSTORE_0",
                                     "DSTORE_1",      "DSTORE_2",     "DSTORE_3",
                                     "ASTORE_0",      "ASTORE_1",     "ASTORE_2",
                                     "ASTORE_3",      "IASTORE",      "LASTORE",
                                     "FASTORE",       "DASTORE",      "AASTORE",
                                     "BASTORE",       "CASTORE",      "SASTORE",
                                     "POP",           "POP2",         "DUP",
                                     "DUP_X1",        "DUP_X2",       "DUP2",
                                     "DUP2_X1",       "DUP2_X2",      "SWAP",
                                     "IADD",          "LADD",         "FADD",
                                     "DADD",          "ISUB",         "LSUB",
                                     "FSUB",          "DSUB",         "IMUL",
                                     "LMUL",          "FMUL",         "DMUL",
                                     "IDIV",          "LDIV",         "FDIV",
                                     "DDIV",          "IREM",         "LREM",
                                     "FREM",          "DREM",         "INEG",
                                     "LNEG",          "FNEG",         "DNEG",
                                     "ISHL",          "LSHL",         "ISHR",
                                     "LSHR",          "IUSHR",        "LUSHR",
                                     "IAND",          "LAND",         "IOR",
                                     "LOR",           "IXOR",         "LXOR",
                                     "IINC",          "I2L",          "I2F",
                                     "I2D",           "L2I",          "L2F",
                                     "L2D",           "F2I",          "F2L",
                                     "F2D",           "D2I",          "D2L",
                                     "D2F",           "I2B",          "I2C",
                                     "I2S",           "LCMP",         "FCMPL",
                                     "FCMPG",         "DCMPL",        "DCMPG",
                                     "IFEQ",          "IFNE",         "IFLT",
                                     "IFGE",          "IFGT",         "IFLE",
                                     "IF_ICMPEQ",     "IF_ICMPNE",    "IF_ICMPLT",
                                     "IF_ICMPGE",     "IF_ICMPGT",    "IF_ICMPLE",
                                     "IF_ACMPEQ",     "IF_ACMPNE",    "GOTO",
                                     "JSR",           "RET",          "TABLESWITCH",
                                     "LOOKUPSWITCH",  "IRETURN",      "LRETURN",
                                     "FRETURN",       "DRETURN",      "ARETURN",
                                     "RETURN",        "GETSTATIC",    "PUTSTATIC",
                                     "GETFIELD",      "PUTFIELD",     "INVOKEVIRTUAL",
                                     "INVOKESPECIAL", "INVOKESTATIC", "INVOKEINTERFACE",
                                     "INVOKEDYNAMIC", "NEW",          "NEWARRAY",
                                     "ANEWARRAY",     "ARRAYLENGTH",  "ATHROW",
                                     "CHECKCAST",     "INSTANCEOF",   "MONITORENTER",
                                     "MONITOREXIT",   "WIDE",         "MULTIANEWARRAY",
                                     "IFNULL",        "IFNONNULL"};

const char* Printer::TYPES[12] = {nullptr,
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  "T_BOOLEAN",
                                  "T_CHAR",
                                  "T_FLOAT",
                                  "T_DOUBLE",
                                  "T_BYTE",
                                  "T_SHORT",
                                  "T_INT",
                                  "T_LONG"};

const char* Printer::HANDLE_TAG[10] = {nullptr,
                                       "H_GETFIELD",
                                       "H_GETSTATIC",
                                       "H_PUTFIELD",
                                       "H_PUTSTATIC",
                                       "H_INVOKEVIRTUAL",
                                       "H_INVOKESTATIC",
                                       "H_INVOKESPECIAL",
                                       "H_NEWINVOKESPECIAL",
                                       "H_INVOKEINTERFACE"};

Printer::Printer(int api)
    : api(api)
{}

void Printer::addLine(Utf8StringRef line)
{
    text.push_back(textArena.intern(line));
}

void Printer::addLine()
{
    text.push_back(Utf8StringRef());
}

void Printer::setBuf(Utf8StringRef s)
{
    stringBuilder.clear();
    stringBuilder.append(s);
}

void Printer::appendBuf(Utf8StringRef s)
{
    stringBuilder.append(s);
}

void Printer::clearBuf()
{
    stringBuilder.clear();
}

// -- Default empty implementations for all virtual methods --

void Printer::visit(int, int, Utf8StringRef, Utf8StringRef, Utf8StringRef,
                    const std::vector<Utf8StringRef>&)
{}
void     Printer::visitSource(Utf8StringRef, Utf8StringRef) {}
void     Printer::visitOuterClass(Utf8StringRef, Utf8StringRef, Utf8StringRef) {}
Printer* Printer::visitClassAnnotation(Utf8StringRef, bool)
{
    return nullptr;
}
Printer* Printer::visitClassTypeAnnotation(int, Utf8StringRef, Utf8StringRef, bool)
{
    return nullptr;
}
void     Printer::visitClassAttribute(const Attribute&) {}
void     Printer::visitInnerClass(Utf8StringRef, Utf8StringRef, Utf8StringRef, int) {}
Printer* Printer::visitField(int, Utf8StringRef, Utf8StringRef, Utf8StringRef, const ca::core::Any&)
{
    return nullptr;
}
Printer* Printer::visitMethod(int, Utf8StringRef, Utf8StringRef, Utf8StringRef,
                              const std::vector<Utf8StringRef>&)
{
    return nullptr;
}
void     Printer::visitClassEnd() {}
void     Printer::visitAnnotationValue(Utf8StringRef, const ca::core::Any&) {}
void     Printer::visitAnnotationEnum(Utf8StringRef, Utf8StringRef, Utf8StringRef) {}
Printer* Printer::visitAnnotationAnnotation(Utf8StringRef, Utf8StringRef)
{
    return nullptr;
}
Printer* Printer::visitAnnotationArray(Utf8StringRef)
{
    return nullptr;
}
void Printer::visitAnnotationEnd() {}
void Printer::visitCode() {}
void Printer::visitFrame(int, int, const std::vector<ca::core::Any>&, int,
                         const std::vector<ca::core::Any>&)
{}
void Printer::visitInsn(int) {}
void Printer::visitIntInsn(int, int) {}
void Printer::visitVarInsn(int, int) {}
void Printer::visitTypeInsn(int, Utf8StringRef) {}
void Printer::visitFieldInsn(int, Utf8StringRef, Utf8StringRef, Utf8StringRef) {}
void Printer::visitMethodInsn(int, Utf8StringRef, Utf8StringRef, Utf8StringRef, bool) {}
void Printer::visitInvokeDynamicInsn(Utf8StringRef, Utf8StringRef, const Handle&,
                                     const std::vector<ca::core::Any>&)
{}
void Printer::visitJumpInsn(int, Label*) {}
void Printer::visitLabel(Label*) {}
void Printer::visitLdcInsn(const ca::core::Any&) {}
void Printer::visitIincInsn(int, int) {}
void Printer::visitTableSwitchInsn(int, int, Label*, const std::vector<Label*>&) {}
void Printer::visitLookupSwitchInsn(Label*, const std::vector<int>&, const std::vector<Label*>&) {}
void Printer::visitMultiANewArrayInsn(Utf8StringRef, int) {}
void Printer::visitTryCatchBlock(Label*, Label*, Label*, Utf8StringRef) {}
void Printer::visitLocalVariable(Utf8StringRef, Utf8StringRef, Utf8StringRef, Label*, Label*, int)
{}
void     Printer::visitLineNumber(int, Label*) {}
void     Printer::visitMaxs(int, int) {}
void     Printer::visitFieldEnd() {}
void     Printer::visitMethodEnd() {}
void     Printer::visitMainClass(Utf8StringRef) {}
void     Printer::visitPackage(Utf8StringRef) {}
void     Printer::visitRequire(Utf8StringRef, int, Utf8StringRef) {}
void     Printer::visitExport(Utf8StringRef, int, const std::vector<Utf8StringRef>&) {}
void     Printer::visitOpen(Utf8StringRef, int, const std::vector<Utf8StringRef>&) {}
void     Printer::visitUse(Utf8StringRef) {}
void     Printer::visitProvide(Utf8StringRef, const std::vector<Utf8StringRef>&) {}
void     Printer::visitModuleEnd() {}
Printer* Printer::visitRecordComponentAnnotation(Utf8StringRef, bool)
{
    return nullptr;
}
Printer* Printer::visitRecordComponentTypeAnnotation(int, Utf8StringRef, Utf8StringRef, bool)
{
    return nullptr;
}
void Printer::visitRecordComponentAttribute(const Attribute&) {}
void Printer::visitRecordComponentEnd() {}

std::string Printer::appendAccessFlags(int access, bool isClass)
{
    std::string result;
    if ((access & Opcodes::ACC_PUBLIC) != 0) result += "public ";
    if ((access & Opcodes::ACC_PRIVATE) != 0) result += "private ";
    if ((access & Opcodes::ACC_PROTECTED) != 0) result += "protected ";
    if ((access & Opcodes::ACC_STATIC) != 0) result += "static ";
    if ((access & Opcodes::ACC_FINAL) != 0) result += "final ";
    if (isClass && (access & Opcodes::ACC_SUPER) != 0) result += "super ";
    if ((access & Opcodes::ACC_SYNCHRONIZED) != 0 && !isClass) result += "synchronized ";
    if ((access & Opcodes::ACC_VOLATILE) != 0 && !isClass) result += "volatile ";
    if ((access & Opcodes::ACC_TRANSIENT) != 0 && !isClass) result += "transient ";
    if ((access & Opcodes::ACC_NATIVE) != 0) result += "native ";
    if ((access & Opcodes::ACC_INTERFACE) != 0) result += "interface ";
    if ((access & Opcodes::ACC_ABSTRACT) != 0) result += "abstract ";
    if ((access & Opcodes::ACC_STRICT) != 0) result += "strictfp ";
    if ((access & Opcodes::ACC_SYNTHETIC) != 0) result += "synthetic ";
    if ((access & Opcodes::ACC_DEPRECATED) != 0) result += "@deprecated ";
    if ((access & Opcodes::ACC_ENUM) != 0) result += "enum ";
    if ((access & Opcodes::ACC_MANDATED) != 0) result += "mandated ";
    if ((access & Opcodes::ACC_MODULE) != 0) result += "module ";
    if ((access & Opcodes::ACC_OPEN) != 0) result += "open ";
    if ((access & Opcodes::ACC_TRANSITIVE) != 0) result += "transitive ";
    if ((access & Opcodes::ACC_STATIC_PHASE) != 0) result += "staticPhase ";
    return result;
}

struct FlagEntry
{
    int         flag;
    const char* nameLower;
    const char* nameUpper;
};
static const FlagEntry FLAG_ENTRIES[] = {
    {Opcodes::ACC_PUBLIC, "public", "PUBLIC"},
    {Opcodes::ACC_PRIVATE, "private", "PRIVATE"},
    {Opcodes::ACC_PROTECTED, "protected", "PROTECTED"},
    {Opcodes::ACC_STATIC, "static", "STATIC"},
    {Opcodes::ACC_FINAL, "final", "FINAL"},
    {Opcodes::ACC_SUPER, "super", "SUPER"},
    {Opcodes::ACC_SYNCHRONIZED, "synchronized", "SYNCHRONIZED"},
    {Opcodes::ACC_VOLATILE, "volatile", "VOLATILE"},
    {Opcodes::ACC_BRIDGE, "bridge", "BRIDGE"},
    {Opcodes::ACC_VARARGS, "varargs", "VARARGS"},
    {Opcodes::ACC_TRANSIENT, "transient", "TRANSIENT"},
    {Opcodes::ACC_NATIVE, "native", "NATIVE"},
    {Opcodes::ACC_INTERFACE, "interface", "INTERFACE"},
    {Opcodes::ACC_ABSTRACT, "abstract", "ABSTRACT"},
    {Opcodes::ACC_STRICT, "strict", "STRICT"},
    {Opcodes::ACC_SYNTHETIC, "synthetic", "SYNTHETIC"},
    {Opcodes::ACC_ANNOTATION, "annotation", "ANNOTATION"},
    {Opcodes::ACC_ENUM, "enum", "ENUM"},
    {Opcodes::ACC_MODULE, "module", "MODULE"},
};

Utf8String Printer::appendString(Utf8StringRef str)
{
    Utf8StringBuilder builder;
    builder.append("\"");
    for (ca::usize i = 0; i < str.byte_length(); ++i) {
        ca::u8 c = str.byte_at(i);
        switch (c) {
        case '\n': builder.append("\\n"); break;
        case '\r': builder.append("\\r"); break;
        case '\t': builder.append("\\t"); break;
        case '"': builder.append("\\\""); break;
        case '\\': builder.append("\\\\"); break;
        default: builder.append(&c, 1); break;
        }
    }
    builder.append("\"");
    return builder.build();
}

std::string Printer::getAccessFlagsStr(int access, bool capital)
{
    std::string buf;
    for (auto& e : FLAG_ENTRIES) {
        if (access & e.flag) {
            if (!buf.empty()) buf += ' ';
            buf += capital ? e.nameUpper : e.nameLower;
        }
    }
    if (buf.empty()) buf = capital ? "(default)" : "(default)";
    return buf;
}
