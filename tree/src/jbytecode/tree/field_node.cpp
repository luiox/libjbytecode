#include "jbytecode/tree/nodes.hpp"

using namespace jbytecode::tree;
using namespace jbytecode::core;

AnnotationVisitor* FieldNode::visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible)
{
    auto* an = create<AnnotationNode>(nodeArena_, descriptor);
    if (visible)
        visibleAnnotations.push_back(an);
    else
        invisibleAnnotations.push_back(an);
    return an;
}

AnnotationVisitor* FieldNode::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                  ca::str::Utf8StringRef descriptor, bool visible)
{
    auto* annotation = create<TypeAnnotationNode>(nodeArena_, typeRef, typePath, descriptor);
    if (visible)
        visibleTypeAnnotations.push_back(annotation);
    else
        invisibleTypeAnnotations.push_back(annotation);
    return annotation;
}

void FieldNode::accept(ClassVisitor* cv)
{
    FieldVisitor* fv = cv->visitField(access, name, desc, signature, value);
    if (fv) {
        for (auto* an : visibleAnnotations) {
            AnnotationVisitor* av = fv->visitAnnotation(an->desc, true);
            if (av) an->accept(av);
        }
        for (auto* an : invisibleAnnotations) {
            AnnotationVisitor* av = fv->visitAnnotation(an->desc, false);
            if (av) an->accept(av);
        }
        for (auto* an : visibleTypeAnnotations) {
            auto*              typeAnnotation = static_cast<TypeAnnotationNode*>(an);
            AnnotationVisitor* av             = fv->visitTypeAnnotation(
                typeAnnotation->typeRef, typeAnnotation->typePath, typeAnnotation->desc, true);
            if (av) typeAnnotation->accept(av);
        }
        for (auto* an : invisibleTypeAnnotations) {
            auto*              typeAnnotation = static_cast<TypeAnnotationNode*>(an);
            AnnotationVisitor* av             = fv->visitTypeAnnotation(
                typeAnnotation->typeRef, typeAnnotation->typePath, typeAnnotation->desc, false);
            if (av) typeAnnotation->accept(av);
        }
        fv->visitEnd();
    }
}
