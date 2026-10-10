#include <gtest/gtest.h>

#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"

using namespace jbytecode::core;
using namespace ca::str;

namespace {

constexpr int LOCAL1 = 1;
constexpr int LOCAL2 = 2;
constexpr int LOCAL3 = 3;
constexpr int LOCAL4 = 4;
constexpr int LOCAL5 = 5;

using ControlFlowGraph = std::map<std::string, std::set<std::string>>;

struct MethodInfo
{
    int maxStack  = -1;
    int maxLocals = -1;
};

struct MaxsMethodVisitor : MethodVisitor
{
    MethodInfo* methodInfo;

    explicit MaxsMethodVisitor(MethodInfo* methodInfo)
        : MethodVisitor(Opcodes::ASM5)
        , methodInfo(methodInfo)
    {}

    void visitMaxs(int maxStack, int maxLocals) override
    {
        methodInfo->maxStack  = maxStack;
        methodInfo->maxLocals = maxLocals;
    }
};

struct MaxsClassVisitor : ClassVisitor
{
    MethodInfo*       methodInfo;
    MaxsMethodVisitor methodVisitor;

    explicit MaxsClassVisitor(MethodInfo* methodInfo)
        : ClassVisitor(Opcodes::ASM5)
        , methodInfo(methodInfo)
        , methodVisitor(methodInfo)
    {}

    MethodVisitor* visitMethod(int, ca::str::Utf8StringRef name, ca::str::Utf8StringRef,
                               ca::str::Utf8StringRef,
                               const std::vector<ca::str::Utf8StringRef>&) override
    {
        if (name.byte_length() == 1 && std::memcmp(name.data(), "m", 1) == 0) {
            return &methodVisitor;
        }
        return nullptr;
    }
};

MethodInfo readMaxStackAndLocals(const std::vector<uint8_t>& classFile)
{
    MethodInfo               methodInfo;
    ca::str::Utf8StringArena arena;
    ClassReader              classReader(arena, classFile);
    MaxsClassVisitor         classVisitor(&methodInfo);
    classReader.accept(&classVisitor, 0);
    return methodInfo;
}

ControlFlowGraph controlFlowGraph(std::initializer_list<const char*> nodes)
{
    ControlFlowGraph graph;
    for (const char* node : nodes) {
        std::string           spec(node);
        auto                  equals = spec.find('=');
        std::string           key    = spec.substr(0, equals);
        std::set<std::string> values;
        if (equals != std::string::npos) {
            size_t start = equals + 1;
            while (start < spec.size()) {
                auto comma = spec.find(',', start);
                values.insert(
                    spec.substr(start, comma == std::string::npos ? comma : comma - start));
                if (comma == std::string::npos) break;
                start = comma + 1;
            }
        }
        graph[key] = values;
    }
    return graph;
}

class TestCaseBuilder
{
public:
    explicit TestCaseBuilder(int classVersion = Opcodes::V1_1)
        : classWriter(arena_, ClassWriter::COMPUTE_MAXS)
    {
        classWriter.visit(classVersion,
                          Opcodes::ACC_PUBLIC,
                          ZUtf8StringRef::from_static("C"),
                          {},
                          ZUtf8StringRef::from_static("java/lang/Object"),
                          {});
        MethodVisitor* constructor = classWriter.visitMethod(Opcodes::ACC_PUBLIC,
                                                             ZUtf8StringRef::from_static("<init>"),
                                                             ZUtf8StringRef::from_static("()V"),
                                                             {},
                                                             {});
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
        methodVisitor = classWriter.visitMethod(Opcodes::ACC_PUBLIC,
                                                ZUtf8StringRef::from_static("m"),
                                                ZUtf8StringRef::from_static("()V"),
                                                {},
                                                {});
        methodVisitor->visitCode();
        label(&start);
    }

    TestCaseBuilder& nop()
    {
        methodVisitor->visitInsn(Opcodes::NOP);
        return *this;
    }
    TestCaseBuilder& push() { return iconst_0(); }
    TestCaseBuilder& push2()
    {
        methodVisitor->visitInsn(Opcodes::LCONST_0);
        return *this;
    }
    TestCaseBuilder& pop()
    {
        methodVisitor->visitInsn(Opcodes::POP);
        return *this;
    }
    TestCaseBuilder& iconst_0()
    {
        methodVisitor->visitInsn(Opcodes::ICONST_0);
        return *this;
    }
    TestCaseBuilder& istore(int varIndex)
    {
        methodVisitor->visitVarInsn(Opcodes::ISTORE, varIndex);
        return *this;
    }
    TestCaseBuilder& aload(int varIndex)
    {
        methodVisitor->visitVarInsn(Opcodes::ALOAD, varIndex);
        return *this;
    }
    TestCaseBuilder& iload(int varIndex)
    {
        methodVisitor->visitVarInsn(Opcodes::ILOAD, varIndex);
        return *this;
    }
    TestCaseBuilder& astore(int varIndex)
    {
        methodVisitor->visitVarInsn(Opcodes::ASTORE, varIndex);
        return *this;
    }
    TestCaseBuilder& ret(int varIndex)
    {
        methodVisitor->visitVarInsn(Opcodes::RET, varIndex);
        return *this;
    }
    TestCaseBuilder& athrow()
    {
        methodVisitor->visitInsn(Opcodes::ATHROW);
        return *this;
    }
    TestCaseBuilder& aconst_null()
    {
        methodVisitor->visitInsn(Opcodes::ACONST_NULL);
        return *this;
    }
    TestCaseBuilder& vreturn()
    {
        methodVisitor->visitInsn(Opcodes::RETURN);
        return *this;
    }
    TestCaseBuilder& label(Label* target)
    {
        methodVisitor->visitLabel(target);
        return *this;
    }
    TestCaseBuilder& iinc(int varIndex, int increment)
    {
        methodVisitor->visitIincInsn(varIndex, increment);
        return *this;
    }
    TestCaseBuilder& frame(int type, int numLocal, const std::vector<ca::core::Any>& local,
                           int numStack, const std::vector<ca::core::Any>& stack)
    {
        methodVisitor->visitFrame(type, numLocal, local, numStack, stack);
        return *this;
    }
    TestCaseBuilder& go(Label* target)
    {
        methodVisitor->visitJumpInsn(Opcodes::GOTO, target);
        return *this;
    }
    TestCaseBuilder& jsr(Label* target)
    {
        methodVisitor->visitJumpInsn(Opcodes::JSR, target);
        return *this;
    }
    TestCaseBuilder& ifnonnull(Label* target)
    {
        methodVisitor->visitJumpInsn(Opcodes::IFNONNULL, target);
        return *this;
    }
    TestCaseBuilder& ifne(Label* target)
    {
        methodVisitor->visitJumpInsn(Opcodes::IFNE, target);
        return *this;
    }
    TestCaseBuilder& trycatch(Label* startLabel, Label* endLabel, Label* handler)
    {
        methodVisitor->visitTryCatchBlock(startLabel, endLabel, handler, {});
        return *this;
    }
    TestCaseBuilder& localVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                   ca::str::Utf8StringRef signature, Label* startLabel,
                                   Label* endLabel, int index)
    {
        methodVisitor->visitLocalVariable(name, descriptor, signature, startLabel, endLabel, index);
        return *this;
    }

    ControlFlowGraph visitMaxs()
    {
        methodVisitor->visitMaxs(0, 0);
        ControlFlowGraph graph;
        Label*           currentLabel = &start;
        while (currentLabel != nullptr) {
            std::string           key = "N" + std::to_string(currentLabel->getOffset());
            std::set<std::string> value;
            Edge*                 outgoingEdge = currentLabel->outgoingEdges;
            if ((currentLabel->flags & Label::FLAG_SUBROUTINE_CALLER) != 0) {
                if (outgoingEdge == nullptr) {
                    throw std::logic_error("Missing virtual JSR edge");
                }
                outgoingEdge = outgoingEdge->nextEdge;
            }
            while (outgoingEdge != nullptr) {
                value.insert("N" + std::to_string(outgoingEdge->successor->getOffset()));
                outgoingEdge = outgoingEdge->nextEdge;
            }
            graph[key]   = value;
            currentLabel = currentLabel->nextBasicBlock;
        }
        return graph;
    }

    std::vector<uint8_t> build()
    {
        methodVisitor->visitEnd();
        classWriter.visitEnd();
        return classWriter.toByteArray();
    }

private:
    ca::str::Utf8StringArena arena_;   // 须先于 classWriter 声明（初始化顺序 = 声明顺序）
    ClassWriter    classWriter;
    MethodVisitor* methodVisitor = nullptr;
    Label          start;
};

}   // namespace

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_basic)
{
    for (int numSubroutines : {1, 31, 32, 33}) {
        TestCaseBuilder                     testCase;
        std::vector<std::unique_ptr<Label>> labels;
        auto                                newLabel = [&]() {
            labels.push_back(std::make_unique<Label>());
            return labels.back().get();
        };
        for (int i = 0; i < numSubroutines; ++i) {
            Label* k0 = newLabel();
            Label* k1 = newLabel();
            Label* k2 = newLabel();
            Label* k3 = newLabel();
            Label* k4 = newLabel();
            testCase.iconst_0()
                .istore(1)
                .label(k0)
                .iinc(1, 1)
                .go(k3)
                .label(k1)
                .astore(3)
                .jsr(k2)
                .aload(3)
                .athrow()
                .label(k2)
                .astore(2)
                .iinc(1, -1)
                .push()
                .push()
                .ret(2)
                .label(k3)
                .jsr(k2)
                .push()
                .push()
                .label(k4)
                .vreturn()
                .trycatch(k0, k1, k1)
                .trycatch(k3, k4, k1);
        }

        ControlFlowGraph     graph     = testCase.visitMaxs();
        std::vector<uint8_t> classFile = testCase.build();

        MethodInfo methodInfo = readMaxStackAndLocals(classFile);
        EXPECT_EQ(4, methodInfo.maxStack);
        EXPECT_EQ(4, methodInfo.maxLocals);
        if (numSubroutines == 1) {
            EXPECT_EQ(controlFlowGraph({"N0=N2",
                                        "N2=N22,N8",
                                        "N8=N14",
                                        "N12=",
                                        "N14=N12,N25",
                                        "N22=N14,N8",
                                        "N25=N27,N8",
                                        "N27="}),
                      graph);
        }
    }
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_ifElseInFinally)
{
    Label           l0, l1, l2, l3, l4, l5, l6;
    TestCaseBuilder testCase;
    testCase.iconst_0()
        .istore(1)
        .label(&l0)
        .iinc(1, 1)
        .go(&l5)
        .label(&l1)
        .astore(3)
        .jsr(&l2)
        .push()
        .push()
        .aload(3)
        .athrow()
        .label(&l2)
        .astore(2)
        .push()
        .push()
        .iload(1)
        .ifne(&l3)
        .iinc(1, 2)
        .go(&l4)
        .label(&l3)
        .iinc(1, 3)
        .label(&l4)
        .ret(2)
        .label(&l5)
        .jsr(&l2)
        .label(&l6)
        .vreturn()
        .trycatch(&l0, &l1, &l1)
        .trycatch(&l5, &l6, &l1);
    ControlFlowGraph graph      = testCase.visitMaxs();
    MethodInfo       methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(5, methodInfo.maxStack);
    EXPECT_EQ(4, methodInfo.maxLocals);
    EXPECT_EQ(controlFlowGraph({"N0=N2",
                                "N2=N34,N8",
                                "N8=N16",
                                "N12=",
                                "N16=N29,N32",
                                "N29=N32",
                                "N32=N37,N12",
                                "N34=N16,N8",
                                "N37="}),
              graph);
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_simpleNestedFinally)
{
    Label           l0, l1, l2, l3, l4, l5;
    TestCaseBuilder testCase;
    testCase.iconst_0()
        .istore(1)
        .label(&l0)
        .iinc(1, 1)
        .jsr(&l2)
        .go(&l5)
        .label(&l1)
        .astore(4)
        .jsr(&l2)
        .aload(4)
        .athrow()
        .label(&l2)
        .astore(2)
        .iinc(1, 2)
        .jsr(&l4)
        .push()
        .push()
        .ret(2)
        .label(&l3)
        .astore(5)
        .jsr(&l4)
        .aload(5)
        .athrow()
        .label(&l4)
        .astore(3)
        .push()
        .push()
        .iinc(1, 3)
        .ret(3)
        .label(&l5)
        .vreturn()
        .trycatch(&l0, &l1, &l1)
        .trycatch(&l2, &l3, &l3);
    ControlFlowGraph graph      = testCase.visitMaxs();
    MethodInfo       methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(5, methodInfo.maxStack);
    EXPECT_EQ(6, methodInfo.maxLocals);
    EXPECT_EQ(controlFlowGraph({"N0=N2",
                                "N2=N11,N19",
                                "N8=N11,N46",
                                "N11=N19",
                                "N16=",
                                "N19=N30,N38",
                                "N26=N16,N30,N8",
                                "N30=N38",
                                "N35=",
                                "N38=N26,N35",
                                "N46="}),
              graph);
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_subroutineWithNoRet)
{
    Label           l0, l1, l2, l3, l4;
    TestCaseBuilder testCase;
    testCase.iconst_0()
        .istore(1)
        .label(&l0)
        .iinc(1, 1)
        .jsr(&l2)
        .go(&l3)
        .label(&l1)
        .astore(2)
        .jsr(&l2)
        .push()
        .push()
        .aload(2)
        .athrow()
        .label(&l2)
        .astore(3)
        .iinc(1, 2)
        .go(&l4)
        .label(&l3)
        .go(&l0)
        .label(&l4)
        .vreturn()
        .trycatch(&l0, &l1, &l1);
    ControlFlowGraph graph      = testCase.visitMaxs();
    MethodInfo       methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(1, methodInfo.maxStack);
    EXPECT_EQ(4, methodInfo.maxLocals);
    EXPECT_EQ(
        controlFlowGraph(
            {"N0=N2", "N2=N11,N19", "N8=N11,N26", "N11=N19", "N15=", "N19=N29", "N26=N2", "N29="}),
        graph);
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_subroutineWithNoRet2)
{
    Label           l0, l1;
    TestCaseBuilder testCase;
    testCase.aconst_null()
        .jsr(&l0)
        .nop()
        .label(&l0)
        .astore(0)
        .astore(0)
        .vreturn()
        .label(&l1)
        .localVariable(
            ZUtf8StringRef::from_static("i"), ZUtf8StringRef::from_static("I"), {}, &l0, &l1, 1);
    ControlFlowGraph graph      = testCase.visitMaxs();
    MethodInfo       methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(2, methodInfo.maxStack);
    EXPECT_EQ(2, methodInfo.maxLocals);
    EXPECT_EQ(controlFlowGraph({"N0=N5", "N4=N5", "N5=", "N8="}), graph);
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_implicitExit)
{
    Label           l0, l1, l2, l3, l4, l5;
    TestCaseBuilder testCase;
    testCase.iconst_0()
        .istore(1)
        .label(&l0)
        .aconst_null()
        .ifnonnull(&l5)
        .label(&l1)
        .iinc(1, 1)
        .jsr(&l3)
        .go(&l4)
        .label(&l2)
        .astore(2)
        .jsr(&l3)
        .aload(2)
        .push()
        .push()
        .athrow()
        .label(&l3)
        .astore(3)
        .iinc(1, 2)
        .go(&l5)
        .label(&l4)
        .go(&l1)
        .label(&l5)
        .vreturn()
        .trycatch(&l1, &l2, &l2);
    ControlFlowGraph graph      = testCase.visitMaxs();
    MethodInfo       methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(1, methodInfo.maxStack);
    EXPECT_EQ(4, methodInfo.maxLocals);
    EXPECT_EQ(controlFlowGraph({"N0=N2",
                                "N2=N6,N33",
                                "N6=N23,N15",
                                "N12=N30,N15",
                                "N15=N23",
                                "N19=",
                                "N23=N33",
                                "N30=N6",
                                "N33="}),
              graph);
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_implicitExitToAnotherSubroutine)
{
    Label           l0, l1, l2, l3, l4, l5, l6, l7;
    TestCaseBuilder testCase;
    testCase.iconst_0()
        .istore(1)
        .label(&l0)
        .jsr(&l2)
        .vreturn()
        .label(&l1)
        .astore(LOCAL2)
        .jsr(&l2)
        .push()
        .push()
        .aload(LOCAL2)
        .athrow()
        .label(&l2)
        .astore(LOCAL4)
        .push()
        .push()
        .go(&l6)
        .label(&l3)
        .jsr(&l5)
        .vreturn()
        .label(&l4)
        .astore(LOCAL3)
        .push()
        .push()
        .jsr(&l5)
        .aload(LOCAL3)
        .athrow()
        .label(&l5)
        .astore(LOCAL5)
        .iload(LOCAL1)
        .ifne(&l7)
        .ret(LOCAL5)
        .label(&l6)
        .iload(LOCAL1)
        .ifne(&l3)
        .label(&l7)
        .ret(LOCAL4)
        .trycatch(&l0, &l1, &l1)
        .trycatch(&l3, &l4, &l4);
    ControlFlowGraph graph      = testCase.visitMaxs();
    MethodInfo       methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(5, methodInfo.maxStack);
    EXPECT_EQ(6, methodInfo.maxLocals);
    EXPECT_EQ(controlFlowGraph({"N0=N2",
                                "N2=N6,N14",
                                "N5=N6",
                                "N6=N14",
                                "N10=",
                                "N14=N41",
                                "N21=N25,N33",
                                "N24=N25",
                                "N25=N33",
                                "N31=",
                                "N33=N31,N45,N24",
                                "N41=N45,N21",
                                "N45=N5,N10"}),
              graph);
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_implicitExitToAnotherSubroutine2)
{
    Label           l0, l1, l2;
    TestCaseBuilder testCase;
    testCase.iconst_0()
        .istore(1)
        .jsr(&l0)
        .vreturn()
        .label(&l0)
        .astore(2)
        .jsr(&l1)
        .go(&l2)
        .label(&l1)
        .astore(3)
        .iload(1)
        .ifne(&l2)
        .ret(3)
        .label(&l2)
        .ret(2);
    ControlFlowGraph graph      = testCase.visitMaxs();
    MethodInfo       methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(1, methodInfo.maxStack);
    EXPECT_EQ(4, methodInfo.maxLocals);
    EXPECT_EQ(controlFlowGraph({"N0=N6", "N5=", "N6=N13", "N10=N20", "N13=N20,N10", "N20=N5"}),
              graph);
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_interleavedCode)
{
    Label           l0, l1, l2, l3;
    TestCaseBuilder testCase;
    testCase.iconst_0()
        .istore(1)
        .jsr(&l0)
        .go(&l1)
        .label(&l0)
        .astore(2)
        .iinc(1, 1)
        .go(&l2)
        .label(&l1)
        .iinc(1, 2)
        .go(&l3)
        .label(&l2)
        .iinc(1, 4)
        .push()
        .push()
        .ret(2)
        .label(&l3)
        .push()
        .push()
        .vreturn();
    ControlFlowGraph graph      = testCase.visitMaxs();
    MethodInfo       methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(4, methodInfo.maxStack);
    EXPECT_EQ(3, methodInfo.maxLocals);
    EXPECT_EQ(controlFlowGraph({"N0=N8", "N5=N15", "N8=N21", "N15=N28", "N21=N5", "N28="}), graph);
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_implicitExitInTryCatch)
{
    Label           l0, l1, l2, l3, l4, l5, l6, l7, l8;
    TestCaseBuilder testCase;
    testCase.iconst_0()
        .istore(1)
        .label(&l0)
        .jsr(&l2)
        .vreturn()
        .label(&l1)
        .astore(LOCAL2)
        .jsr(&l2)
        .aload(LOCAL2)
        .athrow()
        .label(&l2)
        .astore(LOCAL4)
        .go(&l6)
        .label(&l3)
        .jsr(&l5)
        .push()
        .push()
        .vreturn()
        .label(&l4)
        .astore(LOCAL3)
        .jsr(&l5)
        .aload(LOCAL3)
        .athrow()
        .label(&l5)
        .astore(LOCAL5)
        .iload(LOCAL1)
        .ifne(&l7)
        .push()
        .push()
        .ret(LOCAL5)
        .label(&l6)
        .iload(LOCAL1)
        .ifne(&l3)
        .label(&l7)
        .ret(LOCAL4)
        .label(&l8)
        .iinc(LOCAL1, 3)
        .vreturn()
        .trycatch(&l0, &l1, &l1)
        .trycatch(&l3, &l4, &l4)
        .trycatch(&l0, &l8, &l8);
    ControlFlowGraph graph      = testCase.visitMaxs();
    MethodInfo       methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(4, methodInfo.maxStack);
    EXPECT_EQ(6, methodInfo.maxLocals);
    EXPECT_EQ(controlFlowGraph({"N0=N2",
                                "N2=N6,N45,N12",
                                "N5=N6,N45",
                                "N6=N45,N12",
                                "N10=N45",
                                "N12=N39,N45",
                                "N17=N23,N45,N29",
                                "N20=N23,N45",
                                "N23=N45,N29",
                                "N27=N45",
                                "N29=N43,N45,N20,N27",
                                "N39=N43,N45,N17",
                                "N43=N45,N5,N10",
                                "N45="}),
              graph);
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_glassFish2CorbaClientDelegateImplExample)
{
    Label           l0, l1, l2, l3, l4, l5, l6, l7, l8, l9, l10, l11, l12;
    TestCaseBuilder testCase;
    testCase.label(&l0)
        .jsr(&l4)
        .label(&l1)
        .go(&l5)
        .label(&l2)
        .pop()
        .jsr(&l4)
        .label(&l3)
        .aconst_null()
        .athrow()
        .label(&l4)
        .astore(1)
        .ret(1)
        .label(&l5)
        .aconst_null()
        .aconst_null()
        .aconst_null()
        .pop()
        .pop()
        .pop()
        .label(&l6)
        .go(&l8)
        .label(&l7)
        .pop()
        .go(&l8)
        .aconst_null()
        .athrow()
        .label(&l8)
        .iconst_0()
        .ifne(&l0)
        .jsr(&l12)
        .label(&l9)
        .vreturn()
        .label(&l10)
        .pop()
        .jsr(&l12)
        .label(&l11)
        .aconst_null()
        .athrow()
        .label(&l12)
        .astore(2)
        .ret(2)
        .trycatch(&l0, &l1, &l2)
        .trycatch(&l2, &l3, &l2)
        .trycatch(&l0, &l6, &l7)
        .trycatch(&l0, &l9, &l10)
        .trycatch(&l10, &l11, &l10);
    ControlFlowGraph graph      = testCase.visitMaxs();
    MethodInfo       methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(3, methodInfo.maxStack);
    EXPECT_EQ(3, methodInfo.maxLocals);
    EXPECT_EQ(controlFlowGraph({"N0=N6,N12,N24,N38",
                                "N3=N15,N24,N38",
                                "N6=N6,N12,N24,N38",
                                "N10=N24,N38",
                                "N12=N3,N10,N24,N38",
                                "N15=N21,N24,N38",
                                "N21=N30,N38",
                                "N24=N30,N38",
                                "N30=N0,N38,N44",
                                "N37=",
                                "N38=N38,N44",
                                "N42=",
                                "N44=N37,N42"}),
              graph);
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_implicitExitToAnotherSubroutineInverted)
{
    Label           l0, l1, l2, l3, l4;
    TestCaseBuilder testCase;
    testCase.go(&l3)
        .label(&l0)
        .astore(2)
        .label(&l1)
        .ret(1)
        .label(&l2)
        .astore(1)
        .aload(0)
        .ifnonnull(&l1)
        .jsr(&l0)
        .aconst_null()
        .aconst_null()
        .aconst_null()
        .vreturn()
        .label(&l3)
        .jsr(&l2)
        .label(&l4)
        .vreturn();
    ControlFlowGraph graph      = testCase.visitMaxs();
    MethodInfo       methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(1, methodInfo.maxStack);
    EXPECT_EQ(3, methodInfo.maxLocals);
    EXPECT_EQ(controlFlowGraph({"N0=N18", "N3=N4", "N4=N21", "N6=N3,N4", "N14=", "N18=N6", "N21="}),
              graph);
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_framesWithDeadCode)
{
    Label           l0;
    TestCaseBuilder testCase(Opcodes::V1_7);
    testCase.vreturn().label(&l0).frame(Opcodes::F_SAME, 0, {}, 0, {}).aconst_null().vreturn();
    testCase.visitMaxs();
    MethodInfo methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(1, methodInfo.maxStack);
    EXPECT_EQ(1, methodInfo.maxLocals);
}

TEST(ClassWriterComputeMaxsTest, testVisitMaxs_frameWithLong)
{
    Label           l0;
    TestCaseBuilder testCase(Opcodes::V1_7);
    testCase.push2()
        .go(&l0)
        .label(&l0)
        .frame(Opcodes::F_NEW, 0, {}, 1, {Opcodes::LONG})
        .aconst_null()
        .vreturn();
    testCase.visitMaxs();
    MethodInfo methodInfo = readMaxStackAndLocals(testCase.build());
    EXPECT_EQ(3, methodInfo.maxStack);
    EXPECT_EQ(1, methodInfo.maxLocals);
}
