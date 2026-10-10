#include "jbytecode/util/textifier.hpp"
#include "jbytecode/core/type.hpp"

using namespace jbytecode::util;
using namespace jbytecode::core;
using namespace ca::str;

Textifier::Textifier()
    : Printer(/*latest api=*/9)
{}

Utf8StringRef Textifier::getLabelName(Label* label)
{
    auto it = labelNames_.find(label);
    if (it != labelNames_.end()) return it->second;
    auto b = Utf8StringBuilder();
    b.append("L").append(std::to_string(nextLabelId_++).c_str());
    Utf8StringRef interned = labelArena_.intern(b.build().ref());
    labelNames_[label]     = interned;
    return interned;
}

void Textifier::line(Utf8StringRef s)
{
    text.push_back(textArena.intern(s));
}
void Textifier::line()
{
    text.push_back(Utf8StringRef());
}

Textifier* Textifier::subTextifier()
{
    return new Textifier();
}

void Textifier::appendDescriptor(Utf8StringRef desc, int type)
{
    (void)type;
    if (!desc.is_empty()) stringBuilder.append(desc);
}

void Textifier::visit(int version, int access, Utf8StringRef name, Utf8StringRef signature,
                      Utf8StringRef superName, const std::vector<Utf8StringRef>& interfaces)
{
    (void)signature;
    auto b = Utf8StringBuilder();
    b.append("// class version ")
        .append(std::to_string((version >> 16) & 0xFFFF).c_str())
        .append(".")
        .append(std::to_string(version & 0xFFFF).c_str());
    line(textArena.intern(b.build().ref()));
    line();
    b.clear();
    b.append("// access flags 0x").append(std::to_string(access).c_str());
    line(textArena.intern(b.build().ref()));
    line();
    b.clear();
    b.append("class ").append(name).append(" extends ").append(superName);
    line(textArena.intern(b.build().ref()));
    for (auto& iface : interfaces) {
        b.clear();
        b.append("  implements ").append(iface);
        line(textArena.intern(b.build().ref()));
    }
    line();
}

void Textifier::visitSource(Utf8StringRef source, Utf8StringRef debug)
{
    (void)debug;
    if (!source.is_empty()) {
        auto b = Utf8StringBuilder();
        b.append("// compiled from: ").append(source);
        line(textArena.intern(b.build().ref()));
    }
    line();
}

void Textifier::visitOuterClass(Utf8StringRef owner, Utf8StringRef name, Utf8StringRef descriptor)
{
    auto b = Utf8StringBuilder();
    b.append("// enclosing class: ")
        .append(owner)
        .append(" ")
        .append(name)
        .append(" ")
        .append(descriptor);
    line(textArena.intern(b.build().ref()));
    line();
}

Printer* Textifier::visitClassAnnotation(Utf8StringRef descriptor, bool visible)
{
    (void)visible;
    line();
    auto b = Utf8StringBuilder();
    b.append(TAB).append("// annotation: ").append(descriptor);
    line(textArena.intern(b.build().ref()));
    return subTextifier();
}

void Textifier::visitInnerClass(Utf8StringRef name, Utf8StringRef outerName,
                                Utf8StringRef innerName, int access)
{
    line();
    auto b = Utf8StringBuilder();
    b.append(TAB)
        .append("// inner class: ")
        .append(name)
        .append(" ")
        .append(outerName)
        .append(" ")
        .append(innerName)
        .append(" access: ")
        .append(std::to_string(access).c_str());
    line(textArena.intern(b.build().ref()));
}

void Textifier::visitCode()
{
    line();
    auto b = Utf8StringBuilder();
    b.append("  // ----- code -----");
    line(textArena.intern(b.build().ref()));
    line();
}
void Textifier::visitFrame(int, int, const std::vector<ca::core::Any>&, int,
                           const std::vector<ca::core::Any>&)
{}

void visitOpLine(const char* tab, const char* op, Utf8StringRef extra)
{
    auto b = Utf8StringBuilder();
    b.append(tab).append(op);
    if (!extra.is_empty()) b.append(" ").append(extra);
    // line is available from Textifier... actually this doesn't work as a free function
}

void Textifier::visitInsn(int opcode)
{
    auto b = Utf8StringBuilder();
    b.append(TAB).append(OPCODES[opcode]);
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitIntInsn(int opcode, int operand)
{
    auto b = Utf8StringBuilder();
    b.append(TAB).append(OPCODES[opcode]).append(" ").append(std::to_string(operand).c_str());
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitVarInsn(int opcode, int varIndex)
{
    auto b = Utf8StringBuilder();
    b.append(TAB).append(OPCODES[opcode]).append(" ").append(std::to_string(varIndex).c_str());
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitTypeInsn(int opcode, Utf8StringRef type)
{
    auto b = Utf8StringBuilder();
    b.append(TAB).append(OPCODES[opcode]).append(" ").append(type);
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitFieldInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                               Utf8StringRef descriptor)
{
    auto b = Utf8StringBuilder();
    b.append(TAB)
        .append(OPCODES[opcode])
        .append(" ")
        .append(owner)
        .append(".")
        .append(name)
        .append(" : ")
        .append(descriptor);
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitMethodInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                                Utf8StringRef descriptor, bool isInterface)
{
    auto b = Utf8StringBuilder();
    b.append(TAB)
        .append(OPCODES[opcode])
        .append(" ")
        .append(owner)
        .append(".")
        .append(name)
        .append(" ")
        .append(descriptor)
        .append(isInterface ? " (itf)" : "");
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitInvokeDynamicInsn(Utf8StringRef name, Utf8StringRef descriptor,
                                       const Handle&                     bootstrapMethodHandle,
                                       const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    (void)bootstrapMethodArguments;
    auto b = Utf8StringBuilder();
    b.append(TAB).append("INVOKEDYNAMIC ").append(name).append(" ").append(descriptor);
    line(textArena.intern(b.build().ref()));
    b.clear();
    b.append(TAB2)
        .append("Bootstrap: ")
        .append(bootstrapMethodHandle.getOwner())
        .append(".")
        .append(bootstrapMethodHandle.getName());
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitJumpInsn(int opcode, Label* label)
{
    auto b = Utf8StringBuilder();
    b.append(TAB).append(OPCODES[opcode]).append(" ").append(getLabelName(label));
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitLabel(Label* label)
{
    auto b = Utf8StringBuilder();
    b.append(getLabelName(label)).append(":");
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitLdcInsn(const ca::core::Any& value)
{
    stringBuilder.clear();
    auto b = Utf8StringBuilder();
    b.append(TAB).append("LDC ");
    stringBuilder = std::move(b);
    appendConstant(value);
    line(textArena.intern(stringBuilder.build().ref()));
    stringBuilder.clear();
}
void Textifier::visitIincInsn(int varIndex, int increment)
{
    auto b = Utf8StringBuilder();
    b.append(TAB)
        .append("IINC ")
        .append(std::to_string(varIndex).c_str())
        .append(" ")
        .append(std::to_string(increment).c_str());
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitTableSwitchInsn(int min, int max, Label* dflt,
                                     const std::vector<Label*>& labels)
{
    auto b = Utf8StringBuilder();
    b.append(TAB).append("TABLESWITCH");
    line(textArena.intern(b.build().ref()));
    b.clear();
    b.append(TAB2).append("default: ").append(getLabelName(dflt));
    line(textArena.intern(b.build().ref()));
    for (size_t i = 0; i < labels.size(); ++i) {
        b.clear();
        b.append(TAB2)
            .append(std::to_string(min + (int)i).c_str())
            .append(": ")
            .append(getLabelName(labels[i]));
        line(textArena.intern(b.build().ref()));
    }
}
void Textifier::visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                                      const std::vector<Label*>& labels)
{
    auto b = Utf8StringBuilder();
    b.append(TAB).append("LOOKUPSWITCH");
    line(textArena.intern(b.build().ref()));
    b.clear();
    b.append(TAB2).append("default: ").append(getLabelName(dflt));
    line(textArena.intern(b.build().ref()));
    for (size_t i = 0; i < labels.size(); ++i) {
        b.clear();
        b.append(TAB2)
            .append(std::to_string(keys[i]).c_str())
            .append(": ")
            .append(getLabelName(labels[i]));
        line(textArena.intern(b.build().ref()));
    }
}
void Textifier::visitMultiANewArrayInsn(Utf8StringRef descriptor, int numDimensions)
{
    auto b = Utf8StringBuilder();
    b.append(TAB)
        .append("MULTIANEWARRAY ")
        .append(descriptor)
        .append(" ")
        .append(std::to_string(numDimensions).c_str());
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitTryCatchBlock(Label* start, Label* end, Label* handler, Utf8StringRef type)
{
    auto b = Utf8StringBuilder();
    b.append(TAB)
        .append("try-catch: ")
        .append(getLabelName(start))
        .append(" -> ")
        .append(getLabelName(end))
        .append(" -> ")
        .append(getLabelName(handler));
    line(textArena.intern(b.build().ref()));
    if (!type.is_empty()) {
        b.clear();
        b.append(TAB2).append("type: ").append(type);
        line(textArena.intern(b.build().ref()));
    }
}
void Textifier::visitLocalVariable(Utf8StringRef name, Utf8StringRef descriptor,
                                   Utf8StringRef signature, Label* start, Label* end, int index)
{
    auto b = Utf8StringBuilder();
    b.append(TAB)
        .append("local: ")
        .append(name)
        .append(" ")
        .append(descriptor)
        .append(" ")
        .append(signature)
        .append(" range: ")
        .append(getLabelName(start))
        .append("-")
        .append(getLabelName(end))
        .append(" index: ")
        .append(std::to_string(index).c_str());
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitLineNumber(int lineNum, Label* start)
{
    auto b = Utf8StringBuilder();
    b.append(TAB)
        .append("line ")
        .append(std::to_string(lineNum).c_str())
        .append(": ")
        .append(getLabelName(start));
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitMaxs(int maxStack, int maxLocals)
{
    auto b = Utf8StringBuilder();
    b.append("  max_stack = ")
        .append(std::to_string(maxStack).c_str())
        .append(", max_locals = ")
        .append(std::to_string(maxLocals).c_str());
    line(textArena.intern(b.build().ref()));
    line();
}
void Textifier::visitMethodEnd() {}
void Textifier::visitClassEnd() {}
void Textifier::visitMainClass(Utf8StringRef mainClass)
{
    auto b = Utf8StringBuilder();
    b.append("  main class: ").append(mainClass);
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitPackage(Utf8StringRef packaze)
{
    auto b = Utf8StringBuilder();
    b.append("  package: ").append(packaze);
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitRequire(Utf8StringRef module, int access, Utf8StringRef version)
{
    auto b = Utf8StringBuilder();
    b.append("  requires: ")
        .append(module)
        .append(" access=")
        .append(std::to_string(access).c_str())
        .append(" version=")
        .append(version);
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitExport(Utf8StringRef packaze, int access,
                            const std::vector<Utf8StringRef>& modules)
{
    auto b = Utf8StringBuilder();
    b.append("  exports: ")
        .append(packaze)
        .append(" access=")
        .append(std::to_string(access).c_str());
    line(textArena.intern(b.build().ref()));
    for (auto& m : modules) {
        b.clear();
        b.append("    to: ").append(m);
        line(textArena.intern(b.build().ref()));
    }
}
void Textifier::visitOpen(Utf8StringRef packaze, int access,
                          const std::vector<Utf8StringRef>& modules)
{
    auto b = Utf8StringBuilder();
    b.append("  opens: ").append(packaze).append(" access=").append(std::to_string(access).c_str());
    line(textArena.intern(b.build().ref()));
    for (auto& m : modules) {
        b.clear();
        b.append("    to: ").append(m);
        line(textArena.intern(b.build().ref()));
    }
}
void Textifier::visitUse(Utf8StringRef service)
{
    auto b = Utf8StringBuilder();
    b.append("  uses: ").append(service);
    line(textArena.intern(b.build().ref()));
}
void Textifier::visitProvide(Utf8StringRef service, const std::vector<Utf8StringRef>& providers)
{
    auto b = Utf8StringBuilder();
    b.append("  provides: ").append(service);
    line(textArena.intern(b.build().ref()));
    for (auto& p : providers) {
        b.clear();
        b.append("    with: ").append(p);
        line(textArena.intern(b.build().ref()));
    }
}
void     Textifier::visitModuleEnd() {}
Printer* Textifier::visitField(int, Utf8StringRef name, Utf8StringRef descriptor, Utf8StringRef,
                               const ca::core::Any&)
{
    auto b = Utf8StringBuilder();
    b.append("  field: ").append(name).append(" ").append(descriptor);
    line(textArena.intern(b.build().ref()));
    return this;
}
Printer* Textifier::visitMethod(int, Utf8StringRef name, Utf8StringRef descriptor, Utf8StringRef,
                                const std::vector<Utf8StringRef>& exceptions)
{
    auto b = Utf8StringBuilder();
    b.append("  method: ").append(name).append(" ").append(descriptor);
    line(textArena.intern(b.build().ref()));
    for (auto& ex : exceptions) {
        b.clear();
        b.append("    throws: ").append(ex);
        line(textArena.intern(b.build().ref()));
    }
    return this;
}
Printer* Textifier::visitRecordComponentAnnotation(Utf8StringRef descriptor, bool visible)
{
    (void)visible;
    auto b = Utf8StringBuilder();
    b.append("  record component annotation: ").append(descriptor);
    line(textArena.intern(b.build().ref()));
    return this;
}
Printer* Textifier::visitRecordComponentTypeAnnotation(int, Utf8StringRef, Utf8StringRef descriptor,
                                                       bool)
{
    auto b = Utf8StringBuilder();
    b.append("  record component type annotation: ").append(descriptor);
    line(textArena.intern(b.build().ref()));
    return this;
}
void Textifier::visitRecordComponentAttribute(const jbytecode::core::Attribute&) {}
void Textifier::visitRecordComponentEnd() {}

void Textifier::visitAnnotationValue(Utf8StringRef name, const ca::core::Any& value)
{
    if (!name.is_empty()) {
        stringBuilder.clear();
        stringBuilder.append(name).append("=");
    }
    appendConstant(value);
}
void Textifier::visitAnnotationEnum(Utf8StringRef name, Utf8StringRef descriptor,
                                    Utf8StringRef value)
{
    auto b = Utf8StringBuilder();
    b.append(name).append("=").append(descriptor).append(".").append(value);
    line(textArena.intern(b.build().ref()));
}
Printer* Textifier::visitAnnotationAnnotation(Utf8StringRef name, Utf8StringRef descriptor)
{
    auto b = Utf8StringBuilder();
    b.append(name).append(" @").append(descriptor);
    line(textArena.intern(b.build().ref()));
    return this;
}
Printer* Textifier::visitAnnotationArray(Utf8StringRef name)
{
    auto b = Utf8StringBuilder();
    b.append(name).append(" = {...}");
    line(textArena.intern(b.build().ref()));
    return this;
}
void Textifier::visitAnnotationEnd() {}

void Textifier::appendConstant(const ca::core::Any& value)
{
    if (value.is<int>()) {
        stringBuilder.append(std::to_string(value.cast<int>()).c_str());
    }
    else if (value.is<long>()) {
        stringBuilder.append(std::to_string(value.cast<long>()).c_str()).append("L");
    }
    else if (value.is<float>()) {
        stringBuilder.append(std::to_string(value.cast<float>()).c_str()).append("F");
    }
    else if (value.is<double>()) {
        stringBuilder.append(std::to_string(value.cast<double>()).c_str()).append("D");
    }
    else if (auto* sp = value.as<Utf8StringRef>()) {
        stringBuilder.append("\"").append(*sp).append("\"");
    }
    else if (auto* tp = value.as<Type>()) {
        // #921 staticArena 退役：描述符仅瞬时 append，函数级局部 arena 零驻留。
        ca::str::Utf8StringArena scratch;
        stringBuilder.append(tp->getDescriptor(scratch));
    }
    else if (auto* hp = value.as<Handle>()) {
        appendHandle(*hp);
    }
}

void Textifier::appendHandle(const Handle& handle)
{
    auto b = Utf8StringBuilder();
    b.append("Handle{owner=")
        .append(handle.getOwner())
        .append(", name=")
        .append(handle.getName())
        .append(", desc=")
        .append(handle.getDesc())
        .append("}");
    stringBuilder.append(b.build().ref());
}
