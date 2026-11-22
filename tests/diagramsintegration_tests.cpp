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
#include "diagramrecognizer.h"
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

#include "screenorientation.h"
#include <cmath>
#include <limits>

class TestOrientationSensor final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(int orientation READ orientation WRITE setOrientation NOTIFY changed)
    Q_PROPERTY(QString status READ status CONSTANT)
public:
    bool available() const { return true; }
    bool enabled() const { return m_enabled; }
    int orientation() const { return m_orientation; }
    QString status() const { return QStringLiteral("Test sensor"); }
    void setEnabled(bool value) { if (m_enabled != value) { m_enabled = value; emit changed(); } }
    void setOrientation(int value) { if (m_orientation != value) { m_orientation = value; emit changed(); } }
signals:
    void changed();
private:
    bool m_enabled = false;
    int m_orientation = -1;
};

namespace {
QQuickItem *visibleItem(QQuickItem *parent, const QString &name)
{
    if (parent->objectName() == name && parent->isVisible()) return parent;
    for (QQuickItem *child : parent->childItems())
        if (auto *found = visibleItem(child, name)) return found;
    return nullptr;
}

QVariantMap freehandPath(const QList<QPointF> &vertices)
{
    QVariantList points;
    for (int segment = 1; segment < vertices.size(); ++segment)
        for (int sample = segment == 1 ? 0 : 1; sample <= 8; ++sample) {
            const qreal t = qreal(sample) / 8;
            const QPointF p = vertices[segment - 1] * (1 - t) + vertices[segment] * t;
            points.append(QVariantMap{{"x", p.x()}, {"y", p.y()}, {"p", .5}});
        }
    return {{"tool", "pen"}, {"width", 2.5}, {"color", "#222222"}, {"points", points}};
}

QVariantMap freehandBox(const QRectF &rect)
{
    return freehandPath({rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft(), rect.topLeft()});
}

QVariantList qmlArray(QObject *object, const char *property)
{
    const QVariant value = object->property(property);
    return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant().toList() : value.toList();
}

QVariantMap qmlMap(QObject *object, const char *property)
{
    const QVariant value = object->property(property);
    return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant().toMap() : value.toMap();
}

QVariantMap partById(const QVariantList &parts, const QString &id)
{
    for (const QVariant &part : parts)
        if (part.toMap().value("id").toString() == id) return part.toMap();
    return {};
}

int labelIndex(const QVariantList &labels, const QString &kind, const QString &id)
{
    for (int index = 0; index < labels.size(); ++index) {
        const auto label = labels[index].toMap();
        if (label.value("kind").toString() == kind && label.value("id").toString() == id) return index;
    }
    return -1;
}

QVariantList horizontalArrow()
{
    return {freehandPath({{.32, .21}, {.62, .21}}), freehandPath({{.585, .195}, {.62, .21}, {.585, .225}})};
}

QVariantMap convertedDiagramFixture(bool connected = true)
{
    QVariantList strokes{freehandBox(QRectF(.10, .15, .22, .12)), freehandBox(QRectF(.62, .15, .22, .12))};
    if (connected) strokes.append(horizontalArrow());
    auto diagram = DiagramRecognition::recognize(strokes).value("diagram").toMap();
    auto nodes = diagram.value("nodes").toList();
    for (int index = 0; index < nodes.size(); ++index) {
        auto node = nodes[index].toMap();
        node.insert("text", index == 0 ? "Previously edited start" : "Previously edited review");
        nodes[index] = node;
    }
    auto edges = diagram.value("edges").toList();
    for (int index = 0; index < edges.size(); ++index) {
        auto edge = edges[index].toMap();
        edge.insert("text", "Previously edited connection");
        edges[index] = edge;
    }
    diagram.insert("nodes", nodes);
    diagram.insert("edges", edges);
    diagram.insert("title", "Existing workflow");
    return diagram;
}

QVariantList downwardExtension()
{
    return {freehandBox(QRectF(.62, .5, .22, .12)), freehandPath({{.73, .27}, {.73, .5}}),
        freehandPath({{.71, .472}, {.73, .5}, {.75, .472}})};
}

void click(QQuickWindow *window, QQuickItem *item)
{
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
        item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
}

QVariantMap diagramFixture()
{
    return {{"type", "diagram"}, {"nodes", QVariantList{
        QVariantMap{{"id", "start"}, {"shape", "terminal"}, {"text", "Start"}, {"x", .08}, {"y", .1}, {"width", .25}, {"height", .18}},
        QVariantMap{{"id", "step"}, {"shape", "process"}, {"text", "Review"}, {"x", .64}, {"y", .1}, {"width", .28}, {"height", .18}}}},
        {"edges", QVariantList{QVariantMap{{"id", "flow"}, {"from", "start"}, {"to", "step"}, {"text", "Continue"}}}}};
}

void penEvent(QQuickWindow *window, const QPointingDevice &device, QEvent::Type type, QPointF point)
{
    const bool release = type == QEvent::TabletRelease;
    QTabletEvent event(type, &device, point, window->mapToGlobal(point), release ? 0 : .5, 0, 0, 0, 0, 0,
        Qt::NoModifier, type == QEvent::TabletMove ? Qt::NoButton : Qt::LeftButton,
        release ? Qt::NoButton : Qt::LeftButton);
    QCoreApplication::sendEvent(window, &event);
}

QPointF itemCenter(const QVariantMap &item, InkItem *ink)
{
    return {(item.value("x").toDouble() + item.value("width").toDouble() / 2) * ink->width(),
        (item.value("y").toDouble() + item.value("height").toDouble() / 2) * ink->height()};
}

struct DiagramsHarness {
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
    QStringList qmlWarnings;
    QQmlApplicationEngine engine;

    explicit DiagramsHarness(bool tablet = true, QObject *orientation = nullptr)
        : wireless(&notebook, directory.filePath("pairing.key"), true, false)
    {
        notebook.createBook("Diagram UI regression");
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
            {"externalKeyboard", QVariant::fromValue(&keyboard)}, {"tabletMode", tablet},
            {"epaperGeometry", QRect(0, 0, 900, 1200)}, {"fontFamily", QGuiApplication::font().family()}
        };
        if (orientation) initialProperties.insert("screenOrientationModel", QVariant::fromValue(orientation));
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
    bool addConvertedDiagram(const QVariantMap &fixture)
    {
        QVariantList ink{freehandBox(QRectF(.10, .15, .22, .12)), freehandBox(QRectF(.62, .15, .22, .12))};
        if (!fixture.value("edges").toList().isEmpty()) ink.append(horizontalArrow());
        for (const QVariant &stroke : ink)
            if (!notebook.appendStroke(stroke.toMap())) return false;
        const auto selection = notebook.selectContent({QVariantMap{{"x", 0}, {"y", 0}}, QVariantMap{{"x", 1}, {"y", 0}},
            QVariantMap{{"x", 1}, {"y", 1}}, QVariantMap{{"x", 0}, {"y", 1}}}, "contained", {}, "replace");
        const auto plan = notebook.diagramHandwritingPlan(selection, "new");
        if (!notebook.applyDiagramHandwriting(selection, plan, plan.value("diagram").toMap(), {}, true)) return false;
        auto item = notebook.pageItems().last().toMap();
        // Initial recognition retains page coordinates; inserting through the
        // editor would intentionally place and size a newly created diagram.
        item.insert("nodes", fixture.value("nodes"));
        item.insert("edges", fixture.value("edges"));
        return notebook.savePageItem(item);
    }
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

class DiagramsIntegrationTests final : public QObject
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
        qmlRegisterTypesAndRevisions<ScreenOrientation>("Quaderno", 254);
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

    void directImportOpensDraftOnlyForTheSelectedFile_data()
    {
        QTest::addColumn<bool>("diagram");
        QTest::addColumn<QString>("outcome");
        QTest::newRow("csv-import") << false << QStringLiteral("accept");
        QTest::newRow("drawio-import") << true << QStringLiteral("accept");
        QTest::newRow("csv-cancel") << false << QStringLiteral("cancel");
        QTest::newRow("drawio-stale-page") << true << QStringLiteral("stale");
    }

    void directImportOpensDraftOnlyForTheSelectedFile()
    {
        QFETCH(bool, diagram);
        QFETCH(QString, outcome);
        DiagramsHarness h;
        QVERIFY(h.window());
        const auto fileName = h.directory.filePath(diagram ? "fixture.drawio" : "fixture.csv");
        const auto file = QUrl::fromLocalFile(fileName);
        if (diagram) {
            const auto fixture = DiagramRecognition::recognize(
                {freehandBox(QRectF(.1, .15, .22, .12)), freehandBox(QRectF(.62, .15, .22, .12))})
                                     .value("diagram").toMap();
            const auto error = DiagramExport().save(file, fixture, QStringLiteral("Fixture"));
            QVERIFY2(error.isEmpty(), qPrintable(error));
        } else {
            QFile csv(fileName);
            QVERIFY(csv.open(QIODevice::WriteOnly));
            QVERIFY(csv.write("Name,Value\nA,12\nB,7\n") > 0);
        }
        auto *editor = h.find<QObject>(diagram ? "diagramEditor" : "pageItemDialog");
        auto *picker = h.find<QObject>(diagram ? "diagramOpenPicker" : "csvOpenPicker");
        QVERIFY(editor && picker);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "workspaceAction",
                                         Q_ARG(QString, diagram ? "diagramImport" : "importCsv"), Q_ARG(QString, "")));
        QTRY_VERIFY(picker->property("visible").toBool());
        QVERIFY(!editor->property("visible").toBool());
        if (outcome == "cancel") {
            QVERIFY(QMetaObject::invokeMethod(picker, "rejected"));
        } else {
            if (outcome == "stale") QVERIFY(h.notebook.addPage());
            QVERIFY(picker->setProperty("selectedFile", file));
            QVERIFY(QMetaObject::invokeMethod(picker, "accepted"));
        }
        QVERIFY(QMetaObject::invokeMethod(picker, "close"));
        if (outcome == "accept") {
            QTRY_VERIFY(editor->property("visible").toBool());
            if (diagram) QCOMPARE(qmlArray(editor, "nodes").size(), 2);
            else {
                QCOMPARE(editor->property("rowCount").toInt(), 3);
                QCOMPARE(qmlArray(editor, "cellDraft").value(9).toString(), QStringLiteral("12"));
            }
        } else {
            QVERIFY(!editor->property("visible").toBool());
        }
        QVERIFY(h.notebook.pageItems().isEmpty());
    }

    void fifthLibrarySectionOpensAnEmptyWorkspaceWithoutAnEditor()
    {
        DiagramsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.createDiagramBook("Empty design", {}));
        const QString bookId = h.notebook.bookId();
        const QString pageId = h.notebook.pageId();
        const auto revision = h.notebook.bookTransferRevision(bookId);
        QVERIFY(h.notebook.createBook("Other writing"));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showLibrary"));
        auto *section = visibleItem(h.window()->contentItem(), "librarySection_diagrams");
        QTRY_VERIFY(section && section->isVisible());
        QTest::qWait(100);
        click(h.window(), section);
        auto *organizer = h.find<QObject>("libraryOrganizer");
        QVERIFY(organizer);
        QTRY_COMPARE(organizer->property("section").toString(), QStringLiteral("diagrams"));
        QQuickItem *row = nullptr;
        QTRY_VERIFY((row = visibleItem(h.window()->contentItem(), "libraryRow_" + bookId)) != nullptr);
        auto *actions = visibleItem(h.window()->contentItem(), "libraryBookActions_" + bookId);
        QVERIFY(actions);
        const auto otherBook = h.notebook.bookId();
        QTest::qWait(80);
        click(h.window(), actions);
        QTRY_VERIFY(organizer->property("dialogOpen").toBool());
        QVERIFY(!h.window()->property("editing").toBool());
        QCOMPARE(h.notebook.bookId(), otherBook);
        QVERIFY(QMetaObject::invokeMethod(organizer, "closeDialogs"));
        QTRY_VERIFY(!organizer->property("dialogOpen").toBool());
        QTest::qWait(80);
        click(h.window(), row);
        QTRY_VERIFY(h.window()->property("editing").toBool());
        QCOMPARE(h.notebook.bookId(), bookId);
        QCOMPARE(h.notebook.pageId(), pageId);
        QVERIFY(h.notebook.diagramBook());
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        QVERIFY(h.notebook.pageItems().isEmpty());
        QVERIFY(h.notebook.strokes().isEmpty());
        QCOMPARE(h.notebook.bookTransferRevision(bookId), revision);
        h.screenshot("diagram-empty-workspace");
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void automaticRecognitionOffersReviewWithoutConvertingOrOpeningAModal()
    {
        DiagramsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.createDiagramBook("Automatic recognition", {}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *dialog = h.find<QObject>("diagramHandwritingDialog");
        QVERIFY(preferences && dialog);
        QVERIFY(preferences->property("autoDiagramRecognition").toBool());
        QVERIFY(h.notebook.appendStroke(freehandBox(QRectF(.2, .2, .28, .16))));
        const auto before = h.notebook.strokes();
        const QString revision = h.notebook.bookTransferRevision(h.notebook.bookId());
        QTest::qWait(400);
        QVERIFY(visibleItem(h.window()->contentItem(), "diagramRecognitionStatus") == nullptr);
        QQuickItem *review = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((review = visibleItem(h.window()->contentItem(), "reviewHanddrawnDiagram")) != nullptr, 4500);
        QVERIFY(!dialog->property("visible").toBool());
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        QVERIFY(h.notebook.pageItems().isEmpty());
        QCOMPARE(h.notebook.strokes(), before);
        QCOMPARE(h.notebook.bookTransferRevision(h.notebook.bookId()), revision);
        h.screenshot("diagram-recognition-offer");
        QTest::qWait(80);
        click(h.window(), review);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QTRY_VERIFY(dialog->property("canApply").toBool());
        QCOMPARE(qmlArray(dialog, "labelEntries").size(), 1);
        QVERIFY(QMetaObject::invokeMethod(dialog, "setLabelText", Q_ARG(int, 0), Q_ARG(QString, "Reviewed in UI")));
        h.screenshot("diagram-recognition-review");
        // Dismissing review is non-destructive, including manually edited labels.
        QSignalSpy closed(dialog, SIGNAL(closed()));
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_COMPARE(closed.size(), 1);
        QCOMPARE(h.notebook.strokes(), before);
        QVERIFY(h.notebook.pageItems().isEmpty());
        QCOMPARE(h.notebook.bookTransferRevision(h.notebook.bookId()), revision);
        QVERIFY(qmlArray(dialog, "labelEntries").isEmpty());
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void manualReviewAppliesEditableNodesInOneUndoAndDoesNotRequireAutomaticMode()
    {
        DiagramsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.createDiagramBook("Manual review", {}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *dialog = h.find<QObject>("diagramHandwritingDialog");
        QVERIFY(preferences && dialog);
        QVERIFY(preferences->setProperty("autoDiagramRecognition", false));
        QVERIFY(h.notebook.appendStroke(freehandBox(QRectF(.2, .2, .28, .16))));
        const auto original = h.notebook.strokes();
        QTest::qWait(1400);
        QVERIFY(visibleItem(h.window()->contentItem(), "reviewHanddrawnDiagram") == nullptr);
        auto *manual = visibleItem(h.window()->contentItem(), "recognizeDiagramButton");
        QVERIFY(manual && manual->isEnabled());
        click(h.window(), manual);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QTRY_VERIFY(dialog->property("canApply").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "setLabelText", Q_ARG(int, 0), Q_ARG(QString, "Editable step")));
        auto *apply = visibleItem(h.window()->contentItem(), "diagramRecognitionApply");
        QVERIFY(apply && apply->isEnabled());
        QTest::qWait(80);
        click(h.window(), apply);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().size(), 1);
        QVERIFY(h.notebook.strokes().isEmpty());
        const auto diagram = h.notebook.pageItems().first().toMap();
        QCOMPARE(diagram.value("type").toString(), QStringLiteral("diagram"));
        QCOMPARE(diagram.value("nodes").toList().first().toMap().value("text").toString(), QStringLiteral("Editable step"));
        QVERIFY(h.notebook.undo());
        QCOMPARE(h.notebook.strokes(), original);
        QVERIFY(h.notebook.pageItems().isEmpty());
        QVERIFY(h.notebook.redo());
        QVERIFY(h.notebook.strokes().isEmpty());
        QCOMPARE(h.notebook.pageItems().first().toMap(), diagram);
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void extendingConvertedDiagramPreviewsAllPartsAndPreservesEditedLabelsInOneUndo()
    {
        DiagramsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.createDiagramBook("Extend existing workflow", {}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto fixture = convertedDiagramFixture();
        auto fixtureEdges = fixture.value("edges").toList();
        QVERIFY(!fixtureEdges.isEmpty());
        auto fixtureEdge = fixtureEdges.first().toMap();
        // Node and edge IDs are separately scoped in the stored diagram model.
        fixtureEdge.insert("id", fixture.value("nodes").toList().first().toMap().value("id"));
        fixtureEdges[0] = fixtureEdge;
        fixture.insert("edges", fixtureEdges);
        QVERIFY2(h.addConvertedDiagram(fixture), qPrintable(h.notebook.error()));
        const auto original = h.notebook.pageItems().first().toMap();
        QCOMPARE(original.value("nodes").toList().size(), 2);
        QCOMPARE(original.value("edges").toList().size(), 1);
        const QString itemId = original.value("id").toString();
        for (const QVariant &stroke : downwardExtension()) QVERIFY(h.notebook.appendStroke(stroke.toMap()));
        const auto ink = h.notebook.strokes();
        const auto revision = h.notebook.bookTransferRevision(h.notebook.bookId());
        QQuickItem *review = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((review = visibleItem(h.window()->contentItem(), "reviewHanddrawnDiagram")) != nullptr, 4500);
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        QCOMPARE(h.notebook.pageItems(), QVariantList{original});
        QTest::qWait(80);
        click(h.window(), review);
        auto *dialog = h.find<QObject>("diagramHandwritingDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QTRY_VERIFY(dialog->property("canApply").toBool());
        const auto plan = qmlMap(dialog, "plan");
        QCOMPARE(plan.value("targetItemId").toString(), itemId);
        QVERIFY(plan.value("extendsExisting").toBool());
        QVERIFY(plan.value("hasChanges").toBool());
        QCOMPARE(plan.value("addedNodeCount").toInt(), 1);
        QCOMPARE(plan.value("addedEdgeCount").toInt(), 1);
        const auto preview = qmlMap(dialog, "previewDiagram");
        QCOMPARE(preview.value("id").toString(), itemId);
        QCOMPARE(preview.value("nodes").toList().size(), 3);
        QCOMPARE(preview.value("edges").toList().size(), 2);
        const auto labels = qmlArray(dialog, "labelEntries");
        QCOMPARE(labels.size(), 5);
        for (const QString &kind : {QStringLiteral("node"), QStringLiteral("edge")}) {
            const QString collection = kind == "node" ? "nodes" : "edges";
            for (const QVariant &value : original.value(collection).toList()) {
                const auto part = value.toMap();
                const QString id = part.value("id").toString();
                const int index = labelIndex(labels, kind, id);
                QVERIFY(index >= 0);
                QCOMPARE(labels[index].toMap().value("text").toString(), part.value("text").toString());
                QCOMPARE(partById(preview.value(collection).toList(), id).value("text"), part.value("text"));
            }
        }
        int newLabel = -1;
        QString newNodeId;
        for (const QVariant &value : preview.value("nodes").toList()) {
            const QString id = value.toMap().value("id").toString();
            if (partById(original.value("nodes").toList(), id).isEmpty()) {
                newNodeId = id;
                newLabel = labelIndex(labels, "node", id);
            }
        }
        QVERIFY(newLabel >= 0 && !newNodeId.isEmpty());
        QVERIFY(QMetaObject::invokeMethod(dialog, "setLabelText", Q_ARG(int, newLabel), Q_ARG(QString, "New follow-up")));
        // Reviewing the combined graph must not alter either the previous item or the new ink.
        QCOMPARE(h.notebook.pageItems(), QVariantList{original});
        QCOMPARE(h.notebook.strokes(), ink);
        QCOMPARE(h.notebook.bookTransferRevision(h.notebook.bookId()), revision);
        auto *changes = visibleItem(h.window()->contentItem(), "diagramRecognitionChanges");
        QVERIFY(changes);
        QVERIFY(!changes->property("text").toString().isEmpty());
        h.screenshot("diagram-extend-existing-review");
        auto *apply = visibleItem(h.window()->contentItem(), "diagramRecognitionApply");
        QVERIFY(apply && apply->isEnabled());
        QTest::qWait(80);
        click(h.window(), apply);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().size(), 1);
        const auto extended = h.notebook.pageItems().first().toMap();
        QCOMPARE(extended.value("id").toString(), itemId);
        QCOMPARE(extended.value("nodes").toList().size(), 3);
        QCOMPARE(extended.value("edges").toList().size(), 2);
        QCOMPARE(partById(extended.value("nodes").toList(), newNodeId).value("text").toString(), QStringLiteral("New follow-up"));
        QVERIFY(h.notebook.strokes().isEmpty());
        QVERIFY(h.notebook.undo());
        QCOMPARE(h.notebook.pageItems(), QVariantList{original});
        QCOMPARE(h.notebook.strokes(), ink);
        QVERIFY(h.notebook.redo());
        QCOMPARE(h.notebook.pageItems(), QVariantList{extended});
        QVERIFY(h.notebook.strokes().isEmpty());
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void arrowBetweenExistingNodesOffersAndAppliesTheCompleteDiagram()
    {
        DiagramsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.createDiagramBook("Connect existing nodes", {}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QVERIFY2(h.addConvertedDiagram(convertedDiagramFixture(false)), qPrintable(h.notebook.error()));
        const auto original = h.notebook.pageItems().first().toMap();
        QCOMPARE(original.value("nodes").toList().size(), 2);
        QVERIFY(original.value("edges").toList().isEmpty());
        for (const QVariant &stroke : horizontalArrow()) QVERIFY(h.notebook.appendStroke(stroke.toMap()));
        QQuickItem *review = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((review = visibleItem(h.window()->contentItem(), "reviewHanddrawnDiagram")) != nullptr, 4500);
        QTest::qWait(80);
        click(h.window(), review);
        auto *dialog = h.find<QObject>("diagramHandwritingDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QTRY_VERIFY(dialog->property("canApply").toBool());
        const auto plan = qmlMap(dialog, "plan");
        QCOMPARE(plan.value("addedNodeCount").toInt(), 0);
        QCOMPARE(plan.value("addedEdgeCount").toInt(), 1);
        QVERIFY(plan.value("hasChanges").toBool());
        const auto preview = qmlMap(dialog, "previewDiagram");
        QCOMPARE(preview.value("nodes").toList().size(), 2);
        QCOMPARE(preview.value("edges").toList().size(), 1);
        for (const QVariant &node : original.value("nodes").toList()) {
            const auto existing = node.toMap();
            QCOMPARE(partById(preview.value("nodes").toList(), existing.value("id").toString()).value("text"), existing.value("text"));
        }
        auto *apply = visibleItem(h.window()->contentItem(), "diagramRecognitionApply");
        QVERIFY(apply && apply->isEnabled());
        QTest::qWait(80);
        click(h.window(), apply);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().size(), 1);
        const auto connected = h.notebook.pageItems().first().toMap();
        QCOMPARE(connected.value("id"), original.value("id"));
        QCOMPARE(connected.value("nodes").toList().size(), 2);
        QCOMPARE(connected.value("edges").toList().size(), 1);
        QVERIFY(h.notebook.strokes().isEmpty());
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void multipleDiagramsRequireAnExplicitTargetAndOnlyUpdateThatItem()
    {
        DiagramsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.createDiagramBook("Choose workflow to extend", {}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences);
        preferences->setAutoDiagramRecognition(false);
        QVERIFY2(h.addConvertedDiagram(convertedDiagramFixture()), qPrintable(h.notebook.error()));
        auto second = convertedDiagramFixture();
        auto secondNodes = second.value("nodes").toList();
        QVERIFY(!secondNodes.isEmpty());
        auto firstNode = secondNodes.first().toMap();
        firstNode.insert("text", "Second workflow");
        secondNodes[0] = firstNode;
        second.insert("nodes", secondNodes);
        QVERIFY2(h.addConvertedDiagram(second), qPrintable(h.notebook.error()));
        const auto originals = h.notebook.pageItems();
        QCOMPARE(originals.size(), 2);
        const QString firstId = originals.first().toMap().value("id").toString();
        const QString secondId = originals.last().toMap().value("id").toString();
        QVERIFY(firstId != secondId);
        for (const QVariant &stroke : downwardExtension()) QVERIFY(h.notebook.appendStroke(stroke.toMap()));
        const auto ink = h.notebook.strokes();
        const auto revision = h.notebook.bookTransferRevision(h.notebook.bookId());
        auto *manual = visibleItem(h.window()->contentItem(), "recognizeDiagramButton");
        QVERIFY(manual && manual->isEnabled());
        QTest::qWait(80);
        click(h.window(), manual);
        auto *dialog = h.find<QObject>("diagramHandwritingDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(qmlMap(dialog, "plan").value("requiresTarget").toBool());
        QVERIFY(!dialog->property("canApply").toBool());
        auto *target = visibleItem(h.window()->contentItem(), "diagramRecognitionTarget");
        QVERIFY(target && target->isEnabled());
        const auto choices = qmlArray(target, "model");
        int targetIndex = -1;
        for (int index = 0; index < choices.size(); ++index)
            if (choices[index].toMap().value("id").toString() == secondId) targetIndex = index;
        QVERIFY(targetIndex >= 0);
        QVERIFY(target->setProperty("currentIndex", targetIndex));
        QVERIFY(QMetaObject::invokeMethod(target, "activated", Q_ARG(int, targetIndex)));
        QTRY_VERIFY(dialog->property("canApply").toBool());
        QCOMPARE(qmlMap(dialog, "plan").value("targetItemId").toString(), secondId);
        QVERIFY(!qmlMap(dialog, "plan").value("requiresTarget").toBool());
        QCOMPARE(qmlMap(dialog, "previewDiagram").value("nodes").toList().size(), 3);
        QCOMPARE(h.notebook.pageItems(), originals);
        QCOMPARE(h.notebook.strokes(), ink);
        QCOMPARE(h.notebook.bookTransferRevision(h.notebook.bookId()), revision);
        h.screenshot("diagram-choose-existing-target");
        auto *apply = visibleItem(h.window()->contentItem(), "diagramRecognitionApply");
        QVERIFY(apply && apply->isEnabled());
        QTest::qWait(80);
        click(h.window(), apply);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().size(), 2);
        QCOMPARE(partById(h.notebook.pageItems(), firstId), originals.first().toMap());
        const auto extended = partById(h.notebook.pageItems(), secondId);
        QCOMPARE(extended.value("nodes").toList().size(), 3);
        QCOMPARE(extended.value("edges").toList().size(), 2);
        QVERIFY(h.notebook.strokes().isEmpty());
        QVERIFY(h.notebook.undo());
        QCOMPARE(h.notebook.pageItems(), originals);
        QCOMPARE(h.notebook.strokes(), ink);
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void automaticRecognitionWaitsForPinStandbyEditorAndWritableLayer_data()
    {
        QTest::addColumn<QString>("guard");
        QTest::newRow("PIN") << QStringLiteral("pin");
        QTest::newRow("standby") << QStringLiteral("standby");
        QTest::newRow("editor") << QStringLiteral("editor");
        QTest::newRow("hidden-layer") << QStringLiteral("layer");
    }

    void automaticRecognitionWaitsForPinStandbyEditorAndWritableLayer()
    {
        QFETCH(QString, guard);
        DiagramsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.createDiagramBook("Guarded recognition", {}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *dialog = h.find<QObject>("diagramHandwritingDialog");
        QVERIFY(preferences && dialog);
        QVERIFY(preferences->setProperty("autoDiagramRecognition", false));
        QVERIFY(h.notebook.appendStroke(freehandBox(QRectF(.2, .2, .28, .16))));
        const auto revision = h.notebook.bookTransferRevision(h.notebook.bookId());
        QObject *textEditor = nullptr;
        if (guard == "pin") {
            QVERIFY(h.pin.setPin("1234"));
            h.pin.lock();
            QVERIFY(h.pin.locked());
        } else if (guard == "standby") {
            h.standby.suspend();
            QVERIFY(h.standby.sleeping());
        } else if (guard == "editor") {
            QVERIFY(QMetaObject::invokeMethod(h.window(), "editPageItem", Q_ARG(QString, "text"), Q_ARG(QVariant, QVariantMap{})));
            textEditor = h.find<QObject>("pageItemDialog");
            QVERIFY(textEditor);
            QTRY_VERIFY(textEditor->property("visible").toBool());
        } else {
            QVERIFY(h.notebook.setLayerVisible(h.notebook.activeLayer(), false));
        }
        QVERIFY(preferences->setProperty("autoDiagramRecognition", true));
        QTest::qWait(1550);
        QVERIFY(visibleItem(h.window()->contentItem(), "reviewHanddrawnDiagram") == nullptr);
        QVERIFY(!dialog->property("visible").toBool());
        QVERIFY(h.notebook.pageItems().isEmpty());
        if (guard != "layer") QCOMPARE(h.notebook.bookTransferRevision(h.notebook.bookId()), revision);
        if (guard == "pin") QVERIFY(h.pin.unlock("1234"));
        else if (guard == "standby") h.standby.wake();
        else if (guard == "editor") QVERIFY(QMetaObject::invokeMethod(textEditor, "close"));
        else QVERIFY(h.notebook.setLayerVisible(h.notebook.activeLayer(), true));
        QTRY_VERIFY_WITH_TIMEOUT(visibleItem(h.window()->contentItem(), "reviewHanddrawnDiagram") != nullptr, 4500);
        QVERIFY(!dialog->property("visible").toBool());
        QVERIFY(h.notebook.pageItems().isEmpty());
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void diagramWorkspacePlacementAndConnections_data()
    {
        QTest::addColumn<bool>("wide");
        QTest::newRow("desktop-side-panels") << true;
        QTest::newRow("compact-tablet-shelf") << false;
    }

    void diagramWorkspacePlacementAndConnections()
    {
        QFETCH(bool, wide);
        DiagramsHarness h(!wide);
        QVERIFY(h.window());
        h.window()->resize(wide ? QSize(1100, 920) : QSize(460, 790));
        auto *editor = h.find<QObject>("diagramEditor");
        QVERIFY(editor);
        QVERIFY(QMetaObject::invokeMethod(editor, "openFor", Q_ARG(QVariant, QVariant{})));
        QTRY_VERIFY(editor->property("visible").toBool());
        QTRY_COMPARE(editor->property("sidePanels").toBool(), wide);
        auto *palette = visibleItem(h.window()->contentItem(), "diagramShapePalette");
        auto *canvas = visibleItem(h.window()->contentItem(), "diagramCanvas");
        auto *process = visibleItem(h.window()->contentItem(), "diagramShape_process");
        auto *decision = visibleItem(h.window()->contentItem(), "diagramShape_decision");
        QVERIFY(palette && canvas && process && decision);
        const auto center = [](QQuickItem *item) { return item->mapToScene({item->width() / 2, item->height() / 2}).toPoint(); };
        const auto pagePoint = [canvas](qreal x, qreal y) {
            return canvas->mapToScene({x * canvas->width(), y * canvas->height()}).toPoint();
        };
        const auto drag = [&h](QPoint from, QPoint to) {
            QTest::mousePress(h.window(), Qt::LeftButton, Qt::NoModifier, from);
            QTest::mouseMove(h.window(), from + (to - from) / 3);
            QTest::mouseMove(h.window(), to);
            QTest::mouseRelease(h.window(), Qt::LeftButton, Qt::NoModifier, to);
        };

        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, center(process));
        QTRY_COMPARE(qmlArray(editor, "nodes").size(), 1);
        drag(center(decision), pagePoint(.68, .52));
        QTRY_COMPARE(qmlArray(editor, "nodes").size(), 2);
        const auto first = qmlArray(editor, "nodes").at(0).toMap();
        const auto placed = qmlArray(editor, "nodes").at(1).toMap();
        QCOMPARE(placed.value("shape").toString(), QStringLiteral("decision"));
        QVERIFY(std::abs(placed.value("x").toDouble() - .535) < .025);
        QVERIFY(std::abs(placed.value("y").toDouble() - .43) < .025);

        auto *handle = visibleItem(h.window()->contentItem(), "diagramConnectorHandle");
        QVERIFY(handle);
        drag(center(handle), pagePoint(.165, .115));
        QTRY_COMPARE(qmlArray(editor, "edges").size(), 1);
        const auto edge = qmlArray(editor, "edges").first().toMap();
        QCOMPARE(edge.value("from"), placed.value("id"));
        QCOMPARE(edge.value("to"), first.value("id"));
        QQuickItem *orthogonal = nullptr;
        QTRY_VERIFY((orthogonal = visibleItem(h.window()->contentItem(), "diagramOrthogonalRoute"))
            && orthogonal->isEnabled() && orthogonal->width() > 0 && orthogonal->height() > 0);
        QTest::qWait(90); // The inspector's Flow lays out the newly visible edge tools on the next frame.
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, center(orthogonal));
        QTRY_COMPARE(qmlArray(editor, "edges").first().toMap().value("route").toString(), QStringLiteral("orthogonal"));
        auto *moreShapes = visibleItem(h.window()->contentItem(), "diagramMoreShapes");
        QVERIFY(moreShapes);
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, center(moreShapes));
        QQuickItem *database = nullptr;
        QQuickItem *cloud = nullptr;
        QTRY_VERIFY((database = visibleItem(h.window()->contentItem(), "diagramShape_database"))
            && (cloud = visibleItem(h.window()->contentItem(), "diagramShape_cloud"))
            && database->width() > 0 && database->height() > 0
            && cloud->y() >= database->y() + database->height());
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, center(database));
        QTRY_COMPARE(qmlArray(editor, "nodes").size(), 3);
        QCOMPARE(qmlArray(editor, "nodes").last().toMap().value("shape").toString(), QStringLiteral("database"));
        auto *uml = visibleItem(h.window()->contentItem(), "diagramUmlShapes");
        QVERIFY(uml);
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, center(uml));
        QQuickItem *actor = nullptr;
        QQuickItem *lifeline = nullptr;
        QTRY_VERIFY((actor = visibleItem(h.window()->contentItem(), "diagramShape_uml-actor"))
            && (lifeline = visibleItem(h.window()->contentItem(), "diagramShape_uml-lifeline"))
            && actor->width() > 0 && actor->height() > 0 && lifeline->y() >= actor->y() + actor->height());
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, center(actor));
        QTRY_COMPARE(qmlArray(editor, "nodes").size(), 4);
        QCOMPARE(qmlArray(editor, "nodes").last().toMap().value("shape").toString(), QStringLiteral("uml-actor"));
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void diagramDirectTapOpensEditorAndPreservesInk_data()
    {
        QTest::addColumn<QString>("input");
        QTest::newRow("mouse") << QStringLiteral("mouse");
        QTest::newRow("touch") << QStringLiteral("touch");
        QTest::newRow("pen") << QStringLiteral("pen");
    }

    void diagramDirectTapOpensEditorAndPreservesInk()
    {
        QFETCH(QString, input);
        DiagramsHarness h(input != "mouse");
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem(diagramFixture()));
        const auto original = h.notebook.pageItems();
        const auto diagram = original.first().toMap();
        auto *ink = h.find<InkItem>("inkPage");
        auto *editor = h.find<QObject>("diagramEditor");
        QVERIFY(ink && editor);
        QVERIFY(!h.window()->property("editingItems").toBool());
        QTest::qWait(100);
        const auto local = itemCenter(diagram, ink);
        QCOMPARE(ink->diagramItemAt(local), diagram.value("id").toString());
        const auto point = ink->mapToScene(local);
        if (input == "touch") {
            auto *device = QTest::createTouchDevice();
            QTest::touchEvent(h.window(), device).press(0, point.toPoint(), h.window());
            QTest::touchEvent(h.window(), device).release(0, point.toPoint(), h.window());
        } else if (input == "pen") {
            QPointingDevice pen("Diagram UI pen", 731028, QInputDevice::DeviceType::Stylus,
                QPointingDevice::PointerType::Pen,
                QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
            penEvent(h.window(), pen, QEvent::TabletPress, point);
            penEvent(h.window(), pen, QEvent::TabletRelease, point);
        } else QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, point.toPoint());
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(h.window()->property("selectedPageItem").toString(), diagram.value("id").toString());
        QCOMPARE(qmlArray(editor, "nodes"), diagram.value("nodes").toList());
        QCOMPARE(qmlArray(editor, "edges"), diagram.value("edges").toList());
        QVERIFY(h.notebook.strokes().isEmpty());
        QVERIFY(!h.find<QObject>("diagramHandwritingDialog")->property("visible").toBool());
        auto nodes = qmlArray(editor, "nodes");
        auto node = nodes.first().toMap();
        node.insert("text", "Edited diagram step");
        nodes[0] = node;
        QVERIFY(editor->setProperty("nodes", nodes));
        QSignalSpy closed(editor, SIGNAL(closed()));
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QTRY_COMPARE(closed.size(), 1);
        QCOMPARE(h.notebook.pageItems(), original);
        QVERIFY(h.notebook.strokes().isEmpty());
        if (input == "mouse") {
            QTest::qWait(80);
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, point.toPoint());
            QTRY_VERIFY(editor->property("visible").toBool());
            auto edited = diagram;
            edited.insert("nodes", nodes);
            QVERIFY(QMetaObject::invokeMethod(editor, "saved", Q_ARG(QVariant, edited)));
            QTRY_COMPARE(closed.size(), 2);
            QCOMPARE(h.notebook.pageItems().first().toMap().value("nodes").toList(), nodes);
            QCOMPARE(h.notebook.pageItems().first().toMap().value("id"), diagram.value("id"));
            QVERIFY(h.notebook.strokes().isEmpty());
        }
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void diagramPenDragAndHoldStillAnnotateAndTopmostContentBlocksTap()
    {
        DiagramsHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem(diagramFixture()));
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences && preferences->setProperty("autoDiagramRecognition", false));
        auto *ink = h.find<InkItem>("inkPage");
        QVERIFY(ink);
        QTest::qWait(100);
        const auto diagram = h.notebook.pageItems().first().toMap();
        const auto local = itemCenter(diagram, ink);
        const auto point = ink->mapToScene(local);
        QPointingDevice pen("Diagram annotation pen", 731029, QInputDevice::DeviceType::Stylus,
            QPointingDevice::PointerType::Pen,
            QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
        penEvent(h.window(), pen, QEvent::TabletPress, point);
        penEvent(h.window(), pen, QEvent::TabletMove, point + QPointF(24, 8));
        penEvent(h.window(), pen, QEvent::TabletRelease, point + QPointF(40, 12));
        QTRY_COMPARE(h.notebook.strokes().size(), 1);
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        penEvent(h.window(), pen, QEvent::TabletPress, point);
        QTest::qWait(QGuiApplication::styleHints()->mousePressAndHoldInterval() + 30);
        penEvent(h.window(), pen, QEvent::TabletRelease, point);
        QTRY_COMPARE(h.notebook.strokes().size(), 2);
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        const auto annotations = h.notebook.strokes();
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setLasso", Q_ARG(bool, true)));
        penEvent(h.window(), pen, QEvent::TabletPress, point);
        penEvent(h.window(), pen, QEvent::TabletRelease, point);
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        QCOMPARE(h.notebook.strokes(), annotations);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setLasso", Q_ARG(bool, false)));
        QCOMPARE(ink->diagramItemAt(local), diagram.value("id").toString());
        {
            auto *overlay = qvariant_cast<QQuickItem *>(ink->property("inputOverlay"));
            QVERIFY(overlay);
            QQuickItem blocker(overlay);
            blocker.setPosition(overlay->mapFromItem(ink, local) - QPointF(20, 20));
            blocker.setSize({40, 40});
            blocker.setProperty("blocksInk", true);
            QVERIFY(ink->diagramItemAt(local).isEmpty());
        }
        QVERIFY(h.notebook.savePageItem({{"type", "text"}, {"text", "Foreground note"}}));
        const auto textId = h.notebook.pageItems().last().toMap().value("id").toString();
        QVERIFY(h.notebook.movePageItem(textId, local.x() / ink->width() - .03, local.y() / ink->height() - .03));
        QVERIFY(ink->diagramItemAt(local).isEmpty());
        QCOMPARE(ink->textItemAt(local), textId);
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, point.toPoint());
        auto *textEditor = h.find<QObject>("pageItemDialog");
        QVERIFY(textEditor);
        QTRY_VERIFY(textEditor->property("visible").toBool());
        QVERIFY(!h.find<QObject>("diagramEditor")->property("visible").toBool());
        QCOMPARE(h.window()->property("selectedPageItem").toString(), textId);
        QCOMPARE(h.notebook.strokes(), annotations);
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void newPreferencesPersistAndDisplayResetKeepsRecognitionChoice()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("preferences.ini");
        {
            TabletPreferences prefs(path);
            QVERIFY(!prefs.automaticOrientation());
            QVERIFY(prefs.autoDiagramRecognition());
            prefs.setAutomaticOrientation(true);
            prefs.setAutoDiagramRecognition(false);
            QVERIFY(prefs.error().isEmpty());
        }
        TabletPreferences reopened(path);
        QVERIFY(reopened.automaticOrientation());
        QVERIFY(!reopened.autoDiagramRecognition());
        reopened.resetDisplay();
        QVERIFY(!reopened.automaticOrientation());
        QVERIFY(!reopened.autoDiagramRecognition());
        TabletPreferences reset(path);
        QVERIFY(!reset.automaticOrientation());
        QVERIFY(!reset.autoDiagramRecognition());
    }

    void automaticRotationDefersEditorsAndLocksAndHonorsBookOrientation()
    {
        TestOrientationSensor sensor;
        DiagramsHarness h(true, &sensor);
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences);
        sensor.setOrientation(90);
        QVERIFY(!sensor.enabled());
        QCOMPARE(h.window()->property("appliedOrientation").toInt(), 0);
        preferences->setAutomaticOrientation(true);
        QTRY_VERIFY(sensor.enabled());
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 90);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "editPageItem", Q_ARG(QString, "text"), Q_ARG(QVariant, QVariantMap{})));
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(editor);
        QTRY_VERIFY(editor->property("visible").toBool());
        sensor.setOrientation(180);
        QTest::qWait(80);
        QCOMPARE(h.window()->property("appliedOrientation").toInt(), 90);
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 180);
        QVERIFY(h.pin.setPin("1234"));
        h.pin.lock();
        QTRY_VERIFY(!sensor.enabled());
        sensor.setOrientation(270);
        QTest::qWait(80);
        QCOMPARE(h.window()->property("appliedOrientation").toInt(), 180);
        QVERIFY(h.pin.unlock("1234"));
        QTRY_VERIFY(sensor.enabled());
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 270);
        h.standby.suspend();
        QTRY_VERIFY(!sensor.enabled());
        sensor.setOrientation(0);
        QTest::qWait(80);
        QCOMPARE(h.window()->property("appliedOrientation").toInt(), 270);
        h.standby.wake();
        QVERIFY(h.pin.unlock("1234"));
        QTRY_VERIFY(sensor.enabled());
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 0);
        QVERIFY(h.addBookBackground());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QVERIFY(h.notebook.setReadingPreferences({{"readingOrientation", 90}}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 90);
        sensor.setOrientation(270);
        QTest::qWait(80);
        QCOMPARE(h.window()->property("appliedOrientation").toInt(), 90);
        QVERIFY(h.notebook.clearReadingPreferences());
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 270);
        QVERIFY(h.window()->property("fullPage").toBool());
        h.screenshot("diagram-auto-rotation-fullpage");
        sensor.setOrientation(0);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showSettings"));
        auto *settingsView = h.find<QObject>("settingsView");
        QVERIFY(settingsView);
        QVERIFY(QMetaObject::invokeMethod(settingsView, "selectCategory", Q_ARG(int, 5)));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 0);
        h.screenshot("settings-automatic-orientation");
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }

    void dismissedRecognitionOfferWaitsForNewInkWithoutChangingTheSheet()
    {
        DiagramsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.createDiagramBook("Dismiss suggestion", {}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QVERIFY(h.notebook.appendStroke(freehandBox(QRectF(.15, .15, .25, .16))));
        const auto original = h.notebook.strokes();
        const auto revision = h.notebook.bookTransferRevision(h.notebook.bookId());
        QQuickItem *dismiss = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((dismiss = visibleItem(h.window()->contentItem(), "dismissDiagramRecognition")) != nullptr, 4500);
        QTest::qWait(80);
        click(h.window(), dismiss);
        QTRY_VERIFY(visibleItem(h.window()->contentItem(), "diagramRecognitionStatus") == nullptr);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showSettings"));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QTest::qWait(1400);
        QVERIFY(visibleItem(h.window()->contentItem(), "diagramRecognitionStatus") == nullptr);
        QCOMPARE(h.notebook.strokes(), original);
        QVERIFY(h.notebook.pageItems().isEmpty());
        QCOMPARE(h.notebook.bookTransferRevision(h.notebook.bookId()), revision);
        QVERIFY(h.notebook.appendStroke(freehandBox(QRectF(.6, .15, .25, .16))));
        QTRY_VERIFY_WITH_TIMEOUT(visibleItem(h.window()->contentItem(), "reviewHanddrawnDiagram") != nullptr, 4500);
        QVERIFY(h.notebook.pageItems().isEmpty());
        QCOMPARE(h.notebook.strokes().size(), 2);
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
    }

    void applyingWithOriginalInkKeptDoesNotImmediatelyOfferTheSameConversion()
    {
        DiagramsHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.createDiagramBook("Keep handwriting", {}));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QVERIFY(h.notebook.appendStroke(freehandBox(QRectF(.15, .15, .25, .16))));
        const auto original = h.notebook.strokes();
        QQuickItem *review = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((review = visibleItem(h.window()->contentItem(), "reviewHanddrawnDiagram")) != nullptr, 4500);
        QTest::qWait(80);
        click(h.window(), review);
        auto *dialog = h.find<QObject>("diagramHandwritingDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(dialog->setProperty("removeInk", false));
        QVERIFY(QMetaObject::invokeMethod(dialog, "setLabelText", Q_ARG(int, 0), Q_ARG(QString, "Keep both")));
        auto *apply = visibleItem(h.window()->contentItem(), "diagramRecognitionApply");
        QVERIFY(apply && apply->isEnabled());
        QTest::qWait(80);
        click(h.window(), apply);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.strokes(), original);
        QCOMPARE(h.notebook.pageItems().size(), 1);
        const auto revision = h.notebook.bookTransferRevision(h.notebook.bookId());
        auto *quick = h.find<QObject>("quickControlsPopup");
        QVERIFY(quick);
        QVERIFY(QMetaObject::invokeMethod(quick, "open"));
        QTRY_VERIFY(quick->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(quick, "close"));
        QTRY_VERIFY(!quick->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showSettings"));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QTest::qWait(1500);
        QVERIFY(visibleItem(h.window()->contentItem(), "diagramRecognitionStatus") == nullptr);
        QCOMPARE(h.notebook.strokes(), original);
        QCOMPARE(h.notebook.bookTransferRevision(h.notebook.bookId()), revision);
        auto *manual = visibleItem(h.window()->contentItem(), "recognizeDiagramButton");
        QVERIFY(manual && manual->isEnabled());
        const auto existing = h.notebook.pageItems().first().toMap();
        QTest::qWait(80);
        click(h.window(), manual);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(!dialog->property("canApply").toBool());
        QVERIFY(!qmlMap(dialog, "plan").value("hasChanges").toBool());
        apply = visibleItem(h.window()->contentItem(), "diagramRecognitionApply");
        QVERIFY(apply && !apply->isEnabled());
        QCOMPARE(h.notebook.pageItems(), QVariantList{existing});
        QCOMPARE(h.notebook.strokes(), original);
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.bookTransferRevision(h.notebook.bookId()), revision);
        QVERIFY(h.notebook.appendStroke(freehandBox(QRectF(.6, .15, .25, .16))));
        QTRY_VERIFY_WITH_TIMEOUT((review = visibleItem(h.window()->contentItem(), "reviewHanddrawnDiagram")) != nullptr, 4500);
        QCOMPARE(h.notebook.pageItems().size(), 1);
        QCOMPARE(h.notebook.strokes().size(), 2);
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        QTest::qWait(80);
        click(h.window(), review);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QTRY_VERIFY(dialog->property("canApply").toBool());
        const auto plan = qmlMap(dialog, "plan");
        QCOMPARE(plan.value("targetItemId"), existing.value("id"));
        QCOMPARE(plan.value("addedNodeCount").toInt(), 1);
        QCOMPARE(qmlMap(dialog, "previewDiagram").value("nodes").toList().size(), 2);
        QVERIFY(dialog->setProperty("removeInk", false));
        apply = visibleItem(h.window()->contentItem(), "diagramRecognitionApply");
        QVERIFY(apply && apply->isEnabled());
        QTest::qWait(80);
        click(h.window(), apply);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().size(), 1);
        QCOMPARE(h.notebook.pageItems().first().toMap().value("id"), existing.value("id"));
        QCOMPARE(h.notebook.pageItems().first().toMap().value("nodes").toList().size(), 2);
        QCOMPARE(h.notebook.strokes().size(), 2);
        QTest::qWait(1400);
        QVERIFY(visibleItem(h.window()->contentItem(), "diagramRecognitionStatus") == nullptr);
        QVERIFY2(h.qmlWarnings.isEmpty(), qPrintable(h.qmlWarnings.join(QStringLiteral(" | "))));
    }
};

QTEST_MAIN(DiagramsIntegrationTests)
#include "diagramsintegration_tests.moc"
