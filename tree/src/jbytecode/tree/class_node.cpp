#include "jbytecode/tree/nodes.hpp"

using namespace jbytecode::tree;
using namespace jbytecode::core;

// ClassNode move 已删除（nodes.hpp）：类级内容物以 &nodeArena_ 为归属锚，对象不搬。

void ClassNode::visit(int version, int access, ca::str::Utf8StringRef name,
                      ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
                      const std::vector<ca::str::Utf8StringRef>& interfaces)
{
    this->version    = version;
    this->access     = access;
    this->name       = name;
    this->signature  = signature;
    this->superName  = superName;
    this->interfaces = interfaces;
}

void ClassNode::visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug)
{
    sourceFile  = source;
    sourceDebug = debug;
}

void ClassNode::visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                ca::str::Utf8StringRef desc)
{
    outerClass      = owner;
    outerMethod     = name;
    outerMethodDesc = desc;
}

ModuleVisitor* ClassNode::visitModule(ca::str::Utf8StringRef name, int access,
                                      ca::str::Utf8StringRef version)
{
    moduleNode = create<ModuleNode>(name, access, version);
    return moduleNode;
}

void ClassNode::visitNestHost(ca::str::Utf8StringRef host)
{
    nestHost = host;
}
void ClassNode::visitNestMember(ca::str::Utf8StringRef member)
{
    nestMembers.push_back(member);
}
void ClassNode::visitPermittedSubclass(ca::str::Utf8StringRef subclass)
{
    permittedSubclasses.push_back(subclass);
}

AnnotationVisitor* ClassNode::visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible)
{
    auto* an = create<AnnotationNode>(nodeArena_, descriptor);
    if (visible)
        visibleAnnotations.push_back(an);
    else
        invisibleAnnotations.push_back(an);
    return an;
}

AnnotationVisitor* ClassNode::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                  ca::str::Utf8StringRef desc, bool visible)
{
    auto* an = create<TypeAnnotationNode>(nodeArena_, typeRef, typePath, desc);
    if (visible)
        visibleTypeAnnotations.push_back(an);
    else
        invisibleTypeAnnotations.push_back(an);
    return an;
}

void ClassNode::visitInnerClass(ca::str::Utf8StringRef name, ca::str::Utf8StringRef outerName,
                                ca::str::Utf8StringRef innerName, int access)
{
    innerClasses.push_back(create<InnerClassNode>(name, outerName, innerName, access));
}

RecordComponentVisitor* ClassNode::visitRecordComponent(ca::str::Utf8StringRef name,
                                                        ca::str::Utf8StringRef descriptor,
                                                        ca::str::Utf8StringRef signature)
{
    auto* rc = create<RecordComponentNode>(nodeArena_, name, descriptor, signature);
    recordComponents.push_back(rc);
    return rc;
}

FieldVisitor* ClassNode::visitField(int access, ca::str::Utf8StringRef name,
                                    ca::str::Utf8StringRef desc, ca::str::Utf8StringRef sig,
                                    const ca::core::Any& value)
{
    auto* fn = new FieldNode(access, name, desc, sig, value);
    fields.push_back(fn);
    return fn;
}

MethodVisitor* ClassNode::visitMethod(int access, ca::str::Utf8StringRef name,
                                      ca::str::Utf8StringRef desc, ca::str::Utf8StringRef sig,
                                      const std::vector<ca::str::Utf8StringRef>& exc)
{
    auto* mn = new MethodNode(access, name, desc, sig, exc);
    methods.push_back(mn);
    return mn;
}

void ClassNode::accept(ClassVisitor* cv)
{
    cv->visit(version, access, name, signature, superName, interfaces);

    if (!sourceFile.is_empty() || !sourceDebug.is_empty()) cv->visitSource(sourceFile, sourceDebug);
    if (moduleNode) moduleNode->accept(cv);
    if (!nestHost.is_empty()) cv->visitNestHost(nestHost);
    if (!outerClass.is_empty()) cv->visitOuterClass(outerClass, outerMethod, outerMethodDesc);

    for (auto* an : visibleAnnotations) {
        AnnotationVisitor* av = cv->visitAnnotation(an->desc, true);
        if (av) an->accept(av);
    }
    for (auto* an : invisibleAnnotations) {
        AnnotationVisitor* av = cv->visitAnnotation(an->desc, false);
        if (av) an->accept(av);
    }
    for (auto* an : visibleTypeAnnotations) {
        auto*              tan = static_cast<TypeAnnotationNode*>(an);
        AnnotationVisitor* av =
            cv->visitTypeAnnotation(tan->typeRef, tan->typePath, tan->desc, true);
        if (av) tan->accept(av);
    }
    for (auto* an : invisibleTypeAnnotations) {
        auto*              tan = static_cast<TypeAnnotationNode*>(an);
        AnnotationVisitor* av =
            cv->visitTypeAnnotation(tan->typeRef, tan->typePath, tan->desc, false);
        if (av) tan->accept(av);
    }

    for (auto& attr : attrs) cv->visitAttribute(*attr);
    for (auto& m : nestMembers) cv->visitNestMember(m);
    for (auto& s : permittedSubclasses) cv->visitPermittedSubclass(s);
    for (auto* ic : innerClasses)
        cv->visitInnerClass(ic->name, ic->outerName, ic->innerName, ic->access);
    for (auto* rc : recordComponents) rc->accept(cv);
    for (auto* fn : fields) fn->accept(cv);
    for (auto* mn : methods) mn->accept(cv);
    cv->visitEnd();
}

#ifndef NDEBUG
#    include "libca/log/log_macros.hpp"
#endif

namespace jbytecode::tree {

void debugAssertOwnedBy(const ClassNode& node, const ca::str::Utf8StringArena& arena)
{
#ifndef NDEBUG
    auto fail = [](const char* field) {
        // 诊断 I/O 收敛在模块唯一 .cpp（coding-rules §7：头内零 I/O）。
        CA_LOG_ERROR("[jbytecode][tree] ClassNode.{} 的字符串视图不属于装配 arena（悬垂或借错池）",
                     field);
        std::abort();
    };
    auto check = [&](const char* field, ca::str::Utf8StringRef ref) {
        if (!ref.is_empty() && !arena.owns(ref)) fail(field);
    };
    check("name", node.name);
    check("superName", node.superName);
    check("signature", node.signature);
    if (!node.interfaces.empty()) check("interfaces[0]", node.interfaces.front());
    if (!node.fields.empty() && node.fields.front() != nullptr) {
        check("fields[0].name", node.fields.front()->name);
        check("fields[0].desc", node.fields.front()->desc);
    }
    if (!node.methods.empty() && node.methods.front() != nullptr) {
        check("methods[0].name", node.methods.front()->name);
        check("methods[0].desc", node.methods.front()->desc);
    }
#else
    (void)node;
    (void)arena;
#endif
}

}   // namespace jbytecode::tree
