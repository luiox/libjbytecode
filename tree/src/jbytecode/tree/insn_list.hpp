#pragma once

#include <cassert>
#include "libca/core/datatype.hpp"
#include "libca/str/utf8_string.hpp"
#include "garena/arena.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/visitors.hpp"
#include <vector>
#include <unordered_map>
#include <memory>
#include <utility>

namespace jbytecode::tree {
class TypeAnnotationNode;

///  @brief Base class for all bytecode instruction tree nodes.
///
///  Each instruction type has a concrete subclass (InsnNode, VarInsnNode, …).
///  Nodes form a doubly-linked list via previousInsn / nextInsn.
/// Use getType() to dispatch and static_cast<>() to access subclass fields.
class AbstractInsnNode
{
public:
    static constexpr int INSN                = 0;
    static constexpr int INT_INSN            = 1;
    static constexpr int VAR_INSN            = 2;
    static constexpr int TYPE_INSN           = 3;
    static constexpr int FIELD_INSN          = 4;
    static constexpr int METHOD_INSN         = 5;
    static constexpr int INVOKE_DYNAMIC_INSN = 6;
    static constexpr int JUMP_INSN           = 7;
    static constexpr int LABEL               = 8;
    static constexpr int LDC_INSN            = 9;
    static constexpr int IINC_INSN           = 10;
    static constexpr int TABLESWITCH_INSN    = 11;
    static constexpr int LOOKUPSWITCH_INSN   = 12;
    static constexpr int MULTIANEWARRAY_INSN = 13;
    static constexpr int FRAME               = 14;
    static constexpr int LINE                = 15;

    int                              opcode;
    AbstractInsnNode*                previousInsn = nullptr;
    AbstractInsnNode*                nextInsn     = nullptr;
    int                              index        = -1;
    std::vector<TypeAnnotationNode*> visibleTypeAnnotations;
    std::vector<TypeAnnotationNode*> invisibleTypeAnnotations;

    explicit AbstractInsnNode(int opcode)
        : opcode(opcode)
    {}
    virtual ~AbstractInsnNode() = default;

    int               getOpcode() const { return opcode; }
    virtual int       getType() const = 0;
    AbstractInsnNode* getPrevious() const { return previousInsn; }
    AbstractInsnNode* getNext() const { return nextInsn; }

    virtual void accept(jbytecode::core::MethodVisitor* mv) const = 0;
    /// B8 新 clone 协议：节点在目标 arena 上分配（arena.create<XNode>(...)），携带的
    /// Label 经自由函数 resolveCloneLabel(arena, clonedLabels, label) 解析（同 arena
    /// O(1) 直返；未命中则在 arena 建副本并登记）。唯一 clone 通道（B8b 已物理退役
    /// 旧堆 clone(map) 虚函数族）。
    virtual AbstractInsnNode*
    clone(garena::Arena&                                                      arena,
          std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>& clonedLabels)
        const = 0;

    void acceptAnnotations(jbytecode::core::MethodVisitor* mv) const;

protected:
    AbstractInsnNode(const AbstractInsnNode&) = default;
};

/// B8 clone 协议伴生：解析克隆目标 Label。nullptr 直返；label 已归属目标 arena
/// （owner() == &arena）则 O(1) 直返（同域不换身份）；map 命中直返；否则在 arena 上
/// 新建 Label（setOwner(&arena)）并拷贝 {bytecodeOffset, line, flags}，
/// 登记 map 后返回（前向跳转与其目标 LabelNode 共享同一克隆）。
jbytecode::core::Label*
resolveCloneLabel(garena::Arena&                                                        arena,
                  std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>& map,
                  jbytecode::core::Label*                                               label);

// ============================================================
// 具体指令节点
// ============================================================

class InsnNode : public AbstractInsnNode
{
public:
    explicit InsnNode(int opcode)
        : AbstractInsnNode(opcode)
    {}
    int               getType() const override { return INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode* clone(garena::Arena& arena,
                            std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>&)
        const override;
};

class IntInsnNode : public AbstractInsnNode
{
public:
    int operand;
    IntInsnNode(int opcode, int operand)
        : AbstractInsnNode(opcode)
        , operand(operand)
    {}
    int               getType() const override { return INT_INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode* clone(garena::Arena& arena,
                            std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>&)
        const override;
};

class VarInsnNode : public AbstractInsnNode
{
public:
    int var;
    VarInsnNode(int opcode, int var)
        : AbstractInsnNode(opcode)
        , var(var)
    {}
    int               getType() const override { return VAR_INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode* clone(garena::Arena& arena,
                            std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>&)
        const override;
};

class TypeInsnNode : public AbstractInsnNode
{
public:
    ca::str::Utf8StringRef desc;
    TypeInsnNode(int opcode, const ca::str::Utf8StringRef& desc)
        : AbstractInsnNode(opcode)
        , desc(desc)
    {}
    int               getType() const override { return TYPE_INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode* clone(garena::Arena& arena,
                            std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>&)
        const override;
};

class FieldInsnNode : public AbstractInsnNode
{
public:
    ca::str::Utf8StringRef owner, name, desc;
    FieldInsnNode(int opcode, const ca::str::Utf8StringRef& owner,
                  const ca::str::Utf8StringRef& name, const ca::str::Utf8StringRef& desc)
        : AbstractInsnNode(opcode)
        , owner(owner)
        , name(name)
        , desc(desc)
    {}
    int               getType() const override { return FIELD_INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode* clone(garena::Arena& arena,
                            std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>&)
        const override;
};

class MethodInsnNode : public AbstractInsnNode
{
public:
    ca::str::Utf8StringRef owner, name, desc;
    bool                   isInterface;
    MethodInsnNode(int opcode, const ca::str::Utf8StringRef& owner,
                   const ca::str::Utf8StringRef& name, const ca::str::Utf8StringRef& desc,
                   bool isInterface)
        : AbstractInsnNode(opcode)
        , owner(owner)
        , name(name)
        , desc(desc)
        , isInterface(isInterface)
    {}
    int               getType() const override { return METHOD_INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode* clone(garena::Arena& arena,
                            std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>&)
        const override;
};

class InvokeDynamicInsnNode : public AbstractInsnNode
{
public:
    ca::str::Utf8StringRef     name, desc;
    jbytecode::core::Handle    bsm;
    std::vector<ca::core::Any> bsmArgs;
    InvokeDynamicInsnNode(const ca::str::Utf8StringRef& name, const ca::str::Utf8StringRef& desc,
                          const jbytecode::core::Handle&    bsm,
                          const std::vector<ca::core::Any>& bsmArgs)
        : AbstractInsnNode(jbytecode::core::Opcodes::INVOKEDYNAMIC)
        , name(name)
        , desc(desc)
        , bsm(bsm)
        , bsmArgs(bsmArgs)
    {}
    int               getType() const override { return INVOKE_DYNAMIC_INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode* clone(garena::Arena& arena,
                            std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>&)
        const override;
};

class JumpInsnNode : public AbstractInsnNode
{
public:
    jbytecode::core::Label* label;
    JumpInsnNode(int opcode, jbytecode::core::Label* label)
        : AbstractInsnNode(opcode)
        , label(label)
    {}
    int               getType() const override { return JUMP_INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode*
    clone(garena::Arena& arena,
          std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>& clonedLabels)
        const override;
};

class LabelNode : public AbstractInsnNode
{
public:
    jbytecode::core::Label* label;
    explicit LabelNode(jbytecode::core::Label* label);
    ~LabelNode() override;
    int                     getType() const override { return LABEL; }
    jbytecode::core::Label* getLabel() const { return label; }
    void                    accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode*
    clone(garena::Arena& arena,
          std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>& clonedLabels)
        const override;
};

class LdcInsnNode : public AbstractInsnNode
{
public:
    ca::core::Any cst;
    explicit LdcInsnNode(const ca::core::Any& cst)
        : AbstractInsnNode(jbytecode::core::Opcodes::LDC)
        , cst(cst)
    {}
    int               getType() const override { return LDC_INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode* clone(garena::Arena& arena,
                            std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>&)
        const override;
};

class IincInsnNode : public AbstractInsnNode
{
public:
    int var, incr;
    IincInsnNode(int var, int incr)
        : AbstractInsnNode(jbytecode::core::Opcodes::IINC)
        , var(var)
        , incr(incr)
    {}
    int               getType() const override { return IINC_INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode* clone(garena::Arena& arena,
                            std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>&)
        const override;
};

class TableSwitchInsnNode : public AbstractInsnNode
{
public:
    int                                  min, max;
    jbytecode::core::Label*              dflt;
    std::vector<jbytecode::core::Label*> labels;
    TableSwitchInsnNode(int min, int max, jbytecode::core::Label* dflt,
                        const std::vector<jbytecode::core::Label*>& labels)
        : AbstractInsnNode(jbytecode::core::Opcodes::TABLESWITCH)
        , min(min)
        , max(max)
        , dflt(dflt)
        , labels(labels)
    {}
    int               getType() const override { return TABLESWITCH_INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode*
    clone(garena::Arena& arena,
          std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>& clonedLabels)
        const override;
};

class LookupSwitchInsnNode : public AbstractInsnNode
{
public:
    jbytecode::core::Label*              dflt;
    std::vector<int>                     keys;
    std::vector<jbytecode::core::Label*> labels;
    LookupSwitchInsnNode(jbytecode::core::Label* dflt, const std::vector<int>& keys,
                         const std::vector<jbytecode::core::Label*>& labels)
        : AbstractInsnNode(jbytecode::core::Opcodes::LOOKUPSWITCH)
        , dflt(dflt)
        , keys(keys)
        , labels(labels)
    {}
    int               getType() const override { return LOOKUPSWITCH_INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode*
    clone(garena::Arena& arena,
          std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>& clonedLabels)
        const override;
};

class MultiANewArrayInsnNode : public AbstractInsnNode
{
public:
    ca::str::Utf8StringRef desc;
    int                    dims;
    MultiANewArrayInsnNode(const ca::str::Utf8StringRef& desc, int dims)
        : AbstractInsnNode(jbytecode::core::Opcodes::MULTIANEWARRAY)
        , desc(desc)
        , dims(dims)
    {}
    int               getType() const override { return MULTIANEWARRAY_INSN; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode* clone(garena::Arena& arena,
                            std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>&)
        const override;
};

class FrameNode : public AbstractInsnNode
{
public:
    int                        frameType;
    std::vector<ca::core::Any> local;
    std::vector<ca::core::Any> stack;
    FrameNode(int frameType, const std::vector<ca::core::Any>& local,
              const std::vector<ca::core::Any>& stack)
        : AbstractInsnNode(-1)
        , frameType(frameType)
        , local(local)
        , stack(stack)
    {}
    int               getType() const override { return FRAME; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode* clone(garena::Arena& arena,
                            std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>&)
        const override;
};

class LineNumberNode : public AbstractInsnNode
{
public:
    int                     line;
    jbytecode::core::Label* start;
    LineNumberNode(int line, jbytecode::core::Label* start)
        : AbstractInsnNode(-1)
        , line(line)
        , start(start)
    {}
    int               getType() const override { return LINE; }
    void              accept(jbytecode::core::MethodVisitor* mv) const override;
    AbstractInsnNode*
    clone(garena::Arena& arena,
          std::unordered_map<jbytecode::core::Label*, jbytecode::core::Label*>& clonedLabels)
        const override;
};

// ============================================================
// InsnList — 双向链表容器
// ============================================================

/// @brief Doubly-linked list of AbstractInsnNode, owned by MethodNode::instructions.
///
/// 生命周期模型（P2 收编）：绑定 owner arena 的列表（MethodNode::instructions）中，
/// 节点由该 garena::Arena 分配并随方法域整体析构——set/clear/removeAndDelete 对
/// arena 节点只摘链不销毁；未绑定 arena 的临时列表（scratch/holding）以及尚未迁移的
/// 堆分配节点维持旧的「容器负责 delete」语义（混合清除，见 disposeNode）。
class InsnList
{
public:
    InsnList() = default;
    /// @brief 绑定 owner arena：挂入本表的节点必须由该 arena 分配。
    ///        归属一致性目前靠调用纪律维持（DEV-11 裁决：debug 断言推迟，见
    ///        doc_ai/arena-migration-deviations.md）；跨域违规挂入将在 disposeNode
    ///        处对 bump 内存误走 delete（堆损坏），新增建点请走 owner.create<T>()。
    explicit InsnList(garena::Arena* ownerArena) noexcept
        : ownerArena_(ownerArena)
    {}
    ~InsnList() { clear(); }

    InsnList(const InsnList&)            = delete;
    InsnList& operator=(const InsnList&) = delete;

    InsnList(InsnList&& other) noexcept;
    InsnList& operator=(InsnList&& other) noexcept;

    // -- 访问 --
    AbstractInsnNode* getFirst() const { return first_; }
    AbstractInsnNode* getLast() const { return last_; }
    int               size() const { return size_; }
    bool              isEmpty() const { return size_ == 0; }

    AbstractInsnNode* get(int index) const;
    int               indexOf(AbstractInsnNode* node) const;
    bool              contains(AbstractInsnNode* node) const;

    // -- 增 --
    void add(AbstractInsnNode* node);
    void add(InsnList& other);
    void insert(AbstractInsnNode* node);
    void insert(AbstractInsnNode* before, AbstractInsnNode* node);
    void insertBefore(AbstractInsnNode* where, AbstractInsnNode* node);
    void insert(InsnList& other);
    void insert(AbstractInsnNode* before, InsnList& other);

    // -- 改/删 --
    /// @brief 用 newNode 原位替换 oldNode。arena 节点只摘链（arena reset 统一析构），
    /// 堆节点维持立即析构。debug 下若有活跃迭代器则断言失败（见 safety.md 迭代器失效表）。
    void set(AbstractInsnNode* oldNode, AbstractInsnNode* newNode);
    /// 摘链不删：arena 节点由 arena 统一回收；堆节点释放责任转移给调用方。
    void remove(AbstractInsnNode* node);
    /// @brief 摘链并销毁堆节点（arena 节点只摘链）：「摘除即弃」的唯一合法路径。
    /// debug 下有活跃迭代器时断言失败。
    void removeAndDelete(AbstractInsnNode* node);
    /// debug 下有活跃迭代器时断言失败（clear 会析构堆节点，迭代中调用必 UAF）。
    void clear();

    /// Serialize all instructions in this list to the given method visitor.
    void                           accept(jbytecode::core::MethodVisitor* mv) const;
    std::vector<AbstractInsnNode*> toArray() const;

    /// Reset label bytecode offsets to -1 (useful when reusing labels across different
    /// ClassWriters).
    void resetLabels();

    /// owner arena（未绑定为 nullptr）：混合清除场景下供外部判定节点归属（B7）。
    garena::Arena* ownerArena() const { return ownerArena_; }

    /// @brief 在 owner arena 上构造指令节点（B9 建点规范：指令节点一律 arena 分配，
    /// 产物寿命 = 归属方法寿命）。本表须已绑定归属方法域；未绑定的 scratch 列表
    /// 无分配来源，debug 断言、release 未定义。
    template <typename T, typename... Args>
    T* create(Args&&... args)
    {
        assert(ownerArena_ != nullptr &&
               "InsnList::create: list must be bound to an owner arena");
        return ownerArena_->create<T>(std::forward<Args>(args)...);
    }


    // -- 迭代器 --
private:
    /// 活跃迭代器计数（begin/end 登记）：set/removeAndDelete/clear 的「迭代中改表」
    /// debug 断言依据（safety.md 迭代器失效表）。读写仅 debug 生效，release 只占位。
    /// #1065：本字段声明严禁被 NDEBUG 门控。InsnList 是跨 TU 共享布局的类型
    /// （内嵌 MethodNode、由 libjbytecode 本体方法就地改写、消费方经内联访问器
    /// 读写）：门控声明使 NDEBUG TU 与非 NDEBUG TU 的 size_ 等成员偏移漂移 8 字节
    /// （ODR 违规），混链后消费方 size() 读到 ownerArena_ 低半（垃圾堆地址）、
    /// 析构走错位字段——mj2x_unittest LlvmIrBackendTest 堆损坏实证（测试 target
    /// 去 NDEBUG #1063 后显形）。字段恒在（两种构建 sizeof 一致），release 无读写。
    /// 声明须先于 Iterator：MSVC 对嵌套类内联体引用后置类成员报 C2039。
    mutable int activeIterators_ = 0;
public:
    /// 活跃迭代器布局指纹（定义在 insn_list.cpp = libjbytecode 本体 TU）：
    /// 返回 lib 侧 sizeof(InsnList)。消费 TU（尤其去 NDEBUG 的测试 target）
    /// 与本地 sizeof(InsnList) 对比即可检出「NDEBUG 门控字段」等跨 TU 布局
    /// 漂移，不再表现为神秘堆损坏（#1065 回归守卫）。
    static unsigned long long layoutProbeSizeof();
    /// 原始指针遍历器。debug 构建下经 begin()/end() 取得的迭代器会向所属 InsnList
    /// 登记活跃计数，使 set/removeAndDelete/clear 的「迭代中改表」断言可执行；
    /// 直接构造的迭代器不登记（仓内无此用法）。release 下零额外开销。
    /// 注：Iterator 仅在单一 TU 内创建与消费（begin/end 内联、循环体同 TU），
    /// 不跨 TU 传递，其 debug 专属成员 owner_ 不构成跨 TU 布局风险。
    class Iterator
    {
    public:
        explicit Iterator(AbstractInsnNode* start)
            : current_(start)
#ifndef NDEBUG
            , owner_(nullptr)
#endif
        {}
#ifndef NDEBUG
        Iterator(const Iterator& o)
            : current_(o.current_)
            , owner_(o.owner_)
        {
            if (owner_) ++owner_->activeIterators_;
        }
        Iterator& operator=(const Iterator& o)
        {
            if (this != &o) {
                if (owner_) --owner_->activeIterators_;
                current_ = o.current_;
                owner_   = o.owner_;
                if (owner_) ++owner_->activeIterators_;
            }
            return *this;
        }
        ~Iterator()
        {
            if (owner_) --owner_->activeIterators_;
        }
#endif
        bool              operator!=(const Iterator& o) const { return current_ != o.current_; }
        AbstractInsnNode* operator*() const { return current_; }
        Iterator&         operator++()
        {
            current_ = current_->nextInsn;
            return *this;
        }

    private:
        friend class InsnList;
        Iterator(const InsnList* owner, AbstractInsnNode* start)
            : current_(start)
#ifndef NDEBUG
            , owner_(owner)
#endif
        {
#ifndef NDEBUG
            if (owner_) const_cast<InsnList*>(owner_)->activeIterators_++;
#endif
        }

        AbstractInsnNode* current_;
#ifndef NDEBUG
        const InsnList* owner_ = nullptr;   ///< 登记所属表（仅 debug，不计入 release 布局）
#endif
    };

    Iterator begin() const { return Iterator(this, first_); }
    Iterator end() const { return Iterator(this, nullptr); }

private:
    AbstractInsnNode* first_ = nullptr;
    AbstractInsnNode* last_  = nullptr;
    int               size_  = 0;
    /// owner arena（MethodNode::instructions 绑定；scratch 列表为 null）。
    garena::Arena* ownerArena_ = nullptr;
#ifndef NDEBUG
    void checkNoActiveIterator() const
    {
        assert(activeIterators_ == 0 &&
               "InsnList: mutating API called while iterators are alive (see libs/libjbytecode/doc/safety.md)");
    }
#endif
    /// 节点是否由 owner arena 分配（迁移期判别：arena 节点交给 arena 析构，
    /// 未迁移的堆节点仍由容器 delete）。
    bool ownsNode(const AbstractInsnNode* node) const noexcept
    {
        return ownerArena_ != nullptr && ownerArena_->owns(node);
    }
    /// 混合清除：arena 节点跳过（arena reset 统一析构），堆节点照旧 delete。
    void disposeNode(AbstractInsnNode* node)
    {
        if (!ownsNode(node)) delete node;
    }
    // 缓存：用于 get(index) 的最近访问
    mutable AbstractInsnNode* cacheNode_  = nullptr;
    mutable int               cacheIndex_ = -1;

    void removeFromList(AbstractInsnNode* node);
};

}   // namespace jbytecode::tree
