#include <gtest/gtest.h>
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/writers.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

namespace {

// 全测试套共享 arena（gtest 串行，intern 累积无害）。
ca::str::Utf8StringArena arena;

struct EmptyAnnotationVisitor : AnnotationVisitor
{
    explicit EmptyAnnotationVisitor(int api)
        : AnnotationVisitor(api)
    {}

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef, ca::str::Utf8StringRef) override
    {
        return this;
    }

    AnnotationVisitor* visitArray(ca::str::Utf8StringRef) override { return this; }
};

struct RemoveAnnotationsMethodVisitor : MethodVisitor
{
    EmptyAnnotationVisitor annotationVisitor;

    RemoveAnnotationsMethodVisitor(int api, MethodVisitor* methodVisitor)
        : MethodVisitor(api, methodVisitor)
        , annotationVisitor(api)
    {}

    AnnotationVisitor* visitAnnotationDefault() override { return &annotationVisitor; }
    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef, bool) override
    {
        return &annotationVisitor;
    }
    AnnotationVisitor* visitTypeAnnotation(int, const TypePath*, ca::str::Utf8StringRef,
                                           bool) override
    {
        return &annotationVisitor;
    }
    AnnotationVisitor* visitParameterAnnotation(int, ca::str::Utf8StringRef, bool) override
    {
        return &annotationVisitor;
    }
    AnnotationVisitor* visitInsnAnnotation(int, const TypePath*, ca::str::Utf8StringRef,
                                           bool) override
    {
        return &annotationVisitor;
    }
    AnnotationVisitor* visitTryCatchAnnotation(int, const TypePath*, ca::str::Utf8StringRef,
                                               bool) override
    {
        return &annotationVisitor;
    }
    AnnotationVisitor* visitLocalVariableAnnotation(int, const TypePath*,
                                                    const std::vector<Label*>&,
                                                    const std::vector<Label*>&,
                                                    const std::vector<int>&, ca::str::Utf8StringRef,
                                                    bool) override
    {
        return &annotationVisitor;
    }
};

struct RemoveAnnotationsAdapter : ClassVisitor
{
    EmptyAnnotationVisitor annotationVisitor;

    RemoveAnnotationsAdapter(int api, ClassVisitor* classVisitor)
        : ClassVisitor(api, classVisitor)
        , annotationVisitor(api)
    {}

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef, bool) override
    {
        return &annotationVisitor;
    }

    AnnotationVisitor* visitTypeAnnotation(int, const TypePath*, ca::str::Utf8StringRef,
                                           bool) override
    {
        return &annotationVisitor;
    }

    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name,
                               ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        return new RemoveAnnotationsMethodVisitor(
            api, ClassVisitor::visitMethod(access, name, descriptor, signature, exceptions));
    }
};

struct DeleteAnnotationsMethodVisitor : MethodVisitor
{
    DeleteAnnotationsMethodVisitor(int api, MethodVisitor* methodVisitor)
        : MethodVisitor(api, methodVisitor)
    {}

    AnnotationVisitor* visitAnnotationDefault() override { return nullptr; }
    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef, bool) override { return nullptr; }
    AnnotationVisitor* visitTypeAnnotation(int, const TypePath*, ca::str::Utf8StringRef,
                                           bool) override
    {
        return nullptr;
    }
    AnnotationVisitor* visitParameterAnnotation(int, ca::str::Utf8StringRef, bool) override
    {
        return nullptr;
    }
    AnnotationVisitor* visitInsnAnnotation(int, const TypePath*, ca::str::Utf8StringRef,
                                           bool) override
    {
        return nullptr;
    }
    AnnotationVisitor* visitTryCatchAnnotation(int, const TypePath*, ca::str::Utf8StringRef,
                                               bool) override
    {
        return nullptr;
    }
    AnnotationVisitor* visitLocalVariableAnnotation(int, const TypePath*,
                                                    const std::vector<Label*>&,
                                                    const std::vector<Label*>&,
                                                    const std::vector<int>&, ca::str::Utf8StringRef,
                                                    bool) override
    {
        return nullptr;
    }
};

struct DeleteAnnotationsAdapter : ClassVisitor
{
    DeleteAnnotationsAdapter(int api, ClassVisitor* classVisitor)
        : ClassVisitor(api, classVisitor)
    {}

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef, bool) override { return nullptr; }

    AnnotationVisitor* visitTypeAnnotation(int, const TypePath*, ca::str::Utf8StringRef,
                                           bool) override
    {
        return nullptr;
    }

    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name,
                               ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        return new DeleteAnnotationsMethodVisitor(
            api, ClassVisitor::visitMethod(access, name, descriptor, signature, exceptions));
    }
};

}   // namespace

TEST(AnnotationVisitorTest, testConstructor_validApi)
{
    ASSERT_NO_THROW({ AnnotationVisitor av(Opcodes::ASM4); });
}

TEST(AnnotationVisitorTest, testConstructor_invalidApi)
{
    try {
        AnnotationVisitor av(0);
        FAIL() << "Expected std::invalid_argument";
    }
    catch (const std::invalid_argument& e) {
        ASSERT_STREQ("Unsupported api 0", e.what());
    }
}

TEST(AnnotationVisitorTest, testGetDelegate)
{
    AnnotationVisitor* delegate = new AnnotationVisitor(Opcodes::ASM4);
    AnnotationVisitor  visitor(Opcodes::ASM4, delegate);
    EXPECT_EQ(delegate, visitor.getDelegate());
    delete delegate;
}

TEST(AnnotationVisitorTest, testReadAndWrite_removeOrDeleteAnnotations)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader              classReader(arena, classFile);
        ClassWriter              removedAnnotationsClassWriter(arena, 0);
        ClassWriter              deletedAnnotationsClassWriter(arena, 0);
        RemoveAnnotationsAdapter removeAnnotationsAdapter(Opcodes::ASM10_EXPERIMENTAL,
                                                          &removedAnnotationsClassWriter);
        DeleteAnnotationsAdapter deleteAnnotationsAdapter(Opcodes::ASM10_EXPERIMENTAL,
                                                          &deletedAnnotationsClassWriter);

        EXPECT_NO_THROW(classReader.accept(&removeAnnotationsAdapter, 0)) << classParameter.name;
        EXPECT_NO_THROW(classReader.accept(&deleteAnnotationsAdapter, 0)) << classParameter.name;
        EXPECT_EQ(removedAnnotationsClassWriter.toByteArray(),
                  deletedAnnotationsClassWriter.toByteArray())
            << classParameter.name;
    }
}
