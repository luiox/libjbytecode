/// @file        field_writer.cpp
/// @brief       FieldWriter implementation — collects field_info data during visit*,
///              provides accessors for ClassWriter to serialize.
///
///              Follows ASM FieldWriter.java semantics.
///
/// @author      Eric Bruneton (ASM), C++ port

#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/type.hpp"
#include <cstring>

using namespace ca::str;

namespace jbytecode::core {

FieldWriter::FieldWriter(SymbolTable& symbols, ca::i32 access, Utf8StringRef name,
                         Utf8StringRef descriptor, Utf8StringRef signature,
                         const ca::core::Any& value)
    : FieldVisitor(Opcodes::ASM9)
    , symbols_(symbols)
    , access_(access)
    , nameIndex_(symbols.addConstantUtf8(name))
    , descIndex_(symbols.addConstantUtf8(descriptor))
{
    if (signature.data() != nullptr && signature.byte_length() > 0) {
        signatureIndex_ = symbols.addConstantUtf8(signature);
    }
    if (value.has_value()) {
        if (value.is<int>()) {
            hasConstantValue_ = true;
            constantValueIdx_ = symbols.addConstantInteger(value.cast<int>())->index;
        }
        else if (value.is<ca::i64>()) {
            hasConstantValue_ = true;
            constantValueIdx_ = symbols.addConstantLong(value.cast<ca::i64>())->index;
        }
        else if (value.is<float>()) {
            hasConstantValue_ = true;
            constantValueIdx_ = symbols.addConstantFloat(value.cast<float>())->index;
        }
        else if (value.is<double>()) {
            hasConstantValue_ = true;
            constantValueIdx_ = symbols.addConstantDouble(value.cast<double>())->index;
        }
        else if (value.is<Utf8StringRef>()) {
            hasConstantValue_ = true;
            constantValueIdx_ = symbols.addConstantString(value.cast<Utf8StringRef>())->index;
        }
        // Other types (e.g. long/double handled, Handle/Type not valid for field constant)
    }
}

AnnotationVisitor* FieldWriter::visitAnnotation(Utf8StringRef descriptor, bool visible)
{
    auto* ann = new AnnotationWriter(symbols_, descriptor);
    ownedAnns_.push_back(ann);
    if (visible) {
        visibleAnnotations_.push_back(ann);
    }
    else {
        invisibleAnnotations_.push_back(ann);
    }
    return ann;
}

AnnotationVisitor* FieldWriter::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                    Utf8StringRef descriptor, bool visible)
{
    auto* ann = new AnnotationWriter(symbols_, descriptor);
    ann->setTypeAnnotation(typeRef, typePath);
    ownedAnns_.push_back(ann);
    if (visible) {
        visibleTypeAnnotations_.push_back(ann);
    }
    else {
        invisibleTypeAnnotations_.push_back(ann);
    }
    return ann;
}

void FieldWriter::collectConstants()
{
    if (hasConstantValue_) {
        symbols_.addConstantUtf8(Constants::CONSTANT_VALUE);
    }
    if (signatureIndex_ != 0) {
        symbols_.addConstantUtf8(Constants::SIGNATURE);
    }
    if ((access_ & Opcodes::ACC_DEPRECATED) != 0) {
        symbols_.addConstantUtf8(Constants::DEPRECATED);
    }
    if ((access_ & Opcodes::ACC_SYNTHETIC) != 0) {
        symbols_.addConstantUtf8(Constants::SYNTHETIC);
    }
    if (!visibleAnnotations_.empty()) {
        symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_ANNOTATIONS);
    }
    if (!invisibleAnnotations_.empty()) {
        symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_ANNOTATIONS);
    }
    if (!visibleTypeAnnotations_.empty()) {
        symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_TYPE_ANNOTATIONS);
    }
    if (!invisibleTypeAnnotations_.empty()) {
        symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_TYPE_ANNOTATIONS);
    }
    for (const auto& attr : attrs_) {
        symbols_.addConstantUtf8(attr.type.ref());
    }
    for (auto* a : visibleAnnotations_)
        if (a) a->collectConstants();
    for (auto* a : invisibleAnnotations_)
        if (a) a->collectConstants();
    for (auto* a : visibleTypeAnnotations_)
        if (a) a->collectConstants();
    for (auto* a : invisibleTypeAnnotations_)
        if (a) a->collectConstants();
}

}   // namespace jbytecode::core
