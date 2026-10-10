#include <gtest/gtest.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/exceptions.hpp"
#include "jbytecode/core/writers.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

namespace {

// 全测试套共享 arena（gtest 串行，intern 累积无害）；writer/reader 必传 arena 后的便捷处理。
ca::str::Utf8StringArena arena;

void initializeEmptyClass(ClassWriter& classWriter)
{
    classWriter.visit(Opcodes::V1_1,
                      Opcodes::ACC_PUBLIC,
                      ZUtf8StringRef::from_static("C"),
                      ca::str::Utf8StringRef{},
                      ZUtf8StringRef::from_static("java/lang/Object"),
                      {});
}

std::vector<uint8_t> readSampleClass()
{
    auto bytes = readClassFile("DefaultPackage.class");
    if (bytes.empty()) bytes = readClassFile("origin/Issue307600.class");
    return bytes;
}

class NestHostRecorder : public ClassVisitor
{
public:
    ca::str::Utf8StringRef host;

    NestHostRecorder()
        : ClassVisitor(Opcodes::ASM9)
    {}

    void visitNestHost(ca::str::Utf8StringRef nestHost) override { host = nestHost; }
};

/// Returns attribute prototypes matching Java's ClassWriterTest.attributes().
Attribute* testAttributePrototypes()
{
    static CommentAttribute     comment;
    static CodeCommentAttribute codeComment;
    comment.nextAttribute     = &codeComment;
    codeComment.nextAttribute = nullptr;
    return &comment;
}

bool containsAscii(const std::vector<uint8_t>& bytes, const std::string& needle)
{
    return std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end()) != bytes.end();
}

std::string getConstantPoolDump(ClassWriter& classWriter)
{
    return ::getConstantPoolDump(classWriter.toByteArray());
}

std::string getDump(ClassWriter& classWriter)
{
    return getClassFileDump(classWriter.toByteArray());
}

struct MethodDeadCodeInserter : MethodVisitor
{
    bool                                inserted = false;
    std::vector<std::unique_ptr<Label>> ownedLabels;

    explicit MethodDeadCodeInserter(MethodVisitor* methodVisitor)
        : MethodVisitor(Opcodes::ASM9, methodVisitor)
    {}

    void visitInsn(int opcode) override
    {
        MethodVisitor::visitInsn(opcode);
        maybeInsertDeadCode();
    }

    void visitIntInsn(int opcode, int operand) override
    {
        MethodVisitor::visitIntInsn(opcode, operand);
        maybeInsertDeadCode();
    }

    void visitVarInsn(int opcode, int varIndex) override
    {
        MethodVisitor::visitVarInsn(opcode, varIndex);
        maybeInsertDeadCode();
    }

    void visitTypeInsn(int opcode, ca::str::Utf8StringRef type) override
    {
        MethodVisitor::visitTypeInsn(opcode, type);
        maybeInsertDeadCode();
    }

    void visitFieldInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef descriptor) override
    {
        MethodVisitor::visitFieldInsn(opcode, owner, name, descriptor);
        maybeInsertDeadCode();
    }

    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor, bool isInterface) override
    {
        MethodVisitor::visitMethodInsn(opcode, owner, name, descriptor, isInterface);
        maybeInsertDeadCode();
    }

    void visitInvokeDynamicInsn(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                const Handle&                     bootstrapMethodHandle,
                                const std::vector<ca::core::Any>& bootstrapMethodArguments) override
    {
        MethodVisitor::visitInvokeDynamicInsn(
            name, descriptor, bootstrapMethodHandle, bootstrapMethodArguments);
        maybeInsertDeadCode();
    }

    void visitJumpInsn(int opcode, Label* label) override
    {
        MethodVisitor::visitJumpInsn(opcode, label);
        maybeInsertDeadCode();
    }

    void visitLdcInsn(const ca::core::Any& value) override
    {
        MethodVisitor::visitLdcInsn(value);
        maybeInsertDeadCode();
    }

    void visitIincInsn(int varIndex, int increment) override
    {
        MethodVisitor::visitIincInsn(varIndex, increment);
        maybeInsertDeadCode();
    }

    void visitTableSwitchInsn(int min, int max, Label* dflt,
                              const std::vector<Label*>& labels) override
    {
        MethodVisitor::visitTableSwitchInsn(min, max, dflt, labels);
        maybeInsertDeadCode();
    }

    void visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                               const std::vector<Label*>& labels) override
    {
        MethodVisitor::visitLookupSwitchInsn(dflt, keys, labels);
        maybeInsertDeadCode();
    }

    void visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor, int numDimensions) override
    {
        MethodVisitor::visitMultiANewArrayInsn(descriptor, numDimensions);
        maybeInsertDeadCode();
    }

    void visitMaxs(int maxStack, int maxLocals) override
    {
        if (!inserted) insertDeadCode();
        MethodVisitor::visitMaxs(maxStack, maxLocals);
    }

    void maybeInsertDeadCode()
    {
        if (!inserted) insertDeadCode();
    }

    void insertDeadCode()
    {
        ownedLabels.push_back(std::make_unique<Label>());
        Label* end = ownedLabels.back().get();
        MethodVisitor::visitJumpInsn(Opcodes::GOTO, end);
        MethodVisitor::visitLdcInsn(std::string("DEAD CODE"));
        MethodVisitor::visitLabel(end);
        inserted = true;
    }
};

struct DeadCodeInserter : ClassVisitor
{
    std::vector<std::unique_ptr<MethodDeadCodeInserter>> methods;

    explicit DeadCodeInserter(ClassVisitor* classVisitor)
        : ClassVisitor(Opcodes::ASM9, classVisitor)
    {}

    void visit(int version, int access, ca::str::Utf8StringRef name,
               ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override
    {
        ClassVisitor::visit((version & 0xFFFF) < Opcodes::V1_7 ? Opcodes::V1_7 : version,
                            access,
                            name,
                            signature,
                            superName,
                            interfaces);
    }

    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name,
                               ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        MethodVisitor* next =
            ClassVisitor::visitMethod(access, name, descriptor, signature, exceptions);
        methods.push_back(std::make_unique<MethodDeadCodeInserter>(next));
        return methods.back().get();
    }
};

}   // namespace

TEST(ClassWriterTest, testNewConst)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newConst(0);
    classWriter.newConst(1);
    classWriter.newConst(50);
    classWriter.newConst(3);

    std::string constantPoolDump = getConstantPoolDump(classWriter);
    ASSERT_TRUE(constantPoolDump.find("constant_pool: 0") != std::string::npos);
    ASSERT_TRUE(constantPoolDump.find("constant_pool: 1") != std::string::npos);
    ASSERT_TRUE(constantPoolDump.find("constant_pool: 50") != std::string::npos);
    ASSERT_TRUE(constantPoolDump.find("constant_pool: 3") != std::string::npos);
}

TEST(ClassWriterTest, testNewConst_illegalArgument)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    try {
        classWriter.newConst(std::vector<int>{});
        FAIL() << "Expected std::invalid_argument";
    }
    catch (const std::invalid_argument& e) {
        EXPECT_STREQ("Unsupported constant value", e.what());
    }
}

TEST(ClassWriterTest, testNewUtf8)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newUTF8(ZUtf8StringRef::from_static("A"));

    ASSERT_TRUE(getConstantPoolDump(classWriter).find("constant_pool: A") != std::string::npos);
}

TEST(ClassWriterTest, testNewClass)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newClass(ZUtf8StringRef::from_static("A"));

    ASSERT_TRUE(getConstantPoolDump(classWriter).find("constant_pool: ConstantClassInfo A") !=
                std::string::npos);
}

TEST(ClassWriterTest, testNewMethodType)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newMethodType(ZUtf8StringRef::from_static("()V"));

    ASSERT_TRUE(
        getConstantPoolDump(classWriter).find("constant_pool: ConstantMethodTypeInfo ()V") !=
        std::string::npos);
}

TEST(ClassWriterTest, testNewModule)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newModule(ZUtf8StringRef::from_static("A"));

    ASSERT_TRUE(getConstantPoolDump(classWriter).find("constant_pool: ConstantModuleInfo A") !=
                std::string::npos);
}

TEST(ClassWriterTest, testNewPackage)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newPackage(ZUtf8StringRef::from_static("A"));

    ASSERT_TRUE(getConstantPoolDump(classWriter).find("constant_pool: ConstantPackageInfo A") !=
                std::string::npos);
}

TEST(ClassWriterTest, testNewHandle)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newHandle(Opcodes::H_GETFIELD,
                          ZUtf8StringRef::from_static("A"),
                          ZUtf8StringRef::from_static("h"),
                          ZUtf8StringRef::from_static("I"),
                          false);

    ASSERT_TRUE(getConstantPoolDump(classWriter)
                    .find("constant_pool: ConstantMethodHandleInfo 1.ConstantFieldRefInfo A.hI") !=
                std::string::npos);
}

TEST(ClassWriterTest, testDeprecatedNewHandle)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newHandle(Opcodes::H_GETFIELD,
                          ZUtf8StringRef::from_static("A"),
                          ZUtf8StringRef::from_static("h"),
                          ZUtf8StringRef::from_static("I"));

    ASSERT_TRUE(getConstantPoolDump(classWriter)
                    .find("constant_pool: ConstantMethodHandleInfo 1.ConstantFieldRefInfo A.hI") !=
                std::string::npos);
}

TEST(ClassWriterTest, testNewHandleIsInterface)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    int index1 = classWriter.newHandle(Opcodes::H_INVOKEVIRTUAL,
                                       ZUtf8StringRef::from_static("A"),
                                       ZUtf8StringRef::from_static("m"),
                                       ZUtf8StringRef::from_static("()V"),
                                       false);
    int index2 = classWriter.newHandle(Opcodes::H_INVOKEVIRTUAL,
                                       ZUtf8StringRef::from_static("A"),
                                       ZUtf8StringRef::from_static("m"),
                                       ZUtf8StringRef::from_static("()V"),
                                       true);

    ASSERT_NE(index1, index2);
}

TEST(ClassWriterTest, testNewConstantDynamic)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newConstantDynamic(ZUtf8StringRef::from_static("m"),
                                   ZUtf8StringRef::from_static("Ljava/lang/String;"),
                                   Handle(Opcodes::H_INVOKESTATIC,
                                          ZUtf8StringRef::from_static("A"),
                                          ZUtf8StringRef::from_static("m"),
                                          ZUtf8StringRef::from_static("()V"),
                                          false));

    std::string constantPoolDump = getConstantPoolDump(classWriter);
    ASSERT_TRUE(constantPoolDump.find("constant_pool: ConstantDynamicInfo 0.mLjava/lang/String;") !=
                std::string::npos);
    ASSERT_TRUE(constantPoolDump.find(
                    "constant_pool: ConstantMethodHandleInfo 6.ConstantMethodRefInfo A.m()V") !=
                std::string::npos);
    ASSERT_TRUE(constantPoolDump.find("constant_pool: BootstrapMethods") != std::string::npos);
}

TEST(ClassWriterTest, testNewInvokeDynamic)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newInvokeDynamic(ZUtf8StringRef::from_static("m"),
                                 ZUtf8StringRef::from_static("()V"),
                                 Handle(Opcodes::H_GETFIELD,
                                        ZUtf8StringRef::from_static("A"),
                                        ZUtf8StringRef::from_static("h"),
                                        ZUtf8StringRef::from_static("I"),
                                        false));

    std::string constantPoolDump = getConstantPoolDump(classWriter);
    ASSERT_TRUE(constantPoolDump.find("ConstantInvokeDynamicInfo 0.m()V") != std::string::npos);
    ASSERT_TRUE(constantPoolDump.find(
                    "constant_pool: ConstantMethodHandleInfo 1.ConstantFieldRefInfo A.hI") !=
                std::string::npos);
    ASSERT_TRUE(constantPoolDump.find("constant_pool: BootstrapMethods") != std::string::npos);
}

TEST(ClassWriterTest, testNewField)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newField(ZUtf8StringRef::from_static("A"),
                         ZUtf8StringRef::from_static("f"),
                         ZUtf8StringRef::from_static("I"));

    ASSERT_TRUE(getConstantPoolDump(classWriter).find("constant_pool: ConstantFieldRefInfo A.fI") !=
                std::string::npos);
}

TEST(ClassWriterTest, testNewMethod)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newMethod(ZUtf8StringRef::from_static("A"),
                          ZUtf8StringRef::from_static("m"),
                          ZUtf8StringRef::from_static("()V"),
                          false);

    ASSERT_TRUE(
        getConstantPoolDump(classWriter).find("constant_pool: ConstantMethodRefInfo A.m()V") !=
        std::string::npos);
}

TEST(ClassWriterTest, testNewNameType)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);

    classWriter.newNameType(ZUtf8StringRef::from_static("m"), ZUtf8StringRef::from_static("()V"));

    ASSERT_TRUE(
        getConstantPoolDump(classWriter).find("constant_pool: ConstantNameAndTypeInfo m()V") !=
        std::string::npos);
}
TEST(ClassWriterTest, testInstanceFields)
{
    // ASM original uses Java reflection to assert the exact non-static field set of ClassWriter.
    // C++ has no equivalent portable runtime field reflection here; this keeps the expected field
    // contract documented and checks the high-risk state names explicitly for future replacement.
    const std::set<std::string> expectedFields = {"pool",
                                                  "symbols",
                                                  "version",
                                                  "access",
                                                  "thisClass",
                                                  "className",
                                                  "superClass",
                                                  "interfaceIndices",
                                                  "sourceFileIndex",
                                                  "signatureIndex",
                                                  "nestHost",
                                                  "outerClass",
                                                  "outerClassIndex",
                                                  "outerMethodIndex",
                                                  "fields",
                                                  "methods",
                                                  "visibleAnnotations",
                                                  "invisibleAnnotations",
                                                  "visibleTypeAnnotations",
                                                  "invisibleTypeAnnotations",
                                                  "ownedAnns",
                                                  "innerClasses",
                                                  "classAttrs",
                                                  "recordComponents",
                                                  "nestMembers",
                                                  "permittedSubclasses",
                                                  "sourceDebug",
                                                  "computeMaxs",
                                                  "computeFrames",
                                                  "typeResolver",
                                                  "moduleWriter",
                                                  "bootstrapMethods",
                                                  "bsmCache"};

    ASSERT_EQ(33u, expectedFields.size());
    ASSERT_EQ(1u, expectedFields.count("computeMaxs"));
    ASSERT_EQ(1u, expectedFields.count("computeFrames"));
    ASSERT_EQ(1u, expectedFields.count("typeResolver"));
    ASSERT_EQ(1u, expectedFields.count("permittedSubclasses"));
}

TEST(ClassWriterTest, testGetCommonSuperClass)
{
    ClassWriter classWriter(arena, 0);

    EXPECT_EQ("java/lang/Object",
              classWriter
                  .getCommonSuperClass(ZUtf8StringRef::from_static("java/lang/Object"),
                                       ZUtf8StringRef::from_static("java/lang/Integer"))
                  .to_std_string());
    EXPECT_EQ("java/lang/Object",
              classWriter
                  .getCommonSuperClass(ZUtf8StringRef::from_static("java/lang/Integer"),
                                       ZUtf8StringRef::from_static("java/lang/Object"))
                  .to_std_string());
    EXPECT_EQ("java/lang/Object",
              classWriter
                  .getCommonSuperClass(ZUtf8StringRef::from_static("java/lang/Integer"),
                                       ZUtf8StringRef::from_static("java/lang/Runnable"))
                  .to_std_string());
    EXPECT_EQ("java/lang/Object",
              classWriter
                  .getCommonSuperClass(ZUtf8StringRef::from_static("java/lang/Runnable"),
                                       ZUtf8StringRef::from_static("java/lang/Integer"))
                  .to_std_string());
    EXPECT_EQ(
        "java/lang/Throwable",
        classWriter
            .getCommonSuperClass(ZUtf8StringRef::from_static("java/lang/IndexOutOfBoundsException"),
                                 ZUtf8StringRef::from_static("java/lang/AssertionError"))
            .to_std_string());
    EXPECT_THROW(
        {
            try {
                classWriter.getCommonSuperClass(ZUtf8StringRef::from_static("-"),
                                                ZUtf8StringRef::from_static("java/lang/Object"));
            }
            catch (const std::runtime_error& exception) {
                EXPECT_STREQ("Type - not present", exception.what());
                throw;
            }
        },
        std::runtime_error);
    EXPECT_THROW(
        {
            try {
                classWriter.getCommonSuperClass(ZUtf8StringRef::from_static("java/lang/Object"),
                                                ZUtf8StringRef::from_static("-"));
            }
            catch (const std::runtime_error& exception) {
                EXPECT_STREQ("Type - not present", exception.what());
                throw;
            }
        },
        std::runtime_error);
}
TEST(ClassWriterTest, testToByteArray_constantPoolSizeTooLarge)
{
    constexpr int sizes[] = {65535, 65536};
    for (int constantPoolCount : sizes) {
        ClassWriter classWriter(arena, 0);
        initializeEmptyClass(classWriter);
        constexpr int initConstantPoolCount = 5;
        for (int i = 0; i < constantPoolCount - initConstantPoolCount; ++i) {
            classWriter.newConst(i);
        }
        if (constantPoolCount > 65535) {
            EXPECT_THROW(
                {
                    try {
                        classWriter.toByteArray();
                    }
                    catch (const ClassTooLargeException& exception) {
                        EXPECT_EQ("C", exception.getClassName());
                        EXPECT_EQ(constantPoolCount, exception.getConstantPoolCount());
                        EXPECT_STREQ("Class too large: C", exception.what());
                        throw;
                    }
                },
                ClassTooLargeException);
        }
        else {
            EXPECT_NO_THROW(classWriter.toByteArray());
        }
    }
}

TEST(ClassWriterTest, testToByteArray_methodCodeSizeTooLarge)
{
    constexpr int sizes[] = {65535, 65536};
    for (int methodCodeSize : sizes) {
        ClassWriter classWriter(arena, 0);
        initializeEmptyClass(classWriter);
        MethodVisitor* methodVisitor = classWriter.visitMethod(Opcodes::ACC_STATIC,
                                                               ZUtf8StringRef::from_static("m"),
                                                               ZUtf8StringRef::from_static("()V"),
                                                               ca::str::Utf8StringRef{},
                                                               {});
        ASSERT_NE(nullptr, methodVisitor);
        methodVisitor->visitCode();
        for (int i = 0; i < methodCodeSize - 1; ++i) {
            methodVisitor->visitInsn(Opcodes::NOP);
        }
        methodVisitor->visitInsn(Opcodes::RETURN);
        methodVisitor->visitMaxs(0, 0);
        methodVisitor->visitEnd();

        if (methodCodeSize > 65535) {
            EXPECT_THROW(
                {
                    try {
                        classWriter.toByteArray();
                    }
                    catch (const MethodTooLargeException& exception) {
                        EXPECT_EQ("m", exception.getMethodName());
                        EXPECT_EQ("C", exception.getClassName());
                        EXPECT_EQ("()V", exception.getDescriptor());
                        EXPECT_EQ(methodCodeSize, exception.getCodeSize());
                        EXPECT_STREQ("Method too large: C.m ()V", exception.what());
                        throw;
                    }
                },
                MethodTooLargeException);
        }
        else {
            EXPECT_NO_THROW(classWriter.toByteArray());
        }
    }
}

TEST(ClassWriterTest, testToByteArray_largeSourceDebugExtension)
{
    ClassWriter classWriter(arena, 0);
    initializeEmptyClass(classWriter);
    std::string debugExtension(100000, '\0');
    classWriter.visitSource(ZUtf8StringRef::from_static("Test.java"),
                            Utf8StringRef(reinterpret_cast<const ca::u8*>(debugExtension.data()),
                                          debugExtension.size(),
                                          debugExtension.size()));

    classWriter.toByteArray();

    ASSERT_TRUE(getDump(classWriter).find("attribute_name_index: SourceDebugExtension") !=
                std::string::npos);
}
TEST(ClassWriterTest, testToByteArray_computeMaxs_largeSubroutines)
{
    for (const auto* classFileName : {"origin/Issue307600.class", "origin/Issue311642.class"}) {
        auto classFile = readClassFile(classFileName);
        ASSERT_FALSE(classFile.empty()) << classFileName;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, ClassWriter::COMPUTE_MAXS);

        classReader.accept(&classWriter, 0);

        ASSERT_NO_THROW(classWriter.toByteArray()) << classFileName;
    }
}
TEST(ClassWriterTest, testToByteArray_computeFrames_mergeLongOrDouble)
{
    ClassWriter classWriter(arena, ClassWriter::COMPUTE_FRAMES);
    classWriter.visit(Opcodes::V1_7,
                      Opcodes::ACC_PUBLIC,
                      ZUtf8StringRef::from_static("A"),
                      ca::str::Utf8StringRef{},
                      ZUtf8StringRef::from_static("java/lang/Object"),
                      {});
    MethodVisitor* methodVisitor = classWriter.visitMethod(Opcodes::ACC_PUBLIC,
                                                           ZUtf8StringRef::from_static("<init>"),
                                                           ZUtf8StringRef::from_static("()V"),
                                                           ca::str::Utf8StringRef{},
                                                           {});
    ASSERT_NE(nullptr, methodVisitor);
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

    methodVisitor = classWriter.visitMethod(Opcodes::ACC_STATIC,
                                            ZUtf8StringRef::from_static("m"),
                                            ZUtf8StringRef::from_static("(J)V"),
                                            ca::str::Utf8StringRef{},
                                            {});
    ASSERT_NE(nullptr, methodVisitor);
    methodVisitor->visitCode();
    methodVisitor->visitInsn(Opcodes::ICONST_0);
    Label label;
    methodVisitor->visitJumpInsn(Opcodes::IFNE, &label);
    methodVisitor->visitInsn(Opcodes::ICONST_0);
    methodVisitor->visitVarInsn(Opcodes::ISTORE, 1);
    methodVisitor->visitLabel(&label);
    methodVisitor->visitInsn(Opcodes::RETURN);
    methodVisitor->visitMaxs(0, 0);
    methodVisitor->visitEnd();
    classWriter.visitEnd();

    ASSERT_FALSE(classWriter.toByteArray().empty());
}

TEST(ClassWriterTest, testToByteArray_computeFrames_highDimensionArrays)
{
    ClassWriter classWriter(arena, ClassWriter::COMPUTE_FRAMES);
    classWriter.visit(Opcodes::V1_7,
                      Opcodes::ACC_PUBLIC,
                      ZUtf8StringRef::from_static("A"),
                      ca::str::Utf8StringRef{},
                      ZUtf8StringRef::from_static("java/lang/Object"),
                      {});
    MethodVisitor* methodVisitor = classWriter.visitMethod(
        Opcodes::ACC_STATIC,
        ZUtf8StringRef::from_static("m"),
        ZUtf8StringRef::from_static(
            "(I[[[[[[[[Ljava/lang/Integer;[[[[[[[[Ljava/lang/Long;)Ljava/lang/Object;"),
        ca::str::Utf8StringRef{},
        {});
    ASSERT_NE(nullptr, methodVisitor);
    methodVisitor->visitCode();
    methodVisitor->visitVarInsn(Opcodes::ILOAD, 0);
    Label thenLabel;
    Label endIfLabel;
    methodVisitor->visitJumpInsn(Opcodes::IFNE, &thenLabel);
    methodVisitor->visitVarInsn(Opcodes::ALOAD, 1);
    methodVisitor->visitJumpInsn(Opcodes::GOTO, &endIfLabel);
    methodVisitor->visitLabel(&thenLabel);
    methodVisitor->visitVarInsn(Opcodes::ALOAD, 2);
    methodVisitor->visitLabel(&endIfLabel);
    methodVisitor->visitInsn(Opcodes::ARETURN);
    methodVisitor->visitMaxs(0, 0);
    methodVisitor->visitEnd();
    classWriter.visitEnd();

    auto classFile = classWriter.toByteArray();

    ASSERT_TRUE(containsAscii(classFile, "[[[[[[[[Ljava/lang/Number;"));
}
TEST(ClassWriterTest, testToByteArray_manyFramesWithForwardLabelReferences)
{
    ClassWriter classWriter(arena, 0);
    classWriter.visit(Opcodes::V1_7,
                      Opcodes::ACC_PUBLIC,
                      ZUtf8StringRef::from_static("A"),
                      ca::str::Utf8StringRef{},
                      ZUtf8StringRef::from_static("java/lang/Object"),
                      {});
    MethodVisitor* constructor = classWriter.visitMethod(Opcodes::ACC_PUBLIC,
                                                         ZUtf8StringRef::from_static("<init>"),
                                                         ZUtf8StringRef::from_static("()V"),
                                                         ca::str::Utf8StringRef{},
                                                         {});
    ASSERT_NE(nullptr, constructor);
    constructor->visitCode();
    constructor->visitVarInsn(Opcodes::ALOAD, 0);
    constructor->visitMethodInsn(Opcodes::INVOKESPECIAL,
                                 ZUtf8StringRef::from_static("java/lang/Object"),
                                 ZUtf8StringRef::from_static("<init>"),
                                 ZUtf8StringRef::from_static("()V"),
                                 false);
    constructor->visitInsn(Opcodes::RETURN);
    constructor->visitMaxs(1, 1);
    constructor->visitEnd();

    MethodVisitor* methodVisitor = classWriter.visitMethod(Opcodes::ACC_STATIC,
                                                           ZUtf8StringRef::from_static("m"),
                                                           ZUtf8StringRef::from_static("()V"),
                                                           ca::str::Utf8StringRef{},
                                                           {});
    ASSERT_NE(nullptr, methodVisitor);
    methodVisitor->visitCode();
    Label label0;
    methodVisitor->visitJumpInsn(Opcodes::GOTO, &label0);
    Label label1;
    methodVisitor->visitLabel(&label1);
    std::vector<Label>         newLabels(24);
    std::vector<ca::core::Any> newLabelFrameTypes;
    for (auto& label : newLabels) {
        newLabelFrameTypes.emplace_back(&label);
    }
    methodVisitor->visitFrame(
        Opcodes::F_NEW, static_cast<int>(newLabelFrameTypes.size()), newLabelFrameTypes, 0, {});
    for (int i = 0; i < static_cast<int>(newLabels.size()); ++i) {
        methodVisitor->visitVarInsn(Opcodes::ALOAD, i);
        methodVisitor->visitMethodInsn(Opcodes::INVOKESPECIAL,
                                       ZUtf8StringRef::from_static("A"),
                                       ZUtf8StringRef::from_static("<init>"),
                                       ZUtf8StringRef::from_static("()V"),
                                       false);
    }
    Label label2;
    methodVisitor->visitJumpInsn(Opcodes::GOTO, &label2);
    methodVisitor->visitLabel(&label0);
    std::vector<ca::core::Any> topTypes(newLabels.size(), Opcodes::TOP);
    methodVisitor->visitFrame(Opcodes::F_NEW, static_cast<int>(topTypes.size()), topTypes, 0, {});
    for (int i = 0; i < static_cast<int>(newLabels.size()); ++i) {
        methodVisitor->visitLabel(&newLabels[i]);
        methodVisitor->visitTypeInsn(Opcodes::NEW, ZUtf8StringRef::from_static("A"));
        methodVisitor->visitVarInsn(Opcodes::ASTORE, i);
    }
    methodVisitor->visitJumpInsn(Opcodes::GOTO, &label1);
    methodVisitor->visitLabel(&label2);
    std::vector<ca::core::Any> newTypes(newLabels.size(), ZUtf8StringRef::from_static("A"));
    methodVisitor->visitFrame(Opcodes::F_NEW, static_cast<int>(newTypes.size()), newTypes, 0, {});
    methodVisitor->visitInsn(Opcodes::RETURN);
    methodVisitor->visitMaxs(1, static_cast<int>(newLabels.size()));
    methodVisitor->visitEnd();
    classWriter.visitEnd();

    ASSERT_FALSE(classWriter.toByteArray().empty());
}
TEST(ClassWriterTest, testReadAndWrite)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, 0);

        classReader.accept(&classWriter, testAttributePrototypes(), 0);

        ASSERT_TRUE(classFilesEquivalent(classFile, classWriter.toByteArray()))
            << classParameter.name;
    }
}

TEST(ClassWriterTest, testReadAndWrite_skipCode)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, 0);

        classReader.accept(&classWriter, ClassReader::SKIP_CODE);

        ASSERT_FALSE(classWriter.hasFlags(ClassWriter::COMPUTE_MAXS));
        ASSERT_FALSE(classWriter.hasFlags(ClassWriter::COMPUTE_FRAMES));
        auto output = classWriter.toByteArray();
        ASSERT_FALSE(output.empty());
        ASSERT_TRUE(getClassFileDump(output).find(classParameter.internalName()) !=
                    std::string::npos)
            << classParameter.name;
    }
}

TEST(ClassWriterTest, testReadAndWrite_copyPool)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, classReader, 0);

        classReader.accept(&classWriter, testAttributePrototypes(), 0);

        ASSERT_TRUE(classFilesEquivalent(classFile, classWriter.toByteArray()))
            << classParameter.name;
    }
}

TEST(ClassWriterTest, testReadAndWrite_expandFrames)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, 0);

        classReader.accept(&classWriter, testAttributePrototypes(), ClassReader::EXPAND_FRAMES);

        ASSERT_TRUE(classFilesEquivalent(classFile, classWriter.toByteArray()))
            << classParameter.name;
    }
}

TEST(ClassWriterTest, testReadAndWrite_computeMaxs)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, ClassWriter::COMPUTE_MAXS);

        classReader.accept(&classWriter, testAttributePrototypes(), 0);

        ASSERT_TRUE(classWriter.hasFlags(ClassWriter::COMPUTE_MAXS));
        ASSERT_FALSE(classWriter.hasFlags(ClassWriter::COMPUTE_FRAMES));
        ASSERT_TRUE(classFilesEquivalent(classFile, classWriter.toByteArray()))
            << classParameter.name;
    }
}

TEST(ClassWriterTest, testReadAndWrite_computeMaxs_newInstance)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, ClassWriter::COMPUTE_MAXS);

        classReader.accept(&classWriter, 0);

        ASSERT_FALSE(classWriter.toByteArray().empty()) << classParameter.name;
    }
}

TEST(ClassWriterTest, testReadAndWrite_computeFrames)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        if (classParameter.hasJsrOrRetInstructions()) continue;
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, ClassWriter::COMPUTE_FRAMES);

        classReader.accept(&classWriter, 0);

        auto newClassFile = classWriter.toByteArray();
        ASSERT_FALSE(classWriter.hasFlags(ClassWriter::COMPUTE_MAXS));
        ASSERT_TRUE(classWriter.hasFlags(ClassWriter::COMPUTE_FRAMES));
        ASSERT_FALSE(newClassFile.empty()) << classParameter.name;
    }
}
TEST(ClassWriterTest, testReadAndWrite_computeFramesAndDeadCode)
{
    auto classFile = readClassFile("jdk8/AllStructures.class");
    ASSERT_FALSE(classFile.empty());
    ClassReader      classReader(arena, classFile);
    ClassWriter      classWriter(arena, ClassWriter::COMPUTE_FRAMES);
    DeadCodeInserter classVisitor(&classWriter);

    classReader.accept(&classVisitor, ClassReader::SKIP_FRAMES);

    auto newClassFile = classWriter.toByteArray();
    ASSERT_FALSE(newClassFile.empty());
    ASSERT_TRUE(containsAscii(newClassFile, "DEAD CODE"));
}
TEST(ClassWriterTest, testReadAndWrite_computeFrames_jsrInstructions)
{
    for (const auto* classFileName : {"jdk3/AllInstructions.class", "jdk3/LargeMethod.class"}) {
        auto classFile = readClassFile(classFileName);
        ASSERT_FALSE(classFile.empty()) << classFileName;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, ClassWriter::COMPUTE_FRAMES);

        EXPECT_THROW(classReader.accept(&classWriter, 0), std::invalid_argument) << classFileName;
    }
}
TEST(ClassWriterTest, testReadAndWrite_skipAndComputeFrames)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        if (classParameter.hasJsrOrRetInstructions()) continue;
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, ClassWriter::COMPUTE_FRAMES);

        classReader.accept(&classWriter, ClassReader::SKIP_FRAMES);

        ASSERT_FALSE(classWriter.toByteArray().empty()) << classParameter.name;
    }
}

TEST(ClassWriterTest, testReadAndWrite_largeMethod)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        if (classFile.size() > static_cast<size_t>(std::numeric_limits<int16_t>::max())) continue;
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader classReader(arena, classFile);
        ClassWriter classWriter(arena, 0);

        classReader.accept(&classWriter, 0);

        ASSERT_FALSE(classWriter.toByteArray().empty()) << classParameter.name;
    }
}
TEST(ClassWriterTest, testVisitMethods_final)
{
    // ASM original uses Java reflection to verify ClassWriter overrides every public ClassVisitor
    // visit method and marks it final. C++ cannot reflect virtual/final method modifiers at
    // runtime; this exercises the same public visitor surface so missing methods still fail at
    // compile time.
    ClassWriter classWriter(arena, 0);

    EXPECT_NO_THROW(classWriter.visit(Opcodes::V1_1,
                                      Opcodes::ACC_PUBLIC,
                                      ZUtf8StringRef::from_static("C"),
                                      {},
                                      ZUtf8StringRef::from_static("java/lang/Object"),
                                      {}));
    EXPECT_NO_THROW(classWriter.visitSource(ZUtf8StringRef::from_static("Test.java"), {}));
    EXPECT_NO_THROW(classWriter.visitOuterClass(ZUtf8StringRef::from_static("Owner"),
                                                ZUtf8StringRef::from_static("name"),
                                                ZUtf8StringRef::from_static("()V")));
    EXPECT_NE(nullptr, classWriter.visitAnnotation(ZUtf8StringRef::from_static("LA;"), true));
    EXPECT_NE(nullptr,
              classWriter.visitField(Opcodes::ACC_PUBLIC,
                                     ZUtf8StringRef::from_static("f"),
                                     ZUtf8StringRef::from_static("I"),
                                     {},
                                     {}));
    EXPECT_NE(nullptr,
              classWriter.visitMethod(Opcodes::ACC_PUBLIC,
                                      ZUtf8StringRef::from_static("m"),
                                      ZUtf8StringRef::from_static("()V"),
                                      {},
                                      {}));
    EXPECT_NO_THROW(classWriter.visitInnerClass(ZUtf8StringRef::from_static("C$Inner"),
                                                ZUtf8StringRef::from_static("C"),
                                                ZUtf8StringRef::from_static("Inner"),
                                                Opcodes::ACC_PUBLIC));
    EXPECT_NO_THROW(classWriter.visitNestHost(ZUtf8StringRef::from_static("C")));
    EXPECT_NO_THROW(classWriter.visitNestMember(ZUtf8StringRef::from_static("C$NestMember")));
    EXPECT_NO_THROW(classWriter.visitPermittedSubclass(ZUtf8StringRef::from_static("C$Permitted")));
    EXPECT_NE(nullptr,
              classWriter.visitRecordComponent(
                  ZUtf8StringRef::from_static("r"), ZUtf8StringRef::from_static("I"), {}));
    EXPECT_NE(nullptr, classWriter.visitModule(ZUtf8StringRef::from_static("m"), 0, {}));
}

TEST(ClassWriterTest, nestHostConstantPoolIsReadable)
{
    ClassWriter classWriter(arena, 0);
    classWriter.visit(Opcodes::V1_8,
                      Opcodes::ACC_PUBLIC,
                      ZUtf8StringRef::from_static("C"),
                      ca::str::Utf8StringRef{},
                      ZUtf8StringRef::from_static("java/lang/Object"),
                      {});
    classWriter.visitNestHost(ZUtf8StringRef::from_static("Host"));
    classWriter.visitEnd();

    auto             bytes = classWriter.toByteArray();
    NestHostRecorder recorder;
    ClassReader(arena, bytes.data(), static_cast<ca::i32>(bytes.size())).accept(&recorder, 0);
    EXPECT_TRUE(recorder.host.equals(ZUtf8StringRef::from_static("Host")));
}
