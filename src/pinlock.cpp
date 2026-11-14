#include "pinlock.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QSaveFile>

#include <algorithm>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <sys/stat.h>
#endif

namespace {
constexpr int FormatVersion = 1;
constexpr int Iterations = 600000;
constexpr int SaltSize = 16;
constexpr int DigestSize = 32;
constexpr int MaximumAttempts = 5;
constexpr qint64 CooldownMs = 30000;
constexpr qint64 MaxFileSize = 4096;

bool validAutoLockMinutes(int minutes)
{
    return minutes == 1 || minutes == 5 || minutes == 15;
}

bool validPin(const QString &pin)
{
    return pin.size() >= 4 && pin.size() <= 8
        && std::all_of(pin.cbegin(), pin.cend(), [](QChar character) {
               return character >= u'0' && character <= u'9';
           });
}

QByteArray randomSalt()
{
    quint32 random[SaltSize / sizeof(quint32)];
    QRandomGenerator::system()->fillRange(random);
    return QByteArray(reinterpret_cast<const char *>(random), SaltSize);
}

QByteArray digestPin(const QString &pin, const QByteArray &salt)
{
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, pin.toLatin1(),
                                             salt, Iterations, DigestSize);
}

bool equalDigest(const QByteArray &left, const QByteArray &right)
{
    if (left.size() != DigestSize || right.size() != DigestSize)
        return false;

    volatile unsigned char difference = 0;
    for (int index = 0; index < DigestSize; ++index) {
        const auto byteDifference = static_cast<unsigned char>(left.at(index) ^ right.at(index));
        difference = static_cast<unsigned char>(difference | byteDifference);
    }
    return difference == 0;
}

QByteArray decodeHex(const QJsonValue &value, qsizetype size)
{
    const QByteArray encoded = value.toString().toLatin1();
    const QByteArray decoded = QByteArray::fromHex(encoded);
    return decoded.size() == size && decoded.toHex() == encoded ? decoded : QByteArray{};
}
}

PinLock::PinLock(const QString &filePath, QObject *parent)
    : QObject(parent), m_filePath(QFileInfo(filePath).absoluteFilePath())
{
    m_countdown.setInterval(250);
    connect(&m_countdown, &QTimer::timeout, this, &PinLock::updateCountdown);
    m_idleLock.setSingleShot(true);
    m_idleLock.setTimerType(Qt::PreciseTimer);
    connect(&m_idleLock, &QTimer::timeout, this, &PinLock::lock);
    if (QCoreApplication::instance())
        QCoreApplication::instance()->installEventFilter(this);

    Record record;
    if (!readRecord(record)) {
        markUnavailable(tr("Impostazioni del PIN illeggibili. Quaderno è bloccato."));
        return;
    }
    updateRecord(record);
    setLocked(configured());
}

bool PinLock::configured() const
{
    return !m_record.digest.isEmpty();
}

bool PinLock::setPin(const QString &pin)
{
    if (m_locked || configured())
        return fail(tr("Per cambiare il PIN serve quello attuale."));
    if (!validPin(pin))
        return fail(tr("Il PIN deve avere da 4 a 8 cifre."));

    QLockFile guard(m_filePath + QStringLiteral(".lock"));
    return beginUpdate(guard) && storeNewPin(pin);
}

bool PinLock::changePin(const QString &currentPin, const QString &newPin)
{
    if (m_locked || !configured())
        return fail(tr("Sblocca Quaderno per cambiare il PIN."));
    if (!validPin(newPin))
        return fail(tr("Il PIN deve avere da 4 a 8 cifre."));

    QLockFile guard(m_filePath + QStringLiteral(".lock"));
    return beginUpdate(guard) && verifyPin(currentPin) && storeNewPin(newPin);
}

bool PinLock::removePin(const QString &currentPin)
{
    if (m_locked || !configured())
        return fail(tr("Sblocca Quaderno per rimuovere il PIN."));

    QLockFile guard(m_filePath + QStringLiteral(".lock"));
    return beginUpdate(guard) && verifyPin(currentPin) && saveRecord({});
}

bool PinLock::unlock(const QString &pin)
{
    if (!m_available || !m_locked)
        return false;

    QLockFile guard(m_filePath + QStringLiteral(".lock"));
    if (!beginUpdate(guard) || !verifyPin(pin))
        return false;

    if (m_record.failedAttempts != 0 || m_record.retryAfter != 0) {
        Record record = m_record;
        record.failedAttempts = 0;
        record.retryAfter = 0;
        if (!saveRecord(record))
            return false;
    }
    setError({});
    setLocked(false);
    return true;
}

void PinLock::lock()
{
    if (!configured() || m_locked)
        return;

    setError({});
    setLocked(true);
}

void PinLock::recordActivity()
{
    if (!configured() || m_locked) {
        m_idleLock.stop();
        return;
    }

    const int interval = m_record.autoLockMinutes * 60000;
    if (m_idleLock.remainingTime() < interval - 250 || m_idleLock.interval() != interval)
        m_idleLock.start(interval);
}

bool PinLock::setAutoLockMinutes(int minutes)
{
    if (m_locked || !configured() || !validAutoLockMinutes(minutes))
        return false;

    QLockFile guard(m_filePath + QStringLiteral(".lock"));
    if (!beginUpdate(guard) || m_locked)
        return false;
    if (minutes == m_record.autoLockMinutes)
        return true;

    Record record = m_record;
    record.autoLockMinutes = minutes;
    return saveRecord(record);
}

bool PinLock::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TabletPress:
    case QEvent::TabletRelease:
        recordActivity();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

bool PinLock::readRecord(Record &record) const
{
    QFile file(m_filePath);
#ifdef Q_OS_UNIX
    struct stat attributes;
    const QByteArray path = QFile::encodeName(m_filePath);
    if (lstat(path.constData(), &attributes) != 0)
        return errno == ENOENT;
    if (!S_ISREG(attributes.st_mode))
        return false;
#else
    const QFileInfo info(file);
    if (info.isSymLink())
        return false;
    if (!info.exists()) {
        const QFileInfo directory(info.absolutePath());
        return !directory.exists() || (directory.isDir() && directory.isReadable());
    }
    if (!info.isFile())
        return false;
#endif
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray encoded = file.read(MaxFileSize + 1);
    if (file.error() != QFileDevice::NoError || encoded.size() > MaxFileSize)
        return false;

    const QJsonObject object = QJsonDocument::fromJson(encoded).object();
    const QJsonValue enabled = object.value(QStringLiteral("enabled"));
    if (object.value(QStringLiteral("version")).toInt(-1) != FormatVersion || !enabled.isBool())
        return false;
    if (!enabled.toBool())
        return true;

    record.salt = decodeHex(object.value(QStringLiteral("salt")), SaltSize);
    record.digest = decodeHex(object.value(QStringLiteral("digest")), DigestSize);
    record.failedAttempts = object.value(QStringLiteral("failedAttempts")).toInt(-1);
    record.retryAfter = object.value(QStringLiteral("retryAfter")).toInteger(-1);
    const QJsonValue lockMinutes = object.value(QStringLiteral("autoLockMinutes"));
    if (!lockMinutes.isUndefined())
        record.autoLockMinutes = lockMinutes.toInt(-1);
    return !record.salt.isEmpty() && !record.digest.isEmpty()
        && record.failedAttempts >= 0 && record.failedAttempts < MaximumAttempts
        && record.retryAfter >= 0 && (record.retryAfter == 0 || record.failedAttempts == 0)
        && validAutoLockMinutes(record.autoLockMinutes);
}

bool PinLock::beginUpdate(QLockFile &guard)
{
    if (!m_available)
        return false;
    if (!QDir().mkpath(QFileInfo(m_filePath).absolutePath()) || !guard.tryLock(0))
        return fail(tr("Impossibile accedere alle impostazioni del PIN."));

    Record latest;
    if (!readRecord(latest)) {
        markUnavailable(tr("Impostazioni del PIN illeggibili. Quaderno è bloccato."));
        return false;
    }

    const bool changed = latest.salt != m_record.salt || latest.digest != m_record.digest;
    updateRecord(latest);
    if (m_secondsRemaining > 0)
        setLocked(true);
    if (!changed)
        return true;

    setLocked(configured());
    return fail(tr("Le impostazioni del PIN sono cambiate. Riprova."));
}

bool PinLock::saveRecord(const Record &record)
{
    const bool enabled = !record.digest.isEmpty();
    QJsonObject object{
        {QStringLiteral("version"), FormatVersion},
        {QStringLiteral("enabled"), enabled},
    };
    if (enabled) {
        object.insert(QStringLiteral("salt"), QString::fromLatin1(record.salt.toHex()));
        object.insert(QStringLiteral("digest"), QString::fromLatin1(record.digest.toHex()));
        object.insert(QStringLiteral("failedAttempts"), record.failedAttempts);
        object.insert(QStringLiteral("retryAfter"), record.retryAfter);
        object.insert(QStringLiteral("autoLockMinutes"), record.autoLockMinutes);
    }

    const QByteArray contents = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write(contents) != contents.size() || !file.commit()) {
        return fail(tr("Impossibile salvare le impostazioni del PIN."));
    }

    updateRecord(record);
    setError({});
    return true;
}

bool PinLock::verifyPin(const QString &pin)
{
    updateCountdown();
    if (m_secondsRemaining > 0)
        return fail(tr("Attendi prima di riprovare."));
    if (!validPin(pin))
        return fail(tr("Il PIN deve avere da 4 a 8 cifre."));

    const QByteArray digest = digestPin(pin, m_record.salt);
    if (digest.size() != DigestSize)
        return fail(tr("Impossibile verificare il PIN."));
    if (equalDigest(digest, m_record.digest))
        return true;

    Record record = m_record;
    record.retryAfter = 0;
    if (++record.failedAttempts >= MaximumAttempts) {
        record.failedAttempts = 0;
        record.retryAfter = QDateTime::currentMSecsSinceEpoch() + CooldownMs;
        setLocked(true);
    }
    if (!saveRecord(record)) {
        markUnavailable(tr("Impossibile registrare il tentativo di accesso. Quaderno è bloccato."));
        return false;
    }
    return fail(tr("PIN errato."));
}

bool PinLock::storeNewPin(const QString &pin)
{
    Record record;
    record.autoLockMinutes = m_record.autoLockMinutes;
    record.salt = randomSalt();
    record.digest = digestPin(pin, record.salt);
    if (record.digest.size() != DigestSize)
        return fail(tr("Impossibile configurare il PIN."));
    return saveRecord(record);
}

bool PinLock::fail(const QString &message)
{
    setError(message);
    return false;
}

void PinLock::markUnavailable(const QString &message)
{
    m_available = false;
    setError(message);
    setLocked(true);
    emit availableChanged();
}

void PinLock::updateRecord(const Record &record)
{
    const bool wasConfigured = configured();
    const int previousMinutes = m_record.autoLockMinutes;
    if (m_record.retryAfter != record.retryAfter) {
        const qint64 duration = std::clamp(record.retryAfter - QDateTime::currentMSecsSinceEpoch(),
                                           qint64(0), CooldownMs);
        m_retryDeadline = QDeadlineTimer(duration);
    }

    m_record = record;
    updateCountdown();
    if (wasConfigured != configured() || previousMinutes != m_record.autoLockMinutes)
        recordActivity();
    if (wasConfigured != configured())
        emit configuredChanged();
    if (previousMinutes != m_record.autoLockMinutes)
        emit autoLockMinutesChanged();
}

void PinLock::updateCountdown()
{
    const int remaining = int((m_retryDeadline.remainingTime() + 999) / 1000);
    if (remaining != m_secondsRemaining) {
        m_secondsRemaining = remaining;
        emit secondsRemainingChanged();
    }
    if (remaining == 0)
        m_countdown.stop();
    else if (!m_countdown.isActive())
        m_countdown.start();
}

void PinLock::setLocked(bool locked)
{
    if (m_locked == locked)
        return;
    m_locked = locked;
    recordActivity();
    emit lockedChanged();
}

void PinLock::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}
