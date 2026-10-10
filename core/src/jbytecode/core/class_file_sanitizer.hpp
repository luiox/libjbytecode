#pragma once

/// @file        class_file_sanitizer.hpp
/// @brief       classfile 输入净化面（issue #963 一期）：CP 引用语义校验，校验+拒绝。
///
///              ClassReader 已保证结构读取安全（magic/tag/checkRange，非法即抛
///              std::invalid_argument），但引用语义缺口由下游兜底：
///              - CP index 指错 tag 类型时静默产出空值/错类型值（#953 同族问题的
///                reader 侧形态，下游解引用/格式解析时才炸）；
///              - ConstantDynamic 经 bootstrap args 的环引用让 readConst 无界递归
///                （栈溢出不可捕获，memoization 在递归返回后才写入，挡不住环）；
///              - bootstrap_method_attr_index 越界是直接越界读。
///              本净化器在 reader 之前对原始 bytes 做一次只读扫描，把这些形态转成
///              带具体 CP index 的诊断，交由调用方拒绝（管线入口跳过该类并记录
///              faultReport，见 mbase ClassFileSanitizeGate）。
///
///              诊断规则只收录 JVMS 必然违规（越界/保留槽/空引用/tag 错配/环/长度
///              越界），对合法 classfile 零误报；ClassReader 自身的版本、魔数之外的
///              结构裁决权不变——净化器是带精确诊断的前置预检，不是第二套解析器。

#include <vector>

#include "libca/core/datatype.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::core {

/// 诊断类别（测试断言与日志过滤用）。
enum class SanitizeIssueKind : ca::u8
{
    BadHeader,                 ///< 不足 10 字节 / 魔数非 0xCAFEBABE
    BadCpTag,                  ///< CP tag 非法
    LengthOutOfRange,          ///< Utf8 长度 / 属性长度越过文件尾
    RefIndexOutOfRange,        ///< 引用 index 为 0 / 越界 / 指向 long-double 保留槽
    RefTagMismatch,            ///< 引用指向条目的 tag 与 JVMS 规定不符
    BadMethodHandleKind,       ///< MethodHandle reference_kind 不在 [1,9]
    BootstrapIndexOutOfRange,  ///< bootstrap_method_attr_index 越界 / 缺 BootstrapMethods 属性
    CpCycle,                   ///< CP 引用环（readConst 递归栈溢出向量）
};

/// 单条净化诊断。
struct SanitizeIssue
{
    SanitizeIssueKind   kind;
    ca::u32             cpIndex;  ///< 关联 CP 条目；非 CP 问题为 0
    ca::u32             offset;   ///< 字节偏移（定位原始 bytes）
    ca::str::Utf8String message;  ///< 人类可读详情，含具体 index/tag（ASCII）
};

/// @brief 对原始 classfile bytes 做引用语义校验。
///
/// 只读、无异常、无 arena 依赖；返回全部诊断，空 = 通过。诊断条数有上限
/// （防敌意样本产出海量诊断拖垮日志/报告），超限后停止追加。
std::vector<SanitizeIssue> sanitizeClassFile(const ca::u8* data, ca::i32 size);

/// @copydoc sanitizeClassFile(const ca::u8*, ca::i32)
std::vector<SanitizeIssue> sanitizeClassFile(const std::vector<ca::u8>& bytes);

/// @brief 拼接诊断为单行消息（faultReport detail / 异常 what() 用）。
///
/// 单行契约：PassFaultReport::Entry::toLine() 把 detail 放在同一行，诊断间以
/// "; " 连接；超过 maxIssues 条截断并注明余量。
ca::str::Utf8String joinSanitizeIssues(const std::vector<SanitizeIssue>& issues,
                                       ca::usize maxIssues = 8);

}   // namespace jbytecode::core
