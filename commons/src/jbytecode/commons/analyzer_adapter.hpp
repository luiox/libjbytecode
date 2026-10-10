#pragma once

#include "libca/core/any.hpp"
#include "garena/arena.hpp"
#include <string>
#include <vector>
#include <unordered_map>

#include "jbytecode/core/visitors.hpp"
#include "libca/str/utf8_string.hpp"

namespace ca::str {
class Utf8StringArena;
}

namespace jbytecode::commons {

class AnalyzerAdapter : public jbytecode::core::MethodVisitor
{
public:
    /// @param arena 作业级字符串池(借用,可空;字符串线示范,总方案 §5.1)。
    ///   ANEWARRAY/CHECKCAST 等合成描述符 intern 进它;为空时回落实例私有 descArena_
    ///   (#921 staticArena 退役,随 adapter 析构回收)。AnalysisManager 驱动的链路传
    ///   &am.arena()。
    AnalyzerAdapter(ca::str::Utf8StringRef owner, int access, ca::str::Utf8StringRef name,
                    ca::str::Utf8StringRef descriptor, jbytecode::core::MethodVisitor* mv,
                    ca::str::Utf8StringArena* arena = nullptr);

    std::vector<ca::core::Any>                                       locals_;
    std::vector<ca::core::Any>                                       stack_;
    std::vector<jbytecode::core::Label*>                             labels_;
    std::unordered_map<jbytecode::core::Label*, ca::str::Utf8String> uninitializedTypes_;
    int                                                              maxStack_  = 0;
    int                                                              maxLocals_ = 0;
    ca::str::Utf8StringRef                                           owner_;

    void visitFrame(int type, int numLocal, const std::vector<ca::core::Any>& local, int numStack,
                    const std::vector<ca::core::Any>& stack) override;

    void visitInsn(int opcode) override;
    void visitIntInsn(int opcode, int operand) override;
    void visitVarInsn(int opcode, int varIndex) override;
    void visitTypeInsn(int opcode, ca::str::Utf8StringRef type) override;
    void visitFieldInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef descriptor) override;
    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor, bool isInterface) override;
    void visitInvokeDynamicInsn(
        ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
        const jbytecode::core::Handle&    bootstrapMethodHandle,
        const std::vector<ca::core::Any>& bootstrapMethodArguments) override;
    void visitJumpInsn(int opcode, jbytecode::core::Label* label) override;
    void visitLabel(jbytecode::core::Label* label) override;
    void visitLdcInsn(const ca::core::Any& value) override;
    void visitIincInsn(int varIndex, int increment) override;
    void visitTableSwitchInsn(int min, int max, jbytecode::core::Label* dflt,
                              const std::vector<jbytecode::core::Label*>& labels) override;
    void visitLookupSwitchInsn(jbytecode::core::Label* dflt, const std::vector<int>& keys,
                               const std::vector<jbytecode::core::Label*>& labels) override;
    void visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor, int numDimensions) override;
    void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                            ca::str::Utf8StringRef signature, jbytecode::core::Label* start,
                            jbytecode::core::Label* end, int index) override;
    void visitMaxs(int maxStack, int maxLocals) override;

private:
    /// B8：uninitialized 占位 Label 归宿（scratch，寿命 = adapter，理由同 GeneratorAdapter）。
    garena::Arena labelArena_;

    /// #921 staticArena 退役:未注入作业 arena 时的兜底 intern 池。实例级生命周期——
    /// 合成描述符只被本 adapter 的 locals_/stack_ 引用,随 adapter 析构整体回收。
    ca::str::Utf8StringArena descArena_;

    bool frameDead_ = false;

    /// 注入的作业级字符串池(借用);为空时 execute() 的合成描述符回落实例私有 descArena_。
    ca::str::Utf8StringArena* arena_ = nullptr;

    ca::core::Any get(int local);
    void          set(int local, const ca::core::Any& type);
    void          push(const ca::core::Any& type);
    void          pushDescriptor(ca::str::Utf8StringRef descriptor);
    ca::core::Any pop();
    void          pop(int numSlots);
    void          pop(ca::str::Utf8StringRef descriptor);
    void          execute(int opcode, int intArg, ca::str::Utf8StringRef stringArg);

    static void visitFrameTypes(int numTypes, const std::vector<ca::core::Any>& frameTypes,
                                std::vector<ca::core::Any>& result);
};

}   // namespace jbytecode::commons
