#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jbytecode::commons {

struct ModuleTargetAttribute
{
    std::string platform;

    ModuleTargetAttribute() = default;
    explicit ModuleTargetAttribute(const std::string& platform)
        : platform(platform)
    {}
};

struct ModuleResolutionAttribute
{
    static constexpr int RESOLUTION_DO_NOT_RESOLVE_BY_DEFAULT   = 1;
    static constexpr int RESOLUTION_WARN_DEPRECATED             = 2;
    static constexpr int RESOLUTION_WARN_DEPRECATED_FOR_REMOVAL = 4;
    static constexpr int RESOLUTION_WARN_INCUBATING             = 8;

    int resolution = 0;

    ModuleResolutionAttribute() = default;
    explicit ModuleResolutionAttribute(int resolution)
        : resolution(resolution)
    {}
};

struct ModuleHashesAttribute
{
    std::string                       algorithm;
    std::vector<std::string>          modules;
    std::vector<std::vector<uint8_t>> hashes;

    ModuleHashesAttribute() = default;
    ModuleHashesAttribute(const std::string& algorithm, const std::vector<std::string>& modules,
                          const std::vector<std::vector<uint8_t>>& hashes)
        : algorithm(algorithm)
        , modules(modules)
        , hashes(hashes)
    {}
};

}   // namespace jbytecode::commons
