#include "appleshare.h"
#include "applanguage.h"
#include "bluetoothsettings.h"
#include "deviceinfo.h"
#include "epaperrefresh.h"
#include "externalkeyboard.h"
#include "macdocumentexchange.h"
#include "markdownweb.h"
#include "notebook.h"
#include "obsidianintegration.h"
#include "obsidiansync.h"
#include "pinlock.h"
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
        const bool apply = argc == 3 && command == "--usb-transfer-apply";
        const QByteArray token = apply ? QByteArray(argv[2]) : QByteArray{};
        if ((!preview && !apply) || (apply && (token.size() != 64 || QByteArray::fromHex(token).toHex() != token)))
            return 2;

        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("Quaderno"));
        const QDir dataDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
        Notebook notebook(dataDir.filePath(QStringLiteral("quaderni.json")));
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
    EpaperRefresh epaperRefresh;
    ExternalKeyboard externalKeyboard(onDevice);
    WifiSettings wifi(onDevice);
    UsbSsh usbSsh(onDevice);
    const bool verifiedPaperPro = deviceInfo.paperPro() && deviceInfo.osVersion() == QStringLiteral("3.29.1.0");
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
        {QStringLiteral("appleShare"), QVariant::fromValue(&appleShare)},
        {QStringLiteral("pinLock"), QVariant::fromValue(&pinLock)},
        {QStringLiteral("deviceInfo"), QVariant::fromValue(&deviceInfo)},
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
