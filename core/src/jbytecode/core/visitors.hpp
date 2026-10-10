/// @file        visitors.hpp
/// @brief       ASM event-based visitor hierarchy.
///
/// Defines the abstract visitor base classes for the ASM streaming API:
///   AnnotationVisitor, FieldVisitor, MethodVisitor, ClassVisitor,
///   ModuleVisitor, RecordComponentVisitor, SignatureVisitor.
///
/// All visitors use the <strong>delegate pattern</strong>: each class holds
/// a pointer to a "next" visitor (av/fv/mv/cv/mv/delegate/sv). Default method
/// implementations delegate to this next visitor, enabling transparent
/// filtering/transformation chains. If the delegate is nullptr, the default
/// is a no-op (or returns nullptr for factory methods).
///
/// The constructor validates the API version against Opcodes::ASM4–ASM9
/// and Opcodes::ASM10_EXPERIMENTAL.
///
/// Corresponding ASM originals:
///   AnnotationVisitor  ← AnnotationVisitor.java
///   FieldVisitor       ← FieldVisitor.java
///   MethodVisitor      ← MethodVisitor.java
///   ClassVisitor       ← ClassVisitor.java
///   ModuleVisitor      ← ModuleVisitor.java
///   RecordComponentVisitor ← RecordComponentVisitor.java
///   SignatureVisitor   ← signature/SignatureVisitor.java
#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/type.hpp"
#include "libca/core/any.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::core {

/// A visitor to visit a Java annotation. The methods of this class must be
/// called in the following order:
///   ( visit | visitEnum | visitAnnotation | visitArray )* visitEnd.
///
/// @author Eric Bruneton
/// @author Eugene Kuleshov
class AnnotationVisitor
{
public:
    AnnotationVisitor(int api)
        : AnnotationVisitor(api, nullptr)
    {}
    AnnotationVisitor(int api, AnnotationVisitor* av)
        : api(api)
        , av(av)
    {
        if (api != Opcodes::ASM9 && api != Opcodes::ASM8 && api != Opcodes::ASM7 &&
            api != Opcodes::ASM6 && api != Opcodes::ASM5 && api != Opcodes::ASM4 &&
            api != Opcodes::ASM10_EXPERIMENTAL) {
            throw std::invalid_argument("Unsupported api " + std::to_string(api));
        }
    }
    virtual ~AnnotationVisitor() = default;

    AnnotationVisitor* getDelegate() { return av; }

    /// Visits a primitive value of the annotation.
    virtual void visit(ca::str::Utf8StringRef name, const ca::core::Any& value)
    {
        if (av) av->visit(name, value);
    }
    /// Visits an enum value of the annotation.
    virtual void visitEnum(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                           ca::str::Utf8StringRef value)
    {
        if (av) av->visitEnum(name, descriptor, value);
    }
    /// Visits a nested annotation value of the annotation.
    virtual AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef name,
                                               ca::str::Utf8StringRef descriptor)
    {
        if (av) return av->visitAnnotation(name, descriptor);
        return nullptr;
    }
    /// Visits an array value of the annotation.
    virtual AnnotationVisitor* visitArray(ca::str::Utf8StringRef name)
    {
        if (av) return av->visitArray(name);
        return nullptr;
    }
    /// Visits the end of the annotation.
    virtual void visitEnd()
    {
        if (av) av->visitEnd();
    }

protected:
    /// The ASM API version implemented by this visitor.
    const int api;
    /// The annotation visitor to which this visitor must delegate.
    AnnotationVisitor* av;
};

/// A visitor to visit a Java field. The methods of this class must be called
/// in the following order:
///   ( visitAnnotation | visitTypeAnnotation | visitAttribute )* visitEnd.
///
/// @author Eric Bruneton
class FieldVisitor
{
public:
    FieldVisitor(int api)
        : FieldVisitor(api, nullptr)
    {}
    FieldVisitor(int api, FieldVisitor* fv)
        : api(api)
        , fv(fv)
    {
        if (api != Opcodes::ASM9 && api != Opcodes::ASM8 && api != Opcodes::ASM7 &&
            api != Opcodes::ASM6 && api != Opcodes::ASM5 && api != Opcodes::ASM4 &&
            api != Opcodes::ASM10_EXPERIMENTAL) {
            throw std::invalid_argument("Unsupported api " + std::to_string(api));
        }
    }
    virtual ~FieldVisitor() = default;

    FieldVisitor* getDelegate() { return fv; }

    virtual AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible)
    {
        if (fv) return fv->visitAnnotation(descriptor, visible);
        return nullptr;
    }
    virtual AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                   ca::str::Utf8StringRef descriptor, bool visible)
    {
        if (api < Opcodes::ASM5) {
            throw std::runtime_error("This feature requires ASM5");
        }
        if (fv) return fv->visitTypeAnnotation(typeRef, typePath, descriptor, visible);
        return nullptr;
    }
    virtual void visitAttribute(const Attribute& attribute)
    {
        if (fv) fv->visitAttribute(attribute);
    }
    virtual void visitEnd()
    {
        if (fv) fv->visitEnd();
    }

protected:
    const int     api;
    FieldVisitor* fv;
};

/// A visitor to visit a Java method. The methods of this class must be called
/// in the following order:
///   ( visitParameter )* [ visitAnnotationDefault ]
///   ( visitAnnotation | visitAnnotableParameterCount | visitParameterAnnotation |
///     visitTypeAnnotation | visitAttribute )*
///   [ visitCode ( visitFrame | visitXxxInsn | visitLabel | visitInsnAnnotation |
///     visitTryCatchBlock | visitTryCatchAnnotation | visitLocalVariable |
///     visitLocalVariableAnnotation | visitLineNumber | visitAttribute )*
///     visitMaxs ]
///   visitEnd.
///
/// @author Eric Bruneton
class MethodVisitor
{
public:
    MethodVisitor(int api)
        : MethodVisitor(api, nullptr)
    {}
    MethodVisitor(int api, MethodVisitor* mv)
        : api(api)
        , mv(mv)
    {
        if (api != Opcodes::ASM9 && api != Opcodes::ASM8 && api != Opcodes::ASM7 &&
            api != Opcodes::ASM6 && api != Opcodes::ASM5 && api != Opcodes::ASM4 &&
            api != Opcodes::ASM10_EXPERIMENTAL) {
            throw std::invalid_argument("Unsupported api " + std::to_string(api));
        }
    }
    virtual ~MethodVisitor() = default;

    MethodVisitor* getDelegate() { return mv; }

    virtual void visitParameter(ca::str::Utf8StringRef name, int access)
    {
        if (api < Opcodes::ASM5) {
            throw std::runtime_error("This feature requires ASM5");
        }
        if (mv) mv->visitParameter(name, access);
    }
    virtual AnnotationVisitor* visitAnnotationDefault()
    {
        if (mv) return mv->visitAnnotationDefault();
        return nullptr;
    }
    virtual AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible)
    {
        if (mv) return mv->visitAnnotation(descriptor, visible);
        return nullptr;
    }
    virtual AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                   ca::str::Utf8StringRef descriptor, bool visible)
    {
        if (api < Opcodes::ASM5) {
            throw std::runtime_error("This feature requires ASM5");
        }
        if (mv) return mv->visitTypeAnnotation(typeRef, typePath, descriptor, visible);
        return nullptr;
    }
    virtual void visitAnnotableParameterCount(int parameterCount, bool visible)
    {
        if (mv) mv->visitAnnotableParameterCount(parameterCount, visible);
    }
    virtual AnnotationVisitor* visitParameterAnnotation(int                    parameter,
                                                        ca::str::Utf8StringRef descriptor,
                                                        bool                   visible)
    {
        if (mv) return mv->visitParameterAnnotation(parameter, descriptor, visible);
        return nullptr;
    }
    virtual void visitAttribute(const Attribute& attribute)
    {
        if (mv) mv->visitAttribute(attribute);
    }
    virtual void visitCode()
    {
        if (mv) mv->visitCode();
    }
    virtual void visitFrame(int type, int numLocal, const std::vector<ca::core::Any>& local,
                            int numStack, const std::vector<ca::core::Any>& stack)
    {
        if (mv) mv->visitFrame(type, numLocal, local, numStack, stack);
    }
    /// @brief Report the original classfile position of the next instruction.
    ///
    ///
    /// ClassReader calls this immediately before the corresponding visitXxxInsn
    /// callback.
    /// Generated or replayed method visitors may omit it when no
    /// original classfile offset
    /// exists.
    virtual void visitInstructionMetadata(int offset, int length, int rawOpcode)
    {
        if (mv) mv->visitInstructionMetadata(offset, length, rawOpcode);
    }
    virtual void visitInsn(int opcode)
    {
        if (mv) mv->visitInsn(opcode);
    }
    virtual void visitIntInsn(int opcode, int operand)
    {
        if (mv) mv->visitIntInsn(opcode, operand);
    }
    virtual void visitVarInsn(int opcode, int varIndex)
    {
        if (mv) mv->visitVarInsn(opcode, varIndex);
    }
    virtual void visitTypeInsn(int opcode, ca::str::Utf8StringRef type)
    {
        if (mv) mv->visitTypeInsn(opcode, type);
    }
    virtual void visitFieldInsn(int opcode, ca::str::Utf8StringRef owner,
                                ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor)
    {
        if (mv) mv->visitFieldInsn(opcode, owner, name, descriptor);
    }
    /// @deprecated use 5-arg visitMethodInsn instead
    virtual void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner,
                                 ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor)
    {
        int opcodeAndSource = opcode | (api < Opcodes::ASM5 ? Opcodes::SOURCE_DEPRECATED : 0);
        visitMethodInsn(
            opcodeAndSource, owner, name, descriptor, opcode == Opcodes::INVOKEINTERFACE);
    }
    virtual void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner,
                                 ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                 bool isInterface)
    {
        if (api < Opcodes::ASM5 && (opcode & Opcodes::SOURCE_DEPRECATED) == 0) {
            if (isInterface != (opcode == Opcodes::INVOKEINTERFACE)) {
                throw std::runtime_error("INVOKESPECIAL/STATIC on interfaces requires ASM5");
            }
            visitMethodInsn(opcode, owner, name, descriptor);
            return;
        }
        if (mv)
            mv->visitMethodInsn(
                opcode & ~Opcodes::SOURCE_MASK, owner, name, descriptor, isInterface);
    }
    virtual void visitInvokeDynamicInsn(ca::str::Utf8StringRef            name,
                                        ca::str::Utf8StringRef            descriptor,
                                        const Handle&                     bootstrapMethod,
                                        const std::vector<ca::core::Any>& bootstrapMethodArguments)
    {
        if (api < Opcodes::ASM5) {
            throw std::runtime_error("This feature requires ASM5");
        }
        if (mv)
            mv->visitInvokeDynamicInsn(name, descriptor, bootstrapMethod, bootstrapMethodArguments);
    }
    virtual void visitJumpInsn(int opcode, Label* label)
    {
        if (mv) mv->visitJumpInsn(opcode, label);
    }
    virtual void visitLabel(Label* label)
    {
        if (mv) mv->visitLabel(label);
    }
    virtual void visitLdcInsn(const ca::core::Any& value)
    {
        if (api < Opcodes::ASM5) {
            if (auto* h = value.as<Handle>()) {
                (void)h;
                throw std::runtime_error("This feature requires ASM5");
            }
            if (auto* t = value.as<Type>(); t && t->getSort() == Type::METHOD) {
                throw std::runtime_error("This feature requires ASM5");
            }
        }
        if (api < Opcodes::ASM7 && value.is<ConstantDynamic>()) {
            throw std::runtime_error("This feature requires ASM7");
        }
        if (mv) mv->visitLdcInsn(value);
    }
    virtual void visitIincInsn(int varIndex, int increment)
    {
        if (mv) mv->visitIincInsn(varIndex, increment);
    }
    virtual void visitTableSwitchInsn(int min, int max, Label* dflt,
                                      const std::vector<Label*>& labels)
    {
        if (mv) mv->visitTableSwitchInsn(min, max, dflt, labels);
    }
    virtual void visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                                       const std::vector<Label*>& labels)
    {
        if (mv) mv->visitLookupSwitchInsn(dflt, keys, labels);
    }
    virtual void visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor, int numDimensions)
    {
        if (mv) mv->visitMultiANewArrayInsn(descriptor, numDimensions);
    }
    virtual AnnotationVisitor* visitInsnAnnotation(int typeRef, const TypePath* typePath,
                                                   ca::str::Utf8StringRef descriptor, bool visible)
    {
        if (api < Opcodes::ASM5) {
            throw std::runtime_error("This feature requires ASM5");
        }
        if (mv) return mv->visitInsnAnnotation(typeRef, typePath, descriptor, visible);
        return nullptr;
    }
    virtual void visitTryCatchBlock(Label* start, Label* end, Label* handler,
                                    ca::str::Utf8StringRef type)
    {
        if (mv) mv->visitTryCatchBlock(start, end, handler, type);
    }
    virtual AnnotationVisitor* visitTryCatchAnnotation(int typeRef, const TypePath* typePath,
                                                       ca::str::Utf8StringRef descriptor,
                                                       bool                   visible)
    {
        if (api < Opcodes::ASM5) {
            throw std::runtime_error("This feature requires ASM5");
        }
        if (mv) return mv->visitTryCatchAnnotation(typeRef, typePath, descriptor, visible);
        return nullptr;
    }
    virtual void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                    ca::str::Utf8StringRef signature, Label* start, Label* end,
                                    int index)
    {
        if (mv) mv->visitLocalVariable(name, descriptor, signature, start, end, index);
    }
    virtual AnnotationVisitor* visitLocalVariableAnnotation(int typeRef, const TypePath* typePath,
                                                            const std::vector<Label*>& start,
                                                            const std::vector<Label*>& end,
                                                            const std::vector<int>&    index,
                                                            ca::str::Utf8StringRef     descriptor,
                                                            bool                       visible)
    {
        if (api < Opcodes::ASM5) {
            throw std::runtime_error("This feature requires ASM5");
        }
        if (mv)
            return mv->visitLocalVariableAnnotation(
                typeRef, typePath, start, end, index, descriptor, visible);
        return nullptr;
    }
    virtual void visitLineNumber(int line, Label* start)
    {
        if (mv) mv->visitLineNumber(line, start);
    }
    virtual void visitMaxs(int maxStack, int maxLocals)
    {
        if (mv) mv->visitMaxs(maxStack, maxLocals);
    }
    virtual void visitEnd()
    {
        if (mv) mv->visitEnd();
    }

protected:
    const int      api;
    MethodVisitor* mv;
};

/// A visitor to visit a Java module. The methods of this class must be called
/// in the following order:
///   ( visitMainClass | ( visitPackage | visitRequire | visitExport | visitOpen |
///     visitUse | visitProvide )* ) visitEnd.
///
/// @author Remi Forax
/// @author Eric Bruneton
class ModuleVisitor
{
public:
    ModuleVisitor(int api)
        : ModuleVisitor(api, nullptr)
    {}
    ModuleVisitor(int api, ModuleVisitor* mv)
        : api(api)
        , mv(mv)
    {
        if (api != Opcodes::ASM9 && api != Opcodes::ASM8 && api != Opcodes::ASM7 &&
            api != Opcodes::ASM6 && api != Opcodes::ASM5 && api != Opcodes::ASM4 &&
            api != Opcodes::ASM10_EXPERIMENTAL) {
            throw std::invalid_argument("Unsupported api " + std::to_string(api));
        }
    }
    virtual ~ModuleVisitor() = default;

    ModuleVisitor* getDelegate() { return mv; }

    virtual void visitMainClass(ca::str::Utf8StringRef mainClass)
    {
        if (mv) mv->visitMainClass(mainClass);
    }
    virtual void visitPackage(ca::str::Utf8StringRef packaze)
    {
        if (mv) mv->visitPackage(packaze);
    }
    virtual void visitRequire(ca::str::Utf8StringRef module, int access,
                              ca::str::Utf8StringRef version)
    {
        if (mv) mv->visitRequire(module, access, version);
    }
    virtual void visitExport(ca::str::Utf8StringRef packaze, int access,
                             const std::vector<ca::str::Utf8StringRef>& modules)
    {
        if (mv) mv->visitExport(packaze, access, modules);
    }
    virtual void visitOpen(ca::str::Utf8StringRef packaze, int access,
                           const std::vector<ca::str::Utf8StringRef>& modules)
    {
        if (mv) mv->visitOpen(packaze, access, modules);
    }
    virtual void visitUse(ca::str::Utf8StringRef service)
    {
        if (mv) mv->visitUse(service);
    }
    virtual void visitProvide(ca::str::Utf8StringRef                     service,
                              const std::vector<ca::str::Utf8StringRef>& providers)
    {
        if (mv) mv->visitProvide(service, providers);
    }
    virtual void visitEnd()
    {
        if (mv) mv->visitEnd();
    }

protected:
    const int      api;
    ModuleVisitor* mv;
};

class RecordComponentVisitor
{
public:
    RecordComponentVisitor(int api)
        : RecordComponentVisitor(api, nullptr)
    {}
    RecordComponentVisitor(int api, RecordComponentVisitor* delegate)
        : api(api)
        , delegate(delegate)
    {
        if (api != Opcodes::ASM9 && api != Opcodes::ASM8 && api != Opcodes::ASM7 &&
            api != Opcodes::ASM6 && api != Opcodes::ASM5 && api != Opcodes::ASM4 &&
            api != Opcodes::ASM10_EXPERIMENTAL) {
            throw std::invalid_argument("Unsupported api " + std::to_string(api));
        }
    }
    virtual ~RecordComponentVisitor() = default;

    RecordComponentVisitor* getDelegate() { return delegate; }

    virtual AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible)
    {
        if (delegate) return delegate->visitAnnotation(descriptor, visible);
        return nullptr;
    }
    virtual AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                   ca::str::Utf8StringRef descriptor, bool visible)
    {
        if (delegate) return delegate->visitTypeAnnotation(typeRef, typePath, descriptor, visible);
        return nullptr;
    }
    virtual void visitAttribute(const Attribute& attribute)
    {
        if (delegate) delegate->visitAttribute(attribute);
    }
    virtual void visitEnd()
    {
        if (delegate) delegate->visitEnd();
    }

protected:
    const int               api;
    RecordComponentVisitor* delegate;
};

/// A visitor to visit a generic signature. The methods of this interface must
/// be called in one of the three following orders:
///
///   <i>ClassSignature</i> =
///     ( visitFormalTypeParameter visitClassBound? visitInterfaceBound* )*
///     ( visitSuperclass visitInterface* )
///
///   <i>MethodSignature</i> =
///     ( visitFormalTypeParameter visitClassBound? visitInterfaceBound* )*
///     ( visitParameterType* visitReturnType visitExceptionType* )
///
///   <i>TypeSignature</i> =
///     visitBaseType | visitTypeVariable | visitArrayType |
///     ( visitClassType visitTypeArgument* ( visitInnerClassType visitTypeArgument* )*
///       visitEnd )
///
/// @author Thomas Hallgren
/// @author Eric Bruneton
class SignatureVisitor
{
public:
    static constexpr char EXTENDS    = '+';
    static constexpr char SUPER      = '-';
    static constexpr char INSTANCEOF = '=';

    SignatureVisitor(int api)
        : SignatureVisitor(api, nullptr)
    {}
    SignatureVisitor(int api, SignatureVisitor* sv)
        : api(api)
        , sv(sv)
    {
        if (api != Opcodes::ASM9 && api != Opcodes::ASM8 && api != Opcodes::ASM7 &&
            api != Opcodes::ASM6 && api != Opcodes::ASM5 && api != Opcodes::ASM4 &&
            api != Opcodes::ASM10_EXPERIMENTAL) {
            throw std::invalid_argument("Unsupported api " + std::to_string(api));
        }
    }
    virtual ~SignatureVisitor() = default;

    SignatureVisitor* getDelegate() { return sv; }

    virtual void visitFormalTypeParameter(ca::str::Utf8StringRef name)
    {
        if (sv) sv->visitFormalTypeParameter(name);
    }
    virtual SignatureVisitor* visitClassBound()
    {
        if (sv) return sv->visitClassBound();
        return this;
    }
    virtual SignatureVisitor* visitInterfaceBound()
    {
        if (sv) return sv->visitInterfaceBound();
        return this;
    }
    virtual SignatureVisitor* visitSuperclass()
    {
        if (sv) return sv->visitSuperclass();
        return this;
    }
    virtual SignatureVisitor* visitInterface()
    {
        if (sv) return sv->visitInterface();
        return this;
    }
    virtual SignatureVisitor* visitParameterType()
    {
        if (sv) return sv->visitParameterType();
        return this;
    }
    virtual SignatureVisitor* visitReturnType()
    {
        if (sv) return sv->visitReturnType();
        return this;
    }
    virtual SignatureVisitor* visitExceptionType()
    {
        if (sv) return sv->visitExceptionType();
        return this;
    }
    virtual void visitBaseType(char descriptor)
    {
        if (sv) sv->visitBaseType(descriptor);
    }
    virtual void visitTypeVariable(ca::str::Utf8StringRef name)
    {
        if (sv) sv->visitTypeVariable(name);
    }
    virtual SignatureVisitor* visitArrayType()
    {
        if (sv) return sv->visitArrayType();
        return this;
    }
    virtual void visitClassType(ca::str::Utf8StringRef name)
    {
        if (sv) sv->visitClassType(name);
    }
    virtual void visitInnerClassType(ca::str::Utf8StringRef name)
    {
        if (sv) sv->visitInnerClassType(name);
    }
    virtual void visitTypeArgument()
    {
        if (sv) sv->visitTypeArgument();
    }
    virtual SignatureVisitor* visitTypeArgument(char wildcard)
    {
        if (sv) return sv->visitTypeArgument(wildcard);
        return this;
    }
    virtual void visitEnd()
    {
        if (sv) sv->visitEnd();
    }

protected:
    const int         api;
    SignatureVisitor* sv;
};

/// A visitor to visit a Java class. The methods of this class must be called
/// in the following order:
///   visit [ visitSource ] [ visitModule ] [ visitNestHost ] [ visitOuterClass ]
///   ( visitAnnotation | visitTypeAnnotation | visitAttribute )*
///   ( visitNestMember | [ visitPermittedSubclass ] | visitInnerClass |
///     visitRecordComponent | visitField | visitMethod )*
///   visitEnd.
///
/// @author Eric Bruneton
class ClassVisitor
{
public:
    ClassVisitor(int api)
        : ClassVisitor(api, nullptr)
    {}
    ClassVisitor(int api, ClassVisitor* cv)
        : api(api)
        , cv(cv)
    {
        if (api != Opcodes::ASM9 && api != Opcodes::ASM8 && api != Opcodes::ASM7 &&
            api != Opcodes::ASM6 && api != Opcodes::ASM5 && api != Opcodes::ASM4 &&
            api != Opcodes::ASM10_EXPERIMENTAL) {
            throw std::invalid_argument("Unsupported api " + std::to_string(api));
        }
    }
    virtual ~ClassVisitor() = default;

    ClassVisitor* getDelegate() { return cv; }

    virtual void visit(int version, int access, ca::str::Utf8StringRef name,
                       ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
                       const std::vector<ca::str::Utf8StringRef>& interfaces)
    {
        if (api < Opcodes::ASM8 && (access & Opcodes::ACC_RECORD) != 0) {
            throw std::runtime_error("Records requires ASM8");
        }
        if (cv) cv->visit(version, access, name, signature, superName, interfaces);
    }
    virtual void visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug)
    {
        if (cv) cv->visitSource(source, debug);
    }
    virtual ModuleVisitor* visitModule(ca::str::Utf8StringRef name, int access,
                                       ca::str::Utf8StringRef version)
    {
        if (api < Opcodes::ASM6) {
            throw std::runtime_error("Module requires ASM6");
        }
        if (cv) return cv->visitModule(name, access, version);
        return nullptr;
    }
    virtual void visitNestHost(ca::str::Utf8StringRef nestHost)
    {
        if (api < Opcodes::ASM7) {
            throw std::runtime_error("NestHost requires ASM7");
        }
        if (cv) cv->visitNestHost(nestHost);
    }
    virtual void visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                 ca::str::Utf8StringRef descriptor)
    {
        if (cv) cv->visitOuterClass(owner, name, descriptor);
    }
    virtual AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible)
    {
        if (cv) return cv->visitAnnotation(descriptor, visible);
        return nullptr;
    }
    virtual AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                                   ca::str::Utf8StringRef descriptor, bool visible)
    {
        if (api < Opcodes::ASM5) {
            throw std::runtime_error("TypeAnnotation requires ASM5");
        }
        if (cv) return cv->visitTypeAnnotation(typeRef, typePath, descriptor, visible);
        return nullptr;
    }
    virtual void visitAttribute(const Attribute& attribute)
    {
        if (cv) cv->visitAttribute(attribute);
    }
    virtual void visitNestMember(ca::str::Utf8StringRef nestMember)
    {
        if (api < Opcodes::ASM7) {
            throw std::runtime_error("NestMember requires ASM7");
        }
        if (cv) cv->visitNestMember(nestMember);
    }
    virtual void visitPermittedSubclass(ca::str::Utf8StringRef permittedSubclass)
    {
        if (api < Opcodes::ASM9) {
            throw std::runtime_error("PermittedSubclasses requires ASM9");
        }
        if (cv) cv->visitPermittedSubclass(permittedSubclass);
    }
    virtual void visitInnerClass(ca::str::Utf8StringRef name, ca::str::Utf8StringRef outerName,
                                 ca::str::Utf8StringRef innerName, int access)
    {
        if (cv) cv->visitInnerClass(name, outerName, innerName, access);
    }
    virtual RecordComponentVisitor* visitRecordComponent(ca::str::Utf8StringRef name,
                                                         ca::str::Utf8StringRef descriptor,
                                                         ca::str::Utf8StringRef signature)
    {
        if (api < Opcodes::ASM8) {
            throw std::runtime_error("Record requires ASM8");
        }
        if (cv) return cv->visitRecordComponent(name, descriptor, signature);
        return nullptr;
    }
    virtual FieldVisitor* visitField(int access, ca::str::Utf8StringRef name,
                                     ca::str::Utf8StringRef descriptor,
                                     ca::str::Utf8StringRef signature, const ca::core::Any& value)
    {
        if (cv) return cv->visitField(access, name, descriptor, signature, value);
        return nullptr;
    }
    virtual MethodVisitor* visitMethod(int access, ca::str::Utf8StringRef name,
                                       ca::str::Utf8StringRef                     descriptor,
                                       ca::str::Utf8StringRef                     signature,
                                       const std::vector<ca::str::Utf8StringRef>& exceptions)
    {
        if (cv) return cv->visitMethod(access, name, descriptor, signature, exceptions);
        return nullptr;
    }
    virtual void visitEnd()
    {
        if (cv) cv->visitEnd();
    }

protected:
    const int     api;
    ClassVisitor* cv;
};

}   // namespace jbytecode::core
