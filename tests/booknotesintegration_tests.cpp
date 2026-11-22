#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QBuffer>
#include <QPainter>
#include <QPdfWriter>
#include <QPageSize>
#include <QLineF>
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

namespace {
QVariantList qmlList(QObject *object, const char *name)
{
    const QVariant value = object->property(name);
    return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant().toList() : value.toList();
}
QVariantMap qmlMap(QObject *object, const char *name)
{
    const QVariant value = object->property(name);
    return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant().toMap() : value.toMap();
}
QQuickItem *findVisualItem(QQuickItem *parent, const QString &name)
{
    if (parent->objectName() == name) return parent;
    for (QQuickItem *child : parent->childItems())
        if (auto *found = findVisualItem(child, name)) return found;
    return nullptr;
}
struct BookNotesHarness {
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
    WirelessSync wireless{&notebook, directory.filePath("pairing.key"), true, false};
    MacDocumentExchange exchange{&notebook, true};
    MarkdownWeb markdown{&notebook};
    ObsidianIntegration obsidian;
    ObsidianSync sync{&notebook, directory.filePath("obsidian"), false};
    QQmlApplicationEngine engine;

    explicit BookNotesHarness(bool tablet = true)
    {
        notebook.createBook("UI regression");
        engine.setInitialProperties({
            {"notebook", QVariant::fromValue(&notebook)}, {"appleShare", QVariant::fromValue(&share)},
            {"pinLock", QVariant::fromValue(&pin)}, {"standby", QVariant::fromValue(&standby)},
            {"deviceInfo", QVariant::fromValue(&device)}, {"wifi", QVariant::fromValue(&wifi)},
            {"bluetooth", QVariant::fromValue(&bluetooth)}, {"usbSsh", QVariant::fromValue(&ssh)},
            {"wireless", QVariant::fromValue(&wireless)}, {"documentExchange", QVariant::fromValue(&exchange)},
            {"markdownWeb", QVariant::fromValue(&markdown)}, {"obsidian", QVariant::fromValue(&obsidian)},
            {"obsidianSync", QVariant::fromValue(&sync)}, {"epaperRefresh", QVariant::fromValue(&refresh)},
            {"externalKeyboard", QVariant::fromValue(&keyboard)}, {"tabletMode", tablet},
            {"epaperGeometry", QRect(0, 0, 900, 1200)}, {"fontFamily", QGuiApplication::font().family()}
        });
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
        const QString directory = qEnvironmentVariable("QUADERNO_TEST_SCREENSHOTS");
        if (!directory.isEmpty() && window()) {
            QTest::qWait(180); // Capture the settled popup layout, including its opening transition.
            QDir().mkpath(directory);
            window()->grabWindow().save(QDir(directory).filePath(name + QStringLiteral(".png")));
        }
    }
    bool importPdfBook()
    {
        const QString file = directory.filePath("book-notes-fixture.pdf");
        {
            QPdfWriter writer(file);
            writer.setResolution(72);
            writer.setPageSize(QPageSize(QSizeF(300, 400), QPageSize::Point));
            QPainter painter(&writer);
            if (!painter.isActive()) return false;
            painter.drawText(30, 70, "First chapter: annotate this paragraph.");
            if (!writer.newPage()) return false;
            painter.drawText(30, 70, "Second chapter: independent annotations.");
            painter.end();
        }
        return notebook.importDocument(QUrl::fromLocalFile(file), {});
    }
};

QVariantMap noteFixture()
{
    return {{"type", "note"}, {"x", .37}, {"y", .42}, {"width", .05}, {"height", .0375},
        {"text", "Existing note"}, {"noteInk", QVariantList{}}, {"noteColor", "yellow"}};
}
void tabletEvent(QQuickWindow *window, const QPointingDevice &device, QEvent::Type type, QPointF point)
{
    const bool release = type == QEvent::TabletRelease;
    QTabletEvent event(type, &device, point, window->mapToGlobal(point), release ? 0 : .65,
        0, 0, 0, 0, 0, Qt::NoModifier, type == QEvent::TabletMove ? Qt::NoButton : Qt::LeftButton,
        release ? Qt::NoButton : Qt::LeftButton);
    QCoreApplication::sendEvent(window, &event);
}
void tap(QQuickWindow *window, const QString &input, QPointF scenePoint)
{
    if (input == "mouse") QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, scenePoint.toPoint());
    else if (input == "touch") {
        auto *device = QTest::createTouchDevice();
        QTest::touchEvent(window, device).press(0, scenePoint.toPoint(), window).commit();
        QTest::qWait(25);
        QTest::touchEvent(window, device).release(0, scenePoint.toPoint(), window).commit();
    } else {
        QPointingDevice pen("Post-it test pen", 823172, QInputDevice::DeviceType::Stylus,
            QPointingDevice::PointerType::Pen,
            QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
        tabletEvent(window, pen, QEvent::TabletPress, scenePoint);
        tabletEvent(window, pen, QEvent::TabletRelease, scenePoint);
    }
}
void drawNoteStroke(QQuickWindow *window, InkItem *canvas)
{
    QPointingDevice pen("Post-it handwriting pen", 823173, QInputDevice::DeviceType::Stylus,
        QPointingDevice::PointerType::Pen,
        QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
    const auto at = [&](qreal x, qreal y) { return canvas->mapToScene(QPointF(canvas->width() * x, canvas->height() * y)); };
    tabletEvent(window, pen, QEvent::TabletPress, at(.2, .3));
    tabletEvent(window, pen, QEvent::TabletMove, at(.35, .55));
    tabletEvent(window, pen, QEvent::TabletMove, at(.5, .3));
    tabletEvent(window, pen, QEvent::TabletRelease, at(.65, .55));
}
bool startAt(BookNotesHarness &h, QPointF anchor = {.4, .45})
{
    return QMetaObject::invokeMethod(h.window(), "startBookNote")
        && QMetaObject::invokeMethod(h.window(), "placeBookNote", Q_ARG(QPointF, anchor));
}
void click(QQuickWindow *window, QQuickItem *button)
{
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
        button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
}
}

class BookNotesIntegrationTests : public QObject
{
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase()
    {
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        QQuickStyle::setStyle("Basic");
        // Register production classes, then load production QML directly from its source path.
#define REGISTER(T) qmlRegisterTypesAndRevisions<T>("Quaderno", 254)
        REGISTER(AppLanguage); REGISTER(AppleShare); REGISTER(BluetoothSettings); REGISTER(BleSync);
        REGISTER(ChartItem); REGISTER(ChartRecognizer); REGISTER(DeviceInfo); REGISTER(DiagramExport);
        REGISTER(DiagramItem); REGISTER(EpaperRefresh); REGISTER(ExternalKeyboard); REGISTER(Handwriting);
        REGISTER(InkItem); REGISTER(InkSelectionPreview); REGISTER(SelectionContentItem);
        REGISTER(MacDocumentExchange); REGISTER(MarkdownWeb); REGISTER(Notebook); REGISTER(ObsidianIntegration);
        REGISTER(ObsidianSync); REGISTER(PagePreviewItem); REGISTER(PageTemplateItem); REGISTER(PaperProStandby);
        REGISTER(PinLock); REGISTER(UsbSsh); REGISTER(UsbTransfer); REGISTER(WifiSettings); REGISTER(WirelessSync);
        REGISTER(TabletPreferences); REGISTER(LibraryMaintenance); REGISTER(PortableDocuments); REGISTER(CloudAccounts);
#undef REGISTER
        qmlRegisterModule("Quaderno", 254, 254);
    }

    void init()
    {
        QSettings preferences(QSettings::IniFormat, QSettings::UserScope, "Quaderno", "Quaderno");
        preferences.remove("tablet");
        preferences.sync();
    }

    void cleanup()
    {
        QSettings preferences(QSettings::IniFormat, QSettings::UserScope, "Quaderno", "Quaderno");
        preferences.remove("tablet");
        preferences.sync();
    }



    void pointPlacementAndMarkerReopeningPreserveTypedAndHandwrittenNotes_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<int>("angle");
        QTest::newRow("mouse") << QStringLiteral("mouse") << 0;
        QTest::newRow("finger") << QStringLiteral("touch") << 0;
        QTest::newRow("pen") << QStringLiteral("pen") << 0;
        QTest::newRow("pen-landscape") << QStringLiteral("pen") << 90;
        QTest::newRow("finger-landscape") << QStringLiteral("touch") << 270;
    }

    void pointPlacementAndMarkerReopeningPreserveTypedAndHandwrittenNotes()
    {
        QFETCH(QString, input);
        QFETCH(int, angle);
        BookNotesHarness h(input != "mouse");
        QVERIFY(h.window());
        QVERIFY2(h.importPdfBook(), qPrintable(h.notebook.error()));
        QCOMPARE(h.notebook.pageCount(), 2);
        QVERIFY(h.notebook.hasPageBackground());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *ink = h.find<InkItem>("inkPage");
        auto *dialog = h.find<QObject>("bookNoteDialog");
        auto *noteCanvas = h.find<InkItem>("bookNoteInk");
        QVERIFY(preferences && ink && dialog && noteCanvas);
        QVERIFY(preferences->setProperty("orientation", angle));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), angle);
        QTRY_VERIFY(ink->width() > 50 && ink->height() > 50);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "startBookNote"));
        QVERIFY(h.window()->property("placingBookNote").toBool());
        const QPointF anchor(.32, .41);
        QTest::qWait(100);
        tap(h.window(), input, ink->mapToScene(QPointF(anchor.x() * ink->width(), anchor.y() * ink->height())));
        QTRY_VERIFY(dialog->property("visible").toBool());
        // A closed Popup has not attached its content to the window's visual tree yet.
        QQuickItem *save = nullptr;
        QTRY_VERIFY((save = findVisualItem(h.window()->contentItem(), "bookNoteSave")));
        QVERIFY(!h.window()->property("placingBookNote").toBool());
        QVERIFY(h.notebook.strokes().isEmpty());
        QVERIFY(h.notebook.pageItems().isEmpty());
        QVERIFY(!dialog->property("canSave").toBool());
        const QPointF actualAnchor = dialog->property("anchorPoint").toPointF();
        QVERIFY(QLineF(anchor, actualAnchor).length() < .01);
        QVERIFY(dialog->setProperty("noteText", "Remember this paragraph"));
        QVERIFY(dialog->setProperty("noteColor", "blue"));
        QTRY_VERIFY(noteCanvas->width() > 20 && noteCanvas->height() > 20);
        QTest::qWait(100);
        drawNoteStroke(h.window(), noteCanvas);
        QTRY_COMPARE(qmlList(dialog, "noteInk").size(), 1);
        auto handwriting = qmlList(dialog, "noteInk");
        QVERIFY(handwriting.first().toMap().value("points").toList().size() >= 3);
        // The live pen stroke omits its default tool; stored page content names it explicitly.
        auto expectedStroke = handwriting.first().toMap();
        QCOMPARE(expectedStroke.value("tool", "pen").toString(), QStringLiteral("pen"));
        expectedStroke.insert("tool", "pen");
        handwriting[0] = expectedStroke;
        QVERIFY(h.notebook.strokes().isEmpty());
        QTRY_VERIFY(save->isEnabled());
        h.screenshot("book-note-mixed-" + input + "-" + QString::number(angle));
        click(h.window(), save);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().size(), 1);
        const auto original = h.notebook.pageItems().first().toMap();
        QCOMPARE(original.value("type").toString(), QStringLiteral("note"));
        QCOMPARE(original.value("text").toString(), QStringLiteral("Remember this paragraph"));
        QCOMPARE(original.value("noteColor").toString(), QStringLiteral("blue"));
        QCOMPARE(original.value("noteInk").toList(), handwriting);
        QVERIFY(qAbs(original.value("x").toDouble() + original.value("width").toDouble() / 2 - actualAnchor.x()) < .0001);
        QVERIFY(qAbs(original.value("y").toDouble() + original.value("height").toDouble() / 2 - actualAnchor.y()) < .0001);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "selectTool", Q_ARG(QString, QString("highlighter"))));
        QTRY_VERIFY(!noteCanvas->touchBlocked() && !ink->touchBlocked());
        const QPointF center((original.value("x").toDouble() + original.value("width").toDouble() / 2) * ink->width(),
            (original.value("y").toDouble() + original.value("height").toDouble() / 2) * ink->height());
        QCOMPARE(ink->noteItemAt(center), original.value("id").toString());
        tap(h.window(), input, ink->mapToScene(center));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QCOMPARE(qmlMap(dialog, "originalItem").value("id"), original.value("id"));
        QCOMPARE(dialog->property("noteText").toString(), original.value("text").toString());
        QCOMPARE(qmlList(dialog, "noteInk"), handwriting);
        QVERIFY(h.notebook.strokes().isEmpty());
        QVERIFY(dialog->setProperty("noteText", "Edited in place"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "save"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        const auto edited = h.notebook.pageItems().first().toMap();
        QCOMPARE(edited.value("id"), original.value("id"));
        QCOMPARE(edited.value("x"), original.value("x"));
        QCOMPARE(edited.value("y"), original.value("y"));
        QCOMPARE(edited.value("noteInk"), original.value("noteInk"));
        QCOMPARE(edited.value("text").toString(), QStringLiteral("Edited in place"));
        QVERIFY(h.notebook.undo());
        QCOMPARE(h.notebook.pageItems().first().toMap(), original);
    }

    void cancellingADraftNeverChangesTheBook_data()
    {
        QTest::addColumn<bool>("existing");
        QTest::newRow("new-note") << false;
        QTest::newRow("existing-note") << true;
    }

    void cancellingADraftNeverChangesTheBook()
    {
        QFETCH(bool, existing);
        BookNotesHarness h(false);
        QVERIFY(h.window());
        QVERIFY2(h.importPdfBook(), qPrintable(h.notebook.error()));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        if (existing) {
            QVERIFY(h.notebook.savePageNote(noteFixture(), h.notebook.bookId(), h.notebook.pageId()));
            QVERIFY(QMetaObject::invokeMethod(h.window(), "editBookNote", Q_ARG(QString, h.notebook.pageItems().first().toMap().value("id").toString())));
        } else QVERIFY(startAt(h));
        auto *dialog = h.find<QObject>("bookNoteDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        const auto before = h.notebook.exportBookForTransfer(h.notebook.bookId());
        QVERIFY(dialog->setProperty("noteText", "Unsaved changes"));
        QVERIFY(dialog->property("dirty").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "requestClose", Q_ARG(bool, false)));
        QCOMPARE(dialog->property("confirmation").toString(), QStringLiteral("discard"));
        QVERIFY(dialog->property("visible").toBool());
        QVERIFY(!dialog->property("canSave").toBool());
        QCOMPARE(h.notebook.exportBookForTransfer(h.notebook.bookId()), before);
        QVERIFY(QMetaObject::invokeMethod(dialog, "finishClose"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(!dialog->property("hasDraft").toBool());
        QCOMPARE(h.notebook.exportBookForTransfer(h.notebook.bookId()), before);
    }

    void quittingWithAnUnsavedNoteAsksOnce()
    {
        BookNotesHarness h(false);
        QVERIFY(h.window());
        QVERIFY2(h.importPdfBook(), qPrintable(h.notebook.error()));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QVERIFY(startAt(h));
        auto *dialog = h.find<QObject>("bookNoteDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(dialog->setProperty("noteText", "Unsaved note"));
        const auto saved = h.notebook.exportBookForTransfer(h.notebook.bookId());
        QVERIFY(!h.window()->close());
        QCOMPARE(dialog->property("confirmation").toString(), QStringLiteral("discard"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "finishClose"));
        QTRY_VERIFY(!h.window()->isVisible());
        QVERIFY(!dialog->property("hasDraft").toBool());
        QCOMPARE(h.notebook.exportBookForTransfer(h.notebook.bookId()), saved);
    }

    void privateDraftSurvivesLockAndStandbyWithoutSaving_data()
    {
        QTest::addColumn<bool>("lock");
        QTest::newRow("pin") << true;
        QTest::newRow("standby") << false;
    }

    void privateDraftSurvivesLockAndStandbyWithoutSaving()
    {
        QFETCH(bool, lock);
        BookNotesHarness h;
        QVERIFY(h.window());
        QVERIFY2(h.importPdfBook(), qPrintable(h.notebook.error()));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QVERIFY(startAt(h));
        auto *dialog = h.find<QObject>("bookNoteDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(dialog->setProperty("noteText", "Private unsaved note"));
        const auto original = h.notebook.exportBookForTransfer(h.notebook.bookId());
        if (lock) { QVERIFY(h.pin.setPin("1234")); h.pin.lock(); }
        else h.standby.suspend();
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(dialog->property("suspended").toBool());
        QVERIFY(dialog->property("dirty").toBool());
        QCOMPARE(dialog->property("noteText").toString(), QStringLiteral("Private unsaved note"));
        QVERIFY(!dialog->property("canSave").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "save"));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "startBookNote"));
        QTest::qWait(100);
        QVERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.exportBookForTransfer(h.notebook.bookId()), original);
        if (lock) QVERIFY(h.pin.unlock("1234"));
        else h.standby.wake();
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(!dialog->property("suspended").toBool());
        QCOMPARE(dialog->property("noteText").toString(), QStringLiteral("Private unsaved note"));
        QVERIFY(dialog->property("canSave").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "save"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().first().toMap().value("text").toString(), QStringLiteral("Private unsaved note"));
    }

    void draftCannotBeAppliedToAnotherPageOrBook()
    {
        BookNotesHarness h(false);
        QVERIFY(h.window());
        QVERIFY2(h.importPdfBook(), qPrintable(h.notebook.error()));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        const QString book = h.notebook.bookId(), page = h.notebook.pageId();
        QVERIFY(startAt(h));
        auto *dialog = h.find<QObject>("bookNoteDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(dialog->setProperty("noteText", "Anchored to the first page"));
        QVERIFY(h.notebook.goToPage(1));
        QVERIFY(!dialog->property("currentContext").toBool());
        QVERIFY(!dialog->property("canSave").toBool());
        const auto unchanged = h.notebook.exportBookForTransfer(book);
        QVERIFY(QMetaObject::invokeMethod(dialog, "save"));
        QCOMPARE(h.notebook.exportBookForTransfer(book), unchanged);
        QVERIFY(h.notebook.createBook("Different notebook"));
        const QString other = h.notebook.bookId();
        const auto otherBefore = h.notebook.exportBookForTransfer(other);
        QVERIFY(QMetaObject::invokeMethod(dialog, "save"));
        QCOMPARE(h.notebook.exportBookForTransfer(other), otherBefore);
        QCOMPARE(dialog->property("noteText").toString(), QStringLiteral("Anchored to the first page"));
        QVERIFY(h.notebook.openBook(book));
        QVERIFY(h.notebook.openPage(page));
        QTRY_VERIFY(dialog->property("currentContext").toBool());
        QVERIFY(dialog->property("canSave").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "save"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().first().toMap().value("text").toString(), QStringLiteral("Anchored to the first page"));
        QVERIFY(h.notebook.goToPage(1));
        QVERIFY(h.notebook.pageItems().isEmpty());
    }

    void externalKeyboardHidesTouchKeyboardButKeepsTypingAvailable()
    {
        BookNotesHarness h;
        QVERIFY(h.window());
        QVERIFY2(h.importPdfBook(), qPrintable(h.notebook.error()));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QVERIFY(startAt(h));
        auto *dialog = h.find<QObject>("bookNoteDialog");
        auto *text = h.find<QQuickItem>("bookNoteText");
        auto *keyboard = h.find<QQuickItem>("bookNoteKeyboard");
        QVERIFY(dialog && text && keyboard);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(dialog->setProperty("mode", "text"));
        text->forceActiveFocus();
        QTRY_VERIFY(keyboard->isVisible());
        QVERIFY(h.window()->setProperty("externalKeyboard", QVariantMap{{"connected", true}}));
        QTRY_VERIFY(!keyboard->isVisible());
        text->forceActiveFocus();
        QTest::keyClick(h.window(), Qt::Key_A);
        QTest::keyClick(h.window(), Qt::Key_B);
        QTRY_COMPARE(dialog->property("noteText").toString(), QStringLiteral("ab"));
        QVERIFY(dialog->property("canSave").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "save"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().first().toMap().value("text").toString(), QStringLiteral("ab"));
    }

    void editorFitsSmallMacAndRotatedTablet_data()
    {
        QTest::addColumn<bool>("tablet");
        QTest::addColumn<int>("angle");
        QTest::newRow("small-mac") << false << 0;
        QTest::newRow("portrait-tablet") << true << 0;
        QTest::newRow("landscape-tablet") << true << 90;
    }

    void editorFitsSmallMacAndRotatedTablet()
    {
        QFETCH(bool, tablet);
        QFETCH(int, angle);
        BookNotesHarness h(tablet);
        QVERIFY(h.window());
        if (!tablet) h.window()->resize(430, 620);
        QVERIFY2(h.importPdfBook(), qPrintable(h.notebook.error()));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences && preferences->setProperty("orientation", angle));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), angle);
        QVERIFY(startAt(h));
        auto *dialog = h.find<QObject>("bookNoteDialog");
        auto *text = h.find<QQuickItem>("bookNoteText");
        auto *save = findVisualItem(h.window()->contentItem(), "bookNoteSave");
        QVERIFY(dialog && text && save);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(dialog->setProperty("mode", "text"));
        QVERIFY(dialog->setProperty("noteText", "A note remains editable in this layout."));
        text->forceActiveFocus();
        QTest::qWait(150);
        const QRectF windowBounds(QPointF(0, 0), h.window()->size());
        QVERIFY(windowBounds.adjusted(-1, -1, 1, 1).contains(save->mapRectToScene(save->boundingRect())));
        QVERIFY(save->width() >= 40 && save->height() >= 40);
        QVERIFY(save->isEnabled());
        h.screenshot(tablet ? (angle ? "book-note-keyboard-landscape" : "book-note-keyboard-portrait") : "book-note-small-mac");
        click(h.window(), save);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().first().toMap().value("text").toString(), QStringLiteral("A note remains editable in this layout."));
    }

};

QTEST_MAIN(BookNotesIntegrationTests)
#include "booknotesintegration_tests.moc"
