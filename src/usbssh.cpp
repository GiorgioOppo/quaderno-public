#include "usbssh.h"

#include "notebook.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QGuiApplication>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QProcessEnvironment>
#include <QUrl>

namespace {
const QHostAddress tabletAddress(QStringLiteral("10.11.99.1"));
const QHostAddress usbSubnet(QStringLiteral("10.11.99.0"));
constexpr quint16 sshPort = 22;
constexpr qint64 bannerLimit = 2048;
constexpr qsizetype importLimit = 64 * 1024 * 1024;
constexpr qsizetype diagnosticLimit = 4096;

#ifdef Q_OS_MACOS
struct UsbRoute {
    QString interface;
    QString address;
};

UsbRoute locateRoute()
{
    UsbRoute route;
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack) || interface.type() == QNetworkInterface::Wifi)
            continue;
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip == tabletAddress || !ip.isInSubnet(usbSubnet, 24) || entry.prefixLength() <= 0
                || !tabletAddress.isInSubnet(ip, entry.prefixLength()))
                continue;
            if (!route.interface.isEmpty() && route.interface != interface.name())
                return {};
            if (route.interface.isEmpty())
                route = {interface.name(), ip.toString()};
        }
    }
    return route;
}
#endif
}

#ifdef Q_OS_MACOS
QStringList UsbConnection::arguments(const QString &remote)
{
    const UsbRoute route = locateRoute();
    if (route.interface.isEmpty())
        return {};
    return {
        QStringLiteral("-T"), QStringLiteral("-4"), QStringLiteral("-F"), QStringLiteral("/dev/null"),
        QStringLiteral("-o"), QStringLiteral("StrictHostKeyChecking=yes"),
        QStringLiteral("-o"), QStringLiteral("UpdateHostKeys=no"),
        QStringLiteral("-o"), QStringLiteral("ProxyCommand=none"),
        QStringLiteral("-o"), QStringLiteral("ProxyJump=none"),
        QStringLiteral("-o"), QStringLiteral("ControlMaster=no"),
        QStringLiteral("-o"), QStringLiteral("ControlPath=none"),
        QStringLiteral("-o"), QStringLiteral("ClearAllForwardings=yes"),
        QStringLiteral("-o"), QStringLiteral("ForwardAgent=no"),
        QStringLiteral("-o"), QStringLiteral("ForwardX11=no"),
        QStringLiteral("-o"), QStringLiteral("NumberOfPasswordPrompts=1"),
        QStringLiteral("-o"), QStringLiteral("ConnectionAttempts=1"),
        QStringLiteral("-o"), QStringLiteral("ConnectTimeout=8"),
        QStringLiteral("-o"), QStringLiteral("ServerAliveInterval=10"),
        QStringLiteral("-o"), QStringLiteral("ServerAliveCountMax=3"),
        QStringLiteral("-B"), route.interface, QStringLiteral("-b"), route.address,
        QStringLiteral("root@") + tabletAddress.toString(), remote
    };
}

QProcessEnvironment UsbConnection::environment()
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("SSH_ASKPASS"), QCoreApplication::applicationFilePath());
    environment.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
    environment.insert(QStringLiteral("QUADERNO_USB_ASKPASS"), QStringLiteral("1"));
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    return environment;
}

QString UsbConnection::deviceCheck()
{
    return QStringLiteral(R"SH(set -eu
[ "$(id -u)" = 0 ] && [ "$(uname -m)" = aarch64 ] || exit 43
model=$(cat /sys/devices/soc0/machine 2>/dev/null || true)
case "$model" in
    *Ferrari*|*ferrari*|'reMarkable Paper Pro') ;;
    *) model=$(tr -d '\000' < /proc/device-tree/model) || exit 43 ;;
esac
case "$model" in *Ferrari*|*ferrari*|'reMarkable Paper Pro') ;; *) exit 43 ;; esac
)SH");
}
#endif

UsbSsh::UsbSsh(bool onTablet, QObject *parent) : QObject(parent), m_onTablet(onTablet)
{
    localUsbAddress();
    m_status = tr("Premi Verifica per controllare il servizio SSH.");
    m_timeout.setSingleShot(true);
    m_socket.setProxy(QNetworkProxy::NoProxy);
    m_socket.setReadBufferSize(bannerLimit);

    connect(&m_timeout, &QTimer::timeout, this, [this] {
        finish(tr("Nessuna risposta SSH sulla porta 22."));
    });
    connect(&m_socket, &QTcpSocket::readyRead, this, &UsbSsh::readBanner);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        if (!m_busy)
            return;
        finish(error == QAbstractSocket::ConnectionRefusedError
                   ? tr("La porta 22 ha rifiutato la connessione.")
                   : tr("Servizio SSH non raggiungibile sulla porta 22."));
    });

    m_importTimeout.setSingleShot(true);
    connect(&m_importTimeout, &QTimer::timeout, this, [this] {
        if (m_importFailure == ImportFailure::None)
            m_importFailure = ImportFailure::TimedOut;
        m_importProcess.kill();
    });
    connect(&m_importProcess, &QProcess::started, this, [this] {
        if (m_importFailure == ImportFailure::Cancelled)
            m_importProcess.kill();
    });
    connect(&m_importProcess, &QProcess::readyReadStandardOutput, this, &UsbSsh::readImportOutput);
    connect(&m_importProcess, &QProcess::readyReadStandardError, this, &UsbSsh::readImportDiagnostic);
    connect(&m_importProcess, &QProcess::finished, this, &UsbSsh::finishImport);
    connect(&m_importProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_importTimeout.stop();
        m_importing = false;
        clearImportPreview();
        if (m_importFailure != ImportFailure::Cancelled)
            m_importStatus = tr("Impossibile avviare il client SSH del Mac.");
        emit changed();
    });
}

UsbSsh::~UsbSsh()
{
    if (m_importProcess.state() != QProcess::NotRunning) {
        disconnect(&m_importProcess, nullptr, this, nullptr);
        m_importProcess.kill();
        m_importProcess.waitForFinished(1000);
    }
}

QString UsbSsh::command() const
{
    return QStringLiteral("ssh root@") + tabletAddress.toString();
}

bool UsbSsh::canOpenTerminal() const
{
#ifdef Q_OS_MACOS
    return !m_onTablet;
#else
    return false;
#endif
}

QHostAddress UsbSsh::localUsbAddress()
{
    m_linkStatus = m_onTablet ? tr("Interfaccia USB del tablet non rilevata.")
                              : tr("Nessuna interfaccia attiva sulla sottorete USB del tablet.");
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        if (!interface.flags().testFlag(QNetworkInterface::IsUp)
            || interface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (m_onTablet && address == tabletAddress) {
                m_linkStatus = tr("Interfaccia USB del tablet rilevata.");
#ifdef Q_OS_LINUX
                QFile carrier(QStringLiteral("/sys/class/net/%1/carrier").arg(interface.name()));
                if (carrier.open(QIODevice::ReadOnly)) {
                    const QByteArray state = carrier.readLine().trimmed();
                    if (state == "1")
                        m_linkStatus = tr("Collegamento USB attivo.");
                    else if (state == "0")
                        m_linkStatus = tr("Interfaccia USB pronta. Collega un cavo dati.");
                }
#endif
                return address;
            }
            if (!m_onTablet && address != tabletAddress && address.isInSubnet(usbSubnet, 24)
                && entry.prefixLength() > 0 && tabletAddress.isInSubnet(address, entry.prefixLength())) {
                m_linkStatus = tr("Interfaccia sulla sottorete USB rilevata (%1).").arg(address.toString());
                return address;
            }
        }
    }
    return {};
}

void UsbSsh::refresh()
{
    if (m_busy)
        return;

    const QHostAddress source = localUsbAddress();
    if (source.isNull()) {
        m_status = tr("Collega il Paper Pro con un cavo USB‑C per dati e riprova.");
        emit changed();
        return;
    }

    m_socket.abort();
    if (!m_onTablet && !m_socket.bind(source)) {
        m_status = tr("Impossibile usare l’interfaccia USB rilevata.");
        emit changed();
        return;
    }

    m_received = 0;
    m_busy = true;
    m_status = tr("Verifica della porta SSH in corso…");
    m_timeout.start(3000);
    emit changed();
    m_socket.connectToHost(tabletAddress, sshPort);
}

void UsbSsh::readBanner()
{
    if (!m_busy)
        return;
    while (m_socket.canReadLine()) {
        const QByteArray line = m_socket.readLine(bannerLimit - m_received + 1);
        m_received += line.size();
        if (line.endsWith('\n') && (line.startsWith("SSH-2.0-") || line.startsWith("SSH-1.99-"))) {
            finish(m_onTablet ? tr("Servizio SSH attivo sull’indirizzo USB del tablet.")
                              : tr("Il servizio SSH risponde sulla porta 22."));
            return;
        }
        if (m_received >= bannerLimit)
            break;
    }
    if (m_received + m_socket.bytesAvailable() >= bannerLimit)
        finish(tr("La porta 22 ha risposto senza un’intestazione SSH valida."));
}

void UsbSsh::finish(const QString &status)
{
    m_busy = false;
    m_timeout.stop();
    m_socket.abort();
    m_status = status;
    emit changed();
}

void UsbSsh::copyCommand()
{
    QGuiApplication::clipboard()->setText(command());
}

bool UsbSsh::openTerminal()
{
    return canOpenTerminal()
        && QDesktopServices::openUrl(QUrl(QStringLiteral("ssh://root@10.11.99.1")));
}

void UsbSsh::importFromPaperPro(Notebook *notebook)
{
#ifdef Q_OS_MACOS
    if (m_onTablet || !notebook || importBusy())
        return;

    clearImportPreview();
    m_importCompleted = false;
    const QString remote = UsbConnection::deviceCheck() + QStringLiteral(R"SH(file=/home/root/.local/share/Quaderno/quaderni.json
[ -f "$file" ] && [ ! -L "$file" ] && [ -r "$file" ] || exit 42
exec cat "$file"
)SH");
    const QStringList arguments = UsbConnection::arguments(remote);
    if (arguments.isEmpty()) {
        m_importStatus = tr("Collega il Paper Pro con un cavo USB‑C per dati.");
        emit changed();
        return;
    }

    m_importing = true;
    m_importFailure = ImportFailure::None;
    m_importNotebook = notebook;
    m_importStatus = tr("Connessione al Paper Pro…");
    m_importProcess.setProcessEnvironment(UsbConnection::environment());
    m_importProcess.setStandardInputFile(QProcess::nullDevice());
    m_importTimeout.start(180000);
    m_importProcess.start(QStringLiteral("/usr/bin/ssh"), arguments);
    emit changed();
#else
    Q_UNUSED(notebook)
#endif
}

void UsbSsh::readImportOutput()
{
    const QByteArray chunk = m_importProcess.readAllStandardOutput();
    if (m_importFailure != ImportFailure::None)
        return;
    if (chunk.size() > importLimit - m_importPayload.size()) {
        m_importFailure = ImportFailure::Oversized;
        m_importProcess.kill();
        return;
    }
    m_importPayload.append(chunk);
}

void UsbSsh::readImportDiagnostic()
{
    const QByteArray chunk = m_importProcess.readAllStandardError();
    m_importDiagnostic.append(chunk.first(qMin(chunk.size(), diagnosticLimit - m_importDiagnostic.size())));
}

void UsbSsh::finishImport(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_importTimeout.stop();
    readImportOutput();
    readImportDiagnostic();
    m_importing = false;
    m_importStatus.clear();
    if (m_importFailure == ImportFailure::Oversized) {
        m_importStatus = tr("La libreria del Paper Pro supera 64 MB.");
    } else if (m_importFailure == ImportFailure::TimedOut) {
        m_importStatus = tr("Il collegamento SSH è scaduto.");
    } else if (m_importFailure == ImportFailure::Cancelled) {
        clearImportPreview();
        emit changed();
        return;
    } else if (exitStatus == QProcess::NormalExit && exitCode == 0 && m_importNotebook) {
        m_importPreview = m_importNotebook->previewImportFromData(m_importPayload);
        m_importStatus = m_importPreview.take(QStringLiteral("error")).toString();
        m_importToken = m_importPreview.take(QStringLiteral("token")).toString();
    } else if (exitCode == 42) {
        m_importStatus = tr("Sul Paper Pro non ci sono ancora quaderni salvati da Quaderno.");
    } else if (exitCode == 43) {
        m_importStatus = tr("Il dispositivo collegato non è un reMarkable Paper Pro.");
    } else if (m_importDiagnostic.contains("Host key verification failed")
               || m_importDiagnostic.contains("REMOTE HOST IDENTIFICATION HAS CHANGED")) {
        m_importStatus = tr("Verifica l’identità del Paper Pro nel Terminale con ssh root@10.11.99.1.");
    } else {
        m_importStatus = tr("Accesso SSH non riuscito. Controlla la modalità sviluppatore e la password.");
    }
    if (m_importToken.isEmpty())
        clearImportPreview();
    emit changed();
}

void UsbSsh::confirmImport()
{
    if (importBusy() || !m_importNotebook || m_importToken.isEmpty())
        return;
    const QVariantMap result = m_importNotebook->applyImportFromData(m_importPayload, m_importToken);
    clearImportPreview();
    m_importStatus = result.value(QStringLiteral("error")).toString();
    m_importCompleted = m_importStatus.isEmpty();
    if (m_importCompleted)
        m_importStatus = tr("Libreria del Paper Pro importata.");
    emit changed();
}

void UsbSsh::cancelImport()
{
    m_importCompleted = false;
    m_importStatus.clear();
    if (importBusy()) {
        m_importFailure = ImportFailure::Cancelled;
        m_importProcess.kill();
    }
    clearImportPreview();
    emit changed();
}

void UsbSsh::clearImportPreview()
{
    m_importNotebook.clear();
    m_importPayload.clear();
    m_importDiagnostic.clear();
    m_importToken.clear();
    m_importPreview.clear();
}
