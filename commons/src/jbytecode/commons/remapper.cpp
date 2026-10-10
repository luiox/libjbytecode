#include "jbytecode/commons/remapper.hpp"
#include "jbytecode/commons/signature_remapper.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/readers.hpp"

#include <cstring>

using namespace jbytecode::commons;
using namespace jbytecode::core;
using namespace ca::str;

// ============================================================
// Remapper
// ============================================================

Utf8StringRef Remapper::mapFieldName(Utf8StringRef owner, Utf8StringRef name,
                                     Utf8StringRef descriptor)
{
    (void)owner;
    (void)descriptor;
    return name;
}

Utf8StringRef Remapper::mapMethodName(Utf8StringRef owner, Utf8StringRef name,
                                      Utf8StringRef descriptor)
{
    (void)owner;
    (void)descriptor;
    return name;
}

Utf8StringRef Remapper::mapInvokeDynamicMethodName(Utf8StringRef name, Utf8StringRef descriptor)
{
    (void)descriptor;
    return name;
}

Utf8StringRef Remapper::mapRecordComponentName(Utf8StringRef owner, Utf8StringRef name,
                                                Utf8StringRef descriptor)
{
    (void)owner;
    (void)descriptor;
    return name;
}

Type Remapper::mapType(const Type& type)
{
    switch (type.getSort()) {
    case Type::ARRAY:
    {
        int               dims = type.getDimensions();
        Type              elem = type.getElementType();
        Utf8StringBuilder builder;
        for (int i = 0; i < dims; i++) builder.append("[");
        builder.append(mapType(elem).getDescriptor(remapArena()));
        return Type::getType(remapArena().intern(builder.build().ref()));
    }
    case Type::OBJECT:
    {
        // #921 staticArena 退役：OBJECT 排序 getInternalName 返回 value_ 不 intern，
        // 落点仍按统一口径走注入的 remapArena()。
        Utf8StringRef remapped = map(type.getInternalName(remapArena()));
        if (!remapped.is_empty()) return Type::getObjectType(remapped);
        return type;
    }
    case Type::METHOD:
        // #921 staticArena 退役：METHOD 排序 getDescriptor 返回 value_ 不 intern，落点同上。
        return Type::getMethodType(mapMethodDesc(type.getDescriptor(remapArena())));
    default: return type;
    }
}

Utf8StringRef Remapper::mapDesc(Utf8StringRef descriptor)
{
    if (descriptor.byte_length() == 0) return descriptor;
    return mapType(Type::getType(descriptor)).getDescriptor(remapArena());
}

Utf8StringRef Remapper::mapType(Utf8StringRef internalName)
{
    if (internalName.byte_length() == 0) return {};
    if (internalName.byte_at(0) == '[') {
        return mapType(Type::getType(internalName)).getDescriptor(remapArena());
    }
    // #921 staticArena 退役：OBJECT 排序 getInternalName 返回 value_ 不 intern，落点同上。
    return mapType(Type::getObjectType(internalName)).getInternalName(remapArena());
}

Utf8StringRef Remapper::mapMethodDesc(Utf8StringRef methodDescriptor)
{
    Type              methodType = Type::getMethodType(methodDescriptor);
    std::vector<Type> argTypes   = methodType.getArgumentTypes();
    Type              returnType = methodType.getReturnType();

    Utf8StringBuilder builder;
    builder.append("(");
    for (const auto& arg : argTypes) {
        builder.append(mapType(arg).getDescriptor(remapArena()));
    }
    builder.append(")");
    builder.append(mapType(returnType).getDescriptor(remapArena()));
    return remapArena().intern(builder.build().ref());
}

std::vector<Utf8StringRef> Remapper::mapTypes(const std::vector<Utf8StringRef>& internalNames)
{
    std::vector<Utf8StringRef> result;
    result.reserve(internalNames.size());
    for (const auto& name : internalNames) {
        result.push_back(mapType(name));
    }
    return result;
}

Utf8StringRef Remapper::mapSignature(Utf8StringRef signature)
{
    if (signature.byte_length() == 0) return signature;
    SignatureWriter   sw;
    SignatureReader   sr(signature);
    SignatureRemapper srm(&sw, this);
    sr.accept(&srm);
    // SignatureWriter::toString() 返回 std::string，这是外部依赖的遗留接口
    return remapArena().intern(Utf8StringRef::from_cstr(sw.toString().c_str()));
}

ca::core::Any Remapper::mapValue(const ca::core::Any& value)
{
    if (value.is<Type>()) {
        return ca::core::Any(mapType(value.cast<Type>()));
    }
    if (value.is<Handle>()) {
        const Handle handle = value.cast<Handle>();
        const bool fieldHandle = handle.getTag() <= Opcodes::H_PUTSTATIC;
        const auto mappedOwner = mapType(handle.getOwner());
        const auto mappedName = fieldHandle
                                    ? mapFieldName(handle.getOwner(), handle.getName(), handle.getDesc())
                                    : mapMethodName(handle.getOwner(), handle.getName(), handle.getDesc());
        const auto mappedDesc = fieldHandle ? mapDesc(handle.getDesc())
                                            : mapMethodDesc(handle.getDesc());
        return ca::core::Any(
            Handle(handle.getTag(), mappedOwner, mappedName, mappedDesc, handle.isInterface()));
    }
    if (value.is<ConstantDynamic>()) {
        const ConstantDynamic dynamic = value.cast<ConstantDynamic>();
        std::vector<ca::core::Any> mappedArguments;
        mappedArguments.reserve(dynamic.getBootstrapMethodArguments().size());
        for (const auto& argument : dynamic.getBootstrapMethodArguments()) {
            mappedArguments.push_back(mapValue(argument));
        }
        const auto mappedBootstrap = mapValue(ca::core::Any(dynamic.getBootstrapMethod()));
        return ca::core::Any(ConstantDynamic(
            mapInvokeDynamicMethodName(dynamic.getName(), dynamic.getDescriptor()),
            mapDesc(dynamic.getDescriptor()),
            mappedBootstrap.cast<Handle>(),
            std::move(mappedArguments)));
    }
    return value;
}

Utf8StringRef Remapper::mapAnnotationAttributeName(Utf8StringRef descriptor, Utf8StringRef name)
{
    (void)descriptor;
    return name;
}

// ============================================================
// SimpleRemapper
// ============================================================
// #921 staticArena 退役:旧无 arena 兜底构造(oldName/newName 与 mapping 版)已删除,
// 构造必须注入作业级 arena。

Utf8StringRef SimpleRemapper::map(Utf8StringRef internalName)
{
    auto it = mapping_.find(internalName.to_std_string());
    if (it != mapping_.end()) {
        return remapArena().intern(Utf8StringRef::from_cstr(it->second.c_str()));
    }
    return internalName;
}
