#pragma once

#include <string>

#include "jbytecode/core/visitors.hpp"
#include "libca/core/datatype.hpp"
#include "jbytecode/core/byte_vector.hpp"
#include "jbytecode/core/symbol_table.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/frame.hpp"
#include "jbytecode/core/constants.hpp"
#include <vector>

#include "libca/core/any.hpp"
#include <unordered_map>

namespace jbytecode::core {

/// 注解序列化器。继承 AnnotationVisitor，收集 visit 数据，
/// 最终通过 putAnnotation() 写入 ByteVector。
///
/// 被 ClassWriter / FieldWriter / MethodWriter 使用，
/// 作为 visitAnnotation() 的返回值。
class AnnotationWriter : public AnnotationVisitor
{
public:
    AnnotationWriter(SymbolTable& symbols, ca::str::Utf8StringRef descriptor, bool named = true)
        : AnnotationVisitor(Opcodes::ASM4)
        , symbols_(symbols)
        , typeIndex_(symbols.addConstantUtf8(descriptor))
        , named_(named)
    {}

    ~AnnotationWriter() override
    {
        for (auto* c : children_) delete c;
    }

    void               visit(ca::str::Utf8StringRef name, const ca::core::Any& value) override;
    void               visitEnum(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                 ca::str::Utf8StringRef value) override;
    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef name,
                                       ca::str::Utf8StringRef descriptor) override;
    AnnotationVisitor* visitArray(ca::str::Utf8StringRef name) override;
    void               visitEnd() override {}

    void        putAnnotation(ByteVector& out);
    void        putAnnotationDefault(ByteVector& out);
    void        collectConstants();
    static void putAnnotations(ByteVector& out, const std::vector<AnnotationWriter*>& annotations);

    // Type annotation support
    void        setTypeAnnotation(int typeRef, const TypePath* typePath);
    /// LOCAL_VARIABLE / RESOURCE_VARIABLE 目标表（JVMS 4.7.20.1 offset 形态）：
    /// start/end 槽携带 Label，序列化时读解析后的字节码偏移。
    void        setLocalVariableTarget(const std::vector<Label*>& start,
                                       const std::vector<Label*>& end,
                                       const std::vector<int>&    index);
    void        putTypeAnnotation(ByteVector& out);
    static void putTypeAnnotations(ByteVector&                           out,
                                   const std::vector<AnnotationWriter*>& annotations);

private:
    struct Element
    {
        ca::str::Utf8StringRef name;
        char                   tag  = 's';
        int                    ival = 0;
        ca::i64                lval = 0;
        float                  fval = 0;
        double                 dval = 0;
        ca::str::Utf8StringRef sval;
        ca::str::Utf8StringRef enumType;
        ca::str::Utf8StringRef enumName;
        ca::str::Utf8StringRef cls;
        AnnotationWriter*      nested = nullptr;
    };

    void writeElement(ByteVector& out, const Element& el);

    SymbolTable&        symbols_;
    int                 typeIndex_;
    bool                named_;
    int                 typeRef_ = -1;
    TypePath            typePath_;
    std::vector<Element>            elements_;
    std::vector<AnnotationWriter*>  children_;
    std::vector<Label*>             lvStart_;   ///< LOCAL_VARIABLE/RESOURCE 目标表
    std::vector<Label*>             lvEnd_;
    std::vector<int>                lvIndex_;
};

class SignatureWriter : public SignatureVisitor
{
public:
    SignatureWriter()
        : SignatureVisitor(Opcodes::ASM9)
    {}

    void              visitFormalTypeParameter(ca::str::Utf8StringRef name) override;
    SignatureVisitor* visitClassBound() override { return this; }
    SignatureVisitor* visitInterfaceBound() override;
    SignatureVisitor* visitSuperclass() override;
    SignatureVisitor* visitInterface() override { return this; }
    SignatureVisitor* visitParameterType() override;
    SignatureVisitor* visitReturnType() override;
    SignatureVisitor* visitExceptionType() override;
    void              visitBaseType(char descriptor) override;
    void              visitTypeVariable(ca::str::Utf8StringRef name) override;
    SignatureVisitor* visitArrayType() override;
    void              visitClassType(ca::str::Utf8StringRef name) override;
    void              visitInnerClassType(ca::str::Utf8StringRef name) override;
    void              visitTypeArgument() override;
    SignatureVisitor* visitTypeArgument(char wildcard) override;
    void              visitEnd() override;

    std::string toString() const;

private:
    ca::str::Utf8StringBuilder stringBuilder;
    bool                       hasFormals    = false;
    bool                       hasParameters = false;
    int                        argumentStack = 1;

    void endFormals();
    void endArguments();
};


/// 字段数据容器。继承 FieldVisitor，visit 时收集数据，
/// 由 ClassWriter 统一序列化。
class FieldWriter : public FieldVisitor
{
public:
    FieldWriter(SymbolTable& symbols, ca::i32 access, ca::str::Utf8StringRef name,
                ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                const ca::core::Any& value);

    ~FieldWriter() override
    {
        for (auto* a : ownedAnns_) delete a;
    }

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible) override;
    AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                           ca::str::Utf8StringRef descriptor,
                                           bool                   visible) override;
    void visitAttribute(const Attribute& attribute) override { attrs_.push_back(attribute); }
    void visitEnd() override {}

    ca::i32                               access() const { return access_; }
    ca::i32                               nameIndex() const { return nameIndex_; }
    ca::i32                               descIndex() const { return descIndex_; }
    ca::i32                               signatureIndex() const { return signatureIndex_; }
    bool                                  hasConstantValue() const { return hasConstantValue_; }
    ca::i32                               constantValueIndex() const { return constantValueIdx_; }
    const std::vector<AnnotationWriter*>& visibleAnnotations() const { return visibleAnnotations_; }
    const std::vector<AnnotationWriter*>& invisibleAnnotations() const
    {
        return invisibleAnnotations_;
    }
    const std::vector<AnnotationWriter*>& visibleTypeAnnotations() const
    {
        return visibleTypeAnnotations_;
    }
    const std::vector<AnnotationWriter*>& invisibleTypeAnnotations() const
    {
        return invisibleTypeAnnotations_;
    }
    const std::vector<Attribute>& attrs() const { return attrs_; }
    void                          collectConstants();

private:
    SymbolTable&                   symbols_;
    ca::i32                        access_;
    ca::i32                        nameIndex_;
    ca::i32                        descIndex_;
    ca::i32                        signatureIndex_   = 0;
    bool                           hasConstantValue_ = false;
    ca::i32                        constantValueIdx_ = 0;
    std::vector<AnnotationWriter*> visibleAnnotations_;
    std::vector<AnnotationWriter*> invisibleAnnotations_;
    std::vector<AnnotationWriter*> visibleTypeAnnotations_;
    std::vector<AnnotationWriter*> invisibleTypeAnnotations_;
    std::vector<AnnotationWriter*> ownedAnns_;
    std::vector<Attribute>         attrs_;
};

/// 方法序列化器。继承 MethodVisitor，在 visit* 时收集方法数据，
/// 由 ClassWriter 统一调用 toMethodInfo() 序列化。
class ClassWriter;
class RecordComponentWriter;
class MethodWriter : public MethodVisitor
{
public:
    // ---- Compute mode constants (matching ASM MethodWriter) ----
    static constexpr int COMPUTE_NOTHING                         = 0;
    static constexpr int COMPUTE_MAX_STACK_AND_LOCAL             = 1;
    static constexpr int COMPUTE_MAX_STACK_AND_LOCAL_FROM_FRAMES = 2;
    static constexpr int COMPUTE_INSERTED_FRAMES                 = 3;
    static constexpr int COMPUTE_ALL_FRAMES                      = 4;

    MethodWriter(SymbolTable& symbols, ClassWriter& classWriter, ca::i32 access,
                 ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                 ca::str::Utf8StringRef                     signature,
                 const std::vector<ca::str::Utf8StringRef>& exceptions,
                 int compute = COMPUTE_NOTHING, ICommonSuperClassProvider* typeResolver = nullptr);

    ~MethodWriter() override
    {
        for (auto* a : ownedAnns_) delete a;
    }

    void               visitParameter(ca::str::Utf8StringRef name, int access) override;
    AnnotationVisitor* visitAnnotationDefault() override;
    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible) override;
    AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                           ca::str::Utf8StringRef descriptor,
                                           bool                   visible) override;
    AnnotationVisitor* visitParameterAnnotation(int parameter, ca::str::Utf8StringRef descriptor,
                                                bool visible) override;
    /// 局部变量（含资源变量）类型注解：Code 属性 Runtime(In)VisibleTypeAnnotations 的
    /// LOCAL_VARIABLE/RESOURCE_VARIABLE 目标。目标表（start/end/index）随注解收集，
    /// start_pc/length 在序列化阶段读 Label 解析值（与 visitLocalVariable 的 LVT
    /// 写出同规），支撑 visit 在 visitCode 之前的前向引用形态。
    AnnotationVisitor* visitLocalVariableAnnotation(int typeRef, const TypePath* typePath,
                                                    const std::vector<Label*>& start,
                                                    const std::vector<Label*>& end,
                                                    const std::vector<int>&    index,
                                                    ca::str::Utf8StringRef     descriptor,
                                                    bool visible) override;
    void visitAttribute(const Attribute& attribute) override { attrs_.push_back(attribute); }

    void visitCode() override;
    void visitInsn(int opcode) override;
    void visitIntInsn(int opcode, int operand) override;
    void visitVarInsn(int opcode, int varIndex) override;
    void visitTypeInsn(int opcode, ca::str::Utf8StringRef type) override;
    void visitFieldInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef descriptor) override;
    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor, bool isInterface) override;
    void visitInvokeDynamicInsn(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                const Handle&                     bsm,
                                const std::vector<ca::core::Any>& bsmArgs) override;
    void visitJumpInsn(int opcode, Label* label) override;
    void visitLabel(Label* label) override;
    void visitLdcInsn(const ca::core::Any& value) override;
    void visitIincInsn(int varIndex, int increment) override;
    void visitTableSwitchInsn(int min, int max, Label* dflt,
                              const std::vector<Label*>& labels) override;
    void visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                               const std::vector<Label*>& labels) override;
    void visitFrame(int type, int numLocal, const std::vector<ca::core::Any>& local, int numStack,
                    const std::vector<ca::core::Any>& stack) override;
    void visitMultiANewArrayInsn(ca::str::Utf8StringRef descriptor, int numDimensions) override;
    void visitTryCatchBlock(Label* start, Label* end, Label* handler,
                            ca::str::Utf8StringRef type) override;
    void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                            ca::str::Utf8StringRef signature, Label* start, Label* end,
                            int index) override;
    void visitLineNumber(int line, Label* start) override;
    void visitMaxs(int maxStack, int maxLocals) override;
    void visitEnd() override;

    // ---- Two-phase serialization (matching ASM putMethodInfo) ----
    int  computeMethodInfoSize();
    void putMethodInfo(ByteVector& out);
    void collectConstants();

    // ---- Frame emit API (called by Frame::accept, matching ASM) ----
    int  visitFrameStart(int offset, int numLocal, int numStack);
    void visitAbstractType(int frameIndex, int abstractType);
    void visitFrameEnd();

    ca::i32 access() const { return access_; }
    ca::i32 nameIndex() const { return nameIndex_; }
    ca::i32 descIndex() const { return descIndex_; }

private:
    friend class ClassWriter;

    // ---- Helper matching ASM MethodWriter ----
    void       addSuccessorToCurrentBasicBlock(int info, Label* successor);
    void       endCurrentBasicBlockWithNoSuccessor();
    void       computeMaxStackAndLocal();
    void       computeAllFrames();
    void       putFrame();
    void       putAbstractTypes(int start, int end);
    static int getArgumentsAndReturnSizes(const ca::str::Utf8StringRef& descriptor);

    SymbolTable&               symbols_;
    ClassWriter&               classWriter_;
    ICommonSuperClassProvider* typeResolver_;
    const int                  compute_;
    ca::i32                    access_;
    ca::i32                    nameIndex_;
    ca::i32                    descIndex_;
    ca::str::Utf8StringRef     nameStr_;
    ca::str::Utf8StringRef     descStr_;
    ca::i32                    signatureIndex_ = 0;
    std::vector<ca::i32>       exceptionIndices_;

    AnnotationWriter*                              annotationDefault_ = nullptr;
    std::vector<AnnotationWriter*>                 visibleAnnotations_;
    std::vector<AnnotationWriter*>                 invisibleAnnotations_;
    std::vector<AnnotationWriter*>                 visibleTypeAnnotations_;
    std::vector<AnnotationWriter*>                 invisibleTypeAnnotations_;
    std::vector<AnnotationWriter*>                 codeVisibleTypeAnnotations_;
    std::vector<AnnotationWriter*>                 codeInvisibleTypeAnnotations_;
    std::vector<std::pair<int, AnnotationWriter*>> visibleParameterAnnotations_;
    std::vector<std::pair<int, AnnotationWriter*>> invisibleParameterAnnotations_;
    std::vector<AnnotationWriter*>                 ownedAnns_;
    std::vector<Attribute>                         attrs_;

    // ---- Code attribute fields (ASM order) ----
    ca::i32    maxStack_  = 0;
    ca::i32    maxLocals_ = 0;
    ByteVector code_;
    int        lastBytecodeOffset_ = 0;

    // Exception handlers (ASM: Handler chain)
    Handler* firstHandler_ = nullptr;
    Handler* lastHandler_  = nullptr;

    // Line number table
    ByteVector lineNumberTable_;
    int        lineNumberTableLength_ = 0;

    // Local variable tables
    ByteVector localVariableTable_;
    int        localVariableTableLength_ = 0;
    ByteVector localVariableTypeTable_;
    int        localVariableTypeTableLength_ = 0;

    // StackMapTable
    ByteVector       stackMapTableEntries_;
    int              stackMapTableNumberOfEntries_ = 0;
    std::vector<int> currentFrame_;
    std::vector<int> previousFrame_;

    // ---- Non-standard attributes ----
    AnnotationWriter* lastCodeRuntimeVisibleTypeAnnotation_   = nullptr;
    AnnotationWriter* lastCodeRuntimeInvisibleTypeAnnotation_ = nullptr;
    Attribute*        firstCodeAttribute_                     = nullptr;

    // ---- Method-level attributes ----
    AnnotationWriter* lastRuntimeVisibleAnnotation_       = nullptr;
    AnnotationWriter* lastRuntimeInvisibleAnnotation_     = nullptr;
    AnnotationWriter* lastRuntimeVisibleTypeAnnotation_   = nullptr;
    AnnotationWriter* lastRuntimeInvisibleTypeAnnotation_ = nullptr;
    ByteVector        defaultValue_;
    int               parametersCount_ = 0;
    ByteVector        parameters_;
    Attribute*        firstAttribute_ = nullptr;

    // ---- CFG / stack frame computation (ASM L467-544) ----
    Label* firstBasicBlock_      = nullptr;
    Label* lastBasicBlock_       = nullptr;
    Label* currentBasicBlock_    = nullptr;
    int    relativeStackSize_    = 0;
    int    maxRelativeStackSize_ = 0;
    int    currentLocals_        = 0;
    bool   hasSubroutines_       = false;
    bool   hasAsmInstructions_   = false;

    // ---- Source copy optimization ----
    int sourceOffset_ = 0;
    int sourceLength_ = 0;
};


struct BootstrapMethodEntry
{
    int              bsmMethodRefIdx;   // CP index of CONSTANT_MethodHandle
    std::vector<int> bsmArgs;           // CP indices of arguments
};

/// @brief A ModuleVisitor that generates the Module, ModulePackages and ModuleMainClass
/// JVMS attributes. 对标 org.objectweb.asm.ModuleWriter.
///
/// 在 visit* 阶段将数据直接写入内部的 ByteVector buffer，
/// 在 putAttributes() 阶段输出完整的 Module/ModulePackages/ModuleMainClass 属性。
class ModuleWriter : public ModuleVisitor
{
public:
    ModuleWriter(SymbolTable& symbolTable, ca::str::Utf8StringRef name, int access,
                 ca::str::Utf8StringRef version);

    void visitMainClass(ca::str::Utf8StringRef mainClass) override;
    void visitPackage(ca::str::Utf8StringRef packaze) override;
    void visitRequire(ca::str::Utf8StringRef module, int access,
                      ca::str::Utf8StringRef version) override;
    void visitExport(ca::str::Utf8StringRef packaze, int access,
                     const std::vector<ca::str::Utf8StringRef>& modules) override;
    void visitOpen(ca::str::Utf8StringRef packaze, int access,
                   const std::vector<ca::str::Utf8StringRef>& modules) override;
    void visitUse(ca::str::Utf8StringRef service) override;
    void visitProvide(ca::str::Utf8StringRef                     service,
                      const std::vector<ca::str::Utf8StringRef>& providers) override;
    void visitEnd() override {}

    int  getAttributeCount() const;
    int  computeAttributesSize();
    void putAttributes(ByteVector& output);

private:
    SymbolTable& symbolTable_;
    int          moduleNameIndex_;
    int          moduleFlags_;
    int          moduleVersionIndex_;

    int        requiresCount_ = 0;
    ByteVector requires_;
    int        exportsCount_ = 0;
    ByteVector exports_;
    int        opensCount_ = 0;
    ByteVector opens_;
    int        usesCount_ = 0;
    ByteVector usesIndex_;
    int        providesCount_ = 0;
    ByteVector provides_;

    int        packageCount_ = 0;
    ByteVector packageIndex_;
    int        mainClassIndex_ = 0;
};

class ClassReader;
/// @brief Serializes visited class structure into class-file bytes.
///
/// @warning The arena passed to the constructor is borrowed. It must outlive the writer and all
///          Utf8StringRef values passed to visit* APIs while toByteArray() is building the
///          constant pool. Use the same job/test arena as the corresponding ClassReader when
///          doing reader -> tree -> writer roundtrips.
class ClassWriter : public ClassVisitor
{
public:
    static constexpr ca::i32 COMPUTE_MAXS   = 1;
    static constexpr ca::i32 COMPUTE_FRAMES = 2;

    /// @param arena Borrowed UTF-8 storage used by the internal SymbolTable.
    explicit ClassWriter(ca::str::Utf8StringArena& arena);
    /// @param arena Borrowed UTF-8 storage used by the internal SymbolTable.
    /// @param flags COMPUTE_MAXS / COMPUTE_FRAMES flags.
    ClassWriter(ca::str::Utf8StringArena& arena, ca::i32 flags);
    /// @param arena  Borrowed UTF-8 storage shared with the source reader.
    /// @param reader Source reader. Constant-pool copy optimization is not implemented yet.
    /// @param flags  COMPUTE_MAXS / COMPUTE_FRAMES flags.
    ClassWriter(ca::str::Utf8StringArena& arena, const ClassReader& reader, ca::i32 flags);
    ~ClassWriter() override;

    void visit(int version, int access, ca::str::Utf8StringRef name,
               ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override;
    void visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug) override;
    void visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef desc) override;
    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible) override;
    AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                           ca::str::Utf8StringRef descriptor,
                                           bool                   visible) override;
    void           visitInnerClass(ca::str::Utf8StringRef name, ca::str::Utf8StringRef outerName,
                                   ca::str::Utf8StringRef innerName, int access) override;
    void           visitNestHost(ca::str::Utf8StringRef nestHost) override;
    void           visitNestMember(ca::str::Utf8StringRef nestMember) override;
    void           visitPermittedSubclass(ca::str::Utf8StringRef subclass) override;
    void           visitAttribute(const Attribute& attribute) override;
    ModuleVisitor* visitModule(ca::str::Utf8StringRef name, int access,
                               ca::str::Utf8StringRef version) override;
    RecordComponentVisitor* visitRecordComponent(ca::str::Utf8StringRef name,
                                                 ca::str::Utf8StringRef descriptor,
                                                 ca::str::Utf8StringRef signature) override;
    FieldVisitor*           visitField(int access, ca::str::Utf8StringRef name,
                                       ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                                       const ca::core::Any& value) override;
    MethodVisitor*          visitMethod(int access, ca::str::Utf8StringRef name,
                                        ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature,
                                        const std::vector<ca::str::Utf8StringRef>& exceptions) override;
    void                    visitEnd() override {}

    /// @brief 序列化已访问的 class 结构。
    /// @throws ClassTooLargeException 常量池条目超过 65535。
    /// @throws MethodTooLargeException 单个方法 Code 超过 65535 字节。
    /// @throws FrameComputationException COMPUTE_FRAMES 的 fixpoint 不收敛或超出预算。
    std::vector<ca::u8>    toByteArray();
    ca::str::Utf8StringRef getClassName() const { return className_; }
    ca::i32                version() const { return version_; }
    ca::str::Utf8StringRef getCommonSuperClass(ca::str::Utf8StringRef type1,
                                               ca::str::Utf8StringRef type2);

    void setFlags(ca::i32 flags);
    bool hasFlags(ca::i32 flags) const;
    void setTypeResolver(ICommonSuperClassProvider* resolver) { typeResolver_ = resolver; }

    ca::i32 addBootstrapMethod(ca::i32 methodHandleIdx, const std::vector<ca::i32>& argIndices);
    // ---- new* helpers ----
    // NOTE: These will be rewritten in Layer 4b with new SymbolTable API.
    // For now, inline implementations are disabled to let L3 compile.
    ca::i32 newUTF8(const ca::str::Utf8StringRef& v);
    ca::i32 newClass(const ca::str::Utf8StringRef& name);
    ca::i32 newInteger(ca::i32 v);
    ca::i32 newFloat(float v);
    ca::i32 newLong(ca::i64 v);
    ca::i32 newDouble(double v);
    ca::i32 newNameType(ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc);
    ca::i32 newField(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                     ca::str::Utf8StringRef desc);
    ca::i32 newMethod(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                      ca::str::Utf8StringRef desc, bool isInterface);
    ca::i32 newMethodType(ca::str::Utf8StringRef methodDescriptor);
    ca::i32 newModule(ca::str::Utf8StringRef moduleName);
    ca::i32 newPackage(ca::str::Utf8StringRef packageName);
    ca::i32 newHandle(int tag, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                      ca::str::Utf8StringRef desc);
    ca::i32 newHandle(int tag, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                      ca::str::Utf8StringRef desc, bool isInterface);
    ca::i32 newConstantDynamic(ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                               const Handle&                     bootstrapMethod,
                               const std::vector<ca::core::Any>& bootstrapMethodArguments = {});
    ca::i32 newInvokeDynamic(ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                             const Handle&                     bootstrapMethod,
                             const std::vector<ca::core::Any>& bootstrapMethodArguments = {});
    ca::i32 newConst(const ca::core::Any& cst);
    // ---- end new* helpers ----

private:
    ByteVector  pool_;
    SymbolTable symbols_;

    int                    version_   = 0;
    int                    access_    = 0;
    int                    thisClass_ = 0;
    ca::str::Utf8StringRef className_;
    int                    superClass_ = 0;
    std::vector<int>       interfaceIndices_;
    int                    sourceFileIndex_ = 0;
    int                    signatureIndex_  = 0;
    ca::str::Utf8StringRef nestHost_;
    ca::str::Utf8StringRef outerClass_;
    int                    outerClassIndex_  = 0;
    int                    outerMethodIndex_ = 0;

    std::vector<FieldWriter*>      fields_;
    std::vector<MethodWriter*>     methods_;
    std::vector<AnnotationWriter*> visibleAnnotations_;
    std::vector<AnnotationWriter*> invisibleAnnotations_;
    std::vector<AnnotationWriter*> visibleTypeAnnotations_;
    std::vector<AnnotationWriter*> invisibleTypeAnnotations_;
    std::vector<AnnotationWriter*> ownedAnns_;

    struct InnerClassEntry
    {
        int innerName, outerName, innerSimpleName, access;
    };
    std::vector<InnerClassEntry>        innerClasses_;
    std::vector<Attribute>              classAttrs_;
    std::vector<RecordComponentWriter*> recordComponents_;
    std::vector<int>                    nestMembers_;
    std::vector<int>                    permittedSubclasses_;
    ca::str::Utf8StringRef              sourceDebug_;
    bool                                computeMaxs_   = false;
    bool                                computeFrames_ = false;
    ICommonSuperClassProvider*          typeResolver_  = nullptr;

    class ModuleWriter* moduleWriter_ = nullptr;

    std::vector<BootstrapMethodEntry> bootstrapMethods_;
    struct BsmKey
    {
        ca::i32              handleIdx;
        std::vector<ca::i32> args;
    };
    struct BsmKeyHash
    {
        size_t operator()(const BsmKey& k) const
        {
            size_t h = std::hash<ca::i32>{}(k.handleIdx);
            for (auto a : k.args) h ^= std::hash<ca::i32>{}(a) << 1;
            return h;
        }
    };
    struct BsmKeyEq
    {
        bool operator()(const BsmKey& a, const BsmKey& b) const
        {
            return a.handleIdx == b.handleIdx && a.args == b.args;
        }
    };
    std::unordered_map<BsmKey, ca::i32, BsmKeyHash, BsmKeyEq> bsmCache_;
};


/// @brief Record component writer — collects data from visitRecordComponent
///        and serializes it as part of the Record attribute.
class RecordComponentWriter : public RecordComponentVisitor
{
public:
    RecordComponentWriter(SymbolTable& symbols, ca::str::Utf8StringRef name,
                          ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature);

    ~RecordComponentWriter() override;

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor, bool visible) override;

    AnnotationVisitor* visitTypeAnnotation(int typeRef, const TypePath* typePath,
                                           ca::str::Utf8StringRef descriptor,
                                           bool                   visible) override;

    void visitEnd() override {}

    void    collectConstants();
    ca::i32 nameIndex() const { return nameIndex_; }
    ca::i32 descIndex() const { return descIndex_; }
    int     computeComponentSize() const;
    void    putComponent(ByteVector& out);

private:
    SymbolTable&                   symbols_;
    ca::i32                        nameIndex_;
    ca::i32                        descIndex_;
    ca::i32                        signatureIndex_ = 0;
    std::vector<AnnotationWriter*> visibleAnnotations_;
    std::vector<AnnotationWriter*> invisibleAnnotations_;
    std::vector<AnnotationWriter*> visibleTypeAnnotations_;
    std::vector<AnnotationWriter*> invisibleTypeAnnotations_;
    std::vector<AnnotationWriter*> ownedAnns_;
};

}   // namespace jbytecode::core
