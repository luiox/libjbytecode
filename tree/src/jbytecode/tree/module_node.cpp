#include "jbytecode/tree/nodes.hpp"
#include "jbytecode/core/visitors.hpp"

using namespace jbytecode::tree;
using namespace jbytecode::core;

ModuleNode::ModuleNode(ca::str::Utf8StringRef name, int access, ca::str::Utf8StringRef version)
    : ModuleVisitor(Opcodes::ASM9)
    , name(name)
    , access(access)
    , version(version)
{}

void ModuleNode::visitRequire(ca::str::Utf8StringRef module, int access,
                              ca::str::Utf8StringRef version)
{
    requiresList.emplace_back(module, access, version);
}

void ModuleNode::visitExport(ca::str::Utf8StringRef packaze, int access,
                             const std::vector<ca::str::Utf8StringRef>& modules)
{
    exportsList.emplace_back(packaze, access, modules);
}

void ModuleNode::visitOpen(ca::str::Utf8StringRef packaze, int access,
                           const std::vector<ca::str::Utf8StringRef>& modules)
{
    opensList.emplace_back(packaze, access, modules);
}

void ModuleNode::visitUse(ca::str::Utf8StringRef service)
{
    usesList.push_back(service);
}

void ModuleNode::visitProvide(ca::str::Utf8StringRef                     service,
                              const std::vector<ca::str::Utf8StringRef>& providers)
{
    providesList.emplace_back(service, providers);
}

void ModuleNode::accept(ClassVisitor* cv)
{
    ModuleVisitor* mv = cv->visitModule(name, access, version);
    if (mv) {
        for (auto& r : requiresList) mv->visitRequire(r.module, r.access, r.version);
        for (auto& e : exportsList) mv->visitExport(e.packaze, e.access, e.modules);
        for (auto& o : opensList) mv->visitOpen(o.packaze, o.access, o.modules);
        for (auto& u : usesList) mv->visitUse(u);
        for (auto& p : providesList) mv->visitProvide(p.service, p.providers);
        mv->visitEnd();
    }
}
