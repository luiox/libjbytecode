#include "jbytecode/util/asmifier.hpp"
#include "jbytecode/core/type.hpp"
#include <sstream>

using namespace jbytecode::util;
using namespace jbytecode::core;
using namespace ca::str;

ASMifier::ASMifier()
    : Printer(/*latest api=*/9)
{
    state_     = new SharedState();
    ownsState_ = true;
}

ASMifier::~ASMifier()
{
    if (ownsState_) delete state_;
}

void ASMifier::line(Utf8StringRef s)
{
    text.push_back(textArena.intern(s));
}

void ASMifier::line()
{
    text.push_back(Utf8StringRef());
}

ASMifier* ASMifier::subASMifier()
{
    auto* sub       = new ASMifier();
    sub->state_     = state_;
    sub->ownsState_ = false;
    return sub;
}

std::string ASMifier::toASMValue(const ca::core::Any& value)
{
    if (value.is<int>()) {
        return std::to_string(value.cast<int>());
    }
    else if (value.is<long>()) {
        return std::to_string(value.cast<long>()) + "L";
    }
    else if (value.is<long long>()) {
        return std::to_string(value.cast<long long>()) + "L";
    }
    else if (value.is<float>()) {
        return std::to_string(value.cast<float>()) + "f";
    }
    else if (value.is<double>()) {
        return std::to_string(value.cast<double>()) + "d";
    }
    else if (value.is<Utf8StringRef>()) {
        return appendString(value.cast<Utf8StringRef>()).to_std_string();
    }
    return "null";
}

void ASMifier::visit(int version, int access, Utf8StringRef name, Utf8StringRef signature,
                     Utf8StringRef superName, const std::vector<Utf8StringRef>& interfaces)
{
    varName_   = "cw";
    auto nameS = name.to_std_string();
    line(textArena.intern(Utf8StringBuilder()
                              .append("public class ")
                              .append(nameS.substr(nameS.find_last_of('/') + 1).c_str())
                              .append("Dump implements Opcodes {")
                              .build()
                              .ref()));
    line();
    line(Utf8StringRef::from_cstr("public static byte[] dump() {"));
    line();
    {
        auto t = Utf8StringBuilder();
        t.append("ClassWriter ")
            .append(varName_.c_str())
            .append(" = new ClassWriter(ClassWriter.COMPUTE_FRAMES);");
        line(textArena.intern(t.build().ref()));
    }
    line();

    auto buf = Utf8StringBuilder();
    buf.append(varName_.c_str())
        .append(".visit(")
        .append(std::to_string((version >> 16) & 0xFFFF).c_str())
        .append(", ")
        .append(std::to_string(access).c_str())
        .append(", ")
        .append(appendString(name).ref())
        .append(", ")
        .append(signature.is_empty() ? Utf8StringRef::from_cstr("null")
                                     : appendString(signature).ref())
        .append(", ")
        .append(appendString(superName).ref())
        .append(", ");
    if (interfaces.empty()) {
        buf.append("null");
    }
    else {
        buf.append("new String[] {");
        for (size_t i = 0; i < interfaces.size(); ++i) {
            if (i > 0) buf.append(", ");
            buf.append(appendString(interfaces[i]).ref());
        }
        buf.append("}");
    }
    buf.append(");");
    line(textArena.intern(buf.build().ref()));
}

void ASMifier::visitSource(Utf8StringRef source, Utf8StringRef debug)
{
    line(textArena.intern(
        Utf8StringBuilder()
            .append(varName_.c_str())
            .append(".visitSource(")
            .append(appendString(source).ref())
            .append(", ")
            .append(debug.is_empty() ? Utf8StringRef::from_cstr("null") : appendString(debug).ref())
            .append(");")
            .build()
            .ref()));
}

void ASMifier::visitOuterClass(Utf8StringRef owner, Utf8StringRef name, Utf8StringRef descriptor)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitOuterClass(")
                              .append(appendString(owner).ref())
                              .append(", ")
                              .append(appendString(name).ref())
                              .append(", ")
                              .append(appendString(descriptor).ref())
                              .append(");")
                              .build()
                              .ref()));
}

Printer* ASMifier::visitClassAnnotation(Utf8StringRef descriptor, bool visible)
{
    std::string av = nextVar();
    line(textArena.intern(Utf8StringBuilder()
                              .append(av.c_str())
                              .append(" = ")
                              .append(varName_.c_str())
                              .append(".visitAnnotation(")
                              .append(appendString(descriptor).ref())
                              .append(", ")
                              .append(visible ? "true" : "false")
                              .append(");")
                              .build()
                              .ref()));
    auto* sub     = subASMifier();
    sub->varName_ = av;
    return sub;
}

void ASMifier::visitInnerClass(Utf8StringRef name, Utf8StringRef outerName, Utf8StringRef innerName,
                               int access)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitInnerClass(")
                              .append(appendString(name).ref())
                              .append(", ")
                              .append(appendString(outerName).ref())
                              .append(", ")
                              .append(appendString(innerName).ref())
                              .append(", ")
                              .append(std::to_string(access).c_str())
                              .append(");")
                              .build()
                              .ref()));
}

Printer* ASMifier::visitField(int access, Utf8StringRef name, Utf8StringRef descriptor,
                              Utf8StringRef signature, const ca::core::Any& value)
{
    std::string fv  = nextVar();
    auto        buf = Utf8StringBuilder();
    buf.append(fv.c_str())
        .append(" = ")
        .append(varName_.c_str())
        .append(".visitField(")
        .append(std::to_string(access).c_str())
        .append(", ")
        .append(appendString(name).ref())
        .append(", ")
        .append(appendString(descriptor).ref())
        .append(", ")
        .append(signature.is_empty() ? Utf8StringRef::from_cstr("null")
                                     : appendString(signature).ref())
        .append(", ");
    if (!value.has_value()) {
        buf.append("null");
    }
    else {
        buf.append(toASMValue(value).c_str());
    }
    buf.append(");");
    line(textArena.intern(buf.build().ref()));
    auto* sub     = subASMifier();
    sub->varName_ = fv;
    return sub;
}

Printer* ASMifier::visitMethod(int access, Utf8StringRef name, Utf8StringRef descriptor,
                               Utf8StringRef                     signature,
                               const std::vector<Utf8StringRef>& exceptions)
{
    std::string mv  = nextVar();
    auto        buf = Utf8StringBuilder();
    buf.append(mv.c_str())
        .append(" = ")
        .append(varName_.c_str())
        .append(".visitMethod(")
        .append(std::to_string(access).c_str())
        .append(", ")
        .append(appendString(name).ref())
        .append(", ")
        .append(appendString(descriptor).ref())
        .append(", ")
        .append(signature.is_empty() ? Utf8StringRef::from_cstr("null")
                                     : appendString(signature).ref())
        .append(", ");
    if (exceptions.empty()) {
        buf.append("null");
    }
    else {
        buf.append("new String[] {");
        for (size_t i = 0; i < exceptions.size(); ++i) {
            if (i > 0) buf.append(", ");
            buf.append(appendString(exceptions[i]).ref());
        }
        buf.append("}");
    }
    buf.append(");");
    line(textArena.intern(buf.build().ref()));
    auto* sub     = subASMifier();
    sub->varName_ = mv;
    return sub;
}

void ASMifier::visitClassEnd()
{
    line(Utf8StringRef::from_cstr("}"));
    line(Utf8StringRef::from_cstr("}"));
}

void ASMifier::visitCode()
{
    line(Utf8StringRef::from_cstr("// code"));
}



void ASMifier::visitInsn(int opcode)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitInsn(")
                              .append(OPCODES[opcode])
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitIntInsn(int opcode, int operand)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitIntInsn(")
                              .append(OPCODES[opcode])
                              .append(", ")
                              .append(std::to_string(operand).c_str())
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitVarInsn(int opcode, int varIndex)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitVarInsn(")
                              .append(OPCODES[opcode])
                              .append(", ")
                              .append(std::to_string(varIndex).c_str())
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitTypeInsn(int opcode, Utf8StringRef type)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitTypeInsn(")
                              .append(OPCODES[opcode])
                              .append(", ")
                              .append(appendString(type).ref())
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitFieldInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                              Utf8StringRef descriptor)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitFieldInsn(")
                              .append(OPCODES[opcode])
                              .append(", ")
                              .append(appendString(owner).ref())
                              .append(", ")
                              .append(appendString(name).ref())
                              .append(", ")
                              .append(appendString(descriptor).ref())
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitMethodInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                               Utf8StringRef descriptor, bool isInterface)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitMethodInsn(")
                              .append(OPCODES[opcode])
                              .append(", ")
                              .append(appendString(owner).ref())
                              .append(", ")
                              .append(appendString(name).ref())
                              .append(", ")
                              .append(appendString(descriptor).ref())
                              .append(", ")
                              .append(isInterface ? "true" : "false")
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitInvokeDynamicInsn(Utf8StringRef name, Utf8StringRef descriptor,
                                      const Handle&                     bootstrapMethodHandle,
                                      const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    (void)bootstrapMethodHandle;
    (void)bootstrapMethodArguments;
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitInvokeDynamicInsn(")
                              .append(appendString(name).ref())
                              .append(", ")
                              .append(appendString(descriptor).ref())
                              .append(", ")
                              .append("bsm, args);")
                              .build()
                              .ref()));
}

void ASMifier::visitJumpInsn(int opcode, Label* label)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitJumpInsn(")
                              .append(OPCODES[opcode])
                              .append(", ")
                              .append(getLabelName(label).c_str())
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitLabel(Label* label)
{
    line(textArena.intern(
        Utf8StringBuilder().append(getLabelName(label).c_str()).append(":").build().ref()));
}

void ASMifier::visitLdcInsn(const ca::core::Any& value)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitLdcInsn(")
                              .append(toASMValue(value).c_str())
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitIincInsn(int varIndex, int increment)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitIincInsn(")
                              .append(std::to_string(varIndex).c_str())
                              .append(", ")
                              .append(std::to_string(increment).c_str())
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitTableSwitchInsn(int min, int max, Label* dflt,
                                    const std::vector<Label*>& labels)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitTableSwitchInsn(")
                              .append(std::to_string(min).c_str())
                              .append(", ")
                              .append(std::to_string(max).c_str())
                              .append(", ")
                              .append(getLabelName(dflt).c_str())
                              .append(", new Label[] {")
                              .build()
                              .ref()));
    for (auto* lb : labels) {
        line(textArena.intern(
            Utf8StringBuilder().append(getLabelName(lb).c_str()).append(", ").build().ref()));
    }
    line(Utf8StringRef::from_cstr("});"));
}

void ASMifier::visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                                     const std::vector<Label*>& labels)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitLookupSwitchInsn(")
                              .append(getLabelName(dflt).c_str())
                              .append(", new int[] {")
                              .build()
                              .ref()));
    for (size_t i = 0; i < keys.size(); ++i) {
        line(textArena.intern(Utf8StringBuilder()
                                  .append(std::to_string(keys[i]).c_str())
                                  .append(", ")
                                  .build()
                                  .ref()));
    }
    line(Utf8StringRef::from_cstr("}, new Label[] {"));
    for (auto* lb : labels) {
        line(textArena.intern(
            Utf8StringBuilder().append(getLabelName(lb).c_str()).append(", ").build().ref()));
    }
    line(Utf8StringRef::from_cstr("});"));
}

void ASMifier::visitMultiANewArrayInsn(Utf8StringRef descriptor, int numDimensions)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitMultiANewArrayInsn(")
                              .append(appendString(descriptor).ref())
                              .append(", ")
                              .append(std::to_string(numDimensions).c_str())
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitTryCatchBlock(Label* start, Label* end, Label* handler, Utf8StringRef type)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitTryCatchBlock(")
                              .append(getLabelName(start).c_str())
                              .append(", ")
                              .append(getLabelName(end).c_str())
                              .append(", ")
                              .append(getLabelName(handler).c_str())
                              .append(", ")
                              .append(appendString(type).ref())
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitLocalVariable(Utf8StringRef name, Utf8StringRef descriptor,
                                  Utf8StringRef signature, Label* start, Label* end, int index)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitLocalVariable(")
                              .append(appendString(name).ref())
                              .append(", ")
                              .append(appendString(descriptor).ref())
                              .append(", ")
                              .append(signature.is_empty() ? Utf8StringRef::from_cstr("null")
                                                           : appendString(signature).ref())
                              .append(", ")
                              .append(getLabelName(start).c_str())
                              .append(", ")
                              .append(getLabelName(end).c_str())
                              .append(", ")
                              .append(std::to_string(index).c_str())
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitLineNumber(int lineNum, Label* start)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitLineNumber(")
                              .append(std::to_string(lineNum).c_str())
                              .append(", ")
                              .append(getLabelName(start).c_str())
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitMaxs(int maxStack, int maxLocals)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitMaxs(")
                              .append(std::to_string(maxStack).c_str())
                              .append(", ")
                              .append(std::to_string(maxLocals).c_str())
                              .append(");")
                              .build()
                              .ref()));
}

void ASMifier::visitMethodEnd() {}

void ASMifier::visitAnnotationValue(Utf8StringRef name, const ca::core::Any& value)
{
    auto b350 = Utf8StringBuilder();
    b350.append(varName_.c_str())
        .append(".visit(")
        .append(name.is_empty() ? Utf8StringRef::from_cstr("null") : appendString(name).ref())
        .append(", ")
        .append(toASMValue(value).c_str())
        .append(");");
    line(textArena.intern(b350.build().ref()));
}

void ASMifier::visitAnnotationEnum(Utf8StringRef name, Utf8StringRef descriptor,
                                   Utf8StringRef value)
{
    line(textArena.intern(Utf8StringBuilder()
                              .append(varName_.c_str())
                              .append(".visitEnum(")
                              .append(appendString(name).ref())
                              .append(", ")
                              .append(appendString(descriptor).ref())
                              .append(", ")
                              .append(appendString(value).ref())
                              .append(");")
                              .build()
                              .ref()));
}

Printer* ASMifier::visitAnnotationAnnotation(Utf8StringRef name, Utf8StringRef descriptor)
{
    std::string av = nextVar();
    line(textArena.intern(Utf8StringBuilder()
                              .append(av.c_str())
                              .append(" = ")
                              .append(varName_.c_str())
                              .append(".visitAnnotation(")
                              .append(appendString(name).ref())
                              .append(", ")
                              .append(appendString(descriptor).ref())
                              .append(");")
                              .build()
                              .ref()));
    auto* sub     = subASMifier();
    sub->varName_ = av;
    return sub;
}

Printer* ASMifier::visitAnnotationArray(Utf8StringRef name)
{
    std::string av = nextVar();
    line(textArena.intern(Utf8StringBuilder()
                              .append(av.c_str())
                              .append(" = ")
                              .append(varName_.c_str())
                              .append(".visitArray(")
                              .append(appendString(name).ref())
                              .append(");")
                              .build()
                              .ref()));
    auto* sub     = subASMifier();
    sub->varName_ = av;
    return sub;
}

void ASMifier::visitAnnotationEnd() {}

std::string ASMifier::getLabelName(Label* label)
{
    auto it = labelNames_.find(label);
    if (it != labelNames_.end()) return it->second;
    std::string name   = "L" + std::to_string(nextLabelId_++);
    labelNames_[label] = name;
    return name;
}
