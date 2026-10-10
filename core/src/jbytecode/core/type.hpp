// type.hpp
#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <cassert>
#include "jbytecode/core/opcodes.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

namespace jbytecode::core {

class Type
{
public:
    static constexpr int VOID     = 0;
    static constexpr int BOOLEAN  = 1;
    static constexpr int CHAR     = 2;
    static constexpr int BYTE     = 3;
    static constexpr int SHORT    = 4;
    static constexpr int INT      = 5;
    static constexpr int FLOAT    = 6;
    static constexpr int LONG     = 7;
    static constexpr int DOUBLE   = 8;
    static constexpr int ARRAY    = 9;
    static constexpr int OBJECT   = 10;
    static constexpr int METHOD   = 11;
    static constexpr int INTERNAL = 12;

    static const Type VOID_TYPE;
    static const Type BOOLEAN_TYPE;
    static const Type CHAR_TYPE;
    static const Type BYTE_TYPE;
    static const Type SHORT_TYPE;
    static const Type INT_TYPE;
    static const Type FLOAT_TYPE;
    static const Type LONG_TYPE;
    static const Type DOUBLE_TYPE;

    /// 进程级兜底池（原 staticArena，已随 #921 批次2 整体删除，#699/#921，总方案 §5.1）：
    /// getClassName()/getMethodType(ret,args) 无参重载与 remapper/basic_interpreter/
    /// analyzer_adapter 兜底已于批次1 退役；无参 getInternalName()/getDescriptor() 的
    /// 调用面随批次2 迁移完毕后，池本体与两个 @deprecated 无参重载一并删除。
    /// 现 intern 唯一入口是各方法的 `Utf8StringArena&` 注入重载（作业级 arena 必选）。
    /// 门禁：script/gates/gate_grep.sh 对本池的调用/定义形态作硬门禁（>0 即红）。

    static Type getType(const ca::str::Utf8StringRef& typeDescriptor);
    static Type getObjectType(const ca::str::Utf8StringRef& internalName);
    static Type getMethodType(const ca::str::Utf8StringRef& methodDescriptor);

    /// 注入版:拼接出的方法描述符 intern 进调用方作业级 arena(字符串线示范,#699)。
    static Type getMethodType(ca::str::Utf8StringArena&      arena,
                              const Type&                    returnType,
                              const std::vector<Type>&       argumentTypes);

    static std::vector<Type> getArgumentTypes(const ca::str::Utf8StringRef& methodDescriptor);
    static Type              getReturnType(const ca::str::Utf8StringRef& methodDescriptor);
    static int               getArgumentCount(const ca::str::Utf8StringRef& methodDescriptor);
    static int getArgumentsAndReturnSizes(const ca::str::Utf8StringRef& methodDescriptor);
    static ca::str::Utf8String getMethodDescriptor(const Type&              returnType,
                                                   const std::vector<Type>& argumentTypes);
    static ca::str::Utf8String getInternalName(const ca::str::Utf8StringRef& binaryName);

    int                    getSort() const;
    int                    getDimensions() const;
    Type                   getElementType() const;

    /// 注入版:合成/派生名称 intern 进调用方作业级 arena,生命周期随作业(字符串线示范,#699)。
    ca::str::Utf8StringRef getClassName(ca::str::Utf8StringArena& arena) const;
    ca::str::Utf8StringRef getInternalName(ca::str::Utf8StringArena& arena) const;
    ca::str::Utf8StringRef getDescriptor(ca::str::Utf8StringArena& arena) const;

    int                    getSize() const;
    int                    getArgumentCount() const;
    std::vector<Type>      getArgumentTypes() const;
    Type                   getReturnType() const;
    int                    getArgumentsAndReturnSizes() const;
    int                    getOpcode(int opcode) const;

    const ca::str::Utf8StringRef& valueRef() const { return value_; }

    bool        operator==(const Type& other) const;
    bool        operator!=(const Type& other) const;
    std::string toString() const;

    Type()
        : sort_(VOID)
        , value_()
    {}

    /// Create from arena-interned string.
    static Type fromArena(ca::str::Utf8StringArena&     arena,
                          const ca::str::Utf8StringRef& typeDescriptor);

private:
    int                    sort_;
    ca::str::Utf8StringRef value_;

    Type(int sort, const ca::str::Utf8StringRef& value)
        : sort_(sort)
        , value_(value)
    {}

    static Type   getTypeInternal(const ca::str::Utf8StringRef& desc, size_t begin, size_t end);
    static size_t getReturnTypeOffset(const ca::str::Utf8StringRef& methodDescriptor);
    void          appendDescriptor(ca::str::Utf8StringBuilder& out) const;
};

}   // namespace jbytecode::core
