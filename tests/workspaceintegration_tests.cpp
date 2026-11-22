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
    if (!parent->isVisible()) return nullptr;
    if (parent->objectName() == name) return parent;
    for (QQuickItem *child : parent->childItems())
        if (auto *found = findVisualItem(child, name)) return found;
    return nullptr;
}
struct WorkspaceHarness {
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

    explicit WorkspaceHarness(bool tablet = true)
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
    bool addStroke()
    {
        return notebook.appendStroke({{"tool","pen"},{"color","#222222"},{"width",2.5},
            {"points",QVariantList{QVariantMap{{"x",.3},{"y",.4},{"p",.6}}, QVariantMap{{"x",.5},{"y",.5},{"p",.6}}}}});
    }
    QVariantMap addChart(bool linked)
    {
        if (linked) {
            if (!notebook.savePageItem({{"type", "table"}, {"rows", 2}, {"cols", 2},
                {"cells", QVariantList{"1", "2", "3", "=SUM(A1:A2)"}}})) return {};
            const QString tableId = notebook.pageItems().last().toMap().value("id").toString();
            if (!notebook.saveTableChart({{"sourceTable", tableId}, {"xColumn", 1}, {"yColumn", 2},
                {"hasHeader", false}, {"chartKind", "line"}, {"title", "Linked chart"}})) return {};
        } else if (!notebook.savePageItem({{"type", "chart"}, {"chartKind", "line"},
            {"title", "Standalone chart"}, {"points", QVariantList{
                QVariantMap{{"x", .2}, {"y", .3}}, QVariantMap{{"x", .7}, {"y", .8}}}}})) return {};
        return notebook.pageItems().last().toMap();
    }
    bool addBookBackground()
    {
        QImage image(600, 800, QImage::Format_RGB32);
        image.fill(Qt::white);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "JPEG")) return false;
        const QJsonObject page{{"template","blank"},{"strokes",QJsonArray{}},{"items",QJsonArray{}},
                               {"background",QString::fromLatin1(bytes.toBase64())}};
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QJsonObject book{{"id",id},{"title","Gesture fixture"},{"pages",QJsonArray{page,page}},
            {"pageIds",QJsonArray{QUuid::createUuid().toString(QUuid::WithoutBraces), QUuid::createUuid().toString(QUuid::WithoutBraces)}}};
        const QJsonObject root{{"version",9},{"books",QJsonArray{book}},{"folders",QJsonArray{}},{"activeBook",id},{"page",0}};
        return notebook.importFromData(QJsonDocument(root).toJson()) && notebook.openBook(id);
    }
};


QVariantMap tableFixture(const QString &label)
{
    return {{"type", "table"}, {"rows", 2}, {"cols", 2},
        {"cells", QVariantList{label, "Value", "1", "=SUM(A2:A2)"}}};
}
bool openEntry(WorkspaceHarness &h, const QString &itemId)
{
    for (const QVariant &entry : h.notebook.libraryItems())
        if (entry.toMap().value("itemId").toString() == itemId)
            return QMetaObject::invokeMethod(h.window(), "openLibraryItem", Q_ARG(QVariant, entry));
    return false;
}
bool clickItem(WorkspaceHarness &h, QQuickItem *button)
{
    if (!button || !button->isVisible() || !button->isEnabled()) return false;
    const QPointF center = button->mapToScene(QPointF(button->width() / 2, button->height() / 2));
    if (!QRectF(QPointF(), h.window()->size()).contains(center)) return false;
    QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, center.toPoint());
    return true;
}
bool clickAction(WorkspaceHarness &h, const QString &key)
{
    // Resizing and applying the interface scale update the toolbar's geometry on the next frame.
    QTest::qWait(80);
    const QString name = key == "diagramRecognize" ? QStringLiteral("recognizeDiagramButton") : "workspaceAction_" + key;
    auto *button = findVisualItem(h.window()->contentItem(), name);
    if (!button || !button->isVisible()) {
        auto *menu = h.find<QObject>("workspaceMoreMenu");
        if (!menu) return false;
        if (!clickItem(h, h.find<QQuickItem>("workspaceMoreButton"))) return false;
        if (!QTest::qWaitFor([&] { return menu->property("opened").toBool(); })) return false;
        QTest::qWait(80); // Position popup rows before simulating the second tap.
        button = findVisualItem(h.window()->contentItem(), name);
    }
    return clickItem(h, button);
}
}

class WorkspaceIntegrationTests : public QObject
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


    void categoryActionsOpenTheAppropriateWorkflow_data()
    {
        QTest::addColumn<QString>("category");
        QTest::addColumn<QString>("action");
        QTest::addColumn<QString>("dialog");
        QTest::newRow("books") << QStringLiteral("books") << QStringLiteral("highlighter") << QString{};
        QTest::newRow("writing") << QStringLiteral("writing") << QStringLiteral("text") << QStringLiteral("pageItemDialog");
        QTest::newRow("charts") << QStringLiteral("charts") << QStringLiteral("chartEdit") << QStringLiteral("chartEditor");
        QTest::newRow("tables") << QStringLiteral("tables") << QStringLiteral("tableEdit") << QStringLiteral("pageItemDialog");
        QTest::newRow("diagrams") << QStringLiteral("diagrams") << QStringLiteral("diagramNew") << QStringLiteral("diagramEditor");
    }

    void categoryActionsOpenTheAppropriateWorkflow()
    {
        QFETCH(QString, category);
        QFETCH(QString, action);
        QFETCH(QString, dialog);
        WorkspaceHarness h;
        QVERIFY(h.window());
        if (category == "books") QVERIFY(h.addBookBackground());
        else if (category == "charts") {
            const auto chart = h.addChart(false);
            QVERIFY(!chart.isEmpty());
            QVERIFY(openEntry(h, chart.value("id").toString()));
        } else if (category == "tables") {
            QVERIFY(h.notebook.savePageItem(tableFixture("Table A")));
            QVERIFY(openEntry(h, h.notebook.pageItems().first().toMap().value("id").toString()));
        } else if (category == "diagrams") QVERIFY(h.notebook.createDiagramBook("Workflow", {}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QTRY_COMPARE(h.window()->property("editorWorkspace").toString(), category);
        auto *tools = h.find<QObject>("workspaceTools");
        QVERIFY(tools);
        QCOMPARE(tools->property("workspace").toString(), category);
        const auto before = h.notebook.exportBookForTransfer(h.notebook.bookId());
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        QVERIFY(clickAction(h, action));
        if (category == "books") QCOMPARE(h.window()->property("penTool").toString(), QStringLiteral("highlighter"));
        else {
            auto *editor = h.find<QObject>(qPrintable(dialog));
            QVERIFY(editor);
            QTRY_VERIFY(editor->property("visible").toBool());
            if (category == "tables") QCOMPARE(qmlMap(editor, "originalItem").value("id"), h.notebook.pageItems().first().toMap().value("id"));
            if (category == "charts") QCOMPARE(qmlMap(editor, "originalItem").value("id"), h.notebook.pageItems().first().toMap().value("id"));
            QVERIFY(QMetaObject::invokeMethod(editor, "close"));
            QTRY_VERIFY(!editor->property("visible").toBool());
        }
        QCOMPARE(h.notebook.exportBookForTransfer(h.notebook.bookId()), before);
        h.screenshot("workspace-" + category);
    }

    void entryPointContextSurvivesPagesButNeverLeaksToAnotherBook()
    {
        WorkspaceHarness h;
        QVERIFY(h.window());
        const auto chart = h.addChart(false);
        QVERIFY(!chart.isEmpty());
        const QString first = h.notebook.bookId();
        QVERIFY(openEntry(h, chart.value("id").toString()));
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("charts"));
        QVERIFY(h.notebook.addPage());
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("charts"));
        QVERIFY(h.notebook.createBook("Writing after chart"));
        const QString second = h.notebook.bookId();
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
        QVERIFY(h.notebook.openBook(first));
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
        QVERIFY(openEntry(h, chart.value("id").toString()));
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("charts"));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showBook", Q_ARG(QString, second)));
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showBook", Q_ARG(QString, first)));
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
    }

    void multipleTablesRequireChoiceAndOpenTheChosenFormulaEditor()
    {
        WorkspaceHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem(tableFixture("First")));
        QVERIFY(h.notebook.savePageItem(tableFixture("Second")));
        const QString first = h.notebook.pageItems().first().toMap().value("id").toString();
        const QString second = h.notebook.pageItems().last().toMap().value("id").toString();
        QVERIFY(openEntry(h, first));
        const QByteArray before = h.notebook.exportBookForTransfer(h.notebook.bookId());
        QVERIFY(clickAction(h, "tableEdit"));
        auto *picker = h.find<QObject>("workspaceItemPicker");
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(picker && editor);
        QTRY_VERIFY(picker->property("visible").toBool());
        QVERIFY(h.window()->property("editorPopupOpen").toBool());
        QVERIFY(!editor->property("visible").toBool());
        QCOMPARE(qmlList(picker, "choices").size(), 2);
        h.screenshot("workspace-table-choice");
        QVERIFY(QMetaObject::invokeMethod(picker, "choose", Q_ARG(QString, second)));
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(qmlMap(editor, "originalItem").value("id").toString(), second);
        QCOMPARE(qmlList(editor, "cellDraft").last().toString(), QStringLiteral("=SUM(A2:A2)"));
        QCOMPARE(h.notebook.exportBookForTransfer(h.notebook.bookId()), before);
    }

    void staleTargetChoiceCannotEditADifferentPageOrWhileLocked_data()
    {
        QTest::addColumn<QString>("transition");
        QTest::newRow("page") << QStringLiteral("page");
        QTest::newRow("book") << QStringLiteral("book");
        QTest::newRow("lock") << QStringLiteral("lock");
        QTest::newRow("standby") << QStringLiteral("standby");
    }

    void staleTargetChoiceCannotEditADifferentPageOrWhileLocked()
    {
        QFETCH(QString, transition);
        WorkspaceHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem(tableFixture("First")));
        QVERIFY(h.notebook.savePageItem(tableFixture("Second")));
        const QString selected = h.notebook.pageItems().last().toMap().value("id").toString();
        QVERIFY(openEntry(h, selected));
        QVERIFY(clickAction(h, "tableEdit"));
        auto *picker = h.find<QObject>("workspaceItemPicker");
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(picker && editor);
        QTRY_VERIFY(picker->property("visible").toBool());
        if (transition == "page") QVERIFY(h.notebook.addPage());
        else if (transition == "book") QVERIFY(h.notebook.createBook("Other context"));
        else if (transition == "lock") {
            QVERIFY(h.pin.setPin("1234"));
            h.pin.lock();
        } else h.standby.suspend();
        QTRY_VERIFY(!picker->property("visible").toBool());
        const auto unchanged = h.notebook.exportBookForTransfer(h.notebook.bookId());
        QVERIFY(QMetaObject::invokeMethod(picker, "choose", Q_ARG(QString, selected)));
        QTest::qWait(80);
        QVERIFY(!editor->property("visible").toBool());
        QCOMPARE(h.notebook.exportBookForTransfer(h.notebook.bookId()), unchanged);
    }

    void booksOfferAnnotatedPagesAndWholeBookSyncWithoutChangingContent()
    {
        WorkspaceHarness h;
        QVERIFY(h.window());
        QVERIFY(h.addBookBackground());
        QVERIFY(h.notebook.appendStroke({{"tool", "highlighter"}, {"color", "#d9cb59"}, {"width", 9},
            {"points", QVariantList{QVariantMap{{"x", .2}, {"y", .3}, {"p", .7}}, QVariantMap{{"x", .7}, {"y", .3}, {"p", .7}}}}}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        const auto original = h.notebook.exportBookForTransfer(h.notebook.bookId());
        QVERIFY(clickAction(h, "annotatedPages"));
        auto *overview = h.find<QObject>("pageOverview");
        QVERIFY(overview);
        QTRY_VERIFY(overview->property("visible").toBool());
        QCOMPARE(overview->property("pageFilter").toString(), QStringLiteral("annotated"));
        QCOMPARE(overview->property("annotatedCount").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(overview, "close"));
        QTRY_VERIFY(!overview->property("visible").toBool());
        QVERIFY(clickAction(h, "bookSync"));
        auto *syncDialog = h.find<QObject>("wirelessSyncDialog");
        QVERIFY(syncDialog);
        QTRY_VERIFY(syncDialog->property("visible").toBool());
        QVERIFY(!h.wireless.transfer()->busy());
        QCOMPARE(h.notebook.exportBookForTransfer(h.notebook.bookId()), original);
    }

    void narrowAndFullPageLayoutsKeepToolsReachable_data()
    {
        QTest::addColumn<bool>("landscape");
        QTest::newRow("narrow") << false;
        QTest::newRow("landscape") << true;
    }

    void narrowAndFullPageLayoutsKeepToolsReachable()
    {
        QFETCH(bool, landscape);
        WorkspaceHarness h(landscape);
        QVERIFY(h.window());
        h.window()->resize(landscape ? 700 : 380, landscape ? 1000 : 850);
        QVERIFY(h.addBookBackground());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences);
        preferences->setInterfaceScale(130);
        if (landscape) preferences->setOrientation(90);
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), landscape ? 90 : 0);
        auto *tools = h.find<QQuickItem>("workspaceTools");
        auto *row = h.find<QQuickItem>("workspaceActionsRow");
        auto *more = h.find<QQuickItem>("workspaceMoreButton");
        auto *highlighter = findVisualItem(h.window()->contentItem(), QStringLiteral("workspaceAction_highlighter"));
        auto *note = findVisualItem(h.window()->contentItem(), QStringLiteral("workspaceAction_bookNote"));
        QVERIFY(tools && row && more && highlighter && note);
        QTRY_VERIFY(more->width() > 0);
        for (const auto *action : {highlighter, note, more}) {
            QTRY_VERIFY(action->isVisible());
            QTRY_VERIFY(action->mapToItem(row, QPointF()).x() >= 0);
            QTRY_VERIFY(action->mapToItem(row, QPointF(action->width(), 0)).x() <= row->width() + 1);
            const QPointF center = action->mapToScene(QPointF(action->width() / 2, action->height() / 2));
            QVERIFY(QRectF(QPointF(), h.window()->size()).contains(center));
        }
        if (!landscape) {
            for (const auto *name : {"readingPreferencesButton", "readingBookmarksButton"}) {
                auto *button = findVisualItem(h.window()->contentItem(), QString::fromLatin1(name));
                QVERIFY(button);
                QTRY_VERIFY(QRectF(QPointF(), h.window()->size()).contains(
                    button->mapToScene(QPointF(button->width() / 2, button->height() / 2))));
            }
        }
        QVERIFY(!findVisualItem(h.window()->contentItem(), QStringLiteral("workspaceActionsViewport")));
        QVERIFY(clickAction(h, "underline"));
        QTRY_COMPARE(h.window()->property("penTool").toString(), QStringLiteral("underline"));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));
        QTRY_VERIFY(!tools->isVisible());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, false)));
        QTRY_VERIFY(tools->isVisible());
        QVERIFY(clickAction(h, "annotatedPages"));
        auto *overview = h.find<QObject>("pageOverview");
        QVERIFY(overview);
        QTRY_VERIFY(overview->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(overview, "close"));
        QTRY_VERIFY(!overview->property("visible").toBool());
        h.screenshot(landscape ? "workspace-books-landscape" : "workspace-books-narrow");
    }

    void overflowSupportsKeyboardAndDismissesOnContextChange()
    {
        WorkspaceHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.addBookBackground());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto *more = h.find<QQuickItem>("workspaceMoreButton");
        auto *menu = h.find<QObject>("workspaceMoreMenu");
        auto *overview = h.find<QObject>("pageOverview");
        QVERIFY(more && menu && overview);
        more->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(h.window(), Qt::Key_Space);
        QTRY_VERIFY(menu->property("visible").toBool());
        QVERIFY(h.window()->property("editorPopupOpen").toBool());
        QQuickItem *annotated = nullptr;
        QTRY_VERIFY((annotated = findVisualItem(h.window()->contentItem(), QStringLiteral("workspaceAction_annotatedPages"))));
        QTRY_VERIFY(annotated->hasActiveFocus());
        auto *sync = findVisualItem(h.window()->contentItem(), QStringLiteral("workspaceAction_bookSync"));
        QVERIFY(sync);
        QTest::keyClick(h.window(), Qt::Key_Down);
        QTRY_VERIFY(sync->hasActiveFocus());
        QTest::keyClick(h.window(), Qt::Key_Up);
        QTRY_VERIFY(annotated->hasActiveFocus());
        QTest::keyClick(h.window(), Qt::Key_Space);
        QTRY_VERIFY(overview->property("visible").toBool());
        QCOMPARE(overview->property("pageFilter").toString(), QStringLiteral("annotated"));
        QVERIFY(!menu->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(overview, "close"));
        QTRY_VERIFY(!overview->property("visible").toBool());

        QVERIFY(clickItem(h, more));
        QTRY_VERIFY(menu->property("visible").toBool());
        QVERIFY(h.notebook.addPage());
        QTRY_VERIFY(!menu->property("visible").toBool());
        QVERIFY(!overview->property("visible").toBool());
    }

};

QTEST_MAIN(WorkspaceIntegrationTests)
#include "workspaceintegration_tests.moc"
