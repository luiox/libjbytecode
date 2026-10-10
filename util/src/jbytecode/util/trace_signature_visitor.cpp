#include "jbytecode/util/trace_signature_visitor.hpp"
#include "jbytecode/core/opcodes.hpp"

#include <string_view>

using namespace jbytecode::util;
using namespace jbytecode::core;

static const char* COMMA_SEPARATOR      = ", ";
static const char* EXTENDS_SEPARATOR    = " extends ";
static const char* IMPLEMENTS_SEPARATOR = " implements ";

// 把 Utf8StringRef 作为 string_view 写入 ostringstream（避免经 std::string 中转）。
static inline std::string_view refView(ca::str::Utf8StringRef s)
{
    return std::string_view(reinterpret_cast<const char*>(s.data()), s.byte_length());
}

std::string TraceSignatureVisitor::baseTypeName(char descriptor)
{
    switch (descriptor) {
    case 'Z': return "boolean";
    case 'B': return "byte";
    case 'C': return "char";
    case 'S': return "short";
    case 'I': return "int";
    case 'J': return "long";
    case 'F': return "float";
    case 'D': return "double";
    case 'V': return "void";
    default: return "";
    }
}

TraceSignatureVisitor::TraceSignatureVisitor(int accessFlags)
    : SignatureVisitor(Opcodes::ASM4)
    , isInterface_((accessFlags & Opcodes::ACC_INTERFACE) != 0)
    , activeSb_(&declaration_)
{}

TraceSignatureVisitor::~TraceSignatureVisitor()
{
    delete returnType_;
    delete exceptions_;
}

TraceSignatureVisitor::TraceSignatureVisitor(std::ostringstream* ss)
    : SignatureVisitor(Opcodes::ASM4)
    , isInterface_(false)
    , activeSb_(ss)
{}

void TraceSignatureVisitor::visitFormalTypeParameter(ca::str::Utf8StringRef name)
{
    if (formalTypeParameterVisited_) {
        *activeSb_ << COMMA_SEPARATOR;
    }
    else {
        *activeSb_ << '<';
    }
    *activeSb_ << refView(name);
    formalTypeParameterVisited_ = true;
    interfaceBoundVisited_      = false;
}

SignatureVisitor* TraceSignatureVisitor::visitClassBound()
{
    separator_ = EXTENDS_SEPARATOR;
    return this;
}

SignatureVisitor* TraceSignatureVisitor::visitInterfaceBound()
{
    separator_             = interfaceBoundVisited_ ? COMMA_SEPARATOR : EXTENDS_SEPARATOR;
    interfaceBoundVisited_ = true;
    return this;
}

SignatureVisitor* TraceSignatureVisitor::visitSuperclass()
{
    endFormals();
    separator_ = EXTENDS_SEPARATOR;
    return this;
}

SignatureVisitor* TraceSignatureVisitor::visitInterface()
{
    if (interfaceVisited_) {
        separator_ = COMMA_SEPARATOR;
    }
    else {
        separator_        = isInterface_ ? EXTENDS_SEPARATOR : IMPLEMENTS_SEPARATOR;
        interfaceVisited_ = true;
    }
    return this;
}

SignatureVisitor* TraceSignatureVisitor::visitParameterType()
{
    endFormals();
    if (parameterTypeVisited_) {
        declaration_ << COMMA_SEPARATOR;
    }
    else {
        declaration_ << '(';
        parameterTypeVisited_ = true;
    }
    activeSb_ = &declaration_;
    return this;
}

SignatureVisitor* TraceSignatureVisitor::visitReturnType()
{
    endFormals();
    if (parameterTypeVisited_) {
        parameterTypeVisited_ = false;
    }
    else {
        declaration_ << '(';
    }
    declaration_ << ')';
    returnType_ = new std::ostringstream();
    auto* sub   = new TraceSignatureVisitor(returnType_);
    ownedSubVisitors_.emplace_back(sub);
    return sub;
}

SignatureVisitor* TraceSignatureVisitor::visitExceptionType()
{
    if (exceptions_ == nullptr) {
        exceptions_ = new std::ostringstream();
    }
    else {
        *exceptions_ << COMMA_SEPARATOR;
    }
    auto* sub = new TraceSignatureVisitor(exceptions_);
    ownedSubVisitors_.emplace_back(sub);
    return sub;
}

void TraceSignatureVisitor::visitBaseType(char descriptor)
{
    std::string name = baseTypeName(descriptor);
    if (name.empty()) throw std::invalid_argument("Unknown base type");
    *activeSb_ << name;
    endType();
}

void TraceSignatureVisitor::visitTypeVariable(ca::str::Utf8StringRef name)
{
    *activeSb_ << separator_ << refView(name);
    separator_.clear();
    endType();
}

SignatureVisitor* TraceSignatureVisitor::visitArrayType()
{
    pendingTasks_.push_back(Task::END_ARRAY_TYPE);
    return this;
}

void TraceSignatureVisitor::visitClassType(ca::str::Utf8StringRef name)
{
    std::string dotted(reinterpret_cast<const char*>(name.data()), name.byte_length());
    for (auto& c : dotted)
        if (c == '/') c = '.';

    bool needObjectClass = false;
    if (name.equals("java/lang/Object")) {
        needObjectClass = (!pendingTasks_.empty() &&
                           pendingTasks_.back() == Task::END_NON_EMPTY_TYPE_ARGUMENTS) ||
                          parameterTypeVisited_;
    }

    if (!name.equals("java/lang/Object") || needObjectClass) {
        *activeSb_ << separator_ << dotted;
    }
    separator_.clear();
    pendingTasks_.push_back(Task::END_EMPTY_TYPE_ARGUMENTS);
}

void TraceSignatureVisitor::visitInnerClassType(ca::str::Utf8StringRef name)
{
    endTypeArguments();
    std::string dotted(reinterpret_cast<const char*>(name.data()), name.byte_length());
    for (auto& c : dotted)
        if (c == '/') c = '.';
    *activeSb_ << '.' << separator_ << dotted;
    pendingTasks_.push_back(Task::END_EMPTY_TYPE_ARGUMENTS);
    separator_.clear();
}

void TraceSignatureVisitor::visitTypeArgument()
{
    int lastTaskIndex = static_cast<int>(pendingTasks_.size()) - 1;
    if (pendingTasks_[lastTaskIndex] == Task::END_EMPTY_TYPE_ARGUMENTS) {
        pendingTasks_[lastTaskIndex] = Task::END_NON_EMPTY_TYPE_ARGUMENTS;
        *activeSb_ << '<';
    }
    else {
        *activeSb_ << COMMA_SEPARATOR;
    }
    *activeSb_ << '?';
}

SignatureVisitor* TraceSignatureVisitor::visitTypeArgument(char tag)
{
    int lastTaskIndex = static_cast<int>(pendingTasks_.size()) - 1;
    if (pendingTasks_[lastTaskIndex] == Task::END_EMPTY_TYPE_ARGUMENTS) {
        pendingTasks_[lastTaskIndex] = Task::END_NON_EMPTY_TYPE_ARGUMENTS;
        *activeSb_ << '<';
    }
    else {
        *activeSb_ << COMMA_SEPARATOR;
    }
    if (tag == '+') {
        *activeSb_ << "? extends ";
    }
    else if (tag == '-') {
        *activeSb_ << "? super ";
    }
    return this;
}

void TraceSignatureVisitor::visitEnd()
{
    endTypeArguments();
    endType();
}

void TraceSignatureVisitor::endFormals()
{
    if (formalTypeParameterVisited_) {
        declaration_ << '>';
        formalTypeParameterVisited_ = false;
    }
}

void TraceSignatureVisitor::endTypeArguments()
{
    if (!pendingTasks_.empty() && pendingTasks_.back() == Task::END_NON_EMPTY_TYPE_ARGUMENTS) {
        *activeSb_ << '>';
    }
    if (!pendingTasks_.empty()) {
        pendingTasks_.pop_back();
    }
}

void TraceSignatureVisitor::endType()
{
    int numTasks = static_cast<int>(pendingTasks_.size());
    while (numTasks > 0) {
        int idx = numTasks - 1;
        if (pendingTasks_[idx] != Task::END_ARRAY_TYPE) break;
        *activeSb_ << "[]";
        pendingTasks_.erase(pendingTasks_.begin() + idx);
        numTasks = static_cast<int>(pendingTasks_.size());
    }
}
