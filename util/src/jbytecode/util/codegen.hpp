#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "libca/core/any.hpp"

#include "jbytecode/util/printer.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

namespace jbytecode::util {

class CodeGen : public Printer
{
public:
    CodeGen();

    void     visit(int version, int access, ca::str::Utf8StringRef name,
                   ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
                   const std::vector<ca::str::Utf8StringRef>& interfaces) override;
    void     visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug) override;
    void     visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                             ca::str::Utf8StringRef descriptor) override;
    Printer* visitClassAnnotation(ca::str::Utf8StringRef descriptor, bool visible) override;
    void     visitInnerClass(ca::str::Utf8StringRef name, ca::str::Utf8StringRef outerName,
                             ca::str::Utf8StringRef innerName, int access) override;
    Printer* visitField(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                        ca::str::Utf8StringRef signature, const ca::core::Any& value) override;
    Printer* visitMethod(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                         ca::str::Utf8StringRef                     signature,
                         const std::vector<ca::str::Utf8StringRef>& exceptions) override;
    void     visitClassEnd() override;

    void visitCode() override;
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
    void visitTryCatchBlock(jbytecode::core::Label* start, jbytecode::core::Label* end,
                            jbytecode::core::Label* handler, ca::str::Utf8StringRef type) override;
    void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                            ca::str::Utf8StringRef signature, jbytecode::core::Label* start,
                            jbytecode::core::Label* end, int index) override;
    void visitLineNumber(int line, jbytecode::core::Label* start) override;
    void visitMaxs(int maxStack, int maxLocals) override;
    void visitMethodEnd() override;

    void     visitAnnotationValue(ca::str::Utf8StringRef name, const ca::core::Any& value) override;
    void     visitAnnotationEnum(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                 ca::str::Utf8StringRef value) override;
    Printer* visitAnnotationAnnotation(ca::str::Utf8StringRef name,
                                       ca::str::Utf8StringRef descriptor) override;
    Printer* visitAnnotationArray(ca::str::Utf8StringRef name) override;
    void     visitAnnotationEnd() override;

    const std::string& getVarName() const { return name_; }

private:
    std::string name_;
    int         id_ = 0;

    // -- arena 死亡点声明（memory-governance-plan §5-P1③）--
    // 本成员持有 CodeGen 私有的 Utf8StringArena：getLabelName() 生成的 "label<N>" 名字
    // intern 自本 arena，仅缓存在成员 labelNames_ 中；各 visit*Insn/visitLabel 把名字
    // 拼进行文本后重新 intern 进 textArena（Printer 基类成员，见 printer.hpp 的声明），
    // 故本 arena 的视图不出境，仅内部使用。
    // 视图寿命 = 本对象寿命；死亡点：CodeGen 实例析构时（实例由使用者 / subCodeGen()
    // new 出，经 visitMethod/visitAnnotation 返回的 Printer* 由驱动方 delete）。
    // 消费者不得把视图带过本对象的死亡点；需跨期持有则 intern 进自己的 arena 或深拷贝。
    ca::str::Utf8StringArena                                            labelArena_;
    std::unordered_map<jbytecode::core::Label*, ca::str::Utf8StringRef> labelNames_;
    int                                                                 nextLabelId_    = 0;
    bool                                                                declaredLabels_ = false;

    ca::str::Utf8StringRef getLabelName(jbytecode::core::Label* label);
    void                   declareLabel(jbytecode::core::Label* label);
    void                   decl(ca::str::Utf8StringRef line);
    void                   varLine(ca::str::Utf8StringRef call);
    CodeGen*               subCodeGen(const std::string& varName);
    static std::string     accessFlags(int access);
    void                   appendConst(const ca::core::Any& value, ca::str::Utf8StringBuilder& out);
    static int             getVersionConstant(int version);

    CodeGen(const CodeGen&)            = delete;
    CodeGen& operator=(const CodeGen&) = delete;
};

}   // namespace jbytecode::util
