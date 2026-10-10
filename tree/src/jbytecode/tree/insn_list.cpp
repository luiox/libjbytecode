#include "jbytecode/tree/insn_list.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/visitors.hpp"
#include "nodes.hpp"

using namespace jbytecode::tree;
using namespace jbytecode::core;


// ---- B8 clone 协议（clone(arena, clonedLabels)）----

namespace jbytecode::tree {

Label* resolveCloneLabel(garena::Arena& arena, std::unordered_map<Label*, Label*>& map,
                         Label* label)
{
    if (label == nullptr) return nullptr;
    // 目标 arena 产物：归属盖章直返（O(1)），同域克隆不换身份。
    if (label->owner() == &arena) return label;
    auto it = map.find(label);
    if (it != map.end()) return it->second;
    // 外来 Label：拷贝进目标 arena（身份换新，控制流/调试元数据保留）。
    auto* copy           = arena.create<Label>();
    copy->setOwner(&arena);
    copy->bytecodeOffset = label->bytecodeOffset;
    copy->line           = label->line;
    copy->flags          = label->flags;
    map.emplace(label, copy);
    return copy;
}

}   // namespace jbytecode::tree


void AbstractInsnNode::acceptAnnotations(MethodVisitor* mv) const
{
    for (auto* ta : visibleTypeAnnotations) {
        if (ta) {
            AnnotationVisitor* av =
                mv->visitInsnAnnotation(ta->typeRef, ta->typePath, ta->desc, true);
            if (av) ta->accept(av);
        }
    }
    for (auto* ta : invisibleTypeAnnotations) {
        if (ta) {
            AnnotationVisitor* av =
                mv->visitInsnAnnotation(ta->typeRef, ta->typePath, ta->desc, false);
            if (av) ta->accept(av);
        }
    }
}

// ============================================================
// InsnNode
// ============================================================

void InsnNode::accept(MethodVisitor* mv) const
{
    mv->visitInsn(opcode);
}


AbstractInsnNode*
InsnNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>&) const
{
    return arena.create<InsnNode>(opcode);
}

// ============================================================
// IntInsnNode
// ============================================================

void IntInsnNode::accept(MethodVisitor* mv) const
{
    mv->visitIntInsn(opcode, operand);
}


AbstractInsnNode*
IntInsnNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>&) const
{
    return arena.create<IntInsnNode>(opcode, operand);
}

// ============================================================
// VarInsnNode
// ============================================================

void VarInsnNode::accept(MethodVisitor* mv) const
{
    mv->visitVarInsn(opcode, var);
}


AbstractInsnNode*
VarInsnNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>&) const
{
    return arena.create<VarInsnNode>(opcode, var);
}

// ============================================================
// TypeInsnNode
// ============================================================

void TypeInsnNode::accept(MethodVisitor* mv) const
{
    mv->visitTypeInsn(opcode, desc);
}


AbstractInsnNode*
TypeInsnNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>&) const
{
    return arena.create<TypeInsnNode>(opcode, desc);
}

// ============================================================
// FieldInsnNode
// ============================================================

void FieldInsnNode::accept(MethodVisitor* mv) const
{
    mv->visitFieldInsn(opcode, owner, name, desc);
}


AbstractInsnNode*
FieldInsnNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>&) const
{
    return arena.create<FieldInsnNode>(opcode, owner, name, desc);
}

// ============================================================
// MethodInsnNode
// ============================================================

void MethodInsnNode::accept(MethodVisitor* mv) const
{
    mv->visitMethodInsn(opcode, owner, name, desc, isInterface);
}


AbstractInsnNode*
MethodInsnNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>&) const
{
    return arena.create<MethodInsnNode>(opcode, owner, name, desc, isInterface);
}

// ============================================================
// InvokeDynamicInsnNode
// ============================================================

void InvokeDynamicInsnNode::accept(MethodVisitor* mv) const
{
    mv->visitInvokeDynamicInsn(name, desc, bsm, bsmArgs);
}


AbstractInsnNode*
InvokeDynamicInsnNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>&) const
{
    return arena.create<InvokeDynamicInsnNode>(name, desc, bsm, bsmArgs);
}

// ============================================================
// JumpInsnNode
// ============================================================

void JumpInsnNode::accept(MethodVisitor* mv) const
{
    mv->visitJumpInsn(opcode, label);
}


AbstractInsnNode*
JumpInsnNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>& clonedLabels) const
{
    return arena.create<JumpInsnNode>(opcode, resolveCloneLabel(arena, clonedLabels, label));
}

// ============================================================
// LabelNode
// ============================================================


LabelNode::LabelNode(Label* label)
    : AbstractInsnNode(-1)
    , label(label)
{}

LabelNode::~LabelNode() {}

void LabelNode::accept(MethodVisitor* mv) const
{
    mv->visitLabel(label);
}


AbstractInsnNode*
LabelNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>& clonedLabels) const
{
    // 同上（ASM getLabelNode 语义），Label 归宿改为目标 arena（B8）。
    return arena.create<LabelNode>(resolveCloneLabel(arena, clonedLabels, label));
}

// ============================================================
// LdcInsnNode
// ============================================================

void LdcInsnNode::accept(MethodVisitor* mv) const
{
    mv->visitLdcInsn(cst);
}


AbstractInsnNode*
LdcInsnNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>&) const
{
    return arena.create<LdcInsnNode>(cst);
}

// ============================================================
// IincInsnNode
// ============================================================

void IincInsnNode::accept(MethodVisitor* mv) const
{
    mv->visitIincInsn(var, incr);
}


AbstractInsnNode*
IincInsnNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>&) const
{
    return arena.create<IincInsnNode>(var, incr);
}

// ============================================================
// TableSwitchInsnNode
// ============================================================

void TableSwitchInsnNode::accept(MethodVisitor* mv) const
{
    mv->visitTableSwitchInsn(min, max, dflt, labels);
}


AbstractInsnNode* TableSwitchInsnNode::clone(garena::Arena&                    arena,
                                             std::unordered_map<Label*, Label*>& clonedLabels) const
{
    std::vector<Label*> newLabels;
    for (auto* l : labels) {
        newLabels.push_back(resolveCloneLabel(arena, clonedLabels, l));
    }
    return arena.create<TableSwitchInsnNode>(
        min, max, resolveCloneLabel(arena, clonedLabels, dflt), newLabels);
}

// ============================================================
// LookupSwitchInsnNode
// ============================================================

void LookupSwitchInsnNode::accept(MethodVisitor* mv) const
{
    mv->visitLookupSwitchInsn(dflt, keys, labels);
}

AbstractInsnNode* LookupSwitchInsnNode::clone(garena::Arena&                    arena,
                                              std::unordered_map<Label*, Label*>& clonedLabels) const
{
    std::vector<Label*> newLabels;
    for (auto* l : labels) {
        newLabels.push_back(resolveCloneLabel(arena, clonedLabels, l));
    }
    return arena.create<LookupSwitchInsnNode>(
        resolveCloneLabel(arena, clonedLabels, dflt), keys, newLabels);
}

// ============================================================
// MultiANewArrayInsnNode
// ============================================================

void MultiANewArrayInsnNode::accept(MethodVisitor* mv) const
{
    mv->visitMultiANewArrayInsn(desc, dims);
}


AbstractInsnNode*
MultiANewArrayInsnNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>&) const
{
    return arena.create<MultiANewArrayInsnNode>(desc, dims);
}

// ============================================================
// FrameNode
// ============================================================

void FrameNode::accept(MethodVisitor* mv) const
{
    mv->visitFrame(frameType,
                   static_cast<ca::i32>(local.size()),
                   local,
                   static_cast<ca::i32>(stack.size()),
                   stack);
}


AbstractInsnNode*
FrameNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>& clonedLabels) const
{
    // B8：展开帧 local/stack 中 ITEM_Uninitialized 形态的 Label* 槽与指令侧携带的
    // Label 同规，必须经 resolveCloneLabel 收编进目标 arena（visitFrame 收编在
    // clone 侧的对偶）。否则跨方法克隆后，源方法 arena 先亡即槽内悬垂——
    // 与 reader labelArena_ 悬垂同族的隐患（#994 审查 P2）。值类型槽原值拷贝。
    auto adoptCloneTypes = [&arena, &clonedLabels](const std::vector<ca::core::Any>& src) {
        std::vector<ca::core::Any> out;
        out.reserve(src.size());
        for (const auto& slot : src) {
            if (auto* p = slot.as<jbytecode::core::Label*>()) {
                out.emplace_back(resolveCloneLabel(arena, clonedLabels, *p));
            }
            else if (auto* cp = slot.as<const jbytecode::core::Label*>()) {
                out.emplace_back(resolveCloneLabel(
                    arena, clonedLabels, const_cast<jbytecode::core::Label*>(*cp)));
            }
            else {
                out.push_back(slot);
            }
        }
        return out;
    };
    auto* cloned = arena.create<FrameNode>(frameType, adoptCloneTypes(local), adoptCloneTypes(stack));
#ifndef NDEBUG
    // 防御：克隆完成后槽内 Label 必须归属目标 arena（树内 Label 所有权契约 B8）。
    for (const auto* slots : {&cloned->local, &cloned->stack}) {
        for (const auto& slot : *slots) {
            if (auto* p = slot.as<jbytecode::core::Label*>()) {
                assert((*p)->owner() == &arena &&
                       "FrameNode::clone: frame slot label not owned by target arena");
            }
        }
    }
#endif
    return cloned;
}

// ============================================================
// LineNumberNode
// ============================================================

void LineNumberNode::accept(MethodVisitor* mv) const
{
    mv->visitLineNumber(line, start);
}


AbstractInsnNode*
LineNumberNode::clone(garena::Arena& arena, std::unordered_map<Label*, Label*>& clonedLabels) const
{
    // B8：统一走 resolveCloneLabel（未命中即建 arena 副本），不再回退引用原 label，
    // 保证克隆体内携带的 Label 全部归属目标 arena。
    return arena.create<LineNumberNode>(line, resolveCloneLabel(arena, clonedLabels, start));
}

// ============================================================
// InsnList
// ============================================================

unsigned long long InsnList::layoutProbeSizeof()
{
    // 本体 TU（随 libjbytecode 静态库编译）视角的 sizeof。与消费 TU 的本地
    // sizeof(InsnList) 对比即可检出混链布局漂移（#1065：NDEBUG 门控成员时代，
    // NDEBUG 库 TU=48B、去 NDEBUG 测试 TU=56B，size_ 偏移 16↔24 错位读垃圾）。
    return static_cast<unsigned long long>(sizeof(InsnList));
}

InsnList::InsnList(InsnList&& other) noexcept
    : first_(other.first_)
    , last_(other.last_)
    , size_(other.size_)
    , ownerArena_(other.ownerArena_)
    , cacheNode_(other.cacheNode_)
    , cacheIndex_(other.cacheIndex_)
{
    other.first_      = nullptr;
    other.last_       = nullptr;
    other.size_       = 0;
    other.ownerArena_ = nullptr;
    other.cacheNode_  = nullptr;
    other.cacheIndex_ = -1;
}

InsnList& InsnList::operator=(InsnList&& other) noexcept
{
    if (this != &other) {
        clear();
        first_            = other.first_;
        last_             = other.last_;
        size_             = other.size_;
        ownerArena_       = other.ownerArena_;
        cacheNode_        = other.cacheNode_;
        cacheIndex_       = other.cacheIndex_;
        other.first_      = nullptr;
        other.last_       = nullptr;
        other.size_       = 0;
        other.ownerArena_ = nullptr;
        other.cacheNode_  = nullptr;
        other.cacheIndex_ = -1;
    }
    return *this;
}

AbstractInsnNode* InsnList::get(int index) const
{
    if (index < 0 || index >= size_) return nullptr;
    if (cacheNode_ && cacheIndex_ >= 0) {
        int diff = index - cacheIndex_;
        if (diff >= 0 && diff < 10) {
            auto* n = cacheNode_;
            for (int i = 0; i < diff; i++) n = n->nextInsn;
            cacheNode_  = n;
            cacheIndex_ = index;
            return n;
        }
        if (diff < 0 && -diff < 10) {
            auto* n = cacheNode_;
            for (int i = 0; i < -diff; i++) n = n->previousInsn;
            cacheNode_  = n;
            cacheIndex_ = index;
            return n;
        }
    }
    auto* n = first_;
    for (int i = 0; i < index; i++) n = n->nextInsn;
    cacheNode_  = n;
    cacheIndex_ = index;
    return n;
}

int InsnList::indexOf(AbstractInsnNode* node) const
{
    auto* n   = first_;
    int   idx = 0;
    for (; n && n != node; n = n->nextInsn) idx++;
    return n == node ? idx : -1;
}

bool InsnList::contains(AbstractInsnNode* node) const
{
    return indexOf(node) >= 0;
}

void InsnList::add(AbstractInsnNode* node)
{
    node->previousInsn = last_;
    node->nextInsn     = nullptr;
    node->index        = size_;
    if (last_)
        last_->nextInsn = node;
    else
        first_ = node;
    last_ = node;
    size_++;
    cacheNode_  = node;
    cacheIndex_ = size_ - 1;
}

void InsnList::insert(AbstractInsnNode* node)
{
    insert(first_, node);
}

void InsnList::insert(AbstractInsnNode* before, AbstractInsnNode* node)
{
    if (!before || before == first_) {
        node->previousInsn = nullptr;
        node->nextInsn     = first_;
        if (first_) first_->previousInsn = node;
        first_ = node;
        if (!last_) last_ = node;
    }
    else {
        node->previousInsn             = before->previousInsn;
        node->nextInsn                 = before;
        before->previousInsn->nextInsn = node;
        before->previousInsn           = node;
    }
    node->index = -1;
    cacheNode_  = nullptr;
    cacheIndex_ = -1;
    size_++;
}

void InsnList::insertBefore(AbstractInsnNode* where, AbstractInsnNode* node)
{
    insert(where, node);
}

void InsnList::set(AbstractInsnNode* oldNode, AbstractInsnNode* newNode)
{
#ifndef NDEBUG
    checkNoActiveIterator();   // set 会 delete 旧节点：迭代中调用当前节点即 UAF（safety.md）
#endif
    newNode->previousInsn = oldNode->previousInsn;
    newNode->nextInsn     = oldNode->nextInsn;
    newNode->index        = oldNode->index;
    if (oldNode->previousInsn)
        oldNode->previousInsn->nextInsn = newNode;
    else
        first_ = newNode;
    if (oldNode->nextInsn)
        oldNode->nextInsn->previousInsn = newNode;
    else
        last_ = newNode;
    cacheNode_  = nullptr;
    cacheIndex_ = -1;
    disposeNode(oldNode);   ///< arena 节点只摘链（arena reset 统一析构），堆节点照旧 delete
}

void InsnList::removeFromList(AbstractInsnNode* node)
{
    if (node->previousInsn)
        node->previousInsn->nextInsn = node->nextInsn;
    else
        first_ = node->nextInsn;
    if (node->nextInsn)
        node->nextInsn->previousInsn = node->previousInsn;
    else
        last_ = node->previousInsn;
    node->nextInsn     = nullptr;
    node->previousInsn = nullptr;
    node->index        = -1;   // 摘下后回到孤立态：可被 InsnBuilder::add 的孤立性断言接受
    size_--;
    cacheNode_  = nullptr;
    cacheIndex_ = -1;
}

void InsnList::remove(AbstractInsnNode* node)
{
    removeFromList(node);
}

void InsnList::removeAndDelete(AbstractInsnNode* node)
{
#ifndef NDEBUG
    checkNoActiveIterator();   // 删除型操作：与 set 同规（safety.md 迭代器失效表）
#endif
    removeFromList(node);
    disposeNode(node);
}

void InsnList::clear()
{
#ifndef NDEBUG
    checkNoActiveIterator();   // clear 析构堆节点：迭代中调用必 UAF（safety.md）
#endif
    auto* n = first_;
    while (n) {
        auto* next = n->nextInsn;
        disposeNode(n);
        n = next;
    }
    first_ = last_ = nullptr;
    size_          = 0;
    cacheNode_     = nullptr;
    cacheIndex_    = -1;
}

void InsnList::accept(MethodVisitor* mv) const
{
    for (auto* n = first_; n; n = n->nextInsn) n->accept(mv);
}

std::vector<AbstractInsnNode*> InsnList::toArray() const
{
    std::vector<AbstractInsnNode*> arr;
    arr.reserve(size_);
    for (auto* n = first_; n; n = n->nextInsn) arr.push_back(n);
    return arr;
}

void InsnList::add(InsnList& other)
{
    if (other.isEmpty()) return;
    AbstractInsnNode* n = other.first_;
    while (n) {
        AbstractInsnNode* next = n->nextInsn;
        // Interleave: save next before add() clears it
        add(n);
        n = next;
    }
    other.first_ = other.last_ = nullptr;
    other.size_                = 0;
}

void InsnList::insert(InsnList& other)
{
    add(other);
}

void InsnList::insert(AbstractInsnNode* before, InsnList& other)
{
    if (other.isEmpty()) return;
    AbstractInsnNode* n = other.first_;
    while (n) {
        AbstractInsnNode* next = n->nextInsn;
        insert(before, n);
        n = next;
    }
    other.first_ = other.last_ = nullptr;
    other.size_                = 0;
}

void InsnList::resetLabels()
{
    for (auto* n = first_; n; n = n->nextInsn) {
        if (n->getType() == AbstractInsnNode::LABEL) {
            auto* ln = static_cast<LabelNode*>(n);
            if (ln->label) {
                // ClassReader 创建的 Label 带有源方法中的已解析偏移。Tree 再次 accept 时，
                // 这些偏移不再属于新 MethodWriter；保留它们会让远距离前向跳转被按 short 写出。
                ln->label->bytecodeOffset = -1;
                ln->label->flags &= ~Label::FLAG_RESOLVED;
                ln->label->forwardReferences.clear();
            }
        }
    }
}
