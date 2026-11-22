#pragma once
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector3D>
#include <QtQml/qqmlregistration.h>
#include <array>

class QThread;

// Stateless classification plus a timed stability gate; no display requests.
struct OrientationFilter {
    int angle = -1;
    int candidate = -1;
    qint64 candidateSince = -1;
    int candidateSamples = 0;
    double previousMagnitude = 0;
    qint64 lastSample = -1;
    static int classify(const QVector3D &gravity);
    bool update(const QVector3D &gravity, qint64 milliseconds);
    void reset() { *this = {}; }
};

class ScreenOrientation : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(int orientation READ orientation NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit ScreenOrientation(bool tablet, QObject *parent = nullptr,
                               QString sensorRoot = QStringLiteral("/sys/bus/iio/devices"));
    ~ScreenOrientation() override;
    bool available() const { return m_available; }
    bool enabled() const { return m_enabled; }
    int orientation() const { return m_filter.angle; }
    QString status() const;
    void setEnabled(bool value);
    static bool parseMountMatrix(const QString &text, std::array<double, 9> *matrix);
signals:
    void changed();
    void sampleRequested(const QString &directory, quint64 generation);
private:
    void discover();
    void requestSample();
    void received(quint64 generation, bool success, const QVector3D &sample);
    const bool m_tablet;
    const QString m_sensorRoot;
    QString m_device;
    std::array<double, 9> m_matrix{};
    bool m_available = false, m_enabled = false, m_inFlight = false;
    int m_failures = 0;
    quint64 m_generation = 0;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QThread *m_thread = nullptr;
    OrientationFilter m_filter;
};
