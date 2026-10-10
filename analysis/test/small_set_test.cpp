#include <gtest/gtest.h>
#include "jbytecode/analysis/small_set.hpp"

using namespace jbytecode::analysis;

TEST(SmallSetTest, emptySet)
{
    SmallSet<int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(0u, s.size());
    EXPECT_FALSE(s.contains(1));
}

TEST(SmallSetTest, singleElement)
{
    SmallSet<int> s(42);
    EXPECT_FALSE(s.empty());
    EXPECT_EQ(1u, s.size());
    EXPECT_EQ(42, s[0]);
    EXPECT_EQ(42, s.getSingleValue());
    EXPECT_TRUE(s.contains(42));
    EXPECT_FALSE(s.contains(7));
}

TEST(SmallSetTest, twoDistinctElements)
{
    SmallSet<int> s(1, 2);
    EXPECT_EQ(2u, s.size());
    EXPECT_TRUE(s.contains(1));
    EXPECT_TRUE(s.contains(2));
    // getSingleValue 仅对单元素集合有意义，多元素返回 T{}
    EXPECT_EQ(0, s.getSingleValue());
}

TEST(SmallSetTest, twoEqualElementsCollapse)
{
    SmallSet<int> s(5, 5);
    EXPECT_EQ(1u, s.size());
    EXPECT_EQ(5, s.getSingleValue());
}

TEST(SmallSetTest, unionWithEmptyReturnsSelf)
{
    SmallSet<int> a(1, 2);
    SmallSet<int> empty;
    EXPECT_EQ(a, a.union_(empty));
    EXPECT_EQ(a, empty.union_(a));
}

TEST(SmallSetTest, unionDeduplicates)
{
    SmallSet<int> a(1, 2);
    SmallSet<int> b(2, 3);
    SmallSet<int> u = a.union_(b);
    EXPECT_EQ(3u, u.size());
    EXPECT_TRUE(u.contains(1));
    EXPECT_TRUE(u.contains(2));
    EXPECT_TRUE(u.contains(3));
}

TEST(SmallSetTest, equalityIgnoresOrder)
{
    SmallSet<int> a(1, 2);
    SmallSet<int> b(2, 1);
    EXPECT_EQ(a, b);
    EXPECT_FALSE(a != b);
}

TEST(SmallSetTest, inequalityOnDifferentSize)
{
    SmallSet<int> a(1, 2);
    SmallSet<int> b(1);
    EXPECT_NE(a, b);
}
