// clang-format off
/// @file cppdump_full.cpp
/// @brief Enhanced C++ ClassReader dumper — visits ALL events to validate
///        every reader path: annotations, inner classes, nest, module,
///        record, stack map frames, local variables, etc.
///
/// Usage: cppdump_full <classfile>
/// Output: human-readable trace of ALL visitor events.
/// Compare with: `java AsmDumpFull <classfile>`

#include <cstdio>
#include <vector>
#include <cstdint>
#include <string>
#include <map>
#include "libca/core/any.hpp"

#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/constants.hpp"
#include "jbytecode/core/type.hpp"

using namespace jbytecode::core;

static std::vector<uint8_t> readFile(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> d(sz);
    fread(d.data(), 1, sz, f);
    fclose(f);
    return d;
}

static std::string S(ca::str::Utf8StringRef ref)
{
    if (!ref.data()) return "(null)";
    return std::string((const char*)ref.data(), ref.byte_length());
}

static std::string anyStr(const ca::core::Any& v)
{
    if (!v.has_value()) return "(empty)";
    if (v.is<int>()) return std::to_string(v.cast<int>());
    if (v.is<ca::i64>()) return std::to_string(v.cast<ca::i64>()) + "L";
    if (v.is<float>()) return std::to_string(v.cast<float>()) + "f";
    if (v.is<double>()) return std::to_string(v.cast<double>()) + "d";
    if (v.is<std::string>()) return "\"" + v.cast<std::string>() + "\"";
    if (v.is<Type>()) {
        auto ty = v.cast<Type>();
        // #921 staticArena 退役：无参 getDescriptor() 已删除，检视走注入版（局部 arena）。
        ca::str::Utf8StringArena scratch;
        return "Type(" + S(ty.getDescriptor(scratch)) + ")";
    }
    if (v.is<Handle>()) return "(Handle)";
    if (v.is<ConstantDynamic>()) return "(ConstDynamic)";
    return "(unknown)";
}

static const char* opName(int op)
{
#define OPC(n) \
    case Opcodes::n: return #n;
    switch (op) {
        OPC(NOP)
        OPC(ACONST_NULL)
        OPC(ICONST_M1) OPC(ICONST_0) OPC(ICONST_1) OPC(ICONST_2) OPC(ICONST_3) OPC(ICONST_4) OPC(
            ICONST_5) OPC(LCONST_0) OPC(LCONST_1) OPC(FCONST_0) OPC(FCONST_1) OPC(FCONST_2)
            OPC(DCONST_0) OPC(DCONST_1) OPC(BIPUSH) OPC(SIPUSH) OPC(LDC) OPC(ILOAD) OPC(LLOAD) OPC(
                FLOAD) OPC(DLOAD) OPC(ALOAD) OPC(IALOAD) OPC(LALOAD) OPC(FALOAD) OPC(DALOAD)
                OPC(AALOAD) OPC(BALOAD) OPC(CALOAD) OPC(SALOAD) OPC(ISTORE) OPC(LSTORE) OPC(FSTORE)
                    OPC(DSTORE) OPC(ASTORE) OPC(IASTORE) OPC(LASTORE) OPC(FASTORE) OPC(DASTORE) OPC(
                        AASTORE) OPC(BASTORE) OPC(CASTORE) OPC(SASTORE) OPC(POP) OPC(POP2) OPC(DUP)
                        OPC(DUP_X1) OPC(DUP_X2) OPC(DUP2) OPC(DUP2_X1) OPC(DUP2_X2) OPC(SWAP) OPC(
                            IADD) OPC(LADD) OPC(FADD) OPC(DADD) OPC(ISUB) OPC(LSUB) OPC(FSUB)
                            OPC(DSUB) OPC(IMUL) OPC(LMUL) OPC(FMUL) OPC(DMUL) OPC(IDIV) OPC(LDIV)
                                OPC(FDIV) OPC(DDIV) OPC(IREM) OPC(LREM) OPC(FREM) OPC(DREM)
                                    OPC(INEG) OPC(LNEG) OPC(FNEG) OPC(DNEG) OPC(ISHL) OPC(LSHL) OPC(
                                        ISHR) OPC(LSHR) OPC(IUSHR)
                                        OPC(LUSHR) OPC(IAND) OPC(LAND) OPC(IOR)
                                            OPC(LOR) OPC(IXOR) OPC(LXOR) OPC(IINC) OPC(I2L) OPC(
                                                I2F) OPC(I2D) OPC(L2I) OPC(L2F) OPC(L2D)
                                                OPC(F2I) OPC(F2L) OPC(F2D) OPC(
                                                    D2I) OPC(D2L) OPC(D2F) OPC(I2B) OPC(I2C)
                                                    OPC(I2S) OPC(LCMP) OPC(FCMPL) OPC(FCMPG) OPC(DCMPL) OPC(
                                                        DCMPG) OPC(IFEQ) OPC(IFNE) OPC(IFLT) OPC(IFGE) OPC(IFGT)
                                                        OPC(IFLE) OPC(IF_ICMPEQ) OPC(
                                                            IF_ICMPNE) OPC(IF_ICMPLT) OPC(IF_ICMPGE)
                                                            OPC(IF_ICMPGT) OPC(IF_ICMPLE) OPC(IF_ACMPEQ) OPC(
                                                                IF_ACMPNE) OPC(GOTO) OPC(JSR) OPC(RET)
                                                                OPC(TABLESWITCH) OPC(LOOKUPSWITCH) OPC(
                                                                    IRETURN) OPC(LRETURN) OPC(FRETURN)
                                                                    OPC(DRETURN) OPC(ARETURN) OPC(RETURN) OPC(
                                                                        GETSTATIC) OPC(PUTSTATIC) OPC(GETFIELD)
                                                                        OPC(PUTFIELD) OPC(INVOKEVIRTUAL) OPC(
                                                                            INVOKESPECIAL) OPC(INVOKESTATIC)
                                                                            OPC(INVOKEINTERFACE) OPC(
                                                                                INVOKEDYNAMIC) OPC(NEW)
                                                                                OPC(NEWARRAY) OPC(ANEWARRAY) OPC(
                                                                                    ARRAYLENGTH) OPC(ATHROW)
                                                                                    OPC(CHECKCAST) OPC(
                                                                                        INSTANCEOF) OPC(MONITORENTER)
                                                                                        OPC(MONITOREXIT) OPC(
                                                                                            MULTIANEWARRAY)
                                                                                            OPC(IFNULL)
                                                                                                OPC(IFNONNULL)
    }
#undef OPC
    return "UNKNOWN";
}

static const char* frameTypeName(int type)
{
    switch (type) {
    case Opcodes::F_NEW: return "F_NEW";
    case Opcodes::F_FULL: return "F_FULL";
    case Opcodes::F_APPEND: return "F_APPEND";
    case Opcodes::F_CHOP: return "F_CHOP";
    case Opcodes::F_SAME: return "F_SAME";
    case Opcodes::F_SAME1: return "F_SAME1";
    default: return "F_UNKNOWN";
    }
}

// ---- Full dump annotation visitor ----
struct FullDumpAnnotationVisitor : AnnotationVisitor
{
    int indent;
    FullDumpAnnotationVisitor(int indent)
        : AnnotationVisitor(Opcodes::ASM9)
        , indent(indent)
    {}

    void visit(ca::str::Utf8StringRef name, const ca::core::Any& value) override
    {
        for (int i = 0; i < indent; i++) printf("  ");
        printf("ANN_VAL %s = %s\n",
               name.data() ? S(name).c_str() : "(default)",
               anyStr(value).c_str());
    }
    void visitEnum(ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                   ca::str::Utf8StringRef val) override
    {
        for (int i = 0; i < indent; i++) printf("  ");
        printf("ANN_ENUM %s %s = %s\n", S(name).c_str(), S(desc).c_str(), S(val).c_str());
    }
    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef name,
                                       ca::str::Utf8StringRef desc) override
    {
        for (int i = 0; i < indent; i++) printf("  ");
        printf("ANN_NESTED %s : %s\n", S(name).c_str(), S(desc).c_str());
        return new FullDumpAnnotationVisitor(indent + 1);
    }
    AnnotationVisitor* visitArray(ca::str::Utf8StringRef name) override
    {
        for (int i = 0; i < indent; i++) printf("  ");
        printf("ANN_ARRAY %s\n", S(name).c_str());
        return new FullDumpAnnotationVisitor(indent + 1);
    }
    void visitEnd() override {}
};

// ---- Full dump method visitor ----
struct FullDumpMethodVisitor : MethodVisitor
{
    int                  labelId = 0;
    std::map<void*, int> labelMap;

    FullDumpMethodVisitor()
        : MethodVisitor(Opcodes::ASM9)
    {}

    int getId(void* lbl)
    {
        auto it = labelMap.find(lbl);
        if (it == labelMap.end()) {
            int id        = labelId++;
            labelMap[lbl] = id;
            return id;
        }
        return it->second;
    }

    void visitCode() override { printf("    CODE:\n"); }
    void visitInsn(int op) override { printf("      %s\n", opName(op)); }
    void visitIntInsn(int op, int opd) override { printf("      %s %d\n", opName(op), opd); }
    void visitVarInsn(int op, int var) override { printf("      %s %d\n", opName(op), var); }
    void visitTypeInsn(int op, ca::str::Utf8StringRef type) override
    {
        printf("      %s %s\n", opName(op), S(type).c_str());
    }
    void visitFieldInsn(int op, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef desc) override
    {
        printf(
            "      %s %s.%s %s\n", opName(op), S(owner).c_str(), S(name).c_str(), S(desc).c_str());
    }
    void visitMethodInsn(int op, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef desc, bool itf) override
    {
        printf("      %s %s.%s%s%s\n",
               opName(op),
               S(owner).c_str(),
               S(name).c_str(),
               S(desc).c_str(),
               itf ? " (itf)" : "");
    }
    void visitInvokeDynamicInsn(ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                                const Handle&, const std::vector<ca::core::Any>& bsmArgs) override
    {
        printf("      INVOKEDYNAMIC %s%s bsmArgs=%zu\n",
               S(name).c_str(),
               S(desc).c_str(),
               bsmArgs.size());
    }
    void visitJumpInsn(int op, Label* label) override
    {
        printf("      %s L%d\n", opName(op), getId(label));
    }
    void visitLabel(Label* label) override { printf("      L%d:\n", getId(label)); }
    void visitLdcInsn(const ca::core::Any& value) override
    {
        printf("      LDC %s\n", anyStr(value).c_str());
    }
    void visitIincInsn(int var, int inc) override { printf("      IINC %d %d\n", var, inc); }
    void visitTableSwitchInsn(int min, int max, Label* dflt,
                              const std::vector<Label*>& labels) override
    {
        printf("      TABLESWITCH %d-%d -> L%d\n", min, max, getId(dflt));
        for (size_t i = 0; i < labels.size(); i++)
            printf("        %d: L%d\n", min + (int)i, getId(labels[i]));
    }
    void visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                               const std::vector<Label*>& labels) override
    {
        printf("      LOOKUPSWITCH -> L%d\n", getId(dflt));
        for (size_t i = 0; i < keys.size(); i++)
            printf("        %d: L%d\n", keys[i], getId(labels[i]));
    }
    void visitTryCatchBlock(Label* start, Label* end, Label* handler,
                            ca::str::Utf8StringRef type) override
    {
        printf("      TRYCATCH L%d-L%d -> L%d %s\n",
               getId(start),
               getId(end),
               getId(handler),
               type.data() ? S(type).c_str() : "null");
    }
    void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                            ca::str::Utf8StringRef sig, Label* start, Label* end,
                            int index) override
    {
        printf("      LOCALVAR %s %s", S(name).c_str(), S(desc).c_str());
        if (sig.data()) printf(" sig=%s", S(sig).c_str());
        printf(" L%d-L%d idx=%d\n", getId(start), getId(end), index);
    }
    void visitLineNumber(int line, Label* start) override
    {
        printf("      LINE %d L%d\n", line, getId(start));
    }
    void visitMaxs(int maxStack, int maxLocals) override
    {
        printf("    MAXS: %d %d\n", maxStack, maxLocals);
    }
    void visitParameter(ca::str::Utf8StringRef name, int access) override
    {
        printf("    PARAM: %s acc=0x%x\n", S(name).c_str(), access);
    }
    AnnotationVisitor* visitAnnotationDefault() override
    {
        printf("    ANNOTATION_DEFAULT:\n");
        return new FullDumpAnnotationVisitor(3);
    }
    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef desc, bool visible) override
    {
        printf("    METHOD_ANNOTATION: %s visible=%d\n", S(desc).c_str(), visible);
        return new FullDumpAnnotationVisitor(3);
    }
    AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* tp,
                                           ca::str::Utf8StringRef desc, bool visible) override
    {
        printf("    METHOD_TYPE_ANNOTATION: %s visible=%d typeRef=0x%x\n",
               S(desc).c_str(),
               visible,
               typeRef);
        return new FullDumpAnnotationVisitor(3);
    }
    void visitAnnotableParameterCount(int count, bool visible) override
    {
        printf("    ANNOTABLE_PARAM_COUNT: %d visible=%d\n", count, visible);
    }
    AnnotationVisitor* visitParameterAnnotation(int param, ca::str::Utf8StringRef desc,
                                                bool visible) override
    {
        printf("    PARAM_ANNOTATION[%d]: %s visible=%d\n", param, S(desc).c_str(), visible);
        return new FullDumpAnnotationVisitor(3);
    }
    void visitFrame(int type, int numLocal, const std::vector<ca::core::Any>& local, int numStack,
                    const std::vector<ca::core::Any>& stack) override
    {
        printf("      FRAME: %s nLocal=%d nStack=%d\n", frameTypeName(type), numLocal, numStack);
        try {
            for (int i = 0; i < numLocal && i < (int)local.size(); i++) {
                printf("        local[%d]=%s\n", i, anyStr(local[i]).c_str());
            }
            for (int i = 0; i < numStack && i < (int)stack.size(); i++) {
                printf("        stack[%d]=%s\n", i, anyStr(stack[i]).c_str());
            }
        }
        catch (...) {
            printf("        (error printing frame types)\n");
        }
    }
    void visitAttribute(const Attribute& attr) override
    {
        printf(
            "    ATTR: %s (%zu bytes)\n", attr.type.c_str(), static_cast<size_t>(attr.value.len()));
    }
    void visitEnd() override {}
};

// ---- Full dump field visitor ----
struct FullDumpFieldVisitor : FieldVisitor
{
    FullDumpFieldVisitor()
        : FieldVisitor(Opcodes::ASM9)
    {}
    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef desc, bool visible) override
    {
        printf("    FIELD_ANNOTATION: %s visible=%d\n", S(desc).c_str(), visible);
        return new FullDumpAnnotationVisitor(3);
    }
    AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* tp,
                                           ca::str::Utf8StringRef desc, bool visible) override
    {
        printf("    FIELD_TYPE_ANNOTATION: %s visible=%d typeRef=0x%x\n",
               S(desc).c_str(),
               visible,
               typeRef);
        return new FullDumpAnnotationVisitor(3);
    }
    void visitAttribute(const Attribute& attr) override
    {
        printf("    FIELD_ATTR: %s (%zu bytes)\n",
               attr.type.c_str(),
               static_cast<size_t>(attr.value.len()));
    }
    void visitEnd() override {}
};

// ---- Full dump record component visitor ----
struct FullDumpRecordComponentVisitor : RecordComponentVisitor
{
    FullDumpRecordComponentVisitor()
        : RecordComponentVisitor(Opcodes::ASM9)
    {}
    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef desc, bool visible) override
    {
        printf("    RC_ANNOTATION: %s visible=%d\n", S(desc).c_str(), visible);
        return new FullDumpAnnotationVisitor(3);
    }
    AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* tp,
                                           ca::str::Utf8StringRef desc, bool visible) override
    {
        printf("    RC_TYPE_ANNOTATION: %s visible=%d typeRef=0x%x\n",
               S(desc).c_str(),
               visible,
               typeRef);
        return new FullDumpAnnotationVisitor(3);
    }
    void visitAttribute(const Attribute& attr) override
    {
        printf("    RC_ATTR: %s (%zu bytes)\n",
               attr.type.c_str(),
               static_cast<size_t>(attr.value.len()));
    }
    void visitEnd() override {}
};

// ---- Full dump module visitor ----
struct FullDumpModuleVisitor : ModuleVisitor
{
    FullDumpModuleVisitor()
        : ModuleVisitor(Opcodes::ASM9)
    {}
    void visitMainClass(ca::str::Utf8StringRef mainClass) override
    {
        printf("    MODULE_MAIN_CLASS: %s\n", S(mainClass).c_str());
    }
    void visitPackage(ca::str::Utf8StringRef pkg) override
    {
        printf("    MODULE_PACKAGE: %s\n", S(pkg).c_str());
    }
    void visitRequire(ca::str::Utf8StringRef module, int access,
                      ca::str::Utf8StringRef version) override
    {
        printf("    MODULE_REQUIRE: %s acc=0x%x version=%s\n",
               S(module).c_str(),
               access,
               S(version).c_str());
    }
    void visitExport(ca::str::Utf8StringRef pkg, int access,
                     const std::vector<ca::str::Utf8StringRef>& modules) override
    {
        printf("    MODULE_EXPORT: %s acc=0x%x to=[", S(pkg).c_str(), access);
        for (size_t i = 0; i < modules.size(); i++) {
            if (i) printf(",");
            printf("%s", S(modules[i]).c_str());
        }
        printf("]\n");
    }
    void visitOpen(ca::str::Utf8StringRef pkg, int access,
                   const std::vector<ca::str::Utf8StringRef>& modules) override
    {
        printf("    MODULE_OPEN: %s acc=0x%x to=[", S(pkg).c_str(), access);
        for (size_t i = 0; i < modules.size(); i++) {
            if (i) printf(",");
            printf("%s", S(modules[i]).c_str());
        }
        printf("]\n");
    }
    void visitUse(ca::str::Utf8StringRef service) override
    {
        printf("    MODULE_USE: %s\n", S(service).c_str());
    }
    void visitProvide(ca::str::Utf8StringRef                     service,
                      const std::vector<ca::str::Utf8StringRef>& providers) override
    {
        printf("    MODULE_PROVIDE: %s with=[", S(service).c_str());
        for (size_t i = 0; i < providers.size(); i++) {
            if (i) printf(",");
            printf("%s", S(providers[i]).c_str());
        }
        printf("]\n");
    }
    void visitEnd() override {}
};

// ---- Full dump class visitor ----
struct FullDumpClassVisitor : ClassVisitor
{
    FullDumpClassVisitor()
        : ClassVisitor(Opcodes::ASM9)
    {}

    void visit(int version, int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef sig,
               ca::str::Utf8StringRef                     superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override
    {
        printf("CLASS: %s super=%s v=%d.%d acc=0x%x\n",
               S(name).c_str(),
               S(superName).c_str(),
               version & 0xFFFF,
               version >> 16,
               access);
        if (sig.data()) printf("  SIGNATURE: %s\n", S(sig).c_str());
        for (auto& iface : interfaces) printf("  INTERFACE: %s\n", S(iface).c_str());
    }

    void visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug) override
    {
        printf("  SOURCE: %s\n", source.data() ? S(source).c_str() : "(null)");
        if (debug.data()) printf("  SOURCE_DEBUG: %s\n", S(debug).c_str());
    }

    void visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef desc) override
    {
        printf("  OUTER: %s %s %s\n", S(owner).c_str(), S(name).c_str(), S(desc).c_str());
    }

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef desc, bool visible) override
    {
        printf("  CLASS_ANNOTATION: %s visible=%d\n", S(desc).c_str(), visible);
        return new FullDumpAnnotationVisitor(2);
    }

    AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* tp,
                                           ca::str::Utf8StringRef desc, bool visible) override
    {
        printf("  CLASS_TYPE_ANNOTATION: %s visible=%d typeRef=0x%x\n",
               S(desc).c_str(),
               visible,
               typeRef);
        return new FullDumpAnnotationVisitor(2);
    }

    void visitAttribute(const Attribute& attr) override
    {
        printf(
            "  ATTR: %s (%zu bytes)\n", attr.type.c_str(), static_cast<size_t>(attr.value.len()));
    }

    void visitInnerClass(ca::str::Utf8StringRef name, ca::str::Utf8StringRef outerName,
                         ca::str::Utf8StringRef innerName, int access) override
    {
        printf("  INNER: %s outer=%s inner=%s acc=0x%x\n",
               S(name).c_str(),
               S(outerName).c_str(),
               S(innerName).c_str(),
               access);
    }

    void visitNestHost(ca::str::Utf8StringRef nestHost) override
    {
        printf("  NEST_HOST: %s\n", S(nestHost).c_str());
    }

    void visitNestMember(ca::str::Utf8StringRef nestMember) override
    {
        printf("  NEST_MEMBER: %s\n", S(nestMember).c_str());
    }

    void visitPermittedSubclass(ca::str::Utf8StringRef subclass) override
    {
        printf("  PERMITTED_SUBCLASS: %s\n", S(subclass).c_str());
    }

    ModuleVisitor* visitModule(ca::str::Utf8StringRef name, int access,
                               ca::str::Utf8StringRef version) override
    {
        printf("  MODULE: %s acc=0x%x version=%s\n", S(name).c_str(), access, S(version).c_str());
        return new FullDumpModuleVisitor();
    }

    FieldVisitor* visitField(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                             ca::str::Utf8StringRef sig, const ca::core::Any& value) override
    {
        printf("  FIELD: %s %s acc=0x%x", S(name).c_str(), S(desc).c_str(), access);
        if (sig.data()) printf(" sig=%s", S(sig).c_str());
        if (value.has_value()) printf(" val=%s", anyStr(value).c_str());
        printf("\n");
        return new FullDumpFieldVisitor();
    }

    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                               ca::str::Utf8StringRef                     sig,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        printf("  METHOD: %s%s acc=0x%x", S(name).c_str(), S(desc).c_str(), access);
        if (sig.data()) printf(" sig=%s", S(sig).c_str());
        if (!exceptions.empty()) {
            printf(" throws=");
            for (size_t i = 0; i < exceptions.size(); i++) {
                if (i) printf(",");
                printf("%s", S(exceptions[i]).c_str());
            }
        }
        printf("\n");
        return new FullDumpMethodVisitor();
    }

    RecordComponentVisitor* visitRecordComponent(ca::str::Utf8StringRef name,
                                                 ca::str::Utf8StringRef desc,
                                                 ca::str::Utf8StringRef sig) override
    {
        printf("  RECORD_COMPONENT: %s %s", S(name).c_str(), S(desc).c_str());
        if (sig.data()) printf(" sig=%s", S(sig).c_str());
        printf("\n");
        return new FullDumpRecordComponentVisitor();
    }

    void visitEnd() override {}
};

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <classfile>\n", argv[0]);
        return 1;
    }
    auto bytes = readFile(argv[1]);
    if (bytes.empty()) {
        fprintf(stderr, "Cannot read %s\n", argv[1]);
        return 1;
    }
    ca::str::Utf8StringArena arena;
    ClassReader              cr(arena, bytes);
    FullDumpClassVisitor     dv;
    try {
        cr.accept(&dv, 0);
    }
    catch (const std::exception& e) {
        fprintf(stderr, "EXCEPTION: %s\n", e.what());
        return 2;
    }
    catch (...) {
        fprintf(stderr, "UNKNOWN EXCEPTION\n");
        return 3;
    }
    return 0;
}
