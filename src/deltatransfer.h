#pragma once

#include "syncpackage.h"

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantMap>
#include <functional>

// Runs inside the authenticated SyncChannel. Never applies a notebook itself:
// reconstructed payloads must pass the normal preview/conflict/confirmation flow.
class DeltaTransfer final : public QObject
{
    Q_OBJECT
public:
    explicit DeltaTransfer(std::function<QByteArray(const QString &)> localBook,
                           QObject *parent = nullptr, QString storageRoot = {});
    void setConnected(bool connected, const QString &peer);
    void sendPreview(const QVariantMap &metadata, const QByteArray &payload, qsizetype legacyLimit);
    bool receiveMessage(const QVariantMap &metadata, const QByteArray &payload);
    void cancelOutgoing(const QString &request);
    void cancelIncoming(const QString &request);
    void forgetPeer(const QString &peer);
    qsizetype completedBytes() const { return m_completedBytes; }
    qsizetype totalBytes() const { return m_totalBytes; }
    bool incremental() const { return m_capable; }

signals:
    void sendMessage(const QVariantMap &metadata, const QByteArray &payload);
    void previewReady(const QVariantMap &metadata, const QByteArray &payload);
    void failed(const QString &request, const QString &error);
    void progressChanged();

private:
    void beginSend();
    void receiveOffer(const QVariantMap &metadata, const QByteArray &manifest);
    void receiveNeed(const QVariantMap &metadata);
    void receiveChunk(const QVariantMap &metadata, const QByteArray &payload);
    void requestNext();
    void incomingError(const QString &message);
    void clearIncoming();
    QString cacheDirectory() const;
    QString objectPath(const QString &hash, bool partial = false) const;
    QByteArray readObject(const QString &hash, qsizetype expected) const;
    bool storeObject(const QString &hash, const QByteArray &bytes);
    void pruneCache(const QSet<QString> &keep = {});

    std::function<QByteArray(const QString &)> m_localBook;
    QString m_storageRoot;
    QTimer m_negotiationTimer;
    QTimer m_chunkTimer;
    QTimer m_receiveTimer;
    bool m_connected = false;
    bool m_capable = false;
    bool m_negotiated = false;
    int m_peerNotebookVersion = 0;
    QString m_peer;
    QString m_helloRequest;
    QVariantMap m_outgoingMetadata;
    QByteArray m_outgoingPayload;
    qsizetype m_legacyLimit = 0;
    SyncPackage::Package m_package;
    QVariantMap m_lastChunkMetadata;
    QByteArray m_lastChunk;
    int m_chunkRetries = 0;
    qsizetype m_completedBytes = 0;
    qsizetype m_totalBytes = 0;
    QVariantMap m_incomingMetadata;
    QByteArray m_manifest;
    QMap<QString, qsizetype> m_sizes;
    QStringList m_objectOrder;
    qsizetype m_nextObject = 0;
    qint64 m_receivedBytes = 0;
    QSet<QString> m_verified;
    QString m_wantedHash;
    QHash<QString, int> m_integrityRetries;
};
