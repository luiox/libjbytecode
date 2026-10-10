#include "jbytecode/tree/nodes.hpp"

using namespace jbytecode::tree;
using namespace jbytecode::core;

RecordComponentNode::RecordComponentNode(garena::Arena& contentArena,
                                        ca::str::Utf8StringRef name,
                                        ca::str::Utf8StringRef descriptor,
                                        ca::str::Utf8StringRef signature)
    : RecordComponentVisitor(Opcodes::ASM9)
    , name(name)
    , descriptor(descriptor)
    , signature(signature)
    , contentArena_(contentArena)
{}

AnnotationVisitor* RecordComponentNode::visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible)
{
    auto* an = contentArena_.create<AnnotationNode>(contentArena_, descriptor);
    if (visible)
        visibleAnnotations.push_back(an);
    else
        invisibleAnnotations.push_back(an);
    return an;
}

AnnotationVisitor* RecordComponentNode::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                            ca::str::Utf8StringRef descriptor,
                                                            bool                   visible)
{
    auto* annotation = contentArena_.create<TypeAnnotationNode>(contentArena_, typeRef, typePath, descriptor);
    if (visible)
        visibleTypeAnnotations.push_back(annotation);
    else
        invisibleTypeAnnotations.push_back(annotation);
    return annotation;
}

void RecordComponentNode::accept(ClassVisitor* cv)
{
    auto* rcv = cv->visitRecordComponent(name, descriptor, signature);
    if (rcv) {
        for (auto* an : visibleAnnotations) {
            auto* av = rcv->visitAnnotation(an->desc, true);
            if (av) an->accept(av);
        }
        for (auto* an : invisibleAnnotations) {
            auto* av = rcv->visitAnnotation(an->desc, false);
            if (av) an->accept(av);
        }
        for (auto* an : visibleTypeAnnotations) {
            auto* typeAnnotation = static_cast<TypeAnnotationNode*>(an);
            auto* av             = rcv->visitTypeAnnotation(
                typeAnnotation->typeRef, typeAnnotation->typePath, typeAnnotation->desc, true);
            if (av) typeAnnotation->accept(av);
        }
        for (auto* an : invisibleTypeAnnotations) {
            auto* typeAnnotation = static_cast<TypeAnnotationNode*>(an);
            auto* av             = rcv->visitTypeAnnotation(
                typeAnnotation->typeRef, typeAnnotation->typePath, typeAnnotation->desc, false);
            if (av) typeAnnotation->accept(av);
        }
        rcv->visitEnd();
    }
}
