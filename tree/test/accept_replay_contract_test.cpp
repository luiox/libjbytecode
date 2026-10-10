#include "garena/arena.hpp"
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "test_utils.hpp"

// B8b：测试侧 Label 统一归宿（进程寿命 scratch arena）——LabelNode() 默认构造
// 已随旧池制度退役，Label 一律 arena 分配。
static garena::Arena& labelScratchArena()
{
    static garena::Arena arena;
    return arena;
}


namespace {

std::string text(ca::str::Utf8StringRef value)
{
    return value.to_std_string();
}

class RecordingMethodVisitor final : public MethodVisitor
{
public:
    std::vector<std::string> events;

    RecordingMethodVisitor()
        : MethodVisitor(Opcodes::ASM9)
    {}

    void visitParameter(ca::str::Utf8StringRef name, int access) override
    {
        events.push_back("parameter:" + text(name) + ":" + std::to_string(access));
    }
    AnnotationVisitor* visitAnnotationDefault() override
    {
        events.push_back("annotation-default");
        return nullptr;
    }
    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef desc, bool visible) override
    {
        events.push_back("annotation:" + text(desc) + ":" + (visible ? "visible" : "invisible"));
        return nullptr;
    }
    AnnotationVisitor* visitTypeAnnotation(int                    typeRef, const TypePath*,
                                           ca::str::Utf8StringRef desc, bool visible) override
    {
        events.push_back("type-annotation:" + std::to_string(typeRef) + ":" + text(desc) + ":" +
                         (visible ? "visible" : "invisible"));
        return nullptr;
    }
    AnnotationVisitor* visitParameterAnnotation(int parameter, ca::str::Utf8StringRef desc,
                                                bool visible) override
    {
        events.push_back("parameter-annotation:" + std::to_string(parameter) + ":" + text(desc) +
                         ":" + (visible ? "visible" : "invisible"));
        return nullptr;
    }
    void visitCode() override { events.push_back("code"); }
    void visitFrame(int type, int, const std::vector<ca::core::Any>&, int,
                    const std::vector<ca::core::Any>&) override
    {
        events.push_back("frame:" + std::to_string(type));
    }
    void visitInsn(int opcode) override { events.push_back("insn:" + std::to_string(opcode)); }
    void visitIntInsn(int opcode, int operand) override
    {
        events.push_back("int:" + std::to_string(opcode) + ":" + std::to_string(operand));
    }
    void visitVarInsn(int opcode, int var) override
    {
        events.push_back("var:" + std::to_string(opcode) + ":" + std::to_string(var));
    }
    void visitTypeInsn(int opcode, ca::str::Utf8StringRef type) override
    {
        events.push_back("type:" + std::to_string(opcode) + ":" + text(type));
    }
    void visitFieldInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef desc) override
    {
        events.push_back("field:" + std::to_string(opcode) + ":" + text(owner) + ":" + text(name) +
                         ":" + text(desc));
    }
    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef desc, bool itf) override
    {
        events.push_back("method:" + std::to_string(opcode) + ":" + text(owner) + ":" + text(name) +
                         ":" + text(desc) + ":" + (itf ? "interface" : "class"));
    }
    void visitInvokeDynamicInsn(ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                                const Handle&, const std::vector<ca::core::Any>&) override
    {
        events.push_back("indy:" + text(name) + ":" + text(desc));
    }
    void visitJumpInsn(int opcode, Label*) override
    {
        events.push_back("jump:" + std::to_string(opcode));
    }
    void visitLabel(Label*) override { events.push_back("label"); }
    void visitLdcInsn(const ca::core::Any&) override { events.push_back("ldc"); }
    void visitIincInsn(int var, int increment) override
    {
        events.push_back("iinc:" + std::to_string(var) + ":" + std::to_string(increment));
    }
    void visitTableSwitchInsn(int min, int max, Label*, const std::vector<Label*>& labels) override
    {
        events.push_back("table-switch:" + std::to_string(min) + ":" + std::to_string(max) + ":" +
                         std::to_string(labels.size()));
    }
    void visitLookupSwitchInsn(Label*, const std::vector<int>& keys,
                               const std::vector<Label*>& labels) override
    {
        events.push_back("lookup-switch:" + std::to_string(keys.size()) + ":" +
                         std::to_string(labels.size()));
    }
    void visitMultiANewArrayInsn(ca::str::Utf8StringRef desc, int dims) override
    {
        events.push_back("multi-new-array:" + text(desc) + ":" + std::to_string(dims));
    }
    void visitTryCatchBlock(Label*, Label*, Label*, ca::str::Utf8StringRef type) override
    {
        events.push_back("try-catch:" + text(type));
    }
    void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                            ca::str::Utf8StringRef signature, Label*, Label*, int index) override
    {
        events.push_back("local:" + text(name) + ":" + text(desc) + ":" + text(signature) + ":" +
                         std::to_string(index));
    }
    AnnotationVisitor* visitLocalVariableAnnotation(
        int typeRef, const TypePath*, const std::vector<Label*>& start, const std::vector<Label*>&,
        const std::vector<int>& index, ca::str::Utf8StringRef desc, bool visible) override
    {
        events.push_back("local-annotation:" + std::to_string(typeRef) + ":" +
                         std::to_string(start.size()) + ":" + std::to_string(index.size()) + ":" +
                         text(desc) + ":" + (visible ? "visible" : "invisible"));
        return nullptr;
    }
    void visitLineNumber(int line, Label*) override
    {
        events.push_back("line:" + std::to_string(line));
    }
    void visitMaxs(int maxStack, int maxLocals) override
    {
        events.push_back("maxs:" + std::to_string(maxStack) + ":" + std::to_string(maxLocals));
    }
    void visitEnd() override { events.push_back("end"); }
};

class RecordingClassVisitor final : public ClassVisitor
{
public:
    std::vector<std::string> events;

    RecordingClassVisitor()
        : ClassVisitor(Opcodes::ASM9)
    {}

    void visit(int version, int access, ca::str::Utf8StringRef name,
               ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override
    {
        events.push_back("visit:" + std::to_string(version) + ":" + std::to_string(access) + ":" +
                         text(name) + ":" + text(signature) + ":" + text(superName) + ":" +
                         std::to_string(interfaces.size()));
    }
    void visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug) override
    {
        events.push_back("source:" + text(source) + ":" + text(debug));
    }
    void visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef desc) override
    {
        events.push_back("outer:" + text(owner) + ":" + text(name) + ":" + text(desc));
    }
    void visitNestHost(ca::str::Utf8StringRef host) override
    {
        events.push_back("nest-host:" + text(host));
    }
    void visitNestMember(ca::str::Utf8StringRef member) override
    {
        events.push_back("nest-member:" + text(member));
    }
    void visitPermittedSubclass(ca::str::Utf8StringRef subclass) override
    {
        events.push_back("permitted:" + text(subclass));
    }
    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef desc, bool visible) override
    {
        events.push_back("annotation:" + text(desc) + ":" + (visible ? "visible" : "invisible"));
        return nullptr;
    }
    AnnotationVisitor* visitTypeAnnotation(int                    typeRef, const TypePath*,
                                           ca::str::Utf8StringRef desc, bool visible) override
    {
        events.push_back("type-annotation:" + std::to_string(typeRef) + ":" + text(desc) + ":" +
                         (visible ? "visible" : "invisible"));
        return nullptr;
    }
    void visitInnerClass(ca::str::Utf8StringRef name, ca::str::Utf8StringRef outerName,
                         ca::str::Utf8StringRef innerName, int access) override
    {
        events.push_back("inner:" + text(name) + ":" + text(outerName) + ":" + text(innerName) +
                         ":" + std::to_string(access));
    }
    RecordComponentVisitor* visitRecordComponent(ca::str::Utf8StringRef name,
                                                 ca::str::Utf8StringRef desc,
                                                 ca::str::Utf8StringRef signature) override
    {
        events.push_back("record:" + text(name) + ":" + text(desc) + ":" + text(signature));
        return nullptr;
    }
    FieldVisitor* visitField(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                             ca::str::Utf8StringRef signature, const ca::core::Any&) override
    {
        events.push_back("field:" + std::to_string(access) + ":" + text(name) + ":" + text(desc) +
                         ":" + text(signature));
        return nullptr;
    }
    MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                               ca::str::Utf8StringRef                     signature,
                               const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        events.push_back("method:" + std::to_string(access) + ":" + text(name) + ":" + text(desc) +
                         ":" + text(signature) + ":" + std::to_string(exceptions.size()));
        return nullptr;
    }
    void visitEnd() override { events.push_back("end"); }
};

void expectSingleInsn(AbstractInsnNode* node, const std::string& expected)
{
    InsnList list;
    list.add(node);
    RecordingMethodVisitor visitor;
    list.accept(&visitor);
    ASSERT_EQ(visitor.events, std::vector<std::string>({expected}));
}

}   // namespace

TEST(InsnListAcceptContractTest, emptyListDoesNotEmitEvents)
{
    InsnList               list;
    RecordingMethodVisitor visitor;
    list.accept(&visitor);
    EXPECT_TRUE(visitor.events.empty());
}

TEST(InsnListAcceptContractTest, preservesForwardAndBackwardLinks)
{
    InsnList list;
    auto*    first  = new InsnNode(Opcodes::NOP);
    auto*    second = new InsnNode(Opcodes::RETURN);
    auto*    third  = new InsnNode(Opcodes::ATHROW);
    list.add(first);
    list.add(second);
    list.add(third);
    EXPECT_EQ(nullptr, first->previousInsn);
    EXPECT_EQ(second, first->nextInsn);
    EXPECT_EQ(first, second->previousInsn);
    EXPECT_EQ(third, second->nextInsn);
    EXPECT_EQ(second, third->previousInsn);
    EXPECT_EQ(nullptr, third->nextInsn);
}

TEST(InsnListAcceptContractTest, replaysInsnNode)
{
    expectSingleInsn(new InsnNode(Opcodes::RETURN), "insn:" + std::to_string(Opcodes::RETURN));
}
TEST(InsnListAcceptContractTest, replaysIntInsnNode)
{
    expectSingleInsn(new IntInsnNode(Opcodes::BIPUSH, 7),
                     "int:" + std::to_string(Opcodes::BIPUSH) + ":7");
}
TEST(InsnListAcceptContractTest, replaysVarInsnNode)
{
    expectSingleInsn(new VarInsnNode(Opcodes::ILOAD, 3),
                     "var:" + std::to_string(Opcodes::ILOAD) + ":3");
}
TEST(InsnListAcceptContractTest, replaysTypeInsnNode)
{
    expectSingleInsn(new TypeInsnNode(Opcodes::NEW, ZUtf8StringRef::from_static("pkg/T")),
                     "type:" + std::to_string(Opcodes::NEW) + ":pkg/T");
}
TEST(InsnListAcceptContractTest, replaysFieldInsnNode)
{
    expectSingleInsn(new FieldInsnNode(Opcodes::GETFIELD,
                                       ZUtf8StringRef::from_static("pkg/T"),
                                       ZUtf8StringRef::from_static("f"),
                                       ZUtf8StringRef::from_static("I")),
                     "field:" + std::to_string(Opcodes::GETFIELD) + ":pkg/T:f:I");
}
TEST(InsnListAcceptContractTest, replaysMethodInsnNode)
{
    expectSingleInsn(new MethodInsnNode(Opcodes::INVOKEVIRTUAL,
                                        ZUtf8StringRef::from_static("pkg/T"),
                                        ZUtf8StringRef::from_static("m"),
                                        ZUtf8StringRef::from_static("()V"),
                                        false),
                     "method:" + std::to_string(Opcodes::INVOKEVIRTUAL) + ":pkg/T:m:()V:class");
}
TEST(InsnListAcceptContractTest, replaysInvokeDynamicInsnNode)
{
    Handle handle(Opcodes::H_INVOKESTATIC,
                  ZUtf8StringRef::from_static("pkg/T"),
                  ZUtf8StringRef::from_static("bsm"),
                  ZUtf8StringRef::from_static("()V"),
                  false);
    expectSingleInsn(
        new InvokeDynamicInsnNode(
            ZUtf8StringRef::from_static("run"), ZUtf8StringRef::from_static("()V"), handle, {}),
        "indy:run:()V");
}
TEST(InsnListAcceptContractTest, replaysJumpInsnNode)
{
    expectSingleInsn(new JumpInsnNode(Opcodes::GOTO, new Label()),
                     "jump:" + std::to_string(Opcodes::GOTO));
}
TEST(InsnListAcceptContractTest, replaysLabelNode)
{
    expectSingleInsn(new LabelNode(labelScratchArena().create<Label>()), "label");
}
TEST(InsnListAcceptContractTest, replaysLdcInsnNode)
{
    expectSingleInsn(new LdcInsnNode(std::string("constant")), "ldc");
}
TEST(InsnListAcceptContractTest, replaysIincInsnNode)
{
    expectSingleInsn(new IincInsnNode(2, -1), "iinc:2:-1");
}
TEST(InsnListAcceptContractTest, replaysTableSwitchInsnNode)
{
    auto* dflt = new Label();
    expectSingleInsn(new TableSwitchInsnNode(0, 1, dflt, {new Label(), new Label()}),
                     "table-switch:0:1:2");
}
TEST(InsnListAcceptContractTest, replaysLookupSwitchInsnNode)
{
    expectSingleInsn(new LookupSwitchInsnNode(new Label(), {4, 9}, {new Label(), new Label()}),
                     "lookup-switch:2:2");
}
TEST(InsnListAcceptContractTest, replaysMultiANewArrayInsnNode)
{
    expectSingleInsn(new MultiANewArrayInsnNode(ZUtf8StringRef::from_static("[[I"), 2),
                     "multi-new-array:[[I:2");
}
TEST(InsnListAcceptContractTest, replaysFrameNode)
{
    expectSingleInsn(new FrameNode(Opcodes::F_FULL, {std::string("I")}, {std::string("T")}),
                     "frame:" + std::to_string(Opcodes::F_FULL));
}
TEST(InsnListAcceptContractTest, replaysLineNumberNode)
{
    expectSingleInsn(new LineNumberNode(42, new Label()), "line:42");
}

TEST(ClassNodeAcceptContractTest, replaysClassMetadataInStableOrder)
{
    ClassNode node;
    node.version         = Opcodes::V17;
    node.access          = Opcodes::ACC_PUBLIC;
    node.name            = ZUtf8StringRef::from_static("pkg/C");
    node.signature       = ZUtf8StringRef::from_static("<T:Ljava/lang/Object;>Ljava/lang/Object;");
    node.superName       = ZUtf8StringRef::from_static("java/lang/Object");
    node.interfaces      = {ZUtf8StringRef::from_static("java/io/Serializable")};
    node.sourceFile      = ZUtf8StringRef::from_static("C.java");
    node.sourceDebug     = ZUtf8StringRef::from_static("debug");
    node.outerClass      = ZUtf8StringRef::from_static("pkg/O");
    node.outerMethod     = ZUtf8StringRef::from_static("make");
    node.outerMethodDesc = ZUtf8StringRef::from_static("()V");
    node.nestHost        = ZUtf8StringRef::from_static("pkg/O");
    node.nestMembers.push_back(ZUtf8StringRef::from_static("pkg/C$Inner"));
    node.permittedSubclasses.push_back(ZUtf8StringRef::from_static("pkg/CChild"));
    node.visibleAnnotations.push_back(node.create<AnnotationNode>(node.arena(), ZUtf8StringRef::from_static("LVisible;")));
    node.invisibleAnnotations.push_back(
        node.create<AnnotationNode>(node.arena(), ZUtf8StringRef::from_static("LInvisible;")));
    node.visibleTypeAnnotations.push_back(
        node.create<TypeAnnotationNode>(node.arena(), 0x10, nullptr, ZUtf8StringRef::from_static("LType;")));
    node.innerClasses.push_back(node.create<InnerClassNode>(ZUtf8StringRef::from_static("pkg/C$Inner"),
                                                   ZUtf8StringRef::from_static("pkg/C"),
                                                   ZUtf8StringRef::from_static("Inner"),
                                                   1));
    node.recordComponents.push_back(node.create<RecordComponentNode>(node.arena(), ZUtf8StringRef::from_static("value"),
                                                            ZUtf8StringRef::from_static("I"),
                                                            ZUtf8StringRef::from_static("")));
    node.fields.push_back(new FieldNode(2,
                                        ZUtf8StringRef::from_static("f"),
                                        ZUtf8StringRef::from_static("I"),
                                        ZUtf8StringRef::from_static(""),
                                        {}));
    node.methods.push_back(new MethodNode(1,
                                          ZUtf8StringRef::from_static("m"),
                                          ZUtf8StringRef::from_static("()V"),
                                          ZUtf8StringRef::from_static(""),
                                          {}));

    RecordingClassVisitor visitor;
    node.accept(&visitor);
    ASSERT_FALSE(visitor.events.empty());
    EXPECT_EQ(visitor.events.front(),
              "visit:61:1:pkg/C:<T:Ljava/lang/Object;>Ljava/lang/Object;:java/lang/Object:1");
    EXPECT_EQ(visitor.events[1], "source:C.java:debug");
    EXPECT_EQ(visitor.events.back(), "end");
    EXPECT_LT(
        std::find(visitor.events.begin(), visitor.events.end(), "outer:pkg/O:make:()V"),
        std::find(visitor.events.begin(), visitor.events.end(), "annotation:LVisible;:visible"));
    EXPECT_LT(std::find(visitor.events.begin(), visitor.events.end(), "field:2:f:I:"),
              std::find(visitor.events.begin(), visitor.events.end(), "method:1:m:()V:0"));
}

TEST(ClassNodeAcceptContractTest, replaysTypeAnnotationReference)
{
    ClassNode node;
    node.visibleTypeAnnotations.push_back(
        node.create<TypeAnnotationNode>(node.arena(), 0x42, nullptr, ZUtf8StringRef::from_static("LType;")));
    RecordingClassVisitor visitor;
    node.accept(&visitor);
    ASSERT_EQ(visitor.events.size(), 3u);
    EXPECT_EQ(visitor.events[1], "type-annotation:66:LType;:visible");
}

TEST(MethodNodeAcceptContractTest, replaysAllStoredMethodSections)
{
    MethodNode node(9,
                    ZUtf8StringRef::from_static("m"),
                    ZUtf8StringRef::from_static("(I)V"),
                    ZUtf8StringRef::from_static("sig"),
                    {ZUtf8StringRef::from_static("E")});
    node.parameters.push_back(node.create<ParameterNode>(ZUtf8StringRef::from_static("arg"), 16));
    node.annotationDefault = node.create<AnnotationNode>(node.arena(), ZUtf8StringRef::from_static(""));
    node.visibleAnnotations.push_back(node.create<AnnotationNode>(node.arena(), ZUtf8StringRef::from_static("LVisible;")));
    node.invisibleAnnotations.push_back(
        node.create<AnnotationNode>(node.arena(), ZUtf8StringRef::from_static("LInvisible;")));
    node.visibleTypeAnnotations.push_back(
        node.create<TypeAnnotationNode>(node.arena(), 0x14, nullptr, ZUtf8StringRef::from_static("LReturn;")));
    node.invisibleTypeAnnotations.push_back(
        node.create<TypeAnnotationNode>(node.arena(), 0x15, nullptr, ZUtf8StringRef::from_static("LReceiver;")));
    node.visibleParameterAnnotations.push_back(
        node.create<AnnotationNode>(node.arena(), ZUtf8StringRef::from_static("LParam;")));
    node.visibleParameterIndices.push_back(0);
    auto* start   = new Label();
    auto* end     = new Label();
    auto* handler = new Label();
    node.tryCatchBlocks.push_back(
        node.create<TryCatchBlockNode>(start, end, handler, ZUtf8StringRef::from_static("E")));
    node.instructions.add(new LabelNode(start));
    node.instructions.add(new InsnNode(Opcodes::RETURN));
    node.localVariables.push_back(node.create<LocalVariableNode>(ZUtf8StringRef::from_static("arg"),
                                                        ZUtf8StringRef::from_static("I"),
                                                        ZUtf8StringRef::from_static(""),
                                                        start,
                                                        end,
                                                        1));
    node.visibleLocalVariableAnnotations.push_back(node.create<LocalVariableAnnotationNode>(
        node.arena(), 0x40, nullptr, std::vector<jbytecode::core::Label*>{start},
        std::vector<jbytecode::core::Label*>{end}, std::vector<int>{1},
        ZUtf8StringRef::from_static("LLocal;")));
    node.maxStack  = 3;
    node.maxLocals = 2;

    RecordingMethodVisitor visitor;
    node.accept(&visitor);
    EXPECT_EQ(visitor.events.front(), "parameter:arg:16");
    EXPECT_NE(std::find(visitor.events.begin(), visitor.events.end(), "annotation-default"),
              visitor.events.end());
    EXPECT_NE(std::find(visitor.events.begin(), visitor.events.end(), "try-catch:E"),
              visitor.events.end());
    EXPECT_NE(std::find(visitor.events.begin(), visitor.events.end(), "local:arg:I::1"),
              visitor.events.end());
    EXPECT_NE(std::find(visitor.events.begin(),
                        visitor.events.end(),
                        "local-annotation:64:1:1:LLocal;:visible"),
              visitor.events.end());
    EXPECT_EQ(visitor.events[visitor.events.size() - 2], "maxs:3:2");
    EXPECT_EQ(visitor.events.back(), "end");
}

TEST(MethodNodeAcceptContractTest, directMethodVisitorOverloadUsesSameReplay)
{
    MethodNode node(1,
                    ZUtf8StringRef::from_static("m"),
                    ZUtf8StringRef::from_static("()V"),
                    ZUtf8StringRef::from_static(""),
                    {});
    node.instructions.add(new InsnNode(Opcodes::RETURN));
    node.maxStack  = 0;
    node.maxLocals = 1;
    RecordingMethodVisitor visitor;
    node.accept(&visitor);
    EXPECT_EQ(visitor.events, (std::vector<std::string>{"code", "insn:177", "maxs:0:1", "end"}));
}
