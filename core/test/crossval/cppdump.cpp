// clang-format off
#include <cstdio>
#include <vector>
#include <cstdint>
#include <string>
#include <map>

#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/constants.hpp"

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

static std::string refToStr(ca::str::Utf8StringRef ref)
{
    return std::string((const char*)ref.data(), ref.byte_length());
}

static const char* opcodeName(int op)
{
#define OP(n) \
    case Opcodes::n: return #n;
    switch (op) {
        OP(NOP)
        OP(ACONST_NULL)
        OP(ICONST_M1) OP(ICONST_0) OP(ICONST_1) OP(ICONST_2) OP(ICONST_3) OP(ICONST_4) OP(
            ICONST_5) OP(LCONST_0) OP(LCONST_1) OP(FCONST_0) OP(FCONST_1) OP(FCONST_2) OP(DCONST_0)
            OP(DCONST_1) OP(BIPUSH) OP(SIPUSH) OP(LDC) OP(ILOAD) OP(LLOAD) OP(FLOAD) OP(DLOAD) OP(
                ALOAD) OP(IALOAD) OP(LALOAD) OP(FALOAD) OP(DALOAD) OP(AALOAD) OP(BALOAD) OP(CALOAD)
                OP(SALOAD) OP(ISTORE) OP(LSTORE) OP(FSTORE) OP(DSTORE) OP(ASTORE) OP(IASTORE) OP(
                    LASTORE) OP(FASTORE) OP(DASTORE) OP(AASTORE) OP(BASTORE) OP(CASTORE) OP(SASTORE)
                    OP(POP) OP(POP2) OP(DUP) OP(DUP_X1) OP(DUP_X2) OP(DUP2) OP(DUP2_X1) OP(
                        DUP2_X2) OP(SWAP) OP(IADD) OP(LADD) OP(FADD) OP(DADD) OP(ISUB) OP(LSUB)
                        OP(FSUB) OP(DSUB) OP(IMUL) OP(LMUL) OP(FMUL) OP(DMUL) OP(IDIV) OP(LDIV) OP(
                            FDIV) OP(DDIV) OP(IREM) OP(LREM) OP(FREM)
                            OP(DREM) OP(INEG) OP(LNEG) OP(
                                FNEG) OP(DNEG) OP(ISHL) OP(LSHL) OP(ISHR)
                                OP(LSHR) OP(IUSHR) OP(LUSHR) OP(IAND) OP(LAND) OP(IOR) OP(LOR) OP(
                                    IXOR) OP(LXOR)
                                    OP(IINC) OP(I2L) OP(I2F) OP(I2D) OP(L2I) OP(L2F) OP(L2D) OP(F2I) OP(F2L) OP(F2D) OP(
                                        D2I) OP(D2L)
                                        OP(
                                            D2F) OP(I2B)
                                            OP(
                                                I2C) OP(I2S)
                                                OP(LCMP) OP(FCMPL) OP(FCMPG) OP(DCMPL) OP(DCMPG) OP(
                                                    IFEQ) OP(IFNE) OP(IFLT) OP(IFGE) OP(IFGT)
                                                    OP(IFLE) OP(IF_ICMPEQ) OP(IF_ICMPNE) OP(IF_ICMPLT) OP(
                                                        IF_ICMPGE) OP(IF_ICMPGT) OP(IF_ICMPLE)
                                                        OP(IF_ACMPEQ) OP(
                                                            IF_ACMPNE) OP(GOTO) OP(JSR) OP(RET) OP(TABLESWITCH)
                                                            OP(LOOKUPSWITCH) OP(IRETURN) OP(LRETURN) OP(FRETURN) OP(
                                                                DRETURN) OP(ARETURN)
                                                                OP(RETURN) OP(GETSTATIC) OP(PUTSTATIC) OP(
                                                                    GETFIELD) OP(PUTFIELD) OP(INVOKEVIRTUAL)
                                                                    OP(INVOKESPECIAL) OP(
                                                                        INVOKESTATIC) OP(INVOKEINTERFACE)
                                                                        OP(INVOKEDYNAMIC) OP(
                                                                            NEW) OP(NEWARRAY)
                                                                            OP(ANEWARRAY) OP(
                                                                                ARRAYLENGTH) OP(ATHROW)
                                                                                OP(CHECKCAST) OP(
                                                                                    INSTANCEOF)
                                                                                    OP(MONITORENTER) OP(
                                                                                        MONITOREXIT)
                                                                                        OP(MULTIANEWARRAY)
                                                                                            OP(IFNULL)
                                                                                                OP(IFNONNULL)
    }
#undef OP
    return "UNKNOWN";
}

struct DumpVisitor : ClassVisitor
{
    DumpVisitor()
        : ClassVisitor(Opcodes::ASM4)
    {}

    void visit(int version, int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef sig,
               ca::str::Utf8StringRef                     superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override
    {
        printf("CLASS: %s super=%s v=%d.%d acc=0x%x\n",
               refToStr(name).c_str(),
               refToStr(superName).c_str(),
               version & 0xFFFF,
               version >> 16,
               access);
        for (auto& iface : interfaces) printf("  INTERFACE: %s\n", refToStr(iface).c_str());
    }

    void visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef) override
    {
        if (source.data()) printf("  SOURCE: %s\n", refToStr(source).c_str());
    }

    FieldVisitor* visitField(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                             ca::str::Utf8StringRef sig, const ca::core::Any& value) override
    {
        printf("  FIELD: %s %s acc=0x%x\n", refToStr(name).c_str(), refToStr(desc).c_str(), access);
        return nullptr;
    }

    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                               ca::str::Utf8StringRef                     sig,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        printf("  METHOD: %s%s acc=0x%x\n", refToStr(name).c_str(), refToStr(desc).c_str(), access);
        for (auto& ex : exceptions) printf("    EXCEPTION: %s\n", refToStr(ex).c_str());
        return new DumpMethodVisitor();
    }

    struct DumpMethodVisitor : MethodVisitor
    {
        DumpMethodVisitor()
            : MethodVisitor(Opcodes::ASM4)
            , labelId(0)
        {}

        int                  labelId;
        std::map<void*, int> labelMap;

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
        void visitInsn(int opcode) override { printf("      %s\n", opcodeName(opcode)); }
        void visitIntInsn(int opcode, int operand) override
        {
            printf("      %s %d\n", opcodeName(opcode), operand);
        }
        void visitVarInsn(int opcode, int var) override
        {
            printf("      %s %d\n", opcodeName(opcode), var);
        }
        void visitTypeInsn(int opcode, ca::str::Utf8StringRef type) override
        {
            printf("      %s %s\n", opcodeName(opcode), refToStr(type).c_str());
        }
        void visitFieldInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                            ca::str::Utf8StringRef desc) override
        {
            printf("      %s %s.%s %s\n",
                   opcodeName(opcode),
                   refToStr(owner).c_str(),
                   refToStr(name).c_str(),
                   refToStr(desc).c_str());
        }
        void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                             ca::str::Utf8StringRef desc, bool itf) override
        {
            printf("      %s %s.%s%s%s\n",
                   opcodeName(opcode),
                   refToStr(owner).c_str(),
                   refToStr(name).c_str(),
                   refToStr(desc).c_str(),
                   itf ? " (itf)" : "");
        }
        void visitJumpInsn(int opcode, Label* label) override
        {
            printf("      %s L%d\n", opcodeName(opcode), getId(label));
        }
        void visitLabel(Label* label) override { printf("      L%d:\n", getId(label)); }
        void visitLdcInsn(const ca::core::Any& value) override
        {
            if (value.is<int>())
                printf("      LDC %d\n", value.cast<int>());
            else if (value.is<float>())
                printf("      LDC %f\n", value.cast<float>());
            else if (value.is<double>())
                printf("      LDC %f\n", value.cast<double>());
            else if (value.is<ca::i64>())
                printf("      LDC %lld\n", (long long)value.cast<ca::i64>());
            else if (value.is<std::string>())
                printf("      LDC \"%s\"\n", value.cast<std::string>().c_str());
            else
                printf("      LDC (unknown)\n");
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
                   type.data() ? refToStr(type).c_str() : "null");
        }
        void visitLineNumber(int line, Label* start) override
        {
            printf("      LINE %d L%d\n", line, getId(start));
        }
        void visitMaxs(int maxStack, int maxLocals) override
        {
            printf("    MAXS: %d %d\n", maxStack, maxLocals);
        }
    };
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
    DumpVisitor              dv;
    cr.accept(&dv, 0);
    return 0;
}
