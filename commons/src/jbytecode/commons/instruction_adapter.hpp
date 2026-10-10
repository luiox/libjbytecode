#pragma once

#include "libca/core/any.hpp"
#include <cstdint>

#include "libca/core/any.hpp"
#include <string>
#include <vector>

#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class InstructionAdapter : public jbytecode::core::MethodVisitor
{
public:
    static const jbytecode::core::Type OBJECT_TYPE;

    explicit InstructionAdapter(jbytecode::core::MethodVisitor* methodVisitor);

protected:
    InstructionAdapter(int api, jbytecode::core::MethodVisitor* methodVisitor);

public:
    void visitInsn(int opcode) override;
    void visitIntInsn(int opcode, int operand) override;
    void visitVarInsn(int opcode, int varIndex) override;
    void visitTypeInsn(int opcode, ca::str::Utf8StringRef type) override;
    void visitFieldInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef descriptor) override;
    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor, bool isInterface) override;
    void visitInvokeDynamicInsn(
        ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
        const jbytecode::core::Handle&    bootstrapMethodHandle,
        const std::vector<ca::core::Any>& bootstrapMethodArguments) override;
    void visitJumpInsn(int opcode, jbytecode::core::Label* label) override;
    void visitLabel(jbytecode::core::Label* label) override;
    void visitLdcInsn(const ca::core::Any& value) override;
    void visitIincInsn(int varIndex, int increment) override;
    void visitTableSwitchInsn(int min, int max, jbytecode::core::Label* dflt,
                              const std::vector<jbytecode::core::Label*>& labels) override;
    void visitLookupSwitchInsn(jbytecode::core::Label* dflt, const std::vector<int>& keys,
                               const std::vector<jbytecode::core::Label*>& labels) override;
    void visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor, int numDimensions) override;

    void        nop();
    void        aconst(const ca::core::Any& value);
    void        iconst(int intValue);
    void        lconst(int64_t longValue);
    void        fconst(float floatValue);
    void        dconst(double doubleValue);
    void        tconst(const jbytecode::core::Type& type);
    void        hconst(const jbytecode::core::Handle& handle);
    void        cconst(const jbytecode::core::ConstantDynamic& constantDynamic);
    void        load(int varIndex, const jbytecode::core::Type& type);
    void        aload(const jbytecode::core::Type& type);
    void        store(int varIndex, const jbytecode::core::Type& type);
    void        astore(const jbytecode::core::Type& type);
    void        pop();
    void        pop2();
    void        dup();
    void        dup2();
    void        dupX1();
    void        dupX2();
    void        dup2X1();
    void        dup2X2();
    void        swap();
    void        add(const jbytecode::core::Type& type);
    void        sub(const jbytecode::core::Type& type);
    void        mul(const jbytecode::core::Type& type);
    void        div(const jbytecode::core::Type& type);
    void        rem(const jbytecode::core::Type& type);
    void        neg(const jbytecode::core::Type& type);
    void        shl(const jbytecode::core::Type& type);
    void        shr(const jbytecode::core::Type& type);
    void        ushr(const jbytecode::core::Type& type);
    void        and_(const jbytecode::core::Type& type);
    void        or_(const jbytecode::core::Type& type);
    void        xor_(const jbytecode::core::Type& type);
    void        iinc(int varIndex, int increment);
    void        cast(const jbytecode::core::Type& from, const jbytecode::core::Type& to);
    static void cast(jbytecode::core::MethodVisitor* methodVisitor,
                     const jbytecode::core::Type& from, const jbytecode::core::Type& to);
    void        lcmp();
    void        cmpl(const jbytecode::core::Type& type);
    void        cmpg(const jbytecode::core::Type& type);
    void        ifeq(jbytecode::core::Label* label);
    void        ifne(jbytecode::core::Label* label);
    void        iflt(jbytecode::core::Label* label);
    void        ifge(jbytecode::core::Label* label);
    void        ifgt(jbytecode::core::Label* label);
    void        ifle(jbytecode::core::Label* label);
    void        ificmpeq(jbytecode::core::Label* label);
    void        ificmpne(jbytecode::core::Label* label);
    void        ificmplt(jbytecode::core::Label* label);
    void        ificmpge(jbytecode::core::Label* label);
    void        ificmpgt(jbytecode::core::Label* label);
    void        ificmple(jbytecode::core::Label* label);
    void        ifacmpeq(jbytecode::core::Label* label);
    void        ifacmpne(jbytecode::core::Label* label);
    void        goTo(jbytecode::core::Label* label);
    void        jsr(jbytecode::core::Label* label);
    void        ret(int varIndex);
    void        tableswitch(int min, int max, jbytecode::core::Label* dflt,
                            const std::vector<jbytecode::core::Label*>& labels);
    void        lookupswitch(jbytecode::core::Label* dflt, const std::vector<int>& keys,
                             const std::vector<jbytecode::core::Label*>& labels);
    void        areturn(const jbytecode::core::Type& type);
    void        getstatic(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                          ca::str::Utf8StringRef descriptor);
    void        putstatic(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                          ca::str::Utf8StringRef descriptor);
    void        getfield(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor);
    void        putfield(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor);
    void        invokevirtual(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                              ca::str::Utf8StringRef descriptor);
    void        invokevirtual(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                              ca::str::Utf8StringRef descriptor, bool isInterface);
    void        invokespecial(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                              ca::str::Utf8StringRef descriptor);
    void        invokespecial(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                              ca::str::Utf8StringRef descriptor, bool isInterface);
    void        invokestatic(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                             ca::str::Utf8StringRef descriptor);
    void        invokestatic(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                             ca::str::Utf8StringRef descriptor, bool isInterface);
    void        invokeinterface(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                ca::str::Utf8StringRef descriptor);
    void        invokedynamic(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                              const jbytecode::core::Handle&    bootstrapMethodHandle,
                              const std::vector<ca::core::Any>& bootstrapMethodArguments);
    void        anew(const jbytecode::core::Type& type);
    void        newarray(const jbytecode::core::Type& type);
    static void newarray(jbytecode::core::MethodVisitor* methodVisitor,
                         const jbytecode::core::Type&    type);
    void        arraylength();
    void        athrow();
    void        checkcast(const jbytecode::core::Type& type);
    void        instanceOf(const jbytecode::core::Type& type);
    void        monitorenter();
    void        monitorexit();
    void        multianewarray(ca::str::Utf8StringRef descriptor, int numDimensions);
    void        ifnull(jbytecode::core::Label* label);
    void        ifnonnull(jbytecode::core::Label* label);
    void        mark(jbytecode::core::Label* label);
};

}   // namespace jbytecode::commons
