#include "jbytecode/commons/remapper_visitors.hpp"

#include <cstring>

using namespace jbytecode::commons;
using namespace jbytecode::core;
using ca::str::Utf8StringRef;

// ============================================================
// AnnotationRemapper
// ============================================================

AnnotationRemapper::AnnotationRemapper(AnnotationVisitor* av, Remapper* remapper)
    : AnnotationVisitor(Opcodes::ASM4)
    , av_(av)
    , remapper_(remapper)
{}

void AnnotationRemapper::visit(ca::str::Utf8StringRef name, const ca::core::Any& value)
{
    av_->visit(name, remapper_->mapValue(value));
}

void AnnotationRemapper::visitEnum(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                   ca::str::Utf8StringRef value)
{
    av_->visitEnum(name, remapper_->mapDesc(descriptor), value);
}

AnnotationVisitor* AnnotationRemapper::visitAnnotation(ca::str::Utf8StringRef name,
                                                       ca::str::Utf8StringRef descriptor)
{
    AnnotationVisitor* childAv = av_->visitAnnotation(name, remapper_->mapDesc(descriptor));
    return childAv != nullptr ? new AnnotationRemapper(childAv, remapper_) : nullptr;
}

AnnotationVisitor* AnnotationRemapper::visitArray(ca::str::Utf8StringRef name)
{
    AnnotationVisitor* childAv = av_->visitArray(name);
    return childAv != nullptr ? new AnnotationRemapper(childAv, remapper_) : nullptr;
}

void AnnotationRemapper::visitEnd()
{
    av_->visitEnd();
}

// ============================================================
// FieldRemapper
// ============================================================

FieldRemapper::FieldRemapper(FieldVisitor* fv, Remapper* remapper)
    : FieldVisitor(Opcodes::ASM4)
    , fv_(fv)
    , remapper_(remapper)
{}

AnnotationVisitor* FieldRemapper::visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible)
{
    AnnotationVisitor* childAv = fv_->visitAnnotation(remapper_->mapDesc(descriptor), visible);
    if (childAv == nullptr) return nullptr;
    return new AnnotationRemapper(childAv, remapper_);
}

AnnotationVisitor* FieldRemapper::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                      ca::str::Utf8StringRef descriptor,
                                                      bool                   visible)
{
    AnnotationVisitor* childAv =
        fv_->visitTypeAnnotation(typeRef, typePath, remapper_->mapDesc(descriptor), visible);
    if (childAv == nullptr) return nullptr;
    return new AnnotationRemapper(childAv, remapper_);
}

void FieldRemapper::visitEnd()
{
    fv_->visitEnd();
}

// ============================================================
// MethodRemapper
// ============================================================

MethodRemapper::MethodRemapper(MethodVisitor* mv, Remapper* remapper)
    : MethodVisitor(Opcodes::ASM4)
    , mv_(mv)
    , remapper_(remapper)
{}

AnnotationVisitor* MethodRemapper::visitAnnotationDefault()
{
    AnnotationVisitor* childAv = mv_->visitAnnotationDefault();
    if (childAv == nullptr) return nullptr;
    return new AnnotationRemapper(childAv, remapper_);
}

AnnotationVisitor* MethodRemapper::visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible)
{
    AnnotationVisitor* childAv = mv_->visitAnnotation(remapper_->mapDesc(descriptor), visible);
    if (childAv == nullptr) return nullptr;
    return new AnnotationRemapper(childAv, remapper_);
}

AnnotationVisitor* MethodRemapper::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                       ca::str::Utf8StringRef descriptor,
                                                       bool                   visible)
{
    AnnotationVisitor* childAv =
        mv_->visitTypeAnnotation(typeRef, typePath, remapper_->mapDesc(descriptor), visible);
    if (childAv == nullptr) return nullptr;
    return new AnnotationRemapper(childAv, remapper_);
}

AnnotationVisitor* MethodRemapper::visitParameterAnnotation(int                    parameter,
                                                            ca::str::Utf8StringRef descriptor,
                                                            bool                   visible)
{
    AnnotationVisitor* childAv =
        mv_->visitParameterAnnotation(parameter, remapper_->mapDesc(descriptor), visible);
    if (childAv == nullptr) return nullptr;
    return new AnnotationRemapper(childAv, remapper_);
}

void MethodRemapper::visitFrame(int type, int nLocal, const std::vector<ca::core::Any>& local,
                                int nStack, const std::vector<ca::core::Any>& stack)
{
    mv_->visitFrame(type, nLocal, local, nStack, stack);
}

void MethodRemapper::visitFieldInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                                    Utf8StringRef descriptor)
{
    mv_->visitFieldInsn(opcode,
                        remapper_->mapType(owner),
                        remapper_->mapFieldName(owner, name, descriptor),
                        remapper_->mapDesc(descriptor));
}

void MethodRemapper::visitMethodInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                                     Utf8StringRef descriptor, bool isInterface)
{
    mv_->visitMethodInsn(opcode,
                         remapper_->mapType(owner),
                         remapper_->mapMethodName(owner, name, descriptor),
                         remapper_->mapMethodDesc(descriptor),
                         isInterface);
}

void MethodRemapper::visitInvokeDynamicInsn(
    Utf8StringRef name, Utf8StringRef descriptor, const Handle& bootstrapMethodHandle,
    const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    std::vector<ca::core::Any> mappedArguments;
    mappedArguments.reserve(bootstrapMethodArguments.size());
    for (const auto& argument : bootstrapMethodArguments) {
        mappedArguments.push_back(remapper_->mapValue(argument));
    }
    const auto mappedBootstrap = remapper_->mapValue(ca::core::Any(bootstrapMethodHandle));
    mv_->visitInvokeDynamicInsn(remapper_->mapInvokeDynamicMethodName(name, descriptor),
                                remapper_->mapMethodDesc(descriptor),
                                mappedBootstrap.cast<Handle>(),
                                mappedArguments);
}

void MethodRemapper::visitLdcInsn(const ca::core::Any& value)
{
    mv_->visitLdcInsn(remapper_->mapValue(value));
}

void MethodRemapper::visitTypeInsn(int opcode, Utf8StringRef type)
{
    mv_->visitTypeInsn(opcode, remapper_->mapType(type));
}

void MethodRemapper::visitLocalVariable(Utf8StringRef name, Utf8StringRef descriptor,
                                        Utf8StringRef signature, Label* start, Label* end,
                                        int index)
{
    mv_->visitLocalVariable(name,
                            remapper_->mapDesc(descriptor),
                            remapper_->mapSignature(signature),
                            start,
                            end,
                            index);
}

void MethodRemapper::visitMaxs(int maxStack, int maxLocals)
{
    mv_->visitMaxs(maxStack, maxLocals);
}

void MethodRemapper::visitEnd()
{
    mv_->visitEnd();
}

// ============================================================
// ClassRemapper
// ============================================================

ClassRemapper::ClassRemapper(ClassVisitor* cv, Remapper* remapper)
    : ClassVisitor(Opcodes::ASM4)
    , cv_(cv)
    , remapper_(remapper)
{}

void ClassRemapper::visit(int version, int access, Utf8StringRef name, Utf8StringRef signature,
                          Utf8StringRef superName, const std::vector<Utf8StringRef>& interfaces)
{
    className_ = name;
    cv_->visit(version,
               access,
               remapper_->mapType(name),
               remapper_->mapSignature(signature),
               remapper_->mapType(superName),
               remapper_->mapTypes(interfaces));
}

void ClassRemapper::visitSource(Utf8StringRef source, Utf8StringRef debug)
{
    cv_->visitSource(source, debug);
}

void ClassRemapper::visitOuterClass(Utf8StringRef owner, Utf8StringRef name, Utf8StringRef desc)
{
    cv_->visitOuterClass(remapper_->mapType(owner),
                         remapper_->mapMethodName(owner, name, desc),
                         remapper_->mapMethodDesc(desc));
}

AnnotationVisitor* ClassRemapper::visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible)
{
    AnnotationVisitor* childAv = cv_->visitAnnotation(remapper_->mapDesc(descriptor), visible);
    if (childAv == nullptr) return nullptr;
    return new AnnotationRemapper(childAv, remapper_);
}

AnnotationVisitor* ClassRemapper::visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                      ca::str::Utf8StringRef descriptor,
                                                      bool                   visible)
{
    AnnotationVisitor* childAv =
        cv_->visitTypeAnnotation(typeRef, typePath, remapper_->mapDesc(descriptor), visible);
    if (childAv == nullptr) return nullptr;
    return new AnnotationRemapper(childAv, remapper_);
}

void ClassRemapper::visitInnerClass(Utf8StringRef name, Utf8StringRef outerName,
                                    Utf8StringRef innerName, int access)
{
    cv_->visitInnerClass(
        remapper_->mapType(name), remapper_->mapType(outerName), innerName, access);
}

void ClassRemapper::visitNestHost(Utf8StringRef nestHost)
{
    cv_->visitNestHost(remapper_->mapType(nestHost));
}

void ClassRemapper::visitNestMember(Utf8StringRef nestMember)
{
    cv_->visitNestMember(remapper_->mapType(nestMember));
}

void ClassRemapper::visitPermittedSubclass(Utf8StringRef subclass)
{
    cv_->visitPermittedSubclass(remapper_->mapType(subclass));
}

RecordComponentVisitor* ClassRemapper::visitRecordComponent(Utf8StringRef name,
                                                             Utf8StringRef descriptor,
                                                             Utf8StringRef signature)
{
    RecordComponentVisitor* childRcv = cv_->visitRecordComponent(
        remapper_->mapRecordComponentName(className_, name, descriptor),
        remapper_->mapDesc(descriptor),
        remapper_->mapSignature(signature));
    if (childRcv == nullptr) return nullptr;
    return new RecordComponentRemapper(childRcv, remapper_);
}

FieldVisitor* ClassRemapper::visitField(int access, Utf8StringRef name, Utf8StringRef descriptor,
                                        Utf8StringRef signature, const ca::core::Any& value)
{
    FieldVisitor* childFv = cv_->visitField(access,
                                            remapper_->mapFieldName(className_, name, descriptor),
                                            remapper_->mapDesc(descriptor),
                                            remapper_->mapSignature(signature),
                                            remapper_->mapValue(value));
    if (childFv == nullptr) return nullptr;
    return new FieldRemapper(childFv, remapper_);
}

MethodVisitor* ClassRemapper::visitMethod(int access, Utf8StringRef name, Utf8StringRef descriptor,
                                          Utf8StringRef                     signature,
                                          const std::vector<Utf8StringRef>& exceptions)
{
    MethodVisitor* childMv = cv_->visitMethod(access,
                                              remapper_->mapMethodName(className_, name, descriptor),
                                              remapper_->mapMethodDesc(descriptor),
                                              remapper_->mapSignature(signature),
                                              remapper_->mapTypes(exceptions));
    if (childMv == nullptr) return nullptr;
    return new MethodRemapper(childMv, remapper_);
}

void ClassRemapper::visitEnd()
{
    cv_->visitEnd();
}

ModuleVisitor* ClassRemapper::visitModule(Utf8StringRef name, int access, Utf8StringRef version)
{
    ModuleVisitor* childMv = cv_->visitModule(remapper_->map(name), access, version);
    if (childMv == nullptr) return nullptr;
    return new ModuleRemapper(childMv, remapper_);
}

// ============================================================
// ModuleRemapper
// ============================================================

ModuleRemapper::ModuleRemapper(ModuleVisitor* mv, Remapper* remapper)
    : ModuleVisitor(Opcodes::ASM4)
    , mv_(mv)
    , remapper_(remapper)
{}

void ModuleRemapper::visitMainClass(Utf8StringRef mainClass)
{
    mv_->visitMainClass(remapper_->mapType(mainClass));
}

void ModuleRemapper::visitPackage(Utf8StringRef packaze)
{
    mv_->visitPackage(remapper_->map(packaze));
}

void ModuleRemapper::visitRequire(Utf8StringRef module, int access, Utf8StringRef version)
{
    mv_->visitRequire(remapper_->map(module), access, version);
}

void ModuleRemapper::visitExport(Utf8StringRef packaze, int access,
                                 const std::vector<Utf8StringRef>& modules)
{
    std::vector<ca::str::Utf8StringRef> remappedModules;
    remappedModules.reserve(modules.size());
    for (const auto& m : modules) {
        remappedModules.push_back(remapper_->map(m));
    }
    mv_->visitExport(remapper_->map(packaze), access, remappedModules);
}

void ModuleRemapper::visitOpen(Utf8StringRef packaze, int access,
                               const std::vector<Utf8StringRef>& modules)
{
    std::vector<ca::str::Utf8StringRef> remappedModules;
    remappedModules.reserve(modules.size());
    for (const auto& m : modules) {
        remappedModules.push_back(remapper_->map(m));
    }
    mv_->visitOpen(remapper_->map(packaze), access, remappedModules);
}

void ModuleRemapper::visitUse(Utf8StringRef service)
{
    mv_->visitUse(remapper_->mapType(service));
}

void ModuleRemapper::visitProvide(Utf8StringRef                     service,
                                  const std::vector<Utf8StringRef>& providers)
{
    std::vector<ca::str::Utf8StringRef> remappedProviders;
    remappedProviders.reserve(providers.size());
    for (const auto& p : providers) {
        remappedProviders.push_back(remapper_->mapType(p));
    }
    mv_->visitProvide(remapper_->mapType(service), remappedProviders);
}

void ModuleRemapper::visitEnd()
{
    mv_->visitEnd();
}

// ============================================================
// RecordComponentRemapper
// ============================================================

RecordComponentRemapper::RecordComponentRemapper(RecordComponentVisitor* rcv, Remapper* remapper)
    : RecordComponentVisitor(Opcodes::ASM4)
    , rcv_(rcv)
    , remapper_(remapper)
{}

AnnotationVisitor* RecordComponentRemapper::visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                            bool                   visible)
{
    AnnotationVisitor* childAv = rcv_->visitAnnotation(remapper_->mapDesc(descriptor), visible);
    if (childAv == nullptr) return nullptr;
    return new AnnotationRemapper(childAv, remapper_);
}

AnnotationVisitor* RecordComponentRemapper::visitTypeAnnotation(int                    typeRef,
                                                                const TypePath*        typePath,
                                                                ca::str::Utf8StringRef descriptor,
                                                                bool                   visible)
{
    AnnotationVisitor* childAv =
        rcv_->visitTypeAnnotation(typeRef, typePath, remapper_->mapDesc(descriptor), visible);
    if (childAv == nullptr) return nullptr;
    return new AnnotationRemapper(childAv, remapper_);
}

void RecordComponentRemapper::visitEnd()
{
    rcv_->visitEnd();
}
