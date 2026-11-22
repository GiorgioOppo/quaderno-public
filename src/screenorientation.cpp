#include "screenorientation.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QThread>
#include <algorithm>
#include <cmath>

namespace {
QString readSmall(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QByteArray data = file.read(513);
    if (data.size() > 512 || file.error() != QFileDevice::NoError) return {};
    return QString::fromLatin1(data).trimmed();
}
}

class OrientationWorker : public QObject {
    Q_OBJECT
public slots:
    void read(const QString &directory, quint64 generation) {
        QVector3D sample;
        bool success = true;
        for (int axis = 0; axis < 3; ++axis) {
            bool ok = false;
            const QString name = QStringLiteral("in_accel_%1_raw").arg(QStringLiteral("xyz").at(axis));
            const double value = readSmall(QDir(directory).filePath(name)).toDouble(&ok);
            success = success && ok && std::isfinite(value) && std::abs(value) < 1000000;
            sample[axis] = ok ? float(value) : 0;
        }
        emit ready(generation, success, sample);
    }
signals:
    void ready(quint64 generation, bool success, const QVector3D &sample);
};

int OrientationFilter::classify(const QVector3D &gravity)
{
    const double x = gravity.x(), y = gravity.y(), z = gravity.z();
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return -1;
    const double magnitude = std::hypot(std::hypot(x, y), z);
    if (magnitude < 0.0001 || std::hypot(x, y) / magnitude < 0.60) return -1;
    // A diagonal position is intentionally inconclusive: retain the last angle.
    if (std::max(std::abs(x), std::abs(y)) < 1.35 * std::min(std::abs(x), std::abs(y))) return -1;
    if (std::abs(y) >= std::abs(x)) return y > 0 ? 0 : 180;
    return x > 0 ? 90 : 270;
}

bool OrientationFilter::update(const QVector3D &gravity, qint64 milliseconds)
{
    if (lastSample >= 0 && (milliseconds < lastSample || milliseconds - lastSample > 1500)) {
        candidate = -1; candidateSince = -1; candidateSamples = 0; previousMagnitude = 0;
    }
    lastSample = milliseconds;
    const double magnitude = double(gravity.length());
    const bool suddenMotion = previousMagnitude > 0 && magnitude > 0
        && (magnitude / previousMagnitude > 1.8 || magnitude / previousMagnitude < 0.55);
    if (std::isfinite(magnitude) && magnitude > 0) previousMagnitude = magnitude;
    const int next = suddenMotion ? -1 : classify(gravity);
    if (next < 0 || next == angle) {
        candidate = -1; candidateSince = -1; candidateSamples = 0;
        return false;
    }
    if (next != candidate || milliseconds < candidateSince) {
        candidate = next; candidateSince = milliseconds; candidateSamples = 1;
        return false;
    }
    ++candidateSamples;
    if (candidateSamples < 3 || milliseconds - candidateSince < 900) return false;
    angle = next;
    candidate = -1; candidateSince = -1; candidateSamples = 0;
    return true;
}

bool ScreenOrientation::parseMountMatrix(const QString &text, std::array<double, 9> *matrix)
{
    if (!matrix) return false;
    const auto fields = text.split(QRegularExpression(QStringLiteral("\\s*[,;]\\s*")), Qt::SkipEmptyParts);
    if (fields.size() != 9) return false;
    std::array<double, 9> parsed{};
    for (int i = 0; i < 9; ++i) {
        bool ok = false;
        parsed[i] = fields[i].trimmed().toDouble(&ok);
        if (!ok || (parsed[i] != -1 && parsed[i] != 0 && parsed[i] != 1)) return false;
    }
    for (int row = 0; row < 3; ++row) {
        double norm = 0;
        for (int column = 0; column < 3; ++column) norm += parsed[row * 3 + column] * parsed[row * 3 + column];
        if (norm != 1) return false;
        for (int other = row + 1; other < 3; ++other) {
            double dot = 0;
            for (int column = 0; column < 3; ++column) dot += parsed[row * 3 + column] * parsed[other * 3 + column];
            if (dot != 0) return false;
        }
    }
    *matrix = parsed;
    return true;
}

ScreenOrientation::ScreenOrientation(bool tablet, QObject *parent, QString root)
    : QObject(parent), m_tablet(tablet), m_sensorRoot(std::move(root))
{
    m_clock.start();
    m_timer.setInterval(300);
    connect(&m_timer, &QTimer::timeout, this, &ScreenOrientation::requestSample);
    if (!m_tablet) return;
    discover();
    m_thread = new QThread(this);
    auto *worker = new OrientationWorker;
    worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &ScreenOrientation::sampleRequested, worker, &OrientationWorker::read);
    connect(worker, &OrientationWorker::ready, this, &ScreenOrientation::received);
    m_thread->start();
}

ScreenOrientation::~ScreenOrientation()
{
    m_timer.stop();
    if (m_thread) { m_thread->quit(); m_thread->wait(); }
}

void ScreenOrientation::discover()
{
    m_device.clear();
    for (const auto &entry : QDir(m_sensorRoot).entryInfoList({QStringLiteral("iio:device*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString path = entry.absoluteFilePath();
        // This model and mount matrix are provided by the verified Paper Pro driver.
        if (readSmall(QDir(path).filePath(QStringLiteral("name"))) != QStringLiteral("lis2dw12_accel")) continue;
        if (!parseMountMatrix(readSmall(QDir(path).filePath(QStringLiteral("in_accel_mount_matrix"))), &m_matrix)) continue;
        bool axes = true;
        for (const QChar axis : QStringLiteral("xyz"))
            axes = axes && QFileInfo(QDir(path).filePath(QStringLiteral("in_accel_%1_raw").arg(axis))).isReadable();
        if (axes) { m_device = path; break; }
    }
    m_available = !m_device.isEmpty();
}

QString ScreenOrientation::status() const
{
    if (!m_tablet) return tr("La rotazione automatica è disponibile sul Paper Pro.");
    if (!m_available) return tr("Sensore di orientamento non disponibile. Usa la rotazione manuale.");
    if (m_failures) return tr("Lettura del sensore in attesa; l’orientamento attuale viene conservato.");
    if (!m_enabled) return tr("Sensore disponibile. Attiva la rotazione automatica per seguirne l’orientamento.");
    return tr("Ruota il tablet sollevato e attendi un istante. In piano conserva l’orientamento.");
}

void ScreenOrientation::setEnabled(bool value)
{
    value = value && m_tablet;
    if (m_enabled == value) return;
    m_enabled = value;
    ++m_generation;
    const int lastAngle = m_filter.angle;
    m_filter.reset();
    m_filter.angle = lastAngle;
    if (value) { m_timer.start(); requestSample(); }
    else m_timer.stop();
    emit changed();
}

void ScreenOrientation::requestSample()
{
    if (!m_enabled || m_inFlight) return;
    if (m_device.isEmpty()) {
        discover();
        if (!m_available) { m_timer.setInterval(5000); return; }
        emit changed();
    }
    m_inFlight = true;
    emit sampleRequested(m_device, m_generation);
}

void ScreenOrientation::received(quint64 generation, bool success, const QVector3D &raw)
{
    m_inFlight = false;
    if (!m_enabled || generation != m_generation) return;
    if (!success) {
        const int lastAngle = m_filter.angle;
        m_filter.reset();
        m_filter.angle = lastAngle;
        ++m_failures;
        if (m_failures >= 3) { m_device.clear(); m_available = false; m_timer.setInterval(5000); }
        emit changed();
        return;
    }
    const bool recovered = m_failures || !m_available;
    m_failures = 0; m_available = true; m_timer.setInterval(300);
    QVector3D sample;
    for (int row = 0; row < 3; ++row)
        sample[row] = float(m_matrix[row * 3] * raw.x() + m_matrix[row * 3 + 1] * raw.y() + m_matrix[row * 3 + 2] * raw.z());
    const bool changedAngle = m_filter.update(sample, m_clock.elapsed());
    if (recovered || changedAngle) emit changed();
}

#include "screenorientation.moc"
