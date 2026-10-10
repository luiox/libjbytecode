#pragma once

#include <string>
#include <vector>

#include "jbytecode/core/visitors.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class SerialVersionUIDAdder : public jbytecode::core::ClassVisitor
{
public:
    SerialVersionUIDAdder(jbytecode::core::ClassVisitor* cv);

    void visit(int version, int access, ca::str::Utf8StringRef name,
               ca::str::Utf8StringRef signature, ca::str::Utf8StringRef superName,
               const std::vector<ca::str::Utf8StringRef>& interfaces) override;

    jbytecode::core::FieldVisitor* visitField(int access, ca::str::Utf8StringRef name,
                                              ca::str::Utf8StringRef descriptor,
                                              ca::str::Utf8StringRef signature,
                                              const ca::core::Any&   value) override;

    jbytecode::core::MethodVisitor* visitMethod(
        int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
        ca::str::Utf8StringRef                     signature,
        const std::vector<ca::str::Utf8StringRef>& exceptions) override;

    void visitInnerClass(ca::str::Utf8StringRef innerClassName, ca::str::Utf8StringRef outerName,
                         ca::str::Utf8StringRef innerName, int innerClassAccess) override;

    void visitEnd() override;

    bool hasSVUID() const { return hasSvuid_; }

protected:
    SerialVersionUIDAdder(int api, jbytecode::core::ClassVisitor* cv);
    virtual void      addSVUID(long long svuid);
    virtual long long computeSVUID();

private:
    struct Item
    {
        ca::str::Utf8StringRef name;
        int                    access;
        ca::str::Utf8StringRef descriptor;
        bool                   operator<(const Item& other) const;
    };

    static std::vector<uint8_t> computeSHAdigest(const std::vector<uint8_t>& value);
    static void writeItems(std::vector<Item>& items, std::vector<uint8_t>& data, bool dotted);

    static const ca::str::Utf8StringRef CLINIT;

    bool                                computeSvuid_ = false;
    bool                                hasSvuid_     = false;
    int                                 access_       = 0;
    ca::str::Utf8StringRef              name_;
    std::vector<ca::str::Utf8StringRef> interfaces_;
    std::vector<Item>                   svuidFields_;
    std::vector<Item>                   svuidConstructors_;
    std::vector<Item>                   svuidMethods_;
    bool                                hasStaticInitializer_ = false;
};

}   // namespace jbytecode::commons
