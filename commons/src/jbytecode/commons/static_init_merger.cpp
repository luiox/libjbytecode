#include "jbytecode/commons/static_init_merger.hpp"

#include "jbytecode/core/opcodes.hpp"

using namespace jbytecode::commons;
using namespace jbytecode::core;
using ca::str::Utf8StringRef;

StaticInitMerger::StaticInitMerger(Utf8StringRef prefix, ClassVisitor* cv)
    : StaticInitMerger(Opcodes::ASM9, prefix, cv)
{}

StaticInitMerger::StaticInitMerger(int api, Utf8StringRef prefix, ClassVisitor* cv)
    : ClassVisitor(api, cv)
    , renamedClinitMethodPrefix_(prefix)
{}

void StaticInitMerger::visit(int version, int access, Utf8StringRef name, Utf8StringRef signature,
                             Utf8StringRef superName, const std::vector<Utf8StringRef>& interfaces)
{
    ClassVisitor::visit(version, access, name, signature, superName, interfaces);
    owner_ = name;
}

MethodVisitor* StaticInitMerger::visitMethod(int access, Utf8StringRef name,
                                             Utf8StringRef descriptor, Utf8StringRef signature,
                                             const std::vector<Utf8StringRef>& exceptions)
{
    if (name.equals("<clinit>")) {
        int newAccess = Opcodes::ACC_PRIVATE | Opcodes::ACC_STATIC;
        // 重命名后的 <clinit> 方法名：prefix + 序号。用 Utf8StringBuilder 拼接，build()
        // 得拥有所有权的 Utf8String。
        ca::str::Utf8StringBuilder newNameBuilder;
        newNameBuilder.append(renamedClinitMethodPrefix_);
        newNameBuilder.append(std::to_string(numClinitMethods_++).c_str());
        auto           newName = newNameBuilder.build();
        MethodVisitor* mv =
            ClassVisitor::visitMethod(newAccess, newName, descriptor, signature, exceptions);

        if (mergedClinitVisitor_ == nullptr) {
            mergedClinitVisitor_ =
                ClassVisitor::visitMethod(newAccess, name, descriptor, Utf8StringRef(), {});
        }
        mergedClinitVisitor_->visitMethodInsn(
            Opcodes::INVOKESTATIC,
            owner_,
            newName.ref(),
            descriptor,
            false);

        return mv;
    }
    return ClassVisitor::visitMethod(access, name, descriptor, signature, exceptions);
}

void StaticInitMerger::visitEnd()
{
    if (mergedClinitVisitor_ != nullptr) {
        mergedClinitVisitor_->visitInsn(Opcodes::RETURN);
        mergedClinitVisitor_->visitMaxs(0, 0);
    }
    ClassVisitor::visitEnd();
}
