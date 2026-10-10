#include <gtest/gtest.h>
#include "test_utils.hpp"

// Note: InsnList::clear() and destructor delete all nodes via 'delete'.
// Tests must use heap-allocated nodes to avoid double-free on stack objects.

class InsnListTest : public ::testing::Test
{
protected:
    InsnList list;

    void SetUp() override
    {
        list.add(new InsnNode(0));
        list.add(new InsnNode(1));
    }
};

TEST_F(InsnListTest, emptySize)
{
    InsnList empty;
    EXPECT_EQ(0, empty.size());
}

TEST_F(InsnListTest, emptyFirst)
{
    InsnList empty;
    EXPECT_EQ(nullptr, empty.getFirst());
}

TEST_F(InsnListTest, emptyLast)
{
    InsnList empty;
    EXPECT_EQ(nullptr, empty.getLast());
}

TEST_F(InsnListTest, contains)
{
    auto*    n = new InsnNode(99);
    InsnList l;
    l.add(n);
    EXPECT_TRUE(l.contains(n));
    EXPECT_FALSE(l.contains(new InsnNode(0)));   // not added
}

TEST_F(InsnListTest, indexOf)
{
    ASSERT_EQ(2, list.size());
    EXPECT_EQ(0, list.indexOf(list.getFirst()));
    EXPECT_EQ(1, list.indexOf(list.getLast()));
}

TEST_F(InsnListTest, toArray)
{
    auto arr = list.toArray();
    ASSERT_EQ(2, arr.size());
    EXPECT_EQ(list.get(0), arr[0]);
    EXPECT_EQ(list.get(1), arr[1]);
}

TEST_F(InsnListTest, addSingle)
{
    InsnList l;
    auto*    insn = new InsnNode(42);
    l.add(insn);
    EXPECT_EQ(1, l.size());
    EXPECT_EQ(insn, l.getFirst());
    EXPECT_EQ(insn, l.getLast());
    EXPECT_EQ(insn, l.get(0));
}

TEST_F(InsnListTest, addTwo)
{
    InsnList l;
    auto*    a = new InsnNode(0);
    auto*    b = new InsnNode(1);
    l.add(a);
    l.add(b);
    EXPECT_EQ(2, l.size());
    EXPECT_EQ(a, l.getFirst());
    EXPECT_EQ(b, l.getLast());
    EXPECT_EQ(a, l.get(0));
    EXPECT_EQ(b, l.get(1));
}

TEST_F(InsnListTest, insertBefore)
{
    auto* middle = new InsnNode(99);
    auto* second = list.getLast();   // second element before insert
    list.insertBefore(second, middle);
    EXPECT_EQ(3, list.size());
    EXPECT_EQ(middle, second->getPrevious());
    EXPECT_EQ(second, middle->getNext());
}

TEST_F(InsnListTest, insertAtHead)
{
    auto* head = new InsnNode(-1);
    list.insert(list.getFirst(), head);
    EXPECT_EQ(3, list.size());
    EXPECT_EQ(head, list.getFirst());
}

TEST_F(InsnListTest, set)
{
    auto* replacement = new InsnNode(99);
    auto* old         = list.get(0);
    list.set(old, replacement);
    EXPECT_EQ(2, list.size());
    EXPECT_EQ(replacement, list.getFirst());
}

TEST_F(InsnListTest, remove)
{
    auto* old = list.get(0);
    list.remove(old);
    EXPECT_EQ(1, list.size());
    EXPECT_NE(old, list.getFirst());
}

TEST_F(InsnListTest, clear)
{
    InsnList l;
    l.add(new InsnNode(1));
    l.add(new InsnNode(2));
    l.clear();
    EXPECT_EQ(0, l.size());
    EXPECT_EQ(nullptr, l.getFirst());
}

TEST_F(InsnListTest, iterate)
{
    int count = 0;
    for (auto it = list.begin(); it != list.end(); ++it) count++;
    EXPECT_EQ(2, count);
}

TEST_F(InsnListTest, getByIndex)
{
    EXPECT_NE(nullptr, list.get(0));
    EXPECT_NE(nullptr, list.get(1));
}

TEST_F(InsnListTest, getNullForOutOfBounds)
{
    InsnList l;
    EXPECT_EQ(nullptr, l.get(0));
    EXPECT_EQ(nullptr, l.get(-1));
}

TEST_F(InsnListTest, linkedListNavigation)
{
    auto* first = list.getFirst();
    auto* last  = list.getLast();
    EXPECT_EQ(nullptr, first->getPrevious());
    EXPECT_EQ(last, first->getNext());
    EXPECT_EQ(first, last->getPrevious());
    EXPECT_EQ(nullptr, last->getNext());
}

TEST_F(InsnListTest, isEmpty)
{
    InsnList l;
    EXPECT_TRUE(l.isEmpty());
    l.add(new InsnNode(0));
    EXPECT_FALSE(l.isEmpty());
}
