#include "usbtransfer.h"

#include "notebook.h"
#include "usbssh.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <utility>

namespace {
constexpr qsizetype payloadLimit = 64 * 1024 * 1024;
constexpr qsizetype replyLimit = 8192;
constexpr qsizetype diagnosticLimit = 4096;
constexpr int timeoutMs = 180000;
constexpr int wrongDevice = 43;
constexpr int unsupportedVersion = 44;
constexpr int deviceBusy = 45;

#ifdef Q_OS_MACOS
const QString tabletLauncher = QStringLiteral(R"SH(base=/home/root/quaderno
pointer="$base/usb-transfer-installation"
[ -d "$base" ] && [ ! -L "$base" ] && [ -f "$pointer" ] && [ ! -L "$pointer" ] || exit 44
IFS= read -r release < "$pointer" || exit 44
case "$release" in "$base"/installation-?*) ;; *) exit 44 ;; esac
suffix=${release#"$base"/installation-}
case "$suffix" in *[!a-zA-Z0-9]*) exit 44 ;; esac
app="$release/quaderno"
[ -d "$release" ] && [ ! -L "$release" ] && [ -f "$app" ] && [ ! -L "$app" ] && [ -x "$app" ] || exit 44
export HOME=/home/root XDG_DATA_HOME=/home/root/.local/share QT_QPA_PLATFORM=offscreen
unset QUADERNO_USB_ASKPASS
umask 077
[ "$("$app" --usb-transfer-protocol < /dev/null)" = 1 ] || exit 44
)SH");
#endif

bool validAction(const QString &action)
{
    return action == QStringLiteral("unchanged") || action == QStringLiteral("create")
        || action == QStringLiteral("replace") || action == QStringLiteral("conflict");
}

QString limitedText(const QJsonValue &value)
{
    QString result = value.toString().left(360);
    result.remove(QRegularExpression(QStringLiteral("[\\x{0000}-\\x{001f}\\x{007f}-\\x{009f}\\x{202a}-\\x{202e}\\x{2066}-\\x{2069}]")));
    return result;
}
}

UsbTransfer::UsbTransfer(QObject *parent) : QObject(parent)
{
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        m_failure = Failure::TimedOut;
        m_process.kill();
    });
    connect(&m_process, &QProcess::started, this, [this] {
        if (m_failure == Failure::Cancelled) {
            m_process.kill();
            return;
        }
        if (m_process.write(m_payload) != m_payload.size()) {
            m_failure = Failure::Input;
            m_process.kill();
            return;
        }
        m_process.closeWriteChannel();
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &UsbTransfer::readOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, &UsbTransfer::readDiagnostic);
    connect(&m_process, &QProcess::finished, this, &UsbTransfer::finished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && busy()) {
            m_deadline.stop();
            m_stage = Stage::Idle;
            resetPreview();
            fail(tr("Impossibile avviare il client SSH del Mac."));
        }
    });
}

UsbTransfer::~UsbTransfer()
{
    if (m_process.state() != QProcess::NotRunning) {
        disconnect(&m_process, nullptr, this, nullptr);
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

bool UsbTransfer::available() const
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

void UsbTransfer::preview(Notebook *notebook, const QString &bookId)
{
    if (busy())
        return;
    resetPreview();
    m_completed = false;
    if (!available() || !notebook || bookId.isEmpty()) {
        fail(tr("Scegli un quaderno da inviare dal Mac."));
        return;
    }
    m_payload = notebook->exportBookForTransfer(bookId);
    if (m_payload.isEmpty() || m_payload.size() > payloadLimit) {
        const QString error = notebook->error();
        resetPreview();
        fail(error.isEmpty() ? tr("Non è possibile inviare un quaderno di oltre 64 MiB.") : error);
        return;
    }
    m_source = notebook;
    m_bookId = bookId;
    start(Stage::Preview, QStringLiteral("exec \"$app\" --usb-transfer-preview"));
}

void UsbTransfer::apply()
{
    if (busy() || m_token.isEmpty())
        return;
    if (!m_source || m_source->exportBookForTransfer(m_bookId) != m_payload) {
        resetPreview();
        fail(tr("Il quaderno sul Mac è cambiato. Verifica di nuovo prima di inviarlo."));
        return;
    }
    const QString remote = QStringLiteral(R"SH(command -v systemctl >/dev/null || exit 44
if systemctl is-active --quiet quaderno-demo.service || systemctl is-active --quiet quaderno-launch.service; then exit 45; fi
exec "$app" --usb-transfer-apply '%1')SH").arg(m_token);
    start(Stage::Apply, remote);
}

void UsbTransfer::cancel()
{
    if (applying())
        return;
    if (busy()) {
        m_failure = Failure::Cancelled;
        m_process.kill();
    } else {
        resetPreview();
        m_status.clear();
        m_completed = false;
        emit changed();
    }
}

void UsbTransfer::start(Stage stage, const QString &remote)
{
#ifdef Q_OS_MACOS
    const QStringList arguments = UsbConnection::arguments(UsbConnection::deviceCheck() + tabletLauncher + remote);
    if (arguments.isEmpty()) {
        resetPreview();
        fail(tr("Collega il Paper Pro con un cavo USB-C per dati e verifica l’interfaccia di rete USB."));
        return;
    }
    m_stage = stage;
    m_failure = Failure::None;
    m_output.clear();
    m_diagnostic.clear();
    m_status = stage == Stage::Preview ? tr("Controllo della versione sul Paper Pro…")
                                      : tr("Invio del quaderno al Paper Pro…");
    m_process.setProcessEnvironment(UsbConnection::environment());
    m_deadline.start(timeoutMs);
    m_process.start(QStringLiteral("/usr/bin/ssh"), arguments);
    emit changed();
#else
    Q_UNUSED(stage)
    Q_UNUSED(remote)
    resetPreview();
    fail(tr("Il trasferimento USB è disponibile sul Mac."));
#endif
}

void UsbTransfer::readOutput()
{
    const QByteArray chunk = m_process.readAllStandardOutput();
    if (chunk.size() <= replyLimit - m_output.size()) {
        m_output.append(chunk);
    } else if (m_failure == Failure::None) {
        m_failure = Failure::Oversized;
        m_process.kill();
    }
}

void UsbTransfer::readDiagnostic()
{
    const QByteArray chunk = m_process.readAllStandardError();
    m_diagnostic.append(chunk.first(qMin(chunk.size(), diagnosticLimit - m_diagnostic.size())));
}

void UsbTransfer::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_deadline.stop();
    readOutput();
    readDiagnostic();
    const Stage stage = std::exchange(m_stage, Stage::Idle);
    const QJsonObject result = QJsonDocument::fromJson(m_output).object();
    const QString remoteError = limitedText(result.value(QStringLiteral("error")));
    const QString action = result.value(QStringLiteral("action")).toString();
    const QString token = result.value(QStringLiteral("token")).toString();
    const QString revision = result.value(QStringLiteral("revision")).toString();
    const QString bookId = result.value(QStringLiteral("bookId")).toString();
    const int sourcePages = result.value(QStringLiteral("sourcePages")).toInt(-1);
    const int targetPages = result.value(QStringLiteral("targetPages")).toInt(-1);
    const int changedPages = result.value(QStringLiteral("changedPages")).toInt();
    static const QRegularExpression tokenFormat(QStringLiteral("^[0-9a-f]{64}$"));
    QString failure;
    if (m_failure == Failure::Cancelled) {
        resetPreview();
        m_status.clear();
        emit changed();
        return;
    }
    if (m_failure == Failure::TimedOut)
        failure = tr("Il collegamento SSH è scaduto.");
    else if (m_failure == Failure::Oversized || m_failure == Failure::Input)
        failure = tr("Lo scambio con il Paper Pro è incompleto.");
    else if (exitCode == wrongDevice)
        failure = tr("Il dispositivo collegato non è un reMarkable Paper Pro.");
    else if (exitCode == unsupportedVersion)
        failure = tr("Installa sul Paper Pro una versione di Quaderno compatibile con il trasferimento USB.");
    else if (exitCode == deviceBusy)
        failure = tr("Chiudi Quaderno sul Paper Pro e verifica di nuovo prima di inviare.");
    else if (m_diagnostic.contains("Host key verification failed") || m_diagnostic.contains("REMOTE HOST IDENTIFICATION HAS CHANGED"))
        failure = tr("Verifica l’identità del Paper Pro nel Terminale con ssh root@10.11.99.1.");
    else if (!remoteError.isEmpty())
        failure = remoteError;
    else if (exitStatus != QProcess::NormalExit || exitCode != 0 || !validAction(action)
             || bookId != m_bookId || !tokenFormat.match(revision).hasMatch())
        failure = tr("Accesso SSH non riuscito o risposta del Paper Pro non valida.");
    else if (stage == Stage::Preview && (!tokenFormat.match(token).hasMatch() || sourcePages < 1
             || targetPages < 0 || changedPages < 0 || changedPages > qMax(sourcePages, targetPages)))
        failure = tr("L’anteprima del trasferimento non è valida.");
    else if (stage == Stage::Apply && (action != m_preview.value(QStringLiteral("action")).toString() || revision != m_revision))
        failure = tr("Il Paper Pro ha restituito un esito inatteso.");

    if (!failure.isEmpty()) {
        const bool uncertain = stage == Stage::Apply && remoteError.isEmpty()
            && exitCode != wrongDevice && exitCode != unsupportedVersion && exitCode != deviceBusy;
        resetPreview();
        fail(uncertain ? failure + QLatin1Char(' ') + tr("Verifica la libreria del Paper Pro prima di riprovare.") : failure);
        return;
    }
    if (stage == Stage::Apply) {
        bool localRecorded = true;
        if (action != QStringLiteral("conflict"))
            localRecorded = m_source && m_source->recordBookTransfer(bookId, revision);
        resetPreview();
        m_completed = true;
        if (action == QStringLiteral("conflict"))
            m_status = tr("Quaderno inviato come copia separata.");
        else if (action == QStringLiteral("unchanged"))
            m_status = tr("Il quaderno è già aggiornato sul Paper Pro.");
        else
            m_status = tr("Quaderno inviato al Paper Pro.");
        if (!localRecorded)
            m_status += QLatin1Char(' ') + tr("Impossibile salvare la conferma nella libreria del Mac.");
    } else {
        m_token = token;
        m_revision = revision;
        m_preview = {
            {QStringLiteral("action"), action},
            {QStringLiteral("sourceTitle"), limitedText(result.value(QStringLiteral("sourceTitle")))},
            {QStringLiteral("sourcePages"), sourcePages},
            {QStringLiteral("targetTitle"), limitedText(result.value(QStringLiteral("targetTitle")))},
            {QStringLiteral("targetPages"), targetPages},
            {QStringLiteral("changedPages"), changedPages}
        };
        m_status.clear();
    }
    emit changed();
}

void UsbTransfer::resetPreview()
{
    m_source = nullptr;
    m_payload.clear();
    m_bookId.clear();
    m_token.clear();
    m_revision.clear();
    m_preview.clear();
}

void UsbTransfer::fail(const QString &message)
{
    m_status = message;
    emit changed();
}
