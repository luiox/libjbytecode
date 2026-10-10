#include <gtest/gtest.h>
#include "test_utils.hpp"
#include "jbytecode/util/textifier.hpp"
#include "jbytecode/util/trace_class_visitor.hpp"

TEST(TraceClassVisitorTest, testConstructor_unsupportedApi)
{
    jbytecode::util::Textifier textifier;
    EXPECT_NO_THROW(jbytecode::util::TraceClassVisitor tcv(nullptr, &textifier));
}

TEST(TraceClassVisitorTest, testTextifierDirectly)
{
    jbytecode::util::Textifier textifier;
    textifier.visit(Opcodes::V1_8,
                    Opcodes::ACC_PUBLIC,
                    ZUtf8StringRef::from_static("pkg/Class"),
                    Utf8StringRef(),
                    ZUtf8StringRef::from_static("java/lang/Object"),
                    {});
    EXPECT_FALSE(textifier.text.empty());
}

TEST(TraceClassVisitorTest, testVisitRecordComponentSupported)
{
    jbytecode::util::Textifier         textifier;
    jbytecode::util::TraceClassVisitor tcv(nullptr, &textifier);

    EXPECT_NO_THROW({
        auto* rcv = tcv.visitRecordComponent(ZUtf8StringRef::from_static("value"),
                                             ZUtf8StringRef::from_static("Ljava/lang/String;"),
                                             Utf8StringRef());
        ASSERT_NE(rcv, nullptr);
        rcv->visitEnd();
        delete rcv;
    });
}
