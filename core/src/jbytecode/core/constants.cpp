#include "constants.hpp"
#include <regex>
#include <stdexcept>
#include "libca/str/string_util.hpp"

using namespace ca::str;
using namespace ca;

namespace jbytecode::core {

bool Constants::isWhitelisted(const std::string& internalName)
{
    if (internalName.rfind("org/objectweb/asm/", 0) != 0) {
        return false;
    }
    // "Test$" inner class pattern
    if (StringUtil::contains(internalName, "Test$")) {
        return true;
    }
    static const std::regex tracePattern(
        "org/objectweb/asm/util/Trace"
        "(Annotation|Class|Field|Method|Module|RecordComponent|Signature)"
        "Visitor(\\$.*)?");
    if (std::regex_match(internalName, tracePattern)) {
        return true;
    }
    static const std::regex checkPattern(
        "org/objectweb/asm/util/Check"
        "(Annotation|Class|Field|Method|Module|RecordComponent|Signature)"
        "Adapter(\\$.*)?");
    if (std::regex_match(internalName, checkPattern)) {
        return true;
    }
    return false;
}

void Constants::checkIsPreview(const u8* data, int len)
{
    if (data == nullptr) {
        throw std::runtime_error("Bytecode not available, can't check class version");
    }
    if (len < 6) {
        throw std::runtime_error("I/O error, can't check class version");
    }
    int minorVersion = (static_cast<int>(data[4]) << 8) | static_cast<int>(data[5]);
    if (minorVersion != 0xFFFF) {
        throw std::runtime_error(
            "ASM10_EXPERIMENTAL can only be used by classes compiled with --enable-preview");
    }
}

}   // namespace jbytecode::core
