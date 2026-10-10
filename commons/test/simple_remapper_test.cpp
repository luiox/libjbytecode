#include <gtest/gtest.h>
#include <unordered_map>
#include <string>
#include <cstring>
#include "test_utils.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"


using namespace ca::str;


TEST(SimpleRemapperTest, testMapSignature_remapParentOnly_nestedClassExtends)
{
    std::string inputSignature                           = "LOuter<Ljava/lang/Object;>.Inner;";
    std::unordered_map<std::string, std::string> mapping = {{"Outer", "RenamedOuter"}};
    Utf8StringArena                              arena;
    SimpleRemapper                               remapper(arena, mapping);
    Utf8StringRef result = remapper.mapSignature(Utf8StringRef::from_cstr(inputSignature.c_str()));
    ASSERT_EQ(result, Utf8StringRef::from_cstr("LRenamedOuter<Ljava/lang/Object;>.Inner;"));
}

TEST(SimpleRemapperTest, testMapSignature_remapChildOnly_nestedClassExtends)
{
    std::string inputSignature                           = "LOuter<Ljava/lang/Object;>.Inner;";
    std::unordered_map<std::string, std::string> mapping = {{"Outer$Inner", "Outer$RenamedInner"}};
    Utf8StringArena                              arena;
    SimpleRemapper                               remapper(arena, mapping);
    Utf8StringRef result = remapper.mapSignature(Utf8StringRef::from_cstr(inputSignature.c_str()));
    ASSERT_EQ(result, Utf8StringRef::from_cstr("LOuter<Ljava/lang/Object;>.RenamedInner;"));
}

TEST(SimpleRemapperTest, testMapSignature_remapBothParentAndChild_nestedClassExtends)
{
    std::string inputSignature                           = "LOuter<Ljava/lang/Object;>.Inner;";
    std::unordered_map<std::string, std::string> mapping = {
        {"Outer", "RenamedOuter"}, {"Outer$Inner", "RenamedOuter$RenamedInner"}};
    Utf8StringArena arena;
    SimpleRemapper  remapper(arena, mapping);
    Utf8StringRef  result = remapper.mapSignature(Utf8StringRef::from_cstr(inputSignature.c_str()));
    ASSERT_EQ(result, Utf8StringRef::from_cstr("LRenamedOuter<Ljava/lang/Object;>.RenamedInner;"));
}
