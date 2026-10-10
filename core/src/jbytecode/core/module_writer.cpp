/// @file        module_writer.cpp
/// @brief       ModuleWriter implementation — generates Module, ModulePackages and
///              ModuleMainClass JVMS attributes (JVMS 4.7.25, 4.7.26, 4.7.27).
///
///              Follows ASM ModuleWriter.java semantics: data is written to
///              internal ByteVector buffers during visit*, then serialized
///              via putAttributes().
///
/// @author      Eric Bruneton (ASM), C++ port

#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/constants.hpp"

using namespace ca::str;

namespace jbytecode::core {

ModuleWriter::ModuleWriter(SymbolTable& symbolTable, Utf8StringRef name, int access,
                           Utf8StringRef version)
    : ModuleVisitor(Opcodes::ASM9)
    , symbolTable_(symbolTable)
    , moduleNameIndex_(symbolTable.addConstantModule(name)->index)
    , moduleFlags_(access)
    , moduleVersionIndex_(
          version.data() && version.byte_length() > 0 ? symbolTable.addConstantUtf8(version) : 0)
{}

void ModuleWriter::visitMainClass(Utf8StringRef mainClass)
{
    mainClassIndex_ = symbolTable_.addConstantClass(mainClass)->index;
}

void ModuleWriter::visitPackage(Utf8StringRef packaze)
{
    packageIndex_.putShort(symbolTable_.addConstantPackage(packaze)->index);
    packageCount_++;
}

void ModuleWriter::visitRequire(Utf8StringRef module, int access, Utf8StringRef version)
{
    requires_.putShort(symbolTable_.addConstantModule(module)->index);
    requires_.putShort(access);
    requires_.putShort(
        version.data() && version.byte_length() > 0 ? symbolTable_.addConstantUtf8(version) : 0);
    requiresCount_++;
}

void ModuleWriter::visitExport(Utf8StringRef packaze, int access,
                               const std::vector<Utf8StringRef>& modules)
{
    exports_.putShort(symbolTable_.addConstantPackage(packaze)->index);
    exports_.putShort(access);
    exports_.putShort(static_cast<int>(modules.size()));
    for (auto& m : modules) {
        exports_.putShort(symbolTable_.addConstantModule(m)->index);
    }
    exportsCount_++;
}

void ModuleWriter::visitOpen(Utf8StringRef packaze, int access,
                             const std::vector<Utf8StringRef>& modules)
{
    opens_.putShort(symbolTable_.addConstantPackage(packaze)->index);
    opens_.putShort(access);
    opens_.putShort(static_cast<int>(modules.size()));
    for (auto& m : modules) {
        opens_.putShort(symbolTable_.addConstantModule(m)->index);
    }
    opensCount_++;
}

void ModuleWriter::visitUse(Utf8StringRef service)
{
    usesIndex_.putShort(symbolTable_.addConstantClass(service)->index);
    usesCount_++;
}

void ModuleWriter::visitProvide(Utf8StringRef service, const std::vector<Utf8StringRef>& providers)
{
    provides_.putShort(symbolTable_.addConstantClass(service)->index);
    provides_.putShort(static_cast<int>(providers.size()));
    for (auto& p : providers) {
        provides_.putShort(symbolTable_.addConstantClass(p)->index);
    }
    providesCount_++;
}

int ModuleWriter::getAttributeCount() const
{
    return 1 + (packageCount_ > 0 ? 1 : 0) + (mainClassIndex_ > 0 ? 1 : 0);
}

int ModuleWriter::computeAttributesSize()
{
    symbolTable_.addConstantUtf8(Constants::MODULE.ref());
    // 6 (name) + 6 (name/flags/version) + 10 (5*2 for counts) = 22
    int size = 22 + requires_.size() + exports_.size() + opens_.size() + usesIndex_.size() +
               provides_.size();
    if (packageCount_ > 0) {
        symbolTable_.addConstantUtf8(Constants::MODULE_PACKAGES.ref());
        // 6 (name) + 2 (package_count) = 8
        size += 8 + packageIndex_.size();
    }
    if (mainClassIndex_ > 0) {
        symbolTable_.addConstantUtf8(Constants::MODULE_MAIN_CLASS.ref());
        // 6 (name) + 2 (main_class_index) = 8
        size += 8;
    }
    return size;
}

void ModuleWriter::putAttributes(ByteVector& output)
{
    // ---- Module attribute ----
    // 6 (name/flags/version) + 10 (5*2 for counts) = 16 bytes of fixed fields
    int moduleAttrLength = 16 + requires_.size() + exports_.size() + opens_.size() +
                           usesIndex_.size() + provides_.size();
    int moduleNameIdx = symbolTable_.addConstantUtf8(Constants::MODULE.ref());
    output.putShort(moduleNameIdx);
    output.putInt(moduleAttrLength);
    output.putShort(moduleNameIndex_);
    output.putShort(moduleFlags_);
    output.putShort(moduleVersionIndex_);
    output.putShort(requiresCount_);
    output.putByteArray(requires_.data(), 0, requires_.size());
    output.putShort(exportsCount_);
    output.putByteArray(exports_.data(), 0, exports_.size());
    output.putShort(opensCount_);
    output.putByteArray(opens_.data(), 0, opens_.size());
    output.putShort(usesCount_);
    output.putByteArray(usesIndex_.data(), 0, usesIndex_.size());
    output.putShort(providesCount_);
    output.putByteArray(provides_.data(), 0, provides_.size());

    // ---- ModulePackages attribute ----
    if (packageCount_ > 0) {
        int mpNameIdx = symbolTable_.addConstantUtf8(Constants::MODULE_PACKAGES.ref());
        output.putShort(mpNameIdx);
        output.putInt(2 + packageIndex_.size());
        output.putShort(packageCount_);
        output.putByteArray(packageIndex_.data(), 0, packageIndex_.size());
    }

    // ---- ModuleMainClass attribute ----
    if (mainClassIndex_ > 0) {
        int mmcNameIdx = symbolTable_.addConstantUtf8(Constants::MODULE_MAIN_CLASS.ref());
        output.putShort(mmcNameIdx);
        output.putInt(2);
        output.putShort(mainClassIndex_);
    }
}

}   // namespace jbytecode::core
