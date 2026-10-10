#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/type_reference.hpp"
#include "jbytecode/core/writers.hpp"

using namespace jbytecode::core;

namespace {

ca::str::ZUtf8StringRef ref(const char* value)
{
    return ca::str::ZUtf8StringRef::from_static(value);
}

std::string text(ca::str::Utf8StringRef value)
{
    return value.data() == nullptr ? std::string{} : value.to_std_string();
}

struct AnnotationInfo
{
    std::string descriptor;
    bool        visible = false;
    int         typeRef = -1;
};

struct AttributeInfo
{
    std::string         type;
    std::vector<ca::u8> value;
};

AttributeInfo copyAttribute(const Attribute& attribute)
{
    AttributeInfo result;
    result.type = attribute.type.to_std_string();
    if (!attribute.value.is_empty()) {
        result.value.assign(attribute.value.as_ptr(),
                            attribute.value.as_ptr() + attribute.value.len());
    }
    return result;
}

struct FieldInfo
{
    int                         access = 0;
    std::string                 name;
    std::string                 descriptor;
    std::string                 signature;
    ca::core::Any               value;
    std::vector<AnnotationInfo> annotations;
    std::vector<AttributeInfo>  attributes;
};

class FieldRecorder final : public FieldVisitor
{
public:
    explicit FieldRecorder(FieldInfo& info)
        : FieldVisitor(Opcodes::ASM9)
        , info_(info)
        , annotation_(Opcodes::ASM9)
    {}

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible) override
    {
        info_.annotations.push_back({text(descriptor), visible, -1});
        return &annotation_;
    }

    AnnotationVisitor* visitTypeAnnotation(int                    typeRef, const TypePath*,
                                           ca::str::Utf8StringRef descriptor, bool visible) override
    {
        info_.annotations.push_back({text(descriptor), visible, typeRef});
        return &annotation_;
    }

    void visitAttribute(const Attribute& attribute) override
    {
        info_.attributes.push_back(copyAttribute(attribute));
    }

private:
    FieldInfo&        info_;
    AnnotationVisitor annotation_;
};

struct MethodInfo
{
    int                         access = 0;
    std::string                 name;
    std::string                 descriptor;
    std::string                 signature;
    std::vector<std::string>    exceptions;
    std::vector<std::string>    parameters;
    std::vector<AnnotationInfo> annotations;
    std::vector<AttributeInfo>  attributes;
    std::vector<int>            instructions;
    std::vector<int>            lines;
    std::vector<std::string>    localVariables;
    ca::core::Any               annotationDefault;
    bool                        codeVisited   = false;
    int                         tryCatchCount = 0;
    int                         maxStack      = -1;
    int                         maxLocals     = -1;
};

class DefaultValueRecorder final : public AnnotationVisitor
{
public:
    explicit DefaultValueRecorder(MethodInfo& info)
        : AnnotationVisitor(Opcodes::ASM9)
        , info_(info)
    {}

    void visit(ca::str::Utf8StringRef, const ca::core::Any& value) override
    {
        info_.annotationDefault = value;
    }

private:
    MethodInfo& info_;
};

class MethodRecorder final : public MethodVisitor
{
public:
    explicit MethodRecorder(MethodInfo& info)
        : MethodVisitor(Opcodes::ASM9)
        , info_(info)
        , annotation_(Opcodes::ASM9)
        , defaultValue_(info)
    {}

    void visitParameter(ca::str::Utf8StringRef name, int) override
    {
        info_.parameters.push_back(text(name));
    }

    AnnotationVisitor* visitAnnotationDefault() override { return &defaultValue_; }

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible) override
    {
        info_.annotations.push_back({text(descriptor), visible, -1});
        return &annotation_;
    }

    AnnotationVisitor* visitTypeAnnotation(int                    typeRef, const TypePath*,
                                           ca::str::Utf8StringRef descriptor, bool visible) override
    {
        info_.annotations.push_back({text(descriptor), visible, typeRef});
        return &annotation_;
    }

    AnnotationVisitor* visitParameterAnnotation(int parameter, ca::str::Utf8StringRef descriptor,
                                                bool visible) override
    {
        info_.annotations.push_back(
            {text(descriptor) + "@" + std::to_string(parameter), visible, -1});
        return &annotation_;
    }

    void visitAttribute(const Attribute& attribute) override
    {
        info_.attributes.push_back(copyAttribute(attribute));
    }

    void visitCode() override { info_.codeVisited = true; }
    void visitInsn(int opcode) override { info_.instructions.push_back(opcode); }
    void visitTryCatchBlock(Label*, Label*, Label*, ca::str::Utf8StringRef) override
    {
        ++info_.tryCatchCount;
    }
    void visitLineNumber(int line, Label*) override { info_.lines.push_back(line); }
    void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef,
                            ca::str::Utf8StringRef, Label*, Label*, int) override
    {
        info_.localVariables.push_back(text(name));
    }
    void visitMaxs(int maxStack, int maxLocals) override
    {
        info_.maxStack  = maxStack;
        info_.maxLocals = maxLocals;
    }

private:
    MethodInfo&          info_;
    AnnotationVisitor    annotation_;
    DefaultValueRecorder defaultValue_;
};

struct InnerClassInfo
{
    std::string name;
    std::string outerName;
    std::string innerName;
    int         access = 0;
};

class ClassRecorder final : public ClassVisitor
{
public:
    ClassRecorder()
        : ClassVisitor(Opcodes::ASM9)
        , annotation_(Opcodes::ASM9)
    {}

    void visit(int classVersion, int classAccess, ca::str::Utf8StringRef className,
               ca::str::Utf8StringRef classSignature, ca::str::Utf8StringRef classSuperName,
               const std::vector<ca::str::Utf8StringRef>& classInterfaces) override
    {
        version   = classVersion;
        access    = classAccess;
        name      = text(className);
        signature = text(classSignature);
        superName = text(classSuperName);
        for (const auto& interfaceName : classInterfaces) interfaces.push_back(text(interfaceName));
    }

    void visitSource(ca::str::Utf8StringRef file, ca::str::Utf8StringRef debug) override
    {
        sourceFile  = text(file);
        sourceDebug = text(debug);
    }

    void visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef methodName,
                         ca::str::Utf8StringRef methodDescriptor) override
    {
        outerOwner      = text(owner);
        outerMethodName = text(methodName);
        outerMethodDesc = text(methodDescriptor);
    }

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible) override
    {
        annotations.push_back({text(descriptor), visible, -1});
        return &annotation_;
    }

    AnnotationVisitor* visitTypeAnnotation(int                    typeRef, const TypePath*,
                                           ca::str::Utf8StringRef descriptor, bool visible) override
    {
        annotations.push_back({text(descriptor), visible, typeRef});
        return &annotation_;
    }

    void visitAttribute(const Attribute& attribute) override
    {
        attributes.push_back(copyAttribute(attribute));
    }

    void visitInnerClass(ca::str::Utf8StringRef className, ca::str::Utf8StringRef outer,
                         ca::str::Utf8StringRef inner, int classAccess) override
    {
        innerClasses.push_back({text(className), text(outer), text(inner), classAccess});
    }

    void visitNestHost(ca::str::Utf8StringRef host) override { nestHost = text(host); }
    void visitNestMember(ca::str::Utf8StringRef member) override
    {
        nestMembers.push_back(text(member));
    }
    void visitPermittedSubclass(ca::str::Utf8StringRef subclass) override
    {
        permittedSubclasses.push_back(text(subclass));
    }

    FieldVisitor* visitField(int fieldAccess, ca::str::Utf8StringRef fieldName,
                             ca::str::Utf8StringRef fieldDescriptor,
                             ca::str::Utf8StringRef fieldSignature,
                             const ca::core::Any&   fieldValue) override
    {
        fields.push_back({fieldAccess,
                          text(fieldName),
                          text(fieldDescriptor),
                          text(fieldSignature),
                          fieldValue,
                          {},
                          {}});
        fieldRecorders.push_back(std::make_unique<FieldRecorder>(fields.back()));
        return fieldRecorders.back().get();
    }

    MethodVisitor* visitMethod(int methodAccess, ca::str::Utf8StringRef methodName,
                               ca::str::Utf8StringRef                     methodDescriptor,
                               ca::str::Utf8StringRef                     methodSignature,
                               const std::vector<ca::str::Utf8StringRef>& methodExceptions) override
    {
        MethodInfo info;
        info.access     = methodAccess;
        info.name       = text(methodName);
        info.descriptor = text(methodDescriptor);
        info.signature  = text(methodSignature);
        for (const auto& exception : methodExceptions) info.exceptions.push_back(text(exception));
        methods.push_back(std::move(info));
        methodRecorders.push_back(std::make_unique<MethodRecorder>(methods.back()));
        return methodRecorders.back().get();
    }

    int                         version = 0;
    int                         access  = 0;
    std::string                 name;
    std::string                 signature;
    std::string                 superName;
    std::vector<std::string>    interfaces;
    std::string                 sourceFile;
    std::string                 sourceDebug;
    std::string                 outerOwner;
    std::string                 outerMethodName;
    std::string                 outerMethodDesc;
    std::string                 nestHost;
    std::vector<std::string>    nestMembers;
    std::vector<std::string>    permittedSubclasses;
    std::vector<InnerClassInfo> innerClasses;
    std::vector<AnnotationInfo> annotations;
    std::vector<AttributeInfo>  attributes;
    std::vector<FieldInfo>      fields;
    std::vector<MethodInfo>     methods;

private:
    // ClassReader 会在 visitField/visitMethod 返回后继续调用对应 visitor，因此这里持有
    // recorder 的所有权，保证整轮 accept 期间返回的裸指针始终有效。FieldInfo/MethodInfo
    // 已经保存在成员 vector 中；单个字段或方法的全部回调结束后，reader 才会访问下一项。
    AnnotationVisitor                            annotation_;
    std::vector<std::unique_ptr<FieldRecorder>>  fieldRecorders;
    std::vector<std::unique_ptr<MethodRecorder>> methodRecorders;
};

class ClassWriterDirectContractTest : public ::testing::Test
{
protected:
    std::vector<ca::u8> build(const std::function<void(ClassWriter&)>& configure, int flags = 0)
    {
        ClassWriter writer(arena_, flags);
        writer.visit(Opcodes::V17,
                     Opcodes::ACC_PUBLIC,
                     ref("sample/DirectClass"),
                     {},
                     ref("java/lang/Object"),
                     {});
        configure(writer);
        writer.visitEnd();
        return writer.toByteArray();
    }

    void read(const std::vector<ca::u8>& bytes, ClassRecorder& recorder,
              Attribute* prototypes = nullptr)
    {
        ClassReader reader(arena_, bytes);
        if (prototypes == nullptr)
            reader.accept(&recorder, 0);
        else
            reader.accept(&recorder, prototypes, 0);
    }

    // 每个测试都从 writer 事件构造 class，再从 reader 回调观察结果，避免只断言内部状态。
    ClassRecorder roundtrip(const std::function<void(ClassWriter&)>& configure, int flags = 0)
    {
        auto          bytes = build(configure, flags);
        ClassRecorder recorder;
        read(bytes, recorder);
        return recorder;
    }

    ca::str::Utf8StringArena arena_;
};

TEST_F(ClassWriterDirectContractTest, DefaultFlagsDoNotComputeMaxsOrFrames)
{
    ClassWriter writer(arena_);
    EXPECT_FALSE(writer.hasFlags(ClassWriter::COMPUTE_MAXS));
    EXPECT_FALSE(writer.hasFlags(ClassWriter::COMPUTE_FRAMES));
}

TEST_F(ClassWriterDirectContractTest, ComputeMaxsFlagIsReported)
{
    ClassWriter writer(arena_, ClassWriter::COMPUTE_MAXS);
    EXPECT_TRUE(writer.hasFlags(ClassWriter::COMPUTE_MAXS));
    EXPECT_FALSE(writer.hasFlags(ClassWriter::COMPUTE_FRAMES));
}

TEST_F(ClassWriterDirectContractTest, ComputeFramesImplicitlyIncludesMaxs)
{
    ClassWriter writer(arena_, ClassWriter::COMPUTE_FRAMES);
    EXPECT_TRUE(writer.hasFlags(ClassWriter::COMPUTE_FRAMES));
    EXPECT_TRUE(writer.hasFlags(ClassWriter::COMPUTE_MAXS));
}

TEST_F(ClassWriterDirectContractTest, HeaderFieldsRoundtrip)
{
    ClassWriter writer(arena_);
    writer.visit(Opcodes::V11,
                 Opcodes::ACC_PUBLIC | Opcodes::ACC_FINAL,
                 ref("sample/Header"),
                 ref("Ljava/lang/Object;"),
                 ref("sample/Base"),
                 {ref("java/io/Serializable"), ref("java/lang/Runnable")});
    auto bytes = writer.toByteArray();

    ClassRecorder recorder;
    read(bytes, recorder);
    EXPECT_EQ(Opcodes::V11, recorder.version);
    EXPECT_EQ(Opcodes::ACC_PUBLIC | Opcodes::ACC_FINAL, recorder.access);
    EXPECT_EQ("sample/Header", recorder.name);
    EXPECT_EQ("Ljava/lang/Object;", recorder.signature);
    EXPECT_EQ("sample/Base", recorder.superName);
    EXPECT_EQ((std::vector<std::string>{"java/io/Serializable", "java/lang/Runnable"}),
              recorder.interfaces);
}

TEST_F(ClassWriterDirectContractTest, RawHeaderContainsMagicVersionAndConstantPool)
{
    const auto bytes = build([](ClassWriter&) {});
    ASSERT_GE(bytes.size(), 10U);
    EXPECT_EQ(0xCA, bytes[0]);
    EXPECT_EQ(0xFE, bytes[1]);
    EXPECT_EQ(0xBA, bytes[2]);
    EXPECT_EQ(0xBE, bytes[3]);
    EXPECT_EQ(0, (static_cast<int>(bytes[4]) << 8) | bytes[5]);
    EXPECT_EQ(61, (static_cast<int>(bytes[6]) << 8) | bytes[7]);
    EXPECT_GT((static_cast<int>(bytes[8]) << 8) | bytes[9], 1);
}

TEST_F(ClassWriterDirectContractTest, SourceFileRoundtrips)
{
    auto recorder =
        roundtrip([](ClassWriter& writer) { writer.visitSource(ref("Direct.java"), {}); });
    EXPECT_EQ("Direct.java", recorder.sourceFile);
}

TEST_F(ClassWriterDirectContractTest, SourceDebugExtensionRoundtrips)
{
    auto recorder = roundtrip(
        [](ClassWriter& writer) { writer.visitSource(ref("Direct.java"), ref("SMAP\nDirect")); });
    EXPECT_EQ("SMAP\nDirect", recorder.sourceDebug);
}

TEST_F(ClassWriterDirectContractTest, EnclosingMethodRoundtrips)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        writer.visitOuterClass(ref("sample/Owner"), ref("factory"), ref("()V"));
    });
    EXPECT_EQ("sample/Owner", recorder.outerOwner);
    EXPECT_EQ("factory", recorder.outerMethodName);
    EXPECT_EQ("()V", recorder.outerMethodDesc);
}

TEST_F(ClassWriterDirectContractTest, VisibleClassAnnotationRoundtrips)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        writer.visitAnnotation(ref("Lsample/Visible;"), true)->visitEnd();
    });
    ASSERT_EQ(1U, recorder.annotations.size());
    EXPECT_EQ("Lsample/Visible;", recorder.annotations[0].descriptor);
    EXPECT_TRUE(recorder.annotations[0].visible);
}

TEST_F(ClassWriterDirectContractTest, InvisibleClassAnnotationRoundtrips)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        writer.visitAnnotation(ref("Lsample/Invisible;"), false)->visitEnd();
    });
    ASSERT_EQ(1U, recorder.annotations.size());
    EXPECT_FALSE(recorder.annotations[0].visible);
}

TEST_F(ClassWriterDirectContractTest, ClassTypeAnnotationRoundtrips)
{
    const int typeRef  = TypeReference::newSuperTypeReference(-1).getValue();
    auto      recorder = roundtrip([&](ClassWriter& writer) {
        writer.visitTypeAnnotation(typeRef, nullptr, ref("Lsample/TypeUse;"), true)->visitEnd();
    });
    ASSERT_EQ(1U, recorder.annotations.size());
    EXPECT_EQ(typeRef, recorder.annotations[0].typeRef);
}

TEST_F(ClassWriterDirectContractTest, InnerClassRoundtrips)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        writer.visitInnerClass(
            ref("sample/Outer$Inner"), ref("sample/Outer"), ref("Inner"), Opcodes::ACC_PUBLIC);
    });
    ASSERT_EQ(1U, recorder.innerClasses.size());
    EXPECT_EQ("sample/Outer$Inner", recorder.innerClasses[0].name);
    EXPECT_EQ("sample/Outer", recorder.innerClasses[0].outerName);
    EXPECT_EQ("Inner", recorder.innerClasses[0].innerName);
}

TEST_F(ClassWriterDirectContractTest, NestHostRoundtrips)
{
    auto recorder =
        roundtrip([](ClassWriter& writer) { writer.visitNestHost(ref("sample/NestHost")); });
    EXPECT_EQ("sample/NestHost", recorder.nestHost);
}

TEST_F(ClassWriterDirectContractTest, NestMembersRoundtrip)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        writer.visitNestMember(ref("sample/NestHost$One"));
        writer.visitNestMember(ref("sample/NestHost$Two"));
    });
    EXPECT_EQ((std::vector<std::string>{"sample/NestHost$One", "sample/NestHost$Two"}),
              recorder.nestMembers);
}

TEST_F(ClassWriterDirectContractTest, PermittedSubclassesRoundtrip)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        writer.visitPermittedSubclass(ref("sample/ChildOne"));
        writer.visitPermittedSubclass(ref("sample/ChildTwo"));
    });
    EXPECT_EQ((std::vector<std::string>{"sample/ChildOne", "sample/ChildTwo"}),
              recorder.permittedSubclasses);
}

TEST_F(ClassWriterDirectContractTest, CustomClassAttributeRoundtrips)
{
    const ca::u8 payload[] = {1, 2, 3};
    Attribute    written(ref("DirectAttribute"),
                      ca::core::Bytes::copy_from_slice(payload, sizeof(payload)));
    auto         bytes = build([&](ClassWriter& writer) { writer.visitAttribute(written); });

    Attribute     prototype(ref("DirectAttribute"));
    ClassRecorder recorder;
    read(bytes, recorder, &prototype);
    ASSERT_EQ(1U, recorder.attributes.size());
    EXPECT_EQ((std::vector<ca::u8>{1, 2, 3}), recorder.attributes[0].value);
}

TEST_F(ClassWriterDirectContractTest, BasicFieldRoundtrips)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        writer.visitField(Opcodes::ACC_PRIVATE, ref("count"), ref("I"), {}, {})->visitEnd();
    });
    ASSERT_EQ(1U, recorder.fields.size());
    EXPECT_EQ(Opcodes::ACC_PRIVATE, recorder.fields[0].access);
    EXPECT_EQ("count", recorder.fields[0].name);
    EXPECT_EQ("I", recorder.fields[0].descriptor);
}

TEST_F(ClassWriterDirectContractTest, PrimitiveReferenceAndArrayFieldsRoundtrip)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        writer.visitField(Opcodes::ACC_PUBLIC, ref("number"), ref("J"), {}, {})->visitEnd();
        writer.visitField(Opcodes::ACC_PUBLIC, ref("text"), ref("Ljava/lang/String;"), {}, {})
            ->visitEnd();
        writer.visitField(Opcodes::ACC_PUBLIC, ref("values"), ref("[I"), {}, {})->visitEnd();
    });
    ASSERT_EQ(3U, recorder.fields.size());
    EXPECT_EQ("J", recorder.fields[0].descriptor);
    EXPECT_EQ("Ljava/lang/String;", recorder.fields[1].descriptor);
    EXPECT_EQ("[I", recorder.fields[2].descriptor);
}

TEST_F(ClassWriterDirectContractTest, FieldSignatureAndConstantValueRoundtrip)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        writer
            .visitField(Opcodes::ACC_STATIC | Opcodes::ACC_FINAL,
                        ref("answer"),
                        ref("I"),
                        ref("TT;"),
                        ca::core::Any(42))
            ->visitEnd();
    });
    ASSERT_EQ(1U, recorder.fields.size());
    EXPECT_EQ("TT;", recorder.fields[0].signature);
    ASSERT_TRUE(recorder.fields[0].value.is<int>());
    EXPECT_EQ(42, recorder.fields[0].value.cast<int>());
}

TEST_F(ClassWriterDirectContractTest, FieldAnnotationsRoundtrip)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        auto* field = writer.visitField(Opcodes::ACC_PUBLIC, ref("value"), ref("I"), {}, {});
        field->visitAnnotation(ref("Lsample/Field;"), true)->visitEnd();
        field
            ->visitTypeAnnotation(TypeReference::newTypeReference(TypeReference::FIELD).getValue(),
                                  nullptr,
                                  ref("Lsample/FieldType;"),
                                  false)
            ->visitEnd();
        field->visitEnd();
    });
    ASSERT_EQ(2U, recorder.fields[0].annotations.size());
    EXPECT_EQ("Lsample/Field;", recorder.fields[0].annotations[0].descriptor);
    EXPECT_EQ("Lsample/FieldType;", recorder.fields[0].annotations[1].descriptor);
}

TEST_F(ClassWriterDirectContractTest, CustomFieldAttributeRoundtrips)
{
    const ca::u8 payload[] = {9};
    Attribute    written(ref("DirectAttribute"),
                      ca::core::Bytes::copy_from_slice(payload, sizeof(payload)));
    auto         bytes = build([&](ClassWriter& writer) {
        auto* field = writer.visitField(Opcodes::ACC_PUBLIC, ref("value"), ref("I"), {}, {});
        field->visitAttribute(written);
        field->visitEnd();
    });

    Attribute     prototype(ref("DirectAttribute"));
    ClassRecorder recorder;
    read(bytes, recorder, &prototype);
    ASSERT_EQ(1U, recorder.fields[0].attributes.size());
    EXPECT_EQ(9, recorder.fields[0].attributes[0].value[0]);
}

TEST_F(ClassWriterDirectContractTest, AbstractAndNativeMethodsHaveNoCode)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        writer
            .visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_ABSTRACT,
                         ref("abstractMethod"),
                         ref("()V"),
                         {},
                         {})
            ->visitEnd();
        writer
            .visitMethod(
                Opcodes::ACC_PUBLIC | Opcodes::ACC_NATIVE, ref("nativeMethod"), ref("()V"), {}, {})
            ->visitEnd();
    });
    ASSERT_EQ(2U, recorder.methods.size());
    EXPECT_FALSE(recorder.methods[0].codeVisited);
    EXPECT_FALSE(recorder.methods[1].codeVisited);
}

TEST_F(ClassWriterDirectContractTest, MethodSignatureAndExceptionsRoundtrip)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        writer
            .visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_ABSTRACT,
                         ref("load"),
                         ref("()Ljava/lang/Object;"),
                         ref("()TT;"),
                         {ref("java/io/IOException")})
            ->visitEnd();
    });
    ASSERT_EQ(1U, recorder.methods.size());
    EXPECT_EQ("()TT;", recorder.methods[0].signature);
    EXPECT_EQ((std::vector<std::string>{"java/io/IOException"}), recorder.methods[0].exceptions);
}

TEST_F(ClassWriterDirectContractTest, SimpleMethodCodeAndMaxsRoundtrip)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        auto* method = writer.visitMethod(
            Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("one"), ref("()I"), {}, {});
        method->visitCode();
        method->visitInsn(Opcodes::ICONST_1);
        method->visitInsn(Opcodes::IRETURN);
        method->visitMaxs(1, 0);
        method->visitEnd();
    });
    ASSERT_EQ(1U, recorder.methods.size());
    EXPECT_TRUE(recorder.methods[0].codeVisited);
    EXPECT_EQ((std::vector<int>{Opcodes::ICONST_1, Opcodes::IRETURN}),
              recorder.methods[0].instructions);
    EXPECT_EQ(1, recorder.methods[0].maxStack);
    EXPECT_EQ(0, recorder.methods[0].maxLocals);
}

TEST_F(ClassWriterDirectContractTest, ComputeMaxsCalculatesStackAndLocals)
{
    auto recorder = roundtrip(
        [](ClassWriter& writer) {
            auto* method =
                writer.visitMethod(Opcodes::ACC_PUBLIC, ref("identity"), ref("(I)I"), {}, {});
            method->visitCode();
            method->visitVarInsn(Opcodes::ILOAD, 1);
            method->visitInsn(Opcodes::IRETURN);
            method->visitMaxs(0, 0);
            method->visitEnd();
        },
        ClassWriter::COMPUTE_MAXS);
    ASSERT_EQ(1U, recorder.methods.size());
    EXPECT_EQ(1, recorder.methods[0].maxStack);
    EXPECT_EQ(2, recorder.methods[0].maxLocals);
}

TEST_F(ClassWriterDirectContractTest, TryCatchTableRoundtrips)
{
    Label start;
    Label end;
    Label handler;
    auto  recorder = roundtrip([&](ClassWriter& writer) {
        auto* method = writer.visitMethod(
            Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("guarded"), ref("()V"), {}, {});
        method->visitCode();
        method->visitTryCatchBlock(&start, &end, &handler, ref("java/lang/Exception"));
        method->visitLabel(&start);
        method->visitInsn(Opcodes::RETURN);
        method->visitLabel(&end);
        method->visitLabel(&handler);
        method->visitInsn(Opcodes::RETURN);
        method->visitMaxs(1, 0);
        method->visitEnd();
    });
    ASSERT_EQ(1U, recorder.methods.size());
    EXPECT_EQ(1, recorder.methods[0].tryCatchCount);
}

TEST_F(ClassWriterDirectContractTest, LineAndLocalVariableTablesRoundtrip)
{
    Label start;
    Label end;
    auto  recorder = roundtrip([&](ClassWriter& writer) {
        auto* method = writer.visitMethod(
            Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("debug"), ref("(I)V"), {}, {});
        method->visitCode();
        method->visitLabel(&start);
        method->visitLineNumber(123, &start);
        method->visitInsn(Opcodes::RETURN);
        method->visitLabel(&end);
        method->visitLocalVariable(ref("value"), ref("I"), {}, &start, &end, 0);
        method->visitMaxs(0, 1);
        method->visitEnd();
    });
    ASSERT_EQ(1U, recorder.methods.size());
    EXPECT_EQ((std::vector<int>{123}), recorder.methods[0].lines);
    EXPECT_EQ((std::vector<std::string>{"value"}), recorder.methods[0].localVariables);
}

TEST_F(ClassWriterDirectContractTest, MethodAndParameterAnnotationsRoundtrip)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        auto* method = writer.visitMethod(
            Opcodes::ACC_PUBLIC | Opcodes::ACC_ABSTRACT, ref("annotated"), ref("(I)V"), {}, {});
        method->visitParameter(ref("value"), Opcodes::ACC_FINAL);
        method->visitAnnotation(ref("Lsample/Method;"), true)->visitEnd();
        method->visitParameterAnnotation(0, ref("Lsample/Parameter;"), false)->visitEnd();
        method->visitEnd();
    });
    ASSERT_EQ(1U, recorder.methods.size());
    EXPECT_EQ((std::vector<std::string>{"value"}), recorder.methods[0].parameters);
    ASSERT_EQ(2U, recorder.methods[0].annotations.size());
    EXPECT_EQ("Lsample/Parameter;@0", recorder.methods[0].annotations[1].descriptor);
}

TEST_F(ClassWriterDirectContractTest, AnnotationDefaultRoundtrips)
{
    auto recorder = roundtrip([](ClassWriter& writer) {
        auto* method = writer.visitMethod(
            Opcodes::ACC_PUBLIC | Opcodes::ACC_ABSTRACT, ref("value"), ref("()I"), {}, {});
        method->visitAnnotationDefault()->visit({}, ca::core::Any(7));
        method->visitEnd();
    });
    ASSERT_EQ(1U, recorder.methods.size());
    ASSERT_TRUE(recorder.methods[0].annotationDefault.is<int>());
    EXPECT_EQ(7, recorder.methods[0].annotationDefault.cast<int>());
}

TEST_F(ClassWriterDirectContractTest, CustomMethodAttributeRoundtrips)
{
    const ca::u8 payload[] = {4, 5};
    Attribute    written(ref("DirectAttribute"),
                      ca::core::Bytes::copy_from_slice(payload, sizeof(payload)));
    auto         bytes = build([&](ClassWriter& writer) {
        auto* method = writer.visitMethod(
            Opcodes::ACC_PUBLIC | Opcodes::ACC_ABSTRACT, ref("custom"), ref("()V"), {}, {});
        method->visitAttribute(written);
        method->visitEnd();
    });

    Attribute     prototype(ref("DirectAttribute"));
    ClassRecorder recorder;
    read(bytes, recorder, &prototype);
    ASSERT_EQ(1U, recorder.methods[0].attributes.size());
    EXPECT_EQ((std::vector<ca::u8>{4, 5}), recorder.methods[0].attributes[0].value);
}

}   // namespace
