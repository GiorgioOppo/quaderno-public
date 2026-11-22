#pragma once

#include "deltatransfer.h"

#include <QByteArray>
#include <QHash>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class Notebook;
class QSettings;

class BleSync final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool applying READ applying NOTIFY changed)
    Q_PROPERTY(bool completed READ completed NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantMap previewInfo READ previewInfo NOTIFY changed)
    Q_PROPERTY(bool autoSync READ autoSync WRITE setAutoSync NOTIFY changed)
    Q_PROPERTY(bool automaticAllowed READ automaticAllowed WRITE setAutomaticAllowed NOTIFY changed)
    Q_PROPERTY(bool triggeredAutomaticSync READ triggeredAutomaticSync WRITE setTriggeredAutomaticSync NOTIFY changed)
    Q_PROPERTY(QString conflictPolicy READ conflictPolicy WRITE setConflictPolicy NOTIFY changed)
    Q_PROPERTY(QStringList pinnedBooks READ pinnedBooks NOTIFY pinnedBooksChanged)
    Q_PROPERTY(quint64 bookStatusVersion READ bookStatusVersion NOTIFY bookStatusChanged)
    Q_PROPERTY(QVariantMap resumeOffer READ resumeOffer NOTIFY resumeChanged)
    Q_PROPERTY(QString resumeStatus READ resumeStatus NOTIFY resumeChanged)
    Q_PROPERTY(bool resumeBusy READ resumeBusy NOTIFY resumeChanged)
    Q_PROPERTY(bool canAcceptResume READ canAcceptResume NOTIFY resumeChanged)
    Q_PROPERTY(qreal transferProgress READ transferProgress NOTIFY changed)
    Q_PROPERTY(bool incremental READ incremental NOTIFY changed)
    Q_PROPERTY(bool hasPendingTransfer READ hasPendingTransfer NOTIFY changed)
    Q_PROPERTY(int pendingAutomaticCount READ pendingAutomaticCount NOTIFY automaticStateChanged)
    Q_PROPERTY(bool automaticRetryPending READ automaticRetryPending NOTIFY automaticStateChanged)

public:
    explicit BleSync(Notebook *notebook, QObject *parent = nullptr, QString stateDirectory = {});

    bool connected() const { return m_connected; }
    bool busy() const { return m_stage == Stage::Preview || m_stage == Stage::Apply; }
    bool applying() const { return m_stage == Stage::Apply; }
    bool completed() const { return m_completed; }
    QString status() const { return m_status; }
    QVariantMap previewInfo() const { return m_preview; }
    bool autoSync() const { return m_autoSync; }
    bool automaticAllowed() const { return m_automaticAllowed; }
    bool triggeredAutomaticSync() const { return m_triggeredAutomaticSync; }
    QString conflictPolicy() const { return m_conflictPolicy; }
    QStringList pinnedBooks() const { return m_pins; }
    quint64 bookStatusVersion() const { return m_bookStatusVersion; }
    QVariantMap resumeOffer() const { return m_resumeOffer; }
    QString resumeStatus() const { return m_resumeStatus; }
    bool resumeBusy() const { return !m_outgoingResume.bookId.isEmpty(); }
    bool canAcceptResume() const { return !m_editorBusy && !m_resumeOffer.isEmpty(); }
    qreal transferProgress() const { return m_delta.totalBytes() > 0 ? qreal(m_delta.completedBytes()) / m_delta.totalBytes() : 0; }
    bool incremental() const { return m_delta.incremental(); }
    bool hasPendingTransfer() const { return !m_pendingTransferBook.isEmpty(); }
    int pendingAutomaticCount() const;
    bool automaticRetryPending() const;

    void setPeerIdentity(const QString &id);
    void forgetPeer();
    void setPayloadLimit(qsizetype bytes);
    void setConnected(bool connected);
    void receiveMessage(const QVariantMap &message, const QByteArray &payload);

    Q_INVOKABLE void preview(const QString &bookId);
    Q_INVOKABLE void apply();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void setPinned(const QString &bookId, bool pinned);
    Q_INVOKABLE void setEditorBusy(bool busy);
    Q_INVOKABLE QVariantMap bookStatus(const QString &bookId) const;
    Q_INVOKABLE void sendResume(const QString &bookId, const QString &pageId);
    Q_INVOKABLE bool acceptResume();
    Q_INVOKABLE void dismissResume();
    Q_INVOKABLE void cancelResume();
    Q_INVOKABLE void retryPendingTransfer();
    void setAutoSync(bool enabled);
    void setAutomaticAllowed(bool allowed);
    void setTriggeredAutomaticSync(bool triggered);
    void setConflictPolicy(const QString &policy);
    Q_INVOKABLE bool requestAutomaticSync();

signals:
    void changed();
    void pinnedBooksChanged();
    void automaticStateChanged();
    void bookStatusChanged();
    void resumeChanged();
    void sendMessage(const QVariantMap &message, const QByteArray &payload);

private:
    enum class Stage { Idle, Preview, Review, Apply };

    struct AutoAttempt {
        QString revision;
        int retries = 0;
        bool retry = false;
        qint64 retryAt = 0;
    };

    struct BookIssue {
        QString state;
        QString revision;
        QString detail;
    };

    struct ResumeRequest {
        QString bookId;
        QString revision;
        QString pageId;
        QString request;
        int page = 0;
    };

    void beginPreview(const QString &bookId, const QString &revision, bool automatic);
    void receivePreview(const QString &request, const QVariantMap &message, const QByteArray &payload, bool incremental = false);
    void receiveApply(const QString &request, const QVariantMap &message);
    void receiveReply(const QString &type, const QVariantMap &message);
    void sendReply(const QString &type, const QString &request, QVariantMap result);
    void resetOutgoing();
    void resetIncoming();
    void finishOutgoing(const QString &status, bool completed = false, bool conflict = false);
    void scheduleAutoSync();
    void runAutoSync();
    void pauseAutomaticPreview();
    bool queueAutomaticRetry();
    void updateAutomaticState();
    bool savePreferences() const;
    QSettings syncSettings() const;
    void acknowledge(const QString &bookId, const QString &revision);
    void noteBookChanges();
    void updateBookStatus();
    void setBookIssue(const QString &bookId, const QString &revision, const QString &state, const QString &detail);
    void dispatchResume();
    void receiveResume(const QString &request, const QVariantMap &message);
    void receiveResumeReply(const QString &request, const QVariantMap &message);
    void finishResume(const QString &status, bool notifyPeer = false);
    void replyToResume(const QString &status);
    void clearResumeOffer();

    QPointer<Notebook> m_notebook;
    DeltaTransfer m_delta;
    QString m_stateDirectory;
    QTimer m_outgoingTimer;
    QTimer m_incomingTimer;
    QTimer m_autoTimer;
    QTimer m_bookStatusTimer;
    QTimer m_outgoingResumeTimer;
    QTimer m_incomingResumeTimer;
    QElapsedTimer m_retryClock;
    QString m_peer;
    QStringList m_pins;
    QVariantMap m_baselines;
    QVariantMap m_lastSuccess;
    QVariantMap m_applyReceipts;
    QVariantMap m_conflictCopies;
    QStringList m_uncertainAutomaticBooks;
    QStringList m_automaticQueue;
    QHash<QString, QString> m_batchRevisions;
    QString m_pendingTransferBook;
    QString m_pendingTransferRevision;
    bool m_pendingTransferAutomatic = false;
    bool m_pendingTransferApplying = false;
    QHash<QString, BookIssue> m_bookIssues;
    QHash<QString, AutoAttempt> m_autoAttempts;
    quint64 m_bookStatusVersion = 0;
    int m_notifiedAutomaticCount = 0;
    bool m_notifiedAutomaticRetry = false;
    Stage m_stage = Stage::Idle;
    bool m_connected = false;
    bool m_autoSync = false;
    bool m_automaticAllowed = true;
    bool m_triggeredAutomaticSync = false;
    QString m_conflictPolicy = QStringLiteral("ask");
    bool m_automatic = false;
    bool m_completed = false;
    bool m_editorBusy = false;
    qsizetype m_payloadLimit = 8 * 1024 * 1024;
    QString m_status;
    QString m_request;
    QString m_bookId;
    QString m_revision;
    QString m_token;
    QByteArray m_payload;
    QVariantMap m_preview;
    QString m_incomingRequest;
    QString m_incomingToken;
    QString m_incomingAction;
    QString m_incomingBook;
    QByteArray m_incomingPayload;
    QByteArray m_incomingBase;
    ResumeRequest m_outgoingResume;
    ResumeRequest m_incomingResume;
    QVariantMap m_resumeOffer;
    QString m_resumeStatus;
};
