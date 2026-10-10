#pragma once

#include <string>
#include <vector>
#include "libca/core/any.hpp"
#include <unordered_map>
#include <memory>

#include "jbytecode/core/type.hpp"
#include "jbytecode/core/primitives.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class Remapper
{
public:
    virtual ~Remapper() = default;

    /// 注入构造(必选,#921 staticArena 退役):map/mapDesc/mapMethodDesc/mapSignature/
    /// map 内部合成出的描述符与名称 intern 进调用方作业级 arena(字符串线示范,
    /// 总方案 §5.1,#699)。旧默认构造(回落进程级 staticArena)已删除。
    explicit Remapper(ca::str::Utf8StringArena& arena)
        : arena_(&arena)
    {}

    /// @brief Map a JVM internal class name.
    /// @return A stable reference. Implementations returning owned storage must retain it for the
    ///         duration of the remap traversal.
    virtual ca::str::Utf8StringRef map(ca::str::Utf8StringRef internalName) { return internalName; }

    /// @brief Map one field name using its original owner and descriptor.
    virtual ca::str::Utf8StringRef mapFieldName(ca::str::Utf8StringRef owner,
                                                 ca::str::Utf8StringRef name,
                                                 ca::str::Utf8StringRef descriptor);

    /// @brief Map one method name using its original owner and descriptor.
    virtual ca::str::Utf8StringRef mapMethodName(ca::str::Utf8StringRef owner,
                                                  ca::str::Utf8StringRef name,
                                                  ca::str::Utf8StringRef descriptor);

    /// @brief Map an invokedynamic or ConstantDynamic call-site name.
    virtual ca::str::Utf8StringRef mapInvokeDynamicMethodName(
        ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor);

    /// @brief Map a record component name using the original record owner and descriptor.
    virtual ca::str::Utf8StringRef mapRecordComponentName(ca::str::Utf8StringRef owner,
                                                           ca::str::Utf8StringRef name,
                                                           ca::str::Utf8StringRef descriptor);

    ca::str::Utf8StringRef mapDesc(ca::str::Utf8StringRef descriptor);
    ca::str::Utf8StringRef mapType(ca::str::Utf8StringRef internalName);
    ca::str::Utf8StringRef mapMethodDesc(ca::str::Utf8StringRef methodDescriptor);

    std::vector<ca::str::Utf8StringRef> mapTypes(
        const std::vector<ca::str::Utf8StringRef>& internalNames);

    ca::str::Utf8StringRef mapSignature(ca::str::Utf8StringRef signature);
    ca::core::Any          mapValue(const ca::core::Any& value);

    ca::str::Utf8StringRef mapAnnotationAttributeName(ca::str::Utf8StringRef descriptor,
                                                      ca::str::Utf8StringRef name);

protected:
    jbytecode::core::Type mapType(const jbytecode::core::Type& type);

    /// 注入的作业级字符串池(#921 起必选):map 族内部 intern 的唯一落点。
    ca::str::Utf8StringArena* arena_;

    /// intern 目标池:恒为注入的作业级 arena(#921 staticArena 退役后无兜底路径)。
    ca::str::Utf8StringArena& remapArena() const
    {
        return *arena_;
    }
};

class SimpleRemapper : public Remapper
{
public:
    /// 注入构造(字符串线示范,总方案 §5.1):intern 落到调用方作业级 arena。
    /// 旧无 arena 兜底构造已随 #921 staticArena 退役删除。
    SimpleRemapper(ca::str::Utf8StringArena& arena, ca::str::Utf8StringRef oldName,
                   ca::str::Utf8StringRef newName)
        : Remapper(arena)
    {
        mapping_[oldName.to_std_string()] = newName.to_std_string();
    }

    /// 注入构造:同上。
    explicit SimpleRemapper(ca::str::Utf8StringArena&                            arena,
                            const std::unordered_map<std::string, std::string>&  mapping)
        : Remapper(arena)
        , mapping_(mapping)
    {}

    ca::str::Utf8StringRef map(ca::str::Utf8StringRef internalName) override;

private:
    std::unordered_map<std::string, std::string> mapping_;
};

}   // namespace jbytecode::commons
