#include "jbytecode/commons/serial_version_uid_adder.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "libca/crypto/sha1.hpp"
#include "jbytecode/core/opcodes.hpp"

using namespace jbytecode::commons;
using namespace jbytecode::core;
using ca::str::Utf8StringRef;

namespace {

void writeIntBE(std::vector<uint8_t>& data, int value)
{
    data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(value & 0xFF));
}

// 写入 JVM modified-UTF8 编码的字符串（长度前缀 + 编码字节）。直接消费 Utf8StringRef 字节。
void writeUTF(std::vector<uint8_t>& data, Utf8StringRef str)
{
    std::vector<uint8_t> encoded;
    auto                 len = str.byte_length();
    for (ca::usize i = 0; i < len; i++) {
        unsigned char c = static_cast<unsigned char>(str.byte_at(i));
        if (c > 0 && c <= 0x7F) {
            encoded.push_back(c);
        }
        else if (c <= 0x7FF) {
            encoded.push_back(0xC0 | (c >> 6));
            encoded.push_back(0x80 | (c & 0x3F));
        }
        else {
            // c 声明宽度 8 位，先升 unsigned 再移位：直接 c >> 12 触发
            // MSVC C4333（右移位数过大）；升位后语义不变（c ≤ 0xFF 时
            // cp >> 12 恒为 0）。
            const unsigned int cp = c;
            encoded.push_back(0xE0 | (cp >> 12));
            encoded.push_back(0x80 | ((cp >> 6) & 0x3F));
            encoded.push_back(0x80 | (cp & 0x3F));
        }
    }
    if (encoded.size() > 65535) {
        encoded.resize(65535);
    }
    data.push_back(static_cast<uint8_t>((encoded.size() >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(encoded.size() & 0xFF));
    data.insert(data.end(), encoded.begin(), encoded.end());
}

// 将 internal name 的 '/' 替换为 '.'（用于 serialVersionUID 计算），返回 Utf8String。
// 名称均为 ASCII，'/' 与 '.' 均单字节。
ca::str::Utf8String toDotted(Utf8StringRef ref)
{
    std::string s(reinterpret_cast<const char*>(ref.data()), ref.byte_length());
    std::replace(s.begin(), s.end(), '/', '.');
    return ca::str::Utf8String(reinterpret_cast<const ca::u8*>(s.data()), s.size());
}

}   // namespace

const ca::str::Utf8StringRef SerialVersionUIDAdder::CLINIT =
    ca::str::ZUtf8StringRef::from_static("<clinit>");

SerialVersionUIDAdder::SerialVersionUIDAdder(ClassVisitor* cv)
    : SerialVersionUIDAdder(Opcodes::ASM9, cv)
{}

SerialVersionUIDAdder::SerialVersionUIDAdder(int api, ClassVisitor* cv)
    : ClassVisitor(api, cv)
{}

void SerialVersionUIDAdder::visit(int version, int access, Utf8StringRef name,
                                  Utf8StringRef signature, Utf8StringRef superName,
                                  const std::vector<Utf8StringRef>& interfaces)
{
    computeSvuid_ = (access & Opcodes::ACC_ENUM) == 0;
    if (computeSvuid_) {
        name_       = name;
        access_     = access;
        interfaces_ = interfaces;
        svuidFields_.clear();
        svuidConstructors_.clear();
        svuidMethods_.clear();
    }
    ClassVisitor::visit(version, access, name, signature, superName, std::vector<Utf8StringRef>());
}

FieldVisitor* SerialVersionUIDAdder::visitField(int access, Utf8StringRef name,
                                                Utf8StringRef descriptor, Utf8StringRef signature,
                                                const ca::core::Any& value)
{
    if (computeSvuid_) {
        if (name.equals("serialVersionUID")) {
            computeSvuid_ = false;
            hasSvuid_     = true;
        }
        if ((access & Opcodes::ACC_PRIVATE) == 0 ||
            (access & (Opcodes::ACC_STATIC | Opcodes::ACC_TRANSIENT)) == 0) {
            int mods = access & (Opcodes::ACC_PUBLIC | Opcodes::ACC_PRIVATE |
                                 Opcodes::ACC_PROTECTED | Opcodes::ACC_STATIC | Opcodes::ACC_FINAL |
                                 Opcodes::ACC_VOLATILE | Opcodes::ACC_TRANSIENT);
            svuidFields_.push_back({name, mods, descriptor});
        }
    }
    return ClassVisitor::visitField(access, name, descriptor, signature, value);
}

MethodVisitor* SerialVersionUIDAdder::visitMethod(int access, Utf8StringRef name,
                                                  Utf8StringRef descriptor, Utf8StringRef signature,
                                                  const std::vector<Utf8StringRef>& exceptions)
{
    if (computeSvuid_) {
        if (name.equals(CLINIT)) {
            hasStaticInitializer_ = true;
        }
        int mods = access & (Opcodes::ACC_PUBLIC | Opcodes::ACC_PRIVATE | Opcodes::ACC_PROTECTED |
                             Opcodes::ACC_STATIC | Opcodes::ACC_FINAL | Opcodes::ACC_SYNCHRONIZED |
                             Opcodes::ACC_NATIVE | Opcodes::ACC_ABSTRACT | Opcodes::ACC_STRICT);
        if ((access & Opcodes::ACC_PRIVATE) == 0) {
            if (name.equals("<init>")) {
                svuidConstructors_.push_back({name, mods, descriptor});
            }
            else if (!name.equals(CLINIT)) {
                svuidMethods_.push_back({name, mods, descriptor});
            }
        }
    }
    return ClassVisitor::visitMethod(
        access, name, descriptor, signature, std::vector<Utf8StringRef>());
}

void SerialVersionUIDAdder::visitInnerClass(Utf8StringRef innerClassName, Utf8StringRef outerName,
                                            Utf8StringRef innerName, int innerClassAccess)
{
    if (!name_.is_empty() && name_.equals(innerClassName)) {
        access_ = innerClassAccess;
    }
    ClassVisitor::visitInnerClass(innerClassName, outerName, innerName, innerClassAccess);
}

void SerialVersionUIDAdder::visitEnd()
{
    if (computeSvuid_ && !hasSvuid_) {
        addSVUID(computeSVUID());
    }
    ClassVisitor::visitEnd();
}

void SerialVersionUIDAdder::addSVUID(long long svuid)
{
    FieldVisitor* fv = ClassVisitor::visitField(Opcodes::ACC_FINAL | Opcodes::ACC_STATIC,
                                                Utf8StringRef::from_cstr("serialVersionUID"),
                                                Utf8StringRef::from_cstr("J"),
                                                Utf8StringRef(),
                                                ca::core::Any(svuid));
    if (fv) fv->visitEnd();
}

long long SerialVersionUIDAdder::computeSVUID()
{
    std::vector<uint8_t> data;

    writeUTF(data, toDotted(name_));

    int mods = access_;
    if ((mods & Opcodes::ACC_INTERFACE) != 0) {
        mods = svuidMethods_.empty() ? (mods & ~Opcodes::ACC_ABSTRACT)
                                     : (mods | Opcodes::ACC_ABSTRACT);
    }
    writeIntBE(data,
               mods & (Opcodes::ACC_PUBLIC | Opcodes::ACC_FINAL | Opcodes::ACC_INTERFACE |
                       Opcodes::ACC_ABSTRACT));

    std::sort(interfaces_.begin(),
              interfaces_.end(),
              [](const Utf8StringRef& a, const Utf8StringRef& b) { return a.compare(b) < 0; });
    for (auto& iface : interfaces_) {
        writeUTF(data, toDotted(iface));
    }

    writeItems(svuidFields_, data, false);

    if (hasStaticInitializer_) {
        writeUTF(data, CLINIT);
        writeIntBE(data, Opcodes::ACC_STATIC);
        writeUTF(data, Utf8StringRef::from_cstr("()V"));
    }

    writeItems(svuidConstructors_, data, true);
    writeItems(svuidMethods_, data, true);

    std::vector<uint8_t> hashBytes = computeSHAdigest(data);

    long long svuid = 0;
    for (int i = std::min(static_cast<int>(hashBytes.size()), 8) - 1; i >= 0; i--) {
        svuid = (svuid << 8) | (hashBytes[i] & 0xFF);
    }
    return svuid;
}

std::vector<uint8_t> SerialVersionUIDAdder::computeSHAdigest(const std::vector<uint8_t>& value)
{
    ca::crypto::SHA1 sha1;
    unsigned char    rawHash[20];
    sha1.add(value.data(), value.size());
    sha1.get_hash(rawHash);
    return std::vector<uint8_t>(rawHash, rawHash + 20);
}

void SerialVersionUIDAdder::writeItems(std::vector<Item>& items, std::vector<uint8_t>& data,
                                       bool dotted)
{
    std::sort(items.begin(), items.end());
    for (auto& item : items) {
        writeUTF(data, item.name);
        writeIntBE(data, item.access);
        if (dotted) {
            writeUTF(data, toDotted(item.descriptor));
        }
        else {
            writeUTF(data, item.descriptor);
        }
    }
}

bool SerialVersionUIDAdder::Item::operator<(const Item& other) const
{
    int cmp = name.compare(other.name);
    if (cmp != 0) return cmp < 0;
    return descriptor.compare(other.descriptor) < 0;
}
