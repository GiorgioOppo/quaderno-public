#include "cloudstore.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif
#ifdef Q_OS_MACOS
#include <Security/Security.h>
#endif

CloudStore::CloudStore(QString directory, bool useKeychain)
    : m_directory(std::move(directory)), m_keychain(useKeychain) {}

bool CloudStore::privateDirectory(const QString &path)
{
    QFileInfo before(path);
    if (before.isSymLink() || (before.exists() && !before.isDir())) return false;
    if (!QDir().mkpath(path)) return false;
    QFileInfo directory(path);
#ifdef Q_OS_UNIX
    if (directory.ownerId() != uint(geteuid())) return false;
#endif
    return !directory.isSymLink() && QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

bool CloudStore::prepare() { return privateDirectory(m_directory); }

bool CloudStore::privateWrite(const QString &path, const QByteArray &data)
{
    if (QFileInfo(path).isSymLink()) return false;
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    return file.open(QIODevice::WriteOnly) && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
        && file.write(data) == data.size() && file.commit();
}

QByteArray CloudStore::privateRead(const QString &path, qint64 maximum, bool *ok)
{
    *ok = false;
    const QFileInfo info(path);
    if (info.isSymLink() || !info.isFile() || info.size() > maximum) return {};
#ifdef Q_OS_UNIX
    if (info.ownerId() != uint(geteuid())) return {};
#endif
    if (info.permissions() & (QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup
        | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther)) return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QByteArray data = file.read(maximum + 1);
    if (data.size() > maximum) return {};
    *ok = true;
    return data;
}

#ifdef Q_OS_MACOS
namespace {
CFMutableDictionaryRef query(const QString &directory)
{
    CFMutableDictionaryRef result = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const QByteArray account = QCryptographicHash::hash(directory.toUtf8(), QCryptographicHash::Sha256).toHex();
    CFStringRef accountName = CFStringCreateWithBytes(kCFAllocatorDefault,
        reinterpret_cast<const UInt8 *>(account.constData()), account.size(), kCFStringEncodingUTF8, false);
    CFDictionarySetValue(result, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(result, kSecAttrService, CFSTR("org.quaderno.cloud.tokens"));
    CFDictionarySetValue(result, kSecAttrAccount, accountName);
    CFDictionarySetValue(result, kSecUseAuthenticationUI, kSecUseAuthenticationUIFail);
    CFRelease(accountName);
    return result;
}
}
#endif

QJsonObject CloudStore::load(bool *ok) const
{
    *ok = false;
    QByteArray data;
#ifdef Q_OS_MACOS
    if (m_keychain) {
        CFMutableDictionaryRef request = query(m_directory);
        CFDictionarySetValue(request, kSecReturnData, kCFBooleanTrue);
        CFDictionarySetValue(request, kSecMatchLimit, kSecMatchLimitOne);
        CFTypeRef result = nullptr;
        const OSStatus status = SecItemCopyMatching(request, &result);
        CFRelease(request);
        if (status == errSecItemNotFound) { *ok = true; return {}; }
        if (status != errSecSuccess || !result) return {};
        if (CFGetTypeID(result) != CFDataGetTypeID() || CFDataGetLength(static_cast<CFDataRef>(result)) > 128 * 1024) {
            CFRelease(result); return {};
        }
        const auto bytes = static_cast<CFDataRef>(result);
        data = QByteArray(reinterpret_cast<const char *>(CFDataGetBytePtr(bytes)), CFDataGetLength(bytes));
        CFRelease(result);
    } else
#endif
    {
        const QString path = QDir(m_directory).filePath(QStringLiteral("tokens.json"));
        if (!QFileInfo::exists(path)) { *ok = true; return {}; }
        data = privateRead(path, 128 * 1024, ok);
        if (!*ok) return {};
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    *ok = error.error == QJsonParseError::NoError && document.isObject();
    return *ok ? document.object() : QJsonObject{};
}

bool CloudStore::save(const QJsonObject &tokens)
{
    const QByteArray data = QJsonDocument(tokens).toJson(QJsonDocument::Compact);
    if (data.size() > 128 * 1024) return false;
#ifdef Q_OS_MACOS
    if (m_keychain) {
        CFMutableDictionaryRef request = query(m_directory);
        CFDataRef bytes = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(data.constData()), data.size());
        CFMutableDictionaryRef changes = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(changes, kSecValueData, bytes);
        OSStatus status = SecItemUpdate(request, changes);
        if (status == errSecItemNotFound) {
            CFDictionarySetValue(request, kSecValueData, bytes);
            status = SecItemAdd(request, nullptr);
        }
        CFRelease(changes); CFRelease(bytes); CFRelease(request);
        return status == errSecSuccess;
    }
#endif
    return prepare() && privateWrite(QDir(m_directory).filePath(QStringLiteral("tokens.json")), data);
}
