#pragma once

#include <vector>
#include "libca/core/any.hpp"
#include <string>

#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/tree/insn_list.hpp"
#include "jbytecode/tree/insn_type_traits.hpp"
#include "jbytecode/analysis/interpreter.hpp"

namespace jbytecode::analysis {

class BasicValue : public Value
{
public:
    static const BasicValue UNINITIALIZED_VALUE;
    static const BasicValue INT_VALUE;
    static const BasicValue FLOAT_VALUE;
    static const BasicValue LONG_VALUE;
    static const BasicValue DOUBLE_VALUE;
    static const BasicValue REFERENCE_VALUE;
    static const BasicValue RETURNADDRESS_VALUE;

    static const jbytecode::core::Type REFERENCE_TYPE;

    BasicValue()
        : type_(nullptr)
    {}
    explicit BasicValue(const jbytecode::core::Type* type)
        : type_(type)
    {}

    const jbytecode::core::Type* getType() const { return type_; }

    int getSize() const override
    {
        if (type_ == &jbytecode::core::Type::LONG_TYPE ||
            type_ == &jbytecode::core::Type::DOUBLE_TYPE)
            return 2;
        return 1;
    }

    bool isReference() const
    {
        if (type_ == nullptr) return false;
        int sort = type_->getSort();
        return sort == jbytecode::core::Type::OBJECT || sort == jbytecode::core::Type::ARRAY;
    }

    bool equals(const BasicValue& other) const
    {
        if (type_ == nullptr) return other.type_ == nullptr;
        if (other.type_ == nullptr) return false;
        return *type_ == *other.type_;
    }

    bool operator==(const BasicValue& other) const { return equals(other); }
    bool operator!=(const BasicValue& other) const { return !equals(other); }

    std::string toString() const
    {
        if (this == &UNINITIALIZED_VALUE) return ".";
        if (this == &RETURNADDRESS_VALUE) return "A";
        if (this == &REFERENCE_VALUE) return "R";
        if (type_) {
            // #921 staticArena 退役：字节仅在本调用内瞬时消费（立即拷贝进返回的
            // std::string），确无 owner 上下文——按裁决①给显式函数级局部 arena，
            // 零驻留、零跨调用累积。
            ca::str::Utf8StringArena scratch;
            auto                     desc = type_->getDescriptor(scratch);
            return std::string(reinterpret_cast<const char*>(desc.data()), desc.byte_length());
        }
        return ".";
    }

private:
    const jbytecode::core::Type* type_;
};

class BasicInterpreter : public Interpreter<BasicValue>
{
public:
    static const jbytecode::core::Type NULL_TYPE;

    /// @deprecated 兜底构造(#921 批次1 起):ANEWARRAY 合成描述符的 intern 回落到
    /// 本解释器实例私有的 fallbackArena_(随解释器析构回收;进程级 staticArena 兜底池
    /// 已随 #921 批次2 整体删除)。AnalysisManager 驱动的链路一律用 BasicInterpreter(arena)。
    BasicInterpreter() = default;

    /// 注入构造(字符串线示范,总方案 §5.1):合成描述符 intern 进调用方作业级
    /// arena;AM 驱动链路传 am.arena()(同 #680 给 air_builder 的注入做法)。
    explicit BasicInterpreter(ca::str::Utf8StringArena& arena)
        : arena_(&arena)
    {}

    BasicValue newValue(const jbytecode::core::Type* type) override;
    /// Type::getType/getObjectType 按值返回：引用重载转发，避免取临时对象
    /// 地址（gcc address-of-rvalue 报错；newValue 不持有指针，语义等价）。
    BasicValue newValue(const jbytecode::core::Type& type) { return newValue(&type); }

    BasicValue newOperation(const jbytecode::tree::AbstractInsnNode* insn) override;

    BasicValue copyOperation(const jbytecode::tree::AbstractInsnNode* insn,
                             const BasicValue&                        value) override;

    BasicValue unaryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                              const BasicValue&                        value) override;

    BasicValue binaryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                               const BasicValue& value1, const BasicValue& value2) override;

    BasicValue ternaryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                                const BasicValue& value1, const BasicValue& value2,
                                const BasicValue& value3) override;

    BasicValue naryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                             const std::vector<BasicValue>&           values) override;

    void returnOperation(const jbytecode::tree::AbstractInsnNode* insn, const BasicValue& value,
                         const BasicValue& expected) override;

    BasicValue merge(const BasicValue& value1, const BasicValue& value2) override;

protected:
    /// 注入的作业级字符串池;为空时 intern 回落实例私有 fallbackArena_(仅兜底路径)。
    /// protected:派生 interpreter(如 nullability/super)可透传注入。
    ca::str::Utf8StringArena* arena_ = nullptr;

private:
    /// #921 staticArena 退役:未注入作业 arena 时的兜底 intern 池。实例级生命周期——
    /// 随本解释器析构整体回收,消除进程级无界累积。派生 interpreter(SuperVerifier/
    /// SuperInterpreter 等)从合成路径保留下来的 Type 视图因此活到解释器死亡点,
    /// 与既有契约「分析帧不得活过本解释器」一致(见 nullability_interpreter.hpp)。
    ca::str::Utf8StringArena fallbackArena_;
};

}   // namespace jbytecode::analysis
