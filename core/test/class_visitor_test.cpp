#include <gtest/gtest.h>
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/type.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

namespace {

// 全测试套共享 arena（gtest 串行，intern 累积无害）。
ca::str::Utf8StringArena arena;

std::vector<uint8_t> readClassSample(const std::string& name = "DefaultPackage.class")
{
    auto bytes = readClassFile(name);
    if (bytes.empty() && name.size() > 4 && name.substr(name.size() - 4) == ".ref") {
        bytes = readClassFile(name.substr(0, name.size() - 4));
    }
    return bytes;
}

struct PassThroughClassVisitor : ClassVisitor
{
    AnnotationVisitor      annotationVisitor;
    FieldVisitor           fieldVisitor;
    MethodVisitor          methodVisitor;
    ModuleVisitor          moduleVisitor;
    RecordComponentVisitor recordComponentVisitor;

    explicit PassThroughClassVisitor(int api, ClassVisitor* classVisitor)
        : ClassVisitor(api, classVisitor)
        , annotationVisitor(api)
        , fieldVisitor(api)
        , methodVisitor(api)
        , moduleVisitor(api)
        , recordComponentVisitor(api)
    {}

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible) override
    {
        ClassVisitor::visitAnnotation(descriptor, visible);
        return &annotationVisitor;
    }

    AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                           ca::str::Utf8StringRef descriptor, bool visible) override
    {
        ClassVisitor::visitTypeAnnotation(typeRef, typePath, descriptor, visible);
        return &annotationVisitor;
    }

    FieldVisitor* visitField(int access, ca::str::Utf8StringRef name,
                             ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                             const ca::core::Any& value) override
    {
        ClassVisitor::visitField(access, name, descriptor, signature, value);
        return &fieldVisitor;
    }

    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name,
                               ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        ClassVisitor::visitMethod(access, name, descriptor, signature, exceptions);
        return &methodVisitor;
    }

    ModuleVisitor* visitModule(ca::str::Utf8StringRef name, int access,
                               ca::str::Utf8StringRef version) override
    {
        ClassVisitor::visitModule(name, access, version);
        return &moduleVisitor;
    }

    RecordComponentVisitor* visitRecordComponent(ca::str::Utf8StringRef name,
                                                 ca::str::Utf8StringRef descriptor,
                                                 ca::str::Utf8StringRef signature) override
    {
        ClassVisitor::visitRecordComponent(name, descriptor, signature);
        return &recordComponentVisitor;
    }
};

struct ChangeExceptionAdapter : ClassVisitor
{
    explicit ChangeExceptionAdapter(ClassVisitor* classVisitor)
        : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL, classVisitor)
    {}

    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name,
                               ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        auto changed = exceptions;
        if (!changed.empty()) changed[0] = ZUtf8StringRef::from_static("java/lang/Throwable");
        return ClassVisitor::visitMethod(access, name, descriptor, signature, changed);
    }
};

struct ChangeAccessAdapter : ClassVisitor
{
    int accessFlags;

    ChangeAccessAdapter(ClassVisitor* classVisitor, int accessFlags)
        : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL, classVisitor)
        , accessFlags(accessFlags)
    {}

    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name,
                               ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        return ClassVisitor::visitMethod(
            access ^ accessFlags, name, descriptor, signature, exceptions);
    }
};

struct ChangeVersionAdapter : ClassVisitor
{
    int newVersion;

    ChangeVersionAdapter(ClassVisitor* classVisitor, int newVersion)
        : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL, classVisitor)
        , newVersion(newVersion)
    {}

    void visit(int, int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef signature,
               ca::str::Utf8StringRef                     superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override
    {
        ClassVisitor::visit(newVersion, access, name, signature, superName, interfaces);
    }
};

struct AddParameterAdapter : ClassVisitor
{
    explicit AddParameterAdapter(ClassVisitor* classVisitor)
        : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL, classVisitor)
    {}

    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name,
                               ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        std::string desc = descriptor.to_std_string();
        if (!desc.empty() && desc[0] == '(') {
            desc.insert(1, "I");
        }
        return ClassVisitor::visitMethod(
            access, name, ca::str::Utf8StringRef::from_cstr(desc.c_str()), signature, exceptions);
    }
};

struct RemoveAnnotationAdapter : ClassVisitor
{
    bool visibilityValue;

    RemoveAnnotationAdapter(ClassVisitor* classVisitor, bool visibilityValue)
        : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL, classVisitor)
        , visibilityValue(visibilityValue)
    {}

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible) override
    {
        if (visible == visibilityValue) return nullptr;
        return ClassVisitor::visitAnnotation(descriptor, visible);
    }

    AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                           ca::str::Utf8StringRef descriptor, bool visible) override
    {
        if (visible == visibilityValue) return nullptr;
        return ClassVisitor::visitTypeAnnotation(typeRef, typePath, descriptor, visible);
    }
};

}   // namespace

TEST(ClassVisitorTest, testConstructor_validApi)
{
    ASSERT_NO_THROW({ ClassVisitor cv(Opcodes::ASM4); });
}

TEST(ClassVisitorTest, testConstructor_invalidApi)
{
    try {
        ClassVisitor cv(0);
        FAIL() << "Expected std::invalid_argument";
    }
    catch (const std::invalid_argument& e) {
        ASSERT_STREQ("Unsupported api 0", e.what());
    }
}

TEST(ClassVisitorTest, testGetDelegate)
{
    ClassVisitor* delegate = new ClassVisitor(Opcodes::ASM4);
    ClassVisitor  visitor(Opcodes::ASM4, delegate);
    EXPECT_EQ(delegate, visitor.getDelegate());
    delete delegate;
}

TEST(ClassVisitorTest, testReadAndWrite_emptyVisitor)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader             classReader(arena, classFile);
        ClassWriter             classWriter(arena, 0);
        PassThroughClassVisitor classAdapter(Opcodes::ASM10_EXPERIMENTAL, &classWriter);

        EXPECT_NO_THROW(classReader.accept(&classAdapter, 0)) << classParameter.name;
        EXPECT_FALSE(classWriter.toByteArray().empty()) << classParameter.name;
    }
}

TEST(ClassVisitorTest, testReadAndWrite_copyPool_changeMethodExceptions)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, 0);
        ClassWriter classWriterWithCopyPool(arena, classReader, 0);

        ChangeExceptionAdapter adapter(&classWriter);
        ChangeExceptionAdapter copyPoolAdapter(&classWriterWithCopyPool);
        classReader.accept(&adapter, 0);
        classReader.accept(&copyPoolAdapter, 0);

        EXPECT_EQ(classWriter.toByteArray(), classWriterWithCopyPool.toByteArray())
            << classParameter.name;
    }
}

TEST(ClassVisitorTest, testReadAndWrite_copyPool_changeMethodDeprecatedFlag)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, 0);
        ClassWriter classWriterWithCopyPool(arena, classReader, 0);

        ChangeAccessAdapter adapter(&classWriter, Opcodes::ACC_DEPRECATED);
        ChangeAccessAdapter copyPoolAdapter(&classWriterWithCopyPool, Opcodes::ACC_DEPRECATED);
        classReader.accept(&adapter, 0);
        classReader.accept(&copyPoolAdapter, 0);

        EXPECT_EQ(classWriter.toByteArray(), classWriterWithCopyPool.toByteArray())
            << classParameter.name;
    }
}

TEST(ClassVisitorTest, testReadAndWrite_copyPool_changeMethodSyntheticFlag)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, 0);
        ClassWriter classWriterWithCopyPool(arena, classReader, 0);

        ChangeAccessAdapter adapter(&classWriter, Opcodes::ACC_SYNTHETIC);
        ChangeAccessAdapter copyPoolAdapter(&classWriterWithCopyPool, Opcodes::ACC_SYNTHETIC);
        classReader.accept(&adapter, 0);
        classReader.accept(&copyPoolAdapter, 0);

        EXPECT_EQ(classWriter.toByteArray(), classWriterWithCopyPool.toByteArray())
            << classParameter.name;
    }
}

TEST(ClassVisitorTest, testReadAndWrite_copyPool_changeClassVersionAndMethodSyntheticFlag)
{
    ClassWriter sourceClassWriter(arena, 0);
    sourceClassWriter.visit(Opcodes::V1_4,
                            Opcodes::ACC_ABSTRACT,
                            ZUtf8StringRef::from_static("C"),
                            ZUtf8StringRef::from_static(""),
                            ZUtf8StringRef::from_static("java/lang/Object"),
                            {});
    sourceClassWriter
        .visitMethod(Opcodes::ACC_ABSTRACT | Opcodes::ACC_SYNTHETIC,
                     ZUtf8StringRef::from_static("m"),
                     ZUtf8StringRef::from_static("()V"),
                     ZUtf8StringRef::from_static(""),
                     {})
        ->visitEnd();
    sourceClassWriter.visitEnd();
    auto                 classFile = sourceClassWriter.toByteArray();
    ClassReader          classReader(arena, classFile);
    ClassWriter          classWriter(arena, 0);
    ClassWriter          copyPoolClassWriter(arena, classReader, 0);
    ChangeAccessAdapter  accessAdapter(&classWriter, Opcodes::ACC_SYNTHETIC);
    ChangeVersionAdapter versionAdapter(&accessAdapter, Opcodes::V1_5);
    ChangeAccessAdapter  copyPoolAccessAdapter(&copyPoolClassWriter, Opcodes::ACC_SYNTHETIC);
    ChangeVersionAdapter copyPoolVersionAdapter(&copyPoolAccessAdapter, Opcodes::V1_5);

    classReader.accept(&versionAdapter, 0);
    classReader.accept(&copyPoolVersionAdapter, 0);

    EXPECT_EQ(classWriter.toByteArray(), copyPoolClassWriter.toByteArray());
}

TEST(ClassVisitorTest, testReadAndWrite_copyPool_changeMethodDescriptor)
{
    ClassWriter sourceClassWriter(arena, ClassWriter::COMPUTE_MAXS);
    sourceClassWriter.visit(Opcodes::V1_7,
                            Opcodes::ACC_ABSTRACT,
                            ZUtf8StringRef::from_static("C"),
                            ZUtf8StringRef::from_static(""),
                            ZUtf8StringRef::from_static("java/lang/Object"),
                            {});
    MethodVisitor* methodVisitor =
        sourceClassWriter.visitMethod(Opcodes::ACC_PUBLIC,
                                      ZUtf8StringRef::from_static("<init>"),
                                      ZUtf8StringRef::from_static("()V"),
                                      ZUtf8StringRef::from_static(""),
                                      {});
    methodVisitor->visitCode();
    methodVisitor->visitVarInsn(Opcodes::ALOAD, 0);
    methodVisitor->visitMethodInsn(Opcodes::INVOKESPECIAL,
                                   ZUtf8StringRef::from_static("java/lang/Object"),
                                   ZUtf8StringRef::from_static("<init>"),
                                   ZUtf8StringRef::from_static("()V"),
                                   false);
    methodVisitor->visitInsn(Opcodes::RETURN);
    methodVisitor->visitMaxs(0, 0);
    methodVisitor->visitEnd();
    sourceClassWriter.visitEnd();
    auto                classFile = sourceClassWriter.toByteArray();
    ClassReader         classReader(arena, classFile);
    ClassWriter         classWriter(arena, ClassWriter::COMPUTE_MAXS);
    ClassWriter         copyPoolClassWriter(arena, classReader, ClassWriter::COMPUTE_MAXS);
    AddParameterAdapter adapter(&classWriter);
    AddParameterAdapter copyPoolAdapter(&copyPoolClassWriter);

    classReader.accept(&adapter, 0);
    classReader.accept(&copyPoolAdapter, 0);

    EXPECT_EQ(classWriter.toByteArray(), copyPoolClassWriter.toByteArray());
}

TEST(ClassVisitorTest, testReadAndWrite_removeAnnotations)
{
    auto classFile = readClassSample("origin/Issue307600.class");
    ASSERT_FALSE(classFile.empty());
    ClassReader             classReader(arena, classFile);
    ClassWriter             classWriter(arena, 0);
    RemoveAnnotationAdapter removeVisibleAdapter(&classWriter, true);
    classReader.accept(&removeVisibleAdapter, 0);
    auto                    output = classWriter.toByteArray();
    ClassReader             newClassReader(arena, output);
    ClassWriter             newClassWriter(arena, 0);
    RemoveAnnotationAdapter removeVisibleAgainAdapter(&newClassWriter, true);

    newClassReader.accept(&removeVisibleAgainAdapter, 0);

    EXPECT_EQ(output, newClassWriter.toByteArray());

    // Also test visibility=false (invisible annotations)
    ClassWriter             classWriter2(arena, 0);
    RemoveAnnotationAdapter removeInvisibleAdapter(&classWriter2, false);
    classReader.accept(&removeInvisibleAdapter, 0);
    auto                    output2 = classWriter2.toByteArray();
    ClassReader             newClassReader2(arena, output2);
    ClassWriter             newClassWriter2(arena, 0);
    RemoveAnnotationAdapter removeInvisibleAgainAdapter(&newClassWriter2, false);
    newClassReader2.accept(&removeInvisibleAgainAdapter, 0);
    EXPECT_EQ(output2, newClassWriter2.toByteArray());
}

TEST(ClassVisitorTest, testReadAndWrite_removeOptionalModuleData)
{
    auto classFile = readClassSample("origin/Issue307600.class");
    ASSERT_FALSE(classFile.empty());
    ClassReader classReader(arena, classFile);
    ClassWriter classWriter(arena, 0);
    struct RemoveOptionalModuleDataVisitor : ClassVisitor
    {
        ModuleVisitor moduleVisitor;

        explicit RemoveOptionalModuleDataVisitor(ClassVisitor* classVisitor)
            : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL, classVisitor)
            , moduleVisitor(Opcodes::ASM10_EXPERIMENTAL, nullptr)
        {}

        ModuleVisitor* visitModule(ca::str::Utf8StringRef name, int access,
                                   ca::str::Utf8StringRef version) override
        {
            ClassVisitor::visitModule(name, access, version);
            return &moduleVisitor;
        }
    };
    RemoveOptionalModuleDataVisitor classVisitor(&classWriter);

    EXPECT_NO_THROW(classReader.accept(&classVisitor, 0));
    EXPECT_FALSE(classWriter.toByteArray().empty());
}
