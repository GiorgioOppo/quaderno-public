#pragma once

#include <QJsonObject>
#include <QString>
#include <QUrl>

namespace CloudProtocol {
constexpr qint64 maximumFileBytes = 100 * 1024 * 1024;
constexpr qint64 maximumNotebookBytes = 64 * 1024 * 1024;
constexpr qint64 maximumJsonBytes = 4 * 1024 * 1024;
constexpr qint64 uploadChunkBytes = 10 * 1024 * 1024;
QString title(const QString &provider);
bool provider(const QString &value);
bool safeName(const QString &name);
bool supportedName(const QString &name);
bool safeId(const QString &id, bool google = false);
bool secureUrl(const QUrl &url);
bool downloadUrl(const QString &provider, const QUrl &url);
bool uploadUrl(const QString &provider, const QUrl &url);
bool browserUrl(const QString &provider, const QUrl &url);
QString randomToken(int bytes = 32);
QString challenge(const QString &verifier);
QByteArray form(const QList<QPair<QString, QString>> &values);
QByteArray asciiJson(const QJsonObject &value);
QString mimeType(const QString &name);
bool validRegistration(const QString &provider, const QJsonObject &value);
}
