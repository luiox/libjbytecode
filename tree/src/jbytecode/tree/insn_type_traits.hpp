#pragma once

#include "libca/core/tag_cast.hpp"
#include "jbytecode/tree/insn_list.hpp"

/// TypeOf 特化：AbstractInsnNode 子类 → getType() 枚举值
/// 这些类通过 getType() 枚举区分类型，无需 RTTI dynamic_cast。
/// 用 ca::core::tag_cast::cast<T>() 替代 dynamic_cast 做向下转型。

namespace ca::core::tag_cast {

template<>
struct TypeOf<jbytecode::tree::InsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::INSN;
};
template<>
struct TypeOf<jbytecode::tree::IntInsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::INT_INSN;
};
template<>
struct TypeOf<jbytecode::tree::VarInsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::VAR_INSN;
};
template<>
struct TypeOf<jbytecode::tree::TypeInsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::TYPE_INSN;
};
template<>
struct TypeOf<jbytecode::tree::FieldInsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::FIELD_INSN;
};
template<>
struct TypeOf<jbytecode::tree::MethodInsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::METHOD_INSN;
};
template<>
struct TypeOf<jbytecode::tree::InvokeDynamicInsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::INVOKE_DYNAMIC_INSN;
};
template<>
struct TypeOf<jbytecode::tree::JumpInsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::JUMP_INSN;
};
template<>
struct TypeOf<jbytecode::tree::LabelNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::LABEL;
};
template<>
struct TypeOf<jbytecode::tree::LdcInsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::LDC_INSN;
};
template<>
struct TypeOf<jbytecode::tree::IincInsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::IINC_INSN;
};
template<>
struct TypeOf<jbytecode::tree::TableSwitchInsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::TABLESWITCH_INSN;
};
template<>
struct TypeOf<jbytecode::tree::LookupSwitchInsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::LOOKUPSWITCH_INSN;
};
template<>
struct TypeOf<jbytecode::tree::MultiANewArrayInsnNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::MULTIANEWARRAY_INSN;
};
template<>
struct TypeOf<jbytecode::tree::FrameNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::FRAME;
};
template<>
struct TypeOf<jbytecode::tree::LineNumberNode>
{
    static constexpr int value = jbytecode::tree::AbstractInsnNode::LINE;
};

}   // namespace ca::core::tag_cast
