#pragma once

#include <fstream>
#include <functional>
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include "libca/core/any.hpp"
#include "test_helper.hpp"
#include "libca/str/utf8_string.hpp"
using namespace ca::str;
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/visitors.hpp"


///  Read a test resource. Names starting with "origin/" resolve to core's
///  test_resource/ (ASM upstream test resources); otherwise resolve to the
///  asm-test precompiled samples directory (with fallback to
///  test_resource/origin/ for files shared from ASM). Returns empty vector if
/// not found.
inline std::vector<uint8_t> readClassFile(const std::string& name)
{
    if (name.rfind("origin/", 0) == 0) {
        if (!test::hasResource(name)) return {};
        return test::getResource(name);
    }
    if (test::hasAsmTestResource(name)) {
        return test::getAsmTestResource(name);
    }
    const std::string fallback = "test_resource/origin/" + name;
    if (test::hasAsmTestResource(fallback)) {
        return test::getAsmTestResource(fallback);
    }
    return {};
}

///  Extract magic number (0xCAFEBABE) from raw class data.
/// Returns 0 if the vector has fewer than 4 bytes.
inline uint32_t readMagic(const std::vector<uint8_t>& d)
{
    if (d.size() < 4) return 0;
    return (static_cast<uint32_t>(d[0]) << 24) | (static_cast<uint32_t>(d[1]) << 16) |
           (static_cast<uint32_t>(d[2]) << 8) | static_cast<uint32_t>(d[3]);
}

struct PrecompiledClassParam
{
    std::string name;
    std::string resource;

    std::string internalName() const
    {
        if (name.size() >= std::strlen("module-info") &&
            name.substr(name.size() - std::strlen("module-info")) == "module-info") {
            return "module-info";
        }
        std::string result = name;
        for (char& c : result) {
            if (c == '.') c = '/';
        }
        return result;
    }

    std::vector<uint8_t> bytes() const { return readClassFile(resource); }

    bool hasJsrOrRetInstructions() const
    {
        return name == "jdk3.AllInstructions" || name == "jdk3.LargeMethod";
    }
};

inline const std::vector<PrecompiledClassParam>& availablePrecompiledClasses()
{
    static const std::vector<PrecompiledClassParam> classes = {
        {"DefaultPackage", "DefaultPackage.class"},
        {"jdk3.AllInstructions", "jdk3/AllInstructions.class"},
        {"jdk3.AllStructures", "jdk3/AllStructures.class"},
        {"jdk3.AllStructures$1", "jdk3/AllStructures$1.class"},
        {"jdk3.ArtificialStructures", "jdk3/ArtificialStructures.class"},
        {"jdk3.AllStructures$InnerClass", "jdk3/AllStructures$InnerClass.class"},
        {"jdk3.LargeMethod", "jdk3/LargeMethod.class"},
        {"jdk3.SubOptimalMaxStackAndLocals", "jdk3/SubOptimalMaxStackAndLocals.class"},
        {"jdk5.AllInstructions", "jdk5/AllInstructions.class"},
        {"jdk5.AllStructures", "jdk5/AllStructures.class"},
        {"jdk5.AllStructures$InvisibleAnnotation", "jdk5/AllStructures$InvisibleAnnotation.class"},
        {"jdk5.AllStructures$EnumClass", "jdk5/AllStructures$EnumClass.class"},
        {"jdk5.AllStructures$1LocalClass", "jdk5/AllStructures$1LocalClass.class"},
        {"jdk8.AllFrames", "jdk8/AllFrames.class"},
        {"jdk8.AllInstructions", "jdk8/AllInstructions.class"},
        {"jdk8.AllStructures", "jdk8/AllStructures.class"},
        {"jdk8.AllLambdas", "jdk8/AllLambdas.class"},
        {"jdk8.AllStructures$1", "jdk8/AllStructures$1.class"},
        {"jdk8.Artificial$()$Structures", "jdk8/Artificial$()$Structures.class"},
        {"jdk8.AllStructures$InnerClass", "jdk8/AllStructures$InnerClass.class"},
        {"jdk8.LargeMethod", "jdk8/LargeMethod.class"},
        {"jdk9.module-info", "jdk9/module-info.class"},
        {"jdk11.AllInstructions", "jdk11/AllInstructions.class"},
        {"jdk11.AllStructures", "jdk11/AllStructures.class"},
        {"jdk11.AllStructures$Nested", "jdk11/AllStructures$Nested.class"},
        {"jdk14.AllStructures$RecordSubType", "jdk14/AllStructures$RecordSubType.class"},
        {"jdk14.AllStructures$EmptyRecord", "jdk14/AllStructures$EmptyRecord.class"},
        {"jdk15.AllStructures", "jdk15/AllStructures.class"},
    };
    return classes;
}

namespace asm_test_detail {

struct ClassParser
{
    const std::vector<uint8_t>& data;
    size_t                      offset = 0;

    uint8_t u1()
    {
        if (offset >= data.size()) throw std::out_of_range("Unexpected end of class file");
        return data[offset++];
    }

    uint16_t u2()
    {
        uint16_t value = (static_cast<uint16_t>(u1()) << 8);
        value |= u1();
        return value;
    }

    uint32_t u4()
    {
        uint32_t value = (static_cast<uint32_t>(u1()) << 24);
        value |= static_cast<uint32_t>(u1()) << 16;
        value |= static_cast<uint32_t>(u1()) << 8;
        value |= u1();
        return value;
    }

    void skip(size_t count)
    {
        if (offset + count > data.size()) throw std::out_of_range("Unexpected end of class file");
        offset += count;
    }
};

struct CpEntry
{
    int         tag = 0;
    std::string utf8;
    int         a    = 0;
    int         b    = 0;
    int         c    = 0;
    int         size = 1;
};

inline std::string cpUtf8(const std::vector<CpEntry>& cp, int index)
{
    if (index <= 0 || index >= static_cast<int>(cp.size())) return {};
    return cp[index].utf8;
}

inline std::string cpDump(const std::vector<CpEntry>& cp, int index);

inline std::string cpClassName(const std::vector<CpEntry>& cp, int index)
{
    if (index <= 0 || index >= static_cast<int>(cp.size())) return {};
    return cpUtf8(cp, cp[index].a);
}

inline std::string cpNameAndType(const std::vector<CpEntry>& cp, int index)
{
    if (index <= 0 || index >= static_cast<int>(cp.size())) return {};
    return cpUtf8(cp, cp[index].a) + cpUtf8(cp, cp[index].b);
}

inline std::string cpRefDump(const std::vector<CpEntry>& cp, int index, const char* kind)
{
    if (index <= 0 || index >= static_cast<int>(cp.size())) return {};
    return std::string(kind) + " " + cpClassName(cp, cp[index].a) + "." +
           cpNameAndType(cp, cp[index].b);
}

inline std::string cpDump(const std::vector<CpEntry>& cp, int index)
{
    if (index <= 0 || index >= static_cast<int>(cp.size())) return {};
    const CpEntry& entry = cp[index];
    switch (entry.tag) {
    case 1: return entry.utf8;
    case 3: return std::to_string(entry.a);
    case 4: return std::to_string(entry.a);
    case 5:
        return std::to_string((static_cast<int64_t>(entry.a) << 32) |
                              static_cast<uint32_t>(entry.b));
    case 6:
        return std::to_string((static_cast<int64_t>(entry.a) << 32) |
                              static_cast<uint32_t>(entry.b));
    case 7: return "ConstantClassInfo " + cpUtf8(cp, entry.a);
    case 8: return "ConstantStringInfo " + cpUtf8(cp, entry.a);
    case 9: return cpRefDump(cp, index, "ConstantFieldRefInfo");
    case 10: return cpRefDump(cp, index, "ConstantMethodRefInfo");
    case 11: return cpRefDump(cp, index, "ConstantInterfaceMethodRefInfo");
    case 12: return "ConstantNameAndTypeInfo " + cpNameAndType(cp, index);
    case 15:
    {
        const char* refKind = "";
        if (entry.a <= 4)
            refKind = "ConstantFieldRefInfo";
        else if (entry.a == 9)
            refKind = "ConstantInterfaceMethodRefInfo";
        else
            refKind = "ConstantMethodRefInfo";
        return "ConstantMethodHandleInfo " + std::to_string(entry.a) + "." +
               cpRefDump(cp, entry.b, refKind);
    }
    case 16: return "ConstantMethodTypeInfo " + cpUtf8(cp, entry.a);
    case 17:
        return "ConstantDynamicInfo " + std::to_string(entry.a) + "." + cpNameAndType(cp, entry.b);
    case 18:
        return "ConstantInvokeDynamicInfo " + std::to_string(entry.a) + "." +
               cpNameAndType(cp, entry.b);
    case 19: return "ConstantModuleInfo " + cpUtf8(cp, entry.a);
    case 20: return "ConstantPackageInfo " + cpUtf8(cp, entry.a);
    default: return "Unknown";
    }
}

inline std::vector<CpEntry> parseConstantPool(ClassParser& parser)
{
    parser.u4();
    parser.u2();
    parser.u2();
    int                  constantPoolCount = parser.u2();
    std::vector<CpEntry> cp(static_cast<size_t>(constantPoolCount));
    for (int i = 1; i < constantPoolCount; ++i) {
        CpEntry entry;
        entry.tag = parser.u1();
        switch (entry.tag) {
        case 1:
        {
            int         length = parser.u2();
            std::string value;
            value.reserve(static_cast<size_t>(length));
            for (int j = 0; j < length; ++j) value.push_back(static_cast<char>(parser.u1()));
            entry.utf8 = value;
            break;
        }
        case 3:
        case 4: entry.a = static_cast<int>(parser.u4()); break;
        case 5:
        case 6:
            entry.a    = static_cast<int>(parser.u4());
            entry.b    = static_cast<int>(parser.u4());
            entry.size = 2;
            break;
        case 7:
        case 8:
        case 16:
        case 19:
        case 20: entry.a = parser.u2(); break;
        case 9:
        case 10:
        case 11:
        case 12:
        case 17:
        case 18:
            entry.a = parser.u2();
            entry.b = parser.u2();
            break;
        case 15:
            entry.a = parser.u1();
            entry.b = parser.u2();
            break;
        default:
            throw std::invalid_argument("Invalid constant pool item tag " +
                                        std::to_string(entry.tag));
        }
        cp[i] = entry;
        if (entry.size == 2) ++i;
    }
    return cp;
}

inline void skipAttributes(ClassParser& parser, const std::vector<CpEntry>& cp,
                           std::ostringstream& dump)
{
    int attributesCount = parser.u2();
    dump << "attributes_count: " << attributesCount << "\n";
    std::vector<std::string> attributeNames;
    attributeNames.reserve(static_cast<size_t>(attributesCount));
    for (int i = 0; i < attributesCount; ++i) {
        int         nameIndex = parser.u2();
        std::string name      = cpUtf8(cp, nameIndex);
        uint32_t    length    = parser.u4();
        attributeNames.push_back(name);
        parser.skip(length);
    }
    std::sort(attributeNames.begin(), attributeNames.end());
    for (const auto& name : attributeNames) {
        dump << "attribute_name_index: " << name << "\n";
    }
}

inline std::string structuralDump(const std::vector<uint8_t>& bytes, bool constantPoolOnly)
{
    ClassParser          parser{bytes};
    std::vector<CpEntry> cp = parseConstantPool(parser);
    std::ostringstream   dump;
    if (constantPoolOnly) {
        for (int i = 1; i < static_cast<int>(cp.size()); ++i) {
            if (cp[i].tag == 0) continue;
            dump << "constant_pool: " << cpDump(cp, i) << "\n";
            if (cp[i].size == 2) ++i;
        }
        ClassParser attrParser{bytes};
        parseConstantPool(attrParser);
        attrParser.u2();
        attrParser.u2();
        attrParser.u2();
        int interfacesCount = attrParser.u2();
        attrParser.skip(static_cast<size_t>(interfacesCount) * 2);
        int fieldsCount = attrParser.u2();
        for (int i = 0; i < fieldsCount; ++i) {
            attrParser.skip(6);
            int attributesCount = attrParser.u2();
            for (int j = 0; j < attributesCount; ++j) {
                int         nameIndex = attrParser.u2();
                std::string name      = cpUtf8(cp, nameIndex);
                uint32_t    length    = attrParser.u4();
                if (name == "BootstrapMethods") dump << "constant_pool: BootstrapMethods\n";
                attrParser.skip(length);
            }
        }
        int methodsCount = attrParser.u2();
        for (int i = 0; i < methodsCount; ++i) {
            attrParser.skip(6);
            int attributesCount = attrParser.u2();
            for (int j = 0; j < attributesCount; ++j) {
                int         nameIndex = attrParser.u2();
                std::string name      = cpUtf8(cp, nameIndex);
                uint32_t    length    = attrParser.u4();
                if (name == "BootstrapMethods") dump << "constant_pool: BootstrapMethods\n";
                attrParser.skip(length);
            }
        }
        int attributesCount = attrParser.u2();
        for (int j = 0; j < attributesCount; ++j) {
            int         nameIndex = attrParser.u2();
            std::string name      = cpUtf8(cp, nameIndex);
            uint32_t    length    = attrParser.u4();
            if (name == "BootstrapMethods") dump << "constant_pool: BootstrapMethods\n";
            attrParser.skip(length);
        }
        return dump.str();
    }
    dump << "access_flags: " << parser.u2() << "\n";
    dump << "this_class: " << cpDump(cp, parser.u2()) << "\n";
    dump << "super_class: " << cpDump(cp, parser.u2()) << "\n";
    int interfacesCount = parser.u2();
    dump << "interfaces_count: " << interfacesCount << "\n";
    parser.skip(static_cast<size_t>(interfacesCount) * 2);
    int fieldsCount = parser.u2();
    dump << "fields_count: " << fieldsCount << "\n";
    for (int i = 0; i < fieldsCount; ++i) {
        dump << "access_flags: " << parser.u2() << "\n";
        dump << "name_index: " << cpUtf8(cp, parser.u2()) << "\n";
        dump << "descriptor_index: " << cpUtf8(cp, parser.u2()) << "\n";
        skipAttributes(parser, cp, dump);
    }
    int methodsCount = parser.u2();
    dump << "methods_count: " << methodsCount << "\n";
    for (int i = 0; i < methodsCount; ++i) {
        dump << "access_flags: " << parser.u2() << "\n";
        dump << "name_index: " << cpUtf8(cp, parser.u2()) << "\n";
        dump << "descriptor_index: " << cpUtf8(cp, parser.u2()) << "\n";
        skipAttributes(parser, cp, dump);
    }
    skipAttributes(parser, cp, dump);
    return dump.str();
}

}   // namespace asm_test_detail

inline std::string getConstantPoolDump(const std::vector<uint8_t>& classFile)
{
    return asm_test_detail::structuralDump(classFile, true);
}

inline std::string getClassFileDump(const std::vector<uint8_t>& classFile)
{
    return asm_test_detail::structuralDump(classFile, false);
}

inline bool classFilesEquivalent(const std::vector<uint8_t>& expected,
                                 const std::vector<uint8_t>& actual)
{
    return getClassFileDump(expected) == getClassFileDump(actual);
}

struct SignatureCollector : jbytecode::core::ClassVisitor
{
    std::vector<std::string> classSignatures;
    std::vector<std::string> fieldSignatures;
    std::vector<std::string> methodSignatures;

    SignatureCollector()
        : ClassVisitor(jbytecode::core::Opcodes::ASM9)
    {}

    void visit(int version, int access, ca::str::Utf8StringRef name,
               ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override
    {
        (void)version;
        (void)access;
        (void)name;
        (void)superName;
        (void)interfaces;
        if (signature.data() != nullptr && signature.byte_length() > 0) {
            classSignatures.push_back(signature.to_std_string());
        }
    }

    jbytecode::core::FieldVisitor* visitField(int access, ca::str::Utf8StringRef name,
                                              ca::str::Utf8StringRef descriptor,
                                              ca::str::Utf8StringRef signature,
                                              const ca::core::Any&   value) override
    {
        (void)access;
        (void)name;
        (void)descriptor;
        (void)value;
        if (signature.data() != nullptr && signature.byte_length() > 0) {
            fieldSignatures.push_back(signature.to_std_string());
        }
        return nullptr;
    }

    jbytecode::core::MethodVisitor* visitMethod(
        int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
        ca::str::Utf8StringRef                     signature,
        const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        (void)access;
        (void)name;
        (void)descriptor;
        (void)exceptions;
        if (signature.data() != nullptr && signature.byte_length() > 0) {
            methodSignatures.push_back(signature.to_std_string());
        }
        return nullptr;
    }
};

inline void appendDeepSignatures(std::vector<std::string>& signatures)
{
    std::function<std::string(int)> build = [&](int depth) {
        if (depth == 0) return std::string("LGeneric;");
        return std::string("LGeneric<LOpen;") + build(depth - 1) + "LClose;>;";
    };
    for (int depth = 0; depth < 48; ++depth) {
        signatures.push_back(build(depth));
    }
}

inline SignatureCollector collectAvailableSignatures()
{
    SignatureCollector       all;
    ca::str::Utf8StringArena arena;
    for (const auto& clazz : availablePrecompiledClasses()) {
        auto bytes = clazz.bytes();
        if (bytes.empty()) continue;
        SignatureCollector current;
        jbytecode::core::ClassReader(arena, bytes).accept(&current, 0);
        all.classSignatures.insert(all.classSignatures.end(),
                                   current.classSignatures.begin(),
                                   current.classSignatures.end());
        all.fieldSignatures.insert(all.fieldSignatures.end(),
                                   current.fieldSignatures.begin(),
                                   current.fieldSignatures.end());
        all.methodSignatures.insert(all.methodSignatures.end(),
                                    current.methodSignatures.begin(),
                                    current.methodSignatures.end());
    }
    appendDeepSignatures(all.fieldSignatures);
    return all;
}

/// Non-standard class-file attribute fixture (mirrors ASM Comment.java).
class CommentAttribute : public jbytecode::core::Attribute
{
public:
    CommentAttribute()
        : Attribute(ca::str::ZUtf8StringRef::from_static("Comment"))
    {}

    bool isUnknown() const override { return false; }

    std::unique_ptr<Attribute> read(jbytecode::core::ClassReader& classReader, int offset,
                                    int length, std::vector<char>& charBuffer,
                                    int                      codeAttributeOffset,
                                    jbytecode::core::Label** labels) override
    {
        (void)classReader;
        (void)offset;
        (void)length;
        (void)charBuffer;
        (void)codeAttributeOffset;
        (void)labels;
        return std::make_unique<CommentAttribute>();
    }

    jbytecode::core::ByteVector write(jbytecode::core::ClassWriter& classWriter,
                                      const std::vector<ca::u8>& code, int codeLength, int maxStack,
                                      int maxLocals) override
    {
        (void)classWriter;
        (void)code;
        (void)codeLength;
        (void)maxStack;
        (void)maxLocals;
        return jbytecode::core::ByteVector();
    }
};

/// Non-standard code attribute fixture (mirrors ASM CodeComment.java).
class CodeCommentAttribute : public jbytecode::core::Attribute
{
public:
    CodeCommentAttribute()
        : Attribute(ca::str::ZUtf8StringRef::from_static("CodeComment"))
    {}

    bool isUnknown() const override { return false; }

    bool isCodeAttribute() const override { return true; }

    std::unique_ptr<Attribute> read(jbytecode::core::ClassReader& classReader, int offset,
                                    int length, std::vector<char>& charBuffer,
                                    int                      codeAttributeOffset,
                                    jbytecode::core::Label** labels) override
    {
        (void)classReader;
        (void)offset;
        (void)length;
        (void)charBuffer;
        (void)codeAttributeOffset;
        (void)labels;
        return std::make_unique<CodeCommentAttribute>();
    }

    jbytecode::core::ByteVector write(jbytecode::core::ClassWriter& classWriter,
                                      const std::vector<ca::u8>& code, int codeLength, int maxStack,
                                      int maxLocals) override
    {
        (void)classWriter;
        (void)code;
        (void)codeLength;
        (void)maxStack;
        (void)maxLocals;
        return jbytecode::core::ByteVector();
    }
};
