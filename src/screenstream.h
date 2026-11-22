#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <atomic>
#include <memory>

class QQuickImageProvider;
class QQuickWindow;
class QWindow;
class WirelessSync;
struct ScreenStreamFrame;

Q_MOC_INCLUDE("QWindow")

class ScreenStream final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(bool viewerOpen READ viewerOpen NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QUrl frameSource READ frameSource NOTIFY frameChanged)
    Q_PROPERTY(int frameOrientation READ frameOrientation NOTIFY frameChanged)
    Q_PROPERTY(bool remoteNavigationAllowed READ remoteNavigationAllowed NOTIFY navigationChanged)
    Q_PROPERTY(QString remoteSection READ remoteSection NOTIFY navigationChanged)

public:
    explicit ScreenStream(WirelessSync *wireless, QObject *parent = nullptr);
    QQuickImageProvider *createImageProvider() const;
    void attach(QQuickWindow *window);
    void setBlocked(bool blocked);
    bool available() const;
    bool active() const { return !m_session.isEmpty(); }
    bool viewerOpen() const { return m_viewerOpen; }
    QString status() const;
    QUrl frameSource() const;
    int frameOrientation() const;
    bool remoteNavigationAllowed() const { return m_remoteNavigationAllowed; }
    QString remoteSection() const { return m_remoteSection; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void startWithRemoteNavigation();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void setViewerOpen(bool open);
    Q_INVOKABLE void setRemoteNavigationAllowed(bool allowed);
    Q_INVOKABLE void requestTap(qreal x, qreal y);
    Q_INVOKABLE void requestKey(const QString &action, const QString &text);
    Q_INVOKABLE void reportSection(const QString &section);
    Q_INVOKABLE QRect viewerAvailableArea(QWindow *window) const;

signals:
    void stateChanged();
    void frameChanged();
    void navigationChanged();
    void remoteTapRequested(qreal x, qreal y);
    void remoteKeyRequested(const QString &action, const QString &text);

private:
    void startSession(bool allowRemoteNavigation);
    void linkChanged();
    void receive(const QVariantMap &message, const QByteArray &payload);
    bool canSendNavigation() const;
    void capture();
    void sendCapturedFrame();
    bool send(const QString &type, const QString &session = {}, QVariantMap fields = {},
              const QByteArray &payload = {});
    void publishViewer();
    void finish(const QString &message = {}, bool notifyPeer = false);

    WirelessSync *const m_wireless;
    const bool m_tablet;
    const std::shared_ptr<ScreenStreamFrame> m_frame;
    QPointer<QQuickWindow> m_window;
    QTimer m_captureTimer;
    QTimer m_ackDeadline;
    QTimer m_heartbeat;
    QTimer m_livenessDeadline;
    QFutureWatcher<QByteArray> m_encoder;
    bool m_blocked = true;
    bool m_linkAvailable = false;
    bool m_viewerOpen = false;
    bool m_peerViewerOpen = false;
    bool m_remoteNavigationAllowed = false;
    bool m_frameDirty = false;
    bool m_encoding = false;
    QString m_session;
    QString m_navigationPermissionId;
    QString m_localSection;
    QString m_remoteSection;
    QString m_encodingSession;
    QString m_status;
    QByteArray m_lastCapture;
    std::atomic<int> m_synchronizedOrientation{0};
    std::atomic<int> m_presentedOrientation{0};
    int m_encodingOrientation = 0;
    int m_lastCaptureOrientation = 0;
    int m_sequence = 0;
    int m_emptyCaptures = 0;
};
