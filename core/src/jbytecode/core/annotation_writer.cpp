/// @file        annotation_writer.cpp
/// @brief       AnnotationWriter implementation — generates 'annotation' / 'type_annotation'
///              JVMS structures (JVMS 4.7.16, 4.7.20).
///
///              Follows ASM AnnotationWriter.java semantics: collect element_value data
///              during visit*, serialize via putAnnotation().
///
/// @author      Eric Bruneton (ASM), C++ port

#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/core/type_reference.hpp"
#include <cstring>

using namespace ca::str;
using namespace ca::core;

namespace jbytecode::core {

// Helper: get CP index from Symbol* (returned by addConstant*)
static inline int cpIdx(ca::i32 idx)
{
    return idx;
}
static inline int cpIdx(Symbol* sym)
{
    return sym ? sym->index : 0;
}

// ---- visit* methods: collect elements ----

void AnnotationWriter::visit(Utf8StringRef name, const Any& value)
{
    Element el;
    el.name = name;

    if (!value.has_value()) {
        el.tag  = 's';
        el.sval = Utf8StringRef();
    }
    else if (value.is<Utf8StringRef>()) {
        el.tag  = 's';
        el.sval = value.cast<Utf8StringRef>();
    }
    else if (value.is<ZUtf8StringRef>()) {
        el.tag  = 's';
        el.sval = value.cast<ZUtf8StringRef>().ref();
    }
    else if (value.is<int>()) {
        el.tag  = 'I';
        el.ival = value.cast<int>();
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
    else if (value.is<bool>()) {
        el.tag  = 'Z';
        el.ival = value.cast<bool>() ? 1 : 0;
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
    else if (value.is<Type>()) {
        el.tag = 'c';
        // #921 staticArena 退役：描述符 intern 落点必选注入（SymbolTable 的作业 arena）。
        el.cls = symbols_.arena().intern(value.cast<Type>().getDescriptor(symbols_.arena()));
    }
    else if (value.is<Handle>()) {
        el.tag  = 's';
        el.sval = symbols_.arena().intern("(Handle)");
    }
    else if (value.is<ConstantDynamic>()) {
        el.tag  = 's';
        el.sval = symbols_.arena().intern("(ConstDynamic)");
    }
    else {
        // Unknown type — store as string via arena.
        el.tag  = 's';
        el.sval = symbols_.arena().intern("(unknown)");
    }
    elements_.push_back(el);
}

void AnnotationWriter::visitEnum(Utf8StringRef name, Utf8StringRef descriptor, Utf8StringRef value)
{
    Element el;
    el.name     = name;
    el.tag      = 'e';
    el.enumType = descriptor;
    el.enumName = value;
    elements_.push_back(el);
}

AnnotationVisitor* AnnotationWriter::visitAnnotation(Utf8StringRef name, Utf8StringRef descriptor)
{
    Element el;
    el.name = name;
    el.tag  = '@';
    // Nested annotations use named values (standard annotation within annotation)
    auto* child = new AnnotationWriter(symbols_, descriptor, true);
    el.nested   = child;
    children_.push_back(child);
    elements_.push_back(el);
    return child;
}

AnnotationVisitor* AnnotationWriter::visitArray(Utf8StringRef name)
{
    Element el;
    el.name = name;
    el.tag  = '[';
    // Array elements use unnamed values
    auto* child = new AnnotationWriter(symbols_, Utf8StringRef(), false);
    el.nested   = child;
    children_.push_back(child);
    elements_.push_back(el);
    return child;
}

// ---- writeElement: serialize a single element_value ----

void AnnotationWriter::writeElement(ByteVector& out, const Element& el)
{
    // '[' and '@' elements have their children serialized separately
    // via putAnnotation() — they already contain their own type_index/num_pairs header.
    switch (el.tag) {
    case 'B':   // byte
        out.put12('B', cpIdx(symbols_.addConstantInteger(el.ival)));
        break;
    case 'C':   // char
        out.put12('C', cpIdx(symbols_.addConstantInteger(el.ival)));
        break;
    case 'D':   // double
        out.put12('D', cpIdx(symbols_.addConstantDouble(el.dval)));
        break;
    case 'F':   // float
        out.put12('F', cpIdx(symbols_.addConstantFloat(el.fval)));
        break;
    case 'I':   // int
        out.put12('I', cpIdx(symbols_.addConstantInteger(el.ival)));
        break;
    case 'J':   // long
        out.put12('J', cpIdx(symbols_.addConstantLong(el.lval)));
        break;
    case 'S':   // short
        out.put12('S', cpIdx(symbols_.addConstantInteger(el.ival)));
        break;
    case 'Z':   // boolean
        out.put12('Z', cpIdx(symbols_.addConstantInteger(el.ival)));
        break;
    case 's':
    {   // String
        int idx = symbols_.addConstantUtf8(el.sval);
        out.put12('s', idx);
        break;
    }
    case 'e':
    {   // enum constant
        int typeIdx = symbols_.addConstantUtf8(el.enumType);
        int valIdx  = symbols_.addConstantUtf8(el.enumName);
        out.put12('e', typeIdx).putShort(valIdx);
        break;
    }
    case 'c':
    {   // class literal
        int idx = symbols_.addConstantUtf8(el.cls);
        out.put12('c', idx);
        break;
    }
    case '@':
    {   // annotation
        out.putByte('@');
        if (el.nested) {
            el.nested->putAnnotation(out);
        }
        break;
    }
    case '[':
    {   // array
        if (el.nested) {
            // Write '[' tag and num_values
            int numValues = static_cast<int>(el.nested->elements_.size());
            out.put12('[', numValues);
            for (const auto& childEl : el.nested->elements_) {
                writeElement(out, childEl);
            }
        }
        else {
            out.put12('[', 0);
        }
        break;
    }
    default: break;
    }
}

void AnnotationWriter::collectConstants()
{
    for (const auto& el : elements_) {
        if (named_ && el.name.data() != nullptr && el.name.byte_length() > 0) {
            symbols_.addConstantUtf8(el.name);
        }
        switch (el.tag) {
        case 'B':
        case 'C':
        case 'I':
        case 'S':
        case 'Z': symbols_.addConstantInteger(el.ival); break;
        case 'D': symbols_.addConstantDouble(el.dval); break;
        case 'F': symbols_.addConstantFloat(el.fval); break;
        case 'J': symbols_.addConstantLong(el.lval); break;
        case 's': symbols_.addConstantUtf8(el.sval); break;
        case 'e':
            symbols_.addConstantUtf8(el.enumType);
            symbols_.addConstantUtf8(el.enumName);
            break;
        case 'c': symbols_.addConstantUtf8(el.cls); break;
        case '@':
        case '[':
            if (el.nested) el.nested->collectConstants();
            break;
        default: break;
        }
    }
}

// ---- putAnnotation: serialize the full annotation structure ----

void AnnotationWriter::putAnnotation(ByteVector& out)
{
    // JVMS 4.7.16: annotation {
    //     u2 type_index;
    //     u2 num_element_value_pairs;
    //     element_value_pairs[num_element_value_pairs];
    // }
    // Each element_value_pair has:
    //   u2 element_name_index;  (absent for AnnotationDefault / array elements)
    //   element_value value;
    out.putShort(typeIndex_);
    out.putShort(static_cast<int>(elements_.size()));
    for (const auto& el : elements_) {
        if (named_) {
            // Named mode: normal annotation — element_name_index is always required
            out.putShort(el.name.data() != nullptr && el.name.byte_length() > 0
                             ? symbols_.addConstantUtf8(el.name)
                             : 0);
        }
        writeElement(out, el);
    }
}

void AnnotationWriter::putAnnotationDefault(ByteVector& out)
{
    if (!elements_.empty()) {
        writeElement(out, elements_.front());
    }
}

// ---- putAnnotations: serialize a list of annotations as an attribute ----

void AnnotationWriter::putAnnotations(ByteVector&                           out,
                                      const std::vector<AnnotationWriter*>& annotations)
{
    // JVMS 4.7.16: RuntimeVisibleAnnotations_attribute {
    //     u2 attribute_name_index;  (caller writes this)
    //     u4 attribute_length;
    //     u2 num_annotations;
    //     annotation annotations[num_annotations];
    // }
    // The attribute_name_index and attribute_length are written by the caller (e.g. FieldWriter).
    // Here we write only num_annotations + each annotation.
    out.putShort(static_cast<int>(annotations.size()));
    for (auto* ann : annotations) {
        if (ann) {
            ann->putAnnotation(out);
        }
    }
}

// ---- Type annotation support ----

void AnnotationWriter::setTypeAnnotation(int typeRef, const TypePath* typePath)
{
    typeRef_ = typeRef;
    if (typePath) {
        typePath_ = *typePath;
    }
}

void AnnotationWriter::setLocalVariableTarget(const std::vector<Label*>& start,
                                              const std::vector<Label*>& end,
                                              const std::vector<int>&    index)
{
    lvStart_ = start;
    lvEnd_   = end;
    lvIndex_ = index;
}

void AnnotationWriter::putTypeAnnotation(ByteVector& out)
{
    // JVMS 4.7.20: type_annotation { u1 target_type; union target_info;
    //     type_path target_path; u2 type_index; u2 num_pairs; pairs[]; }
    const int sort = static_cast<unsigned>(typeRef_) >> 24;
    if (sort == TypeReference::LOCAL_VARIABLE || sort == TypeReference::RESOURCE_VARIABLE) {
        // offset 目标形态（JVMS 4.7.20.1）：target_type + u2 table_length +
        // { u2 start_pc; u2 length; u2 index; }[]。偏移读 Label 解析值——
        // put 阶段指令已写出、跳转已 resolve（TypeReference::putTarget 不支持
        // 这两个目标类型，故在此分派）。
        out.putByte(static_cast<ca::u8>(sort));
        out.putShort(static_cast<int>(lvStart_.size()));
        for (std::size_t i = 0; i < lvStart_.size(); ++i) {
            const int startPc = lvStart_[i]->getOffset();
            const int length  = lvEnd_[i]->getOffset() - startPc;
            out.putShort(startPc).putShort(length).putShort(lvIndex_[i]);
        }
    }
    else {
        // Use TypeReference::putTarget to write target_type + target_info bytes.
        TypeReference::putTarget(typeRef_, out);
    }

    // Write TypePath
    int pathLength = typePath_.getLength();
    out.putByte(static_cast<ca::u8>(pathLength));
    for (int i = 0; i < pathLength; i++) {
        out.putByte(static_cast<ca::u8>(typePath_.getStep(i)));
        out.putByte(static_cast<ca::u8>(typePath_.getStepArgument(i)));
    }

    // Write annotation body (type_index + num_pairs + pairs)
    putAnnotation(out);
}

void AnnotationWriter::putTypeAnnotations(ByteVector&                           out,
                                          const std::vector<AnnotationWriter*>& annotations)
{
    out.putShort(static_cast<int>(annotations.size()));
    for (auto* ann : annotations) {
        if (ann) {
            ann->putTypeAnnotation(out);
        }
    }
}

}   // namespace jbytecode::core
