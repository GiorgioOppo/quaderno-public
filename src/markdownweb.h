#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class Notebook;
class QNetworkReply;

class MarkdownWeb final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

public:
    explicit MarkdownWeb(Notebook *notebook, QObject *parent = nullptr);
    bool busy() const;
    QString status() const { return m_status; }
    void setLocked(bool locked);

    Q_INVOKABLE void importFromUrl(const QString &address, const QString &folderId);
    Q_INVOKABLE void cancel();

signals:
    void changed();
    void imported(const QString &bookId);

private:
    void read(QNetworkReply *reply);
    void finish(QNetworkReply *reply);
    void reset();
    void report(const QString &message);

    Notebook *const m_notebook;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_deadline;
    QByteArray m_data;
    QString m_folderId;
    QString m_status;
    bool m_locked = false;
};
