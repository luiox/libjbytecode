#include <gtest/gtest.h>
#include <chrono>
#include <set>
#include <string>
#include <vector>
#include "jbytecode/core/constants.hpp"
#include "jbytecode/core/writers.hpp"
#include "libca/str/utf8_string.hpp"

using namespace jbytecode::core;
using namespace ca::str;

TEST(MethodWriterTest, testCanCopyMethodAttributesUpdated)
{
    auto actualAttributes = Constants::allAttributeNames();
    ASSERT_EQ(30u, actualAttributes.size());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("ConstantValue")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("Code")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("StackMapTable")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("Exceptions")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("InnerClasses")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("EnclosingMethod")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("Synthetic")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("Signature")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("SourceFile")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("SourceDebugExtension")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("LineNumberTable")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("LocalVariableTable")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("LocalVariableTypeTable")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("Deprecated")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("RuntimeVisibleAnnotations")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("RuntimeInvisibleAnnotations")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static(
                    "RuntimeVisibleParameterAnnotations")) != actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static(
                    "RuntimeInvisibleParameterAnnotations")) != actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static(
                    "RuntimeVisibleTypeAnnotations")) != actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static(
                    "RuntimeInvisibleTypeAnnotations")) != actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("AnnotationDefault")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("BootstrapMethods")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("MethodParameters")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("Module")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("ModulePackages")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("ModuleMainClass")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("NestHost")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("NestMembers")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("PermittedSubclasses")) !=
                actualAttributes.end());
    EXPECT_TRUE(actualAttributes.find(ZUtf8StringRef::from_static("Record")) !=
                actualAttributes.end());
}

TEST(MethodWriterTest, testRecursiveCondyFastEnough)
{
    Handle          bsm(Opcodes::H_INVOKESTATIC,
               ZUtf8StringRef::from_static("RT"),
               ZUtf8StringRef::from_static("bsm"),
               ZUtf8StringRef::from_static("(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/"
                                                    "String;Ljava/lang/Class;I)Ljava/lang/invoke/CallSite;"),
               false);
    ConstantDynamic chain(
        ZUtf8StringRef::from_static("condy"), ZUtf8StringRef::from_static("I"), bsm, {0});
    /// Utf8StringRef 是非拥有视图：names 扩容重分配会连带销毁 std::string 的 SSO 缓冲，
    /// 指向其中的视图随即悬挂，SymbolTable 递归登记嵌套 condy 时 hash 悬垂 name 即
    /// heap-use-after-free。每个名字必须在视图仍有效的当下 intern 进 arena（池内副本
    /// 地址稳定、与 arena 同生命周期），视图生存期才覆盖全部使用点。
    Utf8StringArena          arena;
    std::vector<std::string> names;
    for (int i = 0; i < 32; ++i) {
        names.push_back("condy" + std::to_string(i));
        chain = ConstantDynamic(arena.intern(names.back().c_str()),
                                ZUtf8StringRef::from_static("I"),
                                bsm,
                                {chain});
    }
    ConstantDynamic condy = chain;

    auto        start = std::chrono::steady_clock::now();
    ClassWriter classWriter(arena, 0);
    classWriter.visit(Opcodes::V11,
                      Opcodes::ACC_SUPER,
                      ZUtf8StringRef::from_static("Test"),
                      ZUtf8StringRef::from_static(""),
                      ZUtf8StringRef::from_static("java/lang/Object"),
                      {});
    MethodVisitor* methodVisitor = classWriter.visitMethod(Opcodes::ACC_STATIC,
                                                           ZUtf8StringRef::from_static("m"),
                                                           ZUtf8StringRef::from_static("()V"),
                                                           ZUtf8StringRef::from_static(""),
                                                           {});
    methodVisitor->visitCode();
    methodVisitor->visitLdcInsn(condy);
    methodVisitor->visitMaxs(0, 0);
    methodVisitor->visitEnd();
    classWriter.visitEnd();
    classWriter.toByteArray();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, std::chrono::seconds(1));
}
