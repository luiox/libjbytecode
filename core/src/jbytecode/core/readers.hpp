#pragma once

/// @file        readers.hpp
/// @brief       所有 Reader 类的声明聚合（对称于 writers.hpp / visitors.hpp）。
///
///              包含：
///              - ClassReader    （原 class_reader.hpp，已并入）
///              - SignatureReader（原 readers.hpp 内联实现，已移至 signature_reader.cpp）
///
///              本文件只含声明，实现见 class_reader.cpp / signature_reader.cpp。

#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/byte_vector.hpp"
#include "jbytecode/core/byte_array.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/symbol_table.hpp"
#include "garena/arena.hpp"
#include "libca/core/datatype.hpp"
#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

#include <string>
#include <vector>
#include <istream>

#include "libca/core/any.hpp"

namespace jbytecode::core {

// ============================================================================
// ClassReader — 对标 org.objectweb.asm.ClassReader
// 原独立于 class_reader.hpp，现并入 readers.hpp 以保持 Reader 聚合。
// ============================================================================

/// @brief 解析 classfile 并驱动 ClassVisitor，行为对齐 ASM ClassReader。
///
/// @warning 传入的 arena
/// 为借用对象，必须长于本 reader 以及持有其 Utf8StringRef 的 visitor/tree。
/// @warning
/// 经由本类读取辅助函数访问的偏移和常量池索引会先校验；检测到非法值时抛出
///
/// std::invalid_argument。调用方仍应将任意外部 classfile 视为不可信输入。
class ClassReader
{
public:
    static constexpr int SKIP_CODE     = 1;
    static constexpr int SKIP_DEBUG    = 2;
    static constexpr int SKIP_FRAMES   = 4;
    static constexpr int EXPAND_FRAMES = 8;

    static constexpr int EXPAND_ASM_INSNS = 256;

    static constexpr int MAX_BUFFER_SIZE              = 1024 * 1024;
    static constexpr int INPUT_STREAM_DATA_CHUNK_SIZE = 4096;

    /// @brief 注解 element value '@'/'[' 互递归的深度上限。
    ///
    /// 与 libjclassfile AnnotationReader::MAX_NESTING 数值一致；敌意 class
    /// 声明的嵌套超过此值时，accept 抛 std::invalid_argument 而非栈溢出。
    static constexpr int MAX_ANNOTATION_NESTING = 50;

    /// @brief ConstantDynamic 经 bootstrap 参数互引用的递归深度上限。
    ///
    /// 环引用（condy A 的 bootstrap 参数指向 condy A 自身）会让 readConst
    /// 无界递归到栈溢出（memoization 在递归返回后才写入，挡不住环）；合法
    /// javac 产物的 condy 嵌套深度只有 1-2 层。超过上限抛 std::invalid_argument。
    /// 管线路径由 classfile 净化层（class_file_sanitizer）在 reader 之前以
    /// 带 CP index 的诊断拒绝环；本守卫保护绕过净化层的直接使用者。
    static constexpr int MAX_CONSTANT_DYNAMIC_NESTING = 64;

    //  @Deprecated(forRemoval = false)
    ca::core::ByteArray b;

    ca::i32                             header;
    ca::core::ByteArray                 classFileBuffer;
    std::vector<ca::i32>                cpInfoOffsets;
    std::vector<ca::str::Utf8StringRef> constantUtf8Values;
    std::vector<ConstantDynamic>        constantDynamicValues;

    std::vector<ca::i32> bootstrapMethodOffsets;
    ca::i32              maxStringLength;

    // ---- Constructors ----
    /// @param arena     Borrowed UTF-8 storage for constant-pool strings decoded by this reader.
    /// @param classFile Class bytes copied into this reader.
    ClassReader(ca::str::Utf8StringArena& arena, const std::vector<ca::u8>& classFile);

    /// @param arena       Borrowed UTF-8 storage for strings decoded by this reader.
    /// @param inputStream Stream read completely during construction.
    ClassReader(ca::str::Utf8StringArena& arena, std::istream& inputStream);

    /// @param arena         Borrowed UTF-8 storage for strings decoded by this reader.
    /// @param classFilePath Path read completely during construction.
    ClassReader(ca::str::Utf8StringArena& arena, const std::string& classFilePath);

    /// @param arena             Borrowed UTF-8 storage for strings decoded by this reader.
    /// @param classFileBuffer   Class bytes copied from this buffer.
    /// @param classFileOffset   Offset of the class bytes in classFileBuffer.
    /// @param classFileLength   Number of bytes to read.
    ClassReader(ca::str::Utf8StringArena& arena, const ca::u8* classFileBuffer,
                ca::i32 classFileOffset, ca::i32 classFileLength);

    /// @param arena           Borrowed UTF-8 storage for strings decoded by this reader.
    /// @param classFileBuffer Class bytes copied from this buffer.
    /// @param classFileLength Number of bytes to read.
    ClassReader(ca::str::Utf8StringArena& arena, const ca::u8* classFileBuffer,
                ca::i32 classFileLength)
        : ClassReader(arena, classFileBuffer, 0, classFileLength)
    {}

    ~ClassReader();

    // ---- Accessors ----

    /// @brief 返回类访问标志。
    ca::i32 getAccess() const { return readUnsignedShort(header_); }
    /// @brief 返回当前类的内部名。
    ca::str::Utf8StringRef getClassName();
    /// @brief 返回父类内部名；java/lang/Object 返回空引用。
    ca::str::Utf8StringRef getSuperName();
    /// @brief 返回直接实现或继承的接口内部名。
    std::vector<ca::str::Utf8StringRef> getInterfaces();

    /// @brief 返回常量池项负载偏移；索引 0 是保留槽位并返回 0。
    ca::i32 getItem(ca::i32 index) const { return index == 0 ? 0 : cpInfoOffset(index); }
    /// @brief 返回常量池槽位总数（含索引 0 保留槽位）。
    ca::i32 getItemCount() const { return constantPoolCount_; }
    /// @brief 返回解析 UTF-8 常量所需的最大临时缓冲区长度。
    ca::i32 getMaxStringLength() const { return maxStringLength_; }
    /// @brief 返回 classfile 原始字节缓冲区的首地址。
    const ca::u8* getClassFileBuffer() const { return classFileBuffer_; }

    // ---- Main API ----

    /// @brief 解析 classfile 并回调 visitor。
    /// @param parsingOptions
    /// SKIP_CODE、SKIP_DEBUG、SKIP_FRAMES 或 EXPAND_FRAMES 的组合。
    /// @throws
    /// std::invalid_argument classfile 结构、常量池索引或读取范围非法时抛出。
    void accept(ClassVisitor* classVisitor, ca::i32 parsingOptions);

    /// @brief 使用自定义属性原型解析 classfile 并回调 visitor。
    void accept(ClassVisitor* classVisitor, Attribute* attributePrototypes, ca::i32 parsingOptions);

    // ---- Read helpers (public so Attribute subclasses can use them) ----

    /// @brief 读取有符号字节；offset 越界时抛 std::invalid_argument。
    ca::i32                readByte(ca::i32 offset) const;
    ca::i32                readUnsignedShort(ca::i32 offset) const;
    ca::i32                readShort(ca::i32 offset) const;
    ca::i32                readInt(ca::i32 offset) const;
    ca::i64                readLong(ca::i32 offset) const;
    ca::str::Utf8StringRef readUTF8(ca::i32 offset, std::vector<char>& charBuffer);
    ca::str::Utf8StringRef readClass(ca::i32 offset, std::vector<char>& charBuffer);
    ca::str::Utf8StringRef readModule(ca::i32 offset, std::vector<char>& charBuffer);
    ca::str::Utf8StringRef readPackage(ca::i32 offset, std::vector<char>& charBuffer)
    {
        return readModule(offset, charBuffer);
    }
    ca::core::Any          readConst(ca::i32 itemIndex, std::vector<char>& charBuffer);
    ca::str::Utf8StringRef readUtf(ca::i32 itemIndex, std::vector<char>& charBuffer);
    ca::str::Utf8StringRef readUtf(ca::i32 offset, ca::i32 utfLength,
                                   std::vector<char>& charBuffer);

private:
    // ---- Internal init ----
    void init(const ca::u8* classFileBuffer, ca::i32 classFileOffset, ca::i32 classFileLength);

    /// 注解递归深度守卫：构造时 +1、析构时 -1，readElementValue 每层递归
    /// 持有一个实例；上限见 MAX_ANNOTATION_NESTING。
    class AnnotationDepthGuard
    {
    public:
        explicit AnnotationDepthGuard(ClassReader& reader)
            : reader_(reader)
        {
            ++reader_.annotationDepth_;
        }
        ~AnnotationDepthGuard() { --reader_.annotationDepth_; }

        AnnotationDepthGuard(const AnnotationDepthGuard&)            = delete;
        AnnotationDepthGuard& operator=(const AnnotationDepthGuard&) = delete;

    private:
        ClassReader& reader_;
    };

    // ---- Internal helpers ----
    int                 getFirstAttributeOffset() const;
    void                readBootstrapMethodsAttribute(ca::i32 maxStringLength);
    ca::core::Any       readConstantDynamic(ca::i32 idx, std::vector<char>& buf);
    std::vector<ca::u8> ownedData_;   // stored when constructed from vector
    void                checkRange(ca::i32 offset, ca::i32 size) const;
    bool                isValidCpIndex(ca::i32 index) const;
    ca::i32             cpInfoOffset(ca::i32 index) const;

    /// ConstantDynamic 递归深度计数与 RAII 守卫（见 MAX_CONSTANT_DYNAMIC_NESTING）。
    ca::i32 constantDynamicDepth_ = 0;
    class CondyDepthGuard
    {
    public:
        explicit CondyDepthGuard(ca::i32& depth)
            : depth_(depth)
        {
            ++depth_;
        }
        ~CondyDepthGuard() { --depth_; }
        CondyDepthGuard(const CondyDepthGuard&)            = delete;
        CondyDepthGuard& operator=(const CondyDepthGuard&) = delete;

    private:
        ca::i32& depth_;
    };
    /// @brief 返回 bootstrap 方法的字节偏移；index 越界抛 std::invalid_argument。
    ///
    /// Dynamic/InvokeDynamic 条目的 bootstrap_method_attr_index 来自不可信输入，
    /// 无界访问 bootstrapMethodOffsets_ 是越界读（UB）；经本辅助统一校验。
    ca::i32             bootstrapMethodOffset(ca::i32 index) const;

    /// B8：reader 侧 Label 统一建点——labelArena_ 分配 + owner 盖章。
    Label* newReaderLabel()
    {
        auto* label = labelArena_.create<Label>();
        label->setOwner(&labelArena_);
        return label;
    }

    // ---- Internal parsing ----

    int  readField(ClassVisitor* classVisitor, Context& context, int fieldInfoOffset);
    int  readMethod(ClassVisitor* classVisitor, Context& context, int methodInfoOffset);
    void readCode(MethodVisitor* methodVisitor, Context& context, int codeAttributeOffset);
    void readModuleAttributes(ClassVisitor* classVisitor, Context& context, int moduleOffset,
                              int                           modulePackagesOffset,
                              const ca::str::Utf8StringRef& moduleMainClass);
    int  readRecordComponent(ClassVisitor* classVisitor, Context& context,
                             int recordComponentOffset);
    int  readAnnotation(AnnotationVisitor* av, int annotationOffset, bool named,
                        std::vector<char>& charBuffer);
    int  readAnnotationType(AnnotationVisitor* av, int annotationOffset,
                            std::vector<char>& charBuffer);
    int  readElementValues(AnnotationVisitor* av, int elementValueOffset, bool named,
                           std::vector<char>& charBuffer);
    int readElementValue(AnnotationVisitor* av, ca::str::Utf8StringRef name, int elementValueOffset,
                         std::vector<char>& charBuffer);
    int readParameterAnnotations(MethodVisitor* mv, Context& context, int parameterAnnotationOffset,
                                 bool visible);
    int readTypeAnnotationTarget(Context& context, int annotationOffset);
    void readTypeAnnotationTarget(Context& context, int targetType, int annotationOffset);
    int  readStackMapFrame(int stackMapFrameOffset, bool compressed, bool expand, Context& context);
    int  readStackMapFrameType(MethodVisitor* mv, Context& context, int frameOffset);
    int  readVerificationTypeInfo(int verificationTypeInfoOffset, std::vector<ca::core::Any>& frame,
                                  int index, std::vector<char>& charBuffer, Label** labels,
                                  int labelsSize);
    void computeImplicitFrame(Context& context);
    /// 隐式帧 locals 收口：计数超过缓冲（maxLocals 说谎的对抗性输入）时扩缓冲填 TOP，
    /// 保证 (currentFrameLocalCount, currentFrameLocalTypes.size()) 自洽。
    void finishImplicitFrameLocals(Context& context, int numLocal);
    std::vector<int> readTypeAnnotations(MethodVisitor* mv, Context& context,
                                         int runtimeTypeAnnotationsOffset, bool visible);
    int              getTypeAnnotationBytecodeOffset(const std::vector<int>& typeAnnotationOffsets,
                                                     int                     typeAnnotationIndex);
    Attribute* readAttribute(const Attribute* prototypes, const ca::str::Utf8StringRef& attrName,
                             int attrOffset, int attrLength, std::vector<char>& charBuffer,
                             int codeAttributeOffset, Label** labels);

    // ---- Fields ----

    const ca::u8* classFileBuffer_;
    int           classFileStartOffset_ = 0;
    int           classFileLength_      = 0;
    int           header_;
    int           constantPoolCount_;
    // 解析期 scratch 一律用 vector：构造/解析中途抛异常时由 RAII 释放
    // （此前裸 new[] 在构造未完成路径上直接泄漏）。
    std::vector<int>                    cpInfoOffsets_;
    std::vector<ca::str::Utf8StringRef> constantUtf8Values_;
    std::vector<ca::core::Any>          constantDynamicValues_;
    std::vector<int>                    bootstrapMethodOffsets_;
    int                                 maxStringLength_;
    // readElementValue 当前递归深度（含 av == nullptr 的跳过路径）。
    int                                 annotationDepth_ = 0;
    // -- arena 死亡点声明（memory-governance-plan §5-P1③）--
    // 本成员借用调用方拥有的 Utf8StringArena（非本类私有；多 reader/writer 可共享同一 arena）：
    // readUTF8/readClass/readModule/readConst/readUtf 返回的视图、getClassName/getSuperName/
    // getInterfaces 的返回值、constantUtf8Values_ 缓存项，以及 accept() 回调传给
    // visitor/tree 的全部字符串视图，均 intern 自该 arena。
    // 视图寿命 ≠ 本对象寿命：本类析构不释放字符串，视图寿命 = 被借用 arena 的寿命，
    // 该 arena 必须长于本 reader 及所有经本类拿到视图的 visitor/tree（见类头 @warning）。
    // 消费者不得把视图带过本对象的死亡点；需跨期持有则 intern 进自己的 arena 或深拷贝。
    ca::str::Utf8StringArena&           arena_;   // 借用：由调用方拥有，多 reader/writer 可共享同一 arena
    // -- B8 Label scratch arena（本类私有，死亡点 = 本对象析构）--
    // readCode/readTypeAnnotations/readStackMapFrame/readVerificationTypeInfo 创建的
    // 跳转 Label 全部由此 arena 分配并盖章 owner（owner_=&labelArena_）。
    // 下游 MethodNode visitor 边界 canonicalize 拷贝收编（树不持有 reader Label）；
    // 下游 MethodWriter 按 ASM 共享 Label 模型借用并改写（visitLabel 重定位、
    // COMPUTE_FRAMES 帧计算在 toByteArray() 时读取），借用窗口 = 本 reader 存活期，
    // writer 侧 toByteArray() 必须在 reader 死亡点之前完成。
    garena::Arena labelArena_;
};

// ============================================================================
// SignatureReader — 对标 org.objectweb.asm.signature.SignatureReader
// 原全部内联实现，已移至 signature_reader.cpp。
// ============================================================================

/// SignatureReader: parse a JVM generic signature string into visitor events.
/// The signature string is stored internally and must outlive accept().
class SignatureReader
{
public:
    explicit SignatureReader(ca::str::Utf8StringRef signature)
        : signatureValue(signature)
    {}

    void accept(SignatureVisitor* v);
    void acceptType(SignatureVisitor* v);

private:
    ca::str::Utf8StringRef signatureValue;

    static size_t parseType(ca::str::Utf8StringRef sig, size_t startOffset, SignatureVisitor* v);
};

}   // namespace jbytecode::core
