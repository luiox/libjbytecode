/// @file        record_component_writer.cpp
/// @brief       RecordComponentWriter 实现 — 从 writers.hpp 内联实现迁移而来。
///              对标 org.objectweb.asm.RecordComponentWriter。
///
///              原本全部内联在 writers.hpp 中，违反"声明/实现分离"约定，
///              现移出以保证头文件只含声明。

#include "jbytecode/core/writers.hpp"

using namespace ca::str;

namespace jbytecode::core {

RecordComponentWriter::RecordComponentWriter(SymbolTable& symbols, Utf8StringRef name,
                                             Utf8StringRef descriptor, Utf8StringRef signature)
    : RecordComponentVisitor(Opcodes::ASM9)
    , symbols_(symbols)
    , nameIndex_(symbols.addConstantUtf8(name))
    , descIndex_(symbols.addConstantUtf8(descriptor))
    , signatureIndex_(
          signature.data() && signature.byte_length() > 0 ? symbols.addConstantUtf8(signature) : 0)
{}

RecordComponentWriter::~RecordComponentWriter()
{
    for (auto* a : ownedAnns_) delete a;
}

AnnotationVisitor* RecordComponentWriter::visitAnnotation(Utf8StringRef descriptor, bool visible)
{
    auto* ann = new AnnotationWriter(symbols_, descriptor);
    ownedAnns_.push_back(ann);
    if (visible)
        visibleAnnotations_.push_back(ann);
    else
        invisibleAnnotations_.push_back(ann);
    return ann;
}

AnnotationVisitor* RecordComponentWriter::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                              Utf8StringRef descriptor,
                                                              bool          visible)
{
    auto* ann = new AnnotationWriter(symbols_, descriptor);
    ownedAnns_.push_back(ann);
    ann->setTypeAnnotation(typeRef, typePath);
    if (visible)
        visibleTypeAnnotations_.push_back(ann);
    else
        invisibleTypeAnnotations_.push_back(ann);
    return ann;
}

void RecordComponentWriter::collectConstants()
{
    if (signatureIndex_) {
        symbols_.addConstantUtf8(Constants::SIGNATURE);
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
    for (auto* a : visibleAnnotations_)
        if (a) a->collectConstants();
    for (auto* a : invisibleAnnotations_)
        if (a) a->collectConstants();
    for (auto* a : visibleTypeAnnotations_)
        if (a) a->collectConstants();
    for (auto* a : invisibleTypeAnnotations_)
        if (a) a->collectConstants();
}

int RecordComponentWriter::computeComponentSize() const
{
    int size = 6;
    if (signatureIndex_) size += 8;
    if (!visibleAnnotations_.empty()) {
        ByteVector tmp;
        AnnotationWriter::putAnnotations(tmp, visibleAnnotations_);
        size += 6 + tmp.size();
    }
    if (!invisibleAnnotations_.empty()) {
        ByteVector tmp;
        AnnotationWriter::putAnnotations(tmp, invisibleAnnotations_);
        size += 6 + tmp.size();
    }
    if (!visibleTypeAnnotations_.empty()) {
        ByteVector tmp;
        AnnotationWriter::putTypeAnnotations(tmp, visibleTypeAnnotations_);
        size += 6 + tmp.size();
    }
    if (!invisibleTypeAnnotations_.empty()) {
        ByteVector tmp;
        AnnotationWriter::putTypeAnnotations(tmp, invisibleTypeAnnotations_);
        size += 6 + tmp.size();
    }
    return size;
}

void RecordComponentWriter::putComponent(ByteVector& out)
{
    int attrCount = 0;
    if (signatureIndex_) ++attrCount;
    if (!visibleAnnotations_.empty()) ++attrCount;
    if (!invisibleAnnotations_.empty()) ++attrCount;
    if (!visibleTypeAnnotations_.empty()) ++attrCount;
    if (!invisibleTypeAnnotations_.empty()) ++attrCount;

    out.putShort(nameIndex_).putShort(descIndex_).putShort(attrCount);

    if (signatureIndex_) {
        int nameIdx = symbols_.addConstantUtf8(Constants::SIGNATURE);
        out.putShort(nameIdx).putInt(2).putShort(signatureIndex_);
    }
    if (!visibleAnnotations_.empty()) {
        int        nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_ANNOTATIONS);
        ByteVector tmp;
        AnnotationWriter::putAnnotations(tmp, visibleAnnotations_);
        out.putShort(nameIdx).putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }
    if (!invisibleAnnotations_.empty()) {
        int        nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_ANNOTATIONS);
        ByteVector tmp;
        AnnotationWriter::putAnnotations(tmp, invisibleAnnotations_);
        out.putShort(nameIdx).putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }
    if (!visibleTypeAnnotations_.empty()) {
        int        nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_TYPE_ANNOTATIONS);
        ByteVector tmp;
        AnnotationWriter::putTypeAnnotations(tmp, visibleTypeAnnotations_);
        out.putShort(nameIdx).putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }
    if (!invisibleTypeAnnotations_.empty()) {
        int nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_TYPE_ANNOTATIONS);
        ByteVector tmp;
        AnnotationWriter::putTypeAnnotations(tmp, invisibleTypeAnnotations_);
        out.putShort(nameIdx).putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }
}

}   // namespace jbytecode::core
