#include "appleshare.h"
#include "applanguage.h"
#include "bluetoothsettings.h"
#include "deviceinfo.h"
#include "cloudaccounts.h"
#include "epaperrefresh.h"
#include "externalkeyboard.h"
#include "macdocumentexchange.h"
#include "markdownweb.h"
#include "notebook.h"
#include "obsidianintegration.h"
#include "obsidiansync.h"
#include "pinlock.h"
#include "paperprostandby.h"
#include "screenorientation.h"
#include "usbssh.h"
#include "wifisettings.h"
#include "wirelesssync.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QStandardPaths>
#include <QStyleHints>
#include <QVariant>

#include <cstdio>

int main(int argc, char *argv[])
{
    if (argc >= 2 && QByteArray(argv[1]).startsWith("--usb-transfer-")) {
        const QByteArray command(argv[1]);
        if (argc == 2 && command == "--usb-transfer-protocol") {
            std::puts("1");
            return 0;
        }
        const bool preview = argc == 2 && command == "--usb-transfer-preview";
        const bool exportLibrary = argc == 2 && command == "--usb-transfer-export";
        const bool apply = argc == 3 && command == "--usb-transfer-apply";
        const QByteArray token = apply ? QByteArray(argv[2]) : QByteArray{};
        if ((!preview && !apply && !exportLibrary) || (apply && (token.size() != 64 || QByteArray::fromHex(token).toHex() != token)))
            return 2;

        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("Quaderno"));
        const QDir dataDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
        Notebook notebook(dataDir.filePath(QStringLiteral("quaderni.json")));
        if (exportLibrary) {
            const QByteArray exported = notebook.exportLibraryForTransfer();
            if (exported.isEmpty()) return 1;
            return std::fwrite(exported.constData(), 1, size_t(exported.size()), stdout) == size_t(exported.size()) ? 0 : 1;
        }
        QByteArray payload;
        char buffer[65536];
        while (payload.size() <= 64 * 1024 * 1024) {
            const auto size = std::fread(buffer, 1, sizeof(buffer), stdin);
            payload.append(buffer, static_cast<qsizetype>(size));
            if (size < sizeof(buffer))
                break;
        }
        if (std::ferror(stdin))
            return 2;
        const QVariantMap result = notebook.receiveBookTransfer(payload, token);
        const QByteArray json = QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact);
        if (std::fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout) != static_cast<size_t>(json.size()))
            return 2;
        return result.contains(QStringLiteral("error")) ? 1 : 0;
    }

#ifdef Q_OS_MACOS
    if (argc == 2 && qEnvironmentVariable("QUADERNO_USB_ASKPASS") == QStringLiteral("1")) {
        QCoreApplication app(argc, argv);
        AppLanguage language(nullptr);
        return runSshAskpass(QString::fromLocal8Bit(argv[1]));
    }
#endif

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Quaderno"));
    AppLanguage language(nullptr);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    const bool onDevice = QGuiApplication::platformName() == QStringLiteral("epaper");
    if (onDevice)
        QGuiApplication::styleHints()->setCursorFlashTime(0);
    const bool tablet = onDevice || app.arguments().contains(QStringLiteral("--tablet"));
    const QScreen *epaperScreen = onDevice ? app.primaryScreen() : nullptr;
    const QRect epaperGeometry = epaperScreen ? epaperScreen->geometry() : QRect{};
    const QDir dataDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    Notebook notebook(dataDir.filePath(QStringLiteral("quaderni.json")));
    AppleShare appleShare(tablet);
    PinLock pinLock(dataDir.filePath(QStringLiteral("accesso.json")));
    CloudAccounts cloudAccounts(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/cloud"), tablet);
    cloudAccounts.setLocked(pinLock.locked());
    MacDocumentExchange documentExchange(&notebook, tablet);
    MarkdownWeb markdownWeb(&notebook);
    ObsidianIntegration obsidian;
    ObsidianSync obsidianSync(&notebook, QStringLiteral("/home/root/quaderno/obsidian-sync"), onDevice);
    documentExchange.setLocked(pinLock.locked());
    markdownWeb.setLocked(pinLock.locked());
    obsidianSync.setLocked(pinLock.locked());
    QObject::connect(&pinLock, &PinLock::lockedChanged, &documentExchange, [&] {
        documentExchange.setLocked(pinLock.locked());
        markdownWeb.setLocked(pinLock.locked());
        obsidianSync.setLocked(pinLock.locked());
        if (pinLock.locked())
            obsidian.clearLastSavedNote();
    });
    DeviceInfo deviceInfo(onDevice);
    ScreenOrientation screenOrientation(onDevice);
    PaperProStandby standby(tablet, &pinLock, &deviceInfo);
    const auto updateCloudLock = [&] { cloudAccounts.setLocked(pinLock.locked() || standby.sleeping()); };
    QObject::connect(&pinLock, &PinLock::lockedChanged, &cloudAccounts, updateCloudLock);
    QObject::connect(&standby, &PaperProStandby::sleepingChanged, &cloudAccounts, updateCloudLock);
    EpaperRefresh epaperRefresh;
    ExternalKeyboard externalKeyboard(onDevice);
    WifiSettings wifi(onDevice);
    const bool verifiedPaperPro = deviceInfo.paperPro() && deviceInfo.osVersion() == QStringLiteral("3.29.1.0");
    UsbSsh usbSsh(onDevice, verifiedPaperPro);
    const auto updateSshLock = [&] { usbSsh.access()->setLocked(pinLock.locked() || standby.sleeping()); };
    QObject::connect(&pinLock, &PinLock::lockedChanged, usbSsh.access(), updateSshLock);
    QObject::connect(&standby, &PaperProStandby::sleepingChanged, usbSsh.access(), updateSshLock);
    updateSshLock();
    BluetoothSettings bluetooth(onDevice, verifiedPaperPro);
    bluetooth.setLocked(pinLock.locked());
    QObject::connect(&pinLock, &PinLock::lockedChanged, &bluetooth, [&] {
        bluetooth.setLocked(pinLock.locked());
    });
    WirelessSync wireless(&notebook, dataDir.filePath(QStringLiteral("wireless-pairing.key")), tablet, verifiedPaperPro);
    QObject::connect(&pinLock, &PinLock::lockedChanged, &wireless, [&] {
        wireless.setLocked(pinLock.locked());
    });
    wireless.setLocked(pinLock.locked());

    const QString preferredFont = QStringLiteral("Source Sans 3");
    const QString fontFamily = tablet && QFontDatabase::hasFamily(preferredFont)
        ? preferredFont : app.font().family();

    QQmlApplicationEngine engine;
    QVariantMap initialProperties{
        {QStringLiteral("notebook"), QVariant::fromValue(&notebook)},
        {QStringLiteral("cloudAccountsModel"), QVariant::fromValue(&cloudAccounts)},
        {QStringLiteral("appleShare"), QVariant::fromValue(&appleShare)},
        {QStringLiteral("pinLock"), QVariant::fromValue(&pinLock)},
        {QStringLiteral("standby"), QVariant::fromValue(&standby)},
        {QStringLiteral("deviceInfo"), QVariant::fromValue(&deviceInfo)},
        {QStringLiteral("screenOrientationModel"), QVariant::fromValue(&screenOrientation)},
        {QStringLiteral("wifi"), QVariant::fromValue(&wifi)},
        {QStringLiteral("bluetooth"), QVariant::fromValue(&bluetooth)},
        {QStringLiteral("usbSsh"), QVariant::fromValue(&usbSsh)},
        {QStringLiteral("wireless"), QVariant::fromValue(&wireless)},
        {QStringLiteral("documentExchange"), QVariant::fromValue(&documentExchange)},
        {QStringLiteral("markdownWeb"), QVariant::fromValue(&markdownWeb)},
        {QStringLiteral("obsidian"), QVariant::fromValue(&obsidian)},
        {QStringLiteral("obsidianSync"), QVariant::fromValue(&obsidianSync)},
        {QStringLiteral("epaperRefresh"), QVariant::fromValue(&epaperRefresh)},
        {QStringLiteral("externalKeyboard"), QVariant::fromValue(&externalKeyboard)},
        {QStringLiteral("tabletMode"), tablet},
        {QStringLiteral("epaperGeometry"), epaperGeometry},
        {QStringLiteral("fontFamily"), fontFamily},
    };
    if (epaperGeometry.isValid()) {
        initialProperties.insert(QStringLiteral("x"), epaperGeometry.x());
        initialProperties.insert(QStringLiteral("y"), epaperGeometry.y());
    }
    engine.setInitialProperties(initialProperties);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("Quaderno"), QStringLiteral("Main"));
    if (app.arguments().contains(QStringLiteral("--check-startup"))) {
        if (engine.rootObjects().isEmpty())
            return 1;
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        if (!window || window->width() < 380 || window->height() < 500)
            return 1;
        // An alive process is insufficient: require a rendered frame and a
        // subsequent event-loop callback. A blocked UI never reaches ready.
        QObject::connect(window, &QQuickWindow::frameSwapped, &app, [&, window] {
            if (!window->isVisible() || engine.rootObjects().isEmpty())
                return;
            QTimer::singleShot(0, &app, [&] {
                std::puts("QUADERNO_READY");
                std::fflush(stdout);
                app.exit(0);
            });
        }, Qt::QueuedConnection);
        QTimer::singleShot(8000, &app, [&] { app.exit(3); });
        window->update();
    }
    if (epaperScreen && !engine.rootObjects().isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        if (window) {
            qInfo() << "E-paper window" << window->geometry() << "screen" << epaperGeometry;
            if (verifiedPaperPro)
                epaperRefresh.attach(window);
        }
    }

    return app.exec();
}
