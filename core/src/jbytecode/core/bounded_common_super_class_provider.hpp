#pragma once

#include <stdexcept>

#include "jbytecode/core/frame.hpp"

namespace jbytecode::core {

/// @brief 装饰器预算超限异常。
///
/// 由 BoundedCommonSuperClassProvider 在调用计数超过预算时抛出，MethodWriter
/// 将其转换为 FrameComputationException（带类名/方法名/迭代数），供调用方
/// 按既有 frame_not_converged 路径降级（SKIP），而不是进程卡死。
class CommonSuperClassBudgetExceeded : public std::runtime_error
{
public:
    CommonSuperClassBudgetExceeded(long long calls, long long budget)
        : std::runtime_error("getCommonSuperClass call budget exceeded: " +
                             std::to_string(calls) + " > " + std::to_string(budget))
        , calls_(calls)
        , budget_(budget)
    {}

    long long getCalls() const { return calls_; }
    long long getBudget() const { return budget_; }

private:
    long long calls_   = 0;
    long long budget_  = 0;
};

/// @brief 带调用预算的 ICommonSuperClassProvider 装饰器。
///
/// @details
/// 把任意 provider 的 getCommonSuperClass 调用包上预算上限（默认 1 亿次）。
/// 用途：issue #575 的防御性兜底——即使某个 provider 实现存在继承环/非单调
/// 等死循环缺陷（如 A→B→A 继承环让上升遍历永不终止），也会在预算内抛出
/// CommonSuperClassBudgetExceeded，由 MethodWriter 转 FrameComputationException
/// 让调用方降级，而不是无限卡死。正常调用（合并缓存 + 环检测已修复的
/// provider）远低于该预算，不影响合法场景。
class BoundedCommonSuperClassProvider : public ICommonSuperClassProvider
{
public:
    explicit BoundedCommonSuperClassProvider(ICommonSuperClassProvider* inner,
                                             long long                  budget = 100000000)
        : inner_(inner)
        , budget_(budget)
    {}

    ca::str::Utf8StringRef getCommonSuperClass(const ca::str::Utf8StringRef& type1,
                                               const ca::str::Utf8StringRef& type2) override
    {
        if (++calls_ > budget_) throw CommonSuperClassBudgetExceeded(calls_, budget_);
        return inner_->getCommonSuperClass(type1, type2);
    }

    /// @brief 当前已消耗的调用计数。
    long long calls() const { return calls_; }

private:
    ICommonSuperClassProvider* inner_ = nullptr;
    long long                  budget_ = 100000000;
    long long                  calls_  = 0;
};

}   // namespace jbytecode::core
