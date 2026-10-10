#include "jbytecode/commons/jsr_inliner_adapter.hpp"

#include <stdexcept>

#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/tree/insn_type_traits.hpp"

using namespace jbytecode::commons;
using namespace jbytecode::core;
using namespace jbytecode::tree;

namespace {

LabelNode* findLabelNode(InsnList& insns, Label* label)
{
    for (auto* node : insns) {
        if (node->getType() == AbstractInsnNode::LABEL) {
            auto* ln = ca::core::tag_cast::cast<LabelNode*>(node);
            if (ln && ln->label == label) return ln;
        }
    }
    return nullptr;
}

/// Instantiation 节点级克隆映射换算到 Label 级后的取值（JSR 内联专用）。
/// 命中直返；未命中（畸形输入：跳转目标无对应 LabelNode）沿用旧协议
/// getClonedLabel 的兜底语义——在宿主方法 arena 新建副本并登记
/// （迁移前为堆副本，映射随后续 clone 调用各自重建而不共享）。
Label* clonedLabelOf(JSRInlinerAdapter&                   owner,
                     std::unordered_map<Label*, Label*>& labelMap,
                     Label*                              label)
{
    if (label == nullptr) return nullptr;
    auto it = labelMap.find(label);
    if (it != labelMap.end()) return it->second;
    auto* fresh = owner.createLabel();
    labelMap.emplace(label, fresh);
    return fresh;
}

}   // namespace

// ============================================================
// JSRInlinerAdapter
// ============================================================

JSRInlinerAdapter::JSRInlinerAdapter(MethodVisitor* mv, int access, ca::str::Utf8StringRef name,
                                     ca::str::Utf8StringRef                     descriptor,
                                     ca::str::Utf8StringRef                     signature,
                                     const std::vector<ca::str::Utf8StringRef>& exceptions)
    : MethodNode(access, name, descriptor, signature, exceptions)
{
    this->mv = mv;
}

void JSRInlinerAdapter::visitJumpInsn(int opcode, Label* label)
{
    MethodNode::visitJumpInsn(opcode, label);
    if (opcode == Opcodes::JSR) {
        auto* jumpNode = ca::core::tag_cast::cast<JumpInsnNode*>(instructions.getLast());
        if (jumpNode) {
            // Find or create the LabelNode for this label
            LabelNode* labelNode = nullptr;
            for (auto* node : instructions) {
                if (node->getType() == AbstractInsnNode::LABEL) {
                    auto* ln = ca::core::tag_cast::cast<LabelNode*>(node);
                    if (ln && ln->label == jumpNode->label) {
                        labelNode = ln;
                        break;
                    }
                }
            }
            if (labelNode && subroutinesInsns_.count(labelNode) == 0) {
                subroutinesInsns_[labelNode] = std::vector<bool>();
            }
        }
    }
}

void JSRInlinerAdapter::visitEnd()
{
    if (!subroutinesInsns_.empty()) {
        findSubroutinesInsns();
        emitCode();
    }
    if (mv) accept(mv);
}

void JSRInlinerAdapter::findSubroutinesInsns()
{
    std::vector<bool> visitedInsns(instructions.size(), false);
    findSubroutineInsns(0, mainSubroutineInsns_, visitedInsns);

    for (auto& entry : subroutinesInsns_) {
        LabelNode*         jsrLabelNode    = entry.first;
        std::vector<bool>& subroutineInsns = entry.second;
        int                idx             = instructions.indexOf(jsrLabelNode);
        if (idx >= 0) {
            findSubroutineInsns(idx, subroutineInsns, visitedInsns);
        }
    }
}

void JSRInlinerAdapter::findSubroutineInsns(int startInsnIndex, std::vector<bool>& subroutineInsns,
                                            std::vector<bool>& visitedInsns)
{
    findReachableInsns(startInsnIndex, subroutineInsns, visitedInsns);

    while (true) {
        bool applicableHandlerFound = false;
        for (auto* tcb : tryCatchBlocks) {
            int handlerIdx = instructions.indexOf(findLabelNode(instructions, tcb->handler));
            if (handlerIdx < 0 || (size_t)handlerIdx >= subroutineInsns.size()) continue;
            if (subroutineInsns[handlerIdx]) continue;

            int startIdx = instructions.indexOf(findLabelNode(instructions, tcb->start));
            int endIdx   = instructions.indexOf(findLabelNode(instructions, tcb->end));

            if (startIdx < 0 || endIdx < 0) continue;

            for (int i = startIdx; i < endIdx && (size_t)i < subroutineInsns.size(); i++) {
                if (subroutineInsns[i]) {
                    findReachableInsns(handlerIdx, subroutineInsns, visitedInsns);
                    applicableHandlerFound = true;
                    break;
                }
            }
        }
        if (!applicableHandlerFound) break;
    }
}

void JSRInlinerAdapter::findReachableInsns(int insnIndex, std::vector<bool>& subroutineInsns,
                                           std::vector<bool>& visitedInsns)
{
    int currentInsnIndex = insnIndex;
    while (currentInsnIndex >= 0 && (size_t)currentInsnIndex < instructions.size()) {
        if (subroutineInsns[currentInsnIndex]) return;
        subroutineInsns[currentInsnIndex] = true;

        if (visitedInsns[currentInsnIndex]) {
            sharedSubroutineInsns_[currentInsnIndex] = true;
        }
        visitedInsns[currentInsnIndex] = true;

        auto* insnNode = instructions.get(currentInsnIndex);
        if (insnNode->getType() == AbstractInsnNode::JUMP_INSN &&
            insnNode->getOpcode() != Opcodes::JSR) {
            auto* jumpNode = ca::core::tag_cast::cast<JumpInsnNode*>(insnNode);
            if (jumpNode) {
                LabelNode* targetLabel = findLabelNode(instructions, jumpNode->label);
                if (targetLabel) {
                    int targetIdx = instructions.indexOf(targetLabel);
                    if (targetIdx >= 0)
                        findReachableInsns(targetIdx, subroutineInsns, visitedInsns);
                }
            }
        }
        else if (insnNode->getType() == AbstractInsnNode::TABLESWITCH_INSN) {
            auto* ts = ca::core::tag_cast::cast<TableSwitchInsnNode*>(insnNode);
            if (ts) {
                LabelNode* dfltLabel = findLabelNode(instructions, ts->dflt);
                if (dfltLabel) {
                    int idx = instructions.indexOf(dfltLabel);
                    if (idx >= 0) findReachableInsns(idx, subroutineInsns, visitedInsns);
                }
            }
        }
        else if (insnNode->getType() == AbstractInsnNode::LOOKUPSWITCH_INSN) {
            auto* ls = ca::core::tag_cast::cast<LookupSwitchInsnNode*>(insnNode);
            if (ls) {
                LabelNode* dfltLabel = findLabelNode(instructions, ls->dflt);
                if (dfltLabel) {
                    int idx = instructions.indexOf(dfltLabel);
                    if (idx >= 0) findReachableInsns(idx, subroutineInsns, visitedInsns);
                }
            }
        }

        int opcode = insnNode->getOpcode();
        switch (opcode) {
        case Opcodes::GOTO:
        case Opcodes::RET:
        case Opcodes::TABLESWITCH:
        case Opcodes::LOOKUPSWITCH:
        case Opcodes::IRETURN:
        case Opcodes::LRETURN:
        case Opcodes::FRETURN:
        case Opcodes::DRETURN:
        case Opcodes::ARETURN:
        case Opcodes::RETURN:
        case Opcodes::ATHROW: return;
        default: currentInsnIndex++; break;
        }
    }
}

void JSRInlinerAdapter::emitCode()
{
    std::deque<Instantiation> worklist;
    worklist.emplace_back(*this, nullptr, &mainSubroutineInsns_, instructions);

    InsnList                        newInstructions;
    std::vector<TryCatchBlockNode*> newTryCatchBlocks;
    std::vector<LocalVariableNode*> newLocalVariables;

    while (!worklist.empty()) {
        Instantiation instantiation = worklist.front();
        worklist.pop_front();
        emitInstantiation(
            instantiation, worklist, newInstructions, newTryCatchBlocks, newLocalVariables);
    }

    instructions   = std::move(newInstructions);
    tryCatchBlocks = std::move(newTryCatchBlocks);
    localVariables = std::move(newLocalVariables);
    // B8：emit 产物（节点与 Label）全部归属本方法 arena，Label 收编由
    // visitor 边界 canonicalizeLabel 承担，无需接管钩子（B8b 已随旧池退役）。
}

void JSRInlinerAdapter::emitInstantiation(Instantiation&                   instantiation,
                                          std::deque<Instantiation>&       worklist,
                                          InsnList&                        newInstructions,
                                          std::vector<TryCatchBlockNode*>& newTryCatchBlocks,
                                          std::vector<LocalVariableNode*>& newLocalVariables)
{

    LabelNode* previousLabelNode = nullptr;
    for (size_t i = 0; i < instructions.size(); i++) {
        AbstractInsnNode* insnNode = instructions.get((int)i);

        if (insnNode->getType() == AbstractInsnNode::LABEL) {
            auto* labelNode = ca::core::tag_cast::cast<LabelNode*>(insnNode);
            if (labelNode) {
                LabelNode* clonedLabel = instantiation.getClonedLabel(labelNode);
                if (clonedLabel && clonedLabel != previousLabelNode) {
                    newInstructions.add(clonedLabel);
                    previousLabelNode = clonedLabel;
                }
            }
        }
        else if (instantiation.findOwner((int)i, sharedSubroutineInsns_) == &instantiation) {
            if (insnNode->getOpcode() == Opcodes::RET) {
                LabelNode* retLabel = nullptr;
                for (Instantiation* retOwner = &instantiation; retOwner != nullptr;
                     retOwner                = retOwner->parent) {
                    if ((size_t)i < retOwner->subroutineInsns->size() &&
                        (*retOwner->subroutineInsns)[i]) {
                        retLabel = retOwner->returnLabel;
                    }
                }
                if (retLabel == nullptr) {
                    throw std::runtime_error("Instruction #" + std::to_string(i) +
                                             " is a RET not owned by any subroutine");
                }
                newInstructions.add(create<JumpInsnNode>(Opcodes::GOTO, retLabel->label));
            }
            else if (insnNode->getOpcode() == Opcodes::JSR) {
                auto* jumpNode = ca::core::tag_cast::cast<JumpInsnNode*>(insnNode);
                if (jumpNode) {
                    LabelNode* jsrTarget = findLabelNode(instructions, jumpNode->label);
                    if (jsrTarget) {
                        auto it = subroutinesInsns_.find(jsrTarget);
                        if (it != subroutinesInsns_.end()) {
                            Instantiation newInst(*this, &instantiation, &it->second, instructions);
                            LabelNode*    clonedJsrLabel = newInst.getClonedLabelForJumpInsn(
                                jsrTarget, sharedSubroutineInsns_);

                            newInstructions.add(create<InsnNode>(Opcodes::ACONST_NULL));
                            newInstructions.add(
                                create<JumpInsnNode>(Opcodes::GOTO, clonedJsrLabel->label));
                            newInstructions.add(newInst.returnLabel);
                            worklist.push_back(std::move(newInst));
                        }
                    }
                }
            }
            else {
                // B8 迁移：克隆产物全部改由本方法 arena 分配。label 携带节点不能直接
                // 走 clone(arena, map) 新协议——resolveCloneLabel 对「label 已归属目标
                // arena」直返原身份，而本适配器的源 label 全部归属 this->arena()（方法
                // 边界 canonicalize 收编），直返会令克隆跳转指向未在新指令流中出现的
                // 原 label（悬空）。JSR 内联需要每次实例化为同一原 label 换取全新身份
                // 且合并连续 label，与 clone 协议「同域不换身份 + 1:1 映射」模型冲突，
                // 故保留 Instantiation 的节点级映射（双 map 不能统一的评估结论见
                // hpp），仅在此唯一换算点折算成 Label 级映射，携带 Label 的四类节点
                // 手工换线，其余节点直通新协议。
                std::unordered_map<Label*, Label*> labelMap;
                for (auto& entry : instantiation.clonedLabels) {
                    labelMap[entry.first->label] = entry.second->label;
                }
                switch (insnNode->getType()) {
                case AbstractInsnNode::JUMP_INSN: {
                    auto* jump = ca::core::tag_cast::cast<JumpInsnNode*>(insnNode);
                    newInstructions.add(create<JumpInsnNode>(
                        jump->getOpcode(), clonedLabelOf(*this, labelMap, jump->label)));
                    break;
                }
                case AbstractInsnNode::TABLESWITCH_INSN: {
                    auto* ts = ca::core::tag_cast::cast<TableSwitchInsnNode*>(insnNode);
                    std::vector<Label*> newLabels;
                    newLabels.reserve(ts->labels.size());
                    for (auto* l : ts->labels) {
                        newLabels.push_back(clonedLabelOf(*this, labelMap, l));
                    }
                    newInstructions.add(create<TableSwitchInsnNode>(
                        ts->min, ts->max, clonedLabelOf(*this, labelMap, ts->dflt), newLabels));
                    break;
                }
                case AbstractInsnNode::LOOKUPSWITCH_INSN: {
                    auto* ls = ca::core::tag_cast::cast<LookupSwitchInsnNode*>(insnNode);
                    std::vector<Label*> newLabels;
                    newLabels.reserve(ls->labels.size());
                    for (auto* l : ls->labels) {
                        newLabels.push_back(clonedLabelOf(*this, labelMap, l));
                    }
                    newInstructions.add(create<LookupSwitchInsnNode>(
                        clonedLabelOf(*this, labelMap, ls->dflt), ls->keys, newLabels));
                    break;
                }
                case AbstractInsnNode::LINE: {
                    auto* line = ca::core::tag_cast::cast<LineNumberNode*>(insnNode);
                    newInstructions.add(create<LineNumberNode>(
                        line->line, clonedLabelOf(*this, labelMap, line->start)));
                    break;
                }
                default:
                    // 其余节点不携带 Label：clone(arena, map) 直通（map 不会被读取）。
                    newInstructions.add(insnNode->clone(this->arena(), labelMap));
                    break;
                }
            }
        }
    }

    for (auto* tcb : tryCatchBlocks) {
        LabelNode* start   = findLabelNode(instructions, tcb->start);
        LabelNode* end     = findLabelNode(instructions, tcb->end);
        LabelNode* handler = findLabelNode(instructions, tcb->handler);
        if (!start || !end || !handler) continue;

        LabelNode* clonedStart = instantiation.getClonedLabel(start);
        LabelNode* clonedEnd   = instantiation.getClonedLabel(end);
        if (clonedStart != clonedEnd) {
            LabelNode* clonedHandler =
                instantiation.getClonedLabelForJumpInsn(handler, sharedSubroutineInsns_);
            if (clonedStart && clonedEnd && clonedHandler) {
                newTryCatchBlocks.push_back(create<TryCatchBlockNode>(
                    clonedStart->label, clonedEnd->label, clonedHandler->label, tcb->type));
            }
        }
    }

    for (auto* lv : localVariables) {
        LabelNode* start = findLabelNode(instructions, lv->start);
        LabelNode* end   = findLabelNode(instructions, lv->end);
        if (!start || !end) continue;

        LabelNode* clonedStart = instantiation.getClonedLabel(start);
        LabelNode* clonedEnd   = instantiation.getClonedLabel(end);
        if (clonedStart != clonedEnd) {
            newLocalVariables.push_back(create<LocalVariableNode>(lv->name,
                                                              lv->descriptor,
                                                              lv->signature,
                                                              clonedStart->label,
                                                              clonedEnd->label,
                                                              lv->index));
        }
    }
}

// ============================================================
// Instantiation
// ============================================================

JSRInlinerAdapter::Instantiation::Instantiation(JSRInlinerAdapter&       owner,
                                                Instantiation*           parent,
                                                const std::vector<bool>* subroutineInsns,
                                                InsnList&                instructions)
    : parent(parent)
    , subroutineInsns(subroutineInsns)
    , returnLabel(parent ? owner.create<LabelNode>(owner.createLabel()) : nullptr)
{

    // Check for recursion
    for (Instantiation* p = parent; p != nullptr; p = p->parent) {
        if (p->subroutineInsns == subroutineInsns) {
            throw std::runtime_error("Recursive invocation of subroutine");
        }
    }

    LabelNode* clonedLabelNode = nullptr;
    for (int insnIndex = 0; (size_t)insnIndex < instructions.size(); insnIndex++) {
        AbstractInsnNode* insnNode = instructions.get(insnIndex);
        if (insnNode->getType() == AbstractInsnNode::LABEL) {
            auto* labelNode = ca::core::tag_cast::cast<LabelNode*>(insnNode);
            if (labelNode) {
                if (clonedLabelNode == nullptr) {
                    // 连续 label 合并为一个克隆 LabelNode（身份去重）；B8 迁移：
                    // 节点与 Label 均落宿主方法 arena，不再堆分配。
                    clonedLabelNode = owner.create<LabelNode>(owner.createLabel());
                }
                clonedLabels[labelNode] = clonedLabelNode;
            }
        }
        else if (findOwner(insnIndex, std::vector<bool>()) == this) {
            clonedLabelNode = nullptr;
        }
    }
}

JSRInlinerAdapter::Instantiation* JSRInlinerAdapter::Instantiation::findOwner(
    int insnIndex, const std::vector<bool>& sharedInsns)
{
    if ((size_t)insnIndex >= subroutineInsns->size() || !(*subroutineInsns)[insnIndex])
        return nullptr;
    if ((size_t)insnIndex >= sharedInsns.size() || !sharedInsns[insnIndex]) return this;

    Instantiation* owner = this;
    for (Instantiation* p = parent; p != nullptr; p = p->parent) {
        if ((size_t)insnIndex < p->subroutineInsns->size() && (*p->subroutineInsns)[insnIndex])
            owner = p;
    }
    return owner;
}

LabelNode* JSRInlinerAdapter::Instantiation::getClonedLabelForJumpInsn(
    LabelNode* labelNode, const std::vector<bool>& sharedInsns)
{
    (void)sharedInsns;
    auto it = clonedLabels.find(labelNode);
    return it != clonedLabels.end() ? it->second : nullptr;
}

LabelNode* JSRInlinerAdapter::Instantiation::getClonedLabel(LabelNode* labelNode)
{
    auto it = clonedLabels.find(labelNode);
    return it != clonedLabels.end() ? it->second : nullptr;
}
