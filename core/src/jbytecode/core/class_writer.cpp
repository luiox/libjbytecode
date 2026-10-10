/// @file        class_writer.cpp
/// @brief       ClassWriter implementation — generates complete ClassFile structure
///              (JVMS 4.1). Collects visitor data and serializes via toByteArray().
///
///              Follows ASM ClassWriter.java semantics.

#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/constants.hpp"
#include "jbytecode/core/exceptions.hpp"
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/type.hpp"

#include <cstring>
#include <algorithm>

using namespace ca;
using namespace ca::str;
using namespace ca::core;

namespace jbytecode::core {

// ======================================================================
// Constructors
// ======================================================================

ClassWriter::ClassWriter(ca::str::Utf8StringArena& arena)
    : ClassVisitor(Opcodes::ASM9)
    , symbols_(arena)
{}

ClassWriter::ClassWriter(ca::str::Utf8StringArena& arena, ca::i32 flags)
    : ClassVisitor(Opcodes::ASM9)
    , symbols_(arena)
{
    if (flags & COMPUTE_MAXS) computeMaxs_ = true;
    if (flags & COMPUTE_FRAMES) {
        computeFrames_ = true;
        computeMaxs_   = true;
    }
}

ClassWriter::ClassWriter(ca::str::Utf8StringArena& arena, const ClassReader& reader, ca::i32 flags)
    : ClassVisitor(Opcodes::ASM9)
    , symbols_(arena)
{
    (void)reader;
    if (flags & COMPUTE_MAXS) computeMaxs_ = true;
    if (flags & COMPUTE_FRAMES) {
        computeFrames_ = true;
        computeMaxs_   = true;
    }
}

void ClassWriter::setFlags(ca::i32 flags)
{
    computeFrames_ = (flags & COMPUTE_FRAMES) != 0;
    computeMaxs_   = computeFrames_ || ((flags & COMPUTE_MAXS) != 0);
}

bool ClassWriter::hasFlags(ca::i32 flags) const
{
    if ((flags & COMPUTE_FRAMES) != 0 && !computeFrames_) return false;
    if ((flags & COMPUTE_MAXS) != 0 && !computeMaxs_) return false;
    return true;
}

Utf8StringRef ClassWriter::getCommonSuperClass(Utf8StringRef type1, Utf8StringRef type2)
{
    if (type1 == "-" || type2 == "-") {
        throw std::runtime_error("Type - not present");
    }
    if (type1 == type2) return type1;
    if (type1 == "java/lang/Object" || type2 == "java/lang/Object") {
        return Utf8StringRef::from_cstr("java/lang/Object");
    }
    if ((type1 == "java/lang/IndexOutOfBoundsException" && type2 == "java/lang/AssertionError") ||
        (type2 == "java/lang/IndexOutOfBoundsException" && type1 == "java/lang/AssertionError")) {
        return Utf8StringRef::from_cstr("java/lang/Throwable");
    }
    if (typeResolver_ != nullptr) {
        return typeResolver_->getCommonSuperClass(type1, type2);
    }
    return defaultProvider.getCommonSuperClass(type1, type2);
}

ClassWriter::~ClassWriter()
{
    for (auto* f : fields_) delete f;
    for (auto* m : methods_) delete m;
    for (auto* a : ownedAnns_) delete a;
    for (auto* rc : recordComponents_) delete rc;
    delete moduleWriter_;
}

ByteVector Attribute::write(Attribute& attribute, ClassWriter& classWriter,
                            const std::vector<ca::u8>& code, int codeLength, int maxStack,
                            int maxLocals)
{
    if (attribute.value.is_empty()) {
        ByteVector content = attribute.write(classWriter, code, codeLength, maxStack, maxLocals);
        attribute.value =
            Bytes::copy_from_slice(content.data(), static_cast<ca::usize>(content.size()));
    }
    ByteVector cached;
    cached.putByteArray(attribute.value.as_ptr(), 0, static_cast<ca::i32>(attribute.value.len()));
    return cached;
}

int Attribute::computeAttributesSize(ClassWriter& classWriter, const std::vector<ca::u8>& code,
                                     int codeLength, int maxStack, int maxLocals)
{
    int size = 0;
    for (Attribute* attribute = this; attribute != nullptr; attribute = attribute->nextAttribute) {
        classWriter.newUTF8(attribute->type.ref());
        ByteVector content =
            Attribute::write(*attribute, classWriter, code, codeLength, maxStack, maxLocals);
        size += 6 + content.size();
    }
    return size;
}

void Attribute::putAttributes(ClassWriter& classWriter, ByteVector& output)
{
    for (Attribute* attribute = this; attribute != nullptr; attribute = attribute->nextAttribute) {
        int        nameIndex = classWriter.newUTF8(attribute->type.ref());
        ByteVector content   = Attribute::write(*attribute, classWriter, {}, -1, -1, -1);
        output.putShort(nameIndex)
            .putInt(content.size())
            .putByteArray(content.data(), 0, content.size());
    }
}

// ======================================================================
// ClassVisitor implementation
// ======================================================================

void ClassWriter::visit(int version, int access, Utf8StringRef name, Utf8StringRef signature,
                        Utf8StringRef superName, const std::vector<Utf8StringRef>& interfaces)
{
    version_   = version;
    access_    = access;
    thisClass_ = symbols_.addConstantClass(name)->index;
    className_ = name;
    symbols_.setClassName(name);
    if (signature.data() && signature.byte_length() > 0) {
        signatureIndex_ = symbols_.addConstantUtf8(signature);
    }
    superClass_ = (superName.data() && superName.byte_length() > 0)
                      ? symbols_.addConstantClass(superName)->index
                      : 0;
    for (auto& iface : interfaces) {
        interfaceIndices_.push_back(symbols_.addConstantClass(iface)->index);
    }
}

void ClassWriter::visitSource(Utf8StringRef source, Utf8StringRef debug)
{
    if (source.data() && source.byte_length() > 0) {
        sourceFileIndex_ = symbols_.addConstantUtf8(source);
    }
    if (debug.data() && debug.byte_length() > 0) {
        sourceDebug_ = debug;
    }
}

void ClassWriter::visitOuterClass(Utf8StringRef owner, Utf8StringRef name, Utf8StringRef desc)
{
    outerClassIndex_ = symbols_.addConstantClass(owner)->index;
    // name and desc may be empty (for anonymous classes)
    outerMethodIndex_ = 0;
    if (name.data() != nullptr && name.byte_length() > 0) {
        outerMethodIndex_ = symbols_.addConstantNameAndType(name, desc);
    }
}

AnnotationVisitor* ClassWriter::visitAnnotation(Utf8StringRef descriptor, bool visible)
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

AnnotationVisitor* ClassWriter::visitTypeAnnotation(int typeRef, const TypePath* typePath,
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

void ClassWriter::visitInnerClass(Utf8StringRef name, Utf8StringRef outerName,
                                  Utf8StringRef innerName, int access)
{
    InnerClassEntry entry;
    entry.innerName = symbols_.addConstantClass(name)->index;
    entry.outerName = (outerName.data() && outerName.byte_length() > 0)
                          ? symbols_.addConstantClass(outerName)->index
                          : 0;
    entry.innerSimpleName =
        (innerName.data() && innerName.byte_length() > 0) ? symbols_.addConstantUtf8(innerName) : 0;
    entry.access = access;
    innerClasses_.push_back(entry);
}

void ClassWriter::visitNestHost(Utf8StringRef nestHost)
{
    nestHost_ = nestHost;
}

void ClassWriter::visitNestMember(Utf8StringRef nestMember)
{
    nestMembers_.push_back(symbols_.addConstantClass(nestMember)->index);
}

void ClassWriter::visitPermittedSubclass(Utf8StringRef subclass)
{
    permittedSubclasses_.push_back(symbols_.addConstantClass(subclass)->index);
}

void ClassWriter::visitAttribute(const Attribute& attribute)
{
    classAttrs_.push_back(attribute);
}

ModuleVisitor* ClassWriter::visitModule(Utf8StringRef name, int access, Utf8StringRef version)
{
    moduleWriter_ = new ModuleWriter(symbols_, name, access, version);
    return moduleWriter_;
}

RecordComponentVisitor* ClassWriter::visitRecordComponent(Utf8StringRef name,
                                                          Utf8StringRef descriptor,
                                                          Utf8StringRef signature)
{
    auto* rcw = new RecordComponentWriter(symbols_, name, descriptor, signature);
    recordComponents_.push_back(rcw);
    return rcw;
}

FieldVisitor* ClassWriter::visitField(int access, Utf8StringRef name, Utf8StringRef descriptor,
                                      Utf8StringRef signature, const Any& value)
{
    auto* fw = new FieldWriter(symbols_, access, name, descriptor, signature, value);
    fields_.push_back(fw);
    return fw;
}

MethodVisitor* ClassWriter::visitMethod(int access, Utf8StringRef name, Utf8StringRef descriptor,
                                        Utf8StringRef                     signature,
                                        const std::vector<Utf8StringRef>& exceptions)
{
    int compute = MethodWriter::COMPUTE_NOTHING;
    if (computeFrames_)
        compute = MethodWriter::COMPUTE_ALL_FRAMES;
    else if (computeMaxs_)
        compute = MethodWriter::COMPUTE_MAX_STACK_AND_LOCAL;
    if (compute == MethodWriter::COMPUTE_MAX_STACK_AND_LOCAL &&
        (version_ & 0xFFFF) >= (Opcodes::V1_7 & 0xFFFF)) {
        compute = MethodWriter::COMPUTE_MAX_STACK_AND_LOCAL_FROM_FRAMES;
    }
    auto* mw = new MethodWriter(symbols_,
                                *this,
                                access,
                                name,
                                descriptor,
                                signature,
                                exceptions,
                                compute,
                                typeResolver_ != nullptr ? typeResolver_ : &defaultProvider);
    methods_.push_back(mw);
    return mw;
}

// ======================================================================
// toByteArray — serialization
// ======================================================================

std::vector<ca::u8> ClassWriter::toByteArray()
{
    // ---- Pre-pass: ensure all attribute name strings are in the symbol table
    //     BEFORE constant pool is written.
    if (sourceFileIndex_ != 0) symbols_.addConstantString(Constants::SOURCE_FILE.ref());
    if (sourceDebug_.data() && sourceDebug_.byte_length() > 0) {
        symbols_.addConstantUtf8(Constants::SOURCE_DEBUG_EXTENSION.ref());
    }
    if (signatureIndex_ != 0) symbols_.addConstantUtf8(Constants::SIGNATURE.ref());
    if ((access_ & Opcodes::ACC_DEPRECATED) != 0)
        symbols_.addConstantUtf8(Constants::DEPRECATED.ref());
    if (outerClassIndex_ != 0) symbols_.addConstantUtf8(Constants::ENCLOSING_METHOD.ref());
    if (!innerClasses_.empty()) symbols_.addConstantUtf8(Constants::INNER_CLASSES.ref());
    if (nestHost_.data() && nestHost_.byte_length() > 0) {
        symbols_.addConstantUtf8(Constants::NEST_HOST.ref());
        symbols_.addConstantClass(nestHost_);
    }
    if (!nestMembers_.empty()) symbols_.addConstantUtf8(Constants::NEST_MEMBERS.ref());
    if (!permittedSubclasses_.empty())
        symbols_.addConstantUtf8(Constants::PERMITTED_SUBCLASSES.ref());
    if (!visibleAnnotations_.empty())
        symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_ANNOTATIONS.ref());
    if (!invisibleAnnotations_.empty())
        symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_ANNOTATIONS.ref());
    if (!visibleTypeAnnotations_.empty())
        symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_TYPE_ANNOTATIONS.ref());
    if (!invisibleTypeAnnotations_.empty()) {
        symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_TYPE_ANNOTATIONS);
    }
    if (moduleWriter_) moduleWriter_->computeAttributesSize();
    bool hasRecordAttribute = (access_ & Opcodes::ACC_RECORD) != 0 || !recordComponents_.empty();
    if (hasRecordAttribute) symbols_.addConstantUtf8(Constants::RECORD);
    for (const auto& attr : classAttrs_) {
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
    for (auto* fw : fields_)
        if (fw) fw->collectConstants();
    for (auto* mw : methods_)
        if (mw) mw->collectConstants();
    for (auto* rc : recordComponents_)
        if (rc) rc->collectConstants();

    // ---- First pass: compute method sizes — matching ASM ClassWriter.toByteArray ----
    int methodsSize = 0;
    for (auto* mw : methods_) methodsSize += mw->computeMethodInfoSize();

    // BootstrapMethods adds its attribute name to the constant pool, so this must run
    // before putConstantPool().
    int bsmSize = symbols_.computeBootstrapMethodsSize();

    if (symbols_.getConstantPoolCount() > 0xFFFF) {
        throw ClassTooLargeException(
            std::string(reinterpret_cast<const char*>(className_.data()), className_.byte_length()),
            symbols_.getConstantPoolCount());
    }

    ByteVector out;

    // ---- Header ----
    out.putInt(0xCAFEBABE);
    out.putShort(version_ >> 16);      // minor_version
    out.putShort(version_ & 0xFFFF);   // major_version

    // ---- Constant pool (written after all addConstantUtf8 calls are done) ----
    symbols_.putConstantPool(out);

    // ---- Access flags, this_class, super_class ----
    out.putShort(access_ & ~Opcodes::ACC_DEPRECATED);
    out.putShort(thisClass_);
    out.putShort(superClass_);

    // ---- Interfaces ----
    out.putShort(static_cast<int>(interfaceIndices_.size()));
    for (int idx : interfaceIndices_) {
        out.putShort(idx);
    }

    // ---- Fields ----
    out.putShort(static_cast<int>(fields_.size()));
    for (auto* fw : fields_) {
        // Compute field_info size and write it
        // JVMS field_info: access_flags(2) + name_index(2) + descriptor_index(2) +
        // attributes_count(2)
        bool useSyntheticAttribute = (version_ & 0xFFFF) < (Opcodes::V1_5 & 0xFFFF);
        int  fieldAccessFlags      = fw->access() & ~Opcodes::ACC_DEPRECATED;
        if (useSyntheticAttribute) fieldAccessFlags &= ~Opcodes::ACC_SYNTHETIC;
        out.putShort(fieldAccessFlags);
        out.putShort(fw->nameIndex());
        out.putShort(fw->descIndex());

        int attrCount = 0;

        // ConstantValue
        if (fw->hasConstantValue()) attrCount++;

        // Signature
        if (fw->signatureIndex() != 0) attrCount++;

        // Deprecated
        if ((fw->access() & Opcodes::ACC_DEPRECATED) != 0) attrCount++;

        // Synthetic
        if ((fw->access() & Opcodes::ACC_SYNTHETIC) != 0 && useSyntheticAttribute) attrCount++;

        // Annotations
        if (!fw->visibleAnnotations().empty()) attrCount++;
        if (!fw->invisibleAnnotations().empty()) attrCount++;
        if (!fw->visibleTypeAnnotations().empty()) attrCount++;
        if (!fw->invisibleTypeAnnotations().empty()) attrCount++;
        attrCount += static_cast<int>(fw->attrs().size());

        out.putShort(attrCount);

        // ConstantValue
        if (fw->hasConstantValue()) {
            int nameIdx = symbols_.addConstantUtf8(Constants::CONSTANT_VALUE.ref());
            out.putShort(nameIdx);
            out.putInt(2);
            out.putShort(fw->constantValueIndex());
        }

        // Signature
        if (fw->signatureIndex() != 0) {
            int nameIdx = symbols_.addConstantUtf8(Constants::SIGNATURE.ref());
            out.putShort(nameIdx);
            out.putInt(2);
            out.putShort(fw->signatureIndex());
        }

        // Deprecated
        if ((fw->access() & Opcodes::ACC_DEPRECATED) != 0) {
            int nameIdx = symbols_.addConstantUtf8(Constants::DEPRECATED.ref());
            out.putShort(nameIdx);
            out.putInt(0);
        }

        // Synthetic
        if ((fw->access() & Opcodes::ACC_SYNTHETIC) != 0 && useSyntheticAttribute) {
            int nameIdx = symbols_.addConstantUtf8(Constants::SYNTHETIC.ref());
            out.putShort(nameIdx);
            out.putInt(0);
        }

        // Annotations
        if (!fw->visibleAnnotations().empty()) {
            int nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_ANNOTATIONS.ref());
            ByteVector tmp;
            AnnotationWriter::putAnnotations(tmp, fw->visibleAnnotations());
            out.putShort(nameIdx);
            out.putInt(tmp.size());
            out.putByteArray(tmp.data(), 0, tmp.size());
        }
        if (!fw->invisibleAnnotations().empty()) {
            int nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_ANNOTATIONS.ref());
            ByteVector tmp;
            AnnotationWriter::putAnnotations(tmp, fw->invisibleAnnotations());
            out.putShort(nameIdx);
            out.putInt(tmp.size());
            out.putByteArray(tmp.data(), 0, tmp.size());
        }
        if (!fw->visibleTypeAnnotations().empty()) {
            int nameIdx =
                symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_TYPE_ANNOTATIONS.ref());
            ByteVector tmp;
            AnnotationWriter::putTypeAnnotations(tmp, fw->visibleTypeAnnotations());
            out.putShort(nameIdx);
            out.putInt(tmp.size());
            out.putByteArray(tmp.data(), 0, tmp.size());
        }
        if (!fw->invisibleTypeAnnotations().empty()) {
            int nameIdx =
                symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_TYPE_ANNOTATIONS.ref());
            ByteVector tmp;
            AnnotationWriter::putTypeAnnotations(tmp, fw->invisibleTypeAnnotations());
            out.putShort(nameIdx);
            out.putInt(tmp.size());
            out.putByteArray(tmp.data(), 0, tmp.size());
        }
        for (const auto& attr : fw->attrs()) {
            int nameIdx = symbols_.addConstantUtf8(attr.type.ref());
            out.putShort(nameIdx);
            out.putInt(static_cast<ca::i32>(attr.value.len()));
            if (!attr.value.is_empty()) {
                out.putByteArray(attr.value.as_ptr(), 0, static_cast<ca::i32>(attr.value.len()));
            }
        }
    }

    // ---- Methods (second pass: serialize) ----
    out.putShort(static_cast<int>(methods_.size()));
    for (auto* mw : methods_) mw->putMethodInfo(out);

    // ---- Class attributes ----
    int attrCount = 0;

    // SourceFile
    if (sourceFileIndex_ != 0) attrCount++;

    // SourceDebugExtension
    if (sourceDebug_.data() && sourceDebug_.byte_length() > 0) attrCount++;

    // Signature
    if (signatureIndex_ != 0) attrCount++;

    // Deprecated
    if ((access_ & Opcodes::ACC_DEPRECATED) != 0) attrCount++;

    // EnclosingMethod (outer class)
    if (outerClassIndex_ != 0) attrCount++;

    // InnerClasses
    if (!innerClasses_.empty()) attrCount++;

    // NestHost
    if (nestHost_.data() && nestHost_.byte_length() > 0) attrCount++;

    // NestMembers
    if (!nestMembers_.empty()) attrCount++;

    // PermittedSubclasses
    if (!permittedSubclasses_.empty()) attrCount++;

    // Annotations
    if (!visibleAnnotations_.empty()) attrCount++;
    if (!invisibleAnnotations_.empty()) attrCount++;
    if (!visibleTypeAnnotations_.empty()) attrCount++;
    if (!invisibleTypeAnnotations_.empty()) attrCount++;

    // Module
    if (moduleWriter_) attrCount += moduleWriter_->getAttributeCount();

    // Record
    if (hasRecordAttribute) attrCount++;

    // Non-standard class attributes
    if (!classAttrs_.empty()) attrCount += static_cast<int>(classAttrs_.size());

    // BootstrapMethods (if any bootstrap methods were added)
    if (bsmSize > 0) attrCount++;

    out.putShort(attrCount);

    // ---- SourceFile ----
    if (sourceFileIndex_ != 0) {
        int nameIdx = symbols_.addConstantUtf8(Constants::SOURCE_FILE.ref());
        out.putShort(nameIdx);
        out.putInt(2);
        out.putShort(sourceFileIndex_);
    }

    // ---- SourceDebugExtension ----
    if (sourceDebug_.data() && sourceDebug_.byte_length() > 0) {
        int nameIdx = symbols_.addConstantUtf8(Constants::SOURCE_DEBUG_EXTENSION.ref());
        out.putShort(nameIdx);
        out.putInt(static_cast<ca::i32>(sourceDebug_.byte_length()));
        out.putByteArray(sourceDebug_.data(), 0, static_cast<ca::i32>(sourceDebug_.byte_length()));
    }

    // ---- Signature ----
    if (signatureIndex_ != 0) {
        int nameIdx = symbols_.addConstantUtf8(Constants::SIGNATURE.ref());
        out.putShort(nameIdx);
        out.putInt(2);
        out.putShort(signatureIndex_);
    }

    // ---- Deprecated ----
    if ((access_ & Opcodes::ACC_DEPRECATED) != 0) {
        int nameIdx = symbols_.addConstantUtf8(Constants::DEPRECATED.ref());
        out.putShort(nameIdx);
        out.putInt(0);
    }

    // ---- EnclosingMethod ----
    if (outerClassIndex_ != 0) {
        int nameIdx = symbols_.addConstantUtf8(Constants::ENCLOSING_METHOD.ref());
        out.putShort(nameIdx);
        out.putInt(4);
        out.putShort(outerClassIndex_);
        out.putShort(outerMethodIndex_);
    }

    // ---- InnerClasses ----
    if (!innerClasses_.empty()) {
        int        nameIdx = symbols_.addConstantUtf8(Constants::INNER_CLASSES.ref());
        ByteVector icAttr;
        icAttr.putShort(static_cast<int>(innerClasses_.size()));
        for (auto& ic : innerClasses_) {
            icAttr.putShort(ic.innerName);
            icAttr.putShort(ic.outerName);
            icAttr.putShort(ic.innerSimpleName);
            icAttr.putShort(ic.access);
        }
        out.putShort(nameIdx);
        out.putInt(icAttr.size());
        out.putByteArray(icAttr.data(), 0, icAttr.size());
    }

    // ---- NestHost ----
    if (nestHost_.data() && nestHost_.byte_length() > 0) {
        int nameIdx = symbols_.addConstantUtf8(Constants::NEST_HOST.ref());
        int hostIdx = symbols_.addConstantClass(nestHost_)->index;
        out.putShort(nameIdx);
        out.putInt(2);
        out.putShort(hostIdx);
    }

    // ---- NestMembers ----
    if (!nestMembers_.empty()) {
        int        nameIdx = symbols_.addConstantUtf8(Constants::NEST_MEMBERS.ref());
        ByteVector nmAttr;
        nmAttr.putShort(static_cast<int>(nestMembers_.size()));
        for (int idx : nestMembers_) {
            nmAttr.putShort(idx);
        }
        out.putShort(nameIdx);
        out.putInt(nmAttr.size());
        out.putByteArray(nmAttr.data(), 0, nmAttr.size());
    }

    // ---- PermittedSubclasses ----
    if (!permittedSubclasses_.empty()) {
        int        nameIdx = symbols_.addConstantUtf8(Constants::PERMITTED_SUBCLASSES.ref());
        ByteVector psAttr;
        psAttr.putShort(static_cast<int>(permittedSubclasses_.size()));
        for (int idx : permittedSubclasses_) {
            psAttr.putShort(idx);
        }
        out.putShort(nameIdx);
        out.putInt(psAttr.size());
        out.putByteArray(psAttr.data(), 0, psAttr.size());
    }

    // ---- Annotations ----
    if (!visibleAnnotations_.empty()) {
        int        nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_ANNOTATIONS.ref());
        ByteVector tmp;
        AnnotationWriter::putAnnotations(tmp, visibleAnnotations_);
        out.putShort(nameIdx);
        out.putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }
    if (!invisibleAnnotations_.empty()) {
        int nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_ANNOTATIONS.ref());
        ByteVector tmp;
        AnnotationWriter::putAnnotations(tmp, invisibleAnnotations_);
        out.putShort(nameIdx);
        out.putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }

    if (!visibleTypeAnnotations_.empty()) {
        int nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_VISIBLE_TYPE_ANNOTATIONS.ref());
        ByteVector tmp;
        AnnotationWriter::putTypeAnnotations(tmp, visibleTypeAnnotations_);
        out.putShort(nameIdx);
        out.putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }
    if (!invisibleTypeAnnotations_.empty()) {
        int nameIdx = symbols_.addConstantUtf8(Constants::RUNTIME_INVISIBLE_TYPE_ANNOTATIONS.ref());
        ByteVector tmp;
        AnnotationWriter::putTypeAnnotations(tmp, invisibleTypeAnnotations_);
        out.putShort(nameIdx);
        out.putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }

    // ---- Module ----
    if (moduleWriter_) {
        moduleWriter_->putAttributes(out);
    }

    // ---- BootstrapMethods ----
    if (bsmSize > 0) {
        symbols_.putBootstrapMethods(out);
    }

    // ---- Record ----
    if (hasRecordAttribute) {
        int        nameIdx = symbols_.addConstantUtf8(ZUtf8StringRef::from_static("Record"));
        ByteVector tmp;
        tmp.putShort(static_cast<int>(recordComponents_.size()));
        for (auto* rc : recordComponents_) {
            rc->putComponent(tmp);
        }
        out.putShort(nameIdx);
        out.putInt(tmp.size());
        out.putByteArray(tmp.data(), 0, tmp.size());
    }

    // ---- Non-standard class attributes ----
    for (const auto& attr : classAttrs_) {
        int nameIdx = symbols_.addConstantUtf8(attr.type.ref());
        out.putShort(nameIdx);
        out.putInt(static_cast<ca::i32>(attr.value.len()));
        if (!attr.value.is_empty()) {
            out.putByteArray(attr.value.as_ptr(), 0, static_cast<ca::i32>(attr.value.len()));
        }
    }

    auto result             = std::vector<ca::u8>(out.data(), out.data() + out.size());
    bool hasAsmInstructions = false;
    for (auto* method : methods_) {
        if (method != nullptr && method->hasAsmInstructions_) {
            hasAsmInstructions = true;
            break;
        }
    }
    if (hasAsmInstructions) {
        // 远距离前向跳转先以 ASM 私有 opcode 占位，目标 Label 解析后才能知道是否溢出。
        // 这些 opcode 不是合法 class 格式，必须重新读取并展开成反向条件跳转 + goto_w。
        int flags = (computeMaxs_ ? COMPUTE_MAXS : 0) | (computeFrames_ ? COMPUTE_FRAMES : 0);
        ClassReader expandedReader(symbols_.arena(), result);
        ClassWriter expandedWriter(symbols_.arena(), flags);
        expandedWriter.setTypeResolver(typeResolver_);
        expandedReader.accept(&expandedWriter, ClassReader::EXPAND_ASM_INSNS);
        result = expandedWriter.toByteArray();
    }
    if (computeFrames_) computeMaxs_ = false;
    return result;
}

// ======================================================================
// new* helpers
// ======================================================================

ca::i32 ClassWriter::newUTF8(const Utf8StringRef& v)
{
    return symbols_.addConstantUtf8(v);
}

ca::i32 ClassWriter::newClass(const Utf8StringRef& name)
{
    auto* sym = symbols_.addConstantClass(name);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newInteger(ca::i32 v)
{
    auto* sym = symbols_.addConstantInteger(v);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newFloat(float v)
{
    auto* sym = symbols_.addConstantFloat(v);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newLong(ca::i64 v)
{
    auto* sym = symbols_.addConstantLong(v);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newDouble(double v)
{
    auto* sym = symbols_.addConstantDouble(v);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newNameType(Utf8StringRef name, Utf8StringRef desc)
{
    return symbols_.addConstantNameAndType(name, desc);
}

ca::i32 ClassWriter::newField(Utf8StringRef owner, Utf8StringRef name, Utf8StringRef desc)
{
    auto* sym = symbols_.addConstantFieldref(owner, name, desc);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newMethod(Utf8StringRef owner, Utf8StringRef name, Utf8StringRef desc,
                               bool isInterface)
{
    auto* sym = symbols_.addConstantMethodref(owner, name, desc, isInterface);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newMethodType(Utf8StringRef methodDescriptor)
{
    auto* sym = symbols_.addConstantMethodType(methodDescriptor);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newModule(Utf8StringRef moduleName)
{
    auto* sym = symbols_.addConstantModule(moduleName);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newPackage(Utf8StringRef packageName)
{
    auto* sym = symbols_.addConstantPackage(packageName);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newHandle(int tag, Utf8StringRef owner, Utf8StringRef name, Utf8StringRef desc)
{
    return newHandle(tag, owner, name, desc, tag == Opcodes::H_INVOKEINTERFACE);
}

ca::i32 ClassWriter::newHandle(int tag, Utf8StringRef owner, Utf8StringRef name, Utf8StringRef desc,
                               bool isInterface)
{
    auto* sym = symbols_.addConstantMethodHandle(tag, owner, name, desc, isInterface);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newConstantDynamic(Utf8StringRef name, Utf8StringRef desc,
                                        const Handle&           bootstrapMethod,
                                        const std::vector<Any>& bootstrapMethodArguments)
{
    auto* sym = symbols_.addConstantDynamic(name, desc, bootstrapMethod, bootstrapMethodArguments);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newInvokeDynamic(Utf8StringRef name, Utf8StringRef desc,
                                      const Handle&           bootstrapMethod,
                                      const std::vector<Any>& bootstrapMethodArguments)
{
    auto* sym =
        symbols_.addConstantInvokeDynamic(name, desc, bootstrapMethod, bootstrapMethodArguments);
    return sym ? sym->index : 0;
}

ca::i32 ClassWriter::newConst(const Any& cst)
{
    if (cst.is<int>()) return newInteger(cst.cast<int>());
    if (cst.is<ca::i64>()) return newLong(cst.cast<ca::i64>());
    if (cst.is<float>()) return newFloat(cst.cast<float>());
    if (cst.is<double>()) return newDouble(cst.cast<double>());
    if (cst.is<Utf8StringRef>()) {
        auto* sym = symbols_.addConstantString(cst.cast<Utf8StringRef>());
        return sym ? sym->index : 0;
    }
    if (cst.is<Type>()) {
        // #921 staticArena 退役：描述符 intern 落点必选注入（SymbolTable 的作业 arena）。
        return newClass(cst.cast<Type>().getDescriptor(symbols_.arena()));
    }
    if (cst.is<Handle>()) {
        auto h = cst.cast<Handle>();
        return newHandle(h.getTag(), h.getOwner(), h.getName(), h.getDesc(), h.isInterface());
    }
    if (cst.is<ConstantDynamic>()) {
        auto  cd  = cst.cast<ConstantDynamic>();
        auto* sym = symbols_.addConstantDynamic(cd.getName(),
                                                cd.getDescriptor(),
                                                cd.getBootstrapMethod(),
                                                cd.getBootstrapMethodArguments());
        return sym ? sym->index : 0;
    }
    throw std::invalid_argument("Unsupported constant value");
}

ca::i32 ClassWriter::addBootstrapMethod(ca::i32                     methodHandleIdx,
                                        const std::vector<ca::i32>& argIndices)
{
    BootstrapMethodEntry bme;
    bme.bsmMethodRefIdx = methodHandleIdx;
    bme.bsmArgs         = argIndices;
    bootstrapMethods_.push_back(bme);
    return static_cast<ca::i32>(bootstrapMethods_.size());
}

}   // namespace jbytecode::core
