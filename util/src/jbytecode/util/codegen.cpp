#include "jbytecode/util/codegen.hpp"
#include "jbytecode/core/type.hpp"

using namespace jbytecode::util;
using namespace jbytecode::core;
using namespace ca::str;

CodeGen::CodeGen()
    : Printer(/*latest api=*/9)
    , name_("classWriter")
    , id_(0)
{}

void CodeGen::decl(Utf8StringRef line)
{
    text.push_back(textArena.intern(line));
}

void CodeGen::varLine(Utf8StringRef call)
{
    auto b = Utf8StringBuilder();
    b.append(name_.c_str()).append(".").append(call);
    text.push_back(textArena.intern(b.build().ref()));
}

Utf8StringRef CodeGen::getLabelName(Label* label)
{
    auto it = labelNames_.find(label);
    if (it != labelNames_.end()) return it->second;
    auto b = Utf8StringBuilder();
    b.append("label").append(std::to_string(nextLabelId_++).c_str());
    Utf8StringRef nameRef = labelArena_.intern(b.build().ref());
    labelNames_[label]    = nameRef;
    b.clear();
    b.append("Label ").append(nameRef).append(" = new Label();");
    text.push_back(textArena.intern(b.build().ref()));
    return nameRef;
}

void CodeGen::declareLabel(Label* label)
{
    getLabelName(label);
}

CodeGen* CodeGen::subCodeGen(const std::string& varName)
{
    auto* sub  = new CodeGen();
    sub->name_ = varName;
    return sub;
}

std::string CodeGen::accessFlags(int access)
{
    std::string result;
    auto        addFlag = [&](int mask, const char* name) {
        if ((access & mask) != 0) {
            if (!result.empty()) result += " | ";
            result += name;
        }
    };
    addFlag(Opcodes::ACC_PUBLIC, "ACC_PUBLIC");
    addFlag(Opcodes::ACC_PRIVATE, "ACC_PRIVATE");
    addFlag(Opcodes::ACC_PROTECTED, "ACC_PROTECTED");
    addFlag(Opcodes::ACC_STATIC, "ACC_STATIC");
    addFlag(Opcodes::ACC_FINAL, "ACC_FINAL");
    addFlag(Opcodes::ACC_SUPER, "ACC_SUPER");
    addFlag(Opcodes::ACC_SYNCHRONIZED, "ACC_SYNCHRONIZED");
    addFlag(Opcodes::ACC_VOLATILE, "ACC_VOLATILE");
    addFlag(Opcodes::ACC_TRANSIENT, "ACC_TRANSIENT");
    addFlag(Opcodes::ACC_NATIVE, "ACC_NATIVE");
    addFlag(Opcodes::ACC_INTERFACE, "ACC_INTERFACE");
    addFlag(Opcodes::ACC_ABSTRACT, "ACC_ABSTRACT");
    addFlag(Opcodes::ACC_STRICT, "ACC_STRICT");
    addFlag(Opcodes::ACC_SYNTHETIC, "ACC_SYNTHETIC");
    addFlag(Opcodes::ACC_ANNOTATION, "ACC_ANNOTATION");
    addFlag(Opcodes::ACC_ENUM, "ACC_ENUM");
    addFlag(Opcodes::ACC_MODULE, "ACC_MODULE");
    if (result.empty()) result = "0";
    return result;
}

int CodeGen::getVersionConstant(int version)
{
    return version;
}

void CodeGen::appendConst(const ca::core::Any& value, Utf8StringBuilder& out)
{
    if (auto* ip = value.as<int>()) {
        out.append(std::to_string(*ip).c_str());
    }
    else if (value.is<long>()) {
        out.append(std::to_string(value.cast<long>()).c_str()).append("L");
    }
    else if (value.is<float>()) {
        out.append(std::to_string(value.cast<float>()).c_str()).append("f");
    }
    else if (value.is<double>()) {
        out.append(std::to_string(value.cast<double>()).c_str()).append("d");
    }
    else if (value.is<Utf8StringRef>()) {
        out.append("\"").append(value.cast<Utf8StringRef>()).append("\"");
    }
    else if (value.is<Type>()) {
        // #921 staticArena 退役：描述符仅瞬时 append 进文本，函数级局部 arena 零驻留。
        ca::str::Utf8StringArena scratch;
        out.append(value.cast<Type>().getDescriptor(scratch));
    }
}

void CodeGen::visit(int version, int access, Utf8StringRef name, Utf8StringRef signature,
                    Utf8StringRef superName, const std::vector<Utf8StringRef>& interfaces)
{
    auto b = Utf8StringBuilder();
    b.append("visit(V")
        .append(std::to_string(getVersionConstant(version)).c_str())
        .append(", ")
        .append(accessFlags(access).c_str())
        .append(", \"")
        .append(name)
        .append("\", ");
    if (signature.is_empty())
        b.append("null");
    else
        b.append("\"").append(signature).append("\"");
    b.append(", \"").append(superName).append("\", ");
    if (interfaces.empty())
        b.append("null");
    else {
        b.append("new String[] {");
        for (size_t i = 0; i < interfaces.size(); ++i) {
            if (i > 0) b.append(", ");
            b.append("\"").append(interfaces[i]).append("\"");
        }
        b.append("}");
    }
    b.append(");");
    varLine(b.build().ref());
}

void CodeGen::visitSource(Utf8StringRef source, Utf8StringRef debug)
{
    auto b = Utf8StringBuilder();
    b.append("visitSource(\"").append(source).append("\", ");
    if (debug.is_empty()) {
        b.append("null");
    }
    else {
        auto t = Utf8StringBuilder();
        t.append("\"").append(debug).append("\"");
        b.append(t.build().ref());
    }
    b.append(");");
    varLine(b.build().ref());
}
void CodeGen::visitOuterClass(Utf8StringRef owner, Utf8StringRef name, Utf8StringRef descriptor)
{
    auto b = Utf8StringBuilder();
    b.append("visitOuterClass(\"")
        .append(owner)
        .append("\", \"")
        .append(name)
        .append("\", \"")
        .append(descriptor)
        .append("\");");
    varLine(b.build().ref());
}
Printer* CodeGen::visitClassAnnotation(Utf8StringRef descriptor, bool visible)
{
    (void)visible;
    auto b = Utf8StringBuilder();
    b.append("visitAnnotation(\"")
        .append(descriptor)
        .append("\", ")
        .append(visible ? "true" : "false")
        .append(");");
    varLine(b.build().ref());
    auto* sub = subCodeGen(name_);
    return sub;
}
void CodeGen::visitInnerClass(Utf8StringRef name, Utf8StringRef outerName, Utf8StringRef innerName,
                              int access)
{
    auto b = Utf8StringBuilder();
    b.append("visitInnerClass(\"")
        .append(name)
        .append("\", \"")
        .append(outerName)
        .append("\", \"")
        .append(innerName)
        .append("\", ")
        .append(std::to_string(access).c_str())
        .append(");");
    varLine(b.build().ref());
}
Printer* CodeGen::visitField(int access, Utf8StringRef name, Utf8StringRef descriptor,
                             Utf8StringRef signature, const ca::core::Any& value)
{
    (void)signature;
    (void)value;
    auto b = Utf8StringBuilder();
    b.append("visitField(")
        .append(std::to_string(access).c_str())
        .append(", \"")
        .append(name)
        .append("\", \"")
        .append(descriptor)
        .append("\", null, null);");
    varLine(b.build().ref());
    auto* sub = subCodeGen(name_);
    return sub;
}
Printer* CodeGen::visitMethod(int access, Utf8StringRef name, Utf8StringRef descriptor,
                              Utf8StringRef signature, const std::vector<Utf8StringRef>& exceptions)
{
    (void)exceptions;
    auto b = Utf8StringBuilder();
    b.append("visitMethod(")
        .append(std::to_string(access).c_str())
        .append(", \"")
        .append(name)
        .append("\", \"")
        .append(descriptor)
        .append("\", ");
    if (signature.is_empty()) {
        b.append("null");
    }
    else {
        auto t = Utf8StringBuilder();
        t.append("\"").append(signature).append("\"");
        b.append(t.build().ref());
    }
    b.append(", null);");
    varLine(b.build().ref());
    auto* sub = subCodeGen(name_);
    return sub;
}
void CodeGen::visitClassEnd() {}
void CodeGen::visitCode() {}
void CodeGen::visitFrame(int, int, const std::vector<ca::core::Any>&, int,
                         const std::vector<ca::core::Any>&)
{}
void CodeGen::visitInsn(int opcode)
{
    auto b = Utf8StringBuilder();
    b.append("visitInsn(").append(OPCODES[opcode]).append(");");
    varLine(b.build().ref());
}
void CodeGen::visitIntInsn(int opcode, int operand)
{
    auto b = Utf8StringBuilder();
    b.append("visitIntInsn(")
        .append(OPCODES[opcode])
        .append(", ")
        .append(std::to_string(operand).c_str())
        .append(");");
    varLine(b.build().ref());
}
void CodeGen::visitVarInsn(int opcode, int varIndex)
{
    auto b = Utf8StringBuilder();
    b.append("visitVarInsn(")
        .append(OPCODES[opcode])
        .append(", ")
        .append(std::to_string(varIndex).c_str())
        .append(");");
    varLine(b.build().ref());
}
void CodeGen::visitTypeInsn(int opcode, Utf8StringRef type)
{
    auto b = Utf8StringBuilder();
    b.append("visitTypeInsn(").append(OPCODES[opcode]).append(", \"").append(type).append("\");");
    varLine(b.build().ref());
}
void CodeGen::visitFieldInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                             Utf8StringRef descriptor)
{
    auto b = Utf8StringBuilder();
    b.append("visitFieldInsn(")
        .append(OPCODES[opcode])
        .append(", \"")
        .append(owner)
        .append("\", \"")
        .append(name)
        .append("\", \"")
        .append(descriptor)
        .append("\");");
    varLine(b.build().ref());
}
void CodeGen::visitMethodInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                              Utf8StringRef descriptor, bool isInterface)
{
    auto b = Utf8StringBuilder();
    b.append("visitMethodInsn(")
        .append(OPCODES[opcode])
        .append(", \"")
        .append(owner)
        .append("\", \"")
        .append(name)
        .append("\", \"")
        .append(descriptor)
        .append("\", ")
        .append(isInterface ? "true" : "false")
        .append(");");
    varLine(b.build().ref());
}
void CodeGen::visitInvokeDynamicInsn(Utf8StringRef, Utf8StringRef, const Handle&,
                                     const std::vector<ca::core::Any>&)
{}
void CodeGen::visitJumpInsn(int opcode, Label* label)
{
    auto b = Utf8StringBuilder();
    b.append("visitJumpInsn(")
        .append(OPCODES[opcode])
        .append(", ")
        .append(getLabelName(label))
        .append(");");
    varLine(b.build().ref());
}
void CodeGen::visitLabel(Label* label)
{
    declareLabel(label);
}
void CodeGen::visitLdcInsn(const ca::core::Any& value)
{
    auto b = Utf8StringBuilder();
    b.append("visitLdcInsn(");
    appendConst(value, b);
    b.append(");");
    varLine(b.build().ref());
}
void CodeGen::visitIincInsn(int varIndex, int increment)
{
    auto b = Utf8StringBuilder();
    b.append("visitIincInsn(")
        .append(std::to_string(varIndex).c_str())
        .append(", ")
        .append(std::to_string(increment).c_str())
        .append(");");
    varLine(b.build().ref());
}
void CodeGen::visitTableSwitchInsn(int, int, Label*, const std::vector<Label*>&) {}
void CodeGen::visitLookupSwitchInsn(Label*, const std::vector<int>&, const std::vector<Label*>&) {}
void CodeGen::visitMultiANewArrayInsn(Utf8StringRef descriptor, int numDimensions)
{
    auto b = Utf8StringBuilder();
    b.append("visitMultiANewArrayInsn(\"")
        .append(descriptor)
        .append("\", ")
        .append(std::to_string(numDimensions).c_str())
        .append(");");
    varLine(b.build().ref());
}
void CodeGen::visitTryCatchBlock(Label* start, Label* end, Label* handler, Utf8StringRef type)
{
    auto b = Utf8StringBuilder();
    b.append("visitTryCatchBlock(")
        .append(getLabelName(start))
        .append(", ")
        .append(getLabelName(end))
        .append(", ")
        .append(getLabelName(handler))
        .append(", \"")
        .append(type)
        .append("\");");
    varLine(b.build().ref());
}
void CodeGen::visitLocalVariable(Utf8StringRef name, Utf8StringRef descriptor,
                                 Utf8StringRef signature, Label* start, Label* end, int index)
{
    auto b = Utf8StringBuilder();
    b.append("visitLocalVariable(\"")
        .append(name)
        .append("\", \"")
        .append(descriptor)
        .append("\", ")
        .append(signature.is_empty()
                    ? Utf8StringRef::from_cstr("null")
                    : Utf8StringBuilder().append("\"").append(signature).append("\"").build().ref())
        .append(", ")
        .append(getLabelName(start))
        .append(", ")
        .append(getLabelName(end))
        .append(", ")
        .append(std::to_string(index).c_str())
        .append(");");
    varLine(b.build().ref());
}
void CodeGen::visitLineNumber(int line, Label* start)
{
    auto b = Utf8StringBuilder();
    b.append("visitLineNumber(")
        .append(std::to_string(line).c_str())
        .append(", ")
        .append(getLabelName(start))
        .append(");");
    varLine(b.build().ref());
}
void     CodeGen::visitMaxs(int, int) {}
void     CodeGen::visitMethodEnd() {}
void     CodeGen::visitAnnotationValue(Utf8StringRef, const ca::core::Any&) {}
void     CodeGen::visitAnnotationEnum(Utf8StringRef, Utf8StringRef, Utf8StringRef) {}
Printer* CodeGen::visitAnnotationAnnotation(Utf8StringRef, Utf8StringRef)
{
    return nullptr;
}
Printer* CodeGen::visitAnnotationArray(Utf8StringRef)
{
    return nullptr;
}
void CodeGen::visitAnnotationEnd() {}
