#include "jbytecode/core/readers.hpp"

#include "jbytecode/core/constants.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/frame.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/core/type_reference.hpp"
#include "jstr/conversion.hpp"
#include "libca/str/os_string.hpp"
#include <cstring>
#include <stdexcept>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace ca;
using namespace ca::core;
using namespace ca::str;

namespace jbytecode::core {

namespace {

std::vector<u8> readAll(std::istream& inputStream)
{
    if (!inputStream) throw std::invalid_argument("Input stream is not readable");
    std::vector<u8> bytes{std::istreambuf_iterator<char>(inputStream),
                          std::istreambuf_iterator<char>()};
    if (bytes.empty()) throw std::invalid_argument("Empty input stream");
    return bytes;
}

std::vector<u8> readFileBytes(const std::string& classFilePath)
{
    // Windows 窄字符文件 API 按 ANSI 代码页解释路径，而本库 std::string 约定为 UTF-8：
    // 非 ASCII（中文/emoji）路径直接喂窄 ifstream 会打不开或落到乱码文件名
    // （同 libca PR #250 zip 模块的修法）。Windows 先转 UTF-16 走 path 宽构造，
    // POSIX 路径即字节序列，原样直开。
#if defined(_WIN32)
    const std::filesystem::path nativePath(
        OsString::from_utf8_lossy(classFilePath).into_wstring());
    std::ifstream input(nativePath, std::ios::binary);
    if (!input) {
        // lossy 判定：转回 UTF-8 与原串不等 ⇔ 存在被替换的非法字节，路径已改写
        const bool lossy =
            OsString::from_utf8_lossy(classFilePath).into_utf8_string().to_std_string() != classFilePath;
        std::string message = "Class file not found: " + classFilePath;
        if (lossy) message += " (path converted lossily: std::string path convention is UTF-8)";
        throw std::invalid_argument(message);
    }
    return readAll(input);
#else
    std::ifstream input(classFilePath, std::ios::binary);
    if (!input) throw std::invalid_argument("Class file not found: " + classFilePath);
    return readAll(input);
#endif
}

Utf8StringRef normalizeFieldDescriptor(Utf8StringArena&      arena,
                                       const Utf8StringRef& descriptor)
{
    if (descriptor.is_empty() || descriptor.byte_at(0) == '[' || descriptor.byte_at(0) == 'L' ||
        descriptor.byte_length() == 1) {
        return descriptor;
    }

    std::string normalized;
    normalized.reserve(descriptor.byte_length() + 2);
    normalized.push_back('L');
    normalized.append(reinterpret_cast<const char*>(descriptor.data()), descriptor.byte_length());
    normalized.push_back(';');
    return arena.intern(reinterpret_cast<const u8*>(normalized.data()), normalized.size());
}

}   // namespace

static Utf8StringRef internMutf8(Utf8StringArena& arena, const u8* d, i32 len)
{
    auto r = jstr::mutf8_to_utf8(jstr::MUtf8StringRef((const char*)d, len));
    if (r.is_ok()) return arena.intern((const u8*)r.unwrap().data(), r.unwrap().size());

    auto raw = jstr::mutf8_to_wtf8(jstr::MUtf8StringRef((const char*)d, len));
    return raw.is_err() ? Utf8StringRef()
                        : arena.intern_raw((const u8*)raw.unwrap().data(), raw.unwrap().size());
}

void ClassReader::init(const u8* buf, i32 off, i32 len)
{
    if (!buf || off < 0 || len < 10) throw std::invalid_argument("Invalid class file");
    classFileStartOffset_ = off;
    classFileLength_      = len;
    if (readInt(off) != 0xCAFEBABE) throw std::invalid_argument("Invalid class header");
    if (readShort(off + 6) > Opcodes::V27) throw std::invalid_argument("Unsupported class version");
    constantPoolCount_ = readUnsignedShort(off + 8);
    cpInfoOffsets_.assign(static_cast<usize>(constantPoolCount_), 0);
    constantUtf8Values_.assign(static_cast<usize>(constantPoolCount_), Utf8StringRef{});
    bool hb = false, hcd = false;
    int  idx = 1, o = off + 10, ms = 0;
    while (idx < constantPoolCount_) {
        checkRange(o, 1);
        cpInfoOffsets_[idx++] = o + 1;
        int tag = buf[o], sz = 0;
        switch (tag) {
        case 3:
        case 4:
        case 9:
        case 10:
        case 11:
        case 12: sz = 5; break;
        case 17:
            sz  = 5;
            hb  = true;
            hcd = true;
            break;
        case 18:
            sz = 5;
            hb = true;
            break;
        case 5:
        case 6:
            sz = 9;
            // Long and Double consume two constant-pool indices. The zero-initialized
            // second entry is the reserved slot required by JVMS 4.4.5.
            idx++;
            break;
        case 1:
            sz = 3 + readUnsignedShort(o + 1);
            if (sz > ms) ms = sz;
            break;
        case 15: sz = 4; break;
        case 7:
        case 8:
        case 16:
        case 19:
        case 20: sz = 3; break;
        default: throw std::invalid_argument("Unknown CP tag");
        }
        checkRange(o, sz);
        o += sz;
    }
    checkRange(o, 8);
    maxStringLength_       = ms;
    header_                = o;
    if (hcd) {
        constantDynamicValues_.assign(static_cast<usize>(constantPoolCount_), Any{});
    }
    if (hb) {
        readBootstrapMethodsAttribute(ms);
    }
    else {
        bootstrapMethodOffsets_.clear();
    }

    header          = header_;
    maxStringLength = maxStringLength_;
    b               = ByteArray{const_cast<u8*>(classFileBuffer_), static_cast<usize>(len)};
    classFileBuffer = b;
    cpInfoOffsets   = cpInfoOffsets_;
    constantUtf8Values = constantUtf8Values_;
    if (!bootstrapMethodOffsets_.empty()) {
        i32 count = readUnsignedShort(bootstrapMethodOffsets_[0] - 2);
        bootstrapMethodOffsets.assign(bootstrapMethodOffsets_.begin(),
                                      bootstrapMethodOffsets_.begin() + count);
    }
}

ClassReader::ClassReader(ca::str::Utf8StringArena& arena, const std::vector<u8>& v)
    : ownedData_(v)
    , classFileBuffer_(ownedData_.data())
    , arena_(arena)
{
    init(ownedData_.data(), 0, (i32)ownedData_.size());
}

ClassReader::ClassReader(ca::str::Utf8StringArena& arena, std::istream& inputStream)
    : ClassReader(arena, readAll(inputStream))
{}

ClassReader::ClassReader(ca::str::Utf8StringArena& arena, const std::string& classFilePath)
    : ClassReader(arena, readFileBytes(classFilePath))
{}

ClassReader::ClassReader(ca::str::Utf8StringArena& arena, const u8* buf, i32 off, i32 len)
    : classFileBuffer_(buf)
    , arena_(arena)
{
    init(buf, off, len);
}

ClassReader::~ClassReader() = default;
void ClassReader::checkRange(i32 offset, i32 size) const
{
    // 读取辅助函数在这里统一验证切片范围。使用 64 位中间值避免 offset + size 溢出，
    // 使这些路径上的畸形偏移在解引用前转换为标准异常。
    if (offset < classFileStartOffset_ || size < 0) {
        throw std::invalid_argument("ClassReader offset out of bounds");
    }
    const i64 begin = offset;
    const i64 end   = begin + size;
    const i64 limit = static_cast<i64>(classFileStartOffset_) + classFileLength_;
    if (end < begin || end > limit) {
        throw std::invalid_argument("ClassReader offset out of bounds");
    }
}
bool ClassReader::isValidCpIndex(i32 index) const
{
    return index > 0 && index < constantPoolCount_;
}
i32 ClassReader::cpInfoOffset(i32 index) const
{
    if (!isValidCpIndex(index)) throw std::invalid_argument("Invalid constant pool index");
    i32 offset = cpInfoOffsets_[index];
    if (offset <= 0) throw std::invalid_argument("Invalid constant pool offset");
    checkRange(offset - 1, 1);
    return offset;
}
i32 ClassReader::bootstrapMethodOffset(i32 index) const
{
    if (index < 0 || index >= static_cast<i32>(bootstrapMethodOffsets_.size()))
        throw std::invalid_argument("Invalid bootstrap method index");
    return bootstrapMethodOffsets_[index];
}
i32 ClassReader::readByte(i32 o) const
{
    checkRange(o, 1);
    return classFileBuffer_[o] & 0xFF;
}
i32 ClassReader::readUnsignedShort(i32 o) const
{
    checkRange(o, 2);
    return ((i32)classFileBuffer_[o] << 8) | classFileBuffer_[o + 1];
}
i32 ClassReader::readShort(i32 o) const
{
    checkRange(o, 2);
    return (i16)((classFileBuffer_[o] << 8) | classFileBuffer_[o + 1]);
}
i32 ClassReader::readInt(i32 o) const
{
    checkRange(o, 4);
    return ((i32)classFileBuffer_[o] << 24) | ((i32)classFileBuffer_[o + 1] << 16) |
           ((i32)classFileBuffer_[o + 2] << 8) | classFileBuffer_[o + 3];
}
i64 ClassReader::readLong(i32 o) const
{
    checkRange(o, 8);
    return ((i64)readInt(o) << 32) | (i64)(unsigned)readInt(o + 4);
}
Utf8StringRef ClassReader::readUTF8(i32 o, std::vector<char>&)
{
    int cpIdx = readUnsignedShort(o);
    if (o == 0 || cpIdx == 0) return {};
    if (!isValidCpIndex(cpIdx)) throw std::invalid_argument("Invalid constant pool index");
    static std::vector<char> dummy;
    return readUtf(cpIdx, dummy);
}
Utf8StringRef ClassReader::readClass(i32 o, std::vector<char>& b)
{
    // Corresponds to Java readClass → readStringish:
    // readUTF8(cpInfoOffsets[readUnsignedShort(offset)], charBuffer)
    int cpIdx = readUnsignedShort(o);
    if (cpIdx == 0) return {};
    int cpDataOff = cpInfoOffset(cpIdx);   // for Class entry, points to name_index
    return readUTF8(cpDataOff, b);
}
Utf8StringRef ClassReader::readModule(i32 o, std::vector<char>& b)
{
    // Same as readClass: goes through readStringish pattern
    int cpIdx = readUnsignedShort(o);
    if (cpIdx == 0) return {};
    int cpDataOff = cpInfoOffset(cpIdx);   // points to name_index
    return readUTF8(cpDataOff, b);
}
Utf8StringRef ClassReader::readUtf(i32 idx, std::vector<char>&)
{
    if (idx == 0) return {};
    if (!isValidCpIndex(idx)) throw std::invalid_argument("Invalid constant pool index");
    if (constantUtf8Values_[idx].data()) return constantUtf8Values_[idx];
    int cp = cpInfoOffset(idx);
    int l  = readUnsignedShort(cp);
    checkRange(cp + 2, l);
    auto ref                 = internMutf8(arena_, classFileBuffer_ + cp + 2, l);
    constantUtf8Values_[idx] = ref;
    return ref;
}
Utf8StringRef ClassReader::readUtf(i32 offset, i32 utfLength, std::vector<char>& charBuffer)
{
    // Read a raw MUTF-8 string from classFileBuffer at given offset with given byte length.
    // This is used by SourceDebugExtension and similar attributes.
    if (utfLength <= 0) return {};
    checkRange(offset, utfLength);
    // Use charBuffer to store the converted UTF-8 data
    charBuffer.resize(utfLength * 2 + 1);   // generous estimate for MUTF-8 → UTF-8 expansion
    auto r = jstr::mutf8_to_utf8(
        jstr::MUtf8StringRef((const char*)(classFileBuffer_ + offset), utfLength));
    if (r.is_err()) return {};
    const auto utf8Str = r.unwrap();
    return arena_.intern((const u8*)utf8Str.data(), (i32)utf8Str.size());
}
Utf8StringRef ClassReader::getClassName()
{
    std::vector<char> b(maxStringLength_);
    return readClass(header_ + 2, b);
}
Utf8StringRef ClassReader::getSuperName()
{
    std::vector<char> b(maxStringLength_);
    return readClass(header_ + 4, b);
}
std::vector<Utf8StringRef> ClassReader::getInterfaces()
{
    i32 currentOffset   = header_ + 6;
    int interfacesCount = readUnsignedShort(currentOffset);

    std::vector<Utf8StringRef> interfaces(interfacesCount);

    if (interfacesCount > 0) {
        std::vector<char> charBuffer(maxStringLength_);
        for (int i = 0; i < interfacesCount; ++i) {
            currentOffset += 2;
            interfaces[i] = readClass(currentOffset, charBuffer);
        }
    }

    return interfaces;
}
void ClassReader::accept(ClassVisitor* classVisitor, i32 parsingOptions)
{
    accept(classVisitor, nullptr, parsingOptions);
}

void ClassReader::accept(ClassVisitor* classVisitor, Attribute* attributePrototypes,
                         i32 parsingOptions)
{
    Context ctx;
    ctx.attributePrototypes = attributePrototypes;
    ctx.parsingOptions      = parsingOptions;
    ctx.charBuffer.resize(maxStringLength_);
    auto& buf = ctx.charBuffer;
    int   off = header_;
    int   acc = readUnsignedShort(off);
    auto  cls = readClass(off + 2, buf);
    auto  sup = readClass(off + 4, buf);
    int   ic  = readUnsignedShort(off + 6);
    off += 8;
    std::vector<Utf8StringRef> ifs;
    for (int i = 0; i < ic; i++) {
        ifs.push_back(readClass(off, buf));
        off += 2;
    }

    // ---- Parse class-level attributes (order matches JVMS 4.7) ----
    int           innerClassesOffset    = 0;
    int           enclosingMethodOffset = 0;
    Utf8StringRef signature;
    Utf8StringRef sourceFile;
    Utf8StringRef sourceDebugExtension;
    int           runtimeVisibleAnnotationsOffset       = 0;
    int           runtimeInvisibleAnnotationsOffset     = 0;
    int           runtimeVisibleTypeAnnotationsOffset   = 0;
    int           runtimeInvisibleTypeAnnotationsOffset = 0;
    int           moduleOffset                          = 0;
    int           modulePackagesOffset                  = 0;
    Utf8StringRef moduleMainClass;
    Utf8StringRef nestHostClass;
    int           nestMembersOffset         = 0;
    int           permittedSubclassesOffset = 0;
    int           recordOffset              = 0;
    Attribute*    attributes                = nullptr;

    int currentAttributeOffset = getFirstAttributeOffset();
    int numClassAttrs          = readUnsignedShort(currentAttributeOffset);
    currentAttributeOffset += 2;
    for (int i = numClassAttrs; i > 0; --i) {
        auto attributeName   = readUTF8(currentAttributeOffset, buf);
        int  attributeLength = readInt(currentAttributeOffset + 2);
        currentAttributeOffset += 6;
        if (attributeName.equals(ZUtf8StringRef::from_static("SourceFile"))) {
            sourceFile = readUTF8(currentAttributeOffset, buf);
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("InnerClasses"))) {
            innerClassesOffset = currentAttributeOffset;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("EnclosingMethod"))) {
            enclosingMethodOffset = currentAttributeOffset;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("NestHost"))) {
            nestHostClass = readClass(currentAttributeOffset, buf);
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("NestMembers"))) {
            nestMembersOffset = currentAttributeOffset;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("PermittedSubclasses"))) {
            permittedSubclassesOffset = currentAttributeOffset;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("Signature"))) {
            signature = readUTF8(currentAttributeOffset, buf);
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("RuntimeVisibleAnnotations"))) {
            runtimeVisibleAnnotationsOffset = currentAttributeOffset;
        }
        else if (attributeName.equals(
                     ZUtf8StringRef::from_static("RuntimeVisibleTypeAnnotations"))) {
            runtimeVisibleTypeAnnotationsOffset = currentAttributeOffset;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("Deprecated"))) {
            acc |= Opcodes::ACC_DEPRECATED;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("Synthetic"))) {
            acc |= Opcodes::ACC_SYNTHETIC;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("SourceDebugExtension"))) {
            sourceDebugExtension = readUtf(currentAttributeOffset, attributeLength, buf);
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("RuntimeInvisibleAnnotations"))) {
            runtimeInvisibleAnnotationsOffset = currentAttributeOffset;
        }
        else if (attributeName.equals(
                     ZUtf8StringRef::from_static("RuntimeInvisibleTypeAnnotations"))) {
            runtimeInvisibleTypeAnnotationsOffset = currentAttributeOffset;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("Record"))) {
            recordOffset = currentAttributeOffset;
            acc |= Opcodes::ACC_RECORD;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("Module"))) {
            moduleOffset = currentAttributeOffset;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("ModuleMainClass"))) {
            moduleMainClass = readClass(currentAttributeOffset, buf);
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("ModulePackages"))) {
            modulePackagesOffset = currentAttributeOffset;
        }
        else if (!attributeName.equals(ZUtf8StringRef::from_static("BootstrapMethods"))) {
            // BootstrapMethods is read in the constructor.
            auto* attr = readAttribute(ctx.attributePrototypes,
                                       attributeName,
                                       currentAttributeOffset,
                                       attributeLength,
                                       buf,
                                       -1,
                                       nullptr);
            if (attr) {
                attr->nextAttribute = attributes;
                attributes          = attr;
            }
        }
        currentAttributeOffset += attributeLength;
    }

    // Visit the class declaration
    classVisitor->visit(readInt(cpInfoOffset(1) - 7), acc, cls, signature, sup, ifs);

    // Visit SourceFile and SourceDebugExtension
    if ((parsingOptions & SKIP_DEBUG) == 0 && (sourceFile.data() || sourceDebugExtension.data())) {
        classVisitor->visitSource(
            sourceFile.data() ? sourceFile : Utf8StringRef{},
            sourceDebugExtension.data() ? sourceDebugExtension : Utf8StringRef{});
    }

    // Visit Module
    if (moduleOffset != 0) {
        readModuleAttributes(
            classVisitor, ctx, moduleOffset, modulePackagesOffset, moduleMainClass);
    }

    // Visit NestHost
    if (nestHostClass.data()) {
        classVisitor->visitNestHost(nestHostClass);
    }

    // Visit EnclosingMethod
    if (enclosingMethodOffset != 0) {
        auto          enclosingClass = readClass(enclosingMethodOffset, buf);
        int           methodIndex    = readUnsignedShort(enclosingMethodOffset + 2);
        Utf8StringRef enclosingName;
        Utf8StringRef enclosingDesc;
        if (methodIndex != 0) {
            int methodOffset = cpInfoOffset(methodIndex);
            enclosingName    = readUTF8(methodOffset, buf);
            enclosingDesc    = readUTF8(methodOffset + 2, buf);
        }
        classVisitor->visitOuterClass(enclosingClass, enclosingName, enclosingDesc);
    }

    // Visit RuntimeVisibleAnnotations
    if (runtimeVisibleAnnotationsOffset != 0) {
        int numAnnotations          = readUnsignedShort(runtimeVisibleAnnotationsOffset);
        int currentAnnotationOffset = runtimeVisibleAnnotationsOffset + 2;
        while (numAnnotations-- > 0) {
            auto annotationDescriptor = readUTF8(currentAnnotationOffset, buf);
            currentAnnotationOffset += 2;
            currentAnnotationOffset =
                readElementValues(classVisitor->visitAnnotation(annotationDescriptor, true),
                                  currentAnnotationOffset,
                                  true,
                                  buf);
        }
    }

    // Visit RuntimeInvisibleAnnotations
    if (runtimeInvisibleAnnotationsOffset != 0) {
        int numAnnotations          = readUnsignedShort(runtimeInvisibleAnnotationsOffset);
        int currentAnnotationOffset = runtimeInvisibleAnnotationsOffset + 2;
        while (numAnnotations-- > 0) {
            auto annotationDescriptor = readUTF8(currentAnnotationOffset, buf);
            currentAnnotationOffset += 2;
            currentAnnotationOffset =
                readElementValues(classVisitor->visitAnnotation(annotationDescriptor, false),
                                  currentAnnotationOffset,
                                  true,
                                  buf);
        }
    }

    // Visit RuntimeVisibleTypeAnnotations
    if (runtimeVisibleTypeAnnotationsOffset != 0) {
        int numAnnotations          = readUnsignedShort(runtimeVisibleTypeAnnotationsOffset);
        int currentAnnotationOffset = runtimeVisibleTypeAnnotationsOffset + 2;
        while (numAnnotations-- > 0) {
            currentAnnotationOffset   = readTypeAnnotationTarget(ctx, currentAnnotationOffset);
            auto annotationDescriptor = readUTF8(currentAnnotationOffset, buf);
            currentAnnotationOffset += 2;
            currentAnnotationOffset = readElementValues(
                classVisitor->visitTypeAnnotation(ctx.currentTypeAnnotationTarget,
                                                  &ctx.currentTypeAnnotationTargetPath,
                                                  annotationDescriptor,
                                                  true),
                currentAnnotationOffset,
                true,
                buf);
        }
    }

    // Visit RuntimeInvisibleTypeAnnotations
    if (runtimeInvisibleTypeAnnotationsOffset != 0) {
        int numAnnotations          = readUnsignedShort(runtimeInvisibleTypeAnnotationsOffset);
        int currentAnnotationOffset = runtimeInvisibleTypeAnnotationsOffset + 2;
        while (numAnnotations-- > 0) {
            currentAnnotationOffset   = readTypeAnnotationTarget(ctx, currentAnnotationOffset);
            auto annotationDescriptor = readUTF8(currentAnnotationOffset, buf);
            currentAnnotationOffset += 2;
            currentAnnotationOffset = readElementValues(
                classVisitor->visitTypeAnnotation(ctx.currentTypeAnnotationTarget,
                                                  &ctx.currentTypeAnnotationTargetPath,
                                                  annotationDescriptor,
                                                  false),
                currentAnnotationOffset,
                true,
                buf);
        }
    }

    // Visit non-standard attributes
    while (attributes) {
        auto* next                = attributes->nextAttribute;
        attributes->nextAttribute = nullptr;
        classVisitor->visitAttribute(*attributes);
        attributes = next;
    }

    // Visit NestMembers
    if (nestMembersOffset != 0) {
        int numberOfNestMembers     = readUnsignedShort(nestMembersOffset);
        int currentNestMemberOffset = nestMembersOffset + 2;
        while (numberOfNestMembers-- > 0) {
            classVisitor->visitNestMember(readClass(currentNestMemberOffset, buf));
            currentNestMemberOffset += 2;
        }
    }

    // Visit PermittedSubclasses
    if (permittedSubclassesOffset != 0) {
        int numberOfPermittedSubclasses      = readUnsignedShort(permittedSubclassesOffset);
        int currentPermittedSubclassesOffset = permittedSubclassesOffset + 2;
        while (numberOfPermittedSubclasses-- > 0) {
            classVisitor->visitPermittedSubclass(readClass(currentPermittedSubclassesOffset, buf));
            currentPermittedSubclassesOffset += 2;
        }
    }

    // Visit InnerClasses
    if (innerClassesOffset != 0) {
        int numberOfClasses      = readUnsignedShort(innerClassesOffset);
        int currentClassesOffset = innerClassesOffset + 2;
        while (numberOfClasses-- > 0) {
            classVisitor->visitInnerClass(readClass(currentClassesOffset, buf),
                                          readClass(currentClassesOffset + 2, buf),
                                          readUTF8(currentClassesOffset + 4, buf),
                                          readUnsignedShort(currentClassesOffset + 6));
            currentClassesOffset += 8;
        }
    }

    // Visit Record components
    if (recordOffset != 0) {
        int recordComponentsCount = readUnsignedShort(recordOffset);
        recordOffset += 2;
        while (recordComponentsCount-- > 0) {
            recordOffset = readRecordComponent(classVisitor, ctx, recordOffset);
        }
    }

    // Visit fields and methods
    int fc = readUnsignedShort(off);
    off += 2;
    while (fc--) off = readField(classVisitor, ctx, off);
    int mc = readUnsignedShort(off);
    off += 2;
    while (mc--) off = readMethod(classVisitor, ctx, off);

    classVisitor->visitEnd();
}
int ClassReader::readField(ClassVisitor* cv, Context& ctx, int off)
{
    auto& buf = ctx.charBuffer;
    int   acc = readUnsignedShort(off);
    auto  n   = readUTF8(off + 2, buf);
    auto  d   = readUTF8(off + 4, buf);
    off += 6;

    // Parse field attributes
    Any           constantValue;
    Utf8StringRef signature;
    int           runtimeVisibleAnnotationsOffset       = 0;
    int           runtimeInvisibleAnnotationsOffset     = 0;
    int           runtimeVisibleTypeAnnotationsOffset   = 0;
    int           runtimeInvisibleTypeAnnotationsOffset = 0;
    Attribute*    attributes                            = nullptr;

    int ac = readUnsignedShort(off);
    off += 2;
    while (ac--) {
        auto attributeName = readUTF8(off, buf);
        int  al            = readInt(off + 2);
        off += 6;
        if (attributeName.equals(ZUtf8StringRef::from_static("ConstantValue"))) {
            int cvIdx     = readUnsignedShort(off);
            constantValue = cvIdx == 0 ? Any{} : readConst(cvIdx, buf);
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("Signature"))) {
            signature = readUTF8(off, buf);
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("Deprecated"))) {
            acc |= Opcodes::ACC_DEPRECATED;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("Synthetic"))) {
            acc |= Opcodes::ACC_SYNTHETIC;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("RuntimeVisibleAnnotations"))) {
            runtimeVisibleAnnotationsOffset = off;
        }
        else if (attributeName.equals(
                     ZUtf8StringRef::from_static("RuntimeVisibleTypeAnnotations"))) {
            runtimeVisibleTypeAnnotationsOffset = off;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("RuntimeInvisibleAnnotations"))) {
            runtimeInvisibleAnnotationsOffset = off;
        }
        else if (attributeName.equals(
                     ZUtf8StringRef::from_static("RuntimeInvisibleTypeAnnotations"))) {
            runtimeInvisibleTypeAnnotationsOffset = off;
        }
        else {
            auto* attr =
                readAttribute(ctx.attributePrototypes, attributeName, off, al, buf, -1, nullptr);
            if (attr) {
                attr->nextAttribute = attributes;
                attributes          = attr;
            }
        }
        off += al;
    }

    if (auto* fv = cv->visitField(acc, n, d, signature, constantValue)) {
        // Visit RuntimeVisibleAnnotations
        if (runtimeVisibleAnnotationsOffset != 0) {
            int numAnnotations          = readUnsignedShort(runtimeVisibleAnnotationsOffset);
            int currentAnnotationOffset = runtimeVisibleAnnotationsOffset + 2;
            while (numAnnotations-- > 0) {
                auto annotationDescriptor = readUTF8(currentAnnotationOffset, buf);
                currentAnnotationOffset += 2;
                currentAnnotationOffset =
                    readElementValues(fv->visitAnnotation(annotationDescriptor, true),
                                      currentAnnotationOffset,
                                      true,
                                      buf);
            }
        }

        // Visit RuntimeInvisibleAnnotations
        if (runtimeInvisibleAnnotationsOffset != 0) {
            int numAnnotations          = readUnsignedShort(runtimeInvisibleAnnotationsOffset);
            int currentAnnotationOffset = runtimeInvisibleAnnotationsOffset + 2;
            while (numAnnotations-- > 0) {
                auto annotationDescriptor = readUTF8(currentAnnotationOffset, buf);
                currentAnnotationOffset += 2;
                currentAnnotationOffset =
                    readElementValues(fv->visitAnnotation(annotationDescriptor, false),
                                      currentAnnotationOffset,
                                      true,
                                      buf);
            }
        }

        // Visit RuntimeVisibleTypeAnnotations
        if (runtimeVisibleTypeAnnotationsOffset != 0) {
            int numAnnotations          = readUnsignedShort(runtimeVisibleTypeAnnotationsOffset);
            int currentAnnotationOffset = runtimeVisibleTypeAnnotationsOffset + 2;
            while (numAnnotations-- > 0) {
                currentAnnotationOffset   = readTypeAnnotationTarget(ctx, currentAnnotationOffset);
                auto annotationDescriptor = readUTF8(currentAnnotationOffset, buf);
                currentAnnotationOffset += 2;
                currentAnnotationOffset =
                    readElementValues(fv->visitTypeAnnotation(ctx.currentTypeAnnotationTarget,
                                                              &ctx.currentTypeAnnotationTargetPath,
                                                              annotationDescriptor,
                                                              true),
                                      currentAnnotationOffset,
                                      true,
                                      buf);
            }
        }

        // Visit RuntimeInvisibleTypeAnnotations
        if (runtimeInvisibleTypeAnnotationsOffset != 0) {
            int numAnnotations          = readUnsignedShort(runtimeInvisibleTypeAnnotationsOffset);
            int currentAnnotationOffset = runtimeInvisibleTypeAnnotationsOffset + 2;
            while (numAnnotations-- > 0) {
                currentAnnotationOffset   = readTypeAnnotationTarget(ctx, currentAnnotationOffset);
                auto annotationDescriptor = readUTF8(currentAnnotationOffset, buf);
                currentAnnotationOffset += 2;
                currentAnnotationOffset =
                    readElementValues(fv->visitTypeAnnotation(ctx.currentTypeAnnotationTarget,
                                                              &ctx.currentTypeAnnotationTargetPath,
                                                              annotationDescriptor,
                                                              false),
                                      currentAnnotationOffset,
                                      true,
                                      buf);
            }
        }

        // Visit non-standard attributes
        while (attributes) {
            auto* next                = attributes->nextAttribute;
            attributes->nextAttribute = nullptr;
            fv->visitAttribute(*attributes);
            attributes = next;
        }

        fv->visitEnd();
    }
    return off;
}
int ClassReader::readMethod(ClassVisitor* cv, Context& ctx, int off)
{
    auto& buf                    = ctx.charBuffer;
    ctx.currentMethodAccessFlags = readUnsignedShort(off);
    ctx.currentMethodName        = readUTF8(off + 2, buf);
    ctx.currentMethodDescriptor  = readUTF8(off + 4, buf);
    off += 6;

    // Parse method attributes
    int                        codeOffset       = 0;
    int                        exceptionsOffset = 0;
    std::vector<Utf8StringRef> exceptions;
    bool                       synthetic                                  = false;
    int                        signatureIndex                             = 0;
    int                        runtimeVisibleAnnotationsOffset            = 0;
    int                        runtimeInvisibleAnnotationsOffset          = 0;
    int                        runtimeVisibleParameterAnnotationsOffset   = 0;
    int                        runtimeInvisibleParameterAnnotationsOffset = 0;
    int                        runtimeVisibleTypeAnnotationsOffset        = 0;
    int                        runtimeInvisibleTypeAnnotationsOffset      = 0;
    int                        annotationDefaultOffset                    = 0;
    int                        methodParametersOffset                     = 0;
    Attribute*                 attributes                                 = nullptr;

    int ac = readUnsignedShort(off);
    off += 2;
    while (ac--) {
        auto attributeName = readUTF8(off, buf);
        int  al            = readInt(off + 2);
        off += 6;
        if (attributeName.equals(ZUtf8StringRef::from_static("Code"))) {
            if ((ctx.parsingOptions & SKIP_CODE) == 0) {
                codeOffset = off;
            }
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("Exceptions"))) {
            exceptionsOffset           = off;
            int numExceptions          = readUnsignedShort(exceptionsOffset);
            int currentExceptionOffset = exceptionsOffset + 2;
            for (int i = 0; i < numExceptions; ++i) {
                exceptions.push_back(readClass(currentExceptionOffset, buf));
                currentExceptionOffset += 2;
            }
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("Signature"))) {
            signatureIndex = readUnsignedShort(off);
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("Deprecated"))) {
            ctx.currentMethodAccessFlags |= Opcodes::ACC_DEPRECATED;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("RuntimeVisibleAnnotations"))) {
            runtimeVisibleAnnotationsOffset = off;
        }
        else if (attributeName.equals(
                     ZUtf8StringRef::from_static("RuntimeVisibleTypeAnnotations"))) {
            runtimeVisibleTypeAnnotationsOffset = off;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("AnnotationDefault"))) {
            annotationDefaultOffset = off;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("Synthetic"))) {
            synthetic = true;
            ctx.currentMethodAccessFlags |= Opcodes::ACC_SYNTHETIC;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("RuntimeInvisibleAnnotations"))) {
            runtimeInvisibleAnnotationsOffset = off;
        }
        else if (attributeName.equals(
                     ZUtf8StringRef::from_static("RuntimeInvisibleTypeAnnotations"))) {
            runtimeInvisibleTypeAnnotationsOffset = off;
        }
        else if (attributeName.equals(
                     ZUtf8StringRef::from_static("RuntimeVisibleParameterAnnotations"))) {
            runtimeVisibleParameterAnnotationsOffset = off;
        }
        else if (attributeName.equals(
                     ZUtf8StringRef::from_static("RuntimeInvisibleParameterAnnotations"))) {
            runtimeInvisibleParameterAnnotationsOffset = off;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("MethodParameters"))) {
            methodParametersOffset = off;
        }
        else {
            auto* attr =
                readAttribute(ctx.attributePrototypes, attributeName, off, al, buf, -1, nullptr);
            if (attr) {
                attr->nextAttribute = attributes;
                attributes          = attr;
            }
        }
        off += al;
    }

    Utf8StringRef signatureStr;
    if (signatureIndex != 0) {
        signatureStr = readUtf(signatureIndex, buf);
    }

    auto* mv = cv->visitMethod(ctx.currentMethodAccessFlags,
                               ctx.currentMethodName,
                               ctx.currentMethodDescriptor,
                               signatureStr,
                               exceptions);
    if (!mv) return off;

    // Visit MethodParameters
    if (methodParametersOffset != 0 && (ctx.parsingOptions & SKIP_DEBUG) == 0) {
        int parametersCount        = readByte(methodParametersOffset);
        int currentParameterOffset = methodParametersOffset + 1;
        while (parametersCount-- > 0) {
            mv->visitParameter(readUTF8(currentParameterOffset, buf),
                               readUnsignedShort(currentParameterOffset + 2));
            currentParameterOffset += 4;
        }
    }

    // Visit AnnotationDefault
    if (annotationDefaultOffset != 0) {
        auto* av = mv->visitAnnotationDefault();
        readElementValue(av, {}, annotationDefaultOffset, buf);
        if (av) av->visitEnd();
    }

    // Visit RuntimeVisibleAnnotations
    if (runtimeVisibleAnnotationsOffset != 0) {
        int numAnnotations          = readUnsignedShort(runtimeVisibleAnnotationsOffset);
        int currentAnnotationOffset = runtimeVisibleAnnotationsOffset + 2;
        while (numAnnotations-- > 0) {
            auto annotationDescriptor = readUTF8(currentAnnotationOffset, buf);
            currentAnnotationOffset += 2;
            currentAnnotationOffset =
                readElementValues(mv->visitAnnotation(annotationDescriptor, true),
                                  currentAnnotationOffset,
                                  true,
                                  buf);
        }
    }

    // Visit RuntimeInvisibleAnnotations
    if (runtimeInvisibleAnnotationsOffset != 0) {
        int numAnnotations          = readUnsignedShort(runtimeInvisibleAnnotationsOffset);
        int currentAnnotationOffset = runtimeInvisibleAnnotationsOffset + 2;
        while (numAnnotations-- > 0) {
            auto annotationDescriptor = readUTF8(currentAnnotationOffset, buf);
            currentAnnotationOffset += 2;
            currentAnnotationOffset =
                readElementValues(mv->visitAnnotation(annotationDescriptor, false),
                                  currentAnnotationOffset,
                                  true,
                                  buf);
        }
    }

    // Visit RuntimeVisibleTypeAnnotations
    if (runtimeVisibleTypeAnnotationsOffset != 0) {
        int numAnnotations          = readUnsignedShort(runtimeVisibleTypeAnnotationsOffset);
        int currentAnnotationOffset = runtimeVisibleTypeAnnotationsOffset + 2;
        while (numAnnotations-- > 0) {
            currentAnnotationOffset   = readTypeAnnotationTarget(ctx, currentAnnotationOffset);
            auto annotationDescriptor = readUTF8(currentAnnotationOffset, buf);
            currentAnnotationOffset += 2;
            currentAnnotationOffset =
                readElementValues(mv->visitTypeAnnotation(ctx.currentTypeAnnotationTarget,
                                                          &ctx.currentTypeAnnotationTargetPath,
                                                          annotationDescriptor,
                                                          true),
                                  currentAnnotationOffset,
                                  true,
                                  buf);
        }
    }

    // Visit RuntimeInvisibleTypeAnnotations
    if (runtimeInvisibleTypeAnnotationsOffset != 0) {
        int numAnnotations          = readUnsignedShort(runtimeInvisibleTypeAnnotationsOffset);
        int currentAnnotationOffset = runtimeInvisibleTypeAnnotationsOffset + 2;
        while (numAnnotations-- > 0) {
            currentAnnotationOffset   = readTypeAnnotationTarget(ctx, currentAnnotationOffset);
            auto annotationDescriptor = readUTF8(currentAnnotationOffset, buf);
            currentAnnotationOffset += 2;
            currentAnnotationOffset =
                readElementValues(mv->visitTypeAnnotation(ctx.currentTypeAnnotationTarget,
                                                          &ctx.currentTypeAnnotationTargetPath,
                                                          annotationDescriptor,
                                                          false),
                                  currentAnnotationOffset,
                                  true,
                                  buf);
        }
    }

    // Visit RuntimeVisibleParameterAnnotations
    if (runtimeVisibleParameterAnnotationsOffset != 0) {
        readParameterAnnotations(mv, ctx, runtimeVisibleParameterAnnotationsOffset, true);
    }

    // Visit RuntimeInvisibleParameterAnnotations
    if (runtimeInvisibleParameterAnnotationsOffset != 0) {
        readParameterAnnotations(mv, ctx, runtimeInvisibleParameterAnnotationsOffset, false);
    }

    // Visit non-standard attributes
    while (attributes) {
        auto* next                = attributes->nextAttribute;
        attributes->nextAttribute = nullptr;
        mv->visitAttribute(*attributes);
        attributes = next;
    }

    // Visit Code
    if (codeOffset) {
        mv->visitCode();
        readCode(mv, ctx, codeOffset);
    }

    mv->visitEnd();

    return off;
}
void ClassReader::readCode(MethodVisitor* mv, Context& ctx, int codeOff)
{
    auto& buf = ctx.charBuffer;
    int   ms  = readUnsignedShort(codeOff), ml = readUnsignedShort(codeOff + 2),
         cl   = readInt(codeOff + 4);
    // code_length 来自敌意 class，不可信：负值会让 be 回绕、指令循环按裸指针
    // 扫到缓冲区外内存；先于任何派生量拒绝。checkRange 同时把 cl 钳在
    // classfile 缓冲区内，保证两遍指令循环对 code 数组的扫描不越界。
    if (cl < 0) {
        throw std::invalid_argument("Invalid code length " + std::to_string(cl));
    }
    checkRange(codeOff + 8, cl);
    int off = codeOff + 8, bs = off, be = off + cl;
    // labels 槽位数以 i64 复核后才能 assign：cl 逼近 INT_MAX 时 cl + 1 会在
    // i32 回绕成负数，vector::assign 将其转为 size_t 即天文预分配。正常
    // class 的槽位数不会超过 code 区段字节数 + 1，也就不会超过缓冲区长度。
    if (static_cast<i64>(cl) + 1 > static_cast<i64>(classFileLength_) + 1) {
        throw std::invalid_argument("Code length exceeds class file buffer");
    }
    ctx.currentMethodLabels.assign(cl + 1, nullptr);
    auto ensureLabel = [&](int bytecodeOffset) -> Label* {
        if (bytecodeOffset < 0 || bytecodeOffset > cl) return nullptr;
        Label*& label = ctx.currentMethodLabels[bytecodeOffset];
        if (label == nullptr) label = newReaderLabel();
        if (!label->isResolved()) {
            label->bytecodeOffset = bytecodeOffset;
            label->flags |= Label::FLAG_RESOLVED;
        }
        return label;
    };
    auto requireLabel = [&](int bytecodeOffset) -> Label* {
        Label* label = ensureLabel(bytecodeOffset);
        if (!label)
            throw std::invalid_argument("Invalid bytecode label offset " +
                                        std::to_string(bytecodeOffset) + " for code length " +
                                        std::to_string(cl));
        return label;
    };
    auto instructionLength = [&](int instructionOffset) -> int {
        const int bytecodeOffset = instructionOffset - bs;
        const int opcode         = classFileBuffer_[instructionOffset] & 0xFF;
        if ((opcode >= 0x15 && opcode <= 0x19) || (opcode >= 0x36 && opcode <= 0x3A) ||
            opcode == 0xA9 || opcode == 0x10 || opcode == 0x12 || opcode == 0xBC) {
            return 2;
        }
        if (opcode == 0x11 || opcode == 0x13 || opcode == 0x14 || opcode == 0x84 ||
            (opcode >= 0x99 && opcode <= 0xA8) || (opcode >= 0xB2 && opcode <= 0xB8) ||
            (opcode >= 0xBB && opcode <= 0xBD) || (opcode >= 0xC0 && opcode <= 0xC1) ||
            (opcode >= 0xC6 && opcode <= 0xC7)) {
            return 3;
        }
        if (opcode == 0xC5) {
            return 4;
        }
        if (opcode == 0xB9 || opcode == 0xBA || opcode == 0xC8 || opcode == 0xC9) {
            return 5;
        }
        if (opcode >= Constants::ASM_IFEQ && opcode <= Constants::ASM_IFNONNULL) {
            return 3;
        }
        if (opcode == Constants::ASM_GOTO_W) {
            return 5;
        }
        if (opcode == 0xC4) {
            return (classFileBuffer_[instructionOffset + 1] & 0xFF) == 0x84 ? 6 : 4;
        }
        if (opcode == 0xAA || opcode == 0xAB) {
            const int padding       = (4 - ((bytecodeOffset + 1) % 4)) % 4;
            const int alignedOffset = instructionOffset + 1 + padding;
            if (opcode == 0xAA) {
                const int caseCount = readInt(alignedOffset + 8) - readInt(alignedOffset + 4) + 1;
                return 1 + padding + 12 + caseCount * 4;
            }
            return 1 + padding + 8 + readInt(alignedOffset + 4) * 8;
        }
        return 1;
    };
    // First pass: scan labels from jump targets and try-catch
    while (off < be) {
        int bo = off - bs;
        int op = classFileBuffer_[off] & 0xFF;
        int sz = 1;
        if ((op >= 0x15 && op <= 0x19) || (op >= 0x36 && op <= 0x3A) || op == 0xA9 || op == 0x10 ||
            op == 0x12 || op == 0xBC)
            sz = 2;
        else if (op == 0x11 || op == 0x13 || op == 0x14)
            sz = 3;
        else if (op == 0x84)
            sz = 3;
        else if ((op >= 0x99 && op <= 0xA6) || (op >= 0xC6 && op <= 0xC7) || op == 0xA7 ||
                 op == 0xA8) {
            int t = bo + readShort(off + 1);
            requireLabel(t);
            sz = 3;
        }
        else if (op == 0xC8 || op == 0xC9) {
            int t = bo + readInt(off + 1);
            requireLabel(t);
            sz = 5;
        }
        else if (op >= Constants::ASM_IFEQ && op <= Constants::ASM_IFNONNULL) {
            int t = bo + readUnsignedShort(off + 1);
            requireLabel(t);
            // 条件伪指令展开后会跳到原指令之后，预先创建该位置的标签。
            if (op != Constants::ASM_GOTO && op != Constants::ASM_JSR) requireLabel(bo + 3);
            sz = 3;
        }
        else if (op == Constants::ASM_GOTO_W) {
            int t = bo + readInt(off + 1);
            requireLabel(t);
            sz = 5;
        }
        else if (op == 0xAA || op == 0xAB) {
            int pad  = (4 - ((bo + 1) % 4)) % 4;
            int ao   = off + 1 + pad;
            int dflt = bo + readInt(ao);
            requireLabel(dflt);
            if (op == 0xAA) {
                int n = readInt(ao + 8) - readInt(ao + 4) + 1;
                sz    = pad + 1 + 12 + n * 4;
                for (int j = 0; j < n; j++) {
                    int t = bo + readInt(ao + 12 + j * 4);
                    requireLabel(t);
                }
            }
            else {
                int np = readInt(ao + 4);
                sz     = pad + 1 + 8 + np * 8;
                for (int j = 0; j < np; j++) {
                    int t = bo + readInt(ao + 12 + j * 8);
                    requireLabel(t);
                }
            }
        }
        else if (op == 0xC4) {
            // 指令长度解码要看操作数字节，先于裸读复核文件边界。
            checkRange(off, 2);
            int wop = classFileBuffer_[off + 1] & 0xFF;
            sz      = (wop == 0x84) ? 6 : 4;
        }
        else if (op == 0xB9 || op == 0xBA)
            sz = 5;
        else if (op == 0xC5)
            sz = 4;
        else if ((op >= 0xB2 && op <= 0xB8) || (op >= 0xBB && op <= 0xBD) ||
                 (op >= 0xC0 && op <= 0xC1))
            sz = 3;
        // 每条指令按文件边界复核后才能推进：多字节指令贴着缓冲区结尾时，
        // 操作数裸读（classFileBuffer_[off + 1] 等）会越过堆缓冲区。
        checkRange(off, sz);
        off += sz;
    }
    int etl = readUnsignedShort(off);
    off += 2;
    int ets = off;
    for (int i = 0; i < etl; i++) {
        int sp = readUnsignedShort(off), ep = readUnsignedShort(off + 2),
            hp = readUnsignedShort(off + 4);
        off += 8;
        requireLabel(sp);
        requireLabel(ep);
        requireLabel(hp);
    }
    // Scan Code attributes
    int              lntOff                       = 0;
    int              localVariableTableOffset     = 0;
    int              localVariableTypeTableOffset = 0;
    int              stackMapFrameOffset          = 0;
    int              stackMapTableEndOffset       = 0;
    bool             compressedFrames             = true;
    std::vector<int> visibleTypeAnnotationOffsetsVec;
    std::vector<int> invisibleTypeAnnotationOffsetsVec;
    int              caCnt = readUnsignedShort(off);
    off += 2;
    for (int a = 0; a < caCnt; a++) {
        auto an = readUTF8(off, buf);
        int  al = readInt(off + 2);
        off += 6;
        if (an.equals(ZUtf8StringRef::from_static("LineNumberTable"))) {
            if ((ctx.parsingOptions & SKIP_DEBUG) == 0) {
                lntOff               = off;
                const int lineCount  = readUnsignedShort(lntOff);
                int       lineOffset = lntOff + 2;
                for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
                    ensureLabel(readUnsignedShort(lineOffset));
                    lineOffset += 4;
                }
            }
        }
        else if (an.equals(ZUtf8StringRef::from_static("LocalVariableTable"))) {
            if ((ctx.parsingOptions & SKIP_DEBUG) == 0) {
                localVariableTableOffset = off;
            }
        }
        else if (an.equals(ZUtf8StringRef::from_static("LocalVariableTypeTable"))) {
            if ((ctx.parsingOptions & SKIP_DEBUG) == 0) {
                localVariableTypeTableOffset = off;
            }
        }
        else if (an.equals(ZUtf8StringRef::from_static("RuntimeVisibleTypeAnnotations"))) {
            visibleTypeAnnotationOffsetsVec = readTypeAnnotations(mv, ctx, off, true);
        }
        else if (an.equals(ZUtf8StringRef::from_static("RuntimeInvisibleTypeAnnotations"))) {
            invisibleTypeAnnotationOffsetsVec = readTypeAnnotations(mv, ctx, off, false);
        }
        else if (an.equals(ZUtf8StringRef::from_static("StackMapTable"))) {
            if ((ctx.parsingOptions & SKIP_FRAMES) == 0) {
                stackMapFrameOffset    = off + 2;
                stackMapTableEndOffset = off + al;
            }
        }
        else if (an.equals(ZUtf8StringRef::from_static("StackMap"))) {
            if ((ctx.parsingOptions & SKIP_FRAMES) == 0) {
                stackMapFrameOffset    = off + 2;
                stackMapTableEndOffset = off + al;
                compressedFrames       = false;
            }
        }
        off += al;
    }
    auto ensureLocalVariableLabels = [&](int tableOffset) {
        if (tableOffset == 0) return;
        int numVariables     = readUnsignedShort(tableOffset);
        int currentVarOffset = tableOffset + 2;
        for (int i = 0; i < numVariables; ++i) {
            int startPc = readUnsignedShort(currentVarOffset);
            int length  = readUnsignedShort(currentVarOffset + 2);
            ensureLabel(startPc);
            ensureLabel(startPc + length);
            currentVarOffset += 10;
        }
    };
    auto findLocalVariableSignature =
        [&](int startPc, int length, Utf8StringRef name, int varIndex) -> Utf8StringRef {
        if (localVariableTypeTableOffset == 0) return {};
        int numVariables     = readUnsignedShort(localVariableTypeTableOffset);
        int currentVarOffset = localVariableTypeTableOffset + 2;
        for (int i = 0; i < numVariables; ++i) {
            int  sigStartPc = readUnsignedShort(currentVarOffset);
            int  sigLength  = readUnsignedShort(currentVarOffset + 2);
            auto sigName    = readUTF8(currentVarOffset + 4, buf);
            auto sig        = readUTF8(currentVarOffset + 6, buf);
            int  sigIndex   = readUnsignedShort(currentVarOffset + 8);
            currentVarOffset += 10;
            if (sigStartPc == startPc && sigLength == length && sigIndex == varIndex &&
                sigName.equals(name)) {
                return sig;
            }
        }
        return {};
    };
    ensureLocalVariableLabels(localVariableTableOffset);
    for (int bytecodeOffset = 0; bytecodeOffset <= cl; ++bytecodeOffset) {
        if (ctx.currentMethodLabels[bytecodeOffset] != nullptr) {
            ensureLabel(bytecodeOffset);
        }
    }
    off = ets;
    for (int i = 0; i < etl; i++) {
        int sp = readUnsignedShort(off), ep = readUnsignedShort(off + 2),
            hp = readUnsignedShort(off + 4), ct = readUnsignedShort(off + 6);
        off += 8;
        Utf8StringRef t;
        if (ct) t = readUTF8(cpInfoOffset(ct), buf);
        mv->visitTryCatchBlock(ctx.currentMethodLabels[sp],
                               ctx.currentMethodLabels[ep],
                               ctx.currentMethodLabels[hp],
                               t);
    }

    // Initialize stack map frame context if StackMapTable exists
    bool expandFrames = (ctx.parsingOptions & EXPAND_FRAMES) != 0;
    if (stackMapFrameOffset != 0) {
        ctx.currentFrameOffset          = -1;
        ctx.currentFrameType            = 0;
        ctx.currentFrameLocalCount      = 0;
        ctx.currentFrameLocalCountDelta = 0;
        ctx.currentFrameLocalTypes.assign(ml, ca::core::Any{});
        ctx.currentFrameStackCount = 0;
        ctx.currentFrameStackTypes.assign(ms, ca::core::Any{});
        if (expandFrames) {
            computeImplicitFrame(ctx);
        }
        // (No pre-scan needed — readVerificationTypeInfo creates labels for ITEM_UNINITIALIZED on
        // the fly)
    }

    off                                   = bs;
    int currentVisibleTypeAnnotationIndex = 0;
    int currentVisibleTypeAnnotationBytecodeOffset =
        getTypeAnnotationBytecodeOffset(visibleTypeAnnotationOffsetsVec, 0);
    int currentInvisibleTypeAnnotationIndex = 0;
    int currentInvisibleTypeAnnotationBytecodeOffset =
        getTypeAnnotationBytecodeOffset(invisibleTypeAnnotationOffsetsVec, 0);

    while (off < be) {
        int       bo        = off - bs;
        const int rawOpcode = classFileBuffer_[off] & 0xFF;
        mv->visitInstructionMetadata(bo, instructionLength(off), rawOpcode);

        // Visit stack map frame for this bytecode offset, if any
        while (stackMapFrameOffset != 0 &&
               (ctx.currentFrameOffset == bo || ctx.currentFrameOffset == -1)) {
            if (ctx.currentFrameOffset != -1) {
                if (!compressedFrames || expandFrames) {
                    mv->visitFrame(Opcodes::F_NEW,
                                   ctx.currentFrameLocalCount,
                                   ctx.currentFrameLocalTypes,
                                   ctx.currentFrameStackCount,
                                   ctx.currentFrameStackTypes);
                }
                else {
                    mv->visitFrame(ctx.currentFrameType,
                                   ctx.currentFrameLocalCountDelta,
                                   ctx.currentFrameLocalTypes,
                                   ctx.currentFrameStackCount,
                                   ctx.currentFrameStackTypes);
                }
            }
            if (stackMapFrameOffset < stackMapTableEndOffset) {
                stackMapFrameOffset =
                    readStackMapFrame(stackMapFrameOffset, compressedFrames, expandFrames, ctx);
            }
            else {
                stackMapFrameOffset = 0;
            }
        }

        if (ctx.currentMethodLabels[bo]) mv->visitLabel(ctx.currentMethodLabels[bo]);

        // Visit type annotations for this bytecode offset, if any
        while (currentVisibleTypeAnnotationIndex < (int)visibleTypeAnnotationOffsetsVec.size() &&
               currentVisibleTypeAnnotationBytecodeOffset <= bo) {
            if (currentVisibleTypeAnnotationBytecodeOffset == bo) {
                int annOff = readTypeAnnotationTarget(
                    ctx, visibleTypeAnnotationOffsetsVec[currentVisibleTypeAnnotationIndex]);
                auto desc = readUTF8(annOff, buf);
                annOff += 2;
                readElementValues(mv->visitInsnAnnotation(ctx.currentTypeAnnotationTarget,
                                                          &ctx.currentTypeAnnotationTargetPath,
                                                          desc,
                                                          true),
                                  annOff,
                                  true,
                                  buf);
            }
            currentVisibleTypeAnnotationBytecodeOffset = getTypeAnnotationBytecodeOffset(
                visibleTypeAnnotationOffsetsVec, ++currentVisibleTypeAnnotationIndex);
        }
        while (currentInvisibleTypeAnnotationIndex <
                   (int)invisibleTypeAnnotationOffsetsVec.size() &&
               currentInvisibleTypeAnnotationBytecodeOffset <= bo) {
            if (currentInvisibleTypeAnnotationBytecodeOffset == bo) {
                int annOff = readTypeAnnotationTarget(
                    ctx, invisibleTypeAnnotationOffsetsVec[currentInvisibleTypeAnnotationIndex]);
                auto desc = readUTF8(annOff, buf);
                annOff += 2;
                readElementValues(mv->visitInsnAnnotation(ctx.currentTypeAnnotationTarget,
                                                          &ctx.currentTypeAnnotationTargetPath,
                                                          desc,
                                                          false),
                                  annOff,
                                  true,
                                  buf);
            }
            currentInvisibleTypeAnnotationBytecodeOffset = getTypeAnnotationBytecodeOffset(
                invisibleTypeAnnotationOffsetsVec, ++currentInvisibleTypeAnnotationIndex);
        }

        int op = classFileBuffer_[off] & 0xFF;
        if (op == 0x10) {
            mv->visitIntInsn(op, (i8)classFileBuffer_[off + 1]);
            off += 2;
        }
        else if (op == 0x11) {
            mv->visitIntInsn(op, readShort(off + 1));
            off += 3;
        }
        else if (op == 0x12) {
            mv->visitLdcInsn(readConst(classFileBuffer_[off + 1], buf));
            off += 2;
        }
        else if (op == 0x13 || op == 0x14) {
            mv->visitLdcInsn(readConst(readUnsignedShort(off + 1), buf));
            off += 3;
        }
        else if (op >= 0x15 && op <= 0x19) {
            mv->visitVarInsn(op, classFileBuffer_[off + 1]);
            off += 2;
        }
        else if (op >= 0x1A && op <= 0x35) {
            // 0x1A-0x2D = ILOAD_0..ALOAD_3 (26-45): shorthand var insns, decode to canonical form
            // 0x2E-0x35 = IALOAD..SALOAD (46-53): standard insns
            if (op <= 0x2D) {
                int varOp  = Opcodes::ILOAD + ((op - 0x1A) >> 2);
                int varIdx = (op - 0x1A) & 3;
                mv->visitVarInsn(varOp, varIdx);
            }
            else {
                mv->visitInsn(op);
            }
            off++;
        }
        else if (op >= 0x36 && op <= 0x3A) {
            mv->visitVarInsn(op, classFileBuffer_[off + 1]);
            off += 2;
        }
        else if (op >= 0x3B && op <= 0x4E) {
            // 0x3B-0x4E = ISTORE_0..ASTORE_3 (59-78): shorthand store var insns, decode to
            // canonical form
            int varOp  = Opcodes::ISTORE + ((op - 0x3B) >> 2);
            int varIdx = (op - 0x3B) & 3;
            mv->visitVarInsn(varOp, varIdx);
            off++;
        }
        else if ((op >= 0x02 && op <= 0x0F) || (op >= 0x4F && op <= 0x56) ||
                 (op >= 0x59 && op <= 0x83) || (op >= 0x85 && op <= 0x98) ||
                 (op >= 0xAC && op <= 0xB1) || op == 0x00 || op == 0x01 || op == 0x57 ||
                 op == 0x58 || op == 0xBE || op == 0xBF || op == 0xC2 || op == 0xC3) {
            mv->visitInsn(op);
            off++;
        }
        else if (op == 0x84) {
            mv->visitIincInsn(classFileBuffer_[off + 1] & 0xFF, (i8)classFileBuffer_[off + 2]);
            off += 3;
        }
        else if (op == 0x99 || op == 0x9A || op == 0x9B || op == 0x9C || op == 0x9D || op == 0x9E ||
                 op == 0x9F || op == 0xA0 || op == 0xA1 || op == 0xA2 || op == 0xA3 || op == 0xA4 ||
                 op == 0xA5 || op == 0xA6 || op == 0xA7 || op == 0xA8 || op == 0xC6 || op == 0xC7) {
            int t = bo + readShort(off + 1);
            mv->visitJumpInsn(op, ctx.currentMethodLabels[t]);
            off += 3;
        }
        else if (op == 0xC8 || op == 0xC9) {
            int t = bo + readInt(off + 1);
            mv->visitJumpInsn(op == 0xC8 ? Opcodes::GOTO : Opcodes::JSR,
                              ctx.currentMethodLabels[t]);
            off += 5;
        }
        else if (op >= Constants::ASM_IFEQ && op <= Constants::ASM_JSR) {
            int t          = bo + readUnsignedShort(off + 1);
            int baseOpcode = op - Constants::ASM_OPCODE_DELTA;
            if ((ctx.parsingOptions & EXPAND_ASM_INSNS) == 0) {
                mv->visitJumpInsn(baseOpcode, ctx.currentMethodLabels[t]);
            }
            else if (baseOpcode == Opcodes::GOTO || baseOpcode == Opcodes::JSR) {
                mv->visitJumpInsn(baseOpcode + Constants::WIDE_JUMP_OPCODE_DELTA,
                                  ctx.currentMethodLabels[t]);
            }
            else {
                // JVM 没有宽条件跳转：反转条件跳过一条 goto_w，即可保持原控制流。
                int oppositeOpcode =
                    baseOpcode >= Opcodes::IFNULL ? baseOpcode ^ 1 : ((baseOpcode + 1) ^ 1) - 1;
                mv->visitJumpInsn(oppositeOpcode, ctx.currentMethodLabels[bo + 3]);
                mv->visitJumpInsn(Constants::GOTO_W, ctx.currentMethodLabels[t]);
            }
            off += 3;
        }
        else if (op == Constants::ASM_IFNULL || op == Constants::ASM_IFNONNULL) {
            int t          = bo + readUnsignedShort(off + 1);
            int baseOpcode = op - Constants::ASM_IFNULL_OPCODE_DELTA;
            if ((ctx.parsingOptions & EXPAND_ASM_INSNS) == 0) {
                mv->visitJumpInsn(baseOpcode, ctx.currentMethodLabels[t]);
            }
            else {
                mv->visitJumpInsn(baseOpcode ^ 1, ctx.currentMethodLabels[bo + 3]);
                mv->visitJumpInsn(Constants::GOTO_W, ctx.currentMethodLabels[t]);
            }
            off += 3;
        }
        else if (op == Constants::ASM_GOTO_W) {
            int t = bo + readInt(off + 1);
            mv->visitJumpInsn(Opcodes::GOTO, ctx.currentMethodLabels[t]);
            off += 5;
        }
        else if (op >= 0xB2 && op <= 0xB5) {
            int idx       = readUnsignedShort(off + 1);
            int refOffset = cpInfoOffset(idx);
            int n         = cpInfoOffset(readUnsignedShort(refOffset + 2));
            auto descriptor = normalizeFieldDescriptor(arena_, readUTF8(n + 2, buf));
            mv->visitFieldInsn(op, readClass(refOffset, buf), readUTF8(n, buf), descriptor);
            off += 3;
        }
        else if (op >= 0xB6 && op <= 0xB8) {
            int  idx       = readUnsignedShort(off + 1);
            int  refOffset = cpInfoOffset(idx);
            int  n         = cpInfoOffset(readUnsignedShort(refOffset + 2));
            bool itf       = classFileBuffer_[refOffset - 1] == 11;
            mv->visitMethodInsn(
                op, readClass(refOffset, buf), readUTF8(n, buf), readUTF8(n + 2, buf), itf);
            off += 3;
        }
        else if (op == 0xB9) {
            int idx       = readUnsignedShort(off + 1);
            int refOffset = cpInfoOffset(idx);
            int n         = cpInfoOffset(readUnsignedShort(refOffset + 2));
            mv->visitMethodInsn(
                op, readClass(refOffset, buf), readUTF8(n, buf), readUTF8(n + 2, buf), true);
            off += 5;
        }
        else if (op == 0xBA) {
            int  idx       = readUnsignedShort(off + 1);
            int  indyOff   = cpInfoOffset(idx);
            int  n         = cpInfoOffset(readUnsignedShort(indyOff + 2));
            auto name      = readUTF8(n, buf);
            auto desc      = readUTF8(n + 2, buf);
            int  bsmIdx    = readUnsignedShort(indyOff);
            int  bsmOff    = bootstrapMethodOffset(bsmIdx);
            // readConst 返回值必须先落局部变量：临时 Any 在语句末析构即 delete 内部
            // Handle，悬垂指针下一行才拷贝——glibc free 会覆写前 16 字节（tcache
            // next/key 正好砸在 tag_ + owner_.data 上），Linux 上以 SIGSEGV 收场
            //（MSVC 不复用不清零，恰好幸存，Windows 全绿的 UB）。
            const Any bsmConst = readConst(readUnsignedShort(bsmOff), buf);
            const Handle* bsmHandlePtr = bsmConst.as<Handle>();
            if (!bsmHandlePtr)
                throw std::invalid_argument("Bootstrap method ref is not a MethodHandle");
            Handle bsmHandle = *bsmHandlePtr;
            int  numArgs   = readUnsignedShort(bsmOff + 2);
            std::vector<ca::core::Any> bsmArgs(numArgs);
            for (int ai = 0; ai < numArgs; ++ai) {
                Any a = readConst(readUnsignedShort(bsmOff + 4 + ai * 2), buf);
                if (a.is<int>())
                    bsmArgs[ai] = ca::core::Any(a.cast<int>());
                else if (a.is<i64>())
                    bsmArgs[ai] = ca::core::Any(a.cast<i64>());
                else if (a.is<float>())
                    bsmArgs[ai] = ca::core::Any(a.cast<float>());
                else if (a.is<double>())
                    bsmArgs[ai] = ca::core::Any(a.cast<double>());
                else if (a.is<Utf8StringRef>())
                    bsmArgs[ai] = ca::core::Any(a.cast<Utf8StringRef>());
                else if (a.is<Type>())
                    bsmArgs[ai] = ca::core::Any(a.cast<Type>());
                else if (a.is<Handle>())
                    bsmArgs[ai] = ca::core::Any(a.cast<Handle>());
                else if (a.is<ConstantDynamic>())
                    bsmArgs[ai] = ca::core::Any(a.cast<ConstantDynamic>());
                else
                    bsmArgs[ai] = ca::core::Any{};
            }
            mv->visitInvokeDynamicInsn(name, desc, bsmHandle, bsmArgs);
            off += 5;
        }
        else if (op == 0xBB || op == 0xBD || op == 0xC0 || op == 0xC1) {
            mv->visitTypeInsn(op, readClass(off + 1, buf));
            off += 3;
        }
        else if (op == 0xBC) {
            mv->visitIntInsn(op, classFileBuffer_[off + 1]);
            off += 2;
        }
        else if (op == 0xC5) {
            mv->visitMultiANewArrayInsn(readClass(off + 1, buf), classFileBuffer_[off + 3]);
            off += 4;
        }
        else if (op == 0xAA) {
            int                 pad = (4 - ((bo + 1) % 4)) % 4;
            int                 ao  = off + 1 + pad;
            int                 lo = readInt(ao + 4), hi = readInt(ao + 8), n = hi - lo + 1;
            std::vector<Label*> tl;
            for (int j = 0; j < n; j++)
                tl.push_back(ctx.currentMethodLabels[bo + readInt(ao + 12 + j * 4)]);
            mv->visitTableSwitchInsn(lo, hi, ctx.currentMethodLabels[bo + readInt(ao)], tl);
            off += pad + 1 + 12 + n * 4;
        }
        else if (op == 0xAB) {
            int                 pad = (4 - ((bo + 1) % 4)) % 4;
            int                 ao  = off + 1 + pad;
            int                 np  = readInt(ao + 4);
            std::vector<int>    ks;
            std::vector<Label*> ll;
            for (int j = 0; j < np; j++) {
                ks.push_back(readInt(ao + 8 + j * 8));
                ll.push_back(ctx.currentMethodLabels[bo + readInt(ao + 12 + j * 8)]);
            }
            mv->visitLookupSwitchInsn(ctx.currentMethodLabels[bo + readInt(ao)], ks, ll);
            off += pad + 1 + 8 + np * 8;
        }
        else if (op == 0xC4) {
            int wop = classFileBuffer_[off + 1] & 0xFF;
            if (wop == 0x84) {
                mv->visitIincInsn(readUnsignedShort(off + 2), (i16)readUnsignedShort(off + 4));
                off += 6;
            }
            else {
                mv->visitVarInsn(wop, readUnsignedShort(off + 2));
                off += 4;
            }
        }
        else if (op == 0xA9) {
            mv->visitVarInsn(op, classFileBuffer_[off + 1]);
            off += 2;
        }
        else {
            off++;
        }
    }
    if (ctx.currentMethodLabels[cl]) mv->visitLabel(ctx.currentMethodLabels[cl]);
    if (lntOff) {
        int n  = readUnsignedShort(lntOff);
        int lo = lntOff + 2;
        for (int i = 0; i < n; i++) {
            int sp = readUnsignedShort(lo), ln = readUnsignedShort(lo + 2);
            lo += 4;
            if (ctx.currentMethodLabels[sp]) mv->visitLineNumber(ln, ctx.currentMethodLabels[sp]);
        }
    }

    // Visit LocalVariableTable
    if (localVariableTableOffset != 0) {
        int numVariables     = readUnsignedShort(localVariableTableOffset);
        int currentVarOffset = localVariableTableOffset + 2;
        for (int i = 0; i < numVariables; ++i) {
            int  startPc  = readUnsignedShort(currentVarOffset);
            int  length   = readUnsignedShort(currentVarOffset + 2);
            auto varName  = readUTF8(currentVarOffset + 4, buf);
            auto varDesc  = readUTF8(currentVarOffset + 6, buf);
            int  varIndex = readUnsignedShort(currentVarOffset + 8);
            currentVarOffset += 10;
            Label* startLabel = ensureLabel(startPc);
            Label* endLabel   = ensureLabel(startPc + length);
            if (startLabel) {
                mv->visitLocalVariable(
                    varName,
                    varDesc,
                    findLocalVariableSignature(startPc, length, varName, varIndex),
                    startLabel,
                    endLabel,
                    varIndex);
            }
        }
    }

    // Visit LocalVariableTypeTable
    if (localVariableTableOffset == 0 && localVariableTypeTableOffset != 0) {
        int numVariables     = readUnsignedShort(localVariableTypeTableOffset);
        int currentVarOffset = localVariableTypeTableOffset + 2;
        for (int i = 0; i < numVariables; ++i) {
            int  startPc  = readUnsignedShort(currentVarOffset);
            int  length   = readUnsignedShort(currentVarOffset + 2);
            auto varName  = readUTF8(currentVarOffset + 4, buf);
            auto varSig   = readUTF8(currentVarOffset + 6, buf);
            int  varIndex = readUnsignedShort(currentVarOffset + 8);
            currentVarOffset += 10;
            Label* startLabel = ensureLabel(startPc);
            Label* endLabel   = ensureLabel(startPc + length);
            if (startLabel) {
                mv->visitLocalVariable(varName, {}, varSig, startLabel, endLabel, varIndex);
            }
        }
    }

    // Visit local variable type annotations from RuntimeVisibleTypeAnnotations
    for (int offIdx : visibleTypeAnnotationOffsetsVec) {
        int targetType = readByte(offIdx);
        if (targetType == TypeReference::LOCAL_VARIABLE ||
            targetType == TypeReference::RESOURCE_VARIABLE) {
            int  cur  = readTypeAnnotationTarget(ctx, offIdx);
            auto desc = readUTF8(cur, buf);
            cur += 2;
            readElementValues(
                mv->visitLocalVariableAnnotation(ctx.currentTypeAnnotationTarget,
                                                 &ctx.currentTypeAnnotationTargetPath,
                                                 ctx.currentLocalVariableAnnotationRangeStarts,
                                                 ctx.currentLocalVariableAnnotationRangeEnds,
                                                 ctx.currentLocalVariableAnnotationRangeIndices,
                                                 desc,
                                                 true),
                cur,
                true,
                buf);
        }
    }

    // Visit local variable type annotations from RuntimeInvisibleTypeAnnotations
    for (int offIdx : invisibleTypeAnnotationOffsetsVec) {
        int targetType = readByte(offIdx);
        if (targetType == TypeReference::LOCAL_VARIABLE ||
            targetType == TypeReference::RESOURCE_VARIABLE) {
            int  cur  = readTypeAnnotationTarget(ctx, offIdx);
            auto desc = readUTF8(cur, buf);
            cur += 2;
            readElementValues(
                mv->visitLocalVariableAnnotation(ctx.currentTypeAnnotationTarget,
                                                 &ctx.currentTypeAnnotationTargetPath,
                                                 ctx.currentLocalVariableAnnotationRangeStarts,
                                                 ctx.currentLocalVariableAnnotationRangeEnds,
                                                 ctx.currentLocalVariableAnnotationRangeIndices,
                                                 desc,
                                                 false),
                cur,
                true,
                buf);
        }
    }

    mv->visitMaxs(ms, ml);
}
i32 ClassReader::getFirstAttributeOffset() const
{
    // Matches ASM's ClassReader.getFirstAttributeOffset()
    int off = header_ + 6;              // start at interfaces_count
    int ic  = readUnsignedShort(off);   // read interfaces_count
    off += 2 + ic * 2;                  // skip past interfaces to fields_count

    int fc = readUnsignedShort(off);   // read fields_count
    off += 2;                          // advance past fields_count to first field_info
    for (int f = 0; f < fc; f++) {
        int ac = readUnsignedShort(off + 6);   // field attributes_count (after name+desc)
        off += 8;   // skip field header: access(2)+name(2)+desc(2)+attrs_count(2)
        for (int a = 0; a < ac; a++) {
            int al = readInt(off + 2);   // attribute_length
            off += 6 + al;               // skip name(2)+length(4)+content(al)
        }
    }   // off now points to methods_count

    int mc = readUnsignedShort(off);   // read methods_count
    off += 2;                          // advance past methods_count to first method_info
    for (int m = 0; m < mc; m++) {
        int ac = readUnsignedShort(off + 6);   // method attributes_count (after name+desc)
        off += 8;                              // skip method header
        for (int a = 0; a < ac; a++) {
            int al = readInt(off + 2);   // attribute_length
            off += 6 + al;               // skip name(2)+length(4)+content(al)
        }
    }   // off now points to class-level attributes_count
    return off;   // = attributes_count offset
}
Any ClassReader::readConst(i32 idx, std::vector<char>& buf)
{
    int cp = cpInfoOffset(idx);
    switch (classFileBuffer_[cp - 1]) {
    case 3: return Any(readInt(cp));   // CONSTANT_Integer
    case 4:
    {
        int   b = readInt(cp);
        float v;
        memcpy(&v, &b, 4);
        return Any(v);
    }   // CONSTANT_Float
    case 5: return Any((i64)readLong(cp));   // CONSTANT_Long
    case 6:
    {
        int64_t b = readLong(cp);
        double  v;
        memcpy(&v, &b, 8);
        return Any(v);
    }   // CONSTANT_Double
    case 7: return Any(Type::getObjectType(readUTF8(cp, buf)));   // CONSTANT_Class → Type
    case 8:
    {
        return Any(readUTF8(cp, buf));
    }   // CONSTANT_String
    case 15:
    {   // CONSTANT_MethodHandle
        int  refKind = readByte(cp);
        int  refIdx  = readUnsignedShort(cp + 1);
        int  refOff  = cpInfoOffset(refIdx);
        int  natOff  = cpInfoOffset(readUnsignedShort(refOff + 2));
        auto owner   = readClass(refOff, buf);
        auto name    = readUTF8(natOff, buf);
        auto desc    = readUTF8(natOff + 2, buf);
        bool isItf   = classFileBuffer_[refOff - 1] == 11;   // CONSTANT_INTERFACE_METHODREF_TAG
        return Any(Handle(refKind, owner, name, desc, isItf));
    }
    case 16: return Any(Type::getMethodType(readUTF8(cp, buf)));   // CONSTANT_MethodType → Type
    case 17: return readConstantDynamic(idx, buf);   // CONSTANT_Dynamic → ConstantDynamic
    default: return Any{};
    }
}
int ClassReader::readElementValues(AnnotationVisitor* av, int off, bool named,
                                   std::vector<char>& buf)
{
    if (!av) {
        int n = readUnsignedShort(off);
        off += 2;
        for (int i = 0; i < n; i++) {
            if (named) off += 2;
            off = readElementValue(nullptr, {}, off, buf);
        }
        return off;
    }
    int n = readUnsignedShort(off);
    off += 2;
    for (int i = 0; i < n; i++) {
        ca::str::Utf8StringRef name;
        if (named) {
            name = readUTF8(off, buf);
            off += 2;
        }
        off = readElementValue(av, name, off, buf);
    }
    av->visitEnd();
    return off;
}
int ClassReader::readElementValue(AnnotationVisitor* av, ca::str::Utf8StringRef name, int off,
                                  std::vector<char>& buf)
{
    // '@'/'[' 分支经 readElementValues 互递归，敌意 class 可声明数万层嵌套把
    // 递归打到栈溢出。深度上限与 libjclassfile AnnotationReader::MAX_NESTING
    // 对齐；guard 放在 av == nullptr 分支之前，两条递归路径都受约束。
    if (annotationDepth_ >= MAX_ANNOTATION_NESTING) {
        throw std::invalid_argument("Annotation element value nesting too deep");
    }
    AnnotationDepthGuard depthGuard(*this);
    if (!av) {
        int t = readByte(off++);
        switch (t) {
        case 'B':
        case 'C':
        case 'D':
        case 'F':
        case 'I':
        case 'J':
        case 'S':
        case 'Z':
        case 's': off += 2; break;
        case 'e': off += 4; break;
        case 'c': off += 2; break;
        case '@':
            off += 2;
            off = readElementValues(nullptr, off, true, buf);
            break;
        case '[': off = readElementValues(nullptr, off, false, buf); break;
        }
        return off;
    }
    int t = readByte(off++);
    switch (t) {
    case 'B':
    {
        int ci = readUnsignedShort(off);
        off += 2;
        av->visit(name, (i8)readInt(cpInfoOffset(ci)));
        break;
    }
    case 'C':
    {
        int ci = readUnsignedShort(off);
        off += 2;
        av->visit(name, (u16)readInt(cpInfoOffset(ci)));
        break;
    }
    case 'D':
    case 'F':
    case 'I':
    case 'J':
    {
        int ci = readUnsignedShort(off);
        off += 2;
        av->visit(name, readConst(ci, buf));
        break;
    }
    case 'S':
    {
        int ci = readUnsignedShort(off);
        off += 2;
        av->visit(name, (i16)readInt(cpInfoOffset(ci)));
        break;
    }
    case 'Z':
    {
        int ci = readUnsignedShort(off);
        off += 2;
        int ival = readInt(cpInfoOffset(ci));
        av->visit(name, ival != 0);
        break;
    }
    case 's':
    {
        int ci = readUnsignedShort(off);
        off += 2;
        av->visit(name, readUtf(ci, buf));
        break;
    }
    case 'e':
    {
        auto tn = readUTF8(off, buf);
        off += 2;
        auto cn = readUTF8(off, buf);
        off += 2;
        av->visitEnum(name, tn, cn);
        break;
    }
    case 'c':
    {
        auto d = readUTF8(off, buf);
        off += 2;
        av->visit(name, Type::getType(d));
        break;
    }
    case '@':
    {
        auto d = readUTF8(off, buf);
        off += 2;
        off = readElementValues(av->visitAnnotation(name, d), off, true, buf);
        break;
    }
    case '[':
    {
        off = readElementValues(av->visitArray(name), off, false, buf);
        break;
    }
    }
    return off;
}
void ClassReader::readBootstrapMethodsAttribute(i32 ms)
{
    std::vector<char> cb(ms > 0 ? ms : 1);
    int               off      = getFirstAttributeOffset();
    int               numAttrs = readUnsignedShort(off);
    off += 2;
    for (int i = numAttrs; i > 0; --i) {
        auto an = readUTF8(off, cb);
        int  al = readInt(off + 2);
        off += 6;
        if (an.equals(ZUtf8StringRef::from_static("BootstrapMethods"))) {
            int n                   = readUnsignedShort(off);
            bootstrapMethodOffsets_.assign(static_cast<usize>(n), 0);
            int bsmOff              = off + 2;
            for (int j = 0; j < n; ++j) {
                bootstrapMethodOffsets_[j] = bsmOff;
                bsmOff += 4 + readUnsignedShort(bsmOff + 2) * 2;
            }
            return;
        }
        off += al;
    }
    // BootstrapMethods attribute must exist if hasBootstrapMethods is true.
    throw std::invalid_argument("Missing BootstrapMethods attribute");
}
Any ClassReader::readConstantDynamic(i32 idx, std::vector<char>& buf)
{
    if (!constantDynamicValues_.empty() && constantDynamicValues_[idx].has_value())
        return constantDynamicValues_[idx];
    // 环引用守卫：condy bootstrap 参数互指会让本函数无界递归（栈溢出不可捕获）。
    if (constantDynamicDepth_ >= MAX_CONSTANT_DYNAMIC_NESTING)
        throw std::invalid_argument("ConstantDynamic nesting too deep (bootstrap arg cycle?)");
    CondyDepthGuard depthGuard(constantDynamicDepth_);
    int              cp        = cpInfoOffset(idx);
    int              natOff    = cpInfoOffset(readUnsignedShort(cp + 2));
    auto             name      = readUTF8(natOff, buf);
    auto             desc      = readUTF8(natOff + 2, buf);
    int              bsmIdx    = readUnsignedShort(cp);
    int              bsmOff    = bootstrapMethodOffset(bsmIdx);
    // 同 readCode 的 indy 路径：临时 Any 的悬垂 as<Handle>() 读取（Linux 段错误源）。
    const Any        bsmConst = readConst(readUnsignedShort(bsmOff), buf);
    const Handle*    bsmHandlePtr = bsmConst.as<Handle>();
    if (!bsmHandlePtr)
        throw std::invalid_argument("Bootstrap method ref is not a MethodHandle");
    Handle           bsmHandle = *bsmHandlePtr;
    int              numArgs   = readUnsignedShort(bsmOff + 2);
    std::vector<Any> bsmArgs(numArgs);
    int              argOff = bsmOff + 4;
    for (int i = 0; i < numArgs; ++i) {
        bsmArgs[i] = readConst(readUnsignedShort(argOff), buf);
        argOff += 2;
    }
    auto cd = ConstantDynamic(name, desc, bsmHandle, bsmArgs);
    if (!constantDynamicValues_.empty()) constantDynamicValues_[idx] = cd;
    return Any(cd);
}
// ---- readTypeAnnotations ----
std::vector<int> ClassReader::readTypeAnnotations(MethodVisitor* mv, Context& context,
                                                  int runtimeTypeAnnotationsOffset, bool visible)
{
    (void)mv;
    auto& buf            = context.charBuffer;
    int   currentOffset  = runtimeTypeAnnotationsOffset;
    int   numAnnotations = readUnsignedShort(currentOffset);
    currentOffset += 2;
    std::vector<int> offsets;
    offsets.reserve(numAnnotations);
    for (int i = 0; i < numAnnotations; ++i) {
        offsets.push_back(currentOffset);
        int targetType = readInt(currentOffset);
        switch ((u32)(targetType) >> 24) {
        case TypeReference::LOCAL_VARIABLE:
        case TypeReference::RESOURCE_VARIABLE:
        {
            int tableLength = readUnsignedShort(currentOffset + 1);
            currentOffset += 3;
            for (int t = 0; t < tableLength; ++t) {
                int startPc = readUnsignedShort(currentOffset);
                int length  = readUnsignedShort(currentOffset + 2);
                currentOffset += 6;
                if (!context.currentMethodLabels[startPc])
                    context.currentMethodLabels[startPc] = newReaderLabel();
                if (!context.currentMethodLabels[startPc + length])
                    context.currentMethodLabels[startPc + length] = newReaderLabel();
            }
            break;
        }
        case TypeReference::CAST:
        case TypeReference::CONSTRUCTOR_INVOCATION_TYPE_ARGUMENT:
        case TypeReference::METHOD_INVOCATION_TYPE_ARGUMENT:
        case TypeReference::CONSTRUCTOR_REFERENCE_TYPE_ARGUMENT:
        case TypeReference::METHOD_REFERENCE_TYPE_ARGUMENT: currentOffset += 4; break;
        case TypeReference::CLASS_EXTENDS:
        case TypeReference::CLASS_TYPE_PARAMETER_BOUND:
        case TypeReference::METHOD_TYPE_PARAMETER_BOUND:
        case TypeReference::THROWS:
        case TypeReference::EXCEPTION_PARAMETER:
        case TypeReference::INSTANCEOF:
        case TypeReference::NEW:
        case TypeReference::CONSTRUCTOR_REFERENCE:
        case TypeReference::METHOD_REFERENCE: currentOffset += 3; break;
        case TypeReference::CLASS_TYPE_PARAMETER:
        case TypeReference::METHOD_TYPE_PARAMETER:
        case TypeReference::METHOD_FORMAL_PARAMETER:
        case TypeReference::FIELD:
        case TypeReference::METHOD_RETURN:
        case TypeReference::METHOD_RECEIVER:
        default: throw std::invalid_argument("Unexpected type annotation target type in Code");
        }
        int pathLength = readByte(currentOffset);
        if (((u32)(targetType) >> 24) == TypeReference::EXCEPTION_PARAMETER) {
            TypePath path;
            if (pathLength > 0) {
                path.data_.assign(classFileBuffer_ + currentOffset,
                                  classFileBuffer_ + currentOffset + 1 + 2 * pathLength);
            }
            currentOffset += 1 + 2 * pathLength;
            auto annotationDescriptor = readUTF8(currentOffset, buf);
            currentOffset += 2;
            currentOffset = readElementValues(
                mv->visitTryCatchAnnotation(
                    targetType & 0xFFFFFF00, &path, annotationDescriptor, visible),
                currentOffset,
                true,
                buf);
        }
        else {
            currentOffset += 3 + 2 * pathLength;
            currentOffset = readElementValues(nullptr, currentOffset, true, buf);
        }
    }
    return offsets;
}

// ---- getTypeAnnotationBytecodeOffset ----
int ClassReader::getTypeAnnotationBytecodeOffset(const std::vector<int>& typeAnnotationOffsets,
                                                 int                     typeAnnotationIndex)
{
    if (typeAnnotationIndex >= (int)typeAnnotationOffsets.size()) return -1;
    int targetType = readByte(typeAnnotationOffsets[typeAnnotationIndex]);
    if (targetType < TypeReference::INSTANCEOF) return -1;
    return readUnsignedShort(typeAnnotationOffsets[typeAnnotationIndex] + 1);
}

// ---- readAnnotation ----
int ClassReader::readAnnotation(AnnotationVisitor* av, int annotationOffset, bool named,
                                std::vector<char>& charBuffer)
{
    auto annotationDescriptor = readUTF8(annotationOffset, charBuffer);
    annotationOffset += 2;
    return readElementValues(av, annotationOffset, named, charBuffer);
}

// ---- readAnnotationType ----
int ClassReader::readAnnotationType(AnnotationVisitor* av, int annotationOffset,
                                    std::vector<char>& charBuffer)
{
    auto annotationDescriptor = readUTF8(annotationOffset, charBuffer);
    annotationOffset += 2;
    return annotationOffset;
}

// ---- readParameterAnnotations ----
int ClassReader::readParameterAnnotations(MethodVisitor* mv, Context& ctx,
                                          int parameterAnnotationOffset, bool visible)
{
    auto& buf           = ctx.charBuffer;
    int   currentOffset = parameterAnnotationOffset;
    int   numParameters = classFileBuffer_[currentOffset++] & 0xFF;
    mv->visitAnnotableParameterCount(numParameters, visible);
    for (int i = 0; i < numParameters; ++i) {
        int numAnnotations = readUnsignedShort(currentOffset);
        currentOffset += 2;
        while (numAnnotations-- > 0) {
            auto annotationDescriptor = readUTF8(currentOffset, buf);
            currentOffset += 2;
            currentOffset =
                readElementValues(mv->visitParameterAnnotation(i, annotationDescriptor, visible),
                                  currentOffset,
                                  true,
                                  buf);
        }
    }
    return currentOffset;
}

// ---- readTypeAnnotationTarget(Context&, int) ----
int ClassReader::readTypeAnnotationTarget(Context& context, int typeAnnotationOffset)
{
    int currentOffset = typeAnnotationOffset;
    int targetType    = readInt(typeAnnotationOffset);
    switch ((u32)(targetType) >> 24) {
    case TypeReference::CLASS_TYPE_PARAMETER:
    case TypeReference::METHOD_TYPE_PARAMETER:
    case TypeReference::METHOD_FORMAL_PARAMETER:
        targetType &= 0xFFFF0000;
        currentOffset += 2;
        break;
    case TypeReference::FIELD:
    case TypeReference::METHOD_RETURN:
    case TypeReference::METHOD_RECEIVER:
        targetType &= 0xFF000000;
        currentOffset += 1;
        break;
    case TypeReference::LOCAL_VARIABLE:
    case TypeReference::RESOURCE_VARIABLE:
        targetType &= 0xFF000000;
        {
            int tableLength = readUnsignedShort(currentOffset + 1);
            currentOffset += 3;
            context.currentLocalVariableAnnotationRangeStarts.assign(tableLength, nullptr);
            context.currentLocalVariableAnnotationRangeEnds.assign(tableLength, nullptr);
            context.currentLocalVariableAnnotationRangeIndices.assign(tableLength, 0);
            for (int i = 0; i < tableLength; ++i) {
                int startPc = readUnsignedShort(currentOffset);
                int length  = readUnsignedShort(currentOffset + 2);
                int index   = readUnsignedShort(currentOffset + 4);
                currentOffset += 6;
                context.currentLocalVariableAnnotationRangeStarts[i] =
                    context.currentMethodLabels[startPc];
                context.currentLocalVariableAnnotationRangeEnds[i] =
                    context.currentMethodLabels[startPc + length];
                context.currentLocalVariableAnnotationRangeIndices[i] = index;
            }
        }
        break;
    case TypeReference::CAST:
    case TypeReference::CONSTRUCTOR_INVOCATION_TYPE_ARGUMENT:
    case TypeReference::METHOD_INVOCATION_TYPE_ARGUMENT:
    case TypeReference::CONSTRUCTOR_REFERENCE_TYPE_ARGUMENT:
    case TypeReference::METHOD_REFERENCE_TYPE_ARGUMENT:
        targetType &= 0xFF0000FF;
        currentOffset += 4;
        break;
    case TypeReference::CLASS_EXTENDS:
    case TypeReference::CLASS_TYPE_PARAMETER_BOUND:
    case TypeReference::METHOD_TYPE_PARAMETER_BOUND:
    case TypeReference::THROWS:
    case TypeReference::EXCEPTION_PARAMETER:
        targetType &= 0xFFFFFF00;
        currentOffset += 3;
        break;
    case TypeReference::INSTANCEOF:
    case TypeReference::NEW:
    case TypeReference::CONSTRUCTOR_REFERENCE:
    case TypeReference::METHOD_REFERENCE:
        targetType &= 0xFF000000;
        currentOffset += 3;
        break;
    default: throw std::invalid_argument("Unknown type annotation target type");
    }
    context.currentTypeAnnotationTarget = targetType;
    // Parse and store the target_path structure.
    int pathLength                          = readByte(currentOffset);
    context.currentTypeAnnotationTargetPath = TypePath();
    if (pathLength > 0) {
        // Copy raw TypePath data from class file
        int pathDataSize = 1 + 2 * pathLength;
        context.currentTypeAnnotationTargetPath.data_.assign(
            classFileBuffer_ + currentOffset, classFileBuffer_ + currentOffset + pathDataSize);
    }
    return currentOffset + 1 + 2 * pathLength;
}

// ---- readTypeAnnotationTarget(Context&, int, int) ----
void ClassReader::readTypeAnnotationTarget(Context& context, int targetType, int annotationOffset)
{
    // This overload is called when the target type is already known.
    // Just parse the target_path and store the target.
    context.currentTypeAnnotationTarget     = targetType;
    int pathLength                          = readByte(annotationOffset);
    context.currentTypeAnnotationTargetPath = TypePath();
    if (pathLength > 0) {
        int pathDataSize = 1 + 2 * pathLength;
        context.currentTypeAnnotationTargetPath.data_.assign(
            classFileBuffer_ + annotationOffset,
            classFileBuffer_ + annotationOffset + pathDataSize);
    }
}

// ---- readRecordComponent ----
int ClassReader::readRecordComponent(ClassVisitor* cv, Context& ctx, int recordComponentOffset)
{
    auto& buf           = ctx.charBuffer;
    int   currentOffset = recordComponentOffset;
    auto  name          = readUTF8(currentOffset, buf);
    auto  descriptor    = readUTF8(currentOffset + 2, buf);
    currentOffset += 4;

    // Parse record component attributes
    Utf8StringRef signature;
    int           runtimeVisibleAnnotationsOffset       = 0;
    int           runtimeInvisibleAnnotationsOffset     = 0;
    int           runtimeVisibleTypeAnnotationsOffset   = 0;
    int           runtimeInvisibleTypeAnnotationsOffset = 0;
    Attribute*    attributes                            = nullptr;

    int attributesCount = readUnsignedShort(currentOffset);
    currentOffset += 2;
    while (attributesCount-- > 0) {
        auto attributeName   = readUTF8(currentOffset, buf);
        int  attributeLength = readInt(currentOffset + 2);
        currentOffset += 6;
        if (attributeName.equals(ZUtf8StringRef::from_static("Signature"))) {
            signature = readUTF8(currentOffset, buf);
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("RuntimeVisibleAnnotations"))) {
            runtimeVisibleAnnotationsOffset = currentOffset;
        }
        else if (attributeName.equals(
                     ZUtf8StringRef::from_static("RuntimeVisibleTypeAnnotations"))) {
            runtimeVisibleTypeAnnotationsOffset = currentOffset;
        }
        else if (attributeName.equals(ZUtf8StringRef::from_static("RuntimeInvisibleAnnotations"))) {
            runtimeInvisibleAnnotationsOffset = currentOffset;
        }
        else if (attributeName.equals(
                     ZUtf8StringRef::from_static("RuntimeInvisibleTypeAnnotations"))) {
            runtimeInvisibleTypeAnnotationsOffset = currentOffset;
        }
        else {
            auto* attr = readAttribute(ctx.attributePrototypes,
                                       attributeName,
                                       currentOffset,
                                       attributeLength,
                                       buf,
                                       -1,
                                       nullptr);
            if (attr) {
                attr->nextAttribute = attributes;
                attributes          = attr;
            }
        }
        currentOffset += attributeLength;
    }

    auto* rcv = cv->visitRecordComponent(name, descriptor, signature);
    if (!rcv) return currentOffset;

    // Visit RuntimeVisibleAnnotations
    if (runtimeVisibleAnnotationsOffset != 0) {
        int numAnnotations = readUnsignedShort(runtimeVisibleAnnotationsOffset);
        int annOff         = runtimeVisibleAnnotationsOffset + 2;
        while (numAnnotations-- > 0) {
            auto ad = readUTF8(annOff, buf);
            annOff += 2;
            annOff = readElementValues(rcv->visitAnnotation(ad, true), annOff, true, buf);
        }
    }

    // Visit RuntimeInvisibleAnnotations
    if (runtimeInvisibleAnnotationsOffset != 0) {
        int numAnnotations = readUnsignedShort(runtimeInvisibleAnnotationsOffset);
        int annOff         = runtimeInvisibleAnnotationsOffset + 2;
        while (numAnnotations-- > 0) {
            auto ad = readUTF8(annOff, buf);
            annOff += 2;
            annOff = readElementValues(rcv->visitAnnotation(ad, false), annOff, true, buf);
        }
    }

    // Visit RuntimeVisibleTypeAnnotations
    if (runtimeVisibleTypeAnnotationsOffset != 0) {
        int numAnnotations = readUnsignedShort(runtimeVisibleTypeAnnotationsOffset);
        int annOff         = runtimeVisibleTypeAnnotationsOffset + 2;
        while (numAnnotations-- > 0) {
            annOff  = readTypeAnnotationTarget(ctx, annOff);
            auto ad = readUTF8(annOff, buf);
            annOff += 2;
            annOff =
                readElementValues(rcv->visitTypeAnnotation(ctx.currentTypeAnnotationTarget,
                                                           &ctx.currentTypeAnnotationTargetPath,
                                                           ad,
                                                           true),
                                  annOff,
                                  true,
                                  buf);
        }
    }

    // Visit RuntimeInvisibleTypeAnnotations
    if (runtimeInvisibleTypeAnnotationsOffset != 0) {
        int numAnnotations = readUnsignedShort(runtimeInvisibleTypeAnnotationsOffset);
        int annOff         = runtimeInvisibleTypeAnnotationsOffset + 2;
        while (numAnnotations-- > 0) {
            annOff  = readTypeAnnotationTarget(ctx, annOff);
            auto ad = readUTF8(annOff, buf);
            annOff += 2;
            annOff =
                readElementValues(rcv->visitTypeAnnotation(ctx.currentTypeAnnotationTarget,
                                                           &ctx.currentTypeAnnotationTargetPath,
                                                           ad,
                                                           false),
                                  annOff,
                                  true,
                                  buf);
        }
    }

    // Visit non-standard attributes
    while (attributes) {
        auto* next                = attributes->nextAttribute;
        attributes->nextAttribute = nullptr;
        rcv->visitAttribute(*attributes);
        attributes = next;
    }

    rcv->visitEnd();
    return currentOffset;
}

// ---- readModuleAttributes ----
void ClassReader::readModuleAttributes(ClassVisitor* cv, Context& ctx, int moduleOffset,
                                       int                  modulePackagesOffset,
                                       const Utf8StringRef& moduleMainClass)
{
    auto& buf           = ctx.charBuffer;
    int   currentOffset = moduleOffset;
    auto  moduleName    = readModule(currentOffset, buf);
    int   moduleFlags   = readUnsignedShort(currentOffset + 2);
    auto  moduleVersion = readUTF8(currentOffset + 4, buf);
    currentOffset += 6;

    auto* mv = cv->visitModule(moduleName, moduleFlags, moduleVersion);
    if (!mv) return;

    // Visit ModuleMainClass
    if (moduleMainClass.data()) {
        mv->visitMainClass(moduleMainClass);
    }

    // Visit ModulePackages
    if (modulePackagesOffset != 0) {
        int packageCount         = readUnsignedShort(modulePackagesOffset);
        int currentPackageOffset = modulePackagesOffset + 2;
        while (packageCount-- > 0) {
            mv->visitPackage(readPackage(currentPackageOffset, buf));
            currentPackageOffset += 2;
        }
    }

    // Read requires_count and requires
    int requiresCount = readUnsignedShort(currentOffset);
    currentOffset += 2;
    while (requiresCount-- > 0) {
        auto
            requires
        = readModule(currentOffset, buf);
        int  requiresFlags   = readUnsignedShort(currentOffset + 2);
        auto requiresVersion = readUTF8(currentOffset + 4, buf);
        currentOffset += 6;
        mv->visitRequire(requires, requiresFlags, requiresVersion);
    }

    // Read exports_count and exports
    int exportsCount = readUnsignedShort(currentOffset);
    currentOffset += 2;
    while (exportsCount-- > 0) {
        auto exports        = readPackage(currentOffset, buf);
        int  exportsFlags   = readUnsignedShort(currentOffset + 2);
        int  exportsToCount = readUnsignedShort(currentOffset + 4);
        currentOffset += 6;
        std::vector<Utf8StringRef> exportsTo;
        for (int i = 0; i < exportsToCount; ++i) {
            exportsTo.push_back(readModule(currentOffset, buf));
            currentOffset += 2;
        }
        mv->visitExport(exports, exportsFlags, exportsTo);
    }

    // Read opens_count and opens
    int opensCount = readUnsignedShort(currentOffset);
    currentOffset += 2;
    while (opensCount-- > 0) {
        auto opens        = readPackage(currentOffset, buf);
        int  opensFlags   = readUnsignedShort(currentOffset + 2);
        int  opensToCount = readUnsignedShort(currentOffset + 4);
        currentOffset += 6;
        std::vector<Utf8StringRef> opensTo;
        for (int i = 0; i < opensToCount; ++i) {
            opensTo.push_back(readModule(currentOffset, buf));
            currentOffset += 2;
        }
        mv->visitOpen(opens, opensFlags, opensTo);
    }

    // Read uses_count and uses
    int usesCount = readUnsignedShort(currentOffset);
    currentOffset += 2;
    while (usesCount-- > 0) {
        mv->visitUse(readClass(currentOffset, buf));
        currentOffset += 2;
    }

    // Read provides_count and provides
    int providesCount = readUnsignedShort(currentOffset);
    currentOffset += 2;
    while (providesCount-- > 0) {
        auto provides          = readClass(currentOffset, buf);
        int  providesWithCount = readUnsignedShort(currentOffset + 2);
        currentOffset += 4;
        std::vector<Utf8StringRef> providesWith;
        for (int i = 0; i < providesWithCount; ++i) {
            providesWith.push_back(readClass(currentOffset, buf));
            currentOffset += 2;
        }
        mv->visitProvide(provides, providesWith);
    }

    mv->visitEnd();
}

// ---- readAttribute ----
Attribute* ClassReader::readAttribute(const Attribute* prototypes, const Utf8StringRef& attrName,
                                      int attrOffset, int attrLength, std::vector<char>& charBuffer,
                                      int codeAttributeOffset, Label** labels)
{
    // Check if we have a matching prototype
    if (prototypes) {
        // We need to search prototypes. But our Attribute class doesn't have a next pointer chain
        // for prototypes like ASM. We'll just create a raw attribute for now.
        // This can be extended when custom attribute support is needed.
    }
    // Default: create an opaque attribute
    if (attrOffset < 0 || attrLength < 0 || attrOffset > classFileLength_ ||
        attrLength > classFileLength_ - attrOffset) {
        throw std::invalid_argument("Invalid attribute length");
    }
    auto attr = std::make_unique<Attribute>(attrName);
    attr->value =
        Bytes::copy_from_slice(classFileBuffer_ + attrOffset, static_cast<usize>(attrLength));
    return attr.release();
}

// ---- readVerificationTypeInfo (matching ASM ClassReader.java L3385-3427) ----
int ClassReader::readVerificationTypeInfo(int                         verificationTypeInfoOffset,
                                          std::vector<ca::core::Any>& frame, int index,
                                          std::vector<char>& charBuffer, Label** labels,
                                          int labelsSize)
{
    int currentOffset = verificationTypeInfoOffset;
    int tag           = classFileBuffer_[currentOffset++] & 0xFF;
    if (index < 0 || index >= static_cast<int>(frame.size())) {
        // Index out of bounds: consume any tag-specific payload, then return.
        switch (tag) {
        case Frame::ITEM_TOP:
        case Frame::ITEM_INTEGER:
        case Frame::ITEM_FLOAT:
        case Frame::ITEM_DOUBLE:
        case Frame::ITEM_LONG:
        case Frame::ITEM_NULL:
        case Frame::ITEM_UNINITIALIZED_THIS: break;
        case Frame::ITEM_OBJECT:
        case Frame::ITEM_UNINITIALIZED: currentOffset += 2; break;
        default:
            throw std::invalid_argument("Unknown verification type tag: " + std::to_string(tag));
        }
        return currentOffset;
    }
    switch (tag) {
    case Frame::ITEM_TOP: frame[index] = ca::core::Any(Opcodes::TOP); break;
    case Frame::ITEM_INTEGER: frame[index] = ca::core::Any(Opcodes::INTEGER); break;
    case Frame::ITEM_FLOAT: frame[index] = ca::core::Any(Opcodes::FLOAT); break;
    case Frame::ITEM_DOUBLE: frame[index] = ca::core::Any(Opcodes::DOUBLE); break;
    case Frame::ITEM_LONG: frame[index] = ca::core::Any(Opcodes::LONG); break;
    case Frame::ITEM_NULL: frame[index] = ca::core::Any(Opcodes::ITEM_NULL); break;
    case Frame::ITEM_UNINITIALIZED_THIS:
        frame[index] = ca::core::Any(Opcodes::UNINITIALIZED_THIS);
        break;
    case Frame::ITEM_OBJECT:
    {
        auto cls = readClass(currentOffset, charBuffer);
        currentOffset += 2;
        frame[index] = cls;
        break;
    }
    case Frame::ITEM_UNINITIALIZED:
    {
        int offset = readUnsignedShort(currentOffset);
        currentOffset += 2;
        if (offset >= 0 && offset < labelsSize) {
            if (!labels[offset]) {
                labels[offset] = newReaderLabel();
            }
            labels[offset]->flags &= ~Label::FLAG_DEBUG_ONLY;
            frame[index] = ca::core::Any(labels[offset]);
        }
        else {
            frame[index] = ca::core::Any(Opcodes::TOP);
        }
        break;
    }
    default: throw std::invalid_argument("Unknown verification type tag: " + std::to_string(tag));
    }
    return currentOffset;
}

// ---- computeImplicitFrame ----
void ClassReader::computeImplicitFrame(Context& context)
{
    auto  methodDescriptor = context.currentMethodDescriptor;
    auto& locals           = context.currentFrameLocalTypes;
    int   numLocal         = 0;
    // 注意：locals 已按 max_locals 预填空 Any（readCode 中 assign(ml, Any{})），
    // 此处必须按索引就地写入（对齐 OW2 ASM ClassReader.computeImplicitFrame 的
    // frame.set(...) 语义）；push_back 会把入口类型追加到预填区之后，导致展开帧
    // 的 local0 为空 Any、写回时落成 TOP（原帧 roundtrip 损坏）。
    if ((context.currentMethodAccessFlags & Opcodes::ACC_STATIC) == 0) {
        if (context.currentMethodName.equals(ZUtf8StringRef::from_static("<init>"))) {
            if (numLocal < static_cast<int>(locals.size())) locals[numLocal] = Opcodes::UNINITIALIZED_THIS;
            numLocal++;
        }
        else {
            std::vector<char> buf(maxStringLength_ > 0 ? maxStringLength_ : 1);
            auto              cls = readClass(header_ + 2, buf);
            if (numLocal < static_cast<int>(locals.size())) locals[numLocal] = cls;
            numLocal++;
        }
    }
    // Parse method descriptor, one argument type at a time
    int cur = 1;   // skip '('
    while (true) {
        if (cur >= (int)methodDescriptor.byte_length()) break;
        char c = (char)methodDescriptor.byte_at(cur++);
        switch (c) {
        case 'Z':
        case 'C':
        case 'B':
        case 'S':
        case 'I':
            if (numLocal < (int)locals.size()) locals[numLocal] = Opcodes::INTEGER;
            numLocal++;
            break;
        case 'F':
            if (numLocal < (int)locals.size()) locals[numLocal] = Opcodes::FLOAT;
            numLocal++;
            break;
        case 'J':
            if (numLocal < (int)locals.size()) locals[numLocal] = Opcodes::LONG;
            numLocal++;
            break;
        case 'D':
            if (numLocal < (int)locals.size()) locals[numLocal] = Opcodes::DOUBLE;
            numLocal++;
            break;
        case '[':
        {
            int start = cur - 1;
            while (cur < (int)methodDescriptor.byte_length()) {
                char c2 = (char)methodDescriptor.byte_at(cur);
                if (c2 == '[') {
                    cur++;
                    continue;
                }
                if (c2 == 'L') {
                    while (cur < (int)methodDescriptor.byte_length() &&
                           methodDescriptor.byte_at(cur) != ';')
                        cur++;
                    cur++;
                }
                break;
            }
            if (numLocal < static_cast<int>(locals.size()))
                locals[numLocal] = Utf8StringRef(methodDescriptor.data() + start,
                                                 static_cast<usize>(cur - start),
                                                 static_cast<usize>(cur - start));
            numLocal++;
            break;
        }
        case 'L':
        {
            int start = cur;
            while (cur < (int)methodDescriptor.byte_length() &&
                   methodDescriptor.byte_at(cur) != ';')
                cur++;
            if (numLocal < static_cast<int>(locals.size()))
                locals[numLocal] = Utf8StringRef(methodDescriptor.data() + start,
                                                 static_cast<usize>(cur - start),
                                                 static_cast<usize>(cur - start));
            cur++;   // skip ';'
            numLocal++;
            break;
        }
        default:
            finishImplicitFrameLocals(context, numLocal);
            return;
        }
    }
    finishImplicitFrameLocals(context, numLocal);
}

/// 帧自洽保证（收口两处出口）：对抗性 classfile 可能声明小于实际实参槽位数的
/// maxLocals，此时计数会超过按 maxLocals 预填的缓冲。把缓冲扩到计数、缺省填 TOP，
/// 保证 visitFrame 的 (numLocal, local.size()) 自洽——否则 MethodWriter 展开帧时
/// 按 numLocal 越界读 local（debug 必崩，release 静默写出垃圾 StackMapTable）。
void ClassReader::finishImplicitFrameLocals(Context& context, int numLocal)
{
    if (static_cast<int>(context.currentFrameLocalTypes.size()) < numLocal)
        context.currentFrameLocalTypes.resize(numLocal, ca::core::Any(Opcodes::TOP));
    context.currentFrameLocalCount = numLocal;
}

// ---- readStackMapFrameType ----
int ClassReader::readStackMapFrameType(MethodVisitor* mv, Context& context, int frameOffset)
{
    (void)mv;
    return readStackMapFrame(frameOffset, true, false, context);
}

// ---- readStackMapFrame (matching ASM ClassReader.java L3286-3369) ----
int ClassReader::readStackMapFrame(int stackMapFrameOffset, bool compressed, bool expand,
                                   Context& context)
{
    int   currentOffset = stackMapFrameOffset;
    auto& charBuffer    = context.charBuffer;
    auto* labels        = context.currentMethodLabels.data();
    int   labelsSize    = static_cast<int>(context.currentMethodLabels.size());
    int   frameType;
    if (compressed) {
        frameType = classFileBuffer_[currentOffset++] & 0xFF;
    }
    else {
        frameType                  = FULL_FRAME;
        context.currentFrameOffset = -1;
    }
    int offsetDelta;
    context.currentFrameLocalCountDelta = 0;
    if (frameType < SAME_LOCALS_1_STACK_ITEM_FRAME) {
        offsetDelta                    = frameType;
        context.currentFrameType       = Opcodes::F_SAME;
        context.currentFrameStackCount = 0;
    }
    else if (frameType < RESERVED) {
        offsetDelta = frameType - SAME_LOCALS_1_STACK_ITEM_FRAME;
        if (context.currentFrameStackTypes.empty()) context.currentFrameStackTypes.resize(1);
        currentOffset = readVerificationTypeInfo(
            currentOffset, context.currentFrameStackTypes, 0, charBuffer, labels, labelsSize);
        context.currentFrameType       = Opcodes::F_SAME1;
        context.currentFrameStackCount = 1;
    }
    else if (frameType >= SAME_LOCALS_1_STACK_ITEM_FRAME_EXTENDED) {
        offsetDelta = readUnsignedShort(currentOffset);
        currentOffset += 2;
        if (frameType == SAME_LOCALS_1_STACK_ITEM_FRAME_EXTENDED) {
            if (context.currentFrameStackTypes.empty()) context.currentFrameStackTypes.resize(1);
            currentOffset = readVerificationTypeInfo(
                currentOffset, context.currentFrameStackTypes, 0, charBuffer, labels, labelsSize);
            context.currentFrameType       = Opcodes::F_SAME1;
            context.currentFrameStackCount = 1;
        }
        else if (frameType >= CHOP_FRAME && frameType < SAME_FRAME_EXTENDED) {
            context.currentFrameType            = Opcodes::F_CHOP;
            context.currentFrameLocalCountDelta = SAME_FRAME_EXTENDED - frameType;
            if (context.currentFrameLocalCount >= context.currentFrameLocalCountDelta)
                context.currentFrameLocalCount -= context.currentFrameLocalCountDelta;
            else
                context.currentFrameLocalCount = 0;
            context.currentFrameStackCount = 0;
        }
        else if (frameType == SAME_FRAME_EXTENDED) {
            context.currentFrameType       = Opcodes::F_SAME;
            context.currentFrameStackCount = 0;
        }
        else if (frameType < FULL_FRAME) {
            int local       = expand ? context.currentFrameLocalCount : 0;
            int numAppended = frameType - SAME_FRAME_EXTENDED;
            if (static_cast<int>(context.currentFrameLocalTypes.size()) < local + numAppended)
                context.currentFrameLocalTypes.resize(local + numAppended);
            for (int k = numAppended; k > 0; k--) {
                currentOffset = readVerificationTypeInfo(currentOffset,
                                                         context.currentFrameLocalTypes,
                                                         local++,
                                                         charBuffer,
                                                         labels,
                                                         labelsSize);
            }
            context.currentFrameType            = Opcodes::F_APPEND;
            context.currentFrameLocalCountDelta = numAppended;
            context.currentFrameLocalCount += context.currentFrameLocalCountDelta;
            context.currentFrameStackCount = 0;
        }
        else {
            int numberOfLocals = readUnsignedShort(currentOffset);
            currentOffset += 2;
            if (numberOfLocals > 65535) {
                context.currentFrameLocalCount = 0;
                context.currentFrameStackCount = 0;
                return currentOffset;
            }
            context.currentFrameType            = Opcodes::F_FULL;
            context.currentFrameLocalCountDelta = numberOfLocals;
            context.currentFrameLocalCount      = numberOfLocals;
            if (static_cast<int>(context.currentFrameLocalTypes.size()) < numberOfLocals)
                context.currentFrameLocalTypes.resize(numberOfLocals);
            for (int local = 0; local < numberOfLocals; ++local) {
                currentOffset = readVerificationTypeInfo(currentOffset,
                                                         context.currentFrameLocalTypes,
                                                         local,
                                                         charBuffer,
                                                         labels,
                                                         labelsSize);
            }
            int numberOfStackItems = readUnsignedShort(currentOffset);
            currentOffset += 2;
            if (numberOfStackItems > 65535) {
                context.currentFrameStackCount = 0;
                return currentOffset;
            }
            context.currentFrameStackCount = numberOfStackItems;
            if (static_cast<int>(context.currentFrameStackTypes.size()) < numberOfStackItems)
                context.currentFrameStackTypes.resize(numberOfStackItems);
            for (int stack = 0; stack < numberOfStackItems; ++stack) {
                currentOffset = readVerificationTypeInfo(currentOffset,
                                                         context.currentFrameStackTypes,
                                                         stack,
                                                         charBuffer,
                                                         labels,
                                                         labelsSize);
            }
        }
    }
    else {
        throw std::invalid_argument("Unknown stack map frame type");
    }
    context.currentFrameOffset += offsetDelta + 1;
    int labelOffset = context.currentFrameOffset;
    if (labelOffset < (int)context.currentMethodLabels.size() &&
        !context.currentMethodLabels[labelOffset]) {
        context.currentMethodLabels[labelOffset] = newReaderLabel();
    }
    if (labelOffset < (int)context.currentMethodLabels.size()) {
        context.currentMethodLabels[labelOffset]->flags &= ~Label::FLAG_DEBUG_ONLY;
    }
    return currentOffset;
}

// ---- Attribute default implementations ----
std::unique_ptr<Attribute> Attribute::read(ClassReader& classReader, int offset, int length,
                                           std::vector<char>&, int, Label**)
{
    auto  attr  = std::make_unique<Attribute>();
    auto* buf   = classReader.getClassFileBuffer();
    attr->value = Bytes::copy_from_slice(buf + offset, static_cast<usize>(length));
    return attr;
}

ByteVector Attribute::write(ClassWriter&, const std::vector<u8>&, int, int, int)
{
    ByteVector out(static_cast<i32>(value.len()));
    for (usize i = 0; i < value.len(); i++) out.putByte(value.as_ptr()[i]);
    return out;
}
}   // namespace jbytecode::core
