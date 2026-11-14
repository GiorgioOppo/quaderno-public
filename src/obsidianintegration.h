#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class ObsidianIntegration : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString publishBaseUrl READ publishBaseUrl WRITE updatePublishBaseUrl NOTIFY publishBaseUrlChanged)
    Q_PROPERTY(QUrl lastSavedFile READ lastSavedFile NOTIFY lastSavedFileChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit ObsidianIntegration(QObject *parent = nullptr);

    bool available() const;
    QString publishBaseUrl() const { return m_publishBase.toString(QUrl::FullyEncoded); }
    QUrl lastSavedFile() const { return m_lastSavedFile; }
    QString status() const { return m_status; }

    Q_INVOKABLE bool setPublishBaseUrl(const QString &address);
    Q_INVOKABLE QUrl publishUrl(const QString &notePath) const;
    Q_INVOKABLE bool openPublished(const QString &notePath);
    Q_INVOKABLE bool copyPublished(const QString &notePath);
    Q_INVOKABLE QUrl vaultUrl(const QString &vault, const QString &notePath) const;
    Q_INVOKABLE bool openInObsidian(const QString &vault, const QString &notePath);
    Q_INVOKABLE bool saveMarkdown(const QUrl &vaultFolder, const QString &notePath, const QString &markdown);
    Q_INVOKABLE bool openSavedNote();
    Q_INVOKABLE void clearLastSavedNote();
    Q_INVOKABLE QString suggestedFileName(const QString &title) const;

signals:
    void publishBaseUrlChanged();
    void lastSavedFileChanged();
    void statusChanged();

private:
    void updatePublishBaseUrl(const QString &address) { setPublishBaseUrl(address); }
    void setLastSavedFile(const QUrl &file);
    bool report(const QString &message, bool success = false);

    QUrl m_publishBase;
    QUrl m_lastSavedFile;
    QString m_status;
};
