#include <gtest/gtest.h>
#include <unordered_map>
#include <string>
#include "test_utils.hpp"
#include "libca/str/utf8_string_arena.hpp"

namespace {

// 记录型 ClassVisitor：捕获 visit() 接收的类名/超类名，用于验证 ClassRemapper 映射。
struct RecordingClassVisitor : jbytecode::core::ClassVisitor
{
    RecordingClassVisitor()
        : jbytecode::core::ClassVisitor(Opcodes::ASM9, nullptr)
    {}
    std::string className, superName;

    void visit(int version, int access, ca::str::Utf8StringRef name,
               ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName_,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override
    {
        className = std::string(reinterpret_cast<const char*>(name.data()), name.byte_length());
        superName =
            std::string(reinterpret_cast<const char*>(superName_.data()), superName_.byte_length());
    }
};

}   // namespace

TEST(ClassRemapperTest, remapsClassNameInVisit)
{
    std::unordered_map<std::string, std::string> mapping = {{"OldClass", "NewClass"},
                                                            {"OldSuper", "NewSuper"}};
    ca::str::Utf8StringArena                     arena;
    SimpleRemapper                               remapper(arena, mapping);
    RecordingClassVisitor                        rec;
    ClassRemapper                                cr(&rec, &remapper);

    cr.visit(Opcodes::V1_8,
             Opcodes::ACC_PUBLIC,
             ZUtf8StringRef::from_static("OldClass"),
             Utf8StringRef(),
             ZUtf8StringRef::from_static("OldSuper"),
             {});

    EXPECT_EQ("NewClass", rec.className);
    EXPECT_EQ("NewSuper", rec.superName);
}

TEST(ClassRemapperTest, unmappedNamePassesThrough)
{
    std::unordered_map<std::string, std::string> mapping = {{"Other", "Renamed"}};
    ca::str::Utf8StringArena                     arena;
    SimpleRemapper                               remapper(arena, mapping);
    RecordingClassVisitor                        rec;
    ClassRemapper                                cr(&rec, &remapper);

    cr.visit(Opcodes::V1_8,
             0,
             ZUtf8StringRef::from_static("Unchanged"),
             Utf8StringRef(),
             ZUtf8StringRef::from_static("java/lang/Object"),
             {});

    EXPECT_EQ("Unchanged", rec.className);
    EXPECT_EQ("java/lang/Object", rec.superName);
}
