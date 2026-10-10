#include <gtest/gtest.h>
#include "jbytecode/core/primitives.hpp"

using namespace jbytecode::core;

namespace {

Label* newLabel(int bytecodeOffset)
{
    Label* label          = new Label();
    label->bytecodeOffset = bytecodeOffset;
    return label;
}

Handler* newHandler(int startPc, int endPc)
{
    return new Handler(newLabel(startPc), newLabel(endPc), newLabel(0), 0, "");
}

}   // namespace

TEST(HandlerTest, testConstructor)
{
    Label       startPc;
    Label       endPc;
    Label       handlerPc;
    int         catchType       = 123;
    std::string catchDescriptor = "123";

    Handler handler(&startPc, &endPc, &handlerPc, catchType, catchDescriptor.c_str());

    ASSERT_EQ(&startPc, handler.startPc);
    ASSERT_EQ(&endPc, handler.endPc);
    ASSERT_EQ(&handlerPc, handler.handlerPc);
    ASSERT_EQ(catchType, handler.catchType);
    ASSERT_EQ(catchDescriptor, handler.catchTypeDescriptor.to_std_string());
}

TEST(HandlerTest, testCopyConstructor)
{
    Label       startPc1;
    Label       endPc1;
    Label       handlerPc;
    int         catchType       = 123;
    std::string catchDescriptor = "123";
    Handler     handler1(&startPc1, &endPc1, &handlerPc, catchType, catchDescriptor.c_str());
    Label       startPc2;
    Label       endPc2;

    Handler handler2(&handler1, &startPc2, &endPc2);

    ASSERT_EQ(&startPc2, handler2.startPc);
    ASSERT_EQ(&endPc2, handler2.endPc);
    ASSERT_EQ(&handlerPc, handler2.handlerPc);
    ASSERT_EQ(catchType, handler2.catchType);
    ASSERT_EQ(catchDescriptor, handler2.catchTypeDescriptor.to_std_string());
}

TEST(HandlerTest, testRemoveRange_removeAllOrNothing)
{
    Handler* handler = newHandler(10, 20);

    ASSERT_EQ(nullptr, Handler::removeRange(nullptr, newLabel(0), newLabel(10)));
    ASSERT_NE(nullptr, Handler::removeRange(handler, newLabel(0), newLabel(10)));
    handler = newHandler(10, 20);
    ASSERT_NE(nullptr, Handler::removeRange(handler, newLabel(20), newLabel(30)));
    handler = newHandler(10, 20);
    ASSERT_NE(nullptr, Handler::removeRange(handler, newLabel(20), nullptr));
    handler = newHandler(10, 20);
    ASSERT_EQ(nullptr, Handler::removeRange(handler, newLabel(0), newLabel(30)));
}

TEST(HandlerTest, testRemoveRange_removeStart)
{
    Handler* original = newHandler(10, 20);
    Handler* handler  = Handler::removeRange(original, newLabel(0), newLabel(15));

    ASSERT_NE(nullptr, handler);
    ASSERT_EQ(15, handler->startPc->bytecodeOffset);
    ASSERT_EQ(20, handler->endPc->bytecodeOffset);
    ASSERT_EQ(nullptr, handler->nextHandler);
}

TEST(HandlerTest, testRemoveRange_removeEnd)
{
    Handler* original = newHandler(10, 20);
    Handler* handler  = Handler::removeRange(original, newLabel(15), newLabel(30));

    ASSERT_NE(nullptr, handler);
    ASSERT_EQ(10, handler->startPc->bytecodeOffset);
    ASSERT_EQ(15, handler->endPc->bytecodeOffset);
    ASSERT_EQ(nullptr, handler->nextHandler);
}

TEST(HandlerTest, testRemoveRange_removeMiddle)
{
    Handler* original = newHandler(10, 20);
    Handler* handler  = Handler::removeRange(original, newLabel(13), newLabel(17));

    ASSERT_NE(nullptr, handler);
    ASSERT_EQ(10, handler->startPc->bytecodeOffset);
    ASSERT_EQ(13, handler->endPc->bytecodeOffset);
    ASSERT_NE(nullptr, handler->nextHandler);
    ASSERT_EQ(17, handler->nextHandler->startPc->bytecodeOffset);
    ASSERT_EQ(20, handler->nextHandler->endPc->bytecodeOffset);
    ASSERT_EQ(nullptr, handler->nextHandler->nextHandler);
}

TEST(HandlerTest, testGetExceptionTableLength)
{
    Handler* handler = newHandler(10, 20);

    ASSERT_EQ(0, Handler::getExceptionTableLength(nullptr));
    ASSERT_EQ(1, Handler::getExceptionTableLength(handler));
}

TEST(HandlerTest, testGetExceptionTableSize)
{
    Handler* original    = newHandler(10, 20);
    Handler* handlerList = Handler::removeRange(original, newLabel(13), newLabel(17));

    ASSERT_EQ(2, Handler::getExceptionTableSize(nullptr));
    ASSERT_EQ(18, Handler::getExceptionTableSize(handlerList));
}

TEST(HandlerTest, testPutExceptionTable)
{
    Handler*   original    = newHandler(10, 20);
    Handler*   handlerList = Handler::removeRange(original, newLabel(13), newLabel(17));
    ByteVector byteVector;

    Handler::putExceptionTable(handlerList, byteVector);

    ASSERT_EQ(18u, byteVector.size());
}
