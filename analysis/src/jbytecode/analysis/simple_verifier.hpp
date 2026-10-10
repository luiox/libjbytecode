#pragma once

#include <vector>
#include <string>
#include <list>
#include <algorithm>
#include <stdexcept>

#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/analysis/basic_verifier.hpp"
#include "jbytecode/analysis/exceptions.hpp"

namespace jbytecode::analysis {

class SimpleVerifier : public BasicVerifier
{
public:
    static const jbytecode::core::Type OBJECT_TYPE;

    SimpleVerifier();

    SimpleVerifier(const jbytecode::core::Type& currentClass,
                   const jbytecode::core::Type& currentSuperClass, bool isInterface);

    SimpleVerifier(const jbytecode::core::Type&              currentClass,
                   const jbytecode::core::Type&              currentSuperClass,
                   const std::vector<jbytecode::core::Type>& currentClassInterfaces,
                   bool                                      isInterface);

    BasicValue newValue(const jbytecode::core::Type* type) override;

protected:
    SimpleVerifier(int api, const jbytecode::core::Type& currentClass,
                   const jbytecode::core::Type&              currentSuperClass,
                   const std::vector<jbytecode::core::Type>& currentClassInterfaces,
                   bool                                      isInterface);

    bool isArrayValue(const BasicValue& value);

    BasicValue getElementValue(const BasicValue& objectArrayValue);

    bool isSubTypeOf(const BasicValue& value, const BasicValue& expected);

    BasicValue merge(const BasicValue& value1, const BasicValue& value2) override;

    bool isInterface(const jbytecode::core::Type& type);

    jbytecode::core::Type getSuperClass(const jbytecode::core::Type& type);

    bool isAssignableFrom(const jbytecode::core::Type& type1, const jbytecode::core::Type& type2);

private:
    jbytecode::core::Type              currentClass_;
    jbytecode::core::Type              currentSuperClass_;
    std::vector<jbytecode::core::Type> currentClassInterfaces_;
    bool                               isInterface_;

    mutable std::list<jbytecode::core::Type> typeCache_;
    // -- arena 死亡点声明（memory-governance-plan §5-P1③）--
    // 本成员持有 SimpleVerifier 私有的 Utf8StringArena：仅承载验证期合成出的数组描述符
    // （newValue / newArrayValue 里给元素类型拼 "[" 前缀后经 Type::fromArena intern）。
    // 这些 Type 副本存入 typeCache_ 并随 BasicValue 经 newValue / merge / getElementValue
    // 等 API 出境，其 valueRef()/getDescriptor() 返回的 Utf8StringRef 即指向本 arena。
    // 视图寿命 = 本对象寿命；死亡点：SimpleVerifier 对象析构（成员逆序析构）时释放全部
    // chunk；typeCache_ 与分析帧（Analyzer 持有的 BasicValue）不得活过本对象。
    // 消费者不得把视图带过本对象的死亡点；需跨期持有则 intern 进自己的 arena 或深拷贝。
    ca::str::Utf8StringArena                 descriptorArena_;

    const jbytecode::core::Type* internType(const jbytecode::core::Type& type) const;

    bool isCurrentClassEmpty() const;

    BasicValue newArrayValue(const jbytecode::core::Type& type, int dimensions);
};

}   // namespace jbytecode::analysis
