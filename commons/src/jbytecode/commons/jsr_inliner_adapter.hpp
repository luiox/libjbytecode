#pragma once

#include <deque>
#include <unordered_map>
#include <vector>

#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/visitors.hpp"
#include "jbytecode/tree/insn_list.hpp"
#include "jbytecode/tree/nodes.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class JSRInlinerAdapter : public jbytecode::tree::MethodNode
{
public:
    JSRInlinerAdapter(jbytecode::core::MethodVisitor* mv, int access, ca::str::Utf8StringRef name,
                      ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                      const std::vector<ca::str::Utf8StringRef>& exceptions);

    void visitJumpInsn(int opcode, jbytecode::core::Label* label) override;

    void visitEnd() override;

private:
    class Instantiation
    {
    public:
        Instantiation*              parent;
        const std::vector<bool>*    subroutineInsns;
        jbytecode::tree::LabelNode* returnLabel;
        /// 节点级克隆映射（原 LabelNode → 克隆 LabelNode），承担两项 clone 协议
        /// Label*→Label* 映射表达不了的语义：① 连续 label 合并为同一克隆节点
        /// （多对一）；② 每次实例化为同一原 label 换取全新身份。B8 评估结论：
        /// 不能与 clone(arena, map) 的 Label 映射统一——resolveCloneLabel 对已归属
        /// 目标 arena 的 label 直返原身份（同域不换身份），而 JSR 内联恰需在
        /// 本方法 arena 内反复换身份，故保留本映射，仅在 emitInstantiation 的
        /// 唯一换算点折算成 Label 级映射。
        std::unordered_map<jbytecode::tree::LabelNode*, jbytecode::tree::LabelNode*> clonedLabels;

        /// @param owner 宿主适配器：克隆 LabelNode 与其携带的 Label 一律经
        /// owner.create<LabelNode>(owner.createLabel()) 落入宿主方法 arena
        /// （B8 迁移，替代已 deprecated 的堆分配 new LabelNode()）。
        Instantiation(JSRInlinerAdapter& owner, Instantiation* parent,
                      const std::vector<bool>* subroutineInsns,
                      jbytecode::tree::InsnList& instructions);

        Instantiation* findOwner(int insnIndex, const std::vector<bool>& sharedInsns);

        jbytecode::tree::LabelNode* getClonedLabelForJumpInsn(jbytecode::tree::LabelNode* labelNode,
                                                              const std::vector<bool>& sharedInsns);

        jbytecode::tree::LabelNode* getClonedLabel(jbytecode::tree::LabelNode* labelNode);
    };

    std::vector<bool>                                                  mainSubroutineInsns_;
    std::unordered_map<jbytecode::tree::LabelNode*, std::vector<bool>> subroutinesInsns_;
    std::vector<bool>                                                  sharedSubroutineInsns_;

    void findSubroutinesInsns();
    void findSubroutineInsns(int startInsnIndex, std::vector<bool>& subroutineInsns,
                             std::vector<bool>& visitedInsns);
    void findReachableInsns(int insnIndex, std::vector<bool>& subroutineInsns,
                            std::vector<bool>& visitedInsns);
    void emitCode();
    void emitInstantiation(Instantiation& instantiation, std::deque<Instantiation>& worklist,
                           jbytecode::tree::InsnList&                        newInstructions,
                           std::vector<jbytecode::tree::TryCatchBlockNode*>& newTryCatchBlocks,
                           std::vector<jbytecode::tree::LocalVariableNode*>& newLocalVariables);
};

}   // namespace jbytecode::commons
