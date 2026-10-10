#include "jbytecode/core/symbol_table.hpp"
#include "jbytecode/core/frame.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/type.hpp"
#include "libca/str/utf8_string.hpp"
#include <cstring>
#include <cstdlib>
#include <new>

using namespace ca;
using namespace ca::str;

namespace jbytecode::core {

// ============================================================================
// Symbol / Entry / LabelEntry constructors
// ============================================================================

Symbol::Symbol(int index, int tag, Utf8StringRef owner, Utf8StringRef name, Utf8StringRef value,
               int64_t data)
    : index(index)
    , tag(tag)
    , owner(owner)
    , name(name)
    , value(value)
    , data(data)
    , info(0)
{}

Entry::Entry(int index, int tag, Utf8StringRef value, int hashCode)
    : Symbol(index, tag, {}, {}, value, 0)
    , hashCode(hashCode)
{}
Entry::Entry(int index, int tag, int data, int hashCode)
    : Symbol(index, tag, {}, {}, {}, data)
    , hashCode(hashCode)
{}
Entry::Entry(int index, int tag, int64_t data, int hashCode)
    : Symbol(index, tag, {}, {}, {}, data)
    , hashCode(hashCode)
{}
Entry::Entry(int index, int tag, Utf8StringRef owner, Utf8StringRef name, Utf8StringRef value,
             int64_t data, int hashCode)
    : Symbol(index, tag, owner, name, value, data)
    , hashCode(hashCode)
{}
Entry::Entry(int index, int tag, Utf8StringRef name, Utf8StringRef value, int hashCode)
    : Symbol(index, tag, {}, name, value, 0)
    , hashCode(hashCode)
{}
Entry::Entry(int index, int tag, Utf8StringRef value, int64_t data, int hashCode)
    : Symbol(index, tag, {}, {}, value, data)
    , hashCode(hashCode)
{}

LabelEntry::LabelEntry(int index, Label* label, int hashCode)
    : index(index)
    , label(label)
    , hashCode(hashCode)
{}

// ============================================================================
// SymbolTable
// ============================================================================

SymbolTable::SymbolTable(ca::str::Utf8StringArena& arena)
    : arena_(arena)
    , constantPool_(256)
{
    entriesCapacity_ = 256;
    entries_         = new Entry*[entriesCapacity_]();
}

SymbolTable::~SymbolTable()
{
    for (int i = 0; i < entriesCapacity_; i++) {
        Entry* e = entries_[i];
        while (e) {
            Entry* next = e->next;
            delete e;
            e = next;
        }
    }
    delete[] entries_;
    // typeTable_ 是非拥有索引数组：其中的 Entry 同时挂在 entries_ 哈希桶上
    // （addTypeInternal 先 put 进桶、再登记进 typeTable_），已随 entries_ 统一
    // 释放，这里只释放数组本身。
    delete[] typeTable_;
    if (labelEntries_) {
        for (int i = 0; i < labelEntriesCapacity_; i++) {
            LabelEntry* e = labelEntries_[i];
            while (e) {
                LabelEntry* next = e->next;
                delete e;
                e = next;
            }
        }
        delete[] labelEntries_;
    }
    delete[] labelTable_;
    delete bootstrapMethods_;
}

// ---- Hash helpers (following ASM's hashCode computation) ----

int SymbolTable::getHashBucket(int hashCode) const
{
    return hashCode % entriesCapacity_;
}

void SymbolTable::resizeEntries(int newCapacity)
{
    Entry** newEntries = new Entry*[newCapacity]();
    for (int i = 0; i < entriesCapacity_; i++) {
        Entry* e = entries_[i];
        while (e) {
            Entry* next     = e->next;
            int    idx      = e->hashCode % newCapacity;
            e->next         = newEntries[idx];
            newEntries[idx] = e;
            e               = next;
        }
    }
    delete[] entries_;
    entries_         = newEntries;
    entriesCapacity_ = newCapacity;
}

Entry* SymbolTable::get(int hashCode)
{
    return entries_[hashCode % entriesCapacity_];
}

Entry* SymbolTable::put(Entry* entry)
{
    if (entryCount_ > (entriesCapacity_ * 3) / 4) {
        resizeEntries(entriesCapacity_ * 2 + 1);
    }
    entryCount_++;
    int idx       = entry->hashCode % entriesCapacity_;
    entry->next   = entries_[idx];
    entries_[idx] = entry;
    return entry;
}

void SymbolTable::add(Entry* entry)
{
    entryCount_++;
    int idx       = entry->hashCode % entriesCapacity_;
    entry->next   = entries_[idx];
    entries_[idx] = entry;
}

static int hashMix(int h)
{
    return h & 0x7FFFFFFF;
}

int SymbolTable::hash(int tag, const Utf8StringRef& value)
{
    int h = tag;
    for (usize i = 0; i < value.byte_length(); i++) h = h * 31 + value.byte_at(i);
    return hashMix(h);
}
int SymbolTable::hash(int tag, int value)
{
    return hashMix(tag + value);
}
int SymbolTable::hash(int tag, int64_t value)
{
    return hashMix(tag + (int)(value >> 32) + (int)value);
}
int SymbolTable::hash(int tag, const Utf8StringRef& a, const Utf8StringRef& b)
{
    // hash(tag, a, b)
    int h = tag;
    for (usize i = 0; i < a.byte_length(); i++) h = h * 31 + a.byte_at(i);
    for (usize i = 0; i < b.byte_length(); i++) h = h * 31 + b.byte_at(i);
    return hashMix(h);
}
int SymbolTable::hash(int tag, const Utf8StringRef& a, const Utf8StringRef& b, int64_t data)
{
    int h = tag;
    for (usize i = 0; i < a.byte_length(); i++) h = h * 31 + a.byte_at(i);
    for (usize i = 0; i < b.byte_length(); i++) h = h * 31 + b.byte_at(i);
    return hashMix(h + (int)data);
}
int SymbolTable::hash(int tag, const Utf8StringRef& a, const Utf8StringRef& b,
                      const Utf8StringRef& c)
{
    int h = tag;
    for (usize i = 0; i < a.byte_length(); i++) h = h * 31 + a.byte_at(i);
    for (usize i = 0; i < b.byte_length(); i++) h = h * 31 + b.byte_at(i);
    for (usize i = 0; i < c.byte_length(); i++) h = h * 31 + c.byte_at(i);
    return hashMix(h);
}
int SymbolTable::hash(int tag, const Utf8StringRef& a, const Utf8StringRef& b,
                      const Utf8StringRef& c, int64_t data)
{
    int h = tag;
    for (usize i = 0; i < a.byte_length(); i++) h = h * 31 + a.byte_at(i);
    for (usize i = 0; i < b.byte_length(); i++) h = h * 31 + b.byte_at(i);
    for (usize i = 0; i < c.byte_length(); i++) h = h * 31 + c.byte_at(i);
    return hashMix(h + (int)data);
}
int SymbolTable::hash(int tag, const Utf8StringRef& value, int data)
{
    int h = tag;
    for (usize i = 0; i < value.byte_length(); i++) h = h * 31 + value.byte_at(i);
    return hashMix(h + data);
}
int SymbolTable::hash(int tag, int a, int b)
{
    return hashMix(tag + a + b);
}

// ---- putConstantPool / putBootstrapMethods ----

void SymbolTable::putConstantPool(ByteVector& output)
{
    output.putShort(constantPoolCount_);
    if (constantPool_.size() > 0)
        output.putByteArray(constantPool_.data(), 0, constantPool_.size());
}

int SymbolTable::computeBootstrapMethodsSize()
{
    if (bootstrapMethods_) {
        addConstantUtf8(ZUtf8StringRef::from_static("BootstrapMethods"));
        return 8 + bootstrapMethods_->size();
    }
    return 0;
}

void SymbolTable::putBootstrapMethods(ByteVector& output)
{
    if (bootstrapMethods_) {
        output.putShort(addConstantUtf8(ZUtf8StringRef::from_static("BootstrapMethods")));
        output.putInt(bootstrapMethods_->size() + 2);
        output.putShort(bootstrapMethodCount_);
        output.putByteArray(bootstrapMethods_->data(), 0, bootstrapMethods_->size());
    }
}

// ---- addConstantIntegerOrFloat (internal, with dedup + write) ----

Symbol* SymbolTable::addConstantIntegerOrFloat(int tag, int value)
{
    int h = hash(tag, value);
    for (Entry* e = get(h); e; e = e->next) {
        if (e->tag == tag && e->hashCode == h && e->data == value) return e;
    }
    constantPool_.putByte(tag).putInt(value);
    return put(new Entry(constantPoolCount_++, tag, value, h));
}

// ---- addConstantLongOrDouble (internal, with dedup + write) ----

Symbol* SymbolTable::addConstantLongOrDouble(int tag, int64_t value)
{
    int h = hash(tag, value);
    for (Entry* e = get(h); e; e = e->next) {
        if (e->tag == tag && e->hashCode == h && e->data == value) return e;
    }
    int index = constantPoolCount_;
    constantPool_.putByte(tag).putLong(value);
    constantPoolCount_ += 2;
    return put(new Entry(index, tag, value, h));
}

// ---- addConstantUtf8Reference (internal, with dedup) ----
// Covers: CONSTANT_Class, CONSTANT_String, CONSTANT_MethodType,
//         CONSTANT_Module, CONSTANT_Package

Symbol* SymbolTable::addConstantUtf8Reference(int tag, const Utf8StringRef& value)
{
    int h = hash(tag, value);
    for (Entry* e = get(h); e; e = e->next) {
        if (e->tag == tag && e->hashCode == h && e->value.equals(value)) return e;
    }
    constantPool_.put12(tag, addConstantUtf8(value));
    return put(new Entry(constantPoolCount_++, tag, value, h));
}

// ---- addConstantMemberReference (internal, with dedup) ----
// Covers: CONSTANT_Fieldref, CONSTANT_Methodref, CONSTANT_InterfaceMethodref

Symbol* SymbolTable::addConstantMemberReference(int tag, const Utf8StringRef& owner,
                                                const Utf8StringRef& name,
                                                const Utf8StringRef& descriptor)
{
    int h = hash(tag, owner, name, descriptor);
    for (Entry* e = get(h); e; e = e->next) {
        if (e->tag == tag && e->hashCode == h && e->owner.equals(owner) && e->name.equals(name) &&
            e->value.equals(descriptor))
            return e;
    }
    constantPool_.put122(
        tag, addConstantClass(owner)->index, addConstantNameAndType(name, descriptor));
    return put(new Entry(constantPoolCount_++, tag, owner, name, descriptor, 0, h));
}

// ---- Public API ----

int SymbolTable::addConstantUtf8(const Utf8StringRef& value)
{
    Utf8StringRef key = arena_.intern(value);
    if (!key.data() && value.data() && value.byte_length() != 0) {
        key = arena_.intern_raw(value.data(), value.byte_length());
    }
    int           h   = hash(Symbol::CONSTANT_UTF8_TAG, key);
    for (Entry* e = get(h); e; e = e->next) {
        if (e->tag == Symbol::CONSTANT_UTF8_TAG && e->hashCode == h && e->value.equals(key))
            return e->index;
    }
    constantPool_.putByte(Symbol::CONSTANT_UTF8_TAG).putUTF8(key);
    return put(new Entry(constantPoolCount_++, Symbol::CONSTANT_UTF8_TAG, key, h))->index;
}

int SymbolTable::addConstantNameAndType(const Utf8StringRef& name, const Utf8StringRef& descriptor)
{
    Utf8StringRef nameKey = arena_.intern(name);
    Utf8StringRef descKey = arena_.intern(descriptor);
    int           tag     = Symbol::CONSTANT_NAME_AND_TYPE_TAG;
    int           h       = hash(tag, nameKey, descKey);
    for (Entry* e = get(h); e; e = e->next) {
        if (e->tag == tag && e->hashCode == h && e->name.equals(nameKey) &&
            e->value.equals(descKey))
            return e->index;
    }
    constantPool_.put122(tag, addConstantUtf8(nameKey), addConstantUtf8(descKey));
    return put(new Entry(constantPoolCount_++, tag, nameKey, descKey, h))->index;
}

Symbol* SymbolTable::addConstantInteger(int value)
{
    return addConstantIntegerOrFloat(Symbol::CONSTANT_INTEGER_TAG, value);
}
Symbol* SymbolTable::addConstantFloat(float value)
{
    int bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return addConstantIntegerOrFloat(Symbol::CONSTANT_FLOAT_TAG, bits);
}
Symbol* SymbolTable::addConstantLong(int64_t value)
{
    return addConstantLongOrDouble(Symbol::CONSTANT_LONG_TAG, value);
}
Symbol* SymbolTable::addConstantDouble(double value)
{
    int64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return addConstantLongOrDouble(Symbol::CONSTANT_DOUBLE_TAG, bits);
}
Symbol* SymbolTable::addConstantClass(const Utf8StringRef& value)
{
    return addConstantUtf8Reference(Symbol::CONSTANT_CLASS_TAG, arena_.intern(value));
}
Symbol* SymbolTable::addConstantString(const Utf8StringRef& value)
{
    auto key = arena_.intern(value);
    if (!key.data() && value.data() && value.byte_length() != 0) {
        key = arena_.intern_raw(value.data(), value.byte_length());
    }
    return addConstantUtf8Reference(Symbol::CONSTANT_STRING_TAG, key);
}
Symbol* SymbolTable::addConstantMethodType(const Utf8StringRef& methodDescriptor)
{
    return addConstantUtf8Reference(Symbol::CONSTANT_METHOD_TYPE_TAG,
                                    arena_.intern(methodDescriptor));
}
Symbol* SymbolTable::addConstantModule(const Utf8StringRef& moduleName)
{
    return addConstantUtf8Reference(Symbol::CONSTANT_MODULE_TAG, arena_.intern(moduleName));
}
Symbol* SymbolTable::addConstantPackage(const Utf8StringRef& packageName)
{
    return addConstantUtf8Reference(Symbol::CONSTANT_PACKAGE_TAG, arena_.intern(packageName));
}
Symbol* SymbolTable::addConstantFieldref(const Utf8StringRef& owner, const Utf8StringRef& name,
                                         const Utf8StringRef& descriptor)
{
    return addConstantMemberReference(Symbol::CONSTANT_FIELDREF_TAG,
                                      arena_.intern(owner),
                                      arena_.intern(name),
                                      arena_.intern(descriptor));
}
Symbol* SymbolTable::addConstantMethodref(const Utf8StringRef& owner, const Utf8StringRef& name,
                                          const Utf8StringRef& descriptor, bool isInterface)
{
    int tag =
        isInterface ? Symbol::CONSTANT_INTERFACE_METHODREF_TAG : Symbol::CONSTANT_METHODREF_TAG;
    return addConstantMemberReference(
        tag, arena_.intern(owner), arena_.intern(name), arena_.intern(descriptor));
}
Symbol* SymbolTable::addConstantMethodHandle(int referenceKind, const Utf8StringRef& owner,
                                             const Utf8StringRef& name,
                                             const Utf8StringRef& descriptor, bool isInterface)
{
    int           tag  = Symbol::CONSTANT_METHOD_HANDLE_TAG;
    int           data = (referenceKind > 4 && isInterface) ? (referenceKind << 8) : referenceKind;
    Utf8StringRef ownKey  = arena_.intern(owner);
    Utf8StringRef nameKey = arena_.intern(name);
    Utf8StringRef descKey = arena_.intern(descriptor);
    int           h       = hash(tag, ownKey, nameKey, descKey, data);
    for (Entry* e = get(h); e; e = e->next) {
        if (e->tag == tag && e->hashCode == h && e->data == data && e->owner.equals(ownKey) &&
            e->name.equals(nameKey) && e->value.equals(descKey))
            return e;
    }
    if (referenceKind <= 4) {   // H_GETFIELD..H_PUTSTATIC → Fieldref
        constantPool_.put112(
            tag, referenceKind, addConstantFieldref(owner, name, descriptor)->index);
    }
    else {
        constantPool_.put112(
            tag, referenceKind, addConstantMethodref(owner, name, descriptor, isInterface)->index);
    }
    return put(new Entry(constantPoolCount_++, tag, ownKey, nameKey, descKey, data, h));
}

// ---- addType / addUninitializedType / addMergedType / getType ----

int SymbolTable::addType(const Utf8StringRef& value)
{
    int h = hash(Symbol::TYPE_TAG, value);
    for (Entry* e = get(h); e; e = e->next) {
        if (e->tag == Symbol::TYPE_TAG && e->hashCode == h && e->value.equals(value))
            return e->index;
    }
    // 驻留后再存：Entry::value 是视图（Utf8StringRef），调用方可能传临时缓冲的视图
    // （Frame::getAbstractTypeFromApiFormat 用栈上 std::string 拼 "L...;" 描述符后
    // slice 出内部名传入），直接存视图会在调用方返回后悬挂——ASan 实证 heap-use-
    // after-free（后续 addConstantClass→arena_.intern 读已释放内存，class 名错乱）。
    // arena_.intern 拷入符号表 arena（与 SymbolTable 同生命周期），内容语义不变。
    return addTypeInternal(new Entry(typeCount_, Symbol::TYPE_TAG, arena_.intern(value), h));
}

int SymbolTable::addUninitializedType(const Utf8StringRef& value, int bytecodeOffset)
{
    int h = hash(Symbol::UNINITIALIZED_TYPE_TAG, value, bytecodeOffset);
    for (Entry* e = get(h); e; e = e->next) {
        if (e->tag == Symbol::UNINITIALIZED_TYPE_TAG && e->hashCode == h &&
            e->data == bytecodeOffset && e->value.equals(value))
            return e->index;
    }
    // 与 addType 同规：Entry::value 是视图，value 可能指向调用方栈上临时缓冲
    // （UNINITIALIZED bitmask 场景同样经 ApiFormat 拼接路径），先驻留再存。
    return addTypeInternal(
        new Entry(typeCount_, Symbol::UNINITIALIZED_TYPE_TAG, arena_.intern(value), bytecodeOffset, h));
}

int SymbolTable::addMergedType(int typeTableIndex1, int typeTableIndex2,
                               ICommonSuperClassProvider* provider)
{
    int64_t data = (typeTableIndex1 < typeTableIndex2)
                       ? typeTableIndex1 | (((int64_t)typeTableIndex2) << 32)
                       : typeTableIndex2 | (((int64_t)typeTableIndex1) << 32);
    int     h    = hash(Symbol::MERGED_TYPE_TAG, typeTableIndex1 + typeTableIndex2);
    for (Entry* e = get(h); e; e = e->next) {
        if (e->tag == Symbol::MERGED_TYPE_TAG && e->hashCode == h && e->data == data)
            return e->info;
    }
    // Get the common super class
    Utf8StringRef t1 = typeTable_[typeTableIndex1]->value;
    Utf8StringRef t2 = typeTable_[typeTableIndex2]->value;
    Utf8StringRef commonSuper;
    if (provider) {
        commonSuper = provider->getCommonSuperClass(t1, t2);
    }
    else {
        static const u8 kObj[] = "java/lang/Object";
        commonSuper            = arena_.intern(Utf8StringRef(kObj, 16, 16));
    }
    int    commonSuperTypeIndex = addType(commonSuper);
    Entry* merged               = new Entry(typeCount_, Symbol::MERGED_TYPE_TAG, data, h);
    merged->info                = commonSuperTypeIndex;
    put(merged);
    return commonSuperTypeIndex;
}

Symbol* SymbolTable::getType(int typeIndex)
{
    if (!typeTable_ || typeIndex < 0 || typeIndex >= typeCount_) return nullptr;
    return typeTable_[typeIndex];
}

int SymbolTable::addTypeInternal(Entry* entry)
{
    if (typeTable_ == nullptr) {
        typeTableCapacity_ = 16;
        typeTable_         = new Entry*[typeTableCapacity_]();
    }
    else if (typeCount_ >= typeTableCapacity_) {
        int     newCap   = typeTableCapacity_ * 2;
        Entry** newTable = new Entry*[newCap]();
        for (int i = 0; i < typeCount_; i++) newTable[i] = typeTable_[i];
        delete[] typeTable_;
        typeTable_         = newTable;
        typeTableCapacity_ = newCap;
    }
    typeTable_[typeCount_] = entry;
    put(entry);
    return typeCount_++;
}

LabelEntry* SymbolTable::getOrAddLabelEntry(Label* label)
{
    int h = hashMix((int)(intptr_t)label);
    if (labelEntries_ == nullptr) {
        labelEntriesCapacity_ = 16;
        labelEntries_         = new LabelEntry*[labelEntriesCapacity_]();
    }
    int idx = h % labelEntriesCapacity_;
    for (LabelEntry* e = labelEntries_[idx]; e; e = e->next) {
        if (e->label == label) return e;
    }
    if (labelCount_ > (labelEntriesCapacity_ * 3) / 4) {
        int          oldCap     = labelEntriesCapacity_;
        int          newCap     = oldCap * 2 + 1;
        LabelEntry** newEntries = new LabelEntry*[newCap]();
        for (int i = 0; i < oldCap; i++) {
            for (LabelEntry* e = labelEntries_[i]; e;) {
                LabelEntry* next = e->next;
                int         ni   = e->hashCode % newCap;
                e->next          = newEntries[ni];
                newEntries[ni]   = e;
                e                = next;
            }
        }
        delete[] labelEntries_;
        labelEntries_         = newEntries;
        labelEntriesCapacity_ = newCap;
        idx                   = h % newCap;
    }
    if (labelTable_ == nullptr) {
        labelTableCapacity_ = 16;
        labelTable_         = new LabelEntry*[labelTableCapacity_]();
    }
    else if (labelCount_ >= labelTableCapacity_) {
        int          newCap   = labelTableCapacity_ * 2;
        LabelEntry** newTable = new LabelEntry*[newCap]();
        for (int i = 0; i < labelCount_; i++) newTable[i] = labelTable_[i];
        delete[] labelTable_;
        labelTable_         = newTable;
        labelTableCapacity_ = newCap;
    }
    auto* le                 = new LabelEntry(labelCount_, label, h);
    le->next                 = labelEntries_[idx];
    labelEntries_[idx]       = le;
    labelTable_[labelCount_] = le;
    return labelTable_[labelCount_++];
}

// ============================================================================
// addForwardUninitializedType / getForwardUninitializedLabel
// ============================================================================

int SymbolTable::addForwardUninitializedType(const Utf8StringRef& value, Label* label)
{
    int labelIndex = getOrAddLabelEntry(label)->index;
    int h          = hash(Symbol::FORWARD_UNINITIALIZED_TYPE_TAG, value, labelIndex);
    for (Entry* e = get(h); e; e = e->next) {
        if (e->tag == Symbol::FORWARD_UNINITIALIZED_TYPE_TAG && e->hashCode == h &&
            e->data == labelIndex && e->value.equals(value))
            return e->index;
    }
    return addTypeInternal(
        new Entry(typeCount_, Symbol::FORWARD_UNINITIALIZED_TYPE_TAG, value, labelIndex, h));
}

Label* SymbolTable::getForwardUninitializedLabel(int typeIndex)
{
    return labelTable_[(int)typeTable_[typeIndex]->data]->label;
}

// ============================================================================
// Private: addConstantDynamicOrInvokeDynamicReference (no dedup — for CP init)
// ============================================================================

void SymbolTable::addConstantDynamicOrInvokeDynamicReference(int tag, int index,
                                                             const Utf8StringRef& name,
                                                             const Utf8StringRef& descriptor,
                                                             int bootstrapMethodIndex)
{
    int h = hash(tag, name, descriptor, bootstrapMethodIndex);
    add(new Entry(index, tag, Utf8StringRef(), name, descriptor, bootstrapMethodIndex, h));
}

// ============================================================================
// Private: addConstantDynamicOrInvokeDynamicReference (with dedup + write)
// ============================================================================

Symbol* SymbolTable::addConstantDynamicOrInvokeDynamicReference(int tag, const Utf8StringRef& name,
                                                                const Utf8StringRef& descriptor,
                                                                int bootstrapMethodIndex)
{
    int h = hash(tag, name, descriptor, bootstrapMethodIndex);
    for (Entry* e = get(h); e; e = e->next) {
        if (e->tag == tag && e->hashCode == h && e->data == bootstrapMethodIndex &&
            e->name.equals(name) && e->value.equals(descriptor)) {
            return e;
        }
    }
    constantPool_.put122(tag, bootstrapMethodIndex, addConstantNameAndType(name, descriptor));
    return put(new Entry(
        constantPoolCount_++, tag, Utf8StringRef(), name, descriptor, bootstrapMethodIndex, h));
}

// ============================================================================
// addBootstrapMethod
// ============================================================================

static int hashHandle(const Handle& h)
{
    auto strHash = [](Utf8StringRef s) -> int {
        int hc = 0;
        for (usize i = 0; i < s.byte_length(); i++) hc = hc * 31 + s.byte_at(i);
        return hc;
    };
    return static_cast<int>(h.getTag()) + (h.isInterface() ? 64 : 0) +
           strHash(h.getOwner()) * strHash(h.getName()) * strHash(h.getDesc());
}

static int hashAny(const ca::core::Any& val, ca::str::Utf8StringArena& arena)
{
    if (val.is<int>()) return val.cast<int>();
    if (val.is<i64>()) return (int)val.cast<i64>();
    if (val.is<float>()) return (int)val.cast<float>();
    if (val.is<double>()) return (int)val.cast<double>();
    if (val.is<Utf8StringRef>()) {
        auto s  = val.cast<Utf8StringRef>();
        int  hc = 0;
        for (usize i = 0; i < s.byte_length(); i++) hc = hc * 31 + s.byte_at(i);
        return hc;
    }
    if (val.is<Type>()) {
        auto ty = val.cast<Type>();
        // #921 staticArena 退役：OBJECT 排序描述符的 intern 落点必选注入本表作业 arena
        //（哈希本身仅瞬时消费字节）。
        auto d  = ty.getDescriptor(arena);
        int  hc = 0;
        for (usize i = 0; i < d.byte_length(); i++) hc = hc * 31 + d.byte_at(i);
        return hc;
    }
    if (val.is<Handle>()) {
        return hashHandle(val.cast<Handle>());
    }
    if (val.is<ConstantDynamic>()) {
        auto cd = val.cast<ConstantDynamic>();
        int  hc = hashAny(ca::core::Any(cd.getName()), arena) ^
                  hashAny(ca::core::Any(cd.getDescriptor()), arena);
        hc ^= hashHandle(cd.getBootstrapMethod());
        for (const auto& arg : cd.getBootstrapMethodArguments()) hc ^= hashAny(arg, arena);
        return hc;
    }
    return 0;
}

Symbol* SymbolTable::addBootstrapMethod(const Handle&                     bootstrapMethodHandle,
                                        const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    if (bootstrapMethods_ == nullptr) {
        bootstrapMethods_ = new ByteVector();
    }

    // First, add all arguments to CP
    std::vector<int> argIndices;
    for (auto& arg : bootstrapMethodArguments) {
        if (arg.is<int>())
            argIndices.push_back(addConstantInteger(arg.cast<int>())->index);
        else if (arg.is<i64>())
            argIndices.push_back(addConstantLong(arg.cast<i64>())->index);
        else if (arg.is<float>())
            argIndices.push_back(addConstantFloat(arg.cast<float>())->index);
        else if (arg.is<double>())
            argIndices.push_back(addConstantDouble(arg.cast<double>())->index);
        else if (arg.is<Utf8StringRef>()) {
            argIndices.push_back(addConstantString(arg.cast<Utf8StringRef>())->index);
        }
        else if (arg.is<Type>()) {
            auto type = arg.cast<Type>();
            // #921 staticArena 退役：intern 落点必选注入本表作业 arena。
            if (type.getSort() == Type::METHOD) {
                argIndices.push_back(addConstantMethodType(type.getDescriptor(arena()))->index);
            }
            else {
                argIndices.push_back(addConstantClass(type.getInternalName(arena()))->index);
            }
        }
        else if (arg.is<Handle>()) {
            auto& h = arg.cast<const Handle&>();
            argIndices.push_back(
                addConstantMethodHandle(
                    h.getTag(), h.getOwner(), h.getName(), h.getDesc(), h.isInterface())
                    ->index);
        }
        else if (arg.is<ConstantDynamic>()) {
            auto cd = arg.cast<ConstantDynamic>();
            argIndices.push_back(addConstantDynamic(cd.getName(),
                                                    cd.getDescriptor(),
                                                    cd.getBootstrapMethod(),
                                                    cd.getBootstrapMethodArguments())
                                     ->index);
        }
        else {
            argIndices.push_back(0);
        }
    }

    // Write bsm data
    int bsmOffset = bootstrapMethods_->size();
    bootstrapMethods_->putShort(addConstantMethodHandle(bootstrapMethodHandle.getTag(),
                                                        bootstrapMethodHandle.getOwner(),
                                                        bootstrapMethodHandle.getName(),
                                                        bootstrapMethodHandle.getDesc(),
                                                        bootstrapMethodHandle.isInterface())
                                    ->index);
    bootstrapMethods_->putShort((int)argIndices.size());
    for (int idx : argIndices) bootstrapMethods_->putShort(idx);

    // Compute hash code
    int bsmLength = bootstrapMethods_->size() - bsmOffset;
    int hashCode  = hashHandle(bootstrapMethodHandle);
    for (auto& arg : bootstrapMethodArguments) hashCode ^= hashAny(arg, arena());
    hashCode &= 0x7FFFFFFF;

    // Check for duplicate — if found, revert bootstrapMethods_ to original length
    Entry* entry = get(hashCode);
    while (entry != nullptr) {
        if (entry->tag == Symbol::BOOTSTRAP_METHOD_TAG && entry->hashCode == hashCode) {
            int   otherOffset = (int)entry->data;
            bool  same        = true;
            auto* bsmData     = bootstrapMethods_->mutableData();
            for (int i = 0; i < bsmLength; ++i) {
                if (bsmData[bsmOffset + i] != bsmData[otherOffset + i]) {
                    same = false;
                    break;
                }
            }
            if (same) {
                bootstrapMethods_->length_ = bsmOffset;   // revert
                return entry;
            }
        }
        entry = entry->next;
    }
    return put(
        new Entry(bootstrapMethodCount_++, Symbol::BOOTSTRAP_METHOD_TAG, bsmOffset, hashCode));
}

// ============================================================================
// addConstantDynamic / addConstantInvokeDynamic
// ============================================================================

Symbol* SymbolTable::addConstantDynamic(const Utf8StringRef& name, const Utf8StringRef& descriptor,
                                        const Handle&                     bootstrapMethodHandle,
                                        const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    Symbol* bsm = addBootstrapMethod(bootstrapMethodHandle, bootstrapMethodArguments);
    return addConstantDynamicOrInvokeDynamicReference(
        Symbol::CONSTANT_DYNAMIC_TAG, name, descriptor, bsm->index);
}

Symbol* SymbolTable::addConstantInvokeDynamic(
    const Utf8StringRef& name, const Utf8StringRef& descriptor, const Handle& bootstrapMethodHandle,
    const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    Symbol* bsm = addBootstrapMethod(bootstrapMethodHandle, bootstrapMethodArguments);
    return addConstantDynamicOrInvokeDynamicReference(
        Symbol::CONSTANT_INVOKE_DYNAMIC_TAG, name, descriptor, bsm->index);
}

}   // namespace jbytecode::core
