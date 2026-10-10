#pragma once

#include "libca/core/any.hpp"
#include <vector>

#include "garena/arena.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/visitors.hpp"
#include "jbytecode/tree/insn_list.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

namespace jbytecode::tree {

/// MethodNode/ClassNode 内容物 arena 的默认首块容量（惰性分配：无内容不占内存）。
inline constexpr ca::usize kNodeArenaFirstChunkBytes = 4096;
/// FieldNode 内容物（注解）arena 首块容量：字段注解罕见且小。
inline constexpr ca::usize kFieldArenaFirstChunkBytes = 1024;

/// @brief Tree model for a JVM annotation (element-value pairs).
class AnnotationNode : public jbytecode::core::AnnotationVisitor
{
public:
    ca::str::Utf8StringRef desc;

    struct Element
    {
        ca::str::Utf8StringRef name;
        char                   tag = 's';          // B/C/D/F/I/J/S/Z/s/e/c/@/[
        ca::str::Utf8String    sval;               // string value (for 's', 'c', enum type/name)
        int                    ival   = 0;         // for 'I'
        ca::i64                lval   = 0;         // for 'J'
        float                  fval   = 0;         // for 'F'
        double                 dval   = 0;         // for 'D'
        AnnotationNode*        nested = nullptr;   // for '@' and '['
    };
    std::vector<Element>         elements;
    std::vector<AnnotationNode*> children;

    /// @param contentArena 嵌套子注解的分配归宿（与宿主 MethodNode/ClassNode/FieldNode 的
    /// 内容 arena 同域）；子注解随该 arena 统一析构，本类析构不再逐个 delete。
    AnnotationNode(garena::Arena& contentArena, ca::str::Utf8StringRef desc)
        : jbytecode::core::AnnotationVisitor(jbytecode::core::Opcodes::ASM9)
        , desc(desc)
        , contentArena_(contentArena)
    {}

    ~AnnotationNode() override = default;

    void visit(ca::str::Utf8StringRef name, const ca::core::Any& value) override;
    void visitEnum(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                   ca::str::Utf8StringRef value) override;
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef name,
                                                        ca::str::Utf8StringRef descriptor) override;
    jbytecode::core::AnnotationVisitor* visitArray(ca::str::Utf8StringRef name) override;
    void                                visitEnd() override {}

    void accept(jbytecode::core::AnnotationVisitor* av);
    void check(int api) const { (void)api; }

private:
    garena::Arena& contentArena_;
};


/// Type annotation node (equivalent to ASM's TypeAnnotationNode).
/// Extends AnnotationNode with target reference and path info for type annotations.
class TypeAnnotationNode : public AnnotationNode
{
public:
    int typeRef;   ///< Target type reference constant (see jbytecode/TypeReference)
    jbytecode::core::TypePath
        typePathValue;                     ///< Owned copy; reader context TypePath is transient.
    jbytecode::core::TypePath* typePath;   ///< Stable pointer to typePathValue, null when no path.

    TypeAnnotationNode(garena::Arena& contentArena, int typeRef,
                       const jbytecode::core::TypePath* typePath,
                       ca::str::Utf8StringRef desc)
        : AnnotationNode(contentArena, desc)
        , typeRef(typeRef)
        , typePath(nullptr)
    {
        if (typePath) {
            typePathValue  = *typePath;
            this->typePath = &typePathValue;
        }
    }

    void accept(jbytecode::core::AnnotationVisitor* av);
};

/// Local variable type annotation node (equivalent to ASM's LocalVariableAnnotationNode).
/// Associates a type annotation with one or more local variable ranges.
class LocalVariableAnnotationNode : public TypeAnnotationNode
{
public:
    std::vector<jbytecode::core::Label*> start;   ///< Start labels of each variable range
    std::vector<jbytecode::core::Label*> end;     ///< End labels of each variable range
    std::vector<int>                     index;   ///< Local variable indices

    LocalVariableAnnotationNode(garena::Arena& contentArena, int typeRef,
                                const jbytecode::core::TypePath* typePath,
                                const std::vector<jbytecode::core::Label*>& start,
                                const std::vector<jbytecode::core::Label*>& end,
                                const std::vector<int>& index, ca::str::Utf8StringRef desc)
        : TypeAnnotationNode(contentArena, typeRef, typePath, desc)
        , start(start)
        , end(end)
        , index(index)
    {}
};

struct LocalVariableNode
{
    ca::str::Utf8StringRef  name;
    ca::str::Utf8StringRef  descriptor;
    ca::str::Utf8StringRef  signature;
    jbytecode::core::Label* start = nullptr;
    jbytecode::core::Label* end   = nullptr;
    int                     index = 0;

    LocalVariableNode() = default;
    LocalVariableNode(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                      ca::str::Utf8StringRef signature, jbytecode::core::Label* start,
                      jbytecode::core::Label* end, int index)
        : name(name)
        , descriptor(descriptor)
        , signature(signature)
        , start(start)
        , end(end)
        , index(index)
    {}
};

struct ModuleRequireNode
{
    ca::str::Utf8StringRef module;
    int                    access = 0;
    ca::str::Utf8StringRef version;

    ModuleRequireNode() = default;
    ModuleRequireNode(ca::str::Utf8StringRef module, int access, ca::str::Utf8StringRef version)
        : module(module)
        , access(access)
        , version(version)
    {}
};

struct ModuleExportNode
{
    ca::str::Utf8StringRef              packaze;
    int                                 access = 0;
    std::vector<ca::str::Utf8StringRef> modules;

    ModuleExportNode() = default;
    ModuleExportNode(ca::str::Utf8StringRef packaze, int access,
                     const std::vector<ca::str::Utf8StringRef>& modules)
        : packaze(packaze)
        , access(access)
        , modules(modules)
    {}
};

struct ModuleOpenNode
{
    ca::str::Utf8StringRef              packaze;
    int                                 access = 0;
    std::vector<ca::str::Utf8StringRef> modules;

    ModuleOpenNode() = default;
    ModuleOpenNode(ca::str::Utf8StringRef packaze, int access,
                   const std::vector<ca::str::Utf8StringRef>& modules)
        : packaze(packaze)
        , access(access)
        , modules(modules)
    {}
};

struct ModuleProvideNode
{
    ca::str::Utf8StringRef              service;
    std::vector<ca::str::Utf8StringRef> providers;

    ModuleProvideNode() = default;
    ModuleProvideNode(ca::str::Utf8StringRef                     service,
                      const std::vector<ca::str::Utf8StringRef>& providers)
        : service(service)
        , providers(providers)
    {}
};

class ModuleNode : public jbytecode::core::ModuleVisitor
{
public:
    ca::str::Utf8StringRef              name;
    int                                 access = 0;
    ca::str::Utf8StringRef              version;
    ca::str::Utf8StringRef              mainClass;   ///< Module main class (set via visitMainClass)
    std::vector<ca::str::Utf8StringRef> packages;    ///< Module packages (set via visitPackage)
    std::vector<ModuleRequireNode>      requiresList;
    std::vector<ModuleExportNode>       exportsList;
    std::vector<ModuleOpenNode>         opensList;
    std::vector<ca::str::Utf8StringRef> usesList;
    std::vector<ModuleProvideNode>      providesList;

    ModuleNode(ca::str::Utf8StringRef name, int access, ca::str::Utf8StringRef version);

    void visitMainClass(ca::str::Utf8StringRef mainClass) override { this->mainClass = mainClass; }
    void visitPackage(ca::str::Utf8StringRef packaze) override { packages.push_back(packaze); }
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

    void accept(jbytecode::core::ClassVisitor* cv);
};


struct ParameterNode
{
    ca::str::Utf8StringRef name;
    int                    access = 0;

    ParameterNode() = default;
    ParameterNode(ca::str::Utf8StringRef name, int access)
        : name(name)
        , access(access)
    {}
};

class RecordComponentNode : public jbytecode::core::RecordComponentVisitor
{
public:
    ca::str::Utf8StringRef                   name;
    ca::str::Utf8StringRef                   descriptor;
    ca::str::Utf8StringRef                   signature;
    std::vector<jbytecode::core::Attribute*> attrs;
    std::vector<AnnotationNode*>             visibleAnnotations;
    std::vector<AnnotationNode*>             invisibleAnnotations;
    std::vector<AnnotationNode*>             visibleTypeAnnotations;
    std::vector<AnnotationNode*>             invisibleTypeAnnotations;

    /// @param contentArena 注解子节点的分配归宿（宿主 ClassNode 的内容 arena）。
    RecordComponentNode(garena::Arena& contentArena, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef descriptor, ca::str::Utf8StringRef signature);

    ~RecordComponentNode() override
    {
        for (auto* attr : attrs) delete attr;   // Attribute 维持堆所有权（tree 从不填充）
    }

    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    /// @brief 记录 record component 上的 JVM 类型注解。
    /// @param typeRef
    /// 类型注解目标引用。
    /// @param typePath 注解在复合类型中的路径，可为空。
    /// @param
    /// descriptor 注解描述符。
    /// @param visible 是否写入 RuntimeVisibleTypeAnnotations。
    ///
    /// @return 由当前节点持有的类型注解节点。
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    void visitEnd() override {}

    void accept(jbytecode::core::ClassVisitor* cv);

private:
    garena::Arena& contentArena_;
};



struct TryCatchBlockNode
{
    jbytecode::core::Label* start   = nullptr;
    jbytecode::core::Label* end     = nullptr;
    jbytecode::core::Label* handler = nullptr;
    ca::str::Utf8StringRef  type;

    TryCatchBlockNode() = default;
    TryCatchBlockNode(jbytecode::core::Label* start, jbytecode::core::Label* end,
                      jbytecode::core::Label* handler, ca::str::Utf8StringRef type)
        : start(start)
        , end(end)
        , handler(handler)
        , type(type)
    {}
};


/// @brief Tree model for a JVM field declaration.
class FieldNode : public jbytecode::core::FieldVisitor
{
public:
    /// 内容物 arena（注解子树）：声明居首 → 析构居末（内容先于壳死亡）。
    /// 惰性分配：无注解的字段零 chunk 开销。
    garena::Arena nodeArena_{kFieldArenaFirstChunkBytes};
    int                    access;   ///< Access flags (ACC_PUBLIC, ACC_STATIC, …)
    ca::str::Utf8StringRef name, desc,
        signature;         ///< Name, descriptor (e.g. "I"), generic signature
    ca::core::Any value;   ///< ConstantValue attribute (nullopt if absent)
    std::vector<jbytecode::core::Attribute*> attrs;
    std::vector<AnnotationNode*>             visibleAnnotations;
    std::vector<AnnotationNode*>             invisibleAnnotations;
    std::vector<AnnotationNode*>             visibleTypeAnnotations;
    std::vector<AnnotationNode*>             invisibleTypeAnnotations;

    FieldNode(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
              ca::str::Utf8StringRef signature, const ca::core::Any& value)
        : jbytecode::core::FieldVisitor(jbytecode::core::Opcodes::ASM9)
        , access(access)
        , name(name)
        , desc(desc)
        , signature(signature)
        , value(value)
    {}

    /// @brief 在本字段的内容 arena 上分配节点（注解等）。产物寿命 = 字段寿命。
    template <class T, class... Args>
    T* create(Args&&... args)
    {
        return nodeArena_.create<T>(std::forward<Args>(args)...);
    }
    garena::Arena& arena() { return nodeArena_; }

    ~FieldNode() override
    {
        // 注解子树由 nodeArena_ 统一析构；Attribute 维持堆所有权（tree 从不填充）。
        for (auto* attr : attrs) delete attr;
    }

    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    /// @brief 记录字段声明上的 JVM 类型注解。
    /// @param typeRef 类型注解目标引用。

    /// * @param typePath 注解在复合类型中的路径，可为空。
    /// @param descriptor 注解描述符。

    /// * @param visible 是否写入 RuntimeVisibleTypeAnnotations。
    /// @return
    /// 由当前节点持有的类型注解节点。
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath, ca::str::Utf8StringRef descriptor,
        bool visible) override;
    void visitEnd() override {}

    void accept(jbytecode::core::ClassVisitor* cv);
    void check(int api) const { (void)api; }
};


///  @brief Tree model for a JVM method / constructor / class initializer.
///
///  Holds all parsed information: access flags, name, descriptor,
///  bytecode instructions (InsnList), exception table, local variables,
/// annotations, and max stack/locals.
class MethodNode : public jbytecode::core::MethodVisitor
{
public:
    /// 内容物 arena（指令节点/Label(B8 起)/TryCatch/局部变量/参数/注解子树）：
    /// 声明居首 → 析构居末（内容先于壳死亡）。惰性分配，首块 4KB 随方法体规模倍增。
    /// 节点创建一律走 create<T>()：产物寿命 = 方法寿命，析构由 arena 逆序统一执行。
    garena::Arena nodeArena_{kNodeArenaFirstChunkBytes};
    int                                 access;
    ca::str::Utf8StringRef              name, desc, signature;
    std::vector<ca::str::Utf8StringRef> exceptions;
    /// 指令链表绑定本方法 arena：add/insert 的节点必须由 nodeArena_ 分配（归属一致性
    /// 目前靠调用纪律维持，DEV-11 裁决 debug 断言推迟，见 doc_ai/arena-migration-deviations.md）。
    InsnList instructions{&nodeArena_};
    /// B8：外来 Label（owner_==nullptr）收编映射：外来指针 → 本方法 arena 副本。
    /// 寿命前提：外来 Label 须存活至本方法装配结束（reader labelArena_ / 调用方堆对象）；
    /// 键为裸指针，外来 Label 提前释放且地址被新 Label 复用时，收编映射会误命中旧副本。
    std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*> foreignLabels_;
    std::vector<TryCatchBlockNode*>             tryCatchBlocks;
    std::vector<LocalVariableNode*>             localVariables;
    std::vector<ParameterNode*>                 parameters;
    std::vector<AnnotationNode*>                visibleAnnotations;
    std::vector<AnnotationNode*>                invisibleAnnotations;
    std::vector<AnnotationNode*>                visibleTypeAnnotations;
    std::vector<AnnotationNode*>                invisibleTypeAnnotations;
    std::vector<AnnotationNode*>                visibleParameterAnnotations;
    std::vector<AnnotationNode*>                invisibleParameterAnnotations;
    std::vector<int>                            visibleParameterIndices;
    std::vector<int>                            invisibleParameterIndices;
    std::vector<LocalVariableAnnotationNode*>   visibleLocalVariableAnnotations;
    std::vector<LocalVariableAnnotationNode*>   invisibleLocalVariableAnnotations;
    std::vector<jbytecode::core::Attribute*>    attrs;
    AnnotationNode*                             annotationDefault = nullptr;
    int                                         maxStack = 0, maxLocals = 0;

    MethodNode(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
               ca::str::Utf8StringRef                     signature,
               const std::vector<ca::str::Utf8StringRef>& exceptions);

    MethodNode(const MethodNode&)            = delete;
    MethodNode& operator=(const MethodNode&) = delete;
    /// move 已删除：MethodNode 持有 nodeArena_，节点指针以 &nodeArena_ 为归属锚，
    /// 对象搬迁会使 arena 归属判别与 Label 归属失效（跨方法成员搬移以指针转移完成，
    /// 对象本身不搬）。
    MethodNode(MethodNode&&)            = delete;
    MethodNode& operator=(MethodNode&&) = delete;

    /// @brief 在本方法的内容 arena 上分配节点。产物寿命 = 方法寿命。
    template <class T, class... Args>
    T* create(Args&&... args)
    {
        return nodeArena_.create<T>(std::forward<Args>(args)...);
    }
    garena::Arena& arena() { return nodeArena_; }

    ~MethodNode() override
    {
        // arena 内容物（指令/tryCatch/局部变量/参数/注解）由 nodeArena_ 析构统一回收。
        // Attribute 维持堆所有权（tree 从不填充）。Label 走 B8 归属制：方法域 label
        // 随 nodeArena_ 回收，外来借用 label 归其创建方，本析构不 delete 任何 Label。
        for (auto* attr : attrs) delete attr;
        if (annotationDefault && !nodeArena_.owns(annotationDefault)) delete annotationDefault;
    }

    void visitCode() override {}
    void visitInsn(int opcode) override { instructions.add(create<InsnNode>(opcode)); }
    void visitIntInsn(int opcode, int operand) override
    {
        instructions.add(create<IntInsnNode>(opcode, operand));
    }
    void visitVarInsn(int opcode, int var) override
    {
        instructions.add(create<VarInsnNode>(opcode, var));
    }
    void visitTypeInsn(int opcode, ca::str::Utf8StringRef type) override
    {
        instructions.add(create<TypeInsnNode>(opcode, type));
    }
    void visitFieldInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                        ca::str::Utf8StringRef desc) override
    {
        instructions.add(create<FieldInsnNode>(opcode, owner, name, desc));
    }
    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef desc, bool itf) override
    {
        instructions.add(create<MethodInsnNode>(opcode, owner, name, desc, itf));
    }
    void visitInvokeDynamicInsn(ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
                                const jbytecode::core::Handle&    bsm,
                                const std::vector<ca::core::Any>& args) override
    {
        instructions.add(create<InvokeDynamicInsnNode>(name, desc, bsm, args));
    }
    void visitJumpInsn(int opcode, jbytecode::core::Label* label) override
    {
        label = canonicalizeLabel(label);   // 外来 Label 方法边界收编（B8）
        instructions.add(create<JumpInsnNode>(opcode, label));
    }
    void visitLabel(jbytecode::core::Label* label) override
    {
        label = canonicalizeLabel(label);
        instructions.add(create<LabelNode>(label));
    }
    void visitLdcInsn(const ca::core::Any& value) override
    {
        instructions.add(create<LdcInsnNode>(value));
    }
    void visitIincInsn(int var, int incr) override
    {
        instructions.add(create<IincInsnNode>(var, incr));
    }
    void visitTableSwitchInsn(int min, int max, jbytecode::core::Label* dflt,
                              const std::vector<jbytecode::core::Label*>& labels) override
    {
        dflt = canonicalizeLabel(dflt);
        std::vector<jbytecode::core::Label*> canonical;
        canonical.reserve(labels.size());
        for (auto* l : labels) canonical.push_back(canonicalizeLabel(l));
        instructions.add(create<TableSwitchInsnNode>(min, max, dflt, canonical));
    }
    void visitLookupSwitchInsn(jbytecode::core::Label* dflt, const std::vector<int>& keys,
                               const std::vector<jbytecode::core::Label*>& labels) override
    {
        dflt = canonicalizeLabel(dflt);
        std::vector<jbytecode::core::Label*> canonical;
        canonical.reserve(labels.size());
        for (auto* l : labels) canonical.push_back(canonicalizeLabel(l));
        instructions.add(create<LookupSwitchInsnNode>(dflt, keys, canonical));
    }
    void visitMultiANewArrayInsn(ca::str::Utf8StringRef desc, int dims) override
    {
        instructions.add(create<MultiANewArrayInsnNode>(desc, dims));
    }
    void visitFrame(int type, int numLocal, const std::vector<ca::core::Any>& local, int numStack,
                    const std::vector<ca::core::Any>& stack) override
    {
        // 帧 local/stack 中 ITEM_Uninitialized 形态的 Label*（ClassReader 自
        // labelArena_ 产出，见 readVerificationTypeInfo）与跳转目标 Label 同为
        // 外来对象：必须经 canonicalizeLabel 随方法边界收编（B8，readers.hpp 的
        // 「树不持有 reader Label」契约）。漏收编时 FrameNode 持裸 Label*，reader
        // 析构释放 labelArena_ 后即悬垂——flags=0 写回（成员删除族）的
        // visitFrame→putVerificationTypeInfo→Label::put→isResolved 首先踩中
        // use-after-free（COMPUTE_FRAMES 写回因 MethodWriter::visitFrame 对
        // COMPUTE_ALL_FRAMES 早退而掩盖）。
        auto adoptFrameTypes = [this](const std::vector<ca::core::Any>& src,
                                      std::size_t                       count) {
            if (count > src.size()) count = src.size();
            std::vector<ca::core::Any> out;
            out.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                if (auto* p = src[i].as<jbytecode::core::Label*>()) {
                    out.emplace_back(canonicalizeLabel(*p));
                }
                else if (auto* cp = src[i].as<const jbytecode::core::Label*>()) {
                    out.emplace_back(canonicalizeLabel(const_cast<jbytecode::core::Label*>(*cp)));
                }
                else {
                    out.push_back(src[i]);
                }
            }
            return out;
        };
        const std::vector<ca::core::Any> frameLocal =
            adoptFrameTypes(local, numLocal > 0 ? static_cast<std::size_t>(numLocal) : 0);
        const std::vector<ca::core::Any> frameStack =
            adoptFrameTypes(stack, numStack > 0 ? static_cast<std::size_t>(numStack) : 0);
        instructions.add(create<FrameNode>(type, frameLocal, frameStack));
    }
    void visitLineNumber(int line, jbytecode::core::Label* start) override
    {
        start = canonicalizeLabel(start);
        instructions.add(create<LineNumberNode>(line, start));
    }

    void visitParameter(ca::str::Utf8StringRef name, int access) override
    {
        parameters.push_back(create<ParameterNode>(name, access));
    }
    void visitTryCatchBlock(jbytecode::core::Label* start, jbytecode::core::Label* end,
                            jbytecode::core::Label* handler, ca::str::Utf8StringRef type) override
    {
        start   = canonicalizeLabel(start);
        end     = canonicalizeLabel(end);
        handler = canonicalizeLabel(handler);
        tryCatchBlocks.push_back(create<TryCatchBlockNode>(start, end, handler, type));
    }
    void visitLocalVariable(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                            ca::str::Utf8StringRef signature, jbytecode::core::Label* start,
                            jbytecode::core::Label* end, int index) override
    {
        start = canonicalizeLabel(start);
        end   = canonicalizeLabel(end);
        localVariables.push_back(
            create<LocalVariableNode>(name, descriptor, signature, start, end, index));
    }
    /// 局部变量（含资源变量）类型注解：不覆写时基类默认转发 mv=nullptr 直接丢弃，
    /// 注解在 reader→tree→writer 往返中静默消失。与 visitTypeAnnotation 同构存储
    /// （visible/invisible 各一列），写回由 replayMethodBody 原序重发。
    jbytecode::core::AnnotationVisitor* visitLocalVariableAnnotation(
        int typeRef, const jbytecode::core::TypePath* typePath,
        const std::vector<jbytecode::core::Label*>& start,
        const std::vector<jbytecode::core::Label*>& end, const std::vector<int>& index,
        ca::str::Utf8StringRef descriptor, bool visible) override;

    jbytecode::core::AnnotationVisitor* visitAnnotationDefault() override;
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(int typeRef,
                                                            const jbytecode::core::TypePath* tp,
                                                            ca::str::Utf8StringRef           desc,
                                                            bool visible) override;
    jbytecode::core::AnnotationVisitor* visitParameterAnnotation(int                    parameter,
                                                                 ca::str::Utf8StringRef descriptor,
                                                                 bool visible) override;
    void                                visitMaxs(int maxStack, int maxLocals) override
    {
        this->maxStack  = maxStack;
        this->maxLocals = maxLocals;
    }
    void visitEnd() override {}

    void accept(jbytecode::core::ClassVisitor* cv);
    void accept(jbytecode::core::MethodVisitor* mv);
    void check(int api) const { (void)api; }

    // ---- Label 归属（B8：arena 盖章即所有权，见 DESIGN.md §6.1）----

    /// 在本方法内容 arena 上新建 Label 并盖章 owner=this（B8 起的标准建点）。
    jbytecode::core::Label* createLabel();

    /// Label 收编：owner==this 直返（O(1) 判别）；外来 Label 查 foreignLabels_，
    /// 未命中则拷贝进本方法 arena（{bytecodeOffset,line,flags}）并登记。
    /// 9 个携带 Label 的 visitor（含 visitFrame 的帧槽与 visitLocalVariableAnnotation
    /// 的起止范围表）在方法边界统一调用。
    jbytecode::core::Label* canonicalizeLabel(jbytecode::core::Label* label);

    /// clone 协议伴生（B8）：语义同 canonicalizeLabel，但映射表由调用方携带，
    /// 供跨方法/scratch 克隆的 Label 映射共用（clone(arena,map) 各实现调用）。
    jbytecode::core::Label*
    resolveCloneLabel(std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>& map,
                      jbytecode::core::Label* label);


};


struct InnerClassNode
{
    ca::str::Utf8StringRef name, outerName, innerName;
    int                    access = 0;

    InnerClassNode() = default;
    InnerClassNode(ca::str::Utf8StringRef name, ca::str::Utf8StringRef outerName,
                   ca::str::Utf8StringRef innerName, int access)
        : name(name)
        , outerName(outerName)
        , innerName(innerName)
        , access(access)
    {}
};

///  @brief Complete tree model for a JVM class file.
///
///  Built by passing this node as jbytecode::core::ClassVisitor to ClassReader::accept().
///  After accept() returns, all fields/methods/annotations are populated.
/// Use accept(ClassWriter*) to serialize back into bytes.
class ClassNode : public jbytecode::core::ClassVisitor
{
public:
    /// 本类直接内容物 arena（类级注解/innerClasses/recordComponents/moduleNode）：
    /// 声明居首 → 析构居末。fields/methods 本体仍是堆对象（可随 tool_class_combiner
    /// 整成员搬移，各自携带内容 arena），由本类析构 delete。
    garena::Arena nodeArena_{kNodeArenaFirstChunkBytes};
    int version = 0, access = 0;   ///< class version (major|minor<<16), access flags
    ca::str::Utf8StringRef name, signature,
        superName;   ///< internal names, e.g. "java/lang/Object"
    std::vector<ca::str::Utf8StringRef> interfaces;
    ca::str::Utf8StringRef              sourceFile, sourceDebug;
    ca::str::Utf8StringRef              outerClass, outerMethod, outerMethodDesc;
    std::vector<AnnotationNode*>        visibleAnnotations;
    std::vector<AnnotationNode*>        invisibleAnnotations;
    std::vector<AnnotationNode*>        visibleTypeAnnotations;
    std::vector<AnnotationNode*>        invisibleTypeAnnotations;
    std::vector<InnerClassNode*>        innerClasses;
    std::vector<FieldNode*>             fields;    ///< Declared fields
    std::vector<MethodNode*>            methods;   ///< Declared methods, constructors, initializers
    std::vector<RecordComponentNode*>   recordComponents;
    std::vector<jbytecode::core::Attribute*> attrs;
    ModuleNode*                              moduleNode = nullptr;
    ca::str::Utf8StringRef                   nestHost;
    std::vector<ca::str::Utf8StringRef>      nestMembers;
    std::vector<ca::str::Utf8StringRef>      permittedSubclasses;

    ClassNode()
        : jbytecode::core::ClassVisitor(jbytecode::core::Opcodes::ASM9)
    {}

    ClassNode(const ClassNode&)            = delete;
    ClassNode& operator=(const ClassNode&) = delete;
    ClassNode& operator=(ClassNode&&)      = delete;
    /// move 已删除：nodeArena_ 归属锚以对象地址为准，ClassNode 一律以
    /// unique_ptr/指针转移，对象本身不搬。
    ClassNode(ClassNode&&) = delete;

    /// @brief 在本类的内容 arena 上分配节点（类级注解/innerClasses/recordComponents/
    /// moduleNode）。产物寿命 = 类寿命。
    template <class T, class... Args>
    T* create(Args&&... args)
    {
        return nodeArena_.create<T>(std::forward<Args>(args)...);
    }
    garena::Arena& arena() { return nodeArena_; }

    ~ClassNode() override
    {
        for (auto* f : fields) delete f;
        for (auto* m : methods) delete m;
        // 类级内容物由 nodeArena_ 统一析构；Attribute 维持堆所有权（tree 从不填充）。
        for (auto* attr : attrs) delete attr;
    }

    void visit(int version, int access, ca::str::Utf8StringRef name,
               ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override;
    void visitSource(ca::str::Utf8StringRef source, ca::str::Utf8StringRef debug) override;
    void visitOuterClass(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef desc) override;
    jbytecode::core::ModuleVisitor* visitModule(ca::str::Utf8StringRef name, int access,
                                                ca::str::Utf8StringRef version) override;
    void                            visitNestHost(ca::str::Utf8StringRef nestHost) override;
    void                            visitNestMember(ca::str::Utf8StringRef nestMember) override;
    void visitPermittedSubclass(ca::str::Utf8StringRef subclass) override;
    jbytecode::core::AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef descriptor,
                                                        bool                   visible) override;
    jbytecode::core::AnnotationVisitor* visitTypeAnnotation(int typeRef,
                                                            const jbytecode::core::TypePath* tp,
                                                            ca::str::Utf8StringRef           desc,
                                                            bool visible) override;
    void visitInnerClass(ca::str::Utf8StringRef name, ca::str::Utf8StringRef outerName,
                         ca::str::Utf8StringRef innerName, int access) override;
    jbytecode::core::RecordComponentVisitor* visitRecordComponent(
        ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
        ca::str::Utf8StringRef signature) override;
    jbytecode::core::FieldVisitor*  visitField(int access, ca::str::Utf8StringRef name,
                                               ca::str::Utf8StringRef desc,
                                               ca::str::Utf8StringRef signature,
                                               const ca::core::Any&   value) override;
    jbytecode::core::MethodVisitor* visitMethod(
        int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc,
        ca::str::Utf8StringRef                     signature,
        const std::vector<ca::str::Utf8StringRef>& exceptions) override;
    void visitEnd() override {}

    void accept(jbytecode::core::ClassVisitor* cv);
    void check(int api) const { (void)api; };
};

/// @brief debug 抽样断言：ClassNode 关键字符串视图确属给定 arena（reader 借用 arena 制护栏）。
/// @details 治理方案 §2.4-2「跨结构 ref 提升门禁」的接线点：reader.accept 完成后，
///          在 arena 与 ClassNode 同处作用域的装配侧调用；抽样 name/superName/signature/
///          接口名/首个成员的 name+desc（非空才查，空视图 owns() 恒 false 属合法缺省）。
///          失配即 fprintf + abort（诊断收敛在本 .cpp，遵循 coding-rules §7）。
///          release 构建为空实现，零开销。
/// @param node 待检查的类节点（accept 已完成）。
/// @param arena reader 构造时传入的 Utf8StringArena。
void debugAssertOwnedBy(const ClassNode& node, const ca::str::Utf8StringArena& arena);



}   // namespace jbytecode::tree
