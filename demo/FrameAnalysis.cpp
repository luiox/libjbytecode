#include <iostream>
#include <memory>

#include "jbytecode/jbytecode.hpp"
#include "libca/str/utf8_string.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace jbytecode::commons;
using namespace jbytecode::util;
using namespace ca::str;


static std::string toStr(Utf8StringRef ref)
{
    return ref.to_std_string();
}

int main()
{
    std::cout << "FrameAnalysis Demo\n\n";

    // Build a simple class with a method that has branches
    ClassNode cn;
    cn.version   = 52 << 16;
    cn.access    = Opcodes::ACC_PUBLIC;
    cn.name      = ZUtf8StringRef::from_static("demo/Calc");
    cn.superName = ZUtf8StringRef::from_static("java/lang/Object");

    // Method: public static int max(int a, int b)
    auto* mn      = new MethodNode(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                              ZUtf8StringRef::from_static("max"),
                              ZUtf8StringRef::from_static("(II)I"),
                              Utf8StringRef(),
                                   {});
    mn->maxStack  = 2;
    mn->maxLocals = 2;

    auto* elseLabel = new Label();
    auto* endLabel  = new Label();

    mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));
    mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 1));
    mn->instructions.add(new JumpInsnNode(Opcodes::IF_ICMPLT, elseLabel));
    mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));
    mn->instructions.add(new JumpInsnNode(Opcodes::GOTO, endLabel));
    mn->instructions.add(new LabelNode(elseLabel));
    mn->instructions.add(new VarInsnNode(Opcodes::ILOAD, 1));
    mn->instructions.add(new LabelNode(endLabel));
    mn->instructions.add(new InsnNode(Opcodes::IRETURN));
    cn.methods.push_back(mn);

    // Method: public static int add(int a, int b)
    auto* mn2      = new MethodNode(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                               ZUtf8StringRef::from_static("add"),
                               ZUtf8StringRef::from_static("(II)I"),
                               Utf8StringRef(),
                                    {});
    mn2->maxStack  = 2;
    mn2->maxLocals = 2;
    mn2->instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));
    mn2->instructions.add(new VarInsnNode(Opcodes::ILOAD, 1));
    mn2->instructions.add(new InsnNode(Opcodes::IADD));
    mn2->instructions.add(new InsnNode(Opcodes::IRETURN));
    cn.methods.push_back(mn2);

    // Run frame analysis on built method
    BasicInterpreter     interp;
    Analyzer<BasicValue> analyzer(&interp);

    for (auto* method : cn.methods) {
        std::cout << "--- " << toStr(method->name) << toStr(method->desc) << " ---\n";

        auto frames = analyzer.analyze(cn.name, method);

        auto insns = method->instructions.toArray();
        for (size_t i = 0; i < insns.size() && i < frames.size(); i++) {
            auto* frame = frames[i];
            if (!frame) {
                std::cout << "  [" << i << "] (unreachable)\n";
                continue;
            }
            std::cout << "  [" << i << "]";
            std::cout << " locals=";
            auto nLocals = frame->getLocals();
            for (int j = 0; j < nLocals; j++) {
                std::cout << frame->getLocal(j).toString();
            }
            std::cout << " stack=";
            auto nStack = frame->getStackSize();
            for (int j = 0; j < nStack; j++) {
                std::cout << frame->getStack(j).toString();
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    return 0;
}
