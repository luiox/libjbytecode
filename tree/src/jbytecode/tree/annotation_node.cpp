#include "jbytecode/tree/nodes.hpp"

using namespace jbytecode::tree;
using namespace jbytecode::core;

void AnnotationNode::visit(ca::str::Utf8StringRef name, const ca::core::Any& value)
{
    Element el;
    el.name = name;
    if (value.is<std::string>()) {
        el.tag        = 's';
        const auto& s = value.cast<std::string>();
        el.sval = ca::str::Utf8String(reinterpret_cast<const unsigned char*>(s.data()), s.size());
    }
    else if (value.is<ca::str::Utf8StringRef>()) {
        el.tag       = 's';
        const auto s = value.cast<ca::str::Utf8StringRef>();
        el.sval      = ca::str::Utf8String(s.data(), s.byte_length());
    }
    else if (value.is<ca::str::ZUtf8StringRef>()) {
        el.tag       = 's';
        const auto s = value.cast<ca::str::ZUtf8StringRef>().ref();
        el.sval      = ca::str::Utf8String(s.data(), s.byte_length());
    }
    else if (value.is<int>()) {
        el.tag  = 'I';
        el.ival = value.cast<int>();
    }
    else if (value.is<ca::i8>()) {
        el.tag  = 'B';
        el.ival = value.cast<ca::i8>();
    }
    else if (value.is<ca::u16>()) {
        el.tag  = 'C';
        el.ival = value.cast<ca::u16>();
    }
    else if (value.is<ca::i16>()) {
        el.tag  = 'S';
        el.ival = value.cast<ca::i16>();
    }
    else if (value.is<bool>()) {
        el.tag  = 'Z';
        el.ival = value.cast<bool>() ? 1 : 0;
    }
    else if (value.is<ca::i64>()) {
        el.tag  = 'J';
        el.lval = value.cast<ca::i64>();
    }
    else if (value.is<float>()) {
        el.tag  = 'F';
        el.fval = value.cast<float>();
    }
    else if (value.is<double>()) {
        el.tag  = 'D';
        el.dval = value.cast<double>();
    }
    else if (value.is<Type>()) {
        el.tag = 'c';
        // #921 staticArena 退役：字节仅本调用内瞬时消费（立即深拷贝进 el.sval），确无
        // owner 上下文——按裁决①给显式函数级局部 arena，零驻留。
        ca::str::Utf8StringArena scratch;
        const auto               d = value.cast<Type>().getDescriptor(scratch);
        el.sval                    = ca::str::Utf8String(d.data(), d.byte_length());
    }
    else {
        el.tag = 's';
    }
    elements.push_back(std::move(el));
}

void AnnotationNode::visitEnum(ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                               ca::str::Utf8StringRef value)
{
    Element el;
    el.name = name;
    el.tag  = 'e';
    ca::str::Utf8StringBuilder sb;
    sb.append(desc).append(" ").append(value);
    el.sval = sb.build();
    elements.push_back(std::move(el));
}

AnnotationVisitor* AnnotationNode::visitAnnotation(ca::str::Utf8StringRef name,
                                                   ca::str::Utf8StringRef descriptor)
{
    auto*   child = contentArena_.create<AnnotationNode>(contentArena_, descriptor);
    Element el;
    el.name   = name;
    el.tag    = '@';
    el.nested = child;
    elements.push_back(std::move(el));
    children.push_back(child);
    return child;
}

AnnotationVisitor* AnnotationNode::visitArray(ca::str::Utf8StringRef name)
{
    auto*   child = contentArena_.create<AnnotationNode>(contentArena_, ca::str::Utf8StringRef());
    Element el;
    el.name   = name;
    el.tag    = '[';
    el.nested = child;
    elements.push_back(std::move(el));
    children.push_back(child);
    return child;
}

void AnnotationNode::accept(AnnotationVisitor* av)
{
    for (auto& el : elements) {
        switch (el.tag) {
        case 's': av->visit(el.name, el.sval.ref()); break;
        case 'B': av->visit(el.name, static_cast<ca::i8>(el.ival)); break;
        case 'C': av->visit(el.name, static_cast<ca::u16>(el.ival)); break;
        case 'S': av->visit(el.name, static_cast<ca::i16>(el.ival)); break;
        case 'Z': av->visit(el.name, el.ival != 0); break;
        case 'I': av->visit(el.name, el.ival); break;
        case 'J': av->visit(el.name, el.lval); break;
        case 'F': av->visit(el.name, el.fval); break;
        case 'D': av->visit(el.name, el.dval); break;
        case 'c': av->visit(el.name, Type::getType(el.sval.ref())); break;
        case 'e':
        {
            auto s        = el.sval.ref();
            auto spacePos = s.byte_length();
            for (decltype(spacePos) i = 0; i < s.byte_length(); ++i) {
                if (s.byte_at(i) == ' ') {
                    spacePos = i;
                    break;
                }
            }
            av->visitEnum(el.name, s.slice(0, spacePos), s.slice(spacePos + 1, s.byte_length()));
            break;
        }
        case '@':
            if (el.nested) {
                auto* sub = av->visitAnnotation(el.name, el.nested->desc);
                if (sub) el.nested->accept(sub);
            }
            break;
        case '[':
            if (el.nested) {
                auto* sub = av->visitArray(el.name);
                if (sub) el.nested->accept(sub);
            }
            break;
        }
    }
    av->visitEnd();
}

void TypeAnnotationNode::accept(AnnotationVisitor* av)
{
    AnnotationNode::accept(av);
}
