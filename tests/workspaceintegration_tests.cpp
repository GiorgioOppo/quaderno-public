#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <algorithm>
#include <memory>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSValue>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
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
#include "smtpmail.h"
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
QQuickItem *findRemoteControl(QQuickItem *parent, const QStringList &labels, const char *permission, const char *label)
{
    if (!parent || !parent->isVisible()) return nullptr;
    if (parent->property(permission).toBool() && labels.contains(parent->property(label).toString()))
        return parent;
    for (auto *child : parent->childItems())
        if (auto *found = findRemoteControl(child, labels, permission, label)) return found;
    return nullptr;
}
QQuickItem *findRemoteAction(QQuickItem *parent, const QStringList &labels)
{
    return findRemoteControl(parent, labels, "remoteClickable", "text");
}

class FakeScreenStream final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(bool available READ active NOTIFY stateChanged)
    Q_PROPERTY(bool remoteNavigationAllowed READ remoteNavigationAllowed NOTIFY stateChanged)
    Q_PROPERTY(QString remoteSection READ remoteSection NOTIFY sectionChanged)
    Q_PROPERTY(QString status READ status CONSTANT)
    Q_PROPERTY(QUrl frameSource READ frameSource NOTIFY frameChanged)
    Q_PROPERTY(int frameOrientation READ frameOrientation NOTIFY frameChanged)

public:
    bool active() const { return m_active; }
    bool remoteNavigationAllowed() const { return m_allowed; }
    QString remoteSection() const { return m_section; }
    QString status() const { return {}; }
    QUrl frameSource() const { return m_frameSource; }
    int frameOrientation() const { return m_frameOrientation; }
    QRect workArea;

    void showFrame(const QUrl &source, int orientation)
    {
        m_frameSource = source;
        m_frameOrientation = orientation;
        emit frameChanged();
    }

    Q_INVOKABLE QRect viewerAvailableArea(QWindow *) const { return workArea; }
    Q_INVOKABLE void setViewerOpen(bool) {}
    Q_INVOKABLE void requestTap(qreal x, qreal y) { emit viewerTapped(x, y); }

    void allowRemoteNavigation(bool allowed)
    {
        m_allowed = allowed;
        emit stateChanged();
    }

    Q_INVOKABLE void reportSection(const QString &section)
    {
        m_section = section;
        emit sectionChanged();
    }

    Q_INVOKABLE void stop()
    {
        m_active = false;
        emit stateChanged();
    }

signals:
    void stateChanged();
    void sectionChanged();
    void frameChanged();
    void viewerTapped(qreal x, qreal y);
    void remoteTapRequested(qreal x, qreal y);
    void remoteKeyRequested(const QString &action, const QString &text);

private:
    bool m_active = true;
    bool m_allowed = true;
    QString m_section;
    QUrl m_frameSource;
    int m_frameOrientation = 0;
};

QByteArray smallRasterDiagram()
{
    QImage image(1200, 1600, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(4.0 / 3, 4.0 / 3);
    painter.setPen(QPen(Qt::black, 2.3));
    for (const auto &box : {QRectF(108, 144, 185, 75), QRectF(135, 278, 50, 42),
                            QRectF(315, 145, 106, 215), QRectF(479, 151, 208, 193),
                            QRectF(690, 151, 38, 193), QRectF(225, 393, 195, 74)}) {
        painter.drawRect(box);
    }
    painter.drawLine(QPointF(421, 245), QPointF(479, 245));

    const QHash<QChar, QList<QPolygonF>> letters{
        {u'A', {{{0, 10}, {3.6, 0}, {7.5, 10}}, {{1.5, 6.5}, {6, 6.3}}}},
        {u'C', {{{7.2, 1}, {4.1, 0}, {1.4, 1.4}, {0.2, 6.5}, {2.2, 9.6}, {5.3, 10}, {7.4, 8.6}}}},
        {u'O', {{{3.6, 0.2}, {1, 1.9}, {0.3, 6.9}, {2.6, 9.7}, {5.6, 9.7}, {7.8, 6.4}, {7.2, 2}, {3.6, 0.2}}}},
        {u'P', {{{0.8, 10}, {0.6, 0.4}, {4.7, 0.1}, {7.2, 1.3}, {7.5, 4.1}, {5.1, 5.4}, {0.9, 5.3}}}},
        {u'R', {{{0.2, 9.8}, {0, 0.4}, {4.5, 0.1}, {7.2, 1.5}, {7.0, 4.0}, {4.8, 5.2}, {0.6, 5.4}},
                {{3, 5.3}, {8, 10.0}}}},
        {u'S', {{{7.3, 1.3}, {4.2, 0.0}, {0.7, 1.7}, {1.4, 4.0}, {6.4, 5.8}, {7.4, 7.9}, {5.5, 10}, {0.7, 9.2}}}},
        {u'T', {{{0.1, 0.5}, {7.6, 0.4}}, {{4.0, 0.6}, {3.7, 10.0}}}},
        {u'U', {{{0.3, 0}, {0.4, 7.2}, {2.2, 9.6}, {4.6, 10}, {7, 8.6}, {7.6, 0.5}}}}
    };
    const auto draw = [&](const QString &word, QPointF center, qreal degrees, qreal scale) {
        painter.save();
        painter.translate(center);
        painter.rotate(degrees);
        painter.scale(scale, scale);
        painter.translate(-(word.size() * 11 - 3) / 2.0, -5);
        painter.setPen(QPen(Qt::black, 1.65, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        for (const auto letter : word) {
            for (const auto &stroke : letters.value(letter))
                painter.drawPolyline(stroke);
            painter.translate(11, 0);
        }
        painter.restore();
    };
    draw(QStringLiteral("CPU"), {200, 181}, 0, .58);
    draw(QStringLiteral("CPU"), {161, 328}, 0, .64);
    draw(QStringLiteral("START"), {368, 251}, 90, .87);
    draw(QStringLiteral("STOP"), {582, 247}, -90, 1.05);
    draw(QStringLiteral("S"), {709, 230}, 0, .88);
    draw(QStringLiteral("O"), {710, 244}, 0, .90);
    draw(QStringLiteral("A"), {708, 260}, 0, .85);
    draw(QStringLiteral("R"), {709, 275}, 0, .89);
    painter.end();

    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
        return {};
    return png;
}

struct WorkspaceHarness {
    QTemporaryDir directory;
    AppLanguage language{nullptr};
    Notebook notebook;
    AppleShare share{true};
    SmtpMail smtp{directory.filePath("smtp")};
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

    explicit WorkspaceHarness(bool tablet = true, const QString &existingLibrary = {})
        : notebook(existingLibrary.isEmpty() ? directory.filePath("notebooks.json") : existingLibrary)
    {
        if (existingLibrary.isEmpty() || notebook.books().isEmpty()) notebook.createBook("UI regression");
        engine.setInitialProperties({
            {"notebook", QVariant::fromValue(&notebook)}, {"appleShare", QVariant::fromValue(&share)},
            {"smtpMail", QVariant::fromValue(&smtp)},
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
bool remoteTap(WorkspaceHarness &h, FakeScreenStream &stream, QQuickItem *item)
{
    if (!item || !item->isEnabled() || item->width() <= 0 || item->height() <= 0) return false;
    const QPointF center = item->mapToScene({item->width() / 2, item->height() / 2});
    if (!QRectF(QPointF(), h.window()->size()).contains(center)) return false;
    for (auto *parent = item->parentItem(); parent; parent = parent->parentItem())
        if (parent->clip() && !parent->contains(parent->mapFromScene(center))) return false;
    emit stream.remoteTapRequested(center.x() / h.window()->width(), center.y() / h.window()->height());
    return true;
}
bool remoteTap(WorkspaceHarness &h, FakeScreenStream &stream, const QString &name)
{
    return remoteTap(h, stream, findVisualItem(h.window()->contentItem(), name));
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

    void entryPointContextFollowsTheCurrentSheetAndNeverLeaksToAnotherBook()
    {
        WorkspaceHarness h;
        QVERIFY(h.window());
        const auto chart = h.addChart(false);
        QVERIFY(!chart.isEmpty());
        const QString first = h.notebook.bookId();
        QVERIFY(openEntry(h, chart.value("id").toString()));
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("charts"));
        QVERIFY(h.notebook.addPage());
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
        QVERIFY(h.notebook.createBook("Writing after chart"));
        const QString second = h.notebook.bookId();
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
        QVERIFY(h.notebook.openBook(first));
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
        QVERIFY(openEntry(h, chart.value("id").toString()));
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("charts"));
        QVERIFY(h.notebook.goToPage(1));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showBook", Q_ARG(QString, second)));
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showBook", Q_ARG(QString, first)));
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
    }

    void viewerFitsTabletFrameAndKeepsRotatedClicksAligned()
    {
        FakeScreenStream stream;
        stream.workArea = QRect(44, 56, 600, 520);
        WorkspaceHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.window()->setProperty("screenStream", QVariant::fromValue(static_cast<QObject *>(&stream))));
        stream.reportSection(QStringLiteral("books"));

        const auto fixture = [&](const QString &name, QColor color) {
            QImage image(300, 400, QImage::Format_RGB32);
            image.fill(color);
            const QString file = h.directory.filePath(name);
            return image.save(file) ? QUrl::fromLocalFile(file) : QUrl{};
        };
        const QUrl first = fixture(QStringLiteral("portrait.png"), Qt::white);
        const QUrl next = fixture(QStringLiteral("next.png"), Qt::lightGray);
        QVERIFY(!first.isEmpty() && !next.isEmpty());
        stream.showFrame(first, 0);

        auto *viewer = h.find<QQuickWindow>("screenStreamWindow");
        QVERIFY(viewer && QMetaObject::invokeMethod(viewer, "open"));
        QVERIFY(waitForLayoutFrame(viewer));
        auto *preview = findVisualItem(viewer->contentItem(), QStringLiteral("screenStreamViewport"));
        auto *frame = findVisualItem(viewer->contentItem(), QStringLiteral("screenStreamImage"));
        auto *tap = findVisualItem(viewer->contentItem(), QStringLiteral("screenStreamTapArea"));
        QVERIFY(preview && frame && tap);
        QTRY_COMPARE(viewer->size(), QSize(354, 520));
        QTRY_COMPARE(preview->size(), QSizeF(354, 472));
        QCOMPARE(frame->property("paintedWidth").toDouble(), 354.0);
        QVERIFY(stream.workArea.contains(viewer->geometry()));

        viewer->resize(320, 490);
        stream.showFrame(next, 0);
        QTRY_COMPARE(frame->property("source").toUrl(), next);
        QVERIFY(waitForLayoutFrame(viewer));
        QCOMPARE(viewer->size(), QSize(320, 490));

        stream.showFrame(next, 90);
        QTRY_COMPARE(viewer->size(), QSize(600, 498));
        QTRY_COMPARE(preview->size(), QSizeF(600, 450));
        QCOMPARE(frame->rotation(), -90.0);
        QCOMPARE(frame->property("paintedWidth").toDouble(), 450.0);
        QVERIFY(stream.workArea.contains(viewer->geometry()));
        QVERIFY(waitForLayoutFrame(viewer));
        QSignalSpy sent(&stream, &FakeScreenStream::viewerTapped);
        const QPointF rawPoint(tap->width() * 0.2, tap->height() * 0.7);
        QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, tap->mapToScene(rawPoint).toPoint());
        QTRY_COMPARE(sent.count(), 1);
        QVERIFY(qAbs(sent.first().at(0).toDouble() - 0.2) < 0.01);
        QVERIFY(qAbs(sent.first().at(1).toDouble() - 0.7) < 0.01);
        viewer->close();
    }

    void screenSharingUsesContentWaveformAndRestoresRefreshProfile()
    {
        FakeScreenStream stream;
        WorkspaceHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences && preferences->setProperty("refreshProfile", QStringLiteral("fast")));
        QTRY_VERIFY(h.window()->property("fastPanelRefresh").toBool());
        QVERIFY(h.window()->setProperty("screenStream", QVariant::fromValue(static_cast<QObject *>(&stream))));
        QTRY_VERIFY(h.window()->property("screenSharing").toBool());
        QVERIFY(!h.window()->property("fastPanelRefresh").toBool());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showSettings"));
        QVERIFY(!h.window()->property("fastPanelRefresh").toBool());
        QCOMPARE(preferences->property("refreshProfile").toString(), QStringLiteral("fast"));
        stream.stop();
        QTRY_VERIFY(h.window()->property("fastPanelRefresh").toBool());
    }

    void remoteControlCanStopSharingFromVisibleButton_data()
    {
        QTest::addColumn<bool>("editorOpen");
        QTest::newRow("toolbar") << false;
        QTest::newRow("editor-overlay") << true;
    }

    void remoteControlCanStopSharingFromVisibleButton()
    {
        QFETCH(bool, editorOpen);
        FakeScreenStream stream;
        WorkspaceHarness h;
        QVERIFY(h.window());
        QVERIFY(h.window()->setProperty("screenStream", QVariant::fromValue(static_cast<QObject *>(&stream))));
        if (editorOpen) {
            auto *editor = h.find<QObject>("chartEditor");
            QVERIFY(editor && QMetaObject::invokeMethod(editor, "openDraft", Q_ARG(QString, QStringLiteral("QA"))));
            QTRY_COMPARE(stream.remoteSection(), QStringLiteral("chart-editor"));
        } else {
            QTRY_VERIFY(!stream.remoteSection().isEmpty());
        }
        QVERIFY(waitForLayoutFrame(h.window()));
        const QString toolbar = QStringLiteral("screenStreamStopButton");
        const QString overlay = QStringLiteral("screenStreamFullscreenStopButton");
        auto *hidden = findVisualItem(h.window()->contentItem(), editorOpen ? toolbar : overlay);
        QVERIFY(!hidden || !hidden->isVisible());
        QVERIFY(remoteTap(h, stream, editorOpen ? overlay : toolbar));
        QTRY_VERIFY(!stream.active());
        QVERIFY(h.window()->property("sharedControlSurface").toString().isEmpty());
    }

    void remoteControlNavigatesSettingsButKeepsSmtpLocal()
    {
        FakeScreenStream stream;
        WorkspaceHarness h;
        QVERIFY(h.window());
        QVERIFY(h.window()->setProperty("screenStream", QVariant::fromValue(static_cast<QObject *>(&stream))));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showSettings"));
        auto *settingsView = h.find<QObject>("settingsView");
        auto *smtpDialog = h.find<QObject>("smtpDialog");
        QVERIFY(settingsView);
        QVERIFY(smtpDialog);
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("settings"));
        QVERIFY(waitForLayoutFrame(h.window()));
        QCOMPARE(settingsView->property("category").toInt(), 9);
        QVERIFY(remoteTap(h, stream, QStringLiteral("settingsCategory_6")));
        QTRY_COMPARE(settingsView->property("category").toInt(), 6);
        QVERIFY(waitForLayoutFrame(h.window()));

        QVERIFY(remoteTap(h, stream, QStringLiteral("openSmtpSettings")));
        QVERIFY(!smtpDialog->property("visible").toBool());
        QCOMPARE(h.window()->property("sharedControlSurface").toString(), QStringLiteral("settings"));

        auto *localSmtp = findVisualItem(h.window()->contentItem(), QStringLiteral("openSmtpSettings"));
        QVERIFY(clickItem(h, localSmtp));
        QTRY_VERIFY(smtpDialog->property("visible").toBool());
        QTRY_VERIFY(h.window()->property("sharedControlSurface").toString().isEmpty());
    }

    void remoteControlCreatesATableWithProjectedTyping()
    {
        FakeScreenStream stream;
        WorkspaceHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem(tableFixture(QStringLiteral("Existing table"))));
        const QString existingId = h.notebook.pageItems().first().toMap().value("id").toString();
        QVERIFY(openEntry(h, existingId));
        QTRY_COMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("tables"));
        QVERIFY(h.window()->setProperty("screenStream", QVariant::fromValue(static_cast<QObject *>(&stream))));
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("editor"));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("workspaceAction_newTable")));

        auto *dialog = h.find<QObject>("pageItemDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("table-editor"));
        QVERIFY(waitForLayoutFrame(h.window()));
        auto *cell = findVisualItem(h.window()->contentItem(), QStringLiteral("tableCellInput"));
        QVERIFY(cell);
        const QString original = cell->property("text").toString();
        emit stream.remoteKeyRequested(QStringLiteral("text"), QStringLiteral("No remote focus"));
        QCOMPARE(cell->property("text").toString(), original);

        QVERIFY(remoteTap(h, stream, QStringLiteral("tableCellInput")));
        emit stream.remoteKeyRequested(QStringLiteral("select-all"), {});
        const QString label = QStringLiteral("Tabella da Mac");
        emit stream.remoteKeyRequested(QStringLiteral("text"), label);
        QTRY_COMPARE(cell->property("text").toString(), label);

        stream.allowRemoteNavigation(false);
        QTRY_VERIFY(h.window()->property("sharedControlSurface").toString().isEmpty());
        emit stream.remoteKeyRequested(QStringLiteral("backspace"), {});
        QCOMPARE(cell->property("text").toString(), label);
        QVERIFY(remoteTap(h, stream, QStringLiteral("savePageItemButton")));
        QVERIFY(dialog->property("visible").toBool());

        stream.allowRemoteNavigation(true);
        QTRY_COMPARE(h.window()->property("sharedControlSurface").toString(), QStringLiteral("table-editor"));
        QVERIFY(remoteTap(h, stream, QStringLiteral("savePageItemButton")));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        const auto items = h.notebook.pageItems();
        QCOMPARE(items.size(), 2);
        const auto table = items.last().toMap();
        QCOMPARE(table.value("type").toString(), QStringLiteral("table"));
        QCOMPARE(table.value("cells").toList().first().toString(), label);

        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("workspaceAction_tableEdit")));
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("create"));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("workspaceItemChoice_") + existingId));
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("table-editor"));
        QCOMPARE(qmlMap(dialog, "originalItem").value("id").toString(), existingId);
        QVERIFY(waitForLayoutFrame(h.window()));
        auto *grid = findVisualItem(h.window()->contentItem(), QStringLiteral("tableGrid"));
        QVERIFY(grid);
        const auto findA2 = [](const auto &self, QQuickItem *item) -> QQuickItem * {
            if (!item->isVisible()) return nullptr;
            if (item->property("remoteClickable").toBool()
                && item->property("row").toInt() == 1 && item->property("column").toInt() == 0)
                return item;
            for (auto *child : item->childItems())
                if (auto *cell = self(self, child)) return cell;
            return nullptr;
        };
        // Qt Repeaters parent delegates into the visual tree of the grid.
        QQuickItem *a2 = nullptr;
        QTRY_VERIFY((a2 = findA2(findA2, grid)));
        QVERIFY(remoteTap(h, stream, a2));
        QCOMPARE(dialog->property("selectedRow").toInt(), 1);
        QVERIFY(remoteTap(h, stream, QStringLiteral("tableCellInput")));
        emit stream.remoteKeyRequested(QStringLiteral("select-all"), {});
        emit stream.remoteKeyRequested(QStringLiteral("text"), QStringLiteral("12"));
        QVERIFY(remoteTap(h, stream, QStringLiteral("savePageItemButton")));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        const auto edited = h.notebook.pageItems().first().toMap();
        QCOMPARE(edited.value("id").toString(), existingId);
        const auto cells = edited.value("cells").toList();
        QCOMPARE(cells.value(2).toString(), QStringLiteral("12"));
        QCOMPARE(cells.value(3).toString(), QStringLiteral("=SUM(A2:A2)"));
        QCOMPARE(h.notebook.tableCellValues(edited).value(3).toString(), QStringLiteral("12"));
    }

    void remoteControlCreatesMarkdownWithProjectedTyping()
    {
        FakeScreenStream stream;
        WorkspaceHarness h;
        QVERIFY(h.window());
        QVERIFY(h.window()->setProperty("screenStream", QVariant::fromValue(static_cast<QObject *>(&stream))));
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("editor"));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("workspaceAction_markdown")));
        auto *editor = h.find<QObject>("markdownEditor");
        QVERIFY(editor);
        QTRY_VERIFY(editor->property("opened").toBool());
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("writing-editor"));

        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("markdownName")));
        const QString title = QStringLiteral("Appunti dal Mac");
        emit stream.remoteKeyRequested(QStringLiteral("text"), title);
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("markdownSource")));
        emit stream.remoteKeyRequested(QStringLiteral("text"), QStringLiteral("# Prova"));
        emit stream.remoteKeyRequested(QStringLiteral("new-line"), {});
        emit stream.remoteKeyRequested(QStringLiteral("text"), QStringLiteral("Testo da Quaderno."));
        QVERIFY(waitForLayoutFrame(h.window()));
        auto *surface = editor->property("remoteSurface").value<QQuickItem *>();
        QVERIFY(remoteTap(h, stream, findRemoteAction(surface, {QStringLiteral("Salva"), QStringLiteral("Save")})));
        QTRY_VERIFY(!editor->property("visible").toBool());
        QCOMPARE(h.notebook.title(), title);
        QCOMPARE(h.notebook.markdownSource().trimmed(), QStringLiteral("# Prova\nTesto da Quaderno."));
    }

    void remoteControlCreatesAndEditsAChart()
    {
        FakeScreenStream stream;
        WorkspaceHarness h;
        QVERIFY(h.window());
        QVERIFY(h.window()->setProperty("screenStream", QVariant::fromValue(static_cast<QObject *>(&stream))));
        const auto surface = [&] { return h.window()->property("remoteControlTarget").value<QQuickItem *>(); };
        const auto typeIn = [&](const QStringList &fieldLabels, const QString &text) {
            auto *field = findRemoteControl(surface(), fieldLabels, "remoteEditable", "placeholderText");
            if (!remoteTap(h, stream, field)) return false;
            emit stream.remoteKeyRequested(QStringLiteral("select-all"), {});
            emit stream.remoteKeyRequested(QStringLiteral("text"), text);
            return field->property("text").toString() == text;
        };
        const QStringList saveLabels{QStringLiteral("Salva"), QStringLiteral("Save")};
        const QStringList titleLabels{QStringLiteral("Titolo del grafico"), QStringLiteral("Chart title")};

        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("editor"));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("editorBackButton")));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("librarySection_charts")));
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("charts"));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, findRemoteAction(surface(),
            {QStringLiteral("Nuovo grafico"), QStringLiteral("New chart")})));
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("create"));
        QVERIFY(waitForLayoutFrame(h.window()));
        const QString notebookName = QStringLiteral("Grafici da Mac");
        QVERIFY(typeIn({QStringLiteral("Nome del quaderno"), QStringLiteral("Notebook name")}, notebookName));
        QVERIFY(remoteTap(h, stream, findRemoteAction(surface(), {QStringLiteral("Crea"), QStringLiteral("Create")})));

        auto *editor = h.find<QObject>("chartEditor");
        QVERIFY(editor);
        QTRY_VERIFY(editor->property("opened").toBool());
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("chart-editor"));
        QVERIFY(waitForLayoutFrame(h.window()));
        const QString firstTitle = QStringLiteral("Vendite di prova");
        QVERIFY(typeIn(titleLabels, firstTitle));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, findRemoteAction(surface(), saveLabels)));
        QTRY_VERIFY(!editor->property("visible").toBool());
        QCOMPARE(h.notebook.title(), notebookName);
        QCOMPARE(h.notebook.pageItems().size(), 1);
        const QString bookId = h.notebook.bookId();
        const auto chart = h.notebook.pageItems().first().toMap();
        QCOMPARE(chart.value("type").toString(), QStringLiteral("chart"));
        QCOMPARE(chart.value("title").toString(), firstTitle);
        QCOMPARE(chart.value("points").toList().size(), 2);
        const QString chartId = chart.value("id").toString();
        QVERIFY(!chartId.isEmpty());

        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("editorBackButton")));
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("charts"));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("libraryRow_") + chartId));
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("editor"));
        QTRY_COMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("charts"));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("workspaceAction_chartEdit")));
        QTRY_VERIFY(editor->property("opened").toBool());
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("chart-editor"));
        QVERIFY(waitForLayoutFrame(h.window()));
        const QString revisedTitle = QStringLiteral("Vendite aggiornate");
        QVERIFY(typeIn(titleLabels, revisedTitle));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, findRemoteAction(surface(), saveLabels)));
        QTRY_VERIFY(!editor->property("visible").toBool());
        QCOMPARE(h.notebook.bookId(), bookId);
        QCOMPARE(h.notebook.pageItems().size(), 1);
        const auto revised = h.notebook.pageItems().first().toMap();
        QCOMPARE(revised.value("id").toString(), chartId);
        QCOMPARE(revised.value("title").toString(), revisedTitle);
        QCOMPARE(revised.value("points"), chart.value("points"));
    }

    void remoteControlPlacesAndSavesABookAnnotation()
    {
        FakeScreenStream stream;
        WorkspaceHarness h;
        QVERIFY(h.window());
        QVERIFY(h.addBookBackground());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QVERIFY(h.window()->setProperty("screenStream", QVariant::fromValue(static_cast<QObject *>(&stream))));
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("book"));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, findRemoteAction(h.window()->contentItem(),
            {QStringLiteral("Pagina successiva"), QStringLiteral("Next page")})));
        QTRY_COMPARE(h.notebook.page(), 1);
        const QString pageId = h.notebook.pageId();
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("workspaceAction_bookNote")));
        QTRY_COMPARE(h.window()->property("bookAnchorKind").toString(), QStringLiteral("note"));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("inkPage")));

        auto *editor = h.find<QObject>("bookNoteDialog");
        QVERIFY(editor);
        QTRY_VERIFY(editor->property("opened").toBool());
        QTRY_COMPARE(stream.remoteSection(), QStringLiteral("book-note"));
        QVERIFY(waitForLayoutFrame(h.window()));
        auto *surface = editor->property("remoteSurface").value<QQuickItem *>();
        QVERIFY(remoteTap(h, stream, findRemoteAction(surface, {QStringLiteral("Testo"), QStringLiteral("Text")})));
        QTRY_COMPARE(editor->property("mode").toString(), QStringLiteral("text"));
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(remoteTap(h, stream, QStringLiteral("bookNoteText")));
        const QString note = QStringLiteral("Rileggere questa pagina");
        emit stream.remoteKeyRequested(QStringLiteral("text"), note);
        QTRY_COMPARE(editor->property("noteText").toString(), note);
        QVERIFY(remoteTap(h, stream, QStringLiteral("bookNoteSave")));
        QTRY_VERIFY(!editor->property("visible").toBool());
        QCOMPARE(h.notebook.pageId(), pageId);
        const auto annotations = h.notebook.pageItems();
        QCOMPARE(annotations.size(), 1);
        QCOMPARE(annotations.first().toMap().value("type").toString(), QStringLiteral("note"));
        QCOMPARE(annotations.first().toMap().value("text").toString(), note);
        QVERIFY(h.notebook.goToPage(0));
        QVERIFY(h.notebook.pageItems().isEmpty());
    }

    void writingAddsATableAsAnEditableSheet()
    {
        WorkspaceHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.importMarkdownText(QStringLiteral("# Appunti\n\nPagina di scrittura."),
            QStringLiteral("Appunti"), {}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        const auto book = h.notebook.bookId();
        const auto writing = h.notebook.pageId();
        QVERIFY(QMetaObject::invokeMethod(h.window(), "workspaceAction", Q_ARG(QString, QStringLiteral("sheetTable")),
            Q_ARG(QString, QString{})));
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(editor);
        QTRY_VERIFY(editor->property("opened").toBool());
        QQuickItem *cell = nullptr;
        QTRY_VERIFY((cell = findVisualItem(h.window()->contentItem(), QStringLiteral("tableCellInput"))));
        QVERIFY(cell->setProperty("text", QStringLiteral("Nuovo foglio")));
        QVERIFY(QMetaObject::invokeMethod(editor, "submit", Q_ARG(bool, false)));
        QTRY_VERIFY(!editor->property("visible").toBool());
        QCOMPARE(h.notebook.bookId(), book);
        QCOMPARE(h.notebook.pageCount(), 2);
        QTRY_COMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("tables"));
        const auto table = h.notebook.pageItems().first().toMap();
        const auto sheet = h.notebook.pageId();
        QCOMPARE(table.value("cells").toList().first().toString(), QStringLiteral("Nuovo foglio"));
        QVERIFY(h.notebook.openPage(writing));
        QVERIFY(openEntry(h, table.value("id").toString()));
        QCOMPARE(h.notebook.pageId(), sheet);
        QCOMPARE(h.notebook.pageItems().first().toMap().value("id"), table.value("id"));
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

    void nativeMixedPageCaptionsUseTheVisibleFootprints()
    {
        const auto node = [](QString id, QString shape, QRectF pixels, QString text = {}) {
            return QVariantMap{{"id", id}, {"shape", shape}, {"text", text}, {"x", pixels.x() / 900},
                {"y", pixels.y() / 1200}, {"width", pixels.width() / 900}, {"height", pixels.height() / 1200}};
        };
        const QVariantList nodes{
            node("outer", "process", {70, 100, 650, 240}),
            node("inside", "process", {116, 170, 150, 108}),
            node("known", "terminal", {525, 170, 160, 108}, "Testo già salvato"),
            node("corner", "decision", {110, 410, 150, 105}),
            node("left", "process", {317, 410, 150, 105}),
            node("right", "decision", {548, 410, 165, 105}),
        };
        const QVariantMap diagram{{"type", "diagram"}, {"x", 0}, {"y", 0}, {"width", 1}, {"height", 1},
            {"nodes", nodes}, {"edges", QVariantList{}}};

        QVariantList strokes;
        const auto addStroke = [&](std::initializer_list<QPointF> pixels) {
            QVariantList points;
            for (const auto &point : pixels)
                points.append(QVariantMap{{"x", point.x() / 900}, {"y", point.y() / 1200}, {"p", .65}});
            strokes.append(QVariantMap{{"tool", "pen"}, {"color", "#222222"}, {"width", 4}, {"points", points}});
        };
        const auto addCaption = [&](double x, double y) {
            // Draw the right-hand character first to verify that geometric
            // grouping retains the original writing order for the recognizer.
            addStroke({{x + 16, y}, {x + 16, y + 16}, {x + 26, y + 16}});
            addStroke({{x, y + 16}, {x + 6, y}, {x + 12, y + 16}});
            addStroke({{x + 3, y + 9}, {x + 9, y + 9}});
        };
        addCaption(360, 123);
        const auto insideStart = strokes.size();
        addCaption(160, 205);
        const auto insideStrokes = strokes.mid(insideStart);
        addCaption(575, 205);
        addCaption(113, 415);
        addCaption(376, 523);
        addCaption(616, 523);

        MixedPageAnalyzer analyzer;
        QSignalSpy completed(&analyzer, &MixedPageAnalyzer::finished);
        const QVariantList selection{QRectF(.06, .07, .77, .55)};
        analyzer.analyze({{"items", QVariantList{diagram}}, {"strokes", strokes}},
            {{"diagram", selection}, {"table", selection}, {"text", selection}});
        QTRY_COMPARE(completed.size(), 1);
        const auto result = completed.first();
        QCOMPARE(result.at(1).toString(), QString{});
        const auto draft = result.at(0).toMap();
        QCOMPARE(draft.value("diagram").toMap().value("nodes").toList(), nodes);
        const auto jobs = draft.value("diagramInk").toList();
        QStringList targets;
        for (const auto &entry : jobs) {
            const auto job = entry.toMap();
            const auto id = job.value("nodeId").toString();
            targets.append(id);
            if (id == QStringLiteral("inside"))
                QCOMPARE(job.value("strokes").toList(), insideStrokes);
        }
        QCOMPARE(targets, (QStringList{QStringLiteral("outer"), QStringLiteral("inside"),
            QStringLiteral("left"), QStringLiteral("right")}));

        auto rotated = diagram;
        rotated.insert("rotation", 90);
        rotated.insert("nodes", QVariantList{node("turned", "decision", {100, 240, 150, 120})});
        strokes.clear();
        addCaption(137, 278); // This lies in the old, unrotated footprint.
        const auto rotatedStart = strokes.size();
        addCaption(732, 317);
        const auto rotatedStrokes = strokes.mid(rotatedStart);
        completed.clear();
        const QVariantList rotatedSelection{QRectF(.04, .07, .91, .73)};
        analyzer.analyze({{"items", QVariantList{rotated}}, {"strokes", strokes}},
            {{"diagram", rotatedSelection}, {"table", rotatedSelection}, {"text", rotatedSelection}});
        QTRY_COMPARE(completed.size(), 1);
        QCOMPARE(completed.first().at(1).toString(), QString{});
        const auto rotatedJobs = completed.first().at(0).toMap().value("diagramInk").toList();
        QCOMPARE(rotatedJobs.size(), 1);
        QCOMPARE(rotatedJobs.first().toMap().value("nodeId").toString(), QStringLiteral("turned"));
        QCOMPARE(rotatedJobs.first().toMap().value("strokes").toList(), rotatedStrokes);
    }

    void rasterDiagramKeepsSmallAndVerticalLabelsWithTheirNodes()
    {
        const QByteArray png = smallRasterDiagram();
        QVERIFY(!png.isEmpty());
        MixedPageAnalyzer analyzer;
        QSignalSpy completed(&analyzer, &MixedPageAnalyzer::finished);
        analyzer.analyze({{"background", png}}, {
            {"diagram", QVariantList{QRectF(.08, .09, .86, .33)}},
            {"table", QVariantList{QRectF(.10, .50, .25, .12)}},
            {"text", QVariantList{QRectF(.50, .50, .25, .12)}}});
        QTRY_COMPARE(completed.size(), 1);
        const auto result = completed.first();
        QVERIFY2(result.at(1).toString().isEmpty(), qPrintable(result.at(1).toString()));
        const auto draft = result.at(0).toMap();
        const auto diagram = draft.value("diagram").toMap();
        const auto nodes = diagram.value("nodes").toList();
        const auto jobs = draft.value("diagramInk").toList();
        QCOMPARE(nodes.size(), 6);
        QCOMPARE(jobs.size(), 5);

        const auto bounds = [](const QVariantMap &value) {
            return QRectF(value.value("x").toDouble(), value.value("y").toDouble(),
                value.value("width").toDouble(), value.value("height").toDouble());
        };
        const QRectF page = bounds(diagram);
        for (const auto &caption : {QPointF(200, 181), QPointF(368, 251), QPointF(582, 247), QPointF(709, 253)}) {
            const QPointF position(caption.x() / 900, caption.y() / 1200);
            const auto node = std::find_if(nodes.cbegin(), nodes.cend(), [&](const QVariant &entry) {
                const QRectF box = bounds(entry.toMap());
                return QRectF(page.x() + box.x() * page.width(), page.y() + box.y() * page.height(),
                    box.width() * page.width(), box.height() * page.height()).contains(position);
            });
            QVERIFY(node != nodes.cend());
            const auto id = node->toMap().value("id");
            const auto job = std::find_if(jobs.cbegin(), jobs.cend(), [&](const QVariant &entry) {
                return entry.toMap().value("nodeId") == id;
            });
            QVERIFY(job != jobs.cend());
            const QRectF crop = job->toMap().value("bounds").toRectF();
            QVERIFY(crop.adjusted(-.005, -.005, .005, .005).contains(position));
            const bool vertical = caption.x() != 200;
            QCOMPARE(job->toMap().value("vertical").toBool(), vertical);
            const qreal ratio = crop.height() * 1200 / (crop.width() * 900);
            QVERIFY(vertical ? ratio > 2 : ratio < 0.6);
            if (caption.x() == 709)
                QCOMPARE(job->toMap().value("glyphBounds").toList().size(), 4);
        }

        const auto smallNode = std::find_if(nodes.cbegin(), nodes.cend(), [&](const auto &value) {
            const QRectF box = bounds(value.toMap());
            return QRectF(page.x() + box.x() * page.width(), page.y() + box.y() * page.height(),
                box.width() * page.width(), box.height() * page.height()).contains(QPointF(160. / 900, 299. / 1200));
        });
        QVERIFY(smallNode != nodes.cend());
        const auto label = std::find_if(jobs.cbegin(), jobs.cend(), [&](const QVariant &entry) {
            return entry.toMap().value("nodeId") == smallNode->toMap().value("id");
        });
        QVERIFY(label != jobs.cend());
        const auto crop = label->toMap().value("tinyCrop").toRectF();
        QVERIFY(crop.contains(QPointF(161. / 900, 328. / 1200)));
        QVERIFY(crop.top() > 320. / 1200);
    }

    void mixedPageNativeGridPreservesEmptyCellsAndStrokeOrder()
    {
        WorkspaceHarness h;
        QVERIFY(h.window());
        const QVariantMap node{{"id", "step"}, {"shape", "process"}, {"text", "QA"},
            {"x", .15}, {"y", .20}, {"width", .35}, {"height", .30}};
        QVERIFY(h.notebook.savePageItem({{"type", "diagram"}, {"x", .05}, {"y", .03},
            {"width", .24}, {"height", .12}, {"nodes", QVariantList{node}}, {"edges", QVariantList{}}}));
        QVERIFY(h.notebook.savePageItem({{"type", "text"}, {"x", .73}, {"y", .73},
            {"width", .20}, {"height", .12}, {"text", "Testo per il controllo della griglia."}}));

        const auto stroke = [](std::initializer_list<QPointF> samples) {
            QVariantList points;
            for (const auto &point : samples)
                points.append(QVariantMap{{"x", point.x()}, {"y", point.y()}, {"p", .64}});
            return QVariantMap{{"color", "#222222"}, {"width", 2.8}, {"points", points}};
        };
        QVariantList rules;
        for (double y : {.20, .32, .44})
            rules.append(stroke({{.205, y - .001}, {.38, y + .002}, {.57, y - .001}, {.755, y + .001}}));
        for (double x : {.21, .39, .57, .75})
            rules.append(stroke({{x - .001, .195}, {x + .001, .26}, {x - .002, .32}, {x + .001, .38}, {x, .447}}));
        const auto a1First = stroke({{.245, .257}, {.254, .245}, {.254, .28}});
        const auto a1Second = stroke({{.275, .249}, {.289, .244}, {.298, .252}, {.285, .267}, {.274, .28}, {.299, .28}});
        const auto c1 = stroke({{.60, .249}, {.634, .249}, {.613, .282}});
        const auto a2 = stroke({{.252, .354}, {.268, .377}, {.252, .392}});
        const auto b2 = stroke({{.455, .350}, {.482, .353}, {.464, .371}, {.485, .380}, {.462, .394}});
        const auto acrossDivider = stroke({{.36, .284}, {.38, .29}, {.419, .295}});
        const QVariantList handwriting{a1First, c1, a1Second, acrossDivider, a2, b2};
        for (const auto &entry : rules + handwriting)
            QVERIFY(h.notebook.appendStroke(entry.toMap()));
        const auto source = h.notebook.mixedPageSource();
        const auto sourceExport = h.notebook.exportBookForTransfer(source.value("bookId").toString());
        const auto outlineOf = [](const QVariantMap &item) {
            const QRectF bounds(item.value("x").toReal(), item.value("y").toReal(),
                item.value("width").toReal(), item.value("height").toReal());
            return bounds.adjusted(-.02, -.02, .02, .02).intersected(QRectF(0, 0, 1, 1));
        };
        const QVariantMap regions{{"diagram", QVariantList{outlineOf(h.notebook.pageItems().at(0).toMap())}},
            {"table", QVariantList{QRectF(.17, .17, .62, .30)}},
            {"text", QVariantList{outlineOf(h.notebook.pageItems().at(1).toMap())}}};
        MixedPageAnalyzer analyzer;
        QSignalSpy completed(&analyzer, &MixedPageAnalyzer::finished);
        analyzer.analyze(source.value("content").toMap(), regions);
        QTRY_COMPARE(completed.size(), 1);
        QVERIFY2(completed.first().at(1).toString().isEmpty(), qPrintable(completed.first().at(1).toString()));
        const auto draft = completed.first().at(0).toMap();
        QCOMPARE(draft.value("diagram").toMap().value("nodes").toList().size(), 1);
        QCOMPARE(draft.value("text").toString(), QStringLiteral("Testo per il controllo della griglia."));
        QCOMPARE(draft.value("tableText").toString(), QStringLiteral("\t\t\n\t\t"));
        QVERIFY(draft.value("tableInk").toList().isEmpty());
        const auto jobs = draft.value("tableCellInk").toList();
        const QVariantList expected{
            QVariantMap{{"row", 0}, {"column", 0}, {"strokes", QVariantList{a1First, a1Second}}},
            QVariantMap{{"row", 0}, {"column", 2}, {"strokes", QVariantList{c1}}},
            QVariantMap{{"row", 1}, {"column", 0}, {"strokes", QVariantList{a2}}},
            QVariantMap{{"row", 1}, {"column", 1}, {"strokes", QVariantList{b2}}}};
        QCOMPARE(jobs, expected);

        auto *dialog = h.find<QObject>("mixedPageDialog");
        auto *tableField = h.find<QQuickItem>("mixedPageTable");
        QVERIFY(dialog && tableField);
        QVERIFY(QMetaObject::invokeMethod(dialog, "openForPage"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "beginReview", Q_ARG(QVariant, QVariant(draft))));
        QVERIFY(QMetaObject::invokeMethod(dialog, "stopRecognition"));
        QCOMPARE(tableField->property("text").toString(), QStringLiteral("\t\t\n\t\t"));

        // Supply deterministic recognizer outcomes to the real QML dialog.
        // A missed A2 and the unwritten B1/C2 must keep their positions.
        QVariantList queue;
        for (const auto &entry : jobs) {
            auto job = entry.toMap();
            job.insert("part", QStringLiteral("table-cell"));
            queue.append(job);
        }
        QVERIFY(dialog->setProperty("recognitionQueue", queue));
        QVERIFY(dialog->setProperty("queueIndex", 0));
        for (const QString &value : {QStringLiteral("12"), QStringLiteral("7"), QString(), QStringLiteral("3,4")})
            QVERIFY(QMetaObject::invokeMethod(dialog, "applyRecognition", Q_ARG(QString, value)));
        QVERIFY(QMetaObject::invokeMethod(dialog, "stopRecognition"));
        QCOMPARE(tableField->property("text").toString(), QStringLiteral("12\t\t7\n\t3,4\t"));
        QCOMPARE(dialog->property("missedLines").toInt(), 1);
        QVERIFY(dialog->property("canCreate").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "saveDraft"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.exportBookForTransfer(source.value("bookId").toString()), sourceExport);
        QCOMPARE(h.notebook.pageCount(), 3);
        QVERIFY(h.notebook.goToPage(2));
        const auto saved = h.notebook.pageItems().first().toMap();
        QCOMPARE(saved.value("rows").toInt(), 2);
        QCOMPARE(saved.value("cols").toInt(), 3);
        QCOMPARE(saved.value("cells").toList(), (QVariantList{QStringLiteral("12"), QStringLiteral(""), QStringLiteral("7"),
            QStringLiteral(""), QStringLiteral("3,4"), QStringLiteral("")}));

        // Remove the rules from the same source and check the handwritten
        // fallback still produces row jobs instead of guessing columns.
        auto unruled = source.value("content").toMap();
        unruled.remove("items");
        unruled.insert("strokes", handwriting);
        completed.clear();
        analyzer.analyze(unruled, regions);
        QTRY_COMPARE(completed.size(), 1);
        QVERIFY(completed.first().at(1).toString().isEmpty());
        const auto fallback = completed.first().at(0).toMap();
        QVERIFY(fallback.value("tableCellInk").toList().isEmpty());
        QVERIFY(fallback.value("tableText").toString().isEmpty());
        QCOMPARE(fallback.value("tableInk").toList().size(), 2);
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

        auto *strip = h.find<QQuickItem>("notebookPageStrip");
        QVERIFY(strip);
        QTRY_VERIFY(strip->isVisible());
        QVERIFY(!h.find<QQuickItem>("workspaceLinkedItemsButton")->isVisible());
        const auto openSheet = [&](int index) {
            const QString name = QStringLiteral("notebookPageTab_%1").arg(index);
            QQuickItem *button = nullptr;
            return QTest::qWaitFor([&] { return (button = findVisualItem(h.window()->contentItem(), name)); })
                && clickItem(h, button);
        };
        const auto diagramLink = links.value(QStringLiteral("diagram"));
        QVERIFY(openSheet(1));
        QTRY_COMPARE(h.notebook.pageId(), diagramLink.value("targetPageId").toString());
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("diagrams"));
        QVERIFY(clickAction(h, "diagramEdit"));
        auto *diagramEditor = h.find<QObject>("diagramEditor");
        QVERIFY(diagramEditor);
        QTRY_VERIFY(diagramEditor->property("opened").toBool());
        QCOMPARE(h.notebook.bookId(), writingBook);
        QCOMPARE(h.notebook.pageId(), diagramLink.value("targetPageId").toString());
        QCOMPARE(h.notebook.pageItems().first().toMap().value("id"), diagramLink.value("targetItemId"));
        auto *canvas = qobject_cast<DiagramItem *>(findVisualItem(h.window()->contentItem(), QStringLiteral("diagramCanvas")));
        QVERIFY(canvas);
        QVERIFY(waitForLayoutFrame(h.window()));
        const QRectF view = canvas->viewRect();
        const QPointF center(view.x() + .205 * view.width(), view.y() + .19 * view.height());
        QCOMPARE(canvas->nodeAt(center.x(), center.y()), QStringLiteral("start"));
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, canvas->mapToScene(center).toPoint());
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
        QVERIFY(openSheet(0));
        QTRY_COMPARE(h.notebook.pageId(), writingPage);
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
        const auto editedDiagram = h.notebook.linkedPageItem(diagramLink.value("id").toString());
        QCOMPARE(editedDiagram.value("id"), diagramLink.value("targetItemId"));
        QCOMPARE(editedDiagram.value("nodes").toList().first().toMap().value("text").toString(), newLabel);

        const auto tableLink = links.value(QStringLiteral("table"));
        QVERIFY(openEntry(h, tableLink.value("targetItemId").toString()));
        QTRY_COMPARE(h.notebook.pageId(), tableLink.value("targetPageId").toString());
        QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("tables"));
        QVERIFY(clickAction(h, "tableEdit"));
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
        QVERIFY(openSheet(0));
        QTRY_COMPARE(h.notebook.pageId(), writingPage);
        const auto editedTable = h.notebook.linkedPageItem(tableLink.value("id").toString());
        QCOMPARE(editedTable.value("id"), tableLink.value("targetItemId"));
        QCOMPARE(editedTable.value("cells").toList().first().toString(), newHeader);
        QCOMPARE(h.notebook.exportBookForTransfer(sourceBook), sourceData);
    }

    void inventoryReportKeepsLiveDataAndReferencesAfterReopening()
    {
        QTemporaryDir library;
        QVERIFY(library.isValid());
        const QString path = library.filePath("notebooks.json");
        const QString title = QStringLiteral("Inventario settimanale");
        const QString report = QStringLiteral("# Inventario settimanale\n\n"
            "Confrontare il saldo netto con gli scarti e seguire il diagramma per ricontare.");
        const QString chartTitle = QStringLiteral("Saldo giornaliero");
        const QString procedure = QStringLiteral("Ricontare gli scarti");
        const QVariantList cells{"Giorno", "Entrate", "Scarti", "Netto",
            "1", "12", "-4", "=SUM(B2:C2)", "2", "10", "-12", "=SUM(B3:C3)", "3", "", "", "=AVERAGE(D2:D3)"};
        const QVariantList nodes{
            QVariantMap{{"id", "count"}, {"shape", "terminal"}, {"text", "Contare gli scarti"},
                {"x", .14}, {"y", .16}, {"width", .3}, {"height", .13}},
            QVariantMap{{"id", "record"}, {"shape", "process"}, {"text", "Registrare il saldo"},
                {"x", .55}, {"y", .42}, {"width", .3}, {"height", .13}}
        };
        const QVariantList edges{QVariantMap{{"id", "flow"}, {"from", "count"}, {"to", "record"}, {"text", "Conferma"}}};
        const auto clickNamed = [](WorkspaceHarness &h, const QString &name) {
            return waitForLayoutFrame(h.window())
                && clickItem(h, findVisualItem(h.window()->contentItem(), name));
        };
        const auto tapPage = [](WorkspaceHarness &h, QPointF point) {
            auto *ink = h.find<QQuickItem>("inkPage");
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
                ink->mapToScene({point.x() * ink->width(), point.y() * ink->height()}).toPoint());
        };
        const auto tapLink = [&](WorkspaceHarness &h, const QVariantMap &link) {
            tapPage(h, {link.value("x").toDouble() + link.value("width").toDouble() / 2,
                link.value("y").toDouble() + link.value("height").toDouble() / 2});
        };
        const auto buttonWithText = [](const auto &self, QQuickItem *item, const QString &text) -> QQuickItem * {
            if (!item || !item->isVisible()) return nullptr;
            if (item->metaObject()->indexOfSignal("clicked()") >= 0 && item->property("text").toString() == text)
                return item;
            for (auto *child : item->childItems())
                if (auto *button = self(self, child, text)) return button;
            return nullptr;
        };
        const auto replaceText = [](WorkspaceHarness &h, QQuickItem *input, const QString &text) {
            if (!input || !input->isVisible()) return false;
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
                input->mapToScene(input->boundingRect().center()).toPoint());
            QTest::keyClick(h.window(), Qt::Key_A, Qt::ControlModifier);
            for (const char key : text.toLatin1()) QTest::keyClick(h.window(), key);
            return input->property("text").toString() == text;
        };
        QString bookId, writingPage, tableId, chartId;
        QVariantMap tableLink, chartLink, diagramLink;
        QVariantList updatedSeries;
        {
            WorkspaceHarness h(false, path);
            QVERIFY(h.window() && h.find<QQuickItem>("inkPage"));
            QVERIFY(h.addStroke());
            const auto source = h.notebook.mixedPageSource();
            const QByteArray original = h.notebook.exportBookForTransfer(h.notebook.bookId());
            QVERIFY2(h.notebook.createMixedDocument(h.notebook.bookId(), h.notebook.pageId(),
                source.value("revision").toString(), title, report, {{"nodes", nodes}, {"edges", edges}},
                {{"rows", 4}, {"cols", 4}, {"cells", cells}}), qPrintable(h.notebook.error()));
            bookId = h.notebook.bookId();
            writingPage = h.notebook.pageId();
            QTRY_COMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
            QCOMPARE(h.notebook.pageCount(), 3);
            for (const auto &entry : h.notebook.pageItems()) {
                if (entry.toMap().value("targetType").toString() == "table") tableLink = entry.toMap();
                if (entry.toMap().value("targetType").toString() == "diagram") diagramLink = entry.toMap();
            }
            QVERIFY(!tableLink.isEmpty() && !diagramLink.isEmpty());

            QVERIFY(clickNamed(h, QStringLiteral("notebookPageTab_2")));
            QTRY_COMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("tables"));
            const auto table = h.notebook.pageItems().first().toMap();
            tableId = table.value("id").toString();
            QCOMPARE(tableId, tableLink.value("targetItemId").toString());
            QVERIFY(clickAction(h, "chartFromTable"));
            auto *chartEditor = h.find<QObject>("tableChartDialog");
            QVERIFY(chartEditor);
            QTRY_VERIFY(chartEditor->property("opened").toBool());
            QVERIFY(!chartEditor->property("sourceIsSnapshot").toBool());
            QVERIFY(waitForLayoutFrame(h.window()));
            auto *axisY = findVisualItem(h.window()->contentItem(), QStringLiteral("chartSourceAxisY"));
            QVERIFY(axisY);
            axisY->forceActiveFocus();
            QTest::keyClick(h.window(), Qt::Key_Down);
            QTest::keyClick(h.window(), Qt::Key_Down);
            QTRY_COMPARE(axisY->property("currentIndex").toInt(), 3);
            const auto fields = chartEditor->findChildren<QQuickItem *>();
            const auto chartTitleField = std::find_if(fields.cbegin(), fields.cend(), [](const QQuickItem *item) {
                return item->isVisible()
                    && item->property("placeholderText").toString() == QStringLiteral("Titolo del grafico");
            });
            QVERIFY(chartTitleField != fields.cend());
            QVERIFY(replaceText(h, *chartTitleField, chartTitle));
            const auto preview = qmlMap(chartEditor, "preview");
            QCOMPARE(preview.value("yLabel").toString(), QStringLiteral("Netto"));
            QCOMPARE(preview.value("points").toList().size(), 3);
            QVERIFY(clickNamed(h, QStringLiteral("saveTableChart")));
            QTRY_VERIFY(!chartEditor->property("visible").toBool());
            const auto initialChart = h.notebook.pageItems().last().toMap();
            chartId = initialChart.value("id").toString();
            QVERIFY(!chartId.isEmpty());
            QCOMPARE(initialChart.value("sourceTable").toString(), tableId);
            QCOMPARE(initialChart.value("yColumn").toInt(), 4);

            QVERIFY(clickNamed(h, QStringLiteral("notebookPageTab_0")));
            QTRY_COMPARE(h.notebook.pageId(), writingPage);
            QVERIFY(clickAction(h, "bookLink"));
            tapPage(h, {.55, .90});
            auto *picker = h.find<QObject>("linkTargetDialog");
            QVERIFY(picker);
            QTRY_VERIFY(picker->property("opened").toBool());
            auto *pickerBody = picker->property("contentItem").value<QQuickItem *>();
            QVERIFY(waitForLayoutFrame(h.window()));
            QVERIFY(clickItem(h, buttonWithText(buttonWithText, pickerBody, QStringLiteral("Grafici"))));
            QTRY_COMPARE(qmlList(picker, "entries").size(), 1);
            const auto target = qmlList(picker, "entries").first().toMap();
            QCOMPARE(target.value("itemId").toString(), chartId);
            const QString chartRow = chartTitle + QStringLiteral(" · ") + title + QStringLiteral(", pagina 3");
            QVERIFY(waitForLayoutFrame(h.window()));
            QVERIFY(clickItem(h, buttonWithText(buttonWithText, pickerBody, chartRow)));
            QTRY_VERIFY(!picker->property("visible").toBool());
            chartLink = h.notebook.pageItems().last().toMap();
            QCOMPARE(chartLink.value("targetItemId").toString(), chartId);

            tapLink(h, tableLink);
            QTRY_COMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("tables"));
            QVERIFY(clickAction(h, "linkedEdit"));
            auto *tableEditor = h.find<QObject>("pageItemDialog");
            QVERIFY(tableEditor);
            QTRY_VERIFY(tableEditor->property("opened").toBool());
            const auto findInputCell = [](const auto &self, QQuickItem *item) -> QQuickItem * {
                if (!item) return nullptr;
                if (item->property("remoteClickable").toBool()
                    && item->property("row").toInt() == 1 && item->property("column").toInt() == 1) return item;
                for (auto *child : item->childItems())
                    if (auto *cell = self(self, child)) return cell;
                return nullptr;
            };
            QVERIFY(waitForLayoutFrame(h.window()));
            QVERIFY(clickItem(h, findInputCell(findInputCell, h.find<QQuickItem>("tableGrid"))));
            auto *input = h.find<QQuickItem>("tableCellInput");
            QVERIFY(input);
            QCOMPARE(input->property("text").toString(), QStringLiteral("12"));
            QVERIFY(replaceText(h, input, QStringLiteral("3")));
            QVERIFY(clickNamed(h, QStringLiteral("savePageItemButton")));
            QTRY_VERIFY(!tableEditor->property("visible").toBool());
            const auto revisedTable = h.notebook.pageItems().first().toMap();
            const auto revisedCells = revisedTable.value("cells").toList();
            const auto values = h.notebook.tableCellValues(revisedTable);
            QCOMPARE(revisedCells.value(15).toString(), QStringLiteral("=AVERAGE(D2:D3)"));
            QCOMPARE(values.value(7).toString(), QStringLiteral("-1"));
            QCOMPARE(values.value(15).toString(), QStringLiteral("-1.5"));

            QVERIFY(clickNamed(h, QStringLiteral("editorBackButton")));
            QTRY_COMPARE(h.notebook.pageId(), writingPage);
            QCOMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("writing"));
            QVERIFY(waitForLayoutFrame(h.window()));
            tapLink(h, chartLink);
            QTRY_COMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("charts"));
            QVERIFY(clickAction(h, "linkedEdit"));
            QTRY_VERIFY(chartEditor->property("opened").toBool());
            const auto revisedChart = qmlMap(chartEditor, "preview");
            updatedSeries = revisedChart.value("points").toList();
            QCOMPARE(updatedSeries.size(), 3);
            QVERIFY(updatedSeries != initialChart.value("points").toList());
            const double minimum = revisedChart.value("minimum").toMap().value("y").toDouble();
            const double range = revisedChart.value("yMax").toDouble() - minimum;
            const double expectedValues[]{-1.0, -2.0, -1.5};
            for (int index = 0; index < 3; ++index)
                QCOMPARE(minimum + range * updatedSeries[index].toMap().value("y").toDouble(), expectedValues[index]);
            QVERIFY(clickNamed(h, QStringLiteral("saveTableChart")));
            QTRY_VERIFY(!chartEditor->property("visible").toBool());
            QVERIFY(clickNamed(h, QStringLiteral("editorBackButton")));
            QTRY_COMPARE(h.notebook.pageId(), writingPage);
            QVERIFY(waitForLayoutFrame(h.window()));
            tapLink(h, diagramLink);
            QTRY_COMPARE(h.window()->property("editorWorkspace").toString(), QStringLiteral("diagrams"));
            QVERIFY(clickAction(h, "linkedEdit"));
            auto *diagramEditor = h.find<QObject>("diagramEditor");
            QVERIFY(diagramEditor);
            QTRY_VERIFY(diagramEditor->property("opened").toBool());
            QCOMPARE(qmlList(diagramEditor, "nodes").size(), 2);
            QVERIFY(waitForLayoutFrame(h.window()));
            auto *diagram = qobject_cast<DiagramItem *>(
                findVisualItem(h.window()->contentItem(), QStringLiteral("diagramCanvas")));
            QVERIFY(diagram);
            const QRectF drawing = diagram->viewRect();
            const QPointF center(drawing.x() + .29 * drawing.width(), drawing.y() + .225 * drawing.height());
            QCOMPARE(diagram->nodeAt(center.x(), center.y()), QStringLiteral("count"));
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, diagram->mapToScene(center).toPoint());
            QTRY_COMPARE(diagramEditor->property("selectedNode").toString(), QStringLiteral("count"));
            QVERIFY(clickNamed(h, QStringLiteral("diagramEditLabel")));
            QVERIFY(waitForLayoutFrame(h.window()));
            auto *label = findVisualItem(h.window()->contentItem(), QStringLiteral("diagramLabelInput"));
            QVERIFY(replaceText(h, label, procedure));
            QVERIFY(clickItem(h, buttonWithText(buttonWithText, h.window()->contentItem(), QStringLiteral("Applica"))));
            QTRY_VERIFY(!label->isVisible());
            QVERIFY(waitForLayoutFrame(h.window()));
            auto *diagramFooter = diagramEditor->property("footer").value<QQuickItem *>();
            QVERIFY(clickItem(h, buttonWithText(buttonWithText, diagramFooter, QStringLiteral("Salva"))));
            QTRY_VERIFY(!diagramEditor->property("visible").toBool());
            QVERIFY(clickNamed(h, QStringLiteral("editorBackButton")));
            QTRY_COMPARE(h.notebook.pageId(), writingPage);
            QCOMPARE(h.notebook.exportBookForTransfer(source.value("bookId").toString()), original);
            QVERIFY(clickNamed(h, QStringLiteral("editorBackButton")));
            QTRY_VERIFY(!h.window()->property("editing").toBool());
            QVERIFY(h.window()->close());
        }
        WorkspaceHarness reopened(false, path);
        QVERIFY(reopened.window());
        QVERIFY(clickNamed(reopened, QStringLiteral("editorBackButton")));
        QVERIFY(clickNamed(reopened, QStringLiteral("librarySection_writing")));
        QVERIFY(clickNamed(reopened, QStringLiteral("libraryRow_") + bookId));
        QTRY_COMPARE(reopened.notebook.bookId(), bookId);
        QCOMPARE(reopened.notebook.pageId(), writingPage);
        QCOMPARE(reopened.notebook.markdownText().trimmed(), report);
        QCOMPARE(reopened.notebook.pageCount(), 3);
        const auto savedChart = reopened.notebook.linkedPageItem(chartLink.value("id").toString());
        QCOMPARE(savedChart.value("id").toString(), chartId);
        QCOMPARE(savedChart.value("title").toString(), chartTitle);
        QCOMPARE(savedChart.value("sourceTable").toString(), tableId);
        QCOMPARE(savedChart.value("points").toList(), updatedSeries);
        const auto savedTable = reopened.notebook.linkedPageItem(tableLink.value("id").toString());
        QCOMPARE(savedTable.value("cells").toList().value(15).toString(), QStringLiteral("=AVERAGE(D2:D3)"));
        QCOMPARE(reopened.notebook.tableCellValues(savedTable).value(15).toString(), QStringLiteral("-1.5"));
        const auto savedDiagram = reopened.notebook.linkedPageItem(diagramLink.value("id").toString());
        QCOMPARE(savedDiagram.value("nodes").toList().first().toMap().value("text").toString(), procedure);
        QCOMPARE(savedDiagram.value("edges").toList().first().toMap().value("to").toString(), QStringLiteral("record"));
        QVERIFY(waitForLayoutFrame(reopened.window()));
        tapLink(reopened, chartLink);
        QTRY_COMPARE(reopened.window()->property("editorWorkspace").toString(), QStringLiteral("charts"));
        QCOMPARE(reopened.window()->property("linkedPageItem").toString(), chartId);
        QVERIFY(clickNamed(reopened, QStringLiteral("editorBackButton")));
        QTRY_COMPARE(reopened.notebook.pageId(), writingPage);
        reopened.screenshot(QStringLiteral("workspace-inventory-reopened"));
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

    void tabletPdfSharesWithoutTheFilePicker()
    {
        WorkspaceHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem(tableFixture(QStringLiteral("Scope test"))));
        const QVariantMap node{{"id", "node-1"}, {"shape", "process"}, {"text", "Prepare"},
            {"x", .12}, {"y", .15}, {"width", .46}, {"height", .32}};
        QVERIFY(h.notebook.savePageItem({{"type", "diagram"}, {"nodes", QVariantList{node}}, {"edges", QVariantList{}}}));
        QVERIFY(h.notebook.addPage());
        QVERIFY(h.notebook.goToPage(0));

        auto *dialog = h.find<QObject>("appleShareView");
        auto *format = h.find<QObject>("fileShareFormat");
        auto *picker = h.find<QObject>("shareSavePicker");
        auto *save = h.find<QQuickItem>("saveShareFile");
        auto *send = h.find<QQuickItem>("sendShareFile");
        auto *print = h.find<QQuickItem>("printShareFile");
        auto *wifi = h.find<QQuickItem>("pdfShareWifi");
        auto *bluetooth = h.find<QQuickItem>("pdfShareBluetooth");
        auto *mail = h.find<QQuickItem>("pdfShareEmail");
        QVERIFY(dialog && format && picker && save && send && print && wifi && bluetooth && mail);
        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(clickItem(h, h.find<QQuickItem>("shareBookButton")));
        QTRY_VERIFY(dialog->property("opened").toBool());
        QCOMPARE(dialog->property("sourceBook").toString(), h.notebook.bookId());
        QCOMPARE(dialog->property("sourcePage").toString(), h.notebook.pageId());
        QVERIFY(!send->isVisible());
        QVERIFY(!print->isVisible());

        const auto options = qmlList(dialog, "formats");
        const auto findFormat = [&options](const QString &name) {
            for (qsizetype index = 0; index < options.size(); ++index)
                if (options.at(index).toMap().value("format").toString() == name)
                    return int(index);
            return -1;
        };
        for (const auto &name : {"pdf", "markdown", "markdown-assets", "drawio", "csv", "quaderno", "png"})
            QVERIFY2(findFormat(QString::fromLatin1(name)) >= 0, name);
        QCOMPARE(findFormat(QStringLiteral("source")), -1);
        const auto pagePdf = findFormat(QStringLiteral("pdf-page"));
        QVERIFY(pagePdf >= 0);
        QVERIFY(format->setProperty("currentIndex", pagePdf));
        QCOMPARE(qmlMap(dialog, "selection").value("format").toString(), QStringLiteral("pdf-page"));
        QTRY_COMPARE(wifi->isVisible(), h.wireless.available());
        QTRY_COMPARE(bluetooth->isVisible(), h.wireless.available());
        QVERIFY(mail->isVisible());
        QVERIFY(!save->isVisible());
        QVERIFY(!picker->property("visible").toBool());

        const auto file = h.directory.filePath(QStringLiteral("current-page.pdf"));
        QVERIFY(h.notebook.exportShareFile(qmlMap(dialog, "selection"), QUrl::fromLocalFile(file)));
        QFile pdf(file);
        QVERIFY2(pdf.open(QIODevice::ReadOnly), qPrintable(h.notebook.error()));
        const auto bytes = pdf.readAll();
        QVERIFY(bytes.startsWith("%PDF-"));
        QVERIFY(bytes.contains("/Count 1"));

        QVERIFY(waitForLayoutFrame(h.window()));
        QVERIFY(clickItem(h, mail));
        auto *smtpDialog = h.find<QObject>("smtpDialog");
        QVERIFY(smtpDialog);
        QTRY_VERIFY(smtpDialog->property("opened").toBool());
        QVERIFY(smtpDialog->property("configuring").toBool());
        QVERIFY(h.find<QObject>("smtpHost")->setProperty("text", QStringLiteral("smtp.example.test")));
        QVERIFY(h.find<QObject>("smtpSender")->setProperty("text", QStringLiteral("sender@example.test")));
        QVERIFY(QMetaObject::invokeMethod(smtpDialog, "saveSettings"));
        QTRY_VERIFY(!smtpDialog->property("configuring").toBool());
        QCOMPARE(qmlMap(smtpDialog, "pdfOption").value("format").toString(), QStringLiteral("pdf-page"));
        QVERIFY(h.smtp.configured());
        QVERIFY(!h.smtp.settings().contains("password"));
        QVERIFY(!picker->property("visible").toBool());

        QVERIFY(h.notebook.goToPage(1));
        QTRY_VERIFY(!smtpDialog->property("visible").toBool());
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QTRY_VERIFY(!picker->property("visible").toBool());

        QVERIFY(QMetaObject::invokeMethod(h.window(), "showSettings"));
        auto *settings = h.find<QObject>("settingsView");
        QVERIFY(settings && settings->setProperty("category", 6));
        auto *settingsButton = h.find<QQuickItem>("openSmtpSettings");
        QTRY_VERIFY(settingsButton && settingsButton->isVisible() && settingsButton->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(settingsButton, "clicked"));
        QTRY_VERIFY(smtpDialog->property("opened").toBool());
        QVERIFY(smtpDialog->property("configuring").toBool());
        QCOMPARE(h.find<QObject>("smtpSender")->property("text").toString(), QStringLiteral("sender@example.test"));
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
