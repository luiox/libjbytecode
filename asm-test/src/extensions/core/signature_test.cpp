#include <gtest/gtest.h>
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

// === SignatureReader Tests ===

TEST(SignatureReaderTest, readClassSignature)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("Ljava/lang/Object;"));
    sr.accept(&sw);
    ASSERT_EQ("Ljava/lang/Object;", sw.toString());
}

TEST(SignatureReaderTest, readSimpleGenericClass)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("Ljava/util/List<TV;>;"));
    sr.accept(&sw);
    ASSERT_EQ("Ljava/util/List<TV;>;", sw.toString());
}

TEST(SignatureReaderTest, readMultiTypeParamClass)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("<T:Ljava/lang/Object;>Ljava/util/List<TT;>;"));
    sr.accept(&sw);
    ASSERT_EQ("<T:Ljava/lang/Object;>Ljava/util/List<TT;>;", sw.toString());
}

TEST(SignatureReaderTest, readMethodSignature)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("(Ljava/util/List<Ljava/lang/String;>;)V"));
    sr.accept(&sw);
    ASSERT_EQ("(Ljava/util/List<Ljava/lang/String;>;)V", sw.toString());
}

TEST(SignatureReaderTest, readGenericMethodWithBounds)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("<T:Ljava/lang/Object;>(TT;)TT;"));
    sr.accept(&sw);
    ASSERT_EQ("<T:Ljava/lang/Object;>(TT;)TT;", sw.toString());
}

TEST(SignatureReaderTest, readArrayTypeSignature)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("[I"));
    sr.acceptType(&sw);
    ASSERT_EQ("[I", sw.toString());
}

TEST(SignatureReaderTest, readMultiDimensionalArray)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("[[Ljava/lang/String;"));
    sr.acceptType(&sw);
    ASSERT_EQ("[[Ljava/lang/String;", sw.toString());
}

TEST(SignatureReaderTest, readTypeVariable)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("TT;"));
    sr.acceptType(&sw);
    ASSERT_EQ("TT;", sw.toString());
}

TEST(SignatureReaderTest, readBaseType)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("I"));
    sr.acceptType(&sw);
    ASSERT_EQ("I", sw.toString());
}

TEST(SignatureReaderTest, readWildcardTypeArgument)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("Ljava/util/List<*>;"));
    sr.accept(&sw);
    ASSERT_EQ("Ljava/util/List<*>;", sw.toString());
}

TEST(SignatureReaderTest, readExtendsWildcard)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("Ljava/util/List<+Ljava/lang/Number;>;"));
    sr.accept(&sw);
    ASSERT_EQ("Ljava/util/List<+Ljava/lang/Number;>;", sw.toString());
}

TEST(SignatureReaderTest, readSuperWildcard)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("Ljava/util/List<-Ljava/lang/Integer;>;"));
    sr.accept(&sw);
    ASSERT_EQ("Ljava/util/List<-Ljava/lang/Integer;>;", sw.toString());
}

TEST(SignatureReaderTest, readThrowsSignature)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("()V^Ljava/io/IOException;"));
    sr.accept(&sw);
    ASSERT_EQ("()V^Ljava/io/IOException;", sw.toString());
}

TEST(SignatureReaderTest, readInnerClass)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("Ljava/util/Map.Entry;"));
    sr.accept(&sw);
    ASSERT_EQ("Ljava/util/Map.Entry;", sw.toString());
}

TEST(SignatureReaderTest, readComplexNestedGeneric)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("Ljava/util/Map<TK;TV;>.Entry<TK;TV;>;"));
    sr.accept(&sw);
    ASSERT_EQ("Ljava/util/Map<TK;TV;>.Entry<TK;TV;>;", sw.toString());
}

TEST(SignatureReaderTest, readMapEntryInnerClass)
{
    SignatureWriter sw;
    SignatureReader sr(ZUtf8StringRef::from_static("Ljava/util/Map$Entry;"));
    sr.accept(&sw);
    ASSERT_EQ("Ljava/util/Map$Entry;", sw.toString());
}

// === SignatureWriter Tests ===

TEST(SignatureWriterTest, writeSimpleClass)
{
    SignatureWriter sw;
    sw.visitClassType(ZUtf8StringRef::from_static("java/lang/Object"));
    sw.visitEnd();
    ASSERT_EQ("Ljava/lang/Object;", sw.toString());
}

TEST(SignatureWriterTest, writeBaseType)
{
    SignatureWriter sw;
    sw.visitBaseType('I');
    ASSERT_EQ("I", sw.toString());
}

TEST(SignatureWriterTest, writeArrayType)
{
    SignatureWriter sw;
    sw.visitArrayType();
    sw.visitBaseType('I');
    ASSERT_EQ("[I", sw.toString());
}

TEST(SignatureWriterTest, writeTypeVariable)
{
    SignatureWriter sw;
    sw.visitTypeVariable(ZUtf8StringRef::from_static("T"));
    ASSERT_EQ("TT;", sw.toString());
}

TEST(SignatureWriterTest, writeSimpleParameterizedType)
{
    SignatureWriter sw;
    sw.visitClassType(ZUtf8StringRef::from_static("java/util/List"));
    sw.visitTypeArgument('=');
    sw.visitClassType(ZUtf8StringRef::from_static("java/lang/String"));
    sw.visitEnd();   // closes String
    sw.visitEnd();   // closes List
    ASSERT_EQ("Ljava/util/List<Ljava/lang/String;>;", sw.toString());
}

TEST(SignatureWriterTest, writeParameterizedTypeWithTVar)
{
    SignatureWriter sw;
    sw.visitClassType(ZUtf8StringRef::from_static("java/util/List"));
    sw.visitTypeArgument('=');
    sw.visitTypeVariable(ZUtf8StringRef::from_static("T"));
    sw.visitEnd();   // closes List and its type arguments
    ASSERT_EQ("Ljava/util/List<TT;>;", sw.toString());
}

TEST(SignatureWriterTest, writeGenericClass)
{
    SignatureWriter sw;
    sw.visitFormalTypeParameter(ZUtf8StringRef::from_static("T"));
    sw.visitClassBound();
    sw.visitClassType(ZUtf8StringRef::from_static("java/lang/Object"));
    sw.visitEnd();
    sw.visitSuperclass();
    sw.visitClassType(ZUtf8StringRef::from_static("java/util/List"));
    sw.visitTypeArgument('=');
    sw.visitTypeVariable(ZUtf8StringRef::from_static("T"));
    sw.visitEnd();   // closes List (including type arguments)
    ASSERT_EQ("<T:Ljava/lang/Object;>Ljava/util/List<TT;>;", sw.toString());
}

TEST(SignatureWriterTest, writeWildcardStar)
{
    SignatureWriter sw;
    sw.visitClassType(ZUtf8StringRef::from_static("java/util/List"));
    sw.visitTypeArgument();
    sw.visitEnd();
    ASSERT_EQ("Ljava/util/List<*>;", sw.toString());
}

TEST(SignatureWriterTest, writeExtendsWildcard)
{
    SignatureWriter sw;
    sw.visitClassType(ZUtf8StringRef::from_static("java/util/List"));
    sw.visitTypeArgument('+');
    sw.visitClassType(ZUtf8StringRef::from_static("java/lang/Number"));
    sw.visitEnd();
    sw.visitEnd();
    ASSERT_EQ("Ljava/util/List<+Ljava/lang/Number;>;", sw.toString());
}

TEST(SignatureWriterTest, writeSuperWildcard)
{
    SignatureWriter sw;
    sw.visitClassType(ZUtf8StringRef::from_static("java/util/List"));
    sw.visitTypeArgument('-');
    sw.visitClassType(ZUtf8StringRef::from_static("java/lang/Integer"));
    sw.visitEnd();
    sw.visitEnd();
    ASSERT_EQ("Ljava/util/List<-Ljava/lang/Integer;>;", sw.toString());
}

TEST(SignatureWriterTest, writeMethodSignature)
{
    SignatureWriter sw;
    sw.visitParameterType();
    sw.visitClassType(ZUtf8StringRef::from_static("java/lang/String"));
    sw.visitEnd();
    sw.visitReturnType();
    sw.visitBaseType('V');
    ASSERT_EQ("(Ljava/lang/String;)V", sw.toString());
}

TEST(SignatureWriterTest, writeThrows)
{
    SignatureWriter sw;
    sw.visitParameterType();
    sw.visitBaseType('I');
    sw.visitReturnType();
    sw.visitClassType(ZUtf8StringRef::from_static("java/io/IOException"));
    sw.visitEnd();
    sw.visitExceptionType();
    sw.visitClassType(ZUtf8StringRef::from_static("java/lang/Exception"));
    sw.visitEnd();
    ASSERT_EQ("(I)Ljava/io/IOException;^Ljava/lang/Exception;", sw.toString());
}

TEST(SignatureWriterTest, writeInnerClassType)
{
    SignatureWriter sw;
    sw.visitClassType(ZUtf8StringRef::from_static("java/util/Map"));
    sw.visitInnerClassType(ZUtf8StringRef::from_static("Entry"));
    sw.visitEnd();
    // Signature format uses '.' for inner class (not '$')
    ASSERT_EQ("Ljava/util/Map.Entry;", sw.toString());
}

TEST(SignatureReaderWriterTest, roundtripClassSignature)
{
    const char* inputs[] = {"Ljava/lang/Object;",
                            "Ljava/util/List<TV;>;",
                            "<T:Ljava/lang/Object;>Ljava/util/List<TT;>;",
                            "(Ljava/util/List<Ljava/lang/String;>;)V",
                            "<T:Ljava/lang/Object;>(TT;)TT;",
                            "()V^Ljava/io/IOException;",
                            "Ljava/util/Map.Entry;",
                            nullptr};
    for (int i = 0; inputs[i] != nullptr; ++i) {
        SignatureWriter sw;
        SignatureReader sr(Utf8StringRef::from_cstr(inputs[i]));
        sr.accept(&sw);
        EXPECT_STREQ(inputs[i], sw.toString().c_str());
    }
}

TEST(SignatureReaderWriterTest, roundtripTypeSignature)
{
    const char* inputs[] = {
        "[I", "[[Ljava/lang/String;", "TT;", "I", "V", "Ljava/lang/Integer;", nullptr};
    for (int i = 0; inputs[i] != nullptr; ++i) {
        SignatureWriter sw;
        SignatureReader sr(Utf8StringRef::from_cstr(inputs[i]));
        sr.acceptType(&sw);
        EXPECT_STREQ(inputs[i], sw.toString().c_str());
    }
}
