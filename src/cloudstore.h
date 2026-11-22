#pragma once
#include <QJsonObject>
#include <QString>

class CloudStore {
public:
    explicit CloudStore(QString directory, bool useKeychain);
    bool prepare();
    QJsonObject load(bool *ok) const;
    bool save(const QJsonObject &tokens);
    QString directory() const { return m_directory; }
    static bool privateDirectory(const QString &path);
    static bool privateWrite(const QString &path, const QByteArray &data);
    static QByteArray privateRead(const QString &path, qint64 maximum, bool *ok);
private:
    QString m_directory;
    bool m_keychain;
};
