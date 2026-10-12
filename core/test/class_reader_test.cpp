#include <gtest/gtest.h>
#include <cstring>
#include <memory>
#include <sstream>
#include <tuple>
#include "jbytecode/core/constants.hpp"
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

namespace {

// 全测试套共享的 arena：gtest 用例串行执行，intern 只增内存不破坏正确性，
// 故共享一个 arena 避免每个 TEST 各自声明（reader 必传 arena 后的便捷处理）。
ca::str::Utf8StringArena arena;

std::vector<uint8_t> readSampleClass()
{
    auto bytes = readClassFile("DefaultPackage.class");
    return bytes;
}

std::string stringOf(ca::str::Utf8StringRef value)
{
    if (!value.data()) return {};
    return std::string(reinterpret_cast<const char*>(value.data()), value.byte_length());
}

void assertBasicAccessors(ClassReader& classReader)
{
    ASSERT_NE(0, classReader.getAccess());
    ASSERT_NE(nullptr, classReader.getClassName().data());
    ASSERT_NE(std::string::npos, stringOf(classReader.getClassName()).find("DefaultPackage"));
}

void putU1(std::vector<uint8_t>& out, uint8_t value)
{
    out.push_back(value);
}

void putU2(std::vector<uint8_t>& out, uint16_t value)
{
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>(value & 0xff));
}

void putU4(std::vector<uint8_t>& out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>(value & 0xff));
}

void putU8(std::vector<uint8_t>& out, uint64_t value)
{
    putU4(out, static_cast<uint32_t>((value >> 32) & 0xffffffff));
    putU4(out, static_cast<uint32_t>(value & 0xffffffff));
}

void putUtf8(std::vector<uint8_t>& out, const char* value)
{
    putU1(out, 1);
    auto length = static_cast<uint16_t>(std::strlen(value));
    putU2(out, length);
    for (uint16_t i = 0; i < length; ++i) {
        putU1(out, static_cast<uint8_t>(value[i]));
    }
}

void putUtf8(std::vector<uint8_t>& out, const std::vector<uint8_t>& value)
{
    putU1(out, 1);
    putU2(out, static_cast<uint16_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

std::vector<uint8_t> classWithUnpairedSurrogateLdc()
{
    std::vector<uint8_t> out;
    putU4(out, 0xCAFEBABE);
    putU2(out, 0);
    putU2(out, 52);
    putU2(out, 10);   /// constant_pool_count
    putUtf8(out, "Mutf8Ldc");
    putU1(out, 7);
    putU2(out, 1);   /// Class #1
    putUtf8(out, "java/lang/Object");
    putU1(out, 7);
    putU2(out, 3);   /// Class #3
    putUtf8(out, "m");
    putUtf8(out, "()V");
    putUtf8(out, "Code");
    putUtf8(out, {0xED, 0xA7, 0x85});   /// Unpaired high surrogate U+D9C5.
    putU1(out, 8);
    putU2(out, 8);   /// String #8

    putU2(out, Opcodes::ACC_PUBLIC);
    putU2(out, 2);   /// this_class
    putU2(out, 4);   /// super_class
    putU2(out, 0);   /// interfaces_count
    putU2(out, 0);   /// fields_count
    putU2(out, 1);   /// methods_count
    putU2(out, Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC);
    putU2(out, 5);   /// name_index
    putU2(out, 6);   /// descriptor_index
    putU2(out, 1);   /// attributes_count
    putU2(out, 7);   /// Code
    putU4(out, 15);  /// attribute_length
    putU2(out, 1);   /// max_stack
    putU2(out, 0);   /// max_locals
    putU4(out, 3);   /// code_length
    putU1(out, Opcodes::LDC);
    putU1(out, 9);   /// String #9
    putU1(out, Opcodes::RETURN);
    putU2(out, 0);   /// exception_table_length
    putU2(out, 0);   /// code attributes_count
    putU2(out, 0);   /// class attributes_count
    return out;
}

std::vector<uint8_t> annotatedClassWithIntAnnotationIndex(uint16_t constValueIndex)
{
    std::vector<uint8_t> out;
    putU4(out, 0xCAFEBABE);
    putU2(out, 0);
    putU2(out, 52);
    putU2(out, 9);                           // constant_pool_count
    putUtf8(out, "P0MalformedAnnotation");   // #1
    putU1(out, 7);
    putU2(out, 1);                      // #2 Class #1
    putUtf8(out, "java/lang/Object");   // #3
    putU1(out, 7);
    putU2(out, 3);                               // #4 Class #3
    putUtf8(out, "RuntimeVisibleAnnotations");   // #5
    putUtf8(out, "LA;");                         // #6
    putUtf8(out, "value");                       // #7
    putU1(out, 3);
    putU4(out, 1);   // #8 Integer 1

    putU2(out, Opcodes::ACC_PUBLIC);
    putU2(out, 2);    // this_class
    putU2(out, 4);    // super_class
    putU2(out, 0);    // interfaces_count
    putU2(out, 0);    // fields_count
    putU2(out, 0);    // methods_count
    putU2(out, 1);    // attributes_count
    putU2(out, 5);    // RuntimeVisibleAnnotations
    putU4(out, 11);   // attribute_length
    putU2(out, 1);    // num_annotations
    putU2(out, 6);    // annotation type_index
    putU2(out, 1);    // num_element_value_pairs
    putU2(out, 7);    // element_name_index
    putU1(out, static_cast<uint8_t>('I'));
    putU2(out, constValueIndex);
    return out;
}

// 从 issue #456 的 a.class 提炼：嵌套注解中的 const_value_index 可指向常量池范围外。
// 只保留触发 ClassReader 读取路径的 JVM 结构，不引入外部 CrackMe 样本的任何代码或数据。
std::vector<uint8_t> classWithNestedAnnotationInvalidConstantPoolIndex()
{
    std::vector<uint8_t> out;
    putU4(out, 0xCAFEBABE);
    putU2(out, 0);
    putU2(out, 52);
    putU2(out, 10);                              // constant_pool_count
    putUtf8(out, "P0NestedInvalidAnnotation");   // #1
    putU1(out, 7);
    putU2(out, 1);                      // #2 Class #1
    putUtf8(out, "java/lang/Object");   // #3
    putU1(out, 7);
    putU2(out, 3);                               // #4 Class #3
    putUtf8(out, "RuntimeVisibleAnnotations");   // #5
    putUtf8(out, "LRoot;");                      // #6
    putUtf8(out, "nested");                      // #7
    putUtf8(out, "LNested;");                    // #8
    putUtf8(out, "value");                       // #9

    putU2(out, Opcodes::ACC_PUBLIC);
    putU2(out, 2);    // this_class
    putU2(out, 4);    // super_class
    putU2(out, 0);    // interfaces_count
    putU2(out, 0);    // fields_count
    putU2(out, 0);    // methods_count
    putU2(out, 1);    // attributes_count
    putU2(out, 5);    // RuntimeVisibleAnnotations
    putU4(out, 18);   // attribute_length
    putU2(out, 1);    // num_annotations
    putU2(out, 6);    // root type_index
    putU2(out, 1);    // root num_element_value_pairs
    putU2(out, 7);    // root element_name_index
    putU1(out, static_cast<uint8_t>('@'));
    putU2(out, 8);   // nested type_index
    putU2(out, 1);   // nested num_element_value_pairs
    putU2(out, 9);   // nested element_name_index
    putU1(out, static_cast<uint8_t>('I'));
    putU2(out, 0x7300);   // a.class 中触发访问冲突的越界常量池索引 29440
    return out;
}

std::vector<uint8_t> annotatedClassWithLongSecondSlotAnnotationIndex()
{
    std::vector<uint8_t> out;
    putU4(out, 0xCAFEBABE);
    putU2(out, 0);
    putU2(out, 52);
    putU2(out, 10);                               // constant_pool_count
    putUtf8(out, "P0LongSecondSlotAnnotation");   // #1
    putU1(out, 7);
    putU2(out, 1);                      // #2 Class #1
    putUtf8(out, "java/lang/Object");   // #3
    putU1(out, 7);
    putU2(out, 3);                               // #4 Class #3
    putUtf8(out, "RuntimeVisibleAnnotations");   // #5
    putUtf8(out, "LA;");                         // #6
    putUtf8(out, "value");                       // #7
    putU1(out, 5);
    putU8(out, 0x0102030405060708ull);   // #8 Long, #9 second slot

    putU2(out, Opcodes::ACC_PUBLIC);
    putU2(out, 2);    // this_class
    putU2(out, 4);    // super_class
    putU2(out, 0);    // interfaces_count
    putU2(out, 0);    // fields_count
    putU2(out, 0);    // methods_count
    putU2(out, 1);    // attributes_count
    putU2(out, 5);    // RuntimeVisibleAnnotations
    putU4(out, 11);   // attribute_length
    putU2(out, 1);    // num_annotations
    putU2(out, 6);    // annotation type_index
    putU2(out, 1);    // num_element_value_pairs
    putU2(out, 7);    // element_name_index
    putU1(out, static_cast<uint8_t>('J'));
    putU2(out, 9);   // Long second slot
    return out;
}

std::vector<uint8_t> annotatedClassWithDoubleSecondSlotAnnotationIndex()
{
    std::vector<uint8_t> out;
    putU4(out, 0xCAFEBABE);
    putU2(out, 0);
    putU2(out, 52);
    putU2(out, 10);                                 // constant_pool_count
    putUtf8(out, "P0DoubleSecondSlotAnnotation");   // #1
    putU1(out, 7);
    putU2(out, 1);                      // #2 Class #1
    putUtf8(out, "java/lang/Object");   // #3
    putU1(out, 7);
    putU2(out, 3);                               // #4 Class #3
    putUtf8(out, "RuntimeVisibleAnnotations");   // #5
    putUtf8(out, "LA;");                         // #6
    putUtf8(out, "value");                       // #7
    putU1(out, 6);
    putU8(out, 0x400c000000000000ull);   // #8 Double 3.5, #9 second slot

    putU2(out, Opcodes::ACC_PUBLIC);
    putU2(out, 2);    // this_class
    putU2(out, 4);    // super_class
    putU2(out, 0);    // interfaces_count
    putU2(out, 0);    // fields_count
    putU2(out, 0);    // methods_count
    putU2(out, 1);    // attributes_count
    putU2(out, 5);    // RuntimeVisibleAnnotations
    putU4(out, 11);   // attribute_length
    putU2(out, 1);    // num_annotations
    putU2(out, 6);    // annotation type_index
    putU2(out, 1);    // num_element_value_pairs
    putU2(out, 7);    // element_name_index
    putU1(out, static_cast<uint8_t>('D'));
    putU2(out, 9);   // Double second slot
    return out;
}

std::vector<uint8_t> classWithThisClassNameIndex(uint16_t nameIndex)
{
    std::vector<uint8_t> out;
    putU4(out, 0xCAFEBABE);
    putU2(out, 0);
    putU2(out, 52);
    putU2(out, 5);                    // constant_pool_count
    putUtf8(out, "P0InvalidClass");   // #1
    putU1(out, 7);
    putU2(out, nameIndex);              // #2 Class name_index
    putUtf8(out, "java/lang/Object");   // #3
    putU1(out, 7);
    putU2(out, 3);   // #4 Class #3

    putU2(out, Opcodes::ACC_PUBLIC);
    putU2(out, 2);   // this_class
    putU2(out, 4);   // super_class
    putU2(out, 0);   // interfaces_count
    putU2(out, 0);   // fields_count
    putU2(out, 0);   // methods_count
    putU2(out, 0);   // attributes_count
    return out;
}

std::vector<uint8_t> classWithTruncatedHugeUtf8Constant()
{
    std::vector<uint8_t> out;
    putU4(out, 0xCAFEBABE);
    putU2(out, 0);
    putU2(out, 52);
    putU2(out, 2);        // constant_pool_count
    putU1(out, 1);        // CONSTANT_Utf8
    putU2(out, 0xffff);   // declared length exceeds remaining class bytes
    return out;
}

std::vector<uint8_t> classWithOutOfRangeExceptionHandler()
{
    std::vector<uint8_t> out;
    putU4(out, 0xCAFEBABE);
    putU2(out, 0);
    putU2(out, 52);
    putU2(out, 8);                    // constant_pool_count
    putUtf8(out, "InvalidHandler");   // #1
    putU1(out, 7);
    putU2(out, 1);                      // #2 Class #1
    putUtf8(out, "java/lang/Object");   // #3
    putU1(out, 7);
    putU2(out, 3);            // #4 Class #3
    putUtf8(out, "method");   // #5
    putUtf8(out, "()V");      // #6
    putUtf8(out, "Code");     // #7

    putU2(out, Opcodes::ACC_PUBLIC);
    putU2(out, 2);   // this_class
    putU2(out, 4);   // super_class
    putU2(out, 0);   // interfaces_count
    putU2(out, 0);   // fields_count
    putU2(out, 1);   // methods_count
    putU2(out, Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC);
    putU2(out, 5);    // name_index
    putU2(out, 6);    // descriptor_index
    putU2(out, 1);    // attributes_count
    putU2(out, 7);    // Code
    putU4(out, 21);   // attribute_length
    putU2(out, 0);    // max_stack
    putU2(out, 0);    // max_locals
    putU4(out, 1);    // code_length
    putU1(out, Opcodes::RETURN);
    putU2(out, 1);   // exception_table_length
    putU2(out, 0);   // start_pc
    putU2(out, 1);   // end_pc
    putU2(out, 2);   // handler_pc, outside code array
    putU2(out, 0);   // catch_type
    putU2(out, 0);   // Code attributes_count
    putU2(out, 0);   // class attributes_count
    return out;
}

std::vector<uint8_t> classWithDeclaredCodeLength(uint32_t declaredCodeLength)
{
    std::vector<uint8_t> out;
    putU4(out, 0xCAFEBABE);
    putU2(out, 0);
    putU2(out, 52);
    putU2(out, 8);                     // constant_pool_count
    putUtf8(out, "HugeCodeLength");    // #1
    putU1(out, 7);
    putU2(out, 1);                      // #2 Class #1
    putUtf8(out, "java/lang/Object");   // #3
    putU1(out, 7);
    putU2(out, 3);            // #4 Class #3
    putUtf8(out, "method");   // #5
    putUtf8(out, "()V");      // #6
    putUtf8(out, "Code");     // #7

    putU2(out, Opcodes::ACC_PUBLIC);
    putU2(out, 2);   // this_class
    putU2(out, 4);   // super_class
    putU2(out, 0);   // interfaces_count
    putU2(out, 0);   // fields_count
    putU2(out, 1);   // methods_count
    putU2(out, Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC);
    putU2(out, 5);   // name_index
    putU2(out, 6);   // descriptor_index
    putU2(out, 1);   // attributes_count
    putU2(out, 7);   // Code
    putU4(out, 13);  // attribute_length
    putU2(out, 1);   // max_stack
    putU2(out, 1);   // max_locals
    putU4(out, declaredCodeLength);   // code_length 谎报值
    putU1(out, Opcodes::RETURN);      // 实际 code 只有 1 字节
    putU2(out, 0);   // exception_table_length
    putU2(out, 0);   // Code attributes_count
    putU2(out, 0);   // class attributes_count
    return out;
}

std::vector<uint8_t> classWithDeeplyNestedArrayAnnotation(int depth)
{
    std::vector<uint8_t> out;
    putU4(out, 0xCAFEBABE);
    putU2(out, 0);
    putU2(out, 52);
    putU2(out, 8);                     // constant_pool_count
    putUtf8(out, "DeepNestedArray");   // #1
    putU1(out, 7);
    putU2(out, 1);                      // #2 Class #1
    putUtf8(out, "java/lang/Object");   // #3
    putU1(out, 7);
    putU2(out, 3);                               // #4 Class #3
    putUtf8(out, "RuntimeVisibleAnnotations");   // #5
    putUtf8(out, "LA;");                         // #6
    putUtf8(out, "value");                       // #7

    putU2(out, Opcodes::ACC_PUBLIC);
    putU2(out, 2);    // this_class
    putU2(out, 4);    // super_class
    putU2(out, 0);    // interfaces_count
    putU2(out, 0);    // fields_count
    putU2(out, 0);    // methods_count
    putU2(out, 1);    // attributes_count
    putU2(out, 5);    // RuntimeVisibleAnnotations

    // attribute content：1 个注解、1 个元素对，随后是 depth 层 '[' 嵌套。
    std::vector<uint8_t> content;
    putU2(content, 1);   // num_annotations
    putU2(content, 6);   // type_index #6
    putU2(content, 1);   // num_element_value_pairs
    putU2(content, 7);   // element_name_index #7
    for (int i = 0; i < depth; ++i) {
        putU1(content, static_cast<uint8_t>('['));
        putU2(content, 1);   // num_values
    }
    // 最内层叶子：深度超限时读不到，仅为结构完整性占位。
    putU1(content, static_cast<uint8_t>('I'));
    putU2(content, 0);   // const_value_index 占位

    putU4(out, static_cast<uint32_t>(content.size()));
    out.insert(out.end(), content.begin(), content.end());
    putU2(out, 0);   // class attributes_count
    return out;
}

std::vector<uint8_t> classWithCode(const char* className, const std::vector<uint8_t>& code)
{
    std::vector<uint8_t> out;
    putU4(out, 0xCAFEBABE);
    putU2(out, 0);
    putU2(out, 52);
    putU2(out, 8);             // constant_pool_count
    putUtf8(out, className);   // #1
    putU1(out, 7);
    putU2(out, 1);                      // #2 Class #1
    putUtf8(out, "java/lang/Object");   // #3
    putU1(out, 7);
    putU2(out, 3);            // #4 Class #3
    putUtf8(out, "method");   // #5
    putUtf8(out, "()V");      // #6
    putUtf8(out, "Code");     // #7

    putU2(out, Opcodes::ACC_PUBLIC);
    putU2(out, 2);   // this_class
    putU2(out, 4);   // super_class
    putU2(out, 0);   // interfaces_count
    putU2(out, 0);   // fields_count
    putU2(out, 1);   // methods_count
    putU2(out, Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC);
    putU2(out, 5);   // name_index
    putU2(out, 6);   // descriptor_index
    putU2(out, 1);   // attributes_count
    putU2(out, 7);   // Code
    putU4(out, static_cast<uint32_t>(12 + code.size()));
    putU2(out, 1);     // max_stack
    putU2(out, 301);   // max_locals
    putU4(out, static_cast<uint32_t>(code.size()));
    out.insert(out.end(), code.begin(), code.end());
    putU2(out, 0);   // exception_table_length
    putU2(out, 0);   // Code attributes_count
    putU2(out, 0);   // class attributes_count
    return out;
}

struct InstructionMetadataVisitor : MethodVisitor
{
    std::vector<std::tuple<int, int, int>> records;

    InstructionMetadataVisitor()
        : MethodVisitor(Opcodes::ASM9)
    {}

    void visitInstructionMetadata(int offset, int length, int rawOpcode) override
    {
        records.emplace_back(offset, length, rawOpcode);
    }
};

struct InstructionMetadataClassVisitor : ClassVisitor
{
    InstructionMetadataVisitor methodVisitor;

    InstructionMetadataClassVisitor()
        : ClassVisitor(Opcodes::ASM9)
    {}

    MethodVisitor* visitMethod(int, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                               ca::str::Utf8StringRef,
                               const std::vector<ca::str::Utf8StringRef>&) override
    {
        return &methodVisitor;
    }
};

struct ReadingClassAnnotationVisitor : ClassVisitor
{
    AnnotationVisitor annotationVisitor;

    ReadingClassAnnotationVisitor()
        : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL)
        , annotationVisitor(Opcodes::ASM10_EXPERIMENTAL)
    {}

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef, bool) override
    {
        return &annotationVisitor;
    }
};

struct NestedAnnotationVisitor : AnnotationVisitor
{
    AnnotationVisitor nestedAnnotationVisitor;

    NestedAnnotationVisitor()
        : AnnotationVisitor(Opcodes::ASM10_EXPERIMENTAL)
        , nestedAnnotationVisitor(Opcodes::ASM10_EXPERIMENTAL)
    {}

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef, ca::str::Utf8StringRef) override
    {
        return &nestedAnnotationVisitor;
    }
};

struct NestedReadingClassAnnotationVisitor : ClassVisitor
{
    NestedAnnotationVisitor rootAnnotationVisitor;

    NestedReadingClassAnnotationVisitor()
        : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL)
        , rootAnnotationVisitor()
    {}

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef, bool) override
    {
        return &rootAnnotationVisitor;
    }
};

// visitAnnotation 返回 nullptr：reader 走 readElementValues(nullptr) 跳过路径，
// 递归仍在 reader 侧进行，深度守卫必须同样生效。
struct NullAnnotationClassVisitor : ClassVisitor
{
    NullAnnotationClassVisitor()
        : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL)
    {}

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef, bool) override { return nullptr; }
};

struct ReadingCodeVisitor : ClassVisitor
{
    MethodVisitor methodVisitor;

    ReadingCodeVisitor()
        : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL)
        , methodVisitor(Opcodes::ASM10_EXPERIMENTAL)
    {}

    MethodVisitor* visitMethod(int, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                               ca::str::Utf8StringRef,
                               const std::vector<ca::str::Utf8StringRef>&) override
    {
        return &methodVisitor;
    }
};

struct CapturingAnnotationVisitor : AnnotationVisitor
{
    ca::core::Any value;

    CapturingAnnotationVisitor()
        : AnnotationVisitor(Opcodes::ASM10_EXPERIMENTAL)
    {}

    void visit(ca::str::Utf8StringRef, const ca::core::Any& v) override { value = v; }
};

struct CapturingClassAnnotationVisitor : ClassVisitor
{
    CapturingAnnotationVisitor annotationVisitor;

    CapturingClassAnnotationVisitor()
        : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL)
    {}

    AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef, bool) override
    {
        return &annotationVisitor;
    }
};

struct FieldDescriptorCapturingMethodVisitor : MethodVisitor
{
    std::string descriptor;

    FieldDescriptorCapturingMethodVisitor()
        : MethodVisitor(Opcodes::ASM10_EXPERIMENTAL)
    {}

    void visitFieldInsn(int, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                        ca::str::Utf8StringRef value) override
    {
        descriptor = stringOf(value);
    }
};

struct FieldDescriptorCapturingClassVisitor : ClassVisitor
{
    FieldDescriptorCapturingMethodVisitor methodVisitor;

    FieldDescriptorCapturingClassVisitor()
        : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL)
    {}

    MethodVisitor* visitMethod(int, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                               ca::str::Utf8StringRef,
                               const std::vector<ca::str::Utf8StringRef>&) override
    {
        return &methodVisitor;
    }
};

}   // namespace

TEST(ClassReaderTest, testByteArrayConstructor)
{
    auto bytes = readSampleClass();
    ASSERT_FALSE(bytes.empty());

    ClassReader classReader(arena, bytes);

    assertBasicAccessors(classReader);
}

TEST(ClassReaderTest, testByteArrayConstructor_withOffset)
{
    auto classFile = readSampleClass();
    ASSERT_FALSE(classFile.empty());
    std::vector<uint8_t> byteBuffer(classFile.size() + 1);
    std::copy(classFile.begin(), classFile.end(), byteBuffer.begin() + 1);

    ClassReader classReader(arena, byteBuffer.data(), 1, static_cast<int>(classFile.size()));

    assertBasicAccessors(classReader);
}

TEST(ClassReaderTest, testByteArrayConstructor_invalidClassHeader)
{
    std::vector<uint8_t> invalidClass = {0, 0, 0, 0};

    EXPECT_THROW(ClassReader classReader(arena, invalidClass), std::invalid_argument);
}

TEST(ClassReaderTest, testReadByte)
{
    auto bytes = readSampleClass();
    ASSERT_FALSE(bytes.empty());
    ClassReader classReader(arena, bytes);

    ASSERT_EQ(bytes[0], classReader.readByte(0));
}

TEST(ClassReaderTest, testGetItem)
{
    ClassReader classReader(arena, readSampleClass());

    int item = classReader.getItem(1);

    ASSERT_GE(item, 10);
    ASSERT_LT(item, classReader.header);
}

TEST(ClassReaderTest, testStreamConstructor)
{
    auto bytes = readSampleClass();
    ASSERT_FALSE(bytes.empty());
    std::string        input(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream stream(input);

    ClassReader classReader(arena, stream);

    assertBasicAccessors(classReader);
}

TEST(ClassReaderTest, testStreamConstructor_emptyStream)
{
    std::istringstream stream;

    EXPECT_THROW(ClassReader classReader(arena, stream), std::invalid_argument);
}

TEST(ClassReaderTest, testStreamConstructor_nullStream)
{
    std::ifstream stream;

    EXPECT_THROW(ClassReader classReader(arena, stream), std::invalid_argument);
}

TEST(ClassReaderTest, testStringConstructor)
{
    auto path = test::getAsmTestResourcePath("test_resource/origin/DefaultPackage.class");

    ClassReader classReader(arena, path.string());

    assertBasicAccessors(classReader);
}

TEST(ClassReaderTest, testGetAccess)
{
    ClassReader classReader(arena, readSampleClass());

    ASSERT_NE(0, classReader.getAccess());
}

TEST(ClassReaderTest, testGetClassName)
{
    ClassReader classReader(arena, readSampleClass());

    ASSERT_NE(std::string::npos, stringOf(classReader.getClassName()).find("DefaultPackage"));
}

TEST(ClassReaderTest, testGetSuperName)
{
    ClassReader classReader(arena, readSampleClass());

    ASSERT_EQ("java/lang/Object", stringOf(classReader.getSuperName()));
}

TEST(ClassReaderTest, testGetInterfaces)
{
    ClassReader classReader(arena, readSampleClass());

    ASSERT_TRUE(classReader.getInterfaces().empty());
}

TEST(ClassReaderTest, testGetInterfaces_empty)
{
    ClassReader classReader(arena, readSampleClass());

    ASSERT_TRUE(classReader.getInterfaces().empty());
}

TEST(ClassReaderTest, testAccept_emptyVisitor)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader  classReader(arena, classFile);
        ClassVisitor classVisitor(Opcodes::ASM10_EXPERIMENTAL);

        EXPECT_NO_THROW(classReader.accept(&classVisitor, 0)) << classParameter.name;
    }
}

TEST(ClassReaderTest, testAccept_emptyVisitor_skipCode)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader  classReader(arena, classFile);
        ClassVisitor classVisitor(Opcodes::ASM10_EXPERIMENTAL);

        EXPECT_NO_THROW(classReader.accept(&classVisitor, ClassReader::SKIP_CODE))
            << classParameter.name;
    }
}

TEST(ClassReaderTest, testAccept_emptyVisitor_skipDebug)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader  classReader(arena, classFile);
        ClassVisitor classVisitor(Opcodes::ASM10_EXPERIMENTAL);

        EXPECT_NO_THROW(classReader.accept(&classVisitor, ClassReader::SKIP_DEBUG))
            << classParameter.name;
    }
}

TEST(ClassReaderTest, testAccept_emptyVisitor_skipFrames)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader  classReader(arena, classFile);
        ClassVisitor classVisitor(Opcodes::ASM10_EXPERIMENTAL);

        EXPECT_NO_THROW(classReader.accept(&classVisitor, ClassReader::SKIP_FRAMES))
            << classParameter.name;
    }
}

TEST(ClassReaderTest, testAccept_emptyVisitor_expandFrames)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader  classReader(arena, classFile);
        ClassVisitor classVisitor(Opcodes::ASM10_EXPERIMENTAL);

        EXPECT_NO_THROW(classReader.accept(&classVisitor, ClassReader::EXPAND_FRAMES))
            << classParameter.name;
    }
}

TEST(ClassReaderTest, testAccept_emptyVisitor_invalidClass)
{
    auto invalidClass = readSampleClass();
    ASSERT_FALSE(invalidClass.empty());
    ASSERT_GT(invalidClass.size(), 16u);
    invalidClass[10] = 0x7f;
    ClassVisitor classVisitor(Opcodes::ASM10_EXPERIMENTAL);

    EXPECT_THROW(
        {
            ClassReader classReader(arena, invalidClass);
            classReader.accept(&classVisitor, 0);
        },
        std::exception);
}

TEST(ClassReaderTest, reportsInstructionOffsetsLengthsAndRawOpcodes)
{
    const std::vector<uint8_t> code = {
        Opcodes::NOP,
        Opcodes::BIPUSH,
        0x7f,
        Opcodes::SIPUSH,
        0x01,
        0x2c,
        Constants::WIDE,
        Opcodes::ILOAD,
        0x01,
        0x2c,
        Constants::WIDE,
        Opcodes::IINC,
        0x01,
        0x2c,
        0x00,
        0x02,
        Constants::GOTO_W,
        0x00,
        0x00,
        0x00,
        0x05,
        Opcodes::RETURN,
    };
    ClassReader                     reader(arena, classWithCode("MetadataFixture", code));
    InstructionMetadataClassVisitor visitor;

    reader.accept(&visitor, 0);

    const std::vector<std::tuple<int, int, int>> expected = {
        {0, 1, Opcodes::NOP},
        {1, 2, Opcodes::BIPUSH},
        {3, 3, Opcodes::SIPUSH},
        {6, 4, Constants::WIDE},
        {10, 6, Constants::WIDE},
        {16, 5, Constants::GOTO_W},
        {21, 1, Opcodes::RETURN},
    };
    EXPECT_EQ(expected, visitor.methodVisitor.records);
}

TEST(ClassReaderTest, reportsAsmPseudoInstructionLengths)
{
    const std::vector<uint8_t> code = {
        Constants::ASM_IFEQ,
        0x00,
        0x03,
        Constants::ASM_GOTO_W,
        0x00,
        0x00,
        0x00,
        0x05,
        Opcodes::RETURN,
    };
    ClassReader                     reader(arena, classWithCode("AsmMetadataFixture", code));
    InstructionMetadataClassVisitor visitor;

    reader.accept(&visitor, 0);

    const std::vector<std::tuple<int, int, int>> expected = {
        {0, 3, Constants::ASM_IFEQ},
        {3, 5, Constants::ASM_GOTO_W},
        {8, 1, Opcodes::RETURN},
    };
    EXPECT_EQ(expected, visitor.methodVisitor.records);
}

TEST(ClassReaderTest, normalizesBareInternalNameFieldDescriptor)
{
    ClassWriter writer(arena, 0);
    writer.visit(Opcodes::V1_8,
                 Opcodes::ACC_PUBLIC,
                 ca::str::Utf8StringRef::from_cstr("BareFieldDescriptor"),
                 ca::str::Utf8StringRef(),
                 ca::str::Utf8StringRef::from_cstr("java/lang/Object"),
                 {});
    auto* method = writer.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                                      ca::str::Utf8StringRef::from_cstr("read"),
                                      ca::str::Utf8StringRef::from_cstr("()V"),
                                      ca::str::Utf8StringRef(),
                                      {});
    method->visitCode();
    method->visitFieldInsn(Opcodes::GETSTATIC,
                           ca::str::Utf8StringRef::from_cstr("java/lang/System"),
                           ca::str::Utf8StringRef::from_cstr("out"),
                           ca::str::Utf8StringRef::from_cstr("java/lang/System"));
    method->visitInsn(Opcodes::POP);
    method->visitInsn(Opcodes::RETURN);
    method->visitMaxs(1, 0);
    method->visitEnd();
    writer.visitEnd();

    ClassReader                         reader(arena, writer.toByteArray());
    FieldDescriptorCapturingClassVisitor visitor;
    reader.accept(&visitor, 0);

    EXPECT_EQ(visitor.methodVisitor.descriptor, "Ljava/lang/System;");
}

TEST(ClassReaderTest, acceptsValidClassAnnotationElementValue)
{
    auto                          bytes = annotatedClassWithIntAnnotationIndex(8);
    ReadingClassAnnotationVisitor classVisitor;

    ClassReader classReader(arena, bytes);

    EXPECT_NO_THROW(classReader.accept(&classVisitor, 0));
}

TEST(ClassReaderTest, rejectsLongReservedConstantPoolSlot)
{
    auto                            bytes = annotatedClassWithLongSecondSlotAnnotationIndex();
    CapturingClassAnnotationVisitor classVisitor;

    ClassReader classReader(arena, bytes);

    EXPECT_THROW(classReader.accept(&classVisitor, 0), std::exception);
}

TEST(ClassReaderTest, rejectsDoubleReservedConstantPoolSlot)
{
    auto                            bytes = annotatedClassWithDoubleSecondSlotAnnotationIndex();
    CapturingClassAnnotationVisitor classVisitor;

    ClassReader classReader(arena, bytes);

    EXPECT_THROW(classReader.accept(&classVisitor, 0), std::exception);
}

TEST(ClassReaderTest, invalidAnnotationElementConstantPoolIndexThrows)
{
    auto                          bytes = annotatedClassWithIntAnnotationIndex(999);
    ReadingClassAnnotationVisitor classVisitor;

    EXPECT_THROW(
        {
            ClassReader classReader(arena, bytes);
            classReader.accept(&classVisitor, 0);
        },
        std::exception);
}

TEST(ClassReaderTest, nestedAnnotationInvalidConstantPoolIndexThrows)
{
    auto                                bytes = classWithNestedAnnotationInvalidConstantPoolIndex();
    NestedReadingClassAnnotationVisitor classVisitor;

    EXPECT_THROW(
        {
            ClassReader classReader(arena, bytes);
            classReader.accept(&classVisitor, 0);
        },
        std::invalid_argument);
}

TEST(ClassReaderTest, invalidClassNameConstantPoolIndexThrows)
{
    auto bytes = classWithThisClassNameIndex(999);

    ClassReader classReader(arena, bytes);

    EXPECT_THROW(classReader.getClassName(), std::invalid_argument);
}

TEST(ClassReaderTest, truncatedHugeUtf8ConstantThrowsBeforeAccept)
{
    auto bytes = classWithTruncatedHugeUtf8Constant();

    EXPECT_THROW(ClassReader classReader(arena, bytes), std::invalid_argument);
}

TEST(ClassReaderTest, rejectsOutOfRangeExceptionHandlerOffset)
{
    ClassReader        classReader(arena, classWithOutOfRangeExceptionHandler());
    ReadingCodeVisitor visitor;
    EXPECT_THROW(classReader.accept(&visitor, 0), std::invalid_argument);
}

// 敌意 class 谎报 code_length（0x7FFFFFFF）：声明值远超 code 区段实际字节，
// 读入必须在指令循环扫描前被 checkRange 拒绝，而不是越界读堆内存。
TEST(ClassReaderTest, codeLengthBeyondActualBytesThrows)
{
    ClassReader        classReader(arena, classWithDeclaredCodeLength(0x7FFFFFFF));
    ReadingCodeVisitor visitor;

    EXPECT_THROW(classReader.accept(&visitor, 0), std::invalid_argument);
}

// code_length 为负（0xFFFFFFFF = -1）：旧实现会让 be 回绕、labels 预分配
// 回绕，必须入口处直接拒绝。
TEST(ClassReaderTest, negativeCodeLengthThrows)
{
    ClassReader        classReader(arena, classWithDeclaredCodeLength(0xFFFFFFFF));
    ReadingCodeVisitor visitor;

    EXPECT_THROW(classReader.accept(&visitor, 0), std::invalid_argument);
}

// 65535 层 '[' 嵌套注解：readElementValue '@'/'[' 互递归必须受深度上限约束，
// 抛 std::invalid_argument 而不是栈溢出。
TEST(ClassReaderTest, deeplyNestedArrayAnnotationThrows)
{
    auto bytes = classWithDeeplyNestedArrayAnnotation(65535);

    EXPECT_THROW(
        {
            ClassReader                        classReader(arena, bytes);
            NestedReadingClassAnnotationVisitor classVisitor;
            classReader.accept(&classVisitor, 0);
        },
        std::invalid_argument);
}

// 深度守卫同样覆盖 av == nullptr 的跳过路径。
TEST(ClassReaderTest, deeplyNestedArrayAnnotationThrowsOnSkipPath)
{
    auto bytes = classWithDeeplyNestedArrayAnnotation(65535);

    EXPECT_THROW(
        {
            ClassReader                classReader(arena, bytes);
            NullAnnotationClassVisitor classVisitor;
            classReader.accept(&classVisitor, 0);
        },
        std::invalid_argument);
}

TEST(ClassReaderTest, testAccept_defaultVisitor)
{
    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader  classReader(arena, classFile);
        ClassVisitor classVisitor(Opcodes::ASM10_EXPERIMENTAL);

        EXPECT_NO_THROW(classReader.accept(&classVisitor, 0)) << classParameter.name;
    }
}

TEST(ClassReaderTest, testAccept_defaultAnnotationFieldMethodAndModuleVisitors)
{
    struct DefaultContentVisitor : ClassVisitor
    {
        AnnotationVisitor      annotationVisitor;
        FieldVisitor           fieldVisitor;
        MethodVisitor          methodVisitor;
        ModuleVisitor          moduleVisitor;
        RecordComponentVisitor recordComponentVisitor;

        DefaultContentVisitor()
            : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL)
            , annotationVisitor(Opcodes::ASM10_EXPERIMENTAL)
            , fieldVisitor(Opcodes::ASM10_EXPERIMENTAL)
            , methodVisitor(Opcodes::ASM10_EXPERIMENTAL)
            , moduleVisitor(Opcodes::ASM10_EXPERIMENTAL)
            , recordComponentVisitor(Opcodes::ASM10_EXPERIMENTAL)
        {}

        AnnotationVisitor* visitAnnotation(ca::str::Utf8StringRef, bool) override
        {
            return &annotationVisitor;
        }

        AnnotationVisitor* visitTypeAnnotation(int, const TypePath*, ca::str::Utf8StringRef,
                                               bool) override
        {
            return &annotationVisitor;
        }

        FieldVisitor* visitField(int, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                                 ca::str::Utf8StringRef, const ca::core::Any&) override
        {
            return &fieldVisitor;
        }

        MethodVisitor* visitMethod(int, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                                   ca::str::Utf8StringRef,
                                   const std::vector<ca::str::Utf8StringRef>&) override
        {
            return &methodVisitor;
        }

        ModuleVisitor* visitModule(ca::str::Utf8StringRef, int, ca::str::Utf8StringRef) override
        {
            return &moduleVisitor;
        }

        RecordComponentVisitor* visitRecordComponent(ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                                                     ca::str::Utf8StringRef) override
        {
            return &recordComponentVisitor;
        }
    };

    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader           classReader(arena, classFile);
        DefaultContentVisitor classVisitor;

        EXPECT_NO_THROW(classReader.accept(&classVisitor, 0)) << classParameter.name;
    }
}

TEST(ClassReaderTest, testAccept_emptyVisitor_skipFieldMethodAndModuleContent)
{
    struct SkipContentVisitor : ClassVisitor
    {
        SkipContentVisitor()
            : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL)
        {}

        ModuleVisitor* visitModule(ca::str::Utf8StringRef, int, ca::str::Utf8StringRef) override
        {
            return nullptr;
        }

        RecordComponentVisitor* visitRecordComponent(ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                                                     ca::str::Utf8StringRef) override
        {
            return nullptr;
        }

        FieldVisitor* visitField(int, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                                 ca::str::Utf8StringRef, const ca::core::Any&) override
        {
            return nullptr;
        }

        MethodVisitor* visitMethod(int, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                                   ca::str::Utf8StringRef,
                                   const std::vector<ca::str::Utf8StringRef>&) override
        {
            return nullptr;
        }
    };

    for (const auto& classParameter : availablePrecompiledClasses()) {
        auto classFile = classParameter.bytes();
        ASSERT_FALSE(classFile.empty()) << classParameter.resource;
        ClassReader        classReader(arena, classFile);
        SkipContentVisitor classVisitor;

        EXPECT_NO_THROW(classReader.accept(&classVisitor, 0)) << classParameter.name;
    }
}

TEST(ClassReaderTest, testAccept_parameterAnnotationIndices)
{
    auto classFile = readClassFile("jdk5/AllStructures$1LocalClass.class");
    ASSERT_FALSE(classFile.empty());
    ClassReader classReader(arena, classFile);
    struct ReadParameterIndexVisitor : ClassVisitor
    {
        struct ReadDeprecatedParameterAnnotationVisitor : MethodVisitor
        {
            int& parameterIndex;

            explicit ReadDeprecatedParameterAnnotationVisitor(int& parameterIndex)
                : MethodVisitor(Opcodes::ASM10_EXPERIMENTAL)
                , parameterIndex(parameterIndex)
            {}

            AnnotationVisitor* visitParameterAnnotation(int                    parameter,
                                                        ca::str::Utf8StringRef descriptor,
                                                        bool) override
            {
                if (stringOf(descriptor) == "Ljava/lang/Deprecated;") {
                    parameterIndex = parameter;
                }
                return nullptr;
            }
        };

        int                                         parameterIndex = -1;
        std::vector<std::unique_ptr<MethodVisitor>> methodVisitors;

        ReadParameterIndexVisitor()
            : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL)
        {}

        MethodVisitor* visitMethod(int, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                                   ca::str::Utf8StringRef,
                                   const std::vector<ca::str::Utf8StringRef>&) override
        {
            methodVisitors.push_back(
                std::make_unique<ReadDeprecatedParameterAnnotationVisitor>(parameterIndex));
            return methodVisitors.back().get();
        }
    };
    ReadParameterIndexVisitor readParameterIndexVisitor;

    classReader.accept(&readParameterIndexVisitor, 0);

    ASSERT_EQ(0, readParameterIndexVisitor.parameterIndex);
}

TEST(ClassReaderTest, testAccept_invalidCustomAttribute)
{
    std::vector<uint8_t> input = {33, 251, 93, 4, 0, 0, 0, 0, 0, 2, 1, 0, 0,   0,   255, 0,  0,
                                  0,  0,   0,  0, 0, 0, 0, 0, 2, 1, 0, 1, 127, 255, 255, 255};
    ClassVisitor         noOpVisitor(Opcodes::ASM9);

    EXPECT_THROW(
        {
            ClassReader reader(arena, input);
            reader.accept(&noOpVisitor, ClassReader::EXPAND_FRAMES);
        },
        std::invalid_argument);
}

TEST(ClassReaderTest, testAccept_previewClass)
{
    auto classFile = readSampleClass();
    ASSERT_FALSE(classFile.empty());
    classFile[4] = 0xFF;
    classFile[5] = 0xFF;
    ClassReader classReader(arena, classFile);
    struct ReadVersionVisitor : ClassVisitor
    {
        int classVersion = 0;

        ReadVersionVisitor()
            : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL)
        {}

        void visit(int version, int, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                   ca::str::Utf8StringRef, const std::vector<ca::str::Utf8StringRef>&) override
        {
            classVersion = version;
        }
    };
    ReadVersionVisitor readVersionVisitor;

    classReader.accept(&readVersionVisitor, 0);

    ASSERT_EQ(Opcodes::V_PREVIEW, readVersionVisitor.classVersion & Opcodes::V_PREVIEW);
}

TEST(ClassReaderTest, preservesUnpairedSurrogateLdcAcrossRoundTrip)
{
    struct LdcCaptureVisitor : ClassVisitor
    {
        struct LdcCaptureMethodVisitor : MethodVisitor
        {
            ca::core::Any& value;

            explicit LdcCaptureMethodVisitor(ca::core::Any& value)
                : MethodVisitor(Opcodes::ASM10_EXPERIMENTAL)
                , value(value)
            {}

            void visitLdcInsn(const ca::core::Any& candidate) override { value = candidate; }
        };

        ca::core::Any           value;
        LdcCaptureMethodVisitor methodVisitor;

        LdcCaptureVisitor()
            : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL)
            , methodVisitor(value)
        {}

        MethodVisitor* visitMethod(int, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                                   ca::str::Utf8StringRef,
                                   const std::vector<ca::str::Utf8StringRef>&) override
        {
            return &methodVisitor;
        }
    };

    const std::string expected("\xED\xA7\x85", 3);
    ClassReader       reader(arena, classWithUnpairedSurrogateLdc());
    LdcCaptureVisitor before;
    reader.accept(&before, 0);

    const auto* beforeValue = before.value.as<ca::str::Utf8StringRef>();
    ASSERT_NE(beforeValue, nullptr);
    EXPECT_EQ(stringOf(*beforeValue), expected);

    ClassWriter writer(arena, 0);
    reader.accept(&writer, 0);
    const auto output = writer.toByteArray();

    ClassReader       roundTripReader(arena, output);
    LdcCaptureVisitor after;
    roundTripReader.accept(&after, 0);
    const auto* afterValue = after.value.as<ca::str::Utf8StringRef>();
    ASSERT_NE(afterValue, nullptr);
    EXPECT_EQ(stringOf(*afterValue), expected);
}

// 回归测试（main linux CI 上游单测 段错误根因）：readCode 曾以
// `readConst(...).as<Handle>()` 读取 indy 的 bootstrap method handle——readConst
// 返回的临时 Any 在语句末析构即 delete 内部 Handle，下一行 `*bsmHandlePtr` 是
// use-after-free。glibc（≥2.34）free 会覆写已释放块前 16 字节
// {protected next, 随机 tcache_key}，恰好砸掉 Handle 的 tag_ + owner_.data_：
// 下游解引用 owner 视图即 SIGSEGV（Windows MSVC free 不清零块内容，长期全绿
// 掩盖了该 UB）。本测试走 writer → reader 全链路并断言 bsm Handle 内容，
// Linux 上预修复形态必触发（断言解引用悬垂指针 / 内容不匹配）。
TEST(ClassReaderTest, parsesInvokeDynamicBootstrapHandleContentsIntact)
{
    const std::string kConcatFactory = "java/lang/invoke/StringConcatFactory";
    const std::string kBsmDesc =
        "(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;)"
        "Ljava/lang/invoke/StringConcatFactory;";
    const std::string kIndyName = "makeConcatWithConstants";
    const std::string kIndyDesc = "(ILjava/lang/String;)Ljava/lang/String;";

    struct IndyCaptureMethodVisitor : MethodVisitor
    {
        bool        visited = false;
        std::string indyName, indyDesc;
        Handle      bsm{Opcodes::H_INVOKESTATIC,
                ca::str::Utf8StringRef(),
                ca::str::Utf8StringRef(),
                ca::str::Utf8StringRef(),
                false};
        size_t      bsmArgCount = 0;

        IndyCaptureMethodVisitor()
            : MethodVisitor(Opcodes::ASM10_EXPERIMENTAL)
        {}

        void visitInvokeDynamicInsn(ca::str::Utf8StringRef            name,
                                    ca::str::Utf8StringRef            descriptor,
                                    const Handle&                     bootstrapMethod,
                                    const std::vector<ca::core::Any>& bootstrapMethodArguments) override
        {
            visited     = true;
            indyName    = stringOf(name);
            indyDesc    = stringOf(descriptor);
            bsm         = bootstrapMethod;
            bsmArgCount = bootstrapMethodArguments.size();
        }
    };

    struct IndyCaptureClassVisitor : ClassVisitor
    {
        IndyCaptureMethodVisitor methodVisitor;

        IndyCaptureClassVisitor()
            : ClassVisitor(Opcodes::ASM10_EXPERIMENTAL)
        {}

        MethodVisitor* visitMethod(int, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                                   ca::str::Utf8StringRef,
                                   const std::vector<ca::str::Utf8StringRef>&) override
        {
            return &methodVisitor;
        }
    };

    ClassWriter writer(arena, 0);
    writer.visit(Opcodes::V11,
                 Opcodes::ACC_PUBLIC | Opcodes::ACC_SUPER,
                 ca::str::Utf8StringRef::from_cstr("IndyUafRegressionTarget"),
                 ca::str::Utf8StringRef(),
                 ca::str::Utf8StringRef::from_cstr("java/lang/Object"),
                 {});
    auto* method =
        writer.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                           ca::str::Utf8StringRef::from_cstr("concat"),
                           ca::str::Utf8StringRef::from_cstr(kIndyDesc.c_str()),
                           ca::str::Utf8StringRef(),
                           {});
    const Handle bsm(Opcodes::H_INVOKESTATIC,
                     ca::str::Utf8StringRef::from_cstr(kConcatFactory.c_str()),
                     ca::str::Utf8StringRef::from_cstr(kIndyName.c_str()),
                     ca::str::Utf8StringRef::from_cstr(kBsmDesc.c_str()),
                     false);
    method->visitCode();
    method->visitInvokeDynamicInsn(ca::str::Utf8StringRef::from_cstr(kIndyName.c_str()),
                                   ca::str::Utf8StringRef::from_cstr(kIndyDesc.c_str()),
                                   bsm,
                                   {ca::core::Any(ca::str::Utf8StringRef::from_cstr("\x01"))});
    method->visitInsn(Opcodes::ARETURN);
    method->visitMaxs(3, 0);
    method->visitEnd();
    writer.visitEnd();

    ClassReader             reader(arena, writer.toByteArray());
    IndyCaptureClassVisitor visitor;
    reader.accept(&visitor, 0);

    ASSERT_TRUE(visitor.methodVisitor.visited);
    EXPECT_EQ(kIndyName, visitor.methodVisitor.indyName);
    EXPECT_EQ(kIndyDesc, visitor.methodVisitor.indyDesc);
    EXPECT_EQ(1u, visitor.methodVisitor.bsmArgCount);
    // bsm Handle 内容逐项断言：预修复形态下这些字段来自已释放内存
    // （glibc tcache_key 砸在 owner_.data_ 上），断言即 SEGV / 不匹配。
    EXPECT_EQ(Opcodes::H_INVOKESTATIC, visitor.methodVisitor.bsm.getTag());
    EXPECT_EQ(kConcatFactory, stringOf(visitor.methodVisitor.bsm.getOwner()));
    EXPECT_EQ(kIndyName, stringOf(visitor.methodVisitor.bsm.getName()));
    EXPECT_EQ(kBsmDesc, stringOf(visitor.methodVisitor.bsm.getDesc()));
    EXPECT_FALSE(visitor.methodVisitor.bsm.isInterface());
}
