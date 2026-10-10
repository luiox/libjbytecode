#include "jbytecode/commons/try_catch_block_sorter.hpp"

#include "jbytecode/tree/insn_type_traits.hpp"

using namespace jbytecode::commons;
using namespace jbytecode::core;
using namespace jbytecode::tree;

namespace {

int findLabelIndex(InsnList& insns, Label* label)
{
    int idx = 0;
    for (auto* node : insns) {
        if (node->getType() == AbstractInsnNode::LABEL) {
            auto* ln = ca::core::tag_cast::cast<LabelNode*>(node);
            if (ln && ln->label == label) return idx;
        }
        idx++;
    }
    return 0;
}

}   // namespace

TryCatchBlockSorter::TryCatchBlockSorter(MethodVisitor* mv, int access, ca::str::Utf8StringRef name,
                                         ca::str::Utf8StringRef                     descriptor,
                                         ca::str::Utf8StringRef                     signature,
                                         const std::vector<ca::str::Utf8StringRef>& exceptions)
    : MethodNode(access, name, descriptor, signature, exceptions)
{
    this->mv = mv;
}

void TryCatchBlockSorter::visitEnd()
{
    std::sort(tryCatchBlocks.begin(),
              tryCatchBlocks.end(),
              [this](TryCatchBlockNode* a, TryCatchBlockNode* b) {
                  int lenA =
                      findLabelIndex(instructions, a->end) - findLabelIndex(instructions, a->start);
                  int lenB =
                      findLabelIndex(instructions, b->end) - findLabelIndex(instructions, b->start);
                  return lenA < lenB;
              });

    if (mv) accept(mv);
}
