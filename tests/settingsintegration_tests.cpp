#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <memory>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSValue>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QPointingDevice>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QQmlError>
#include <QStyleHints>
#include <QTabletEvent>
#include <QTemporaryDir>
#include <QTransform>
#include <QUuid>
#include "applanguage.h"
#include "appleshare.h"
#include "bluetoothsettings.h"
#include "blesync.h"
#include "chartitem.h"
#include "chartrecognizer.h"
#include "deviceinfo.h"
#include "diagramexport.h"
#include "diagramitem.h"
#include "epaperrefresh.h"
#include "externalkeyboard.h"
#include "handwriting.h"
#include "inkitem.h"
#include "inkselection.h"
#include "macdocumentexchange.h"
#include "markdownweb.h"
#include "notebook.h"
#include "obsidianintegration.h"
#include "obsidiansync.h"
#include "mixedpageanalyzer.h"
#include "pagepreviewitem.h"
#include "pagetemplateitem.h"
#include "paperprostandby.h"
#include "pinlock.h"
#include "tabletpreferences.h"
#include "librarymaintenance.h"
#include "portabledocuments.h"
#include "cloudaccounts.h"
#include "usbssh.h"
#include "usbtransfer.h"
#include "wifisettings.h"
#include "wirelesssync.h"

#include "shaperecognition.h"
#include <cmath>
#include <limits>

namespace {
QQuickItem *visibleItem(QQuickItem *parent, const QString &name)
{
    if (parent->objectName() == name && parent->isVisible()) return parent;
    for (QQuickItem *child : parent->childItems())
        if (auto *found = visibleItem(child, name)) return found;
    return nullptr;
}

bool hasVisibleText(const QQuickItem *parent, const QString &text)
{
    if (!parent->isVisible()) return false;
    if (parent->property("text").toString() == text) return true;
    for (const QQuickItem *child : parent->childItems())
        if (hasVisibleText(child, text)) return true;
    return false;
}

void stylusEvent(QQuickWindow *window, const QPointingDevice &device, QEvent::Type type, QPointF point, qreal pressure)
{
    const bool release = type == QEvent::TabletRelease;
    QTabletEvent event(type, &device, point, window->mapToGlobal(point), pressure, 0, 0, 0, 0, 0,
        Qt::NoModifier, type == QEvent::TabletMove ? Qt::NoButton : Qt::LeftButton,
        release ? Qt::NoButton : Qt::LeftButton);
    QCoreApplication::sendEvent(window, &event);
}

QVariantMap testStroke()
{
    return {{"tool", "pen"}, {"color", "#222222"}, {"width", 2.5},
        {"points", QVariantList{QVariantMap{{"x", .2}, {"y", .3}, {"p", .5}},
            QVariantMap{{"x", .4}, {"y", .5}, {"p", .5}}}}};
}

QString pairingPathFor(const QTemporaryDir &directory, bool paired)
{
    const QString path = directory.filePath("pairing.key");
    if (paired) {
        QFile key(path);
        if (key.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            key.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            key.write(QByteArray(16, 'k'));
        }
    }
    return path;
}

struct SettingsHarness {
    QTemporaryDir directory;
    AppLanguage language{nullptr};
    Notebook notebook{directory.filePath("notebooks.json")};
    AppleShare share{true};
    PinLock pin{directory.filePath("access.json")};
    DeviceInfo device{false};
    PaperProStandby standby{true, &pin, &device};
    EpaperRefresh refresh;
    ExternalKeyboard keyboard{false};
    WifiSettings wifi{false};
    UsbSsh ssh{false};
    BluetoothSettings bluetooth{false, false};
    WirelessSync wireless;
    MacDocumentExchange exchange{&notebook, true};
    MarkdownWeb markdown{&notebook};
    ObsidianIntegration obsidian;
    ObsidianSync sync{&notebook, directory.filePath("obsidian"), false};
    QNetworkAccessManager cloudNetwork;
    std::unique_ptr<CloudAccounts> cloud;
    QStringList qmlWarnings;
    QQmlApplicationEngine engine;

    explicit SettingsHarness(bool paired = false, bool withCloud = false)
        : wireless(&notebook, pairingPathFor(directory, paired), true, false)
    {
        notebook.createBook("Settings regression");
        if (withCloud) cloud = std::make_unique<CloudAccounts>(directory.filePath("cloud"), true, nullptr, &cloudNetwork);
        QObject::connect(&engine, &QQmlEngine::warnings, &engine, [this](const QList<QQmlError> &warnings) {
            for (const QQmlError &warning : warnings) {
                const QString text = warning.toString();
                if (text.contains("Unable", Qt::CaseInsensitive) || text.contains("TypeError")
                    || text.contains("ReferenceError") || text.contains("Binding", Qt::CaseInsensitive))
                    qmlWarnings.append(text);
            }
        });
        QVariantMap initialProperties{
            {"notebook", QVariant::fromValue(&notebook)}, {"appleShare", QVariant::fromValue(&share)},
            {"pinLock", QVariant::fromValue(&pin)}, {"standby", QVariant::fromValue(&standby)},
            {"deviceInfo", QVariant::fromValue(&device)}, {"wifi", QVariant::fromValue(&wifi)},
            {"bluetooth", QVariant::fromValue(&bluetooth)}, {"usbSsh", QVariant::fromValue(&ssh)},
            {"wireless", QVariant::fromValue(&wireless)}, {"documentExchange", QVariant::fromValue(&exchange)},
            {"markdownWeb", QVariant::fromValue(&markdown)}, {"obsidian", QVariant::fromValue(&obsidian)},
            {"obsidianSync", QVariant::fromValue(&sync)}, {"epaperRefresh", QVariant::fromValue(&refresh)},
            {"externalKeyboard", QVariant::fromValue(&keyboard)}, {"tabletMode", true},
            {"epaperGeometry", QRect(0, 0, 900, 1200)}, {"fontFamily", QGuiApplication::font().family()}
        };
        if (cloud) initialProperties.insert("cloudAccountsModel", QVariant::fromValue(cloud.get()));
        engine.setInitialProperties(initialProperties);
        engine.load(QUrl::fromLocalFile(QStringLiteral(QUADERNO_SOURCE_DIR "/qml/Main.qml")));
        if (window()) {
            window()->setVisibility(QWindow::Windowed);
            window()->resize(900, 1200);
            QMetaObject::invokeMethod(window(), "showCurrentBook");
            window()->show();
        }
    }
    QQuickWindow *window() const
    {
        return engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    }
    template<class T> T *find(const char *name) const { return window() ? window()->findChild<T *>(QString::fromLatin1(name)) : nullptr; }
    void screenshot(const QString &name) const
    {
        const QString path = qEnvironmentVariable("QUADERNO_TEST_SCREENSHOTS");
        if (path.isEmpty() || !window()) return;
        QTest::qWait(180);
        QDir().mkpath(path);
        window()->grabWindow().save(QDir(path).filePath(name + QStringLiteral(".png")));
    }
    bool addBookBackground()
    {
        QImage image(600, 800, QImage::Format_RGB32);
        image.fill(Qt::white);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "JPEG")) return false;
        const QJsonObject page{{"template", "blank"}, {"strokes", QJsonArray{}}, {"items", QJsonArray{}},
            {"background", QString::fromLatin1(bytes.toBase64())}};
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QJsonObject book{{"id", id}, {"title", "Settings reader"}, {"pages", QJsonArray{page, page}},
            {"pageIds", QJsonArray{QUuid::createUuid().toString(QUuid::WithoutBraces), QUuid::createUuid().toString(QUuid::WithoutBraces)}}};
        const QJsonObject root{{"version", 9}, {"books", QJsonArray{book}}, {"folders", QJsonArray{}}, {"activeBook", id}, {"page", 0}};
        return notebook.importFromData(QJsonDocument(root).toJson()) && notebook.openBook(id);
    }
};
}

class SettingsIntegrationTests : public QObject
{
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase()
    {
        QVERIFY(settings.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        QQuickStyle::setStyle("Basic");
        qmlRegisterTypesAndRevisions<AppLanguage>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<AppleShare>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<BluetoothSettings>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<BleSync>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<ChartItem>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<ChartRecognizer>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<CloudAccounts>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<DeviceInfo>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<DiagramExport>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<DiagramItem>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<EpaperRefresh>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<ExternalKeyboard>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<Handwriting>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<InkItem>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<InkSelectionPreview>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<SelectionContentItem>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<LibraryMaintenance>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<MacDocumentExchange>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<MarkdownWeb>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<Notebook>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<ObsidianIntegration>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<ObsidianSync>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<MixedPageAnalyzer>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<PagePreviewItem>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<PageTemplateItem>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<PaperProStandby>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<PinLock>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<PortableDocuments>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<TabletPreferences>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<UsbSsh>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<UsbTransfer>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<WifiSettings>("Quaderno", 254);
        qmlRegisterTypesAndRevisions<WirelessSync>("Quaderno", 254);
        qmlRegisterModule("Quaderno", 254, 254);
    }
    void init()
    {
        QSettings saved(QSettings::IniFormat, QSettings::UserScope, "Quaderno", "Quaderno");
        saved.clear();
        saved.sync();
    }

    void pressurePreferenceChangesStoredInkOnce_data()
    {
        QTest::addColumn<QString>("curve");
        QTest::addColumn<qreal>("expected");
        QTest::newRow("light") << QStringLiteral("light") << qreal(std::pow(.5, .65));
        QTest::newRow("normal") << QStringLiteral("normal") << qreal(.5);
        QTest::newRow("firm") << QStringLiteral("firm") << qreal(std::pow(.5, 1.5));
    }
    void pressurePreferenceChangesStoredInkOnce()
    {
        QFETCH(QString, curve);
        QFETCH(qreal, expected);
        SettingsHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *ink = h.find<InkItem>("inkPage");
        QVERIFY(preferences && ink);
        preferences->setPenPressureCurve(curve);
        QTRY_VERIFY(ink->isVisible() && ink->isEnabled() && ink->width() > 100);
        QPointingDevice pen("Settings pen", 731027, QInputDevice::DeviceType::Stylus,
            QPointingDevice::PointerType::Pen, QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
        const auto position = [&](qreal x, qreal y) { return ink->mapToScene(QPointF(x * ink->width(), y * ink->height())); };
        stylusEvent(h.window(), pen, QEvent::TabletPress, position(.3, .3), .5);
        stylusEvent(h.window(), pen, QEvent::TabletMove, position(.4, .4), .5);
        stylusEvent(h.window(), pen, QEvent::TabletRelease, position(.5, .5), 0);
        QTRY_COMPARE(h.notebook.strokes().size(), 1);
        const QVariantList points = h.notebook.strokes().first().toMap().value("points").toList();
        QVERIFY(points.size() >= 3);
        for (const QVariant &point : points)
            QVERIFY2(qAbs(point.toMap().value("p").toDouble() - expected) < .00001,
                "Incoming pressure and the zero-pressure release fallback must each retain one curve application.");
        QCOMPARE(inkToolFromName(h.notebook.strokes().first().toMap().value("tool").toString()), InkTool::Pen);
    }

    void palmHoldAndRecognitionPreferencesReachProductionInput()
    {
        SettingsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.addBookBackground());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *ink = h.find<InkItem>("inkPage");
        auto *gestures = h.find<QQuickItem>("readingGestures");
        auto *selection = h.find<QObject>("selectionOverlay");
        auto *handwriting = qvariant_cast<Handwriting *>(h.window()->property("handwritingEngine"));
        QVERIFY(preferences && ink && gestures && selection && handwriting);
        preferences->setPalmRejection("extended");
        preferences->setLassoKeepInk(true);
        preferences->setOcrLanguage("en");
        preferences->setOcrNumericMode(true);
        preferences->setOcrWordList({"Quaderno", "macOS"});
        preferences->setOcrCorrections({"Qvaderno=Quaderno", "foo=\\1$literal"});
        QTRY_COMPARE(ink->palmRejection(), QStringLiteral("extended"));
        QTRY_VERIFY(selection->property("keepInk").toBool());
        QCOMPARE(h.window()->property("recognitionLanguage").toString(), QStringLiteral("en"));
        QCOMPARE(handwriting->applyTextPreferences("quaderno macos Qvaderno foo foobar\nO1,5\nOffice 10"),
            QStringLiteral("Quaderno macOS Quaderno \\1$literal foobar\n01,5\nOffice 10"));
        QVERIFY(handwriting->originalResult().isEmpty());
        QCOMPARE(h.window()->property("penTool").toString(), QStringLiteral("read"));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "selectTool", Q_ARG(QString, QStringLiteral("highlighter"))));
        QTRY_VERIFY(gestures->isEnabled() && ink->isVisible() && ink->isEnabled() && ink->width() > 100);
        QTest::qWait(100); // Settle the imported background's viewport and clipping geometry.
        QPointingDevice pen("Palm pen", 731028, QInputDevice::DeviceType::Stylus,
            QPointingDevice::PointerType::Pen, QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
        const QPointF start = ink->mapToScene(QPointF(.3 * ink->width(), .35 * ink->height()));
        const QPointF end = ink->mapToScene(QPointF(.6 * ink->width(), .4 * ink->height()));
        stylusEvent(h.window(), pen, QEvent::TabletPress, start, .5);
        QVERIFY(ink->touchBlocked());
        stylusEvent(h.window(), pen, QEvent::TabletRelease, end, 0);
        QTRY_COMPARE(h.notebook.strokes().size(), 1);
        QVERIFY(!ink->drawing());
        QVERIFY(ink->touchBlocked());
        QVERIFY(!gestures->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!ink->touchBlocked() && gestures->isEnabled(), 1500);
        stylusEvent(h.window(), pen, QEvent::TabletPress, start, .5);
        stylusEvent(h.window(), pen, QEvent::TabletRelease, end, 0);
        QVERIFY(ink->touchBlocked());
        preferences->setPalmRejection("off");
        QTRY_VERIFY(!ink->touchBlocked() && gestures->isEnabled());
        stylusEvent(h.window(), pen, QEvent::TabletPress, start, .5);
        QVERIFY(ink->drawing());
        QVERIFY(!ink->touchBlocked());
        stylusEvent(h.window(), pen, QEvent::TabletRelease, end, 0);
        QVERIFY(!ink->touchBlocked());
    }

    void toolbarOrderingHandednessAndAccessibilityAffectUsableControls()
    {
        SettingsHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences);
        preferences->setToolbarActions({"table", "pen", "lasso"});
        preferences->setHighContrast(true);
        preferences->setBoldLabels(true);
        QQuickItem *table = nullptr, *pen = nullptr, *lasso = nullptr;
        QTRY_VERIFY((table = visibleItem(h.window()->contentItem(), "toolbarAction_table")));
        QTRY_VERIFY((pen = visibleItem(h.window()->contentItem(), "toolbarAction_pen")));
        QTRY_VERIFY((lasso = visibleItem(h.window()->contentItem(), "toolbarAction_lasso")));
        QTRY_VERIFY(table->x() < pen->x() && pen->x() < lasso->x());
        QVERIFY(!visibleItem(h.window()->contentItem(), "toolbarAction_highlighter"));
        QCOMPARE(h.window()->property("ink").value<QColor>(), QColor(Qt::black));
        QVERIFY(h.window()->property("font").value<QFont>().bold());
        preferences->setHandedness("left");
        QTRY_VERIFY(table->x() > pen->x() && pen->x() > lasso->x());
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, table->mapToScene(table->boundingRect().center()).toPoint());
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(editor);
        QTRY_VERIFY(editor->property("visible").toBool());
        QVERIFY(h.notebook.pageItems().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QTRY_VERIFY(!editor->property("visible").toBool());
        preferences->setQuickPanelEnabled(false);
        QTRY_VERIFY(!visibleItem(h.window()->contentItem(), "quickControlsButton"));
        preferences->setQuickPanelEnabled(true);
        QTRY_VERIFY(visibleItem(h.window()->contentItem(), "quickControlsButton"));
    }

    void perBookPreferencesAreStagedAndResetPreservesDocument()
    {
        SettingsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.addBookBackground());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto *dialog = h.find<QObject>("readingPreferencesDialog");
        QVERIFY(dialog);
        const QString book = h.notebook.bookId();
        const QByteArray original = h.notebook.exportBookForTransfer(book);
        QVERIFY(h.notebook.localReadingPreferences().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(dialog, "begin"));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(dialog->setProperty("zoom", 175));
        QVERIFY(dialog->setProperty("draftMargins", 16));
        QVERIFY(dialog->setProperty("orientation", 90));
        QVERIFY(dialog->setProperty("swipe", "scroll"));
        h.screenshot("settings-reading-preferences");
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(h.notebook.localReadingPreferences().isEmpty());
        QCOMPARE(h.notebook.exportBookForTransfer(book), original);
        QVERIFY(QMetaObject::invokeMethod(dialog, "begin"));
        QVERIFY(dialog->setProperty("zoom", 175));
        QVERIFY(dialog->setProperty("draftMargins", 16));
        QVERIFY(dialog->setProperty("orientation", 90));
        QVERIFY(dialog->setProperty("swipe", "scroll"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "save"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.localReadingPreferences().value("readingZoom").toInt(), 175);
        QCOMPARE(h.notebook.localReadingPreferences().value("readingMargins").toInt(), 16);
        QCOMPARE(h.notebook.localReadingPreferences().value("readingOrientation").toInt(), 90);
        QCOMPARE(h.notebook.localReadingPreferences().value("swipeAction").toString(), QStringLiteral("scroll"));
        QTRY_COMPARE(h.window()->property("readingZoom").toDouble(), 1.75);
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 90);
        const QVariantMap saved = h.notebook.localReadingPreferences();
        QVERIFY(QMetaObject::invokeMethod(dialog, "begin"));
        auto *reset = dialog->findChild<QQuickItem *>("resetBookReadingPreferences");
        QVERIFY(reset);
        QVERIFY(QMetaObject::invokeMethod(reset, "clicked"));
        QVERIFY(dialog->property("resetRequested").toBool());
        QCOMPARE(h.notebook.localReadingPreferences(), saved);
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QCOMPARE(h.notebook.localReadingPreferences(), saved);
        QVERIFY(QMetaObject::invokeMethod(dialog, "begin"));
        QVERIFY(QMetaObject::invokeMethod(reset, "clicked"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "save"));
        QVERIFY(h.notebook.localReadingPreferences().isEmpty());
        QTRY_COMPARE(h.window()->property("readingZoom").toDouble(), 1.0);
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 0);
        QCOMPARE(h.notebook.exportBookForTransfer(book), original);
        // Reset must also beat a pending gesture zoom save when the visible
        // zoom already equals the global default (so no zoom restoration runs).
        QVERIFY(h.notebook.setReadingPreferences({{"readingZoom", 175}}));
        QTRY_COMPARE(h.window()->property("readingZoom").toDouble(), 1.75);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setReadingZoom", Q_ARG(double, 1.0), Q_ARG(bool, false)));
        QVERIFY(QMetaObject::invokeMethod(dialog, "begin"));
        QVERIFY(QMetaObject::invokeMethod(reset, "clicked"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "save"));
        QTest::qWait(850);
        QVERIFY(h.notebook.localReadingPreferences().isEmpty());
        QCOMPARE(h.notebook.exportBookForTransfer(book), original);
        Notebook reopened(h.directory.filePath("notebooks.json"));
        QVERIFY(reopened.openBook(book));
        QVERIFY(reopened.localReadingPreferences().isEmpty());
    }

    void quickRotationTargetsCurrentBookOverrideWithoutChangingGlobalOrientation()
    {
        SettingsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.addBookBackground());
        QVERIFY(h.notebook.setReadingPreferences({{"readingOrientation", 90}}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *popup = h.find<QObject>("quickControlsPopup");
        QVERIFY(preferences && popup);
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 90);
        const QString book = h.notebook.bookId();
        const QByteArray document = h.notebook.exportBookForTransfer(book);
        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(popup->property("visible").toBool());
        auto *rotate = popup->findChild<QQuickItem *>("quickRotate");
        QVERIFY(rotate);
        QTest::qWait(100);
        h.screenshot("settings-quick-controls");
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, rotate->mapToScene(rotate->boundingRect().center()).toPoint());
        QTRY_VERIFY(!popup->property("visible").toBool());
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 180);
        QCOMPARE(preferences->orientation(), 0);
        QCOMPARE(h.notebook.localReadingPreferences().value("readingOrientation").toInt(), 180);
        QCOMPARE(h.notebook.exportBookForTransfer(book), document);
    }

    void allSettingsCategoriesOpenFromTheirVisibleButtons_data()
    {
        QTest::addColumn<bool>("tablet");
        QTest::addColumn<int>("orientation");
        QTest::newRow("tablet-portrait") << true << 0;
        QTest::newRow("tablet-landscape") << true << 90;
        QTest::newRow("compact-desktop") << false << 0;
    }

    void allSettingsCategoriesOpenFromTheirVisibleButtons()
    {
        QFETCH(bool, tablet);
        QFETCH(int, orientation);
        SettingsHarness h;
        QVERIFY(h.window());
        auto *settingsView = h.find<QQuickItem>("settingsView");
        const auto *panel = h.find<QQuickItem>("tabletSettingsPanel");
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(settingsView && panel && panel->parentItem() && preferences);
        const auto *pages = panel->parentItem();
        QVERIFY(h.window()->setProperty("tabletMode", tablet));
        if (!tablet) h.window()->resize(760, 560);
        preferences->setOrientation(orientation);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showSettings"));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), orientation);

        const QByteArray library = h.notebook.exportLibraryForTransfer();
        struct Category { int id; QString pageText; };
        const Category categories[] = {
            {0, QStringLiteral("Wi‑Fi")},
            {4, QStringLiteral("Bluetooth")},
            {5, QStringLiteral("Orientamento dello schermo")},
            {10, QStringLiteral("Penna e tocco")},
            {11, QStringLiteral("Backup locali")},
            {7, QStringLiteral("Penna e ricarica")},
            {6, QStringLiteral("Apple e iCloud")},
            {2, QStringLiteral("PIN di Quaderno")},
            {12, QStringLiteral("Accesso SSH")},
            {8, QStringLiteral("Lingua dell’app")},
            {1, QStringLiteral("SSH via USB‑C")},
            {3, QStringLiteral("Informazioni")},
            {9, QStringLiteral("A colpo d’occhio")},
        };
        const auto center = [](QQuickItem *item) {
            return item->mapToScene(item->boundingRect().center()).toPoint();
        };
        const auto canClick = [&](QQuickItem *item) {
            return item && item->isEnabled() && item->width() > 0 && item->height() > 0
                && QRect(QPoint(0, 0), h.window()->size()).contains(center(item));
        };
        const auto click = [&](QQuickItem *item) {
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, center(item));
        };

        for (const auto &category : categories) {
            const QString buttonName = QStringLiteral("settingsCategory_%1").arg(category.id);
            if (category.id == 12 && !tablet) {
                QVERIFY(!visibleItem(settingsView, buttonName));
                continue;
            }
            QQuickItem *button = nullptr;
            QTRY_VERIFY(canClick(button = visibleItem(settingsView, buttonName)));
            click(button);
            QTRY_COMPARE(settingsView->property("category").toInt(), category.id);
            QTRY_VERIFY2(hasVisibleText(pages, category.pageText), qPrintable(category.pageText));
        }

        QQuickItem *search = nullptr;
        QTRY_VERIFY(canClick(search = visibleItem(settingsView, QStringLiteral("settingsSearch"))));
        click(search);
        QTRY_VERIFY(search->hasActiveFocus());
        for (const auto key : {Qt::Key_P, Qt::Key_I, Qt::Key_N}) QTest::keyClick(h.window(), key);
        QTRY_COMPARE(search->property("text").toString(), QStringLiteral("pin"));
        QTRY_VERIFY(!visibleItem(settingsView, QStringLiteral("settingsCategory_3")));
        // The search replaces the Flow delegates before their new positions are laid out.
        QSignalSpy frame(h.window(), &QQuickWindow::frameSwapped);
        h.window()->update();
        QVERIFY(!frame.isEmpty() || frame.wait());
        QQuickItem *security = nullptr;
        QTRY_VERIFY(canClick(security = visibleItem(settingsView, QStringLiteral("settingsCategory_2"))));
        click(security);
        QTRY_COMPARE(settingsView->property("category").toInt(), 2);
        QTRY_VERIFY(search->property("text").toString().isEmpty());
        QTRY_VERIFY(visibleItem(settingsView, QStringLiteral("settingsCategory_3")));
        QCOMPARE(h.notebook.exportLibraryForTransfer(), library);
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void settingsSectionsExposeWritingAndStorageControls()
    {
        SettingsHarness h;
        QVERIFY(h.window());
        const QByteArray original = h.notebook.exportLibraryForTransfer();
        auto *settingsView = h.find<QObject>("settingsView");
        QVERIFY(settingsView);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showSettings"));
        QVERIFY(QMetaObject::invokeMethod(settingsView, "selectCategory", Q_ARG(int, 10)));
        QTRY_VERIFY(visibleItem(h.window()->contentItem(), "preference_penPressureCurve"));
        QTRY_VERIFY(visibleItem(h.window()->contentItem(), "preference_palmRejection"));
        h.screenshot("settings-pen-recognition");
        QVERIFY(QMetaObject::invokeMethod(settingsView, "selectCategory", Q_ARG(int, 11)));
        QTRY_VERIFY(visibleItem(h.window()->contentItem(), "preference_backupSchedule"));
        QTRY_VERIFY(visibleItem(h.window()->contentItem(), "preference_cacheLimitMiB"));
        h.screenshot("settings-storage-backup");
        QCOMPARE(h.notebook.exportLibraryForTransfer(), original);
    }

    void sshAccessShowsAllRoutesWithoutOfferingWritesOnAnUnverifiedDevice()
    {
        SettingsHarness h;
        QVERIFY(h.window());
        auto *settings = h.find<QObject>("settingsView");
        QVERIFY(settings);
        QSignalSpy changes(h.ssh.access(), &PaperProSshAccess::changed);
        const auto before = h.notebook.exportLibraryForTransfer();
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showSettings"));
        QVERIFY(QMetaObject::invokeMethod(settings, "selectCategory", Q_ARG(int, 12)));
        QTRY_VERIFY(visibleItem(h.window()->contentItem(), "sshAccessCard"));
        for (const QString &transport : {QStringLiteral("wifi"), QStringLiteral("usb"), QStringLiteral("bluetooth")}) {
            QTRY_VERIFY(visibleItem(h.window()->contentItem(), QStringLiteral("sshAccessTransport_") + transport));
            QVERIFY(!visibleItem(h.window()->contentItem(), QStringLiteral("sshAccessEnable_") + transport));
            QVERIFY(!visibleItem(h.window()->contentItem(), QStringLiteral("sshAccessDisable_") + transport));
        }
        auto *refresh = visibleItem(h.window()->contentItem(), "sshAccessRefresh");
        QVERIFY(refresh && !refresh->isEnabled());
        QCOMPARE(changes.size(), 0);
        QCOMPARE(h.notebook.exportLibraryForTransfer(), before);
        h.screenshot("settings-ssh-unavailable");

        QVERIFY(h.window()->setProperty("tabletMode", false));
        QTRY_VERIFY(!visibleItem(h.window()->contentItem(), "sshAccessCard"));
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void readerActionsUseSelectedPreferencesAndKeepInkUntouched()
    {
        SettingsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.addBookBackground());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *gestures = h.find<QQuickItem>("readingGestures");
        QVERIFY(preferences && gestures);
        QTRY_VERIFY(gestures->isEnabled());
        const QPointF center(gestures->width() / 2, gestures->height() / 2);
        preferences->setDoubleTapAction("none");
        QVERIFY(QMetaObject::invokeMethod(gestures, "toggleZoomAt", Q_ARG(QPointF, center)));
        QCOMPARE(h.window()->property("readingZoom").toDouble(), 1.0);
        QVERIFY(!h.window()->property("fullPage").toBool());
        preferences->setDoubleTapAction("fullPage");
        QVERIFY(QMetaObject::invokeMethod(gestures, "toggleZoomAt", Q_ARG(QPointF, center)));
        QTRY_VERIFY(h.window()->property("fullPage").toBool());
        QCOMPARE(h.window()->property("readingZoom").toDouble(), 1.0);
        preferences->setDoubleTapAction("refresh");
        QSignalSpy refresh(gestures, SIGNAL(refreshRequested()));
        QVERIFY(QMetaObject::invokeMethod(gestures, "toggleZoomAt", Q_ARG(QPointF, center)));
        QCOMPARE(refresh.count(), 1);
        preferences->setDoubleTapAction("zoom");
        QVERIFY(QMetaObject::invokeMethod(gestures, "toggleZoomAt", Q_ARG(QPointF, center)));
        QTRY_COMPARE(h.window()->property("readingZoom").toDouble(), 2.0);
        QVERIFY(h.notebook.strokes().isEmpty());
        QCOMPARE(h.notebook.page(), 0);
        preferences->setGestureNavigationEnabled(false);
        QTRY_VERIFY(!gestures->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(gestures, "toggleZoomAt", Q_ARG(QPointF, center)));
        QCOMPARE(h.window()->property("readingZoom").toDouble(), 2.0);
    }

    void openingWirelessReviewKeepsAutomaticPreviewConfirmable()
    {
        SettingsHarness h(true);
        QVERIFY(h.window());
        QVERIFY(h.wireless.paired());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *dialog = h.find<QObject>("wirelessSyncDialog");
        auto *automation = h.find<QObject>("syncAutomation");
        QVERIFY(preferences && dialog && automation);
        Notebook destination(h.directory.filePath("peer.json"));
        BleSync receiver(&destination, nullptr, h.directory.filePath("peer-state"));
        receiver.setPeerIdentity(QString(32, QLatin1Char('b')));
        auto *sender = h.wireless.transfer();
        connect(sender, &BleSync::sendMessage, &receiver, [&receiver](const QVariantMap &metadata, const QByteArray &payload) {
            receiver.receiveMessage(metadata, payload);
        }, Qt::QueuedConnection);
        connect(&receiver, &BleSync::sendMessage, sender, [sender](const QVariantMap &metadata, const QByteArray &payload) {
            sender->receiveMessage(metadata, payload);
        }, Qt::QueuedConnection);
        receiver.setConnected(true);
        sender->setConnected(true);
        preferences->setWifiAutoSync("onOpen");
        sender->setPinned(h.notebook.bookId(), true);
        // This in-process authenticated transport exercises the real Main/
        // BleSync review flow without starting a Wi-Fi or Bluetooth service.
        sender->setAutomaticAllowed(true);
        QVERIFY(QMetaObject::invokeMethod(automation, "bookOpened", Q_ARG(QString, h.notebook.bookId())));
        QTRY_COMPARE_WITH_TIMEOUT(sender->previewInfo().value("action").toString(), QStringLiteral("create"), 5000);
        QVERIFY(sender->previewInfo().value("automatic").toBool());
        const QByteArray before = destination.exportLibraryForTransfer();
        QVERIFY(QMetaObject::invokeMethod(dialog, "begin", Q_ARG(QVariant, QVariant(false))));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QTest::qWait(100);
        QVERIFY(automation->property("allowed").toBool());
        QVERIFY(sender->automaticAllowed());
        QCOMPARE(sender->previewInfo().value("action").toString(), QStringLiteral("create"));
        QCOMPARE(destination.exportLibraryForTransfer(), before);
        sender->apply();
        QTRY_VERIFY(sender->completed());
        QVERIFY(destination.hasBookId(h.notebook.bookId()));
        QCOMPARE(destination.bookTransferRevision(h.notebook.bookId()), h.notebook.bookTransferRevision(h.notebook.bookId()));
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void reapplyingSelectedProfileRestoresCurrentFullPageMode()
    {
        SettingsHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences);
        const QByteArray original = h.notebook.exportLibraryForTransfer();
        preferences->applyUsageProfile("reading");
        QTRY_VERIFY(h.window()->property("fullPage").toBool());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, false)));
        QVERIFY(!h.window()->property("fullPage").toBool());
        preferences->applyUsageProfile("reading");
        QTRY_VERIFY(h.window()->property("fullPage").toBool());
        QCOMPARE(h.notebook.exportLibraryForTransfer(), original);
    }

    void realWirelessAutomationPolicyChangesRemainSynchronousAndDoNotLoop()
    {
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("notebooks.json"));
        QVERIFY(notebook.createBook("Pinned automation regression"));
        TabletPreferences preferences(directory.filePath("preferences.ini"));
        WirelessSync wireless(&notebook, pairingPathFor(directory, true), true, false);
        QVERIFY(wireless.paired());
        wireless.transfer()->setPinned(notebook.bookId(), true);
        QCOMPARE(wireless.transfer()->pinnedBooks(), QStringList{notebook.bookId()});
        QQmlEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, &engine, [&warnings](const QList<QQmlError> &entries) {
            for (const auto &entry : entries) warnings.append(entry.toString());
        });
        QQmlComponent component(&engine);
        component.setData(R"QML(
import QtQuick
import "."
Item {
    id: fixture
    required property var wirelessBackend
    required property var preferencesBackend
    required property var notebookBackend
    property alias scheduler: automation
    property alias telemetry: device
    property alias network: networkState
    QtObject {
        id: device
        property bool charging: true
        property bool freshCharging: true
        function refresh() { charging = freshCharging }
    }
    QtObject {
        id: networkState
        property bool available: true
        property bool connected: true
        property string ssid: "Trusted"
    }
    SyncAutomation {
        id: automation
        preferences: fixture.preferencesBackend
        wireless: fixture.wirelessBackend
        notebook: fixture.notebookBackend
        deviceInfo: device
        wifi: networkState
    }
}
)QML", QUrl::fromLocalFile(QStringLiteral(QUADERNO_SOURCE_DIR "/qml/RealWirelessAutomationFixture.qml")));
        std::unique_ptr<QObject> fixture(component.createWithInitialProperties({
            {"wirelessBackend", QVariant::fromValue(&wireless)},
            {"preferencesBackend", QVariant::fromValue(&preferences)},
            {"notebookBackend", QVariant::fromValue(&notebook)}}));
        QVERIFY2(fixture, qPrintable(component.errorString()));
        auto *automation = qvariant_cast<QObject *>(fixture->property("scheduler"));
        auto *device = qvariant_cast<QObject *>(fixture->property("telemetry"));
        auto *wifi = qvariant_cast<QObject *>(fixture->property("network"));
        QVERIFY(automation && device && wifi);
        QVERIFY(!automation->property("eligible").toBool());
        QVERIFY(!wireless.automaticAllowed());
        preferences.setSyncTransport("wifi");
        preferences.setWifiAutoSync("onOpen");
        QVERIFY(automation->property("eligible").toBool());
        QVERIFY(wireless.automaticAllowed());
        QVERIFY(wireless.transfer()->autoSync());
        QVERIFY(wireless.transfer()->triggeredAutomaticSync());
        QVERIFY(wireless.automaticWifiOnly());

        // These checks intentionally do not process events: a revoked policy
        // must close its real C++ gate in the same turn, without a deferred gap.
        QVERIFY(automation->setProperty("allowed", false));
        QVERIFY(!automation->property("eligible").toBool());
        QVERIFY(!wireless.automaticAllowed());
        QVERIFY(!wireless.transfer()->automaticAllowed());
        QVERIFY(QMetaObject::invokeMethod(automation, "bookOpened", Q_ARG(QString, notebook.bookId())));
        QVERIFY(automation->property("pending").toBool());
        QVERIFY(automation->setProperty("allowed", true));
        QVERIFY(automation->property("eligible").toBool());
        QVERIFY(wireless.automaticAllowed());
        preferences.setWifiChargingOnly(true);
        QVERIFY(device->setProperty("freshCharging", false));
        QVERIFY(QMetaObject::invokeMethod(automation, "refreshChargingGate"));
        QVERIFY(!automation->property("eligible").toBool());
        QVERIFY(!wireless.automaticAllowed());
        QVERIFY(!wireless.transfer()->automaticAllowed());
        QVERIFY(device->setProperty("freshCharging", true));
        QVERIFY(QMetaObject::invokeMethod(automation, "refreshChargingGate"));
        QVERIFY(automation->property("eligible").toBool());
        QVERIFY(wireless.automaticAllowed());
        preferences.setTrustedWifiSsids({QStringLiteral("Trusted")});
        QVERIFY(wifi->setProperty("ssid", "Guest"));
        QVERIFY(!automation->property("eligible").toBool());
        QVERIFY(!wireless.automaticAllowed());
        QVERIFY(wifi->setProperty("ssid", "Trusted"));
        QVERIFY(automation->property("eligible").toBool());
        QVERIFY(wireless.automaticAllowed());
        QVERIFY(wifi->setProperty("available", false));
        QVERIFY(!automation->property("eligible").toBool());
        QVERIFY(!wireless.automaticAllowed());
        QVERIFY(wifi->setProperty("available", true));
        QVERIFY(automation->property("eligible").toBool());
        QVERIFY(wireless.automaticAllowed());
        wireless.transfer()->setPinned(notebook.bookId(), false);
        QVERIFY(automation->property("eligible").toBool()); // The device can still receive.
        QVERIFY(wireless.automaticAllowed());
        QVERIFY(!automation->property("pending").toBool());
        QCOMPARE(wireless.transfer()->pendingAutomaticCount(), 0);
        wireless.transfer()->setPinned(notebook.bookId(), true);
        QVERIFY(automation->property("eligible").toBool());
        QVERIFY(wireless.automaticAllowed());
        preferences.setSyncConflictPolicy("keepBoth");
        QCOMPARE(wireless.transfer()->conflictPolicy(), QStringLiteral("keepBoth"));
        Notebook destination(directory.filePath("peer.json"));
        BleSync receiver(&destination, nullptr, directory.filePath("peer-state"));
        receiver.setPeerIdentity(QString(32, QLatin1Char('b')));
        auto *sender = wireless.transfer();
        connect(sender, &BleSync::sendMessage, &receiver, [&receiver](const QVariantMap &metadata, const QByteArray &payload) {
            receiver.receiveMessage(metadata, payload);
        }, Qt::QueuedConnection);
        connect(&receiver, &BleSync::sendMessage, sender, [sender](const QVariantMap &metadata, const QByteArray &payload) {
            sender->receiveMessage(metadata, payload);
        }, Qt::QueuedConnection);
        receiver.setConnected(true);
        sender->setConnected(true);
        // Simulate an already-authenticated transport in process, as the Main
        // review test does. No listener, Wi-Fi connection or Bluetooth starts.
        sender->setAutomaticAllowed(true);
        QVERIFY(QMetaObject::invokeMethod(automation, "bookOpened", Q_ARG(QString, notebook.bookId())));
        QTRY_COMPARE_WITH_TIMEOUT(sender->previewInfo().value("action").toString(), QStringLiteral("create"), 5000);
        QVERIFY(sender->previewInfo().value("automatic").toBool());
        const auto destinationBefore = destination.exportLibraryForTransfer();
        QVERIFY(automation->setProperty("allowed", false));
        QVERIFY(!wireless.automaticAllowed());
        QVERIFY(!sender->automaticAllowed());
        QVERIFY(sender->previewInfo().isEmpty());
        QVERIFY(!sender->busy());
        QCOMPARE(destination.exportLibraryForTransfer(), destinationBefore);
        preferences.setWifiAutoSync("manual");
        QVERIFY(!automation->property("eligible").toBool());
        QVERIFY(!automation->property("pending").toBool());
        QVERIFY(!wireless.automaticAllowed());
        QVERIFY(!wireless.transfer()->autoSync());
        QCoreApplication::processEvents();
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join(QStringLiteral(" | "))));
        QVERIFY(!wireless.active()); // The regression never opens a network service.
    }

    void automaticSyncRefreshesChargingBeforeDispatchAndRejectsUnknownTrustedNetwork()
    {
        QQmlEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, &engine, [&warnings](const QList<QQmlError> &entries) {
            for (const auto &entry : entries) warnings.append(entry.toString());
        });
        QQmlComponent component(&engine);
        component.setData(R"QML(
import QtQuick
import "."
Item {
    property alias fakeDevice: device
    property alias fakePreferences: preferences
    property alias fakeTransfer: transfer
    property alias fakeWifi: wifi
    property alias fakeWireless: wireless
    property alias scheduler: automation
    QtObject {
        id: preferences
        property string wifiAutoSync: "onOpen"
        property string syncTransport: "wifi"
        property int wifiSyncMinutes: 15
        property bool wifiChargingOnly: true
        property var trustedWifiSsids: []
        property string syncConflictPolicy: "ask"
    }
    QtObject {
        id: transfer
        signal changed()
        signal resumeChanged()
        property bool triggeredAutomaticSync: false
        property bool autoSync: false
        property string conflictPolicy: "ask"
        property var pinnedBooks: ["book"]
        property bool connected: true
        property bool busy: false
        property bool resumeBusy: false
        onResumeBusyChanged: resumeChanged()
        property int pendingAutomaticCount: 0
        property bool automaticRetryPending: false
        property var previewInfo: ({})
        property int requests: 0
        function requestAutomaticSync() { requests++; return true }
    }
    QtObject {
        id: wireless
        signal changed()
        property var transfer: transferObject
        property bool paired: true
        property bool tablet: true
        property bool bluetoothActive: false
        property bool automaticAllowed: false
        property string automaticTransport: "auto"
        property bool automaticWifiAllowed: true
        property bool connectionPaused: false
        property bool reconnectPending: false
        property int reconnectDelaySeconds: 0
    }
    property var transferObject: transfer
    QtObject {
        id: device
        property bool charging: true
        property bool freshCharging: true
        property int readings: 0
        function refresh() { readings++; charging = freshCharging }
    }
    QtObject { id: wifi; property bool available: false; property bool connected: false; property string ssid: "" }
    QtObject { id: notebook }
    SyncAutomation {
        id: automation
        preferences: preferencesObject
        wireless: wirelessObject
        deviceInfo: device
        wifi: wifiObject
        notebook: notebookObject
    }
    property var preferencesObject: preferences
    property var wirelessObject: wireless
    property var wifiObject: wifi
    property var notebookObject: notebook
}
)QML", QUrl::fromLocalFile(QStringLiteral(QUADERNO_SOURCE_DIR "/qml/SettingsAutomationFixture.qml")));
        std::unique_ptr<QObject> fixture(component.create());
        QVERIFY2(fixture, qPrintable(component.errorString()));
        auto *device = qvariant_cast<QObject *>(fixture->property("fakeDevice"));
        auto *preferences = qvariant_cast<QObject *>(fixture->property("fakePreferences"));
        auto *transfer = qvariant_cast<QObject *>(fixture->property("fakeTransfer"));
        auto *wifi = qvariant_cast<QObject *>(fixture->property("fakeWifi"));
        auto *wireless = qvariant_cast<QObject *>(fixture->property("fakeWireless"));
        auto *automation = qvariant_cast<QObject *>(fixture->property("scheduler"));
        QVERIFY(device && preferences && transfer && wifi && wireless && automation);
        QTRY_VERIFY(automation->property("eligible").toBool());
        QVERIFY(device->setProperty("freshCharging", false));
        QVERIFY(device->property("charging").toBool()); // Cached status is deliberately stale.
        const int previousReads = device->property("readings").toInt();
        QVERIFY(QMetaObject::invokeMethod(automation, "bookOpened", Q_ARG(QString, QString("book"))));
        QVERIFY(device->property("readings").toInt() > previousReads);
        QVERIFY(!device->property("charging").toBool());
        QCOMPARE(transfer->property("requests").toInt(), 0);
        QVERIFY(automation->property("pending").toBool());
        QVERIFY(device->setProperty("freshCharging", true));
        QVERIFY(preferences->setProperty("trustedWifiSsids", QStringList{"Home"}));
        QVERIFY(QMetaObject::invokeMethod(automation, "refreshChargingGate"));
        QCoreApplication::processEvents();
        QVERIFY(!automation->property("eligible").toBool());
        QCOMPARE(transfer->property("requests").toInt(), 0);
        QVERIFY(wifi->setProperty("available", true));
        QVERIFY(wifi->setProperty("connected", true));
        QVERIFY(wifi->setProperty("ssid", "Home"));
        QTRY_COMPARE(transfer->property("requests").toInt(), 1);
        QVERIFY(!automation->property("pending").toBool());
        QVERIFY(automation->setProperty("allowed", false));
        QVERIFY(QMetaObject::invokeMethod(automation, "bookOpened", Q_ARG(QString, QString("book"))));
        QCOMPARE(transfer->property("requests").toInt(), 1);
        QVERIFY(preferences->setProperty("wifiAutoSync", "manual"));
        QVERIFY(!automation->property("pending").toBool());
        QVERIFY(automation->setProperty("allowed", true));
        QCoreApplication::processEvents();
        QCOMPARE(transfer->property("requests").toInt(), 1);

        // Bluetooth automation remains independent of Wi-Fi SSIDs, while the
        // charging and editing gates continue to apply to both transports.
        QVERIFY(wifi->setProperty("available", false));
        QVERIFY(wireless->setProperty("bluetoothActive", true));
        QVERIFY(preferences->setProperty("syncTransport", "bluetooth"));
        QVERIFY(preferences->setProperty("wifiAutoSync", "onChange"));
        QTRY_COMPARE(transfer->property("requests").toInt(), 2);
        QVERIFY(!transfer->property("triggeredAutomaticSync").toBool());
        QCOMPARE(wireless->property("automaticTransport").toString(), QStringLiteral("bluetooth"));
        QVERIFY(!wireless->property("automaticWifiAllowed").toBool());
        QVERIFY(automation->property("eligible").toBool());
        QVERIFY(!automation->property("pending").toBool());

        // Selecting Wi-Fi only must immediately close automatic permission on
        // an unknown network and cannot dispatch over an existing BT link.
        QVERIFY(preferences->setProperty("syncTransport", "wifi"));
        QVERIFY(!wireless->property("automaticAllowed").toBool());
        QVERIFY(wifi->setProperty("available", true));
        QCoreApplication::processEvents();
        QCOMPARE(transfer->property("requests").toInt(), 2);
        QVERIFY(wireless->setProperty("bluetoothActive", false));
        QTRY_COMPARE(transfer->property("requests").toInt(), 3);

        // Many notifications during a disconnection represent one catch-up,
        // and editor guards keep it pending until editing has finished.
        QVERIFY(transfer->setProperty("connected", false));
        QVERIFY(automation->setProperty("allowed", false));
        for (int i = 0; i < 8; ++i) QVERIFY(QMetaObject::invokeMethod(automation, "catchUp"));
        QVERIFY(transfer->setProperty("connected", true));
        QCoreApplication::processEvents();
        QCOMPARE(transfer->property("requests").toInt(), 3);
        QVERIFY(automation->property("pending").toBool());
        QVERIFY(automation->setProperty("allowed", true));
        QTRY_COMPARE(transfer->property("requests").toInt(), 4);
        QCoreApplication::processEvents();
        QCOMPARE(transfer->property("requests").toInt(), 4);

        QVERIFY(transfer->setProperty("resumeBusy", true));
        QVERIFY(QMetaObject::invokeMethod(automation, "syncNow"));
        QCoreApplication::processEvents();
        QCOMPARE(transfer->property("requests").toInt(), 4);
        QVERIFY(automation->property("pending").toBool());
        QVERIFY(transfer->setProperty("resumeBusy", false));
        QTRY_COMPARE(transfer->property("requests").toInt(), 5);

        // A paired receiver can listen even if it has no notebooks to send.
        QVERIFY(transfer->setProperty("pinnedBooks", QStringList{}));
        QVERIFY(preferences->setProperty("syncTransport", "auto"));
        QVERIFY(transfer->setProperty("connected", false));
        QVERIFY(wireless->property("automaticAllowed").toBool());
        QVERIFY(!automation->property("pending").toBool());
        QVERIFY(transfer->setProperty("connected", true));
        QCoreApplication::processEvents();
        QCOMPARE(transfer->property("requests").toInt(), 5);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join(QStringLiteral(" | "))));
    }

    void shapeToleranceChangesRecognitionOfTheSameImperfectLine()
    {
        QList<InkPoint> line;
        for (int i = 0; i <= 40; ++i) {
            const qreal fraction = i / 40.0;
            line.append({QPointF(.2 + .6 * fraction, .4 + .0055 * std::sin(fraction * 3.141592653589793)), .5});
        }
        QVERIFY(!recognizeInkShape(line, QSizeF(600, 800), .65));
        const auto relaxed = recognizeInkShape(line, QSizeF(600, 800), 1.35);
        QVERIFY(relaxed.has_value());
        QCOMPARE(relaxed->tool, InkTool::Line);
        QCOMPARE(relaxed->anchors.size(), 2);
        QVERIFY(!recognizeInkShape(line, QSizeF(600, 800), std::numeric_limits<qreal>::quiet_NaN()));
        SettingsHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *ink = h.find<InkItem>("inkPage");
        QVERIFY(preferences && ink);
        preferences->setShapeTolerance("strict");
        QTRY_COMPARE(ink->shapeTolerance(), .65);
        preferences->setShapeTolerance("relaxed");
        QTRY_COMPARE(ink->shapeTolerance(), 1.35);
        QVERIFY(h.notebook.strokes().isEmpty());
    }

    void cloudDialogWithRealUnconfiguredBackendHasNoStartupBindingWarnings()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QNetworkAccessManager network;
        // Supplying the test network manager disables platform credential storage.
        // Missing registrations mean this fixture never starts authentication or requests.
        CloudAccounts cloud(directory.filePath("cloud"), true, nullptr, &network);
        QVERIFY(cloud.provider().isEmpty());
        for (const QVariant &account : cloud.accounts()) {
            QVERIFY(!account.toMap().value("configured").toBool());
            QVERIFY(!account.toMap().value("connected").toBool());
        }
        QStringList bindingWarnings;
        QQmlApplicationEngine engine;
        connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &warnings) {
            for (const QQmlError &warning : warnings) {
                const QString text = warning.toString();
                if (text.contains("Unable", Qt::CaseInsensitive) || text.contains("TypeError")
                    || text.contains("ReferenceError") || text.contains("Binding", Qt::CaseInsensitive))
                    bindingWarnings.append(text);
            }
        });
        engine.setInitialProperties({{"model", QVariant::fromValue(&cloud)}});
        engine.loadData(R"QML(
import QtQuick
import QtQuick.Controls
import Quaderno
import "."
ApplicationWindow {
    id: fixture
    required property CloudAccounts model
    visible: true
    width: 900
    height: 1200
    CloudAccountsDialog {
        cloud: fixture.model
        uiScale: 1
        tabletMode: true
        showTouchKeyboard: false
        canUploadCurrent: false
    }
}
)QML", QUrl::fromLocalFile(QStringLiteral(QUADERNO_SOURCE_DIR "/qml/CloudDialogFixture.qml")));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(bindingWarnings.join(QStringLiteral(" | "))));
        auto *dialog = engine.rootObjects().first()->findChild<QObject *>("cloudAccountsDialog");
        QVERIFY(dialog);
        QCoreApplication::processEvents();
        QVERIFY2(bindingWarnings.isEmpty(), qPrintable(bindingWarnings.join(QStringLiteral(" | "))));
        auto *signIn = dialog->findChild<QQuickItem *>("cloudSignIn");
        auto *disconnect = dialog->findChild<QQuickItem *>("cloudDisconnect");
        auto *download = dialog->findChild<QQuickItem *>("cloudDownloadSelected");
        QVERIFY(signIn && disconnect && download);
        for (const QString &provider : {QStringLiteral("googleDrive"), QStringLiteral("dropbox"), QStringLiteral("oneDrive")}) {
            QVERIFY(QMetaObject::invokeMethod(dialog, "begin", Q_ARG(QVariant, QVariant(provider))));
            QTRY_VERIFY(dialog->property("visible").toBool());
            QCOMPARE(cloud.provider(), provider);
            QVERIFY(!signIn->isEnabled());
            QVERIFY(!disconnect->isVisible());
            QVERIFY(!download->isVisible());
            QVERIFY(!cloud.busy() && !cloud.authPending());
            QCoreApplication::processEvents();
            QVERIFY2(bindingWarnings.isEmpty(), qPrintable(bindingWarnings.join(QStringLiteral(" | "))));
            QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
            QTRY_VERIFY(!dialog->property("visible").toBool());
        }
        QVERIFY2(bindingWarnings.isEmpty(), qPrintable(bindingWarnings.join(QStringLiteral(" | "))));
    }

    void mainAccountButtonsOpenCloudDialogsInsideTheWindowOverlay()
    {
        SettingsHarness h(false, true);
        QVERIFY2(h.window(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
        QVERIFY(h.cloud);
        QVERIFY(h.cloud->provider().isEmpty());
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
        auto *settingsView = h.find<QObject>("settingsView");
        auto *overlay = qvariant_cast<QQuickItem *>(h.window()->property("popupOverlay"));
        QVERIFY(settingsView && overlay);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showSettings"));
        for (const QString &provider : {QStringLiteral("googleDrive"), QStringLiteral("dropbox"), QStringLiteral("oneDrive")}) {
            QVERIFY(QMetaObject::invokeMethod(settingsView, "selectCategory", Q_ARG(int, 6)));
            QQuickItem *button = nullptr;
            QTRY_VERIFY((button = visibleItem(h.window()->contentItem(), "accountAction_" + provider + "_cloud")));
            QVERIFY(button->isEnabled());
            QTest::qWait(100); // Allow the newly selected category to lay out its account rows.
            // Scroll the real account row into view, then deliver a real pointer
            // click through AccountsPanel, SettingsView and Main's Loader.
            bool foundScroll = false;
            for (auto *ancestor = button->parentItem(); ancestor; ancestor = ancestor->parentItem()) {
                if (!ancestor->property("contentY").isValid() || !ancestor->property("contentHeight").isValid()) continue;
                auto *content = qvariant_cast<QQuickItem *>(ancestor->property("contentItem"));
                if (!content) continue;
                const QPointF center = button->mapToItem(content, button->boundingRect().center());
                const qreal maximum = qMax(qreal(0), ancestor->property("contentHeight").toReal() - ancestor->height());
                QVERIFY(ancestor->setProperty("contentY", qBound(qreal(0), center.y() - ancestor->height() / 2, maximum)));
                foundScroll = true;
                break;
            }
            QVERIFY(foundScroll);
            QTest::qWait(100);
            const QPoint center = button->mapToScene(button->boundingRect().center()).toPoint();
            QVERIFY(QRect(QPoint(0, 0), h.window()->size()).contains(center));
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, center);
            auto *dialog = h.find<QObject>("cloudAccountsDialog");
            QVERIFY(dialog);
            QTRY_VERIFY(dialog->property("visible").toBool());
            QCOMPARE(h.cloud->provider(), provider);
            QTRY_VERIFY(dialog->property("width").toReal() > 100 && dialog->property("height").toReal() > 100);
            QCOMPARE(qvariant_cast<QQuickItem *>(dialog->property("parent")), overlay);
            auto *content = qvariant_cast<QQuickItem *>(dialog->property("contentItem"));
            QVERIFY(content && content->parentItem());
            auto *popupItem = content->parentItem();
            QTRY_VERIFY(overlay->boundingRect().adjusted(-1, -1, 1, 1).contains(
                popupItem->mapRectToItem(overlay, popupItem->boundingRect())));
            QVERIFY(!h.cloud->busy() && !h.cloud->authPending());
            QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
            h.screenshot("settings-cloud-" + provider);
            QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
            QTRY_VERIFY(!dialog->property("visible").toBool());
        }
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void portableNotebookImportRequiresFreshReviewAndLockRevokesPreparedData()
    {
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        Notebook destination(directory.filePath("destination.json"));
        QVERIFY(source.createBook("Portable settings fixture"));
        QVERIFY(source.appendStroke(testStroke()));
        QVERIFY(source.setReadingPreferences({{"readingZoom", 175}, {"readingOrientation", 90}}));
        PortableDocuments exporter;
        exporter.setNotebook(&source);
        const QUrl file = exporter.exportCurrent();
        QVERIFY(file.isLocalFile());
        QVERIFY(QFileInfo::exists(file.toLocalFile()));
        PortableDocuments importer;
        importer.setNotebook(&destination);
        const QByteArray before = destination.exportLibraryForTransfer();
        QVERIFY(importer.prepareImport(file));
        QCOMPARE(destination.exportLibraryForTransfer(), before);
        QVERIFY(!importer.preview().value("token").toString().isEmpty());
        importer.setLocked(true);
        QVERIFY(importer.preview().isEmpty());
        QVERIFY(!importer.applyImport());
        QCOMPARE(destination.exportLibraryForTransfer(), before);
        importer.setLocked(false);
        QVERIFY(importer.prepareImport(file));
        QVERIFY(destination.createBook("New local work after preview"));
        QVERIFY(destination.appendStroke(testStroke()));
        const QByteArray edited = destination.exportLibraryForTransfer();
        QVERIFY(!importer.applyImport());
        QCOMPARE(destination.exportLibraryForTransfer(), edited);
        QVERIFY(importer.prepareImport(file));
        QVERIFY(importer.applyImport());
        QVERIFY(destination.hasBookId(source.bookId()));
        QCOMPARE(destination.bookTransferRevision(source.bookId()), source.bookTransferRevision(source.bookId()));
        QVERIFY(destination.readingPreferencesForBook(source.bookId()).isEmpty());
        QVERIFY(importer.preview().isEmpty());
        QVERIFY(!importer.prepareImport(QUrl("https://example.invalid/private.quaderno")));
        QVERIFY(!importer.error().isEmpty());
        exporter.setLocked(true);
        QVERIFY(exporter.exportCurrent().isEmpty());
        QVERIFY(!QFileInfo::exists(file.toLocalFile()));
        QVERIFY(source.localReadingPreferences().contains("readingZoom"));
    }
};

QTEST_MAIN(SettingsIntegrationTests)
#include "settingsintegration_tests.moc"
