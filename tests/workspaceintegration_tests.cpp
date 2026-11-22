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
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QPointingDevice>
#include <QSet>
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
#include "mixedpageanalyzer.h"
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
    if (!button || !button->isVisible() || !button->isEnabled() || button->width() <= 0 || button->height() <= 0)
        return false;
    const QPointF center = button->mapToScene(QPointF(button->width() / 2, button->height() / 2));
    if (!QRectF(QPointF(), h.window()->size()).contains(center)) return false;
    for (auto *parent = button->parentItem(); parent; parent = parent->parentItem())
        if (parent->clip() && !parent->contains(parent->mapFromScene(center))) return false;
    QSignalSpy clicked(button, SIGNAL(clicked()));
    QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, center.toPoint());
    return clicked.count() == 1;
}
bool waitForLayoutFrame(QQuickWindow *window)
{
    if (!QTest::qWaitForWindowExposed(window)) return false;
    QSignalSpy frame(window, &QQuickWindow::frameSwapped);
    window->update();
    return !frame.isEmpty() || frame.wait();
}
bool clickAction(WorkspaceHarness &h, const QString &key)
{
    if (!waitForLayoutFrame(h.window())) return false;
    const QString name = key == "diagramRecognize" ? QStringLiteral("recognizeDiagramButton") : "workspaceAction_" + key;
    auto *button = findVisualItem(h.window()->contentItem(), name);
    if (!button || !button->isVisible()) {
        auto *menu = h.find<QObject>("workspaceMoreMenu");
        if (!menu) return false;
        if (!clickItem(h, h.find<QQuickItem>("workspaceMoreButton"))) return false;
        if (!QTest::qWaitFor([&] { return menu->property("opened").toBool(); })) return false;
        if (!waitForLayoutFrame(h.window())) return false;
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
        REGISTER(MacDocumentExchange); REGISTER(MarkdownWeb); REGISTER(MixedPageAnalyzer); REGISTER(Notebook);
        REGISTER(ObsidianIntegration);
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

    void bookLinkCanBePlacedFollowedAndReturnToTheExactPage()
    {
        WorkspaceHarness h(false);
        QVERIFY(h.window());
        const auto chart = h.addChart(false);
        QVERIFY(!chart.isEmpty());
        const QString targetBook = h.notebook.bookId();
        const QString targetPage = h.notebook.pageId();
        QVERIFY(h.addBookBackground());
        QVERIFY(h.notebook.goToPage(1));
        const QString book = h.notebook.bookId();
        const QString page = h.notebook.pageId();
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QTRY_COMPARE(h.window()->property("penTool").toString(), QStringLiteral("read"));
        QVERIFY(!findVisualItem(h.window()->contentItem(), QStringLiteral("toolbarAction_pen")));

        auto *ink = h.find<QQuickItem>("inkPage");
        QVERIFY(ink);
        const auto clickPage = [&](QPointF normalized) {
            const QPointF point = ink->mapToScene(QPointF(ink->width() * normalized.x(), ink->height() * normalized.y()));
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, point.toPoint());
        };
        clickPage(QPointF(.45, .45));
        QVERIFY(h.notebook.strokes().isEmpty());
        QVERIFY(clickAction(h, "bookLink"));
        QTRY_COMPARE(h.window()->property("bookAnchorKind").toString(), QStringLiteral("link"));
        const QPointF anchor(.4, .4);
        clickPage(anchor);
        auto *picker = h.find<QObject>("linkTargetDialog");
        QVERIFY(picker);
        QTRY_VERIFY(picker->property("visible").toBool());
        QVariant target;
        for (const QVariant &entry : h.notebook.libraryItems())
            if (entry.toMap().value("itemId") == chart.value("id")) target = entry;
        QVERIFY(target.isValid());
        QVERIFY(QMetaObject::invokeMethod(picker, "save", Q_ARG(QVariant, target)));
        QTRY_VERIFY(!picker->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().size(), 1);
        const auto link = h.notebook.pageItems().first().toMap();
        QCOMPARE(link.value("type").toString(), QStringLiteral("link"));
        clickPage(QPointF(link.value("x").toDouble() + .01, link.value("y").toDouble() + .01));
        QTRY_COMPARE(h.notebook.bookId(), targetBook);
        QCOMPARE(h.notebook.pageId(), targetPage);
        QCOMPARE(h.window()->property("linkedPageItem").toString(), chart.value("id").toString());
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("charts"));
        QVERIFY(clickItem(h, h.find<QQuickItem>("editorBackButton")));
        QTRY_COMPARE(h.notebook.bookId(), book);
        QCOMPARE(h.notebook.pageId(), page);
        QCOMPARE(h.window()->property("penTool").toString(), QStringLiteral("read"));
        QVERIFY(h.notebook.strokes().isEmpty());
    }

    void writingLinkEditsTheOriginalAndReturnsToItsPage()
    {
        WorkspaceHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem(tableFixture("Original")));
        const auto targetId = h.notebook.pageItems().first().toMap().value("id").toString();
        const auto targetBook = h.notebook.bookId();
        const auto targetPage = h.notebook.pageId();
        QVERIFY(h.notebook.importMarkdownText(QStringLiteral("# Appunti\n\nTesto della nota."),
            QStringLiteral("Appunti"), {}));
        QVERIFY(h.notebook.addPage());
        const auto sourceBook = h.notebook.bookId();
        const auto sourcePage = h.notebook.pageId();
        QVERIFY(QMetaObject::invokeMethod(h.window(), "selectEditorWorkspace", Q_ARG(QString, QStringLiteral("writing"))));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QVERIFY(clickAction(h, "bookLink"));

        auto *ink = h.find<QQuickItem>("inkPage");
        auto *picker = h.find<QObject>("linkTargetDialog");
        QVERIFY(ink && picker);
        const auto clickPage = [&](QPointF normalized) {
            const QPointF position = ink->mapToScene({ink->width() * normalized.x(), ink->height() * normalized.y()});
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, position.toPoint());
        };
        clickPage({.4, .4});
        QTRY_VERIFY(picker->property("visible").toBool());
        QVariant target;
        for (const QVariant &entry : h.notebook.libraryItems())
            if (entry.toMap().value("itemId") == targetId)
                target = entry;
        QVERIFY(target.isValid());
        QVERIFY(QMetaObject::invokeMethod(picker, "save", Q_ARG(QVariant, target)));
        QTRY_VERIFY(!picker->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().size(), 1);
        const auto link = h.notebook.pageItems().first().toMap();
        QCOMPARE(link.value("type").toString(), QStringLiteral("link"));
        QCOMPARE(link.value("targetItemId").toString(), targetId);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));
        QTRY_VERIFY(h.window()->property("fullPage").toBool());
        clickPage({link.value("x").toDouble() + link.value("width").toDouble() / 2,
            link.value("y").toDouble() + link.value("height").toDouble() / 2});

        QTRY_COMPARE(h.notebook.bookId(), targetBook);
        QCOMPARE(h.notebook.pageId(), targetPage);
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("tables"));
        QVERIFY(!h.window()->property("fullPage").toBool());
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(editor);
        QVERIFY(!editor->property("visible").toBool());
        QVERIFY(clickAction(h, "linkedEdit"));
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(qmlMap(editor, "originalItem").value("id").toString(), targetId);
        auto *cell = findVisualItem(h.window()->contentItem(), QStringLiteral("tableCellInput"));
        QVERIFY(cell && cell->setProperty("text", QStringLiteral("Originale modificato")));
        QVERIFY(QMetaObject::invokeMethod(editor, "submit", Q_ARG(bool, false)));
        QTRY_VERIFY(!editor->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().first().toMap().value("cells").toList().first().toString(),
            QStringLiteral("Originale modificato"));

        QVERIFY(clickItem(h, h.find<QQuickItem>("editorBackButton")));
        QTRY_COMPARE(h.notebook.bookId(), sourceBook);
        QCOMPARE(h.notebook.pageId(), sourcePage);
        QCOMPARE(h.window()->property("editorWorkspaceOverride").toString(), QStringLiteral("writing"));
        QVERIFY(h.window()->property("fullPage").toBool());
        QCOMPARE(h.notebook.pageItems(), QVariantList{link});

        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, false)));
        QTest::qWait(80); // Allow the navigation and toolbar layout to settle after full-page mode.
        auto *linked = h.find<QObject>("workspaceLinkedItems");
        QVERIFY(linked);
        QVERIFY(clickItem(h, h.find<QQuickItem>("workspaceLinkedItemsButton")));
        QTRY_VERIFY(linked->property("visible").toBool());
        QTRY_COMPARE(qmlList(linked, "entries").size(), 1);
        const auto live = qmlList(linked, "entries").first().toMap().value("target").toMap();
        QCOMPARE(live.value("id").toString(), targetId);
        QCOMPARE(live.value("cells").toList().first().toString(), QStringLiteral("Originale modificato"));
        QQuickItem *directEdit = nullptr;
        QTRY_VERIFY((directEdit = findVisualItem(h.window()->contentItem(),
            QStringLiteral("workspaceLinkedEdit_") + link.value("id").toString())));
        QVERIFY(clickItem(h, directEdit));
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(qmlMap(editor, "originalItem").value("id").toString(), targetId);
    }

    void mixedPageReviewCreatesOneNotebookWithEditableLinkedPages()
    {
        WorkspaceHarness h;
        QVERIFY(h.window());
        const auto sourceBook = h.notebook.bookId();
        const QString originalText = QStringLiteral("# Procedura\n\nTesto di partenza.");
        const QString reviewedText = QStringLiteral("# Procedura\n\nTesto controllato prima del salvataggio.");
        QVERIFY(h.notebook.savePageItem({{"type", "text"}, {"text", originalText}}));
        const auto text = h.notebook.pageItems().last().toMap();
        const QVariantList originalCells{QString{}, QStringLiteral("Voce | descrizione"), QString{},
            QStringLiteral("1"), QStringLiteral("=SUM(A2:A2)"), QString{}, QString{}, QString{}, QString{}};
        QVERIFY(h.notebook.savePageItem({{"type", "table"}, {"rows", 3}, {"cols", 3}, {"cells", originalCells}}));
        const auto table = h.notebook.pageItems().last().toMap();
        QVERIFY(h.notebook.savePageItem({{"type", "diagram"}, {"nodes", QVariantList{
            QVariantMap{{"id", "start"}, {"shape", "terminal"}, {"text", "Inizio"},
                {"x", .08}, {"y", .10}, {"width", .25}, {"height", .18}},
            QVariantMap{{"id", "step"}, {"shape", "process"}, {"text", "Controllo"},
                {"x", .64}, {"y", .10}, {"width", .27}, {"height", .18}}}},
            {"edges", QVariantList{QVariantMap{{"id", "flow"}, {"from", "start"}, {"to", "step"}, {"text", "Continua"}}}}}));
        const auto diagram = h.notebook.pageItems().last().toMap();
        const QByteArray sourceData = h.notebook.exportBookForTransfer(sourceBook);
        const auto bookCount = h.notebook.books().size();
        QVERIFY(QMetaObject::invokeMethod(h.window(), "selectEditorWorkspace", Q_ARG(QString, QStringLiteral("writing"))));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QTRY_COMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
        QVERIFY(clickAction(h, "mixedPage"));

        auto *dialog = h.find<QObject>("mixedPageDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *picker = h.find<QQuickItem>("mixedPageRegionPicker");
        QVERIFY(picker);
        QTRY_VERIFY(picker->isEnabled() && picker->width() > 0 && picker->height() > 0);
        QPointingDevice marker("Mixed-page Marker", 843216, QInputDevice::DeviceType::Stylus,
            QPointingDevice::PointerType::Pen, QInputDevice::Capability::Position, 1, 1);
        ulong markerTime = 0;
        const auto markerEvent = [&](QEvent::Type type, QPoint position) {
            QMouseEvent event(type, position, position, h.window()->mapToGlobal(position),
                type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                Qt::NoModifier, Qt::MouseEventSynthesizedByApplication, &marker);
            event.setTimestamp(++markerTime);
            QCoreApplication::sendEvent(h.window(), &event);
        };
        const auto tap = picker->mapToScene({picker->width() * .3, picker->height() * .3}).toPoint();
        markerEvent(QEvent::MouseButtonPress, tap);
        markerEvent(QEvent::MouseButtonRelease, tap);
        QVERIFY(qmlMap(dialog, "regions").value("diagram").toList().isEmpty());
        const auto selectPart = [&](const QString &key) {
            return clickItem(h, findVisualItem(h.window()->contentItem(), "mixedPagePart_" + key));
        };
        const auto drawRegion = [&](const QString &key, const QVariantMap &item, bool useMarker) {
            if (!selectPart(key)) return false;
            const double x = item.value("x").toDouble(), y = item.value("y").toDouble();
            const double width = item.value("width").toDouble(), height = item.value("height").toDouble();
            const auto scenePoint = [&](double px, double py) {
                return picker->mapToScene(QPointF(px * picker->width(), py * picker->height())).toPoint();
            };
            const QList<QPoint> outline{
                scenePoint(x - .01, y + .01),
                scenePoint(x + .025, y - .01),
                scenePoint(x + width + .01, y - .01),
                scenePoint(x + width + .01, y + height + .01),
                scenePoint(x - .01, y + height + .01)
            };
            if (useMarker) {
                markerEvent(QEvent::MouseButtonPress, outline.first());
                for (qsizetype i = 1; i < outline.size() - 1; ++i)
                    markerEvent(QEvent::MouseMove, outline.at(i));
                markerEvent(QEvent::MouseButtonRelease, outline.last());
            } else {
                QTest::mousePress(h.window(), Qt::LeftButton, Qt::NoModifier, outline.first());
                for (qsizetype i = 1; i < outline.size() - 1; ++i)
                    QTest::mouseMove(h.window(), outline.at(i));
                QTest::mouseRelease(h.window(), Qt::LeftButton, Qt::NoModifier, outline.last());
            }
            return QTest::qWaitFor([&] { return qmlMap(dialog, "regions").value(key).toList().size() == 1; });
        };
        QVERIFY(drawRegion(QStringLiteral("diagram"), diagram, true));
        QVERIFY(drawRegion(QStringLiteral("table"), table, true));
        QVERIFY(drawRegion(QStringLiteral("text"), text, false));
        const auto selections = qmlMap(dialog, "regions");
        for (const auto &selection : selections) {
            const auto points = selection.toList().first().toMap().value("points").toList();
            QCOMPARE(points.size(), 5);
            for (const auto &value : points) {
                QVERIFY(value.canConvert<QPointF>());
                const auto point = value.toPointF();
                QVERIFY(QRectF(0, 0, 1, 1).contains(point));
            }
        }
        auto *continueButton = h.find<QQuickItem>("mixedPageContinue");
        QVERIFY(continueButton);
        QTRY_VERIFY(continueButton->isEnabled());
        QVERIFY(clickItem(h, continueButton));
        QTRY_VERIFY(dialog->property("reviewing").toBool());
        QTRY_VERIFY(!dialog->property("readingInk").toBool());
        QCOMPARE(qmlMap(dialog, "diagramDraft").value("nodes"), diagram.value("nodes"));
        auto *tableField = h.find<QQuickItem>("mixedPageTable");
        auto *textField = h.find<QQuickItem>("mixedPageText");
        QVERIFY(tableField && textField);
        QCOMPARE(tableField->property("text").toString(), QStringLiteral("\tVoce | descrizione\t\n1\t=SUM(A2:A2)\t\n\t\t"));
        QCOMPARE(textField->property("text").toString(), originalText);
        QVERIFY(textField->setProperty("text", reviewedText));
        QCOMPARE(h.notebook.books().size(), bookCount);
        QTRY_VERIFY(continueButton->isEnabled());
        QVERIFY(clickItem(h, continueButton));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.books().size(), bookCount + 1);
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
        QCOMPARE(h.notebook.markdownText().trimmed(), reviewedText);
        QCOMPARE(h.notebook.exportBookForTransfer(sourceBook), sourceData);
        QCOMPARE(h.notebook.pageCount(), 3);

        const auto writingBook = h.notebook.bookId();
        const auto writingPage = h.notebook.pageId();
        QVERIFY(writingBook != sourceBook);
        QHash<QString, QVariantMap> links;
        QSet<QString> pages{writingPage};
        for (const QVariant &entry : h.notebook.pageItems()) {
            const auto link = entry.toMap();
            if (link.value("type").toString() != QStringLiteral("link")) continue;
            const auto target = h.notebook.linkedPageItem(link.value("id").toString());
            QVERIFY(!target.isEmpty());
            QCOMPARE(target.value("id"), link.value("targetItemId"));
            QCOMPARE(target.value("bookId").toString(), writingBook);
            const auto kind = target.value("type").toString();
            if (kind == QStringLiteral("table")) {
                QCOMPARE(target.value("rows").toInt(), 3);
                QCOMPARE(target.value("cols").toInt(), 3);
                QCOMPARE(target.value("cells").toList(), originalCells);
            } else if (kind == QStringLiteral("diagram"))
                QCOMPARE(target.value("edges"), diagram.value("edges"));
            QVERIFY(!links.contains(kind));
            links.insert(kind, link);
            pages.insert(target.value("pageId").toString());
        }
        QCOMPARE(links.size(), 2);
        QCOMPARE(pages.size(), 3);

        auto *linked = h.find<QObject>("workspaceLinkedItems");
        QVERIFY(linked);
        const auto editLinked = [&](const QVariantMap &link) {
            if (!clickItem(h, h.find<QQuickItem>("workspaceLinkedItemsButton"))) return false;
            if (!QTest::qWaitFor([&] { return linked->property("opened").toBool(); })) return false;
            const QString name = QStringLiteral("workspaceLinkedEdit_") + link.value("id").toString();
            QQuickItem *button = nullptr;
            return QTest::qWaitFor([&] { return (button = findVisualItem(h.window()->contentItem(), name)); })
                && clickItem(h, button);
        };
        const auto diagramLink = links.value(QStringLiteral("diagram"));
        QVERIFY(editLinked(diagramLink));
        auto *diagramEditor = h.find<QObject>("diagramEditor");
        QVERIFY(diagramEditor);
        QTRY_VERIFY(diagramEditor->property("opened").toBool());
        QCOMPARE(h.notebook.bookId(), writingBook);
        QCOMPARE(h.notebook.pageId(), diagramLink.value("targetPageId").toString());
        QCOMPARE(h.notebook.pageItems().first().toMap().value("id"), diagramLink.value("targetItemId"));
        auto *canvas = findVisualItem(h.window()->contentItem(), QStringLiteral("diagramCanvas"));
        QVERIFY(canvas);
        const auto point = canvas->mapToScene({.205 * canvas->width(), .19 * canvas->height()}).toPoint();
        auto *touchscreen = QTest::createTouchDevice();
        QTest::touchEvent(h.window(), touchscreen).press(0, point, h.window());
        QTest::touchEvent(h.window(), touchscreen).release(0, point, h.window());
        QTRY_COMPARE(diagramEditor->property("selectedNode").toString(), QStringLiteral("start"));
        QQuickItem *labelButton = nullptr;
        QTRY_VERIFY((labelButton = findVisualItem(h.window()->contentItem(), QStringLiteral("diagramEditLabel"))));
        QVERIFY(clickItem(h, labelButton));
        const auto diagramControl = [&](const char *property, const QString &value) -> QQuickItem * {
            for (auto *item : diagramEditor->findChildren<QQuickItem *>()) {
                if (item->isVisible() && item->property(property).toString() == value)
                    return item;
            }
            return nullptr;
        };
        QQuickItem *labelInput = nullptr;
        QTRY_VERIFY((labelInput = diagramControl("placeholderText", QStringLiteral("Testo dell’etichetta"))));
        const QString newLabel = QStringLiteral("Avvio verificato");
        QVERIFY(labelInput->setProperty("text", newLabel));
        QVERIFY(clickItem(h, diagramControl("text", QStringLiteral("Applica"))));
        QTRY_VERIFY(!labelInput->isVisible());
        QVERIFY(clickItem(h, diagramControl("text", QStringLiteral("Salva"))));
        QTRY_VERIFY(!diagramEditor->property("visible").toBool());
        QVERIFY(clickItem(h, h.find<QQuickItem>("editorBackButton")));
        QTRY_COMPARE(h.notebook.pageId(), writingPage);
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
        const auto editedDiagram = h.notebook.linkedPageItem(diagramLink.value("id").toString());
        QCOMPARE(editedDiagram.value("id"), diagramLink.value("targetItemId"));
        QCOMPARE(editedDiagram.value("nodes").toList().first().toMap().value("text").toString(), newLabel);

        const auto tableLink = links.value(QStringLiteral("table"));
        QVERIFY(editLinked(tableLink));
        auto *tableEditor = h.find<QObject>("pageItemDialog");
        QVERIFY(tableEditor);
        QTRY_VERIFY(tableEditor->property("opened").toBool());
        QCOMPARE(h.notebook.pageId(), tableLink.value("targetPageId").toString());
        QCOMPARE(qmlMap(tableEditor, "originalItem").value("id"), tableLink.value("targetItemId"));
        auto *tableCell = findVisualItem(h.window()->contentItem(), QStringLiteral("tableCellInput"));
        const QString newHeader = QStringLiteral("Intestazione verificata");
        QVERIFY(tableCell && tableCell->setProperty("text", newHeader));
        QVERIFY(QMetaObject::invokeMethod(tableEditor, "submit", Q_ARG(bool, false)));
        QTRY_VERIFY(!tableEditor->property("visible").toBool());
        QVERIFY(clickItem(h, h.find<QQuickItem>("editorBackButton")));
        QTRY_COMPARE(h.notebook.pageId(), writingPage);
        const auto editedTable = h.notebook.linkedPageItem(tableLink.value("id").toString());
        QCOMPARE(editedTable.value("id"), tableLink.value("targetItemId"));
        QCOMPARE(editedTable.value("cells").toList().first().toString(), newHeader);
        QCOMPARE(h.notebook.exportBookForTransfer(sourceBook), sourceData);
    }

    void chartMenuCanUseASeparateTableWithoutEmbeddingTheSource()
    {
        WorkspaceHarness h(false);
        QVERIFY(h.window());
        auto table = tableFixture(QStringLiteral("Source label"));
        table.insert("rows", 3);
        table.insert("cells", QVariantList{"Month", "Value", "1", "-3", "2", "=SUM(B2:B2)"});
        QVERIFY(h.notebook.savePageItem(table));
        const QString sourceBook = h.notebook.bookId();
        const QByteArray original = h.notebook.exportBookForTransfer(sourceBook);
        QVERIFY(h.notebook.createBook("Charts"));
        const auto chart = h.addChart(false);
        QVERIFY(!chart.isEmpty());
        QVERIFY(openEntry(h, chart.value("id").toString()));
        QTRY_COMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("charts"));
        QVERIFY(clickAction(h, "chartSources"));
        auto *dialog = h.find<QObject>("tableChartDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(dialog->property("sourceIsSnapshot").toBool());
        QQuickItem *save = nullptr;
        QTRY_VERIFY((save = findVisualItem(h.window()->contentItem(), QStringLiteral("saveTableChart"))));
        QVERIFY(clickItem(h, save));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().size(), 2);
        const auto result = h.notebook.pageItems().last().toMap();
        QCOMPARE(result.value("type").toString(), QStringLiteral("chart"));
        QVERIFY(result.value("sourceTable").toString().isEmpty());
        QCOMPARE(result.value("points").toList().size(), 2);
        QCOMPARE(h.notebook.exportBookForTransfer(sourceBook), original);
        QVERIFY(clickAction(h, "chartInk"));
        QTRY_COMPARE(h.window()->property("penTool").toString(), QStringLiteral("pen"));
        QVERIFY(clickAction(h, "lasso"));
        QTRY_VERIFY(h.window()->property("lassoActive").toBool());
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
