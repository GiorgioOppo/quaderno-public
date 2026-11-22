#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <functional>

// Portable, content-addressed snapshots shared by local storage and wireless sync.
// Pages and large string assets are immutable objects; the manifest commits to every byte.
namespace SyncPackage {
inline constexpr qsizetype defaultMaximumBytes = 64 * 1024 * 1024;
inline constexpr qsizetype maximumManifestBytes = 8 * 1024 * 1024;

struct Package {
    QByteArray manifest;
    QMap<QString, QByteArray> objects;
    bool isValid() const { return !manifest.isEmpty(); }
};

QString digest(const QByteArray &bytes);
Package split(const QByteArray &payload, QString *error = nullptr,
              qsizetype maximumBytes = defaultMaximumBytes);
QMap<QString, qsizetype> objectSizes(const QByteArray &manifest, QString *error = nullptr,
                                   qsizetype maximumBytes = defaultMaximumBytes);
QByteArray assemble(const QByteArray &manifest,
                    const std::function<QByteArray(const QString &)> &load,
                    QString *error = nullptr, qsizetype maximumBytes = defaultMaximumBytes);
}
