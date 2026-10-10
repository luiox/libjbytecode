/// @file        symbol_table.hpp
/// @brief       常量池 / bootstrap 方法 / 类型表的注册与去重中心。对标 ASM SymbolTable.java。
///
/// ClassWriter 写出时所有「往常量池里塞一个条目」的请求都经过 SymbolTable：它负责
/// 分配索引、按内容去重（同一 UTF8/Class/Methodref 只存一份）、并把条目的二进制形式
/// 缓存进内部 ByteVector，最终在 putConstantPool() 一次性写出。
///
/// 三张表由本类统一管理：
///   - 常量池（CONSTANT_* 条目）
///   - bootstrap 方法表（invokedynamic / dynamic 常量引用）
///   - 类型表（栈帧计算用的 TYPE_/UNINITIALIZED_/MERGED_TYPE 条目，非 class 文件常量池）
///
/// 字符串借用同一个 Utf8StringArena（构造时传入、由上层拥有），多个 Writer 可共享。
/// 实现位于 symbol_table.cpp。
#pragma once

#include "libca/core/datatype.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"
#include "jbytecode/core/byte_vector.hpp"
#include "jbytecode/core/primitives.hpp"
#include <string>
#include <vector>

namespace jbytecode::core {

class ICommonSuperClassProvider;
class Label;

/// @brief 常量池 / bootstrap / 类型表里的一个条目。对标 ASM Symbol.java。
///
/// 字段含义随 tag 不同而复用（owner/name/value/data/info 是「多用途槽位」，与 ASM 一致）。
/// 拷贝被禁用——Symbol 由 SymbolTable 拥有并以指针对外暴露。
class Symbol
{
public:
    /// @name 常量池标签（JVMS §4.4 tag 值）+ ASM 内部类型表标签（≥64）
    /// @{
    static constexpr int CONSTANT_CLASS_TAG               = 7;
    static constexpr int CONSTANT_FIELDREF_TAG            = 9;
    static constexpr int CONSTANT_METHODREF_TAG           = 10;
    static constexpr int CONSTANT_INTERFACE_METHODREF_TAG = 11;
    static constexpr int CONSTANT_STRING_TAG              = 8;
    static constexpr int CONSTANT_INTEGER_TAG             = 3;
    static constexpr int CONSTANT_FLOAT_TAG               = 4;
    static constexpr int CONSTANT_LONG_TAG                = 5;
    static constexpr int CONSTANT_DOUBLE_TAG              = 6;
    static constexpr int CONSTANT_NAME_AND_TYPE_TAG       = 12;
    static constexpr int CONSTANT_UTF8_TAG                = 1;
    static constexpr int CONSTANT_METHOD_HANDLE_TAG       = 15;
    static constexpr int CONSTANT_METHOD_TYPE_TAG         = 16;
    static constexpr int CONSTANT_DYNAMIC_TAG             = 17;
    static constexpr int CONSTANT_INVOKE_DYNAMIC_TAG      = 18;
    static constexpr int CONSTANT_MODULE_TAG              = 19;
    static constexpr int CONSTANT_PACKAGE_TAG             = 20;
    static constexpr int BOOTSTRAP_METHOD_TAG             = 64;   ///< bootstrap 方法表条目
    static constexpr int TYPE_TAG               = 128;   ///< 类型表：普通引用类型
    static constexpr int UNINITIALIZED_TYPE_TAG = 129;   ///< 类型表：NEW 后未初始化
    static constexpr int FORWARD_UNINITIALIZED_TYPE_TAG =
        130;   ///< 类型表：前向未初始化（标签未解析）
    static constexpr int MERGED_TYPE_TAG = 131;   ///< 类型表：两类型合并的公共父类
    /// @}

    int index;   ///< 在所属表中的索引（常量池索引 / bootstrap 索引 / 类型表索引）。
    int                    tag;     ///< CONSTANT_*_TAG 之一，决定其余字段含义。
    ca::str::Utf8StringRef owner;   ///< 多用途：成员引用的属主类等。
    ca::str::Utf8StringRef name;    ///< 多用途：名称槽位。
    ca::str::Utf8StringRef value;   ///< 多用途：UTF8 值 / 描述符等。
    int64_t                data;    ///< 多用途：数值常量位 / 编码后的辅助数据。
    int info;   ///< 多用途：标志位（如 method handle 的 isInterface）。

    Symbol(int index, int tag, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
           ca::str::Utf8StringRef value, int64_t data);
    virtual ~Symbol()                = default;
    Symbol(const Symbol&)            = delete;
    Symbol& operator=(const Symbol&) = delete;
};

/// @brief 哈希桶里的 Symbol 节点（内部类型）。在 Symbol 基础上加哈希码与链表指针，用于去重查找。
struct Entry : Symbol
{
    int    hashCode;         ///< 内容哈希，用于去重定位。
    Entry* next = nullptr;   ///< 同桶链表的下一个节点。
    Entry(int index, int tag, ca::str::Utf8StringRef value, int hashCode);
    Entry(int index, int tag, int data, int hashCode);
    Entry(int index, int tag, int64_t data, int hashCode);
    Entry(int index, int tag, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
          ca::str::Utf8StringRef value, int64_t data, int hashCode);
    Entry(int index, int tag, ca::str::Utf8StringRef name, ca::str::Utf8StringRef value,
          int hashCode);
    Entry(int index, int tag, ca::str::Utf8StringRef value, int64_t data, int hashCode);
};

/// @brief 类型表里前向未初始化类型与其 Label 的关联条目（内部类型）。
struct LabelEntry
{
    int         index;            ///< 类型表索引。
    Label*      label;            ///< 关联的字节码偏移锚点。
    int         hashCode;         ///< 内容哈希。
    LabelEntry* next = nullptr;   ///< 同桶链表的下一个节点。
    LabelEntry(int index, Label* label, int hashCode);
};


// ---- SymbolTable ----

/// @brief 常量池 / bootstrap / 类型表的注册去重中心。对标 ASM SymbolTable.java。
///
/// 典型用法由 ClassWriter 内部驱动：写出过程中反复调用 addConstantXxx() 注册条目并拿到
/// 索引，最后 putConstantPool() / putBootstrapMethods() 把累积的二进制写进输出。
/// 所有 addConstantXxx() 都做内容去重，重复内容返回同一 Symbol/索引。
class SymbolTable
{
public:
    /// @param arena 字符串区（借用，由调用方拥有，可多 Writer 共享）。
    /// @warning arena 必须活得比本 SymbolTable 以及它返回的 Symbol/Utf8StringRef 更久。
    SymbolTable(ca::str::Utf8StringArena& arena);
    ~SymbolTable();

    /// 本类以裸指针数组 + 链表持有 Entry/LabelEntry（析构统一 delete），
    /// 拷贝会 double-free、移动会让源对象悬空，一律禁用。
    SymbolTable(const SymbolTable&)            = delete;
    SymbolTable& operator=(const SymbolTable&) = delete;
    SymbolTable(SymbolTable&&)                 = delete;
    SymbolTable& operator=(SymbolTable&&)      = delete;

    /// @return 当前常量池条目数（含占位的第 0 项，故初值为 1）。
    int getConstantPoolCount() const { return constantPoolCount_; }
    /// @brief 把累积的常量池二进制写入 output。
    void putConstantPool(ByteVector& output);
    /// @return BootstrapMethods 属性的字节大小（用于预留长度字段）。
    int computeBootstrapMethodsSize();
    /// @brief 把 BootstrapMethods 属性写入 output。
    void putBootstrapMethods(ByteVector& output);

    /// @name 常量池注册（去重后返回 Symbol*，UTF8/NameAndType 直接返回索引）
    /// @{
    Symbol* addConstantClass(const ca::str::Utf8StringRef& value);
    Symbol* addConstantFieldref(const ca::str::Utf8StringRef& owner,
                                const ca::str::Utf8StringRef& name,
                                const ca::str::Utf8StringRef& descriptor);
    Symbol* addConstantMethodref(const ca::str::Utf8StringRef& owner,
                                 const ca::str::Utf8StringRef& name,
                                 const ca::str::Utf8StringRef& descriptor, bool isInterface);
    Symbol* addConstantString(const ca::str::Utf8StringRef& value);
    Symbol* addConstantInteger(int value);
    Symbol* addConstantFloat(float value);
    Symbol* addConstantLong(int64_t value);
    Symbol* addConstantDouble(double value);
    int     addConstantNameAndType(const ca::str::Utf8StringRef& name,
                                   const ca::str::Utf8StringRef& descriptor);
    int     addConstantUtf8(const ca::str::Utf8StringRef& value);
    Symbol* addConstantMethodHandle(int referenceKind, const ca::str::Utf8StringRef& owner,
                                    const ca::str::Utf8StringRef& name,
                                    const ca::str::Utf8StringRef& descriptor, bool isInterface);
    Symbol* addConstantMethodType(const ca::str::Utf8StringRef& methodDescriptor);
    Symbol* addConstantModule(const ca::str::Utf8StringRef& moduleName);
    Symbol* addConstantPackage(const ca::str::Utf8StringRef& packageName);
    Symbol* addConstantDynamic(const ca::str::Utf8StringRef&     name,
                               const ca::str::Utf8StringRef&     descriptor,
                               const Handle&                     bootstrapMethodHandle,
                               const std::vector<ca::core::Any>& bootstrapMethodArguments);
    Symbol* addConstantInvokeDynamic(const ca::str::Utf8StringRef&     name,
                                     const ca::str::Utf8StringRef&     descriptor,
                                     const Handle&                     bootstrapMethodHandle,
                                     const std::vector<ca::core::Any>& bootstrapMethodArguments);
    /// @}

    /// @brief 注册一个 bootstrap 方法（去重），返回携带其索引的 Symbol。
    Symbol* addBootstrapMethod(const Handle&                     bootstrapMethodHandle,
                               const std::vector<ca::core::Any>& bootstrapMethodArguments);

    /// @name 类型表（栈帧计算用，非 class 文件常量池）
    /// @{
    int addType(const ca::str::Utf8StringRef& value);
    int addUninitializedType(const ca::str::Utf8StringRef& value, int bytecodeOffset);
    int addForwardUninitializedType(const ca::str::Utf8StringRef& value, Label* label);
    /// @brief 合并两类型表项，登记其公共父类（经 provider 求解）。
    int     addMergedType(int typeTableIndex1, int typeTableIndex2,
                          ICommonSuperClassProvider* provider);
    Symbol* getType(int typeIndex);
    Label*  getForwardUninitializedLabel(int typeIndex);
    /// @}

    /// @return 正在写出的类内部名。
    ca::str::Utf8StringRef getClassName() const { return className_; }
    void                   setClassName(ca::str::Utf8StringRef name) { className_ = name; }
    /// @return 共享的字符串区。
    ca::str::Utf8StringArena& arena() { return arena_; }


private:
    // ---- Private "add" (insert without write, for CP init from ClassReader) ----
    void addConstantDynamicOrInvokeDynamicReference(int tag, int index,
                                                    const ca::str::Utf8StringRef& name,
                                                    const ca::str::Utf8StringRef& descriptor,
                                                    int bootstrapMethodIndex);

    // ---- Private hash helpers ----
    static int hash(int tag, const ca::str::Utf8StringRef& value);
    static int hash(int tag, int value);
    static int hash(int tag, int64_t value);
    static int hash(int tag, const ca::str::Utf8StringRef& a, const ca::str::Utf8StringRef& b);
    static int hash(int tag, const ca::str::Utf8StringRef& a, const ca::str::Utf8StringRef& b,
                    const ca::str::Utf8StringRef& c);
    static int hash(int tag, const ca::str::Utf8StringRef& a, const ca::str::Utf8StringRef& b,
                    int64_t data);
    static int hash(int tag, const ca::str::Utf8StringRef& a, const ca::str::Utf8StringRef& b,
                    const ca::str::Utf8StringRef& c, int64_t data);
    static int hash(int tag, const ca::str::Utf8StringRef& value, int data);
    static int hash(int tag, int a, int b);

    int    getHashBucket(int hashCode) const;
    void   resizeEntries(int newCapacity);
    Entry* get(int hashCode);
    Entry* put(Entry* entry);
    void   add(Entry* entry);

    void addConstantMemberReference(int index, int tag, const ca::str::Utf8StringRef& owner,
                                    const ca::str::Utf8StringRef& name,
                                    const ca::str::Utf8StringRef& descriptor);
    void addConstantIntegerOrFloat(int index, int tag, int value);
    void addConstantLongOrDouble(int index, int tag, int64_t value);
    void addConstantNameAndType(int index, const ca::str::Utf8StringRef& name,
                                const ca::str::Utf8StringRef& descriptor);
    void addConstantUtf8(int index, const ca::str::Utf8StringRef& value);
    void addConstantUtf8Reference(int index, int tag, const ca::str::Utf8StringRef& value);
    void addConstantMethodHandle(int index, int referenceKind, const ca::str::Utf8StringRef& owner,
                                 const ca::str::Utf8StringRef& name,
                                 const ca::str::Utf8StringRef& descriptor, bool isInterface);

    // ---- Private "put" (with dedup + write) ----

    Symbol* addConstantIntegerOrFloat(int tag, int value);
    Symbol* addConstantLongOrDouble(int tag, int64_t value);
    Symbol* addConstantUtf8Reference(int tag, const ca::str::Utf8StringRef& value);
    Symbol* addConstantMemberReference(int tag, const ca::str::Utf8StringRef& owner,
                                       const ca::str::Utf8StringRef& name,
                                       const ca::str::Utf8StringRef& descriptor);
    Symbol* addConstantDynamicOrInvokeDynamicReference(int tag, const ca::str::Utf8StringRef& name,
                                                       const ca::str::Utf8StringRef& descriptor,
                                                       int bootstrapMethodIndex);
    int     addTypeInternal(Entry* entry);
    LabelEntry* getOrAddLabelEntry(Label* label);

    int                    majorVersion_ = 0;
    ca::str::Utf8StringRef className_;
    int                    entryCount_        = 0;
    Entry**                entries_           = nullptr;
    int                    entriesCapacity_   = 0;
    int                    constantPoolCount_ = 1;
    ByteVector             constantPool_;
    int                    bootstrapMethodCount_ = 0;
    ByteVector*            bootstrapMethods_     = nullptr;
    int                    typeCount_            = 0;
    Entry**                typeTable_            = nullptr;
    int                    typeTableCapacity_    = 0;
    int                    labelCount_           = 0;
    LabelEntry**           labelTable_           = nullptr;
    int                    labelTableCapacity_   = 0;
    LabelEntry**           labelEntries_         = nullptr;
    int                    labelEntriesCapacity_ = 0;
    // -- arena 死亡点声明（memory-governance-plan §5-P1③）--
    // 本成员借用调用方（ClassWriter/上层）拥有的 Utf8StringArena（非本类私有，可多 Writer
    // 共享）：addConstantClass/addConstantUtf8/addType/addUninitializedType 等登记的类名、
    // 描述符、类型表项 intern 自该 arena；返回的 Symbol*、getType()、getClassName() 及
    // 成员 className_ 持有的视图均指向本 arena。
    // 视图寿命 ≠ 本对象寿命：本类析构不释放字符串，视图寿命 = 被借用 arena 的寿命，
    // 必须长于本 SymbolTable 及其返回的 Symbol/Utf8StringRef（见构造器 @warning）。
    // 消费者不得把视图带过本对象的死亡点；需跨期持有则 intern 进自己的 arena 或深拷贝。
    ca::str::Utf8StringArena&
        arena_;   // 借用：由调用方（ClassWriter/上层）拥有，多 Writer 可共享同一 arena
};

}   // namespace jbytecode::core
