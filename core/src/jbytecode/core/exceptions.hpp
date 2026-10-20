/// @file        exceptions.hpp
/// @brief       core 写出侧的容量与帧计算异常类型。对标 ASM 的同名异常。
///
/// 当 ClassWriter 序列化结果超出 class 文件格式硬限制，或 COMPUTE_FRAMES 不收敛时
/// 抛出。异常类极薄（仅携带诊断字段），按 C++ 惯例保留 header-only 内联构造，
/// 不下沉到 .cpp。解析侧的畸形输入统一抛 std::invalid_argument，不在此文件定义。
#pragma once

#include <string>
#include <stdexcept>

namespace jbytecode::core {

/// @brief 常量池条目数超过 65535（u2 上限）时抛出。对标 ASM ClassTooLargeException。
class ClassTooLargeException : public std::length_error
{
public:
    /// @param className          触发的类内部名。
    /// @param constantPoolCount  实际常量池条目数。
    ClassTooLargeException(const std::string& className, int constantPoolCount)
        : std::length_error("Class too large: " + className)
        , className_(className)
        , constantPoolCount_(constantPoolCount)
    {}

    /// @return 触发异常的类内部名。
    const std::string& getClassName() const { return className_; }
    /// @return 越限的常量池条目数。
    int getConstantPoolCount() const { return constantPoolCount_; }

private:
    std::string className_;
    int         constantPoolCount_;
};

/// @brief 单个方法的 Code 长度超过 65535 字节时抛出。对标 ASM MethodTooLargeException。
class MethodTooLargeException : public std::length_error
{
public:
    /// @param className   方法所属类内部名。
    /// @param methodName  方法名。
    /// @param descriptor  方法描述符。
    /// @param codeSize    实际 Code 字节数。
    MethodTooLargeException(const std::string& className, const std::string& methodName,
                            const std::string& descriptor, int codeSize)
        : std::length_error("Method too large: " + className + "." + methodName + " " + descriptor)
        , className_(className)
        , methodName_(methodName)
        , descriptor_(descriptor)
        , codeSize_(codeSize)
    {}

    /// @return 方法所属类内部名。
    const std::string& getClassName() const { return className_; }
    /// @return 方法名。
    const std::string& getMethodName() const { return methodName_; }
    /// @return 方法描述符。
    const std::string& getDescriptor() const { return descriptor_; }
    /// @return 越限的 Code 字节数。
    int getCodeSize() const { return codeSize_; }

private:
    std::string className_;
    std::string methodName_;
    std::string descriptor_;
    int         codeSize_;
};

/// @brief COMPUTE_FRAMES 栈帧 fixpoint 不收敛（或迭代超限）时抛出。
///
/// ASM 原版 MethodWriter 的 fixpoint worklist 无显式迭代上限，依赖
/// getCommonSuperClass 的单调性保证收敛。本库的 ICommonSuperClassProvider
/// 在长继承链 + 多类引用合并场景下（如 obfuscator 的 controlflow flatten）
/// 可能反复触发 successorBlockChanged，导致 worklist 极慢甚至不收敛，
/// 表现为进程卡死。本异常把"卡死"变成"可观测的明确报错"，供调用方降级
/// （如跳过该类、原样保留字节码）。
class FrameComputationException : public std::runtime_error
{
public:
    /// @param className   方法所属类内部名。
    /// @param methodName  方法名。
    /// @param descriptor  方法描述符。
    /// @param iterations  触发超限时 worklist 已处理的迭代次数。
    FrameComputationException(const std::string& className, const std::string& methodName,
                              const std::string& descriptor, long long iterations)
        : std::runtime_error("Frame computation did not converge: " + className + "." + methodName +
                             " " + descriptor + " (iterations=" + std::to_string(iterations) + ")")
        , className_(className)
        , methodName_(methodName)
        , descriptor_(descriptor)
        , iterations_(iterations)
    {}

    /// @return 方法所属类内部名。
    const std::string& getClassName() const { return className_; }
    /// @return 方法名。
    const std::string& getMethodName() const { return methodName_; }
    /// @return 方法描述符。
    const std::string& getDescriptor() const { return descriptor_; }
    /// @return 触发超限时的迭代次数。
    long long getIterations() const { return iterations_; }

private:
    std::string className_;
    std::string methodName_;
    std::string descriptor_;
    long long   iterations_;
};

}   // namespace jbytecode::core
