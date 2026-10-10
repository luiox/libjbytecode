#include "jbytecode/tree/nodes.hpp"
#include "jbytecode/tree/insn_list.hpp"

using namespace jbytecode::tree;
using namespace jbytecode::core;

// MethodNode move 已删除（nodes.hpp）：节点以 &nodeArena_ 为归属锚，对象不搬。

MethodNode::MethodNode(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                       ca::str::Utf8StringRef                     signature,
                       const std::vector<ca::str::Utf8StringRef>& exceptions)
    : MethodVisitor(Opcodes::ASM9)
    , access(access)
    , name(name)
    , desc(desc)
    , signature(signature)
    , exceptions(exceptions)
{
}

AnnotationVisitor* MethodNode::visitAnnotationDefault()
{
    annotationDefault = create<AnnotationNode>(nodeArena_, ca::str::Utf8StringRef());
    return annotationDefault;
}

AnnotationVisitor* MethodNode::visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible)
{
    auto* an = create<AnnotationNode>(nodeArena_, descriptor);
    if (visible)
        visibleAnnotations.push_back(an);
    else
        invisibleAnnotations.push_back(an);
    return an;
}

AnnotationVisitor* MethodNode::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                   ca::str::Utf8StringRef desc, bool visible)
{
    auto* an = create<TypeAnnotationNode>(nodeArena_, typeRef, typePath, desc);
    if (visible)
        visibleTypeAnnotations.push_back(an);
    else
        invisibleTypeAnnotations.push_back(an);
    return an;
}

AnnotationVisitor* MethodNode::visitParameterAnnotation(int                    parameter,
                                                        ca::str::Utf8StringRef descriptor,
                                                        bool                   visible)
{
    auto* an = create<AnnotationNode>(nodeArena_, descriptor);
    if (visible) {
        visibleParameterAnnotations.push_back(an);
        visibleParameterIndices.push_back(parameter);
    }
    else {
        invisibleParameterAnnotations.push_back(an);
        invisibleParameterIndices.push_back(parameter);
    }
    return an;
}

AnnotationVisitor* MethodNode::visitLocalVariableAnnotation(
    int typeRef, const TypePath* typePath, const std::vector<Label*>& start,
    const std::vector<Label*>& end, const std::vector<int>& index, ca::str::Utf8StringRef desc,
    bool visible)
{
    // 起止范围表随注解整体收编：Label 经 canonicalizeLabel 换成方法 arena 副本
    // （B8，树不持有 reader/调用方 Label），其余载体与 visitTypeAnnotation 同构。
    std::vector<Label*> canonicalStart;
    canonicalStart.reserve(start.size());
    for (auto* l : start) canonicalStart.push_back(canonicalizeLabel(l));
    std::vector<Label*> canonicalEnd;
    canonicalEnd.reserve(end.size());
    for (auto* l : end) canonicalEnd.push_back(canonicalizeLabel(l));
    auto* an = create<LocalVariableAnnotationNode>(
        nodeArena_, typeRef, typePath, canonicalStart, canonicalEnd, index, desc);
    if (visible)
        visibleLocalVariableAnnotations.push_back(an);
    else
        invisibleLocalVariableAnnotations.push_back(an);
    return an;
}

static void replayMethodBody(MethodVisitor* mv, MethodNode* m)
{
    // Label 中的 resolved/offset 属于上一次 Reader 或 Writer。每次重放都从未解析状态开始，
    // 使 MethodWriter 能重新计算跳转距离，并让同一 MethodNode 可以稳定地重复 accept。
    m->instructions.resetLabels();
    for (auto* p : m->parameters) mv->visitParameter(p->name, p->access);
    if (m->annotationDefault) {
        AnnotationVisitor* dav = mv->visitAnnotationDefault();
        if (dav) m->annotationDefault->accept(dav);
    }
    for (auto* an : m->visibleAnnotations) {
        AnnotationVisitor* av = mv->visitAnnotation(an->desc, true);
        if (av) an->accept(av);
    }
    for (auto* an : m->invisibleAnnotations) {
        AnnotationVisitor* av = mv->visitAnnotation(an->desc, false);
        if (av) an->accept(av);
    }
    for (size_t i = 0; i < m->visibleTypeAnnotations.size(); i++) {
        auto*              tan = static_cast<TypeAnnotationNode*>(m->visibleTypeAnnotations[i]);
        AnnotationVisitor* av =
            mv->visitTypeAnnotation(tan->typeRef, tan->typePath, tan->desc, true);
        if (av) tan->accept(av);
    }
    for (size_t i = 0; i < m->invisibleTypeAnnotations.size(); i++) {
        auto*              tan = static_cast<TypeAnnotationNode*>(m->invisibleTypeAnnotations[i]);
        AnnotationVisitor* av =
            mv->visitTypeAnnotation(tan->typeRef, tan->typePath, tan->desc, false);
        if (av) tan->accept(av);
    }
    for (size_t i = 0; i < m->visibleParameterAnnotations.size(); i++) {
        int param = i < m->visibleParameterIndices.size() ? m->visibleParameterIndices[i] : 0;
        AnnotationVisitor* av =
            mv->visitParameterAnnotation(param, m->visibleParameterAnnotations[i]->desc, true);
        if (av) m->visibleParameterAnnotations[i]->accept(av);
    }
    for (size_t i = 0; i < m->invisibleParameterAnnotations.size(); i++) {
        int param = i < m->invisibleParameterIndices.size() ? m->invisibleParameterIndices[i] : 0;
        AnnotationVisitor* av =
            mv->visitParameterAnnotation(param, m->invisibleParameterAnnotations[i]->desc, false);
        if (av) m->invisibleParameterAnnotations[i]->accept(av);
    }
    for (auto* lvan : m->visibleLocalVariableAnnotations) {
        AnnotationVisitor* av = mv->visitLocalVariableAnnotation(
            lvan->typeRef, lvan->typePath, lvan->start, lvan->end, lvan->index, lvan->desc, true);
        if (av) lvan->accept(av);
    }
    for (auto* lvan : m->invisibleLocalVariableAnnotations) {
        AnnotationVisitor* av = mv->visitLocalVariableAnnotation(
            lvan->typeRef, lvan->typePath, lvan->start, lvan->end, lvan->index, lvan->desc, false);
        if (av) lvan->accept(av);
    }
    mv->visitCode();
    for (auto* tc : m->tryCatchBlocks) {
        mv->visitTryCatchBlock(tc->start, tc->end, tc->handler, tc->type);
    }
    m->instructions.accept(mv);
    for (auto* lv : m->localVariables) {
        mv->visitLocalVariable(
            lv->name, lv->descriptor, lv->signature, lv->start, lv->end, lv->index);
    }
    mv->visitMaxs(m->maxStack, m->maxLocals);
    mv->visitEnd();
}

void MethodNode::accept(ClassVisitor* cv)
{
    MethodVisitor* mv = cv->visitMethod(access, name, desc, signature, exceptions);
    if (mv) replayMethodBody(mv, this);
}

void MethodNode::accept(MethodVisitor* mv)
{
    if (!mv) return;
    replayMethodBody(mv, this);
}

// ---- Label 归属（B8：arena 盖章即所有权，见 DESIGN.md §6.1）----

jbytecode::core::Label* MethodNode::createLabel()
{
    auto* label = nodeArena_.create<Label>();
    label->setOwner(&nodeArena_);   // 归属锚 = 内容 arena 地址（与 P2 同源）
    return label;
}

jbytecode::core::Label* MethodNode::canonicalizeLabel(jbytecode::core::Label* label)
{
    if (label == nullptr) return nullptr;
    // 本方法 arena 产物：owner 盖章直返（O(1)）。
    if (label->owner() == &nodeArena_) return label;
    // 外来 Label（reader/调用方自建堆对象）：查映射，未命中则拷贝收编进本方法 arena。
    // 拷贝语义 {bytecodeOffset, line, flags}：身份换新，控制流/调试元数据保留。
    auto it = foreignLabels_.find(label);
    if (it != foreignLabels_.end()) return it->second;
    auto* copy             = createLabel();
    copy->bytecodeOffset   = label->bytecodeOffset;
    copy->line             = label->line;
    copy->flags            = label->flags;
    foreignLabels_.emplace(label, copy);
    return copy;
}

jbytecode::core::Label*
MethodNode::resolveCloneLabel(std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>& map,
                              jbytecode::core::Label* label)
{
    // B8：委托自由函数（arena=nodeArena_），拷贝逻辑单点维护（见 insn_list.cpp）。
    return jbytecode::tree::resolveCloneLabel(nodeArena_, map, label);
}

