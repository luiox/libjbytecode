#pragma once

#include "libca/core/any.hpp"
#include <cstdint>
#include <string>
#include <vector>

#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/commons/instruction_adapter.hpp"
#include "garena/arena.hpp"
#include "jbytecode/commons/local_variables_sorter.hpp"
#include "jbytecode/commons/method.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class TableSwitchGenerator
{
public:
    virtual ~TableSwitchGenerator()                                 = default;
    virtual void generateCase(int key, jbytecode::core::Label* end) = 0;
    virtual void generateDefault()                                  = 0;
};

class GeneratorAdapter : public LocalVariablesSorter
{
public:
    static constexpr int ADD  = jbytecode::core::Opcodes::IADD;
    static constexpr int SUB  = jbytecode::core::Opcodes::ISUB;
    static constexpr int MUL  = jbytecode::core::Opcodes::IMUL;
    static constexpr int DIV  = jbytecode::core::Opcodes::IDIV;
    static constexpr int REM  = jbytecode::core::Opcodes::IREM;
    static constexpr int NEG  = jbytecode::core::Opcodes::INEG;
    static constexpr int SHL  = jbytecode::core::Opcodes::ISHL;
    static constexpr int SHR  = jbytecode::core::Opcodes::ISHR;
    static constexpr int USHR = jbytecode::core::Opcodes::IUSHR;
    static constexpr int AND  = jbytecode::core::Opcodes::IAND;
    static constexpr int OR   = jbytecode::core::Opcodes::IOR;
    static constexpr int XOR  = jbytecode::core::Opcodes::IXOR;
    static constexpr int EQ   = jbytecode::core::Opcodes::IFEQ;
    static constexpr int NE   = jbytecode::core::Opcodes::IFNE;
    static constexpr int LT   = jbytecode::core::Opcodes::IFLT;
    static constexpr int GE   = jbytecode::core::Opcodes::IFGE;
    static constexpr int GT   = jbytecode::core::Opcodes::IFGT;
    static constexpr int LE   = jbytecode::core::Opcodes::IFLE;

    static const jbytecode::core::Type BYTE_TYPE;
    static const jbytecode::core::Type BOOLEAN_TYPE;
    static const jbytecode::core::Type SHORT_TYPE;
    static const jbytecode::core::Type CHARACTER_TYPE;
    static const jbytecode::core::Type INTEGER_TYPE;
    static const jbytecode::core::Type FLOAT_TYPE;
    static const jbytecode::core::Type LONG_TYPE;
    static const jbytecode::core::Type DOUBLE_TYPE;
    static const jbytecode::core::Type NUMBER_TYPE;
    static const jbytecode::core::Type OBJECT_TYPE;

    static const Method BOOLEAN_VALUE;
    static const Method CHAR_VALUE;
    static const Method INT_VALUE;
    static const Method FLOAT_VALUE;
    static const Method LONG_VALUE;
    static const Method DOUBLE_VALUE;

    GeneratorAdapter(jbytecode::core::MethodVisitor* mv, int access, ca::str::Utf8StringRef name,
                     ca::str::Utf8StringRef descriptor);

    GeneratorAdapter(int access, const Method& method, jbytecode::core::MethodVisitor* mv);

    GeneratorAdapter(int access, const Method& method, ca::str::Utf8StringRef signature,
                     const std::vector<jbytecode::core::Type>& exceptions,
                     jbytecode::core::ClassVisitor*            classVisitor);

    int                                getAccess() const { return access_; }
    ca::str::Utf8StringRef             getName() const { return name_.ref(); }
    jbytecode::core::Type              getReturnType() const { return returnType_; }
    std::vector<jbytecode::core::Type> getArgumentTypes() const { return argumentTypes_; }

    void push(bool value);
    void push(int value);
    void push(long value);
    void push(float value);
    void push(double value);
    void push(ca::str::Utf8StringRef value);
    void push(const jbytecode::core::Type& value);
    void push(const jbytecode::core::Handle& handle);
    void push(const jbytecode::core::ConstantDynamic& constantDynamic);

    void loadThis();
    void loadArg(int arg);
    void loadArgs(int arg, int count);
    void loadArgs();
    void loadArgArray();
    void storeArg(int arg);

    jbytecode::core::Type getLocalType(int local);
    void                  setLocalType(int local, const jbytecode::core::Type& type);
    void                  loadLocal(int local);
    void                  loadLocal(int local, const jbytecode::core::Type& type);
    void                  storeLocal(int local);
    void                  storeLocal(int local, const jbytecode::core::Type& type);

    void arrayLoad(const jbytecode::core::Type& type);
    void arrayStore(const jbytecode::core::Type& type);

    void pop();
    void pop2();
    void dup();
    void dup2();
    void dupX1();
    void dupX2();
    void dup2X1();
    void dup2X2();
    void swap();
    void swap(const jbytecode::core::Type& prev, const jbytecode::core::Type& type);

    void math(int op, const jbytecode::core::Type& type);
    void not_();
    void iinc(int local, int amount);

    void cast(const jbytecode::core::Type& from, const jbytecode::core::Type& to);

    void box(const jbytecode::core::Type& type);
    void valueOf(const jbytecode::core::Type& type);
    void unbox(const jbytecode::core::Type& type);

    jbytecode::core::Label* newLabel();
    void                    mark(jbytecode::core::Label* label);
    jbytecode::core::Label* mark();
    void ifCmp(const jbytecode::core::Type& type, int mode, jbytecode::core::Label* label);
    void ifICmp(int mode, jbytecode::core::Label* label);
    void ifZCmp(int mode, jbytecode::core::Label* label);
    void ifNull(jbytecode::core::Label* label);
    void ifNonNull(jbytecode::core::Label* label);
    void goTo(jbytecode::core::Label* label);
    void ret(int local);
    void tableSwitch(const std::vector<int>& keys, TableSwitchGenerator* generator);
    void tableSwitch(const std::vector<int>& keys, TableSwitchGenerator* generator, bool useTable);

    void returnValue();

    void getStatic(const jbytecode::core::Type& owner, ca::str::Utf8StringRef name,
                   const jbytecode::core::Type& type);
    void putStatic(const jbytecode::core::Type& owner, ca::str::Utf8StringRef name,
                   const jbytecode::core::Type& type);
    void getField(const jbytecode::core::Type& owner, ca::str::Utf8StringRef name,
                  const jbytecode::core::Type& type);
    void putField(const jbytecode::core::Type& owner, ca::str::Utf8StringRef name,
                  const jbytecode::core::Type& type);

    void invokeVirtual(const jbytecode::core::Type& owner, const Method& method);
    void invokeConstructor(const jbytecode::core::Type& type, const Method& method);
    void invokeStatic(const jbytecode::core::Type& owner, const Method& method);
    void invokeInterface(const jbytecode::core::Type& owner, const Method& method);
    void invokeDynamic(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                       const jbytecode::core::Handle&    bootstrapMethodHandle,
                       const std::vector<ca::core::Any>& bootstrapMethodArguments);

    void newInstance(const jbytecode::core::Type& type);
    void newArray(const jbytecode::core::Type& type);

    void arrayLength();
    void throwException();
    void throwException(const jbytecode::core::Type& type, ca::str::Utf8StringRef message);
    void checkCast(const jbytecode::core::Type& type);
    void instanceOf(const jbytecode::core::Type& type);
    void monitorEnter();
    void monitorExit();

    void endMethod();
    void catchException(jbytecode::core::Label* start, jbytecode::core::Label* end,
                        const jbytecode::core::Type& exception);

protected:
    GeneratorAdapter(int api, jbytecode::core::MethodVisitor* mv, int access,
                     ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor);

private:
    /// B8：本 adapter 产生的 Label 归宿（scratch）。外来堆 Label 被下游
    /// MethodNode visitor 边界 canonicalize 拷贝收编后原对象无人释放
    /// （旧池时代由池回收），arena 化使其寿命 = adapter 生命周期。
    garena::Arena labelArena_;

    static jbytecode::core::Type getBoxedType(const jbytecode::core::Type& type);

    int  getArgIndex(int arg);
    void loadInsn(const jbytecode::core::Type& type, int index);
    void storeInsn(const jbytecode::core::Type& type, int index);
    void fieldInsn(int opcode, const jbytecode::core::Type& ownerType, ca::str::Utf8StringRef name,
                   const jbytecode::core::Type& fieldType);
    void invokeInsn(int opcode, const jbytecode::core::Type& type, const Method& method,
                    bool isInterface);
    void typeInsn(int opcode, const jbytecode::core::Type& type);

    int                                access_;
    ca::str::Utf8String                name_;
    jbytecode::core::Type              returnType_;
    std::vector<jbytecode::core::Type> argumentTypes_;
    std::vector<jbytecode::core::Type> localTypes_;

    static const ca::str::Utf8String CLASS_DESCRIPTOR;
};

}   // namespace jbytecode::commons
