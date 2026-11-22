#include "paperprosshaccess.h"

#include <QFileInfo>
#include <QHash>
#include <QProcessEnvironment>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

namespace {
const QString systemctl = QStringLiteral("/usr/bin/systemctl");
const QString wifiHelper = QStringLiteral("/usr/bin/rm-ssh-over-wlan");
const QString wifiMarker = QStringLiteral("/data/internal/rm_enable_ssh_wifi_marker");
const QString usb0 = QStringLiteral("dropbear-usb0.socket");
const QString usb1 = QStringLiteral("dropbear-usb1.socket");
const QString wlan = QStringLiteral("dropbear-wlan.socket");
const QString unitDirectory = QStringLiteral("/usr/lib/systemd/system/");
const QString runtimeDirectory = QStringLiteral("/run/systemd/system/");

bool canManage(bool verifiedDevice)
{
#ifdef Q_OS_LINUX
    return verifiedDevice && geteuid() == 0 && QFileInfo(systemctl).isExecutable();
#else
    Q_UNUSED(verifiedDevice)
    return false;
#endif
}

bool verifiedRuntimeMask(const QString &unit)
{
    const QFileInfo runtime(runtimeDirectory + unit);
    return runtime.isSymLink() && runtime.symLinkTarget() == QStringLiteral("/dev/null")
        && QFileInfo(unitDirectory + unit).isFile();
}
}

PaperProSshAccess::PaperProSshAccess(bool verifiedDevice) : m_available(canManage(verifiedDevice))
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("PATH"), QStringLiteral("/usr/sbin:/usr/bin:/sbin:/bin"));
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    m_process.setProcessEnvironment(environment);
    m_process.setStandardErrorFile(QProcess::nullDevice());
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        m_timedOut = true;
        m_process.kill();
    });
    connect(&m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        completed(status == QProcess::NormalExit && exitCode == 0 && !m_timedOut);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            completed(false);
    });
}

PaperProSshAccess::~PaperProSshAccess()
{
    if (m_process.state() != QProcess::NotRunning) {
        disconnect(&m_process, nullptr, this, nullptr);
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

QVariantList PaperProSshAccess::transports() const
{
    const auto entry = [this](const QString &key, State state) {
        QString value;
        QString description;
        const bool editable = m_available && state != State::Unknown && state != State::Unavailable;
        switch (state) {
        case State::Enabled:
            value = QStringLiteral("enabled");
            description = tr("Nuove connessioni consentite.");
            break;
        case State::Disabled:
            value = QStringLiteral("disabled");
            description = tr("Nuove connessioni disattivate.");
            break;
        case State::Partial:
            value = QStringLiteral("partial");
            description = tr("Configurazione e stato del servizio non coincidono.");
            break;
        case State::Unavailable:
            value = QStringLiteral("unavailable");
            description = key == QStringLiteral("bluetooth")
                ? tr("Nessun servizio SSH disponibile su una rete Bluetooth PAN.")
                : tr("Il servizio SSH del firmware non è verificabile su questo collegamento.");
            break;
        case State::Unknown:
            value = QStringLiteral("unknown");
            description = tr("Stato non verificato.");
            break;
        }
        return QVariantMap{{QStringLiteral("key"), key}, {QStringLiteral("state"), value},
            {QStringLiteral("status"), description}, {QStringLiteral("editable"), editable}};
    };
    return {entry(QStringLiteral("wifi"), m_policy.wifi), entry(QStringLiteral("usb"), m_policy.usb),
        entry(QStringLiteral("bluetooth"), State::Unavailable)};
}

PaperProSshAccess::Command PaperProSshAccess::command(Step step, bool enabled)
{
    switch (step) {
    case Step::Probe: case Step::Verify:
        return {systemctl, {QStringLiteral("show"), QStringLiteral("--no-pager"),
            QStringLiteral("--property=Id,LoadState,ActiveState,SubState,FragmentPath,UnitFileState,BindToDevice,Accept,Listen"),
            usb0, usb1, wlan}};
    case Step::WiFi:
        return {wifiHelper, {enabled ? QStringLiteral("on") : QStringLiteral("off")}};
    case Step::MaskUsb:
        return {systemctl, {QStringLiteral("--no-ask-password"), QStringLiteral("mask"),
            QStringLiteral("--runtime"), QStringLiteral("--now"), usb0, usb1}};
    case Step::UnmaskUsb:
        return {systemctl, {QStringLiteral("--no-ask-password"), QStringLiteral("unmask"),
            QStringLiteral("--runtime"), usb0, usb1}};
    case Step::StartUsb:
        return {systemctl, {QStringLiteral("--no-ask-password"), QStringLiteral("start"), usb0, usb1}};
    case Step::Idle:
        return {};
    }
    return {};
}

PaperProSshAccess::Policy PaperProSshAccess::inspect(const QByteArray &systemctlOutput, bool wifiTool,
                                                  bool wifiMarkerPresent, bool usb0Masked, bool usb1Masked)
{
    QHash<QByteArray, QHash<QByteArray, QByteArray>> units;
    QHash<QByteArray, QByteArray> fields;
    const auto saveUnit = [&] {
        if (fields.contains("Id"))
            units.insert(fields.value("Id"), fields);
        fields.clear();
    };
    for (const QByteArray &line : systemctlOutput.split('\n')) {
        if (line.trimmed().isEmpty()) {
            saveUnit();
            continue;
        }
        const auto separator = line.indexOf('=');
        if (separator > 0)
            fields.insert(line.left(separator), line.mid(separator + 1).trimmed());
    }
    saveUnit();
    const auto socket = [&units](const QString &unit, const QByteArray &interface, bool runtimeMask = false) {
        const auto it = units.constFind(unit.toLatin1());
        if (it == units.cend())
            return Socket::Unrecognized;
        const auto &values = it.value();
        if (runtimeMask && values.value("LoadState") == "masked" && values.value("UnitFileState") == "masked-runtime"
            && values.value("ActiveState") == "inactive")
            return Socket::RuntimeMasked;
        if (values.value("LoadState") != "loaded" || values.value("Accept") != "yes"
            || values.value("BindToDevice") != interface || values.value("Listen") != "[::]:22 (Stream)"
            || values.value("FragmentPath") != (unitDirectory + unit).toLatin1()
            || values.value("UnitFileState") != "enabled")
            return Socket::Unrecognized;
        return values.value("ActiveState") == "active" && values.value("SubState") == "listening"
            ? Socket::Listening : Socket::Inactive;
    };
    const auto first = socket(usb0, "usb0", usb0Masked);
    const auto second = socket(usb1, "usb1", usb1Masked);
    const auto wireless = socket(wlan, "wlan0");
    const auto usb = first == Socket::Unrecognized || second == Socket::Unrecognized ? State::Unavailable
        : first == Socket::RuntimeMasked && second == Socket::RuntimeMasked ? State::Disabled
        : first == Socket::Listening && second == Socket::Listening ? State::Enabled : State::Partial;
    const auto wifi = !wifiTool || wireless == Socket::Unrecognized ? State::Unavailable
        : (wireless == Socket::Listening) != wifiMarkerPresent ? State::Partial
        : wifiMarkerPresent ? State::Enabled : State::Disabled;
    return {wifi, usb};
}

void PaperProSshAccess::refresh()
{
    if (!m_available || m_locked || busy())
        return;
    m_message.clear();
    run(Step::Probe);
}

void PaperProSshAccess::setEnabled(const QString &key, bool enabled)
{
    if (!m_available || m_locked || busy())
        return;
    const auto target = key == QStringLiteral("wifi") ? Transport::WiFi
        : key == QStringLiteral("usb") ? Transport::Usb : Transport::None;
    if (target == Transport::None)
        return;
    m_target = target;
    m_targetEnabled = enabled;
    refresh();
}

void PaperProSshAccess::run(Step step)
{
    m_step = step;
    m_timedOut = false;
    const auto requested = command(step, m_targetEnabled);
    m_deadline.start(10000);
    m_process.start(requested.program, requested.arguments);
    emit changed();
}

void PaperProSshAccess::completed(bool success)
{
    m_deadline.stop();
    const auto output = m_process.readAllStandardOutput();
    if (m_step == Step::Probe || m_step == Step::Verify) {
        m_policy = success ? inspect(output, QFileInfo(wifiHelper).isExecutable(), QFileInfo::exists(wifiMarker),
            verifiedRuntimeMask(usb0), verifiedRuntimeMask(usb1)) : Policy{};
        if (!success) {
            finish(tr("Impossibile leggere lo stato SSH del Paper Pro."));
            return;
        }
        if (m_target == Transport::None || (m_step == Step::Probe && m_locked)) {
            finish();
            return;
        }
        const auto state = m_target == Transport::WiFi ? m_policy.wifi : m_policy.usb;
        const auto expected = m_targetEnabled ? State::Enabled : State::Disabled;
        if (m_step == Step::Verify) {
            finish(state == expected && m_writeSucceeded ? tr("Impostazione SSH verificata.")
                : tr("Il firmware non ha confermato la modifica SSH."));
        } else if (state == State::Unavailable || state == State::Unknown) {
            finish(tr("Il collegamento SSH non può essere modificato."));
        } else if (state == expected) {
            finish();
        } else {
            run(m_target == Transport::WiFi ? Step::WiFi : m_targetEnabled ? Step::UnmaskUsb : Step::MaskUsb);
        }
    } else if (m_step == Step::UnmaskUsb && success) {
        run(Step::StartUsb);
    } else {
        m_writeSucceeded = success;
        run(Step::Verify);
    }
}

void PaperProSshAccess::finish(const QString &message)
{
    m_target = Transport::None;
    m_step = Step::Idle;
    m_message = message;
    emit changed();
}
