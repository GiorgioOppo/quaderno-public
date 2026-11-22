#include "cloudprotocol.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrlQuery>

namespace CloudProtocol {
QString title(const QString &value)
{
    if (value == QStringLiteral("googleDrive")) return QStringLiteral("Google Drive");
    if (value == QStringLiteral("dropbox")) return QStringLiteral("Dropbox");
    if (value == QStringLiteral("oneDrive")) return QStringLiteral("OneDrive");
    return {};
}

bool provider(const QString &value) { return !title(value).isEmpty(); }

bool safeName(const QString &name)
{
    if (name.isEmpty() || name.toUtf8().size() > 240 || name == QStringLiteral(".")
        || name == QStringLiteral("..") || name.contains(u'/') || name.contains(u'\\')) return false;
    for (const QChar ch : name)
        if (ch.unicode() < 32 || ch.unicode() == 127) return false;
    return true;
}

bool supportedName(const QString &name)
{
    static const QStringList extensions{QStringLiteral("pdf"), QStringLiteral("epub"),
        QStringLiteral("md"), QStringLiteral("csv"), QStringLiteral("quaderno"),
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("webp")};
    return safeName(name) && extensions.contains(QFileInfo(name).suffix().toLower());
}

bool safeId(const QString &id, bool google)
{
    if (id.isEmpty() || id.size() > 1024) return false;
    if (google) {
        static const QRegularExpression pattern(QStringLiteral("\\A[A-Za-z0-9_-]+\\z"));
        return pattern.match(id).hasMatch();
    }
    for (const QChar ch : id)
        if (ch.unicode() < 32 || ch.unicode() == 127 || ch == u'\\') return false;
    return true;
}

bool secureUrl(const QUrl &url)
{
    return url.isValid() && url.scheme() == QStringLiteral("https") && !url.host().isEmpty()
        && url.userInfo().isEmpty() && (url.port() == -1 || url.port() == 443)
        && url.fragment().isEmpty() && url.toEncoded().size() <= 16384;
}

static bool domain(const QString &host, const QString &expected)
{
    return host == expected || host.endsWith(u'.' + expected);
}

bool downloadUrl(const QString &provider, const QUrl &url)
{
    if (!secureUrl(url)) return false;
    const QString host = url.host().toLower();
    if (provider == QStringLiteral("googleDrive"))
        return host == QStringLiteral("www.googleapis.com") || host == QStringLiteral("drive.google.com")
            || domain(host, QStringLiteral("googleusercontent.com"));
    if (provider == QStringLiteral("dropbox"))
        return host == QStringLiteral("content.dropboxapi.com") || domain(host, QStringLiteral("dropboxusercontent.com"));
    if (provider == QStringLiteral("oneDrive"))
        return host == QStringLiteral("graph.microsoft.com") || domain(host, QStringLiteral("1drv.com"))
            || domain(host, QStringLiteral("sharepoint.com")) || domain(host, QStringLiteral("onedrive.com"))
            || domain(host, QStringLiteral("storage.live.com"));
    return false;
}

bool uploadUrl(const QString &provider, const QUrl &url)
{
    if (!secureUrl(url)) return false;
    if (provider == QStringLiteral("googleDrive"))
        return url.host() == QStringLiteral("www.googleapis.com") && url.path().startsWith(QStringLiteral("/upload/drive/v3/files"));
    if (provider == QStringLiteral("oneDrive"))
        return downloadUrl(provider, url) && url.host() != QStringLiteral("graph.microsoft.com");
    return false;
}

bool browserUrl(const QString &provider, const QUrl &url)
{
    if (!secureUrl(url)) return false;
    if (provider == QStringLiteral("googleDrive"))
        return url.host() == QStringLiteral("accounts.google.com") && url.path() == QStringLiteral("/o/oauth2/v2/auth");
    if (provider == QStringLiteral("dropbox"))
        return url.host() == QStringLiteral("www.dropbox.com") && url.path() == QStringLiteral("/oauth2/authorize");
    if (provider == QStringLiteral("oneDrive"))
        return url.host() == QStringLiteral("microsoft.com") || url.host() == QStringLiteral("www.microsoft.com")
            || url.host() == QStringLiteral("login.microsoftonline.com");
    return false;
}

QString randomToken(int bytes)
{
    QByteArray random(bytes, Qt::Uninitialized);
    for (int i = 0; i < bytes; ++i) random[i] = char(QRandomGenerator::system()->generate() & 0xff);
    return QString::fromLatin1(random.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString challenge(const QString &verifier)
{
    return QString::fromLatin1(QCryptographicHash::hash(verifier.toLatin1(), QCryptographicHash::Sha256)
        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QByteArray form(const QList<QPair<QString, QString>> &values)
{
    QByteArray result;
    for (const auto &pair : values) {
        if (!result.isEmpty()) result += '&';
        result += QUrl::toPercentEncoding(pair.first) + '=' + QUrl::toPercentEncoding(pair.second);
    }
    return result;
}

QByteArray asciiJson(const QJsonObject &value)
{
    const QString json = QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact));
    QByteArray result;
    for (const QChar ch : json) {
        if (ch.unicode() > 127) result += "\\u" + QByteArray::number(ch.unicode(), 16).rightJustified(4, '0');
        else result += char(ch.unicode());
    }
    return result;
}

QString mimeType(const QString &name)
{
    const QString suffix = QFileInfo(name).suffix().toLower();
    if (suffix == QStringLiteral("pdf")) return QStringLiteral("application/pdf");
    if (suffix == QStringLiteral("epub")) return QStringLiteral("application/epub+zip");
    if (suffix == QStringLiteral("md")) return QStringLiteral("text/markdown; charset=utf-8");
    if (suffix == QStringLiteral("csv")) return QStringLiteral("text/csv; charset=utf-8");
    if (suffix == QStringLiteral("quaderno")) return QStringLiteral("application/json");
    if (suffix == QStringLiteral("png")) return QStringLiteral("image/png");
    if (suffix == QStringLiteral("webp")) return QStringLiteral("image/webp");
    if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")) return QStringLiteral("image/jpeg");
    return QStringLiteral("application/octet-stream");
}

bool validRegistration(const QString &provider, const QJsonObject &value)
{
    const QString client = value.value(QStringLiteral("clientId")).toString();
    if (!CloudProtocol::provider(provider) || client.isEmpty() || client.size() > 256) return false;
    static const QRegularExpression clientPattern(QStringLiteral("\\A[A-Za-z0-9._-]+\\z"));
    if (!clientPattern.match(client).hasMatch()) return false;
    const QString secret = value.value(QStringLiteral("clientSecret")).toString();
    if (secret.size() > 1024 || secret.contains(QChar::Null) || secret.contains(u'\n')) return false;
    if (provider == QStringLiteral("googleDrive") && !client.endsWith(QStringLiteral(".apps.googleusercontent.com"))) return false;
    if (provider == QStringLiteral("oneDrive")) {
        const QString tenant = value.value(QStringLiteral("tenant")).toString(QStringLiteral("common"));
        static const QRegularExpression tenantPattern(QStringLiteral("\\A(?:common|consumers|organizations|[0-9a-fA-F-]{36})\\z"));
        if (!tenantPattern.match(tenant).hasMatch()) return false;
    }
    return true;
}
}
