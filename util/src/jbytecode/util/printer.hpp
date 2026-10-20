#pragma once

#include <string>
#include <vector>
#include "libca/core/any.hpp"
#include <sstream>

#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/primitives.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

namespace jbytecode::util {

class Printer
{
public:
    static const char* OPCODES[];
    static const char* TYPES[];
    static const char* HANDLE_TAG[];

    static std::string getAccessFlagsStr(int access, bool capital = true);

    // -- arena 死亡点声明 --
    // 本成员持有 Printer 私有的 Utf8StringArena：全部输出行（addLine/line 及各 visit*
    // 的 textArena.intern(...)）的视图 intern 自本 arena，存入公开成员 text 并经
    // getText() 出境。
    // 视图寿命 = 本对象寿命；死亡点：Printer 对象析构时（含 CodeGen/Textifier 及其
    // 子实例；子实例各自拥有独立 textArena，由驱动方 delete）。
    // 消费者不得把视图带过本对象的死亡点；需跨期持有则 intern 进自己的 arena 或深拷贝。
    ca::str::Utf8StringArena            textArena;
    std::vector<ca::str::Utf8StringRef> text;
    ca::str::Utf8StringBuilder          stringBuilder;

    explicit Printer(int api);
    virtual ~Printer() = default;

    // -- Class-level visitors --
    virtual void     visit(int version, int access, ca::str::Utf8StringRef name,
                           ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
                           const std::vector<ca::str::Utf8StringRef>& interfaces);
    virtual void     visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug);
    virtual void     visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                     ca::str::Utf8StringRef descriptor);
    virtual Printer* visitClassAnnotation(ca::str::Utf8StringRef descriptor, bool visible);
    virtual Printer* visitClassTypeAnnotation(int typeRef, ca::str::Utf8StringRef typePath,
                                              ca::str::Utf8StringRef descriptor, bool visible);
    virtual void     visitClassAttribute(const jbytecode::core::Attribute& attribute);
    virtual void     visitInnerClass(ca::str::Utf8StringRef name, ca::str::Utf8StringRef outerName,
                                     ca::str::Utf8StringRef innerName, int access);
    virtual Printer* visitField(int access, ca::str::Utf8StringRef name,
                                ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                                const ca::core::Any& value);
    virtual Printer* visitMethod(int access, ca::str::Utf8StringRef name,
                                 ca::str::Utf8StringRef                     descriptor,
                                 ca::str::Utf8StringRef                     signature,
                                 const std::vector<ca::str::Utf8StringRef>& exceptions);
    virtual void     visitClassEnd();

    // -- Module visitors --
    virtual void visitMainClass(ca::str::Utf8StringRef mainClass);
    virtual void visitPackage(ca::str::Utf8StringRef packaze);
    virtual void visitRequire(ca::str::Utf8StringRef module, int access,
                              ca::str::Utf8StringRef version);
    virtual void visitExport(ca::str::Utf8StringRef packaze, int access,
                             const std::vector<ca::str::Utf8StringRef>& modules);
    virtual void visitOpen(ca::str::Utf8StringRef packaze, int access,
                           const std::vector<ca::str::Utf8StringRef>& modules);
    virtual void visitUse(ca::str::Utf8StringRef service);
    virtual void visitProvide(ca::str::Utf8StringRef                     service,
                              const std::vector<ca::str::Utf8StringRef>& providers);
    virtual void visitModuleEnd();

    // -- Record Component visitors --
    virtual Printer* visitRecordComponentAnnotation(ca::str::Utf8StringRef descriptor,
                                                    bool                   visible);
    virtual Printer* visitRecordComponentTypeAnnotation(int                    typeRef,
                                                        ca::str::Utf8StringRef typePath,
                                                        ca::str::Utf8StringRef descriptor,
                                                        bool                   visible);
    virtual void     visitRecordComponentAttribute(const jbytecode::core::Attribute& attribute);
    virtual void     visitRecordComponentEnd();

    // -- Annotation visitors --
    virtual void visitAnnotationValue(ca::str::Utf8StringRef name, const ca::core::Any& value);
    virtual void visitAnnotationEnum(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                     ca::str::Utf8StringRef value);
    virtual Printer* visitAnnotationAnnotation(ca::str::Utf8StringRef name,
                                               ca::str::Utf8StringRef descriptor);
    virtual Printer* visitAnnotationArray(ca::str::Utf8StringRef name);
    virtual void     visitAnnotationEnd();

    // -- Code visitors (also in MethodVisitor) --
    virtual void visitCode();
    virtual void visitFrame(int type, int numLocal, const std::vector<ca::core::Any>& local,
                            int numStack, const std::vector<ca::core::Any>& stack);
    virtual void visitInsn(int opcode);
    virtual void visitIntInsn(int opcode, int operand);
    virtual void visitVarInsn(int opcode, int varIndex);
    virtual void visitTypeInsn(int opcode, ca::str::Utf8StringRef type);
    virtual void visitFieldInsn(int opcode, ca::str::Utf8StringRef owner,
                                ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor);
    virtual void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner,
                                 ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                 bool isInterface);
    virtual void visitInvokeDynamicInsn(ca::str::Utf8StringRef            name,
                                        ca::str::Utf8StringRef            descriptor,
                                        const jbytecode::core::Handle&    bootstrapMethodHandle,
                                        const std::vector<ca::core::Any>& bootstrapMethodArguments);
    virtual void visitJumpInsn(int opcode, jbytecode::core::Label* label);
    virtual void visitLabel(jbytecode::core::Label* label);
    virtual void visitLdcInsn(const ca::core::Any& value);
    virtual void visitIincInsn(int varIndex, int increment);
    virtual void visitTableSwitchInsn(int min, int max, jbytecode::core::Label* dflt,
                                      const std::vector<jbytecode::core::Label*>& labels);
    virtual void visitLookupSwitchInsn(jbytecode::core::Label* dflt, const std::vector<int>& keys,
                                       const std::vector<jbytecode::core::Label*>& labels);
    virtual void visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor, int numDimensions);
    virtual void visitTryCatchBlock(jbytecode::core::Label* start, jbytecode::core::Label* end,
                                    jbytecode::core::Label* handler, ca::str::Utf8StringRef type);
    virtual void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                    ca::str::Utf8StringRef signature, jbytecode::core::Label* start,
                                    jbytecode::core::Label* end, int index);
    virtual void visitLineNumber(int line, jbytecode::core::Label* start);
    virtual void visitMaxs(int maxStack, int maxLocals);
    virtual void visitFieldEnd();
    virtual void visitMethodEnd();

    // -- Static utilities --
    static std::string         appendAccessFlags(int access, bool isClass = false);
    static ca::str::Utf8String appendString(ca::str::Utf8StringRef str);

protected:
    int                                        api;
    const std::vector<ca::str::Utf8StringRef>& getText() const { return text; }

    void addLine(ca::str::Utf8StringRef line);
    void addLine();
    void setBuf(ca::str::Utf8StringRef s);
    void appendBuf(ca::str::Utf8StringRef s);
    void clearBuf();
};

}   // namespace jbytecode::util
