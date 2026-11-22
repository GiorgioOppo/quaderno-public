#pragma once

#include "cloudstore.h"
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <functional>
#include <memory>

class QFile;
class QIODevice;
class QLockFile;
class QNetworkAccessManager;
class QNetworkReply;
class QSaveFile;

class CloudAccounts final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY changed)
    Q_PROPERTY(QString provider READ provider NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool authPending READ authPending NOTIFY changed)
    Q_PROPERTY(bool locked READ locked NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QUrl authorizationUrl READ authorizationUrl NOTIFY changed)
    Q_PROPERTY(QString userCode READ userCode NOTIFY changed)
    Q_PROPERTY(int callbackPort READ callbackPort NOTIFY changed)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(QString currentFolder READ currentFolder NOTIFY changed)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY changed)
    Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
public:
    explicit CloudAccounts(QString privateDirectory, bool tabletMode, QObject *parent = nullptr,
                           QNetworkAccessManager *networkForTests = nullptr);
    ~CloudAccounts() override;
    QVariantList accounts() const;
    QString provider() const { return m_provider; }
    bool busy() const { return m_busy; }
    bool authPending() const { return m_authPending; }
    bool locked() const { return m_locked; }
    QString status() const { return m_status; }
    QString error() const { return m_error; }
    QUrl authorizationUrl() const { return m_authorizationUrl; }
    QString userCode() const { return m_userCode; }
    int callbackPort() const { return m_callbackPort; }
    QVariantList entries() const { return m_entries; }
    QString currentFolder() const { return m_folders.isEmpty() ? QString{} : m_folders.last().first; }
    QString currentPath() const;
    bool canGoUp() const { return !m_folders.isEmpty(); }
    bool hasMore() const { return !m_cursor.isEmpty(); }
    double progress() const { return m_progress; }
    void setLocked(bool locked);

    Q_INVOKABLE bool selectProvider(const QString &provider);
    Q_INVOKABLE void beginAuthorization();
    Q_INVOKABLE void submitAuthorizationCode(const QString &code);
    Q_INVOKABLE void openAuthorizationBrowser();
    Q_INVOKABLE void refreshFiles();
    Q_INVOKABLE void enterFolder(const QString &id);
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void downloadFile(const QString &id);
    Q_INVOKABLE bool discardDownload(const QUrl &file);
    Q_INVOKABLE void uploadFile(const QUrl &file, const QString &name = {});
    Q_INVOKABLE void refreshConnection();
    Q_INVOKABLE void disconnectAccount(bool revoke = false);
    Q_INVOKABLE void cancel();

signals:
    void changed();
    void downloaded(const QUrl &localFile, const QString &originalName);

private:
    struct Result {
        int status = 0;
        QByteArray body;
        QMap<QByteArray, QByteArray> headers;
        bool transportError = false;
        bool exceeded = false;
    };
    using Headers = QMap<QByteArray, QByteArray>;
    using Completion = std::function<void(Result)>;
    void request(const QByteArray &method, const QUrl &url, const QByteArray &body,
                 Headers headers, qint64 maximum, Completion completed,
                 QIODevice *upload = nullptr, std::shared_ptr<QSaveFile> sink = {});
    void fail(const QString &message);
    void complete(const QString &message);
    bool beginOperation();
    bool reloadRegistrations();
    QJsonObject registration() const;
    QJsonObject credential() const;
    bool connected(const QString &provider) const;
    QUrl tokenUrl() const;
    Headers bearerHeaders() const;
    void ensureToken(std::function<void()> action, bool force = false);
    bool acceptTokens(const QJsonObject &body, bool refresh);
    void exchangeCode(const QString &code);
    void pollDeviceCode();
    void clearAuthorization();
    void publishAuthorization();
    void acceptCallback();
    void listPage(bool append);
    bool appendEntries(const QJsonObject &body, bool append);
    QVariantMap entry(const QString &id) const;
    void downloadRequest(const QUrl &url, Headers headers, int redirects,
                         const QVariantMap &entry, const QString &path);
    void uploadRequest();
    void uploadNextChunk();
    QString httpError(const Result &result) const;

    const bool m_tabletMode;
    const bool m_testNetwork;
    CloudStore m_store;
    std::unique_ptr<QLockFile> m_storeLock;
    QJsonObject m_registrations;
    QJsonObject m_credentials;
    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_deadline;
    QTimer m_authDeadline;
    QTimer m_poll;
    QTcpServer m_callback;
    quint64 m_generation = 0;
    bool m_storeReady = false;
    bool m_locked = false;
    bool m_busy = false;
    bool m_authPending = false;
    QString m_provider;
    QString m_status;
    QString m_error;
    QUrl m_authorizationUrl;
    QString m_userCode;
    int m_callbackPort = 0;
    QString m_verifier;
    QString m_state;
    QString m_deviceCode;
    qint64 m_authExpires = 0;
    int m_pollSeconds = 5;
    QVariantList m_entries;
    QList<QPair<QString, QString>> m_folders;
    QString m_cursor;
    double m_progress = 0;
    std::shared_ptr<QSaveFile> m_sink;
    QMap<QString, QString> m_downloads;
    std::unique_ptr<QFile> m_upload;
    QString m_uploadName;
    qint64 m_uploadSize = 0;
    qint64 m_uploadOffset = 0;
    QUrl m_uploadUrl;
};
