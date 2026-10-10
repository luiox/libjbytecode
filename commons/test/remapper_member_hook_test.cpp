#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "test_utils.hpp"

namespace {

ca::str::Utf8StringRef S(const char* value)
{
    return ca::str::Utf8StringRef::from_cstr(value);
}

std::string text(ca::str::Utf8StringRef value)
{
    return value.to_std_string();
}

class MemberHookRemapper final : public Remapper
{
public:
    /// #921 staticArena 退役:基类注入构造必选,合成描述符 intern 进测试局部 arena。
    explicit MemberHookRemapper(ca::str::Utf8StringArena& arena)
        : Remapper(arena)
    {}

    ca::str::Utf8StringRef map(ca::str::Utf8StringRef internalName) override
    {
        if (internalName.equals(S("old/Owner"))) return S("new/Owner");
        if (internalName.equals(S("old/Type")) || internalName.equals(S("Old/Type"))) {
            return S("new/Type");
        }
        return internalName;
    }

    ca::str::Utf8StringRef mapFieldName(ca::str::Utf8StringRef owner,
                                         ca::str::Utf8StringRef name,
                                         ca::str::Utf8StringRef descriptor) override
    {
        if (owner.equals(S("old/Owner")) && name.equals(S("oldField")) &&
            descriptor.equals(S("LOld/Type;"))) {
            return S("newField");
        }
        return name;
    }

    ca::str::Utf8StringRef mapMethodName(ca::str::Utf8StringRef owner,
                                          ca::str::Utf8StringRef name,
                                          ca::str::Utf8StringRef descriptor) override
    {
        if (owner.equals(S("old/Owner")) && name.equals(S("oldMethod")) &&
            descriptor.equals(S("(LOld/Type;)LOld/Type;"))) {
            return S("newMethod");
        }
        if (owner.equals(S("old/Owner")) && name.equals(S("bootstrap")) &&
            descriptor.equals(S("()LOld/Type;"))) {
            return S("newBootstrap");
        }
        return name;
    }

    ca::str::Utf8StringRef mapInvokeDynamicMethodName(ca::str::Utf8StringRef name,
                                                       ca::str::Utf8StringRef descriptor) override
    {
        if (name.equals(S("oldDynamic"))) {
            return S("newDynamic");
        }
        return name;
    }

    ca::str::Utf8StringRef mapRecordComponentName(ca::str::Utf8StringRef owner,
                                                   ca::str::Utf8StringRef name,
                                                   ca::str::Utf8StringRef descriptor) override
    {
        if (owner.equals(S("old/Owner")) && name.equals(S("oldComponent")) &&
            descriptor.equals(S("LOld/Type;"))) {
            return S("newComponent");
        }
        return name;
    }
};

class RecordingRecordComponentVisitor final : public RecordComponentVisitor
{
public:
    RecordingRecordComponentVisitor()
        : RecordComponentVisitor(Opcodes::ASM9, nullptr)
    {}
};

class RecordingMethodVisitor final : public MethodVisitor
{
public:
    RecordingMethodVisitor()
        : MethodVisitor(Opcodes::ASM9, nullptr)
    {}

    void visitFieldInsn(int, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef descriptor) override
    {
        fieldOwner      = text(owner);
        fieldName       = text(name);
        fieldDescriptor = text(descriptor);
    }

    void visitMethodInsn(int, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor, bool) override
    {
        methodOwner      = text(owner);
        methodName       = text(name);
        methodDescriptor = text(descriptor);
    }

    void visitInvokeDynamicInsn(ca::str::Utf8StringRef name,
                                ca::str::Utf8StringRef descriptor,
                                const Handle&         bootstrapMethod,
                                const std::vector<ca::core::Any>& bootstrapArguments) override
    {
        indyName      = text(name);
        indyDescriptor = text(descriptor);
        indyBootstrap = bootstrapMethod;
        indyArguments = bootstrapArguments;
    }

    void visitLdcInsn(const ca::core::Any& value) override
    {
        ldcValue = value;
    }

    std::string                fieldOwner;
    std::string                fieldName;
    std::string                fieldDescriptor;
    std::string                methodOwner;
    std::string                methodName;
    std::string                methodDescriptor;
    std::string                indyName;
    std::string                indyDescriptor;
    std::optional<Handle>      indyBootstrap;
    std::vector<ca::core::Any> indyArguments;
    std::optional<ca::core::Any> ldcValue;
};

class RecordingClassVisitor final : public ClassVisitor
{
public:
    RecordingClassVisitor()
        : ClassVisitor(Opcodes::ASM9, nullptr)
    {}

    RecordComponentVisitor* visitRecordComponent(ca::str::Utf8StringRef name,
                                                  ca::str::Utf8StringRef descriptor,
                                                  ca::str::Utf8StringRef) override
    {
        recordName       = text(name);
        recordDescriptor = text(descriptor);
        return &recordVisitor;
    }

    FieldVisitor* visitField(int, ca::str::Utf8StringRef name,
                             ca::str::Utf8StringRef descriptor,
                             ca::str::Utf8StringRef, const ca::core::Any&) override
    {
        declarationFieldName       = text(name);
        declarationFieldDescriptor = text(descriptor);
        return nullptr;
    }

    MethodVisitor* visitMethod(int, ca::str::Utf8StringRef name,
                               ca::str::Utf8StringRef descriptor,
                               ca::str::Utf8StringRef,
                               const std::vector<ca::str::Utf8StringRef>&) override
    {
        declarationMethodName       = text(name);
        declarationMethodDescriptor = text(descriptor);
        return &methodVisitor;
    }

    RecordingRecordComponentVisitor recordVisitor;
    RecordingMethodVisitor          methodVisitor;
    std::string                     recordName;
    std::string                     recordDescriptor;
    std::string                     declarationFieldName;
    std::string                     declarationFieldDescriptor;
    std::string                     declarationMethodName;
    std::string                     declarationMethodDescriptor;
};

}   // namespace

TEST(RemapperMemberHookTest, mapsMemberDeclarationsReferencesRecordAndInvokeDynamic)
{
    ca::str::Utf8StringArena remapArena;
    MemberHookRemapper     remapper{remapArena};
    RecordingClassVisitor  output;
    ClassRemapper          classRemapper(&output, &remapper);

    classRemapper.visit(Opcodes::V17, Opcodes::ACC_PUBLIC, S("old/Owner"), {},
                        S("java/lang/Object"), {});
    classRemapper.visitField(Opcodes::ACC_PUBLIC, S("oldField"), S("LOld/Type;"), {}, {});
    auto* record = classRemapper.visitRecordComponent(S("oldComponent"), S("LOld/Type;"), {});
    ASSERT_NE(record, nullptr);

    auto* method = classRemapper.visitMethod(Opcodes::ACC_PUBLIC,
                                             S("oldMethod"),
                                             S("(LOld/Type;)LOld/Type;"),
                                             {},
                                             {});
    ASSERT_NE(method, nullptr);
    method->visitFieldInsn(Opcodes::GETFIELD, S("old/Owner"), S("oldField"), S("LOld/Type;"));
    method->visitMethodInsn(Opcodes::INVOKEVIRTUAL,
                            S("old/Owner"),
                            S("oldMethod"),
                            S("(LOld/Type;)LOld/Type;"),
                            false);
    method->visitInvokeDynamicInsn(
        S("oldDynamic"),
        S("()LOld/Type;"),
        Handle(Opcodes::H_INVOKESTATIC, S("old/Owner"), S("bootstrap"), S("()LOld/Type;"), false),
        {ca::core::Any(Handle(Opcodes::H_GETFIELD,
                              S("old/Owner"),
                              S("oldField"),
                              S("LOld/Type;"),
                              false))});
    method->visitLdcInsn(ca::core::Any(Type::getObjectType(S("old/Type"))));

    EXPECT_EQ(output.recordName, "newComponent");
    EXPECT_EQ(output.recordDescriptor, "Lnew/Type;");
    EXPECT_EQ(output.declarationFieldName, "newField");
    EXPECT_EQ(output.declarationFieldDescriptor, "Lnew/Type;");
    EXPECT_EQ(output.declarationMethodName, "newMethod");
    EXPECT_EQ(output.declarationMethodDescriptor, "(Lnew/Type;)Lnew/Type;");
    EXPECT_EQ(output.methodVisitor.fieldOwner, "new/Owner");
    EXPECT_EQ(output.methodVisitor.fieldName, "newField");
    EXPECT_EQ(output.methodVisitor.fieldDescriptor, "Lnew/Type;");
    EXPECT_EQ(output.methodVisitor.methodOwner, "new/Owner");
    EXPECT_EQ(output.methodVisitor.methodName, "newMethod");
    EXPECT_EQ(output.methodVisitor.methodDescriptor, "(Lnew/Type;)Lnew/Type;");
    EXPECT_EQ(output.methodVisitor.indyName, "newDynamic");
    EXPECT_EQ(output.methodVisitor.indyDescriptor, "()Lnew/Type;");
    ASSERT_TRUE(output.methodVisitor.indyBootstrap.has_value());
    EXPECT_EQ(text(output.methodVisitor.indyBootstrap->getOwner()), "new/Owner");
    EXPECT_EQ(text(output.methodVisitor.indyBootstrap->getName()), "newBootstrap");
    EXPECT_EQ(text(output.methodVisitor.indyBootstrap->getDesc()), "()Lnew/Type;");
    ASSERT_EQ(output.methodVisitor.indyArguments.size(), 1U);
    const auto fieldHandle = output.methodVisitor.indyArguments[0].cast<Handle>();
    EXPECT_EQ(text(fieldHandle.getOwner()), "new/Owner");
    EXPECT_EQ(text(fieldHandle.getName()), "newField");
    EXPECT_EQ(text(fieldHandle.getDesc()), "Lnew/Type;");
    ASSERT_TRUE(output.methodVisitor.ldcValue.has_value());
    ASSERT_TRUE(output.methodVisitor.ldcValue->is<Type>());
    // #921 staticArena 退役：无参 getInternalName() 已删除，检视走注入版（本测试 remapArena）。
    EXPECT_EQ(text(output.methodVisitor.ldcValue->cast<Type>().getInternalName(remapArena)),
              "new/Type");

    delete method;
    delete record;
}

TEST(RemapperMemberHookTest, mapsTypeHandleAndConstantDynamicValuesWithoutChangingTheirKinds)
{
    ca::str::Utf8StringArena remapArena;
    MemberHookRemapper remapper{remapArena};

    const auto mappedType = remapper.mapValue(ca::core::Any(Type::getObjectType(S("old/Type"))));
    ASSERT_TRUE(mappedType.is<Type>());
    // #921 staticArena 退役：同上，检视走注入版（本测试 remapArena）。
    EXPECT_EQ(text(mappedType.cast<Type>().getInternalName(remapArena)), "new/Type");

    const auto mappedHandle = remapper.mapValue(ca::core::Any(
        Handle(Opcodes::H_GETFIELD, S("old/Owner"), S("oldField"), S("LOld/Type;"), false)));
    ASSERT_TRUE(mappedHandle.is<Handle>());
    EXPECT_EQ(text(mappedHandle.cast<Handle>().getOwner()), "new/Owner");
    EXPECT_EQ(text(mappedHandle.cast<Handle>().getName()), "newField");
    EXPECT_EQ(text(mappedHandle.cast<Handle>().getDesc()), "Lnew/Type;");

    const ConstantDynamic original(
        S("oldDynamic"),
        S("LOld/Type;"),
        Handle(Opcodes::H_INVOKESTATIC, S("old/Owner"), S("bootstrap"), S("()LOld/Type;"), false),
        {ca::core::Any(Type::getObjectType(S("old/Type")))});
    const auto mappedDynamic = remapper.mapValue(ca::core::Any(original));
    ASSERT_TRUE(mappedDynamic.is<ConstantDynamic>());
    const auto dynamic = mappedDynamic.cast<ConstantDynamic>();
    EXPECT_EQ(text(dynamic.getName()), "newDynamic");
    EXPECT_EQ(text(dynamic.getDescriptor()), "Lnew/Type;");
    EXPECT_EQ(text(dynamic.getBootstrapMethod().getOwner()), "new/Owner");
    EXPECT_EQ(text(dynamic.getBootstrapMethod().getName()), "newBootstrap");
    EXPECT_EQ(text(dynamic.getBootstrapMethod().getDesc()), "()Lnew/Type;");
    ASSERT_EQ(dynamic.getBootstrapMethodArguments().size(), 1U);
    ASSERT_TRUE(dynamic.getBootstrapMethodArguments()[0].is<Type>());
    // #921 staticArena 退役：同上，检视走注入版（本测试 remapArena）。
    EXPECT_EQ(text(dynamic.getBootstrapMethodArguments()[0].cast<Type>().getInternalName(remapArena)),
              "new/Type");
}
