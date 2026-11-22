#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <memory>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSValue>
#include <QPainterPath>
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

namespace {
QVariantList outline()
{
    return {QVariantMap{{"x",0.0},{"y",0.0}}, QVariantMap{{"x",1.0},{"y",0.0}},
            QVariantMap{{"x",1.0},{"y",1.0}}, QVariantMap{{"x",0.0},{"y",1.0}}};
}

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

QPointF chartPoint(const QVariantMap &chart, qreal x, qreal y, InkItem *ink)
{
    const QRectF bounds(chart.value("x").toDouble() * 600, chart.value("y").toDouble() * 800,
                        chart.value("width").toDouble() * 600, chart.value("height").toDouble() * 800);
    QTransform transform;
    transform.translate(bounds.center().x(), bounds.center().y());
    transform.rotate(chart.value("rotation").toDouble());
    transform.translate(-bounds.center().x(), -bounds.center().y());
    const QPointF point = transform.map(bounds.topLeft() + QPointF(x * bounds.width(), y * bounds.height()));
    return {point.x() / 600 * ink->width(), point.y() / 800 * ink->height()};
}

QPointF tableCellPoint(const QVariantMap &table, int row, int column, InkItem *ink)
{
    const QRectF bounds(table.value("x").toDouble() * 600, table.value("y").toDouble() * 800,
                        table.value("width").toDouble() * 600, table.value("height").toDouble() * 800);
    const QPointF cell(bounds.left() + (column + .5) * bounds.width() / table.value("cols").toInt(),
                       bounds.top() + (row + .5) * bounds.height() / table.value("rows").toInt());
    QTransform transform;
    transform.translate(bounds.center().x(), bounds.center().y());
    transform.rotate(table.value("rotation").toDouble());
    transform.translate(-bounds.center().x(), -bounds.center().y());
    const QPointF rotated = transform.map(cell);
    return {rotated.x() / 600 * ink->width(), rotated.y() / 800 * ink->height()};
}

void tabletEvent(QQuickWindow *window, const QPointingDevice &device, QEvent::Type type, QPointF point,
                 qreal pressure = .65)
{
    const bool release = type == QEvent::TabletRelease;
    QTabletEvent event(type, &device, point, window->mapToGlobal(point), release ? 0 : pressure,
                       0, 0, 0, 0, 0, Qt::NoModifier,
                       type == QEvent::TabletMove ? Qt::NoButton : Qt::LeftButton,
                       release ? Qt::NoButton : Qt::LeftButton);
    QCoreApplication::sendEvent(window, &event);
}

// Exercise real asynchronous Handwriting/QProcess callbacks without depending
// on OCR accuracy or a platform-specific recognition engine in UI tests.
struct TestHandwritingHelper {
    QStringList created;
    QString capturePath;
    bool ready = false;
    explicit TestHandwritingHelper(bool captureInk = false, bool probeContext = false)
    {
        const QDir directory(QCoreApplication::applicationDirPath());
        for (const QString &name : {QStringLiteral("quaderno-handwriting"), QStringLiteral("quaderno-handwriting-it.dct"),
                                   QStringLiteral("quaderno-handwriting-en.dct")}) {
            if (directory.exists(name)) return; // Never replace an installed engine or dictionary.
        }
        if (captureInk) {
            capturePath = directory.filePath(QStringLiteral("quaderno-handwriting.ink"));
            QFile capture(capturePath);
            if (!capture.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return;
            created.append(capturePath);
        }
        for (const QString &name : {QStringLiteral("quaderno-handwriting"), QStringLiteral("quaderno-handwriting-it.dct"),
                                   QStringLiteral("quaderno-handwriting-en.dct")}) {
            const QString path = directory.filePath(name);
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return;
            created.append(path);
            const QByteArray capture = captureInk ? QByteArray("cat > \"$0.ink\"\n") : QByteArray("cat >/dev/null\n");
            const QByteArray proseResponse = probeContext ? QByteArray(
                "if [ \"${QUADERNO_HANDWRITING_PROSE-}\" = 1 ]; then\n"
                "    printf '%s\\n' '{\"text\":\"CIAO\",\"alternatives\":[\"CIAO\",\"CIA0\"]}'\n"
                "    exit 0\nfi\n") : QByteArray{};
            const QByteArray contents = name.endsWith(".dct") ? QByteArray("test dictionary")
                : "#!/bin/sh\n" + capture + "sleep 0.1\n" + proseResponse + "printf '%s\\n' '{\"text\":\"12\"}'\n";
            if (file.write(contents) != contents.size()) return;
            file.close();
            if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) return;
        }
        ready = true;
    }
    ~TestHandwritingHelper() { for (const QString &path : created) QFile::remove(path); }
};

struct MainHarness {
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

    explicit MainHarness(bool tablet = true)
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
    QVariantMap addHandwrittenTable(bool includeFormula = false)
    {
        if (!notebook.savePageItem({{"type", "table"}, {"rows", 2}, {"cols", 3},
            {"cells", QVariantList{"", "=SUM(A1:A2)", "", "", "", ""}}})) return {};
        const QVariantMap table = notebook.pageItems().first().toMap();
        const double x = table.value("x").toDouble(), y = table.value("y").toDouble();
        const double width = table.value("width").toDouble() / 3, height = table.value("height").toDouble() / 2;
        const QList<QPoint> cells = includeFormula ? QList<QPoint>{{0, 0}, {0, 1}, {1, 0}} : QList<QPoint>{{0, 0}, {0, 1}};
        for (const QPoint &cell : cells) {
            QVariantList points;
            for (double offset : {.3, .6}) points.append(QVariantMap{{"x", x + (cell.x() + offset) * width},
                {"y", y + (cell.y() + .5) * height}, {"p", .6}});
            if (!notebook.appendStroke({{"tool", "pen"}, {"color", "#222222"}, {"width", 2.5}, {"points", points}})) return {};
        }
        return table;
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
}

class UiTests : public QObject
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
        REGISTER(ObsidianSync); REGISTER(MixedPageAnalyzer); REGISTER(PagePreviewItem); REGISTER(PageTemplateItem); REGISTER(PaperProStandby);
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

    void rotatedSceneKeepsCellAndChartInputAligned_data()
    {
        QTest::addColumn<int>("angle");
        QTest::addColumn<bool>("penInput");
        for (const int angle : {0, 90, 180, 270}) {
            QTest::newRow(qPrintable(QString("%1-finger").arg(angle))) << angle << false;
            QTest::newRow(qPrintable(QString("%1-pen").arg(angle))) << angle << true;
        }
    }

    void rotatedSceneKeepsCellAndChartInputAligned()
    {
        QFETCH(int, angle);
        QFETCH(bool, penInput);
        MainHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *scene = h.find<QQuickItem>("orientedScene");
        auto *ink = h.find<InkItem>("inkPage");
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(preferences && scene && ink && editor);
        QVERIFY(preferences->setProperty("orientation", angle));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), angle);
        QTRY_COMPARE(scene->size(), angle % 180 ? QSizeF(1200, 900) : QSizeF(900, 1200));
        QCOMPARE(h.window()->size(), QSize(900, 1200));
        const QRectF nativeBounds(QPointF(0, 0), h.window()->size());
        QTRY_VERIFY(qAbs(scene->mapRectToScene(scene->boundingRect()).x()) < .01
            && qAbs(scene->mapRectToScene(scene->boundingRect()).y()) < .01);
        const QRectF sceneBounds = scene->mapRectToScene(scene->boundingRect());
        QVERIFY(qAbs(sceneBounds.width() - 900) < .01 && qAbs(sceneBounds.height() - 1200) < .01);
        auto *overlay = qvariant_cast<QQuickItem *>(h.window()->property("popupOverlay"));
        QVERIFY(overlay);
        QTRY_COMPARE(overlay->size(), scene->size());
        QTRY_COMPARE(overlay->rotation(), qreal(angle));
        QVERIFY(h.notebook.savePageItem({{"type", "table"}, {"rows", 2}, {"cols", 2},
            {"cells", QVariantList{"12", "=SUM(A1:A2)", "7", ""}}}));
        const auto table = h.notebook.pageItems().first().toMap();
        const QString bookId = h.notebook.bookId(), pageId = h.notebook.pageId();
        auto *touchDevice = QTest::createTouchDevice();
        QPointingDevice pen("Rotated UI pen", 731026, QInputDevice::DeviceType::Stylus,
            QPointingDevice::PointerType::Pen,
            QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
        const auto tap = [&](QPointF point) {
            if (penInput) {
                tabletEvent(h.window(), pen, QEvent::TabletPress, point);
                tabletEvent(h.window(), pen, QEvent::TabletRelease, point);
            } else {
                QTest::touchEvent(h.window(), touchDevice).press(0, point.toPoint(), h.window());
                QTest::touchEvent(h.window(), touchDevice).release(0, point.toPoint(), h.window());
            }
        };
        QTest::qWait(80);
        tap(ink->mapToScene(tableCellPoint(table, 0, 1, ink)));
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(editor->property("selectedRow").toInt(), 0);
        QCOMPARE(editor->property("selectedColumn").toInt(), 1);
        QCOMPARE(editor->findChild<QQuickItem *>("tableCellInput")->property("text").toString(), QString("=SUM(A1:A2)"));
        auto *content = qvariant_cast<QQuickItem *>(editor->property("contentItem"));
        QVERIFY(content && content->parentItem());
        QTRY_VERIFY(nativeBounds.adjusted(-1, -1, 1, 1).contains(
            content->parentItem()->mapRectToScene(content->parentItem()->boundingRect())));
        QSignalSpy closed(editor, SIGNAL(closed()));
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QTRY_COMPARE(closed.count(), 1);

        const auto chart = h.addChart(true);
        QVERIFY(!chart.isEmpty());
        const auto itemsBefore = h.notebook.pageItems();
        auto *chartEditor = h.find<QObject>("tableChartDialog");
        QVERIFY(chartEditor);
        QTest::qWait(50);
        tap(ink->mapToScene(chartPoint(chart, .5, .5, ink)));
        QTRY_VERIFY(chartEditor->property("visible").toBool());
        QCOMPARE(qmlMap(chartEditor, "original").value("id"), chart.value("id"));
        QCOMPARE(qmlMap(chartEditor, "source").value("id"), chart.value("sourceTable"));
        QVERIFY(h.notebook.strokes().isEmpty());
        QCOMPARE(h.notebook.pageItems(), itemsBefore);
        QCOMPARE(h.notebook.bookId(), bookId);
        QCOMPARE(h.notebook.pageId(), pageId);
        h.screenshot(QString("rotation-%1-%2").arg(angle).arg(penInput ? "pen" : "touch"));
    }

    void orientationWaitsForEditorAndPreservesDocument()
    {
        MainHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences && h.addStroke());
        QVERIFY(h.notebook.savePageItem({{"type", "table"}, {"rows", 2}, {"cols", 2},
            {"cells", QVariantList{"1", "2", "3", "4"}}}));
        const auto table = h.notebook.pageItems().first().toMap();
        QVERIFY(preferences->setProperty("orientation", 90));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 90);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "editTableCell", Q_ARG(QString, table.value("id").toString()),
            Q_ARG(int, 0), Q_ARG(int, 0)));
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(editor && editor->property("visible").toBool());
        auto *input = editor->findChild<QQuickItem *>("tableCellInput");
        QVERIFY(input && input->setProperty("text", "Unsaved draft"));
        QVERIFY(preferences->setProperty("orientation", 270));
        QTest::qWait(60);
        QCOMPARE(h.window()->property("appliedOrientation").toInt(), 90);
        QCOMPARE(input->property("text").toString(), QString("Unsaved draft"));
        const QString bookId = h.notebook.bookId(), pageId = h.notebook.pageId();
        const auto originalInk = h.notebook.strokes(), originalItems = h.notebook.pageItems();
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 270);
        QCOMPARE(h.notebook.bookId(), bookId);
        QCOMPARE(h.notebook.pageId(), pageId);
        QCOMPARE(h.notebook.strokes(), originalInk);
        QCOMPARE(h.notebook.pageItems(), originalItems);
        h.window()->resize(940, 1240);
        auto *overlay = qvariant_cast<QQuickItem *>(h.window()->property("popupOverlay"));
        QTRY_COMPARE(overlay->size(), QSizeF(1240, 940));
        QTRY_COMPARE(overlay->mapRectToScene(overlay->boundingRect()).size(), QSizeF(940, 1240));
    }

    void orientationWaitsForPenStrokeAndSelection()
    {
        MainHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *ink = h.find<InkItem>("inkPage");
        auto *selection = h.find<QQuickItem>("selectionOverlay");
        QVERIFY(preferences && ink && selection);
        QTest::qWait(80);
        QPointingDevice pen("Rotation deferred pen", 731027, QInputDevice::DeviceType::Stylus,
            QPointingDevice::PointerType::Pen,
            QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
        const QPointF start = ink->mapToScene({ink->width() * .3, ink->height() * .4});
        const QPointF end = ink->mapToScene({ink->width() * .5, ink->height() * .5});
        tabletEvent(h.window(), pen, QEvent::TabletPress, start);
        QVERIFY(ink->property("drawing").toBool());
        QVERIFY(preferences->setProperty("orientation", 90));
        QTest::qWait(30);
        QCOMPARE(h.window()->property("appliedOrientation").toInt(), 0);
        tabletEvent(h.window(), pen, QEvent::TabletMove, end);
        tabletEvent(h.window(), pen, QEvent::TabletRelease, end);
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 90);
        QCOMPARE(h.notebook.strokes().size(), 1);
        const auto strokes = h.notebook.strokes();
        QVERIFY(QMetaObject::invokeMethod(h.window(), "useLasso", Q_ARG(QVariant, QVariant(outline()))));
        QVERIFY(selection->property("active").toBool());
        QVERIFY(preferences->setProperty("orientation", 180));
        QTest::qWait(30);
        QCOMPARE(h.window()->property("appliedOrientation").toInt(), 90);
        QVERIFY(selection->property("active").toBool());
        QVERIFY(QMetaObject::invokeMethod(selection, "clear"));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 180);
        QCOMPARE(h.notebook.strokes(), strokes);
    }

    void rotatedPinAndStandbyRemainUsable()
    {
        MainHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences && preferences->setProperty("orientation", 270));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 270);
        const QString bookId = h.notebook.bookId(), pageId = h.notebook.pageId();
        QVERIFY(h.pin.setPin("1234"));
        h.pin.lock();
        auto *pinScreen = h.find<QQuickItem>("pinScreen");
        QVERIFY(pinScreen);
        QTRY_VERIFY(pinScreen->isVisible());
        QCOMPARE(pinScreen->mapRectToScene(pinScreen->boundingRect()).size(), QSizeF(900, 1200));
        const auto buttonWithText = [](auto &&self, QQuickItem *item, const QString &text) -> QQuickItem * {
            if (item->isVisible() && item->isEnabled() && item->property("text").toString() == text
                && item->metaObject()->indexOfSignal("clicked()") >= 0) return item;
            for (auto *child : item->childItems()) if (auto *found = self(self, child, text)) return found;
            return nullptr;
        };
        auto *touch = QTest::createTouchDevice();
        QTest::qWait(60);
        for (const QString &label : {QString("1"), QString("2"), QString("3"), QString("4"), QString("Sblocca")}) {
            auto *button = buttonWithText(buttonWithText, pinScreen, label);
            QVERIFY2(button, qPrintable(label));
            const QPoint point = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
            QTest::touchEvent(h.window(), touch).press(0, point, h.window());
            QTest::touchEvent(h.window(), touch).release(0, point, h.window());
        }
        QTRY_VERIFY(!h.pin.locked());
        QVERIFY(h.pin.removePin("1234"));
        h.standby.suspend();
        QTRY_VERIFY(h.standby.sleeping());
        auto *sleep = h.find<QQuickItem>("standbyOverlay");
        QVERIFY(sleep && sleep->isVisible());
        QCOMPARE(sleep->mapRectToScene(sleep->boundingRect()).size(), QSizeF(900, 1200));
        const QPoint point = sleep->mapToScene(QPointF(20, 20)).toPoint();
        QTest::touchEvent(h.window(), touch).press(0, point, h.window());
        QTest::touchEvent(h.window(), touch).release(0, point, h.window());
        QTRY_VERIFY(!h.standby.sleeping());
        QCOMPARE(h.notebook.bookId(), bookId);
        QCOMPARE(h.notebook.pageId(), pageId);
        QVERIFY(h.notebook.strokes().isEmpty());
    }

    void tabletPreferencesPersistAndFullPageAppliesOnlyOnEntry()
    {
        {
            MainHarness h;
            QVERIFY(h.window());
            auto *preferences = h.find<TabletPreferences>("tabletPreferences");
            QVERIFY(preferences);
            QVERIFY(preferences->setProperty("orientation", 270));
            QVERIFY(preferences->setProperty("interfaceScale", 115));
            QVERIFY(preferences->setProperty("idleMinutes", 10));
            QVERIFY(preferences->setProperty("keepAwakeWhileCharging", true));
            QVERIFY(preferences->setProperty("showStatusBar", false));
            QVERIFY(preferences->setProperty("gestureNavigationEnabled", false));
            QVERIFY(preferences->setProperty("startFullPage", true));
            QVERIFY(!h.window()->property("fullPage").toBool());
        }
        MainHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences);
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 270);
        QVERIFY(h.window()->property("fullPage").toBool());
        QCOMPARE(h.standby.idleMinutes(), 10);
        QVERIFY(h.standby.property("keepAwakeWhileCharging").toBool());
        QVERIFY(!h.find<QQuickItem>("deviceStatusRow")->isVisible());
        QVERIFY(qAbs(h.window()->property("uiScale").toDouble() - (900.0 / 790 * 1.15)) < .00001);
        QVERIFY(preferences->setProperty("startFullPage", false));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QVERIFY(h.window()->property("fullPage").toBool());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showLibrary"));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showCurrentBook"));
        QVERIFY(!h.window()->property("fullPage").toBool());
        QVERIFY2(h.addBookBackground(), qPrintable(h.notebook.error()));
        auto *gestures = h.find<QQuickItem>("readingGestures");
        QVERIFY(gestures && !gestures->isEnabled());
        QVERIFY(preferences->setProperty("gestureNavigationEnabled", true));
        QTRY_VERIFY(gestures->isEnabled());
    }

    void rotatedReaderSwipeUsesPageAxes_data()
    {
        QTest::addColumn<int>("angle");
        QTest::newRow("landscape-right") << 90;
        QTest::newRow("landscape-left") << 270;
    }

    void rotatedReaderSwipeUsesPageAxes()
    {
        QFETCH(int, angle);
        MainHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences && preferences->setProperty("orientation", angle));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), angle);
        QVERIFY2(h.addBookBackground(), qPrintable(h.notebook.error()));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));
        auto *gestures = h.find<QQuickItem>("readingGestures");
        QVERIFY(gestures);
        QTRY_VERIFY(gestures->isEnabled());
        QTest::qWait(100);
        auto *device = QTest::createTouchDevice();
        const auto point = [&](qreal distance) {
            return gestures->mapToScene(QPointF(gestures->width() / 2 + distance, gestures->height() / 2)).toPoint();
        };
        auto swipe = QTest::touchEvent(h.window(), device, false);
        swipe.press(0, point(180), h.window()).commit();
        for (int distance = 130; distance >= -190; distance -= 40) {
            swipe.move(0, point(distance), h.window()).commit();
            QTest::qWait(15);
        }
        swipe.release(0, point(-190), h.window()).commit();
        QTRY_COMPARE(h.notebook.page(), 1);
        QVERIFY(h.notebook.strokes().isEmpty());
    }

    void settingsSearchDisplayControlsAndUnknownPenStatus()
    {
        MainHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *settingsView = h.find<QQuickItem>("settingsView");
        auto *search = h.find<QQuickItem>("settingsSearch");
        QVERIFY(preferences && settingsView && search);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showSettings"));
        QTRY_VERIFY(settingsView->isVisible());
        QVERIFY(search->setProperty("text", "orientamento"));
        QTRY_COMPARE(qmlList(settingsView, "matchingCategories").size(), 1);
        QCOMPARE(qmlList(settingsView, "matchingCategories").first().toMap().value("id").toInt(), 5);
        QQuickItem *screenButton = nullptr;
        QTRY_VERIFY((screenButton = findVisualItem(h.window()->contentItem(), "settingsCategory_5")));
        QTest::qWait(60);
        auto *touch = QTest::createTouchDevice();
        const auto tap = [&](QQuickItem *button) {
            const QPoint point = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
            QTest::touchEvent(h.window(), touch).press(0, point, h.window());
            QTest::touchEvent(h.window(), touch).release(0, point, h.window());
        };
        tap(screenButton);
        QTRY_COMPARE(settingsView->property("category").toInt(), 5);
        QCOMPARE(search->property("text").toString(), QString());
        auto *landscape = findVisualItem(h.window()->contentItem(), "orientation_90");
        auto *scale = findVisualItem(h.window()->contentItem(), "interfaceScale_130");
        QVERIFY(landscape && scale);
        QTest::qWait(60);
        tap(landscape);
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 90);
        QVERIFY(QMetaObject::invokeMethod(scale, "clicked"));
        QCOMPARE(preferences->property("interfaceScale").toInt(), 130);
        QVERIFY(qAbs(h.window()->property("uiScale").toDouble() - (900.0 / 790 * 1.3)) < .00001);
        h.screenshot("settings-screen-landscape-large");

        QVERIFY(QMetaObject::invokeMethod(settingsView, "selectCategory", Q_ARG(int, 7)));
        auto *marker = h.find<QQuickItem>("markerBatteryReadout");
        auto *statusMarker = h.find<QQuickItem>("markerStatusLabel");
        QVERIFY(marker && statusMarker);
        QCOMPARE(h.device.markerBatteryPercent(), -1);
        QCOMPARE(marker->property("text").toString(), h.device.markerStatus());
        QCOMPARE(statusMarker->property("text").toString(), h.device.markerStatus());
        QVERIFY(!marker->property("text").toString().contains("0%"));
        h.screenshot("settings-pen-unknown");

        QVERIFY(QMetaObject::invokeMethod(settingsView, "selectCategory", Q_ARG(int, 6)));
        auto *accounts = h.find<QQuickItem>("accountsPanel");
        QTRY_VERIFY(accounts && accounts->isVisible());
        QVERIFY(!accounts->property("browserAvailable").toBool());
        h.screenshot("settings-accounts-landscape-large");
        QVERIFY(search->setProperty("text", "nonexistent-setting-keyword"));
        QTRY_VERIFY(qmlList(settingsView, "matchingCategories").isEmpty());
        QVERIFY(search->setProperty("text", "cloud sync"));
        QTRY_COMPARE(qmlList(settingsView, "matchingCategories").size(), 1);
        QCOMPARE(qmlList(settingsView, "matchingCategories").first().toMap().value("id").toInt(), 6);
    }

    void accountsActionsAreExplicitAndLockedAccountsDoNothing()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(QUADERNO_SOURCE_DIR "/qml/AccountsPanel.qml")));
        const QVariantMap sync{{"available", true}, {"installed", true}, {"busy", false}, {"status", "Ready"}};
        const QVariantMap wireless{{"available", true}, {"active", false}, {"paired", false}, {"status", "Ready"}};
        std::unique_ptr<QObject> panel(component.createWithInitialProperties({
            {"uiScale", 1.0}, {"tabletMode", false}, {"obsidianSync", sync}, {"wireless", wireless}}));
        QVERIFY2(panel, qPrintable(component.errorString()));
        QSignalSpy markdown(panel.get(), SIGNAL(openMarkdown()));
        QSignalSpy obsidian(panel.get(), SIGNAL(openObsidianSync()));
        QSignalSpy apple(panel.get(), SIGNAL(openApple()));
        QSignalSpy books(panel.get(), SIGNAL(openAppleBooks()));
        QSignalSpy files(panel.get(), SIGNAL(importFiles()));
        QSignalSpy network(panel.get(), SIGNAL(openWireless()));
        const auto dispatch = [&](const QString &action) {
            return QMetaObject::invokeMethod(panel.get(), "dispatchAction", Q_ARG(QString, action), Q_ARG(QString, QString()));
        };
        QVERIFY(dispatch("markdown") && dispatch("obsidianSync") && dispatch("wireless"));
        QCOMPARE(markdown.count(), 1);
        QCOMPARE(obsidian.count(), 1);
        QCOMPARE(network.count(), 1);
        QVERIFY(dispatch("apple") && dispatch("appleBooks") && dispatch("import"));
        const int macActions = panel->property("browserAvailable").toBool() ? 1 : 0;
        QCOMPARE(apple.count(), macActions);
        QCOMPARE(books.count(), macActions);
        QCOMPARE(files.count(), macActions);
        bool launched = true;
        QVERIFY(QMetaObject::invokeMethod(panel.get(), "launchService", Q_RETURN_ARG(bool, launched),
            Q_ARG(QString, "unknown-provider")));
        QVERIFY(!launched);
        QVERIFY(panel->property("browserError").toBool());
        QVERIFY(!panel->property("browserStatus").toString().isEmpty());
        QVERIFY(panel->setProperty("pinLocked", true));
        QCOMPARE(panel->property("browserStatus").toString(), QString());
        QVERIFY(!panel->property("enabled").toBool());
        for (const QString &action : {QString("markdown"), QString("obsidianSync"), QString("wireless"),
             QString("apple"), QString("appleBooks"), QString("import"), QString("website")}) QVERIFY(dispatch(action));
        QVERIFY(QMetaObject::invokeMethod(panel.get(), "launchService", Q_RETURN_ARG(bool, launched),
            Q_ARG(QString, "googleDrive")));
        QVERIFY(!launched);
        QCOMPARE(markdown.count(), 1);
        QCOMPARE(obsidian.count(), 1);
        QCOMPARE(network.count(), 1);
        QCOMPARE(apple.count(), macActions);
        QCOMPARE(books.count(), macActions);
        QCOMPARE(files.count(), macActions);
        QCOMPARE(panel->property("browserStatus").toString(), QString());
    }

    // Included inside UiTests; uses the production MainHarness and QML components.
    void selectionPrecisionAndKeyboardRespectTextFocus()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.addStroke());
        QVERIFY(h.notebook.savePageItem({{"type", "text"}, {"text", "Editable"}}));
        const auto before = h.notebook.strokes();
        const auto itemsBefore = h.notebook.pageItems();
        QTest::qWait(80);
        auto *selection = h.find<QQuickItem>("selectionOverlay");
        QVERIFY(selection);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "useLasso", Q_ARG(QVariant, QVariant(outline()))));
        QTRY_VERIFY(selection->hasActiveFocus());
        auto *summary = h.find<QQuickItem>("selectionSummary");
        QVERIFY(summary && summary->property("text").toString().contains("1"));
        QTest::keyClick(h.window(), Qt::Key_Right);
        QVERIFY(qAbs(selection->property("dx").toDouble() - 1.0/600) < .000001);
        QTest::keyClick(h.window(), Qt::Key_Down, Qt::ShiftModifier);
        QVERIFY(qAbs(selection->property("dy").toDouble() - 10.0/800) < .000001);
        QVERIFY(QMetaObject::invokeMethod(selection, "turn", Q_ARG(int, 1)));
        QCOMPARE(selection->property("angle").toInt(), 15);
        QVERIFY(QMetaObject::invokeMethod(selection, "resizeBy", Q_ARG(double, 1.1)));
        QCOMPARE(selection->property("factor").toDouble(), 1.1);
        QCOMPARE(h.notebook.strokes(), before);
        QCOMPARE(h.notebook.pageItems(), itemsBefore);
        QVERIFY(QMetaObject::invokeMethod(selection, "resetTransform"));
        QVERIFY(selection->property("active").toBool());
        QVERIFY(!selection->property("changed").toBool());
        QTest::keyClick(h.window(), Qt::Key_Right);
        QTest::keyClick(h.window(), Qt::Key_Return);
        QTRY_VERIFY(!selection->property("active").toBool());
        QVERIFY(h.notebook.strokes() != before);
        QVERIFY(h.notebook.undo());
        QCOMPARE(h.notebook.strokes(), before);
        QCOMPARE(h.notebook.pageItems(), itemsBefore);

        QVERIFY(QMetaObject::invokeMethod(h.window(), "useLasso", Q_ARG(QVariant, QVariant(outline()))));
        QTRY_VERIFY(selection->property("active").toBool());
        selection->setProperty("previewTranscript", true);
        auto *transcript = selection->findChild<QQuickItem *>("selectionTranscript");
        QVERIFY(transcript && transcript->isVisible());
        transcript->setProperty("text", "ab");
        transcript->setProperty("cursorPosition", 1);
        transcript->forceActiveFocus();
        QTRY_VERIFY(transcript->hasActiveFocus());
        QTest::keyClick(h.window(), Qt::Key_Right);
        QTest::keyClick(h.window(), Qt::Key_Return);
        QCOMPARE(transcript->property("text").toString(), QString("ab\n"));
        QCOMPARE(selection->property("dx").toDouble(), 0.0);
        QVERIFY(selection->property("active").toBool());
        QCOMPARE(h.notebook.strokes(), before);
        QCOMPARE(h.notebook.pageItems(), itemsBefore);
        selection->setProperty("previewTranscript", false);
        selection->forceActiveFocus();
        QTest::keyClick(h.window(), Qt::Key_Escape);
        QTRY_VERIFY(!selection->property("active").toBool());
    }

    void touchKeyboardEditsWithoutSplittingSupplementaryCharacters()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QQmlComponent inputComponent(&h.engine);
        inputComponent.setData("import QtQuick\nTextEdit { width: 320; height: 40 }", QUrl());
        QScopedPointer<QObject> input(inputComponent.create());
        QVERIFY2(input, qPrintable(inputComponent.errorString()));
        auto *inputItem = qobject_cast<QQuickItem *>(input.data());
        QVERIFY(inputItem);
        inputItem->setParentItem(h.window()->contentItem());
        inputItem->setY(200);
        inputItem->setZ(1000);

        QQmlComponent component(&h.engine, QUrl::fromLocalFile(QStringLiteral(QUADERNO_SOURCE_DIR "/qml/TouchKeyboard.qml")));
        QScopedPointer<QObject> keyboard(component.createWithInitialProperties({
            {"target", QVariant::fromValue(inputItem)}, {"uiScale", 1.0}}));
        QVERIFY2(keyboard, qPrintable(component.errorString()));
        auto *keyboardItem = qobject_cast<QQuickItem *>(keyboard.data());
        QVERIFY(keyboardItem);
        keyboardItem->setParentItem(h.window()->contentItem());
        keyboardItem->setPosition({10, 250});
        keyboardItem->setWidth(320);
        keyboardItem->setZ(1000);
        QTRY_VERIFY(keyboardItem->implicitHeight() > 0);
        keyboardItem->setHeight(keyboardItem->implicitHeight());
        const QString text = QString::fromUtf8("a🙂è");
        input->setProperty("text", text);
        input->setProperty("cursorPosition", 1);
        QVERIFY(QMetaObject::invokeMethod(keyboard.data(), "moveCursor", Q_ARG(int, 1)));
        QCOMPARE(input->property("cursorPosition").toInt(), 3);
        QVERIFY(QMetaObject::invokeMethod(keyboard.data(), "typeKey", Q_ARG(QString, QString("⌫"))));
        QCOMPARE(input->property("text").toString(), QString::fromUtf8("aè"));
        QVERIFY(QMetaObject::invokeMethod(keyboard.data(), "editCommand", Q_ARG(QString, QString("undo"))));
        QCOMPARE(input->property("text").toString(), text);
        input->setProperty("cursorPosition", 1);
        auto *deleteKey = keyboard->findChild<QQuickItem *>("keyboardDelete");
        QVERIFY(deleteKey && deleteKey->isVisible());
        QTest::qWait(50);
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
                         deleteKey->mapToScene({deleteKey->width()/2, deleteKey->height()/2}).toPoint());
        QCOMPARE(input->property("text").toString(), QString::fromUtf8("aè"));
        QVERIFY(QMetaObject::invokeMethod(keyboard.data(), "editCommand", Q_ARG(QString, QString("undo"))));
        QCOMPARE(input->property("text").toString(), text);
        QVERIFY(QMetaObject::invokeMethod(keyboard.data(), "editCommand", Q_ARG(QString, QString("selectAll"))));
        QVERIFY(QMetaObject::invokeMethod(keyboard.data(), "editCommand", Q_ARG(QString, QString("cut"))));
        QCOMPARE(input->property("text").toString(), QString());
        QVERIFY(QMetaObject::invokeMethod(keyboard.data(), "editCommand", Q_ARG(QString, QString("paste"))));
        QCOMPARE(input->property("text").toString(), text);
        auto *returnKey = keyboard->findChild<QQuickItem *>("keyboardReturn");
        QVERIFY(returnKey && returnKey->isVisible());
        QVERIFY(QMetaObject::invokeMethod(keyboard.data(), "moveToEnd", Q_ARG(bool, true)));
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
                         returnKey->mapToScene({returnKey->width()/2, returnKey->height()/2}).toPoint());
        QCOMPARE(input->property("text").toString(), text + '\n');
        input->setProperty("readOnly", true);
        QVERIFY(QMetaObject::invokeMethod(keyboard.data(), "typeKey", Q_ARG(QString, QString("x"))));
        QVERIFY(QMetaObject::invokeMethod(keyboard.data(), "deleteForward"));
        QCOMPARE(input->property("text").toString(), text + '\n');
        keyboard->setProperty("editingTools", true);
        keyboardItem->setHeight(keyboardItem->implicitHeight());
        h.screenshot("touch-keyboard-editing");
    }

    void templatePreviewCanBeCancelledBeforeApplying()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.addStroke());
        const auto inkBefore = h.notebook.strokes();
        const QString original = h.notebook.pageTemplate();
        auto *dialog = h.find<QObject>("pageTemplateDialog");
        QVERIFY(dialog);
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY(dialog->property("visible").toBool());
        // Repeater delegates have a visual parent but need not be QObject
        // descendants of the window. Locate the rendered card through childItems.
        const auto findGrid = [](auto &&self, QQuickItem *item) -> QQuickItem * {
            if (item->objectName() == "templateOption_grid") return item;
            for (QQuickItem *child : item->childItems())
                if (auto *found = self(self, child)) return found;
            return nullptr;
        };
        QQuickItem *grid = nullptr;
        QTRY_VERIFY((grid = findGrid(findGrid, h.window()->contentItem())));
        QTRY_VERIFY(grid->isVisible() && grid->width() > 0 && grid->height() > 0);
        QTest::qWait(80);
        const QPointF center = grid->mapToScene({grid->width()/2, grid->height()/2});
        QVERIFY(QRectF(0, 0, h.window()->width(), h.window()->height()).contains(center));
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
                         grid->mapToScene({grid->width()/2, grid->height()/2}).toPoint());
        QCOMPARE(dialog->property("pendingTemplate").toString(), QString("grid"));
        QCOMPARE(h.notebook.pageTemplate(), original);
        h.screenshot("template-preview");
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_COMPARE(dialog->property("pendingTemplate").toString(), original);
        dialog->setProperty("pendingTemplate", "grid");
        QVERIFY(QMetaObject::invokeMethod(dialog, "applyPreview"));
        QTRY_COMPARE(h.notebook.pageTemplate(), QString("grid"));
        QCOMPARE(h.notebook.strokes(), inkBefore);
        QTRY_VERIFY(!dialog->property("visible").toBool());
    }

    void hiddenActiveLayerOffersDirectRecovery()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.addStroke());
        const auto inkBefore = h.notebook.strokes();
        QVERIFY(h.notebook.setLayerVisible(h.notebook.activeLayer(), false));
        QVERIFY(!h.notebook.canWriteLayer());
        auto *dialog = h.find<QObject>("pageLayersDialog");
        QVERIFY(dialog);
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY(dialog->property("visible").toBool());
        auto *notice = h.find<QQuickItem>("hiddenLayerNotice");
        auto *show = h.find<QQuickItem>("showActiveLayer");
        QVERIFY(notice && notice->isVisible() && show && show->isVisible());
        QTest::qWait(60);
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
                         show->mapToScene({show->width()/2, show->height()/2}).toPoint());
        QTRY_VERIFY(h.notebook.canWriteLayer());
        QVERIFY(!notice->isVisible());
        QCOMPARE(h.notebook.strokes(), inkBefore);
    }

    // Included inside UiTests; exercises production dialogs/backends without contacting devices.
    void wifiReconnectUsesSavedLocalAddressAndRejectsInternetTargets()
    {
        for (const QString &address : {QStringLiteral("192.168.1.20"), QStringLiteral("10.20.30.40"),
                 QStringLiteral("172.20.4.2"), QStringLiteral("169.254.5.6")})
            QVERIFY2(WifiSyncLink::isSupportedAddress(address), qPrintable(address));
        for (const QString &address : {QStringLiteral("8.8.8.8"), QStringLiteral("127.0.0.1"),
                 QStringLiteral("10.11.99.1"), QStringLiteral("192.168.1.999"), QStringLiteral("192.168.1"),
                 QStringLiteral("192.168.001.2"), QStringLiteral("host.local"), QStringLiteral("::1")})
            QVERIFY2(!WifiSyncLink::isSupportedAddress(address), qPrintable(address));
        QTemporaryDir directory;
        QFile key(directory.filePath("pairing.key"));
        QVERIFY(key.open(QIODevice::WriteOnly));
        QCOMPARE(key.write(QByteArray(16, 'x')), qint64(16));
        key.close();
        QVERIFY(key.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        QSettings config(QSettings::IniFormat, QSettings::UserScope, "Quaderno", "Quaderno");
        config.beginGroup("wirelessTransport");
        config.setValue("type", "wifi");
        config.setValue("address", "192.168.1.20");
        config.sync();
        Notebook notebook(directory.filePath("books.json"));
        WirelessSync wireless(&notebook, key.fileName(), false);
        QCOMPARE(wireless.savedWifiAddress(), QStringLiteral("192.168.1.20"));
        QVERIFY(wireless.paired());
        wireless.startWifi("8.8.8.8", {});
        QVERIFY(!wireless.active());
        QVERIFY(!wireless.status().isEmpty());
        QCOMPARE(wireless.savedWifiAddress(), QStringLiteral("192.168.1.20"));
        config.remove("");
        config.sync();
    }

    void obsidianSearchPreservesConflictActionsAndHonorsExternalKeyboard()
    {
        MainHarness h;
        QVERIFY(h.window());
        const QString vault = h.directory.filePath("search-vault");
        QVERIFY(QDir().mkpath(vault + "/vault/Plans"));
        QVERIFY(QDir().mkpath(vault + "/vault/Recipes"));
        auto write = [](const QString &path, const QByteArray &bytes) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
        };
        QVERIFY(write(vault + "/vault-id", "fixture-vault"));
        QVERIFY(write(vault + "/ob-quaderno", "#!/bin/sh\nexit 99\n"));
        QVERIFY(QFile::setPermissions(vault + "/ob-quaderno", QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        QVERIFY(write(vault + "/vault/Plans/Roadmap.md", "# Original\n"));
        QVERIFY(write(vault + "/vault/Recipes/Soup.md", "# Recipe\n"));
        ObsidianSync sync(&h.notebook, vault, true);
        sync.reload();
        QCOMPARE(sync.notes().size(), 2);
        sync.openNote("Plans/Roadmap.md", {});
        QVERIFY(h.notebook.updateMarkdownText("# Local changes\n"));
        QVERIFY(write(vault + "/vault/Plans/Roadmap.md", "# Cloud changes\n"));
        sync.reload();
        QQmlComponent component(&h.engine, QUrl::fromLocalFile(QStringLiteral(QUADERNO_SOURCE_DIR "/qml/ObsidianSyncDialog.qml")));
        std::unique_ptr<QObject> dialog(component.createWithInitialProperties({
            {"sync", QVariant::fromValue(&sync)}, {"uiScale", 1.0}, {"showTouchKeyboard", true},
            {"parent", QVariant::fromValue(h.window()->contentItem())}}));
        QVERIFY2(dialog, qPrintable(component.errorString()));
        QVERIFY(QMetaObject::invokeMethod(dialog.get(), "open"));
        auto *search = dialog->findChild<QQuickItem *>("obsidianSearchInput");
        auto *list = dialog->findChild<QQuickItem *>("obsidianNoteList");
        auto *keyboard = dialog->findChild<QQuickItem *>("obsidianSearchKeyboard");
        QVERIFY(search && list && keyboard);
        QTRY_COMPARE(dialog->property("conflictCount").toInt(), 1);
        QCOMPARE(dialog->property("changedCount").toInt(), 1);
        QTRY_COMPARE(list->property("count").toInt(), 2);
        dialog->setProperty("noteFilter", 1);
        QTRY_COMPARE(list->property("count").toInt(), 1);
        search->setProperty("text", "PLANS/");
        QTRY_COMPARE(list->property("count").toInt(), 1);
        search->setProperty("text", "Soup");
        QTRY_COMPARE(list->property("count").toInt(), 0);
        dialog->setProperty("noteFilter", 0);
        QTRY_COMPARE(list->property("count").toInt(), 1);
        dialog->setProperty("searchKeyboardOpen", true);
        QTRY_VERIFY(keyboard->isVisible());
        dialog->setProperty("showTouchKeyboard", false);
        QTRY_VERIFY(!keyboard->isVisible());
        QCOMPARE(sync.notes().size(), 2);
        QVERIFY(!sync.busy()); // Searching/filtering never launches the sync wrapper or changes notes.
        QVERIFY(QMetaObject::invokeMethod(dialog.get(), "close"));
    }

    void appleExportRevealResolvesOnlyItsOwnFiles()
    {
#ifdef Q_OS_MACOS
        QTemporaryDir directory;
        QTemporaryDir outside;
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(QDir().mkpath(directory.filePath(id)));
        auto write = [](const QString &path, const QByteArray &bytes) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
        };
        const QString path = directory.filePath(id + "/note.md");
        QVERIFY(write(path, "# Export\n"));
        QVERIFY(write(directory.filePath("queue.json"), QJsonDocument(QJsonArray{QJsonObject{
            {"id", id}, {"file", "note.md"}, {"state", "completed"}, {"destination", "notes"}, {"title", "Export"}}}).toJson()));
        AppleShare share(false, nullptr, directory.path());
        QCOMPARE(share.deliveryFileUrl(id), QUrl::fromLocalFile(QFileInfo(path).canonicalFilePath()));
        MainHarness h(false);
        QVERIFY(h.window());
        h.window()->resize(400, 500);
        auto *dialog = h.find<QObject>("appleShareView");
        QVERIFY(dialog);
        QVERIFY(dialog->setProperty("appleShare", QVariant::fromValue(&share)));
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY(dialog->property("height").toReal() <= h.window()->height());
        dialog->setProperty("deliveryFilter", 1);
        QTRY_COMPARE(dialog->property("visibleDeliveries").value<QJSValue>().property("length").toInt(), 0);
        dialog->setProperty("deliveryFilter", 2);
        QTRY_COMPARE(dialog->property("visibleDeliveries").value<QJSValue>().property("length").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QVERIFY(share.deliveryFileUrl("../../outside").isEmpty());
        QVERIFY(share.deliveryFileUrl(QUuid::createUuid().toString(QUuid::WithoutBraces)).isEmpty());
        QVERIFY(QFile::remove(path));
        QVERIFY(share.deliveryFileUrl(id).isEmpty());
        QVERIFY(write(outside.filePath("note.md"), "# Outside\n"));
        QVERIFY(QFile::link(outside.filePath("note.md"), path));
        QVERIFY(share.deliveryFileUrl(id).isEmpty());
        QVERIFY(!share.revealDelivery(id)); // No Finder launch for a symlink or a missing file.
        QCOMPARE(share.deliveries().first().toMap().value("state").toString(), QStringLiteral("completed"));
        QVERIFY(QFile::remove(path));
        QVERIFY(QDir().rmdir(directory.filePath(id)));
        QVERIFY(QFile::link(outside.path(), directory.filePath(id)));
        QVERIFY(share.deliveryFileUrl(id).isEmpty());
#else
        QSKIP("Finder exports are available only on macOS");
#endif
    }

    void transferReviewsFitSmallMacWindows()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        h.window()->resize(400, 500);
        for (const char *name : {"usbImportDialog", "usbTransferDialog", "appleShareView", "markdownEditor"}) {
            auto *dialog = h.find<QObject>(name);
            QVERIFY2(dialog, name);
            QVERIFY(QMetaObject::invokeMethod(dialog, "open")); // Review only; no preview/download action.
            QTRY_VERIFY(dialog->property("height").toReal() <= h.window()->height());
            // Window resize reaches the overlay and popup bindings on the next layout pass.
            QTRY_VERIFY2(dialog->property("width").toReal() <= h.window()->width(), name);
            QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        }
    }


    void overviewJumpsAndFiltersWithoutChangingTheOpenPage()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.notebook.togglePageBookmark());
        QVERIFY(h.notebook.addPage());
        QVERIFY(h.addStroke());
        QVERIFY(h.notebook.addPage());
        QVERIFY(h.notebook.goToPage(0));
        auto *overview = h.find<QObject>("pageOverview");
        QVERIFY(overview);
        QVERIFY(QMetaObject::invokeMethod(overview, "open"));
        QTRY_VERIFY(overview->property("visible").toBool());
        auto *number = h.find<QObject>("overviewPageNumber");
        QVERIFY(number);
        number->setProperty("text", "3");
        QVERIFY(QMetaObject::invokeMethod(overview, "jumpToNumber"));
        QCOMPARE(overview->property("selectedPage").toInt(), 2);
        QCOMPARE(h.notebook.page(), 0);
        QVERIFY(QMetaObject::invokeMethod(overview, "setPageFilter", Q_ARG(QString, QStringLiteral("bookmarks"))));
        QCOMPARE(overview->property("selectedPage").toInt(), 0);
        QCOMPARE(overview->property("bookmarkCount").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(overview, "setPageFilter", Q_ARG(QString, QStringLiteral("annotated"))));
        QCOMPARE(overview->property("selectedPage").toInt(), 1);
        QCOMPARE(overview->property("annotatedCount").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(overview, "openPage"));
        QTRY_COMPARE(h.notebook.page(), 1);
    }

    void librarySortsByLengthAndFiltersMarkdown()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.notebook.createBook("Zebra"));
        QVERIFY(h.notebook.addPage());
        QVERIFY(h.notebook.addPage());
        QVERIFY(h.notebook.importMarkdownText("# Markdown", "Alpha", {}));
        auto *organizer = h.find<QObject>("libraryOrganizer");
        QVERIFY(organizer);
        organizer->setProperty("sortMode", "pagesDescending");
        QVariant result;
        QVERIFY(QMetaObject::invokeMethod(organizer, "filteredBooks", Q_RETURN_ARG(QVariant, result), Q_ARG(QString, QString{})));
        const auto list = [](const QVariant &value) {
            return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant().toList() : value.toList();
        };
        QVERIFY(!list(result).isEmpty());
        QCOMPARE(list(result).first().toMap().value("title").toString(), QStringLiteral("Zebra"));
        organizer->setProperty("formatFilter", "markdown");
        QVERIFY(QMetaObject::invokeMethod(organizer, "filteredBooks", Q_RETURN_ARG(QVariant, result), Q_ARG(QString, QString{})));
        QCOMPARE(list(result).size(), 1);
        QCOMPARE(list(result).first().toMap().value("title").toString(), QStringLiteral("Alpha"));
        organizer->setProperty("section", "books");
        QCOMPARE(organizer->property("formatFilter").toString(), QStringLiteral("all"));
    }

    void libraryItemsOpenTheirSheetBeforeExplicitEditing_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::newRow("table") << QStringLiteral("table");
        QTest::newRow("standalone-chart") << QStringLiteral("chart");
        QTest::newRow("table-linked-chart") << QStringLiteral("linked-chart");
    }

    void libraryItemsOpenTheirSheetBeforeExplicitEditing()
    {
        QFETCH(QString, kind);
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.notebook.addPage()); // The library item must navigate to its actual page, not page one.
        const QString targetBook = h.notebook.bookId();
        const QString targetPage = h.notebook.pageId();
        if (kind != "chart") {
            QVERIFY(h.notebook.savePageItem({{"type", "table"}, {"rows", 2}, {"cols", 2},
                {"cells", QVariantList{"1", "2", "3", "=SUM(A1:A2)"}}}));
        }
        if (kind == "linked-chart") {
            const QString tableId = h.notebook.pageItems().first().toMap().value("id").toString();
            QVERIFY(h.notebook.saveTableChart({{"sourceTable", tableId}, {"xColumn", 1}, {"yColumn", 2},
                {"hasHeader", false}, {"chartKind", "line"}, {"title", "Linked library chart"}}));
        } else if (kind == "chart") {
            QVERIFY(h.notebook.savePageItem({{"type", "chart"}, {"chartKind", "line"},
                {"title", "Standalone library chart"}, {"points", QVariantList{
                    QVariantMap{{"x", .2}, {"y", .3}}, QVariantMap{{"x", .7}, {"y", .8}}}}}));
        }
        QVERIFY(h.addStroke());
        const QVariantList originalItems = h.notebook.pageItems();
        const QVariantList originalInk = h.notebook.strokes();
        const QVariantMap item = originalItems.last().toMap();
        const QString itemId = item.value("id").toString();
        QVariant entry;
        for (const QVariant &candidate : h.notebook.libraryItems()) {
            if (candidate.toMap().value("itemId").toString() == itemId)
                entry = candidate;
        }
        QVERIFY(entry.isValid());
        QVERIFY(h.notebook.createBook("Another notebook"));
        const QString otherBook = h.notebook.bookId();
        auto *organizer = h.find<QObject>("libraryOrganizer");
        auto *ink = h.find<InkItem>("inkPage");
        QVERIFY(organizer && ink);
        organizer->setProperty("section", kind == "table" ? "tables" : "charts");
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showLibrary"));

        // Exercise the real library row, including its delegate's onClicked routing.
        auto *library = h.find<QQuickItem>("libraryBooks");
        QVERIFY(library);
        const auto findRow = [&](auto &&self, QQuickItem *parent) -> QQuickItem * {
            if (parent->objectName() == "libraryRow_" + itemId)
                return parent;
            for (QQuickItem *child : parent->childItems()) {
                if (auto *found = self(self, child))
                    return found;
            }
            return nullptr;
        };
        QQuickItem *row = nullptr;
        QTRY_VERIFY((row = findRow(findRow, library)) != nullptr);
        QTRY_VERIFY(row->isVisible() && row->width() > 0 && row->height() > 0);
        QTest::qWait(80);
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
            row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint());
        QTRY_COMPARE(h.notebook.bookId(), targetBook);
        QCOMPARE(h.notebook.pageId(), targetPage);
        QVERIFY(h.window()->property("editing").toBool());
        QVERIFY2(!h.window()->property("editorPopupOpen").toBool(),
            "Opening a library item should show its sheet without opening an editor.");
        QVERIFY(!h.window()->property("editingItems").toBool());
        QCOMPARE(h.notebook.pageItems(), originalItems);
        QCOMPARE(h.notebook.strokes(), originalInk);

        // Editing remains an explicit action on the displayed sheet.
        const char *editorName = kind == "table" ? "pageItemDialog"
            : kind == "linked-chart" ? "tableChartDialog" : "chartEditor";
        auto *editor = h.find<QObject>(editorName);
        QVERIFY(editor);
        QTest::qWait(80);
        if (kind == "table") {
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
                ink->mapToScene(tableCellPoint(item, 1, 1, ink)).toPoint());
        } else {
            h.window()->setProperty("editingItems", true);
            const QPointF center((item.value("x").toDouble() + item.value("width").toDouble() / 2) * ink->width(),
                                 (item.value("y").toDouble() + item.value("height").toDouble() / 2) * ink->height());
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, ink->mapToScene(center).toPoint());
        }
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(h.window()->property("selectedPageItem").toString(), itemId);
        if (kind == "table") {
            QCOMPARE(editor->property("selectedRow").toInt(), 1);
            QCOMPARE(editor->property("selectedColumn").toInt(), 1);
            auto *input = editor->findChild<QQuickItem *>("tableCellInput");
            QVERIFY(input);
            QCOMPARE(input->property("text").toString(), QStringLiteral("=SUM(A1:A2)"));
        }
        QSignalSpy closed(editor, SIGNAL(closed()));
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QTRY_COMPARE(closed.count(), 1);
        QCOMPARE(h.notebook.pageItems(), originalItems);
        QCOMPARE(h.notebook.strokes(), originalInk);

        // Programmatic navigation has the same viewing-only contract as the row click.
        QVERIFY(h.notebook.openBook(otherBook));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "showLibrary"));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "openLibraryItem", Q_ARG(QVariant, entry)));
        QCOMPARE(h.notebook.bookId(), targetBook);
        QCOMPARE(h.notebook.pageId(), targetPage);
        QVERIFY(h.window()->property("editing").toBool());
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        QVERIFY(!h.window()->property("editingItems").toBool());
        QCOMPARE(h.notebook.pageItems(), originalItems);
        QCOMPARE(h.notebook.strokes(), originalInk);
    }

    void readerDoubleTapZoomAnchorsAndIgnoresDisabledInput()
    {
        MainHarness h;
        QVERIFY(h.window());
        QVERIFY(h.addBookBackground());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));
        auto *gestures = h.find<QQuickItem>("readingGestures");
        QVERIFY(gestures);
        QTRY_VERIFY(gestures->isEnabled());
        QTest::qWait(80);
        const QPointF point(gestures->width() * .65, gestures->height() * .6);
        QVERIFY(QMetaObject::invokeMethod(gestures, "toggleZoomAt", Q_ARG(QPointF, point)));
        QTRY_COMPARE(h.window()->property("readingZoom").toDouble(), 2.0);
        auto *viewport = h.find<QQuickItem>("paperViewport");
        QVERIFY(viewport);
        QTRY_VERIFY(viewport->property("contentY").toDouble() > 0);
        QVERIFY(QMetaObject::invokeMethod(gestures, "toggleZoomAt", Q_ARG(QPointF, point)));
        QTRY_COMPARE(h.window()->property("readingZoom").toDouble(), 1.0);
        QVERIFY(!h.window()->property("readingFitWidth").toBool());
        QTRY_COMPARE(viewport->property("contentY").toDouble(), 0.0);
        gestures->setEnabled(false);
        QVERIFY(QMetaObject::invokeMethod(gestures, "toggleZoomAt", Q_ARG(QPointF, point)));
        QCOMPARE(h.window()->property("readingZoom").toDouble(), 1.0);
        QVERIFY(h.notebook.strokes().isEmpty());
    }

    void markdownFormattingPreviewAndDiscardKeepSourceSafe()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        auto *editor = h.find<QObject>("markdownEditor");
        QVERIFY(editor);
        QVERIFY(QMetaObject::invokeMethod(editor, "begin", Q_ARG(bool, true)));
        auto *name = editor->findChild<QQuickItem *>("markdownName");
        auto *source = editor->findChild<QQuickItem *>("markdownSource");
        auto *mode = editor->findChild<QObject *>("markdownMode");
        QVERIFY(name && source && mode);
        name->setProperty("text", "Test note");
        source->setProperty("text", "hello world");
        QVERIFY(QMetaObject::invokeMethod(source, "select", Q_ARG(int,0),Q_ARG(int,5)));
        QVERIFY(QMetaObject::invokeMethod(editor, "insertMarkup", Q_ARG(QString,QString("**")),Q_ARG(QString,QString("**"))));
        QCOMPARE(source->property("text").toString(), QString("**hello** world"));
        QCOMPARE(editor->property("wordCount").toInt(), 2);
        source->setProperty("text", "## title\n- word");
        QCOMPARE(editor->property("wordCount").toInt(), 2);
        source->setProperty("text", "**hello** world");
        mode->setProperty("currentIndex",1);
        QTRY_VERIFY(editor->property("previewing").toBool());
        auto *preview = editor->findChild<QQuickItem *>("markdownPreview");
        QVERIFY(preview && preview->isVisible());
        QCOMPARE(source->property("text").toString(), QString("**hello** world"));
        h.screenshot("markdown-preview");
        QVERIFY(QMetaObject::invokeMethod(editor, "requestDiscard",Q_ARG(bool,false)));
        QVERIFY(editor->property("visible").toBool());
        QVERIFY(editor->property("confirmingDiscard").toBool());
        editor->setProperty("confirmingDiscard",false);
        QVERIFY(QMetaObject::invokeMethod(editor,"save"));
        QTRY_VERIFY(!editor->property("visible").toBool());
        QCOMPARE(h.notebook.markdownSource(), QString("**hello** world"));
        const auto before=h.notebook.markdownSource();
        QVERIFY(QMetaObject::invokeMethod(editor,"begin",Q_ARG(bool,false)));
        source->setProperty("text","Unsaved replacement");
        QVERIFY(QMetaObject::invokeMethod(editor,"requestDiscard",Q_ARG(bool,false)));
        QVERIFY(QMetaObject::invokeMethod(editor,"discard"));
        QCOMPARE(h.notebook.markdownSource(),before);
    }

    void tableCropRequiresConfirmationAndExplainsFormulaErrors()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem({{"type","table"},{"rows",3},{"cols",2},
            {"cells",QVariantList{"1","2","3","4","5","6"}}}));
        const QVariant item=h.notebook.pageItems()[0];
        auto *editor=h.find<QObject>("pageItemDialog");
        QVERIFY(editor);
        QVERIFY(QMetaObject::invokeMethod(editor,"openFor",Q_ARG(QString,QString("table")),Q_ARG(QVariant,item),
            Q_ARG(QString,QString()),Q_ARG(QVariant,QVariant())));
        QVERIFY(QMetaObject::invokeMethod(editor,"selectCell",Q_ARG(int,2),Q_ARG(int,1),Q_ARG(bool,false)));
        auto *input=editor->findChild<QQuickItem *>("tableCellInput");
        QVERIFY(input);
        input->setProperty("text","42");
        QVERIFY(QMetaObject::invokeMethod(editor,"requestDimensions",Q_ARG(int,2),Q_ARG(int,2)));
        QCOMPARE(editor->property("rowCount").toInt(),3);
        QCOMPARE(editor->property("pendingRows").toInt(),2);
        QVERIFY(!editor->property("canSave").toBool());
        QCOMPARE(h.notebook.pageItems()[0],item);
        QVERIFY(QMetaObject::invokeMethod(editor,"confirmDimensions"));
        QCOMPARE(editor->property("rowCount").toInt(),2);
        QVERIFY(editor->property("canSave").toBool());
        input->setProperty("text","=MAX(Z1:Z2)");
        QTRY_COMPARE(editor->property("selectedValue").toString(),QString("#REF!"));
        QString explanation;
        QVERIFY(QMetaObject::invokeMethod(editor,"formulaErrorExplanation",Q_RETURN_ARG(QString,explanation),Q_ARG(QString,QString("#REF!"))));
        QVERIFY(!explanation.isEmpty());
        editor->setProperty("formulaHelpOpen",true);
        h.screenshot("table-formulas");
    }

    void tableCellMouseTapEditsChosenValueAndPreservesFormulas()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem({{"type", "table"}, {"rows", 2}, {"cols", 3},
            {"cells", QVariantList{"12", "=SUM(A1:A2)", "", "7", "fixed", ""}}}));
        auto *ink = h.find<InkItem>("inkPage");
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(ink && editor);
        QTest::qWait(80);
        const auto originalItems = h.notebook.pageItems();
        const QVariantMap table = originalItems.first().toMap();
        const auto clickCell = [&](int row, int column) {
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
                ink->mapToScene(tableCellPoint(table, row, column, ink)).toPoint());
        };
        clickCell(0, 1);
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(editor->property("selectedRow").toInt(), 0);
        QCOMPARE(editor->property("selectedColumn").toInt(), 1);
        auto *input = editor->findChild<QQuickItem *>("tableCellInput");
        QVERIFY(input);
        QCOMPARE(input->property("text").toString(), QString("=SUM(A1:A2)"));
        QVERIFY(h.notebook.strokes().isEmpty());
        input->setProperty("text", "999");
        QSignalSpy closed(editor, SIGNAL(closed()));
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QTRY_COMPARE(closed.count(), 1);
        QCOMPARE(h.notebook.pageItems(), originalItems); // Closing the draft must not overwrite the formula.
        clickCell(1, 0);
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(editor->property("selectedRow").toInt(), 1);
        QCOMPARE(editor->property("selectedColumn").toInt(), 0);
        QCOMPARE(input->property("text").toString(), QString("7"));
        QTRY_VERIFY(input->hasActiveFocus());
        input->setProperty("text", "9");
        h.screenshot("table-cell-direct-edit");
        QVERIFY(QMetaObject::invokeMethod(editor, "submit", Q_ARG(bool, false)));
        QTRY_COMPARE(closed.count(), 2);
        const auto updated = h.notebook.pageItems().first().toMap();
        QCOMPARE(updated.value("cells").toList(), QVariantList({"12", "=SUM(A1:A2)", "", "9", "fixed", ""}));
        QCOMPARE(h.notebook.tableCellValues(updated)[1].toString(), QString("21"));
        QVERIFY(h.notebook.strokes().isEmpty());
    }

    void tableCellTouchAndElementsTapSelectRotatedCell()
    {
        MainHarness h;
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem({{"type", "table"}, {"rows", 2}, {"cols", 3},
            {"cells", QVariantList{"12", "=SUM(A1:A2)", "", "7", "fixed", ""}}}));
        const auto selection = h.notebook.selectContent(outline(), "contained", {}, "replace");
        QVERIFY(h.notebook.transformSelection(selection, 0, .2, .65, 20));
        const QVariantMap table = h.notebook.pageItems().first().toMap();
        auto *ink = h.find<InkItem>("inkPage");
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(ink && editor);
        QTest::qWait(80);
        const QPointF local = tableCellPoint(table, 1, 2, ink);
        const auto hit = ink->tableCellAt(local);
        QCOMPARE(hit.value("id").toString(), table.value("id").toString());
        QCOMPARE(hit.value("row").toInt(), 1);
        QCOMPARE(hit.value("column").toInt(), 2);
        QVERIFY(!hit.value("occupied").toBool());
        auto *device = QTest::createTouchDevice();
        const QPoint finger = ink->mapToScene(local).toPoint();
        QTest::touchEvent(h.window(), device).press(0, finger, h.window());
        QTest::touchEvent(h.window(), device).release(0, finger, h.window());
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(editor->property("selectedRow").toInt(), 1);
        QCOMPARE(editor->property("selectedColumn").toInt(), 2);
        QVERIFY(editor->findChild<QQuickItem *>("tableCellInput")->property("text").toString().isEmpty());
        QVERIFY(h.notebook.strokes().isEmpty());
        QSignalSpy closed(editor, SIGNAL(closed()));
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QTRY_COMPARE(closed.count(), 1);

        QVERIFY(h.window()->setProperty("editingItems", true));
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
            ink->mapToScene(tableCellPoint(table, 1, 1, ink)).toPoint());
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(editor->property("selectedRow").toInt(), 1);
        QCOMPARE(editor->property("selectedColumn").toInt(), 1);
        QCOMPARE(editor->findChild<QQuickItem *>("tableCellInput")->property("text").toString(), QString("fixed"));
        QVERIFY(h.notebook.strokes().isEmpty());
    }

    void tableCellPenTapPreservesDotsDragsAndLasso()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem({{"type", "table"}, {"rows", 2}, {"cols", 3},
            {"cells", QVariantList{"12", "=SUM(A1:A2)", "", "7", "fixed", ""}}}));
        const QVariantMap table = h.notebook.pageItems().first().toMap();
        auto *ink = h.find<InkItem>("inkPage");
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(ink && editor);
        QTest::qWait(80);
        QPointingDevice pen("UI table pen", 731004, QInputDevice::DeviceType::Stylus,
            QPointingDevice::PointerType::Pen,
            QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
        const QPointF empty = ink->mapToScene(tableCellPoint(table, 1, 2, ink));
        tabletEvent(h.window(), pen, QEvent::TabletPress, empty);
        tabletEvent(h.window(), pen, QEvent::TabletRelease, empty);
        QTRY_COMPARE(h.notebook.strokes().size(), 1);
        QVERIFY(!editor->property("visible").toBool());
        QCOMPARE(h.notebook.strokes().first().toMap().value("points").toList().size(), 1);

        const QPointF filled = ink->mapToScene(tableCellPoint(table, 0, 0, ink));
        tabletEvent(h.window(), pen, QEvent::TabletPress, filled);
        tabletEvent(h.window(), pen, QEvent::TabletRelease, filled);
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(editor->property("selectedRow").toInt(), 0);
        QCOMPARE(editor->property("selectedColumn").toInt(), 0);
        QCOMPARE(h.notebook.strokes().size(), 1);
        QSignalSpy closed(editor, SIGNAL(closed()));
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QTRY_COMPARE(closed.count(), 1);

        tabletEvent(h.window(), pen, QEvent::TabletPress, filled);
        tabletEvent(h.window(), pen, QEvent::TabletMove, filled + QPointF(24, 0));
        tabletEvent(h.window(), pen, QEvent::TabletRelease, filled + QPointF(40, 0));
        QTRY_COMPARE(h.notebook.strokes().size(), 2);
        QVERIFY(!editor->property("visible").toBool());
        QVERIFY(h.notebook.strokes().last().toMap().value("points").toList().size() >= 2);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setLasso", Q_ARG(bool, true)));
        tabletEvent(h.window(), pen, QEvent::TabletPress, filled);
        tabletEvent(h.window(), pen, QEvent::TabletRelease, filled);
        QVERIFY(!editor->property("visible").toBool());
        QCOMPARE(h.notebook.strokes().size(), 2);
    }

    void tableCellHitTestingRespectsOverlappingTextAndControls()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem({{"type", "table"}, {"rows", 2}, {"cols", 3},
            {"cells", QVariantList{"12", "=SUM(A1:A2)", "", "7", "fixed", ""}}}));
        const QVariantMap table = h.notebook.pageItems().first().toMap();
        auto *ink = h.find<InkItem>("inkPage");
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(ink && editor);
        QTest::qWait(80);
        const QPointF local = tableCellPoint(table, 0, 0, ink);
        QVERIFY(!ink->tableCellAt(local).isEmpty());
        QVERIFY(ink->tableCellAt({-1, -1}).isEmpty());
        {
            // ApplicationWindow has an inner content item, distinct from the
            // QQuickWindow root. Controls must belong to the actual ink overlay.
            auto *overlay = qvariant_cast<QQuickItem *>(ink->property("inputOverlay"));
            QVERIFY(overlay);
            QQuickItem blocker(overlay);
            blocker.setPosition(overlay->mapFromItem(ink, local) - QPointF(20, 20));
            blocker.setSize({40, 40});
            blocker.setProperty("blocksInk", true);
            QVERIFY(ink->tableCellAt(local).isEmpty());
        }
        QVERIFY(h.notebook.savePageItem({{"type", "text"}, {"text", "Foreground text"}}));
        const QString textId = h.notebook.pageItems().last().toMap().value("id").toString();
        QVERIFY(h.notebook.movePageItem(textId, table.value("x").toDouble(), table.value("y").toDouble()));
        QVERIFY(ink->tableCellAt(local).isEmpty());
        QCOMPARE(ink->textItemAt(local), textId);
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, ink->mapToScene(local).toPoint());
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(editor->property("itemType").toString(), QString("text"));
        QVERIFY(h.notebook.strokes().isEmpty());
    }

    void tableChartButtonCreatesLinkedChartAndTracksEditedFormulaInputs()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.notebook.savePageItem({{"type", "table"}, {"rows", 3}, {"cols", 3},
            {"cells", QVariantList{"X", "Input", "Computed", "1", "5", "=SUM(B2:B2)",
                                   "2", "7", "=SUM(B3:B3)"}}}));
        const QString tableId = h.notebook.pageItems().first().toMap().value("id").toString();
        auto *tableEditor = h.find<QObject>("pageItemDialog");
        auto *chartEditor = h.find<QObject>("tableChartDialog");
        QVERIFY(tableEditor && chartEditor);
        auto *input = tableEditor->findChild<QQuickItem *>("tableCellInput");
        QVERIFY(input);
        const auto findControl = [](auto &&self, QQuickItem *parent, const auto &matches) -> QQuickItem * {
            if (!parent) return nullptr;
            if (matches(parent)) return parent;
            for (QQuickItem *child : parent->childItems()) {
                if (auto *found = self(self, child, matches)) return found;
            }
            return nullptr;
        };
        QVERIFY(QMetaObject::invokeMethod(h.window(), "editTableCell", Q_ARG(QString, tableId), Q_ARG(int, 1), Q_ARG(int, 1)));
        QTRY_VERIFY(tableEditor->property("visible").toBool());
        input->setProperty("text", "6");
        auto *chartButton = findControl(findControl, qvariant_cast<QQuickItem *>(tableEditor->property("footer")),
            [](QQuickItem *item) { return item->property("iconName").toString() == "chart"; });
        QVERIFY(chartButton);
        QTRY_VERIFY(chartButton->isVisible() && chartButton->isEnabled());
        QTest::qWait(80);
        QSignalSpy tableClosed(tableEditor, SIGNAL(closed()));
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
            chartButton->mapToScene(QPointF(chartButton->width() / 2, chartButton->height() / 2)).toPoint());
        QTRY_VERIFY(chartEditor->property("visible").toBool());
        QTRY_COMPARE(tableClosed.count(), 1);
        QCOMPARE(h.notebook.pageItems().size(), 1);
        // Current design saves the table draft on Chart; Cancel only cancels chart creation.
        QCOMPARE(h.notebook.pageItems().first().toMap().value("cells").toList()[4].toString(), QStringLiteral("6"));
        auto *cancelButton = findControl(findControl, qvariant_cast<QQuickItem *>(chartEditor->property("footer")),
            [](QQuickItem *item) {
                return item->property("text").toString() == QCoreApplication::translate("TableChartDialog", "Annulla");
            });
        QVERIFY(cancelButton);
        QSignalSpy chartClosed(chartEditor, SIGNAL(closed()));
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
            cancelButton->mapToScene(QPointF(cancelButton->width() / 2, cancelButton->height() / 2)).toPoint());
        QTRY_COMPARE(chartClosed.count(), 1);
        QCOMPARE(h.notebook.pageItems().size(), 1);
        QCOMPARE(h.notebook.pageItems().first().toMap().value("cells").toList()[4].toString(), QStringLiteral("6"));

        QVERIFY(QMetaObject::invokeMethod(h.window(), "editTableCell", Q_ARG(QString, tableId), Q_ARG(int, 1), Q_ARG(int, 1)));
        QTRY_VERIFY(tableEditor->property("visible").toBool());
        QTest::qWait(80);
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
            chartButton->mapToScene(QPointF(chartButton->width() / 2, chartButton->height() / 2)).toPoint());
        QTRY_VERIFY(chartEditor->property("visible").toBool());
        QTRY_COMPARE(tableClosed.count(), 2);
        QList<QQuickItem *> columns;
        const auto collectColumns = [&](auto &&self, QQuickItem *item) -> void {
            if (!item) return;
            if (item->property("displayText").isValid() && item->property("model").toInt() == 3)
                columns.append(item);
            for (QQuickItem *child : item->childItems()) self(self, child);
        };
        collectColumns(collectColumns, qvariant_cast<QQuickItem *>(chartEditor->property("contentItem")));
        QCOMPARE(columns.size(), 2);
        QVERIFY(columns[0]->setProperty("currentIndex", 0));
        QVERIFY(columns[1]->setProperty("currentIndex", 2)); // Plot the formula column, not the default input column.
        const QVariantMap preview = qmlMap(chartEditor, "preview");
        QCOMPARE(preview.value("sourceTable").toString(), tableId);
        QCOMPARE(preview.value("xColumn").toInt(), 1);
        QCOMPARE(preview.value("yColumn").toInt(), 3);
        QVERIFY(preview.value("hasHeader").toBool());
        QCOMPARE(preview.value("xLabel").toString(), QStringLiteral("X"));
        QCOMPARE(preview.value("yLabel").toString(), QStringLiteral("Computed"));
        QCOMPARE(preview.value("yMax").toDouble(), 7.0);
        QCOMPARE(preview.value("points").toList().size(), 2);
        QVERIFY(qAbs(preview.value("points").toList()[0].toMap().value("y").toDouble() - 6.0 / 7) < 1e-9);
        auto *saveButton = findControl(findControl, qvariant_cast<QQuickItem *>(chartEditor->property("footer")),
            [](QQuickItem *item) { return item->property("filled").toBool(); });
        QVERIFY(saveButton);
        QTRY_VERIFY(saveButton->isVisible() && saveButton->isEnabled());
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
            saveButton->mapToScene(QPointF(saveButton->width() / 2, saveButton->height() / 2)).toPoint());
        QTRY_COMPARE(chartClosed.count(), 2);
        QCOMPARE(h.notebook.pageItems().size(), 2);
        const QVariantMap savedChart = h.notebook.pageItems().last().toMap();
        QCOMPARE(savedChart.value("sourceTable").toString(), tableId);
        QCOMPARE(savedChart.value("yColumn").toInt(), 3);
        QCOMPARE(savedChart.value("points"), preview.value("points"));

        QVERIFY(QMetaObject::invokeMethod(h.window(), "editTableCell", Q_ARG(QString, tableId), Q_ARG(int, 1), Q_ARG(int, 1)));
        QTRY_VERIFY(tableEditor->property("visible").toBool());
        input->setProperty("text", "9");
        QVERIFY(QMetaObject::invokeMethod(tableEditor, "submit", Q_ARG(bool, true)));
        QTRY_COMPARE(tableClosed.count(), 3);
        const QVariantList updatedCells = h.notebook.pageItems().first().toMap().value("cells").toList();
        QCOMPARE(updatedCells, QVariantList({"X", "Input", "Computed", "1", "9", "=SUM(B2:B2)",
                                            "2", "7", "=SUM(B3:B3)"}));
        const QVariantMap updatedChart = h.notebook.pageItems().last().toMap();
        QCOMPARE(updatedChart.value("id"), savedChart.value("id"));
        QCOMPARE(updatedChart.value("sourceTable").toString(), tableId);
        QCOMPARE(updatedChart.value("yColumn").toInt(), 3);
        QCOMPARE(updatedChart.value("yMax").toDouble(), 9.0);
        const QVariantList points = updatedChart.value("points").toList();
        QCOMPARE(points.size(), 2);
        QCOMPARE(points[0].toMap().value("y").toDouble(), 1.0);
        QVERIFY(qAbs(points[1].toMap().value("y").toDouble() - 7.0 / 9) < 1e-9);
        QVERIFY(h.notebook.strokes().isEmpty());
    }

    void chartDirectTapOpensMatchingEditorWithoutChangingData_data()
    {
        QTest::addColumn<bool>("linked");
        QTest::addColumn<QString>("input");
        QTest::newRow("standalone-mouse") << false << QStringLiteral("mouse");
        QTest::newRow("standalone-touch") << false << QStringLiteral("touch");
        QTest::newRow("standalone-pen") << false << QStringLiteral("pen");
        QTest::newRow("linked-mouse") << true << QStringLiteral("mouse");
        QTest::newRow("linked-touch") << true << QStringLiteral("touch");
        QTest::newRow("linked-pen") << true << QStringLiteral("pen");
    }

    void chartDirectTapOpensMatchingEditorWithoutChangingData()
    {
        QFETCH(bool, linked);
        QFETCH(QString, input);
        MainHarness h(input != "mouse");
        QVERIFY(h.window());
        const QVariantMap chart = h.addChart(linked);
        QVERIFY(!chart.isEmpty());
        QVERIFY(h.addStroke());
        const QVariantList originalItems = h.notebook.pageItems();
        const QVariantList originalInk = h.notebook.strokes();
        auto *ink = h.find<InkItem>("inkPage");
        auto *editor = h.find<QObject>(linked ? "tableChartDialog" : "chartEditor");
        auto *otherEditor = h.find<QObject>(linked ? "chartEditor" : "tableChartDialog");
        QVERIFY(ink && editor && otherEditor);
        QVERIFY(!h.window()->property("editingItems").toBool());
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        QTest::qWait(80);
        const QPointF local = chartPoint(chart, .5, .5, ink);
        const QPointF point = ink->mapToScene(local);
        QCOMPARE(ink->chartItemAt(local), chart.value("id").toString());
        if (input == "touch") {
            auto *device = QTest::createTouchDevice();
            QTest::touchEvent(h.window(), device).press(0, point.toPoint(), h.window());
            QTest::touchEvent(h.window(), device).release(0, point.toPoint(), h.window());
        } else if (input == "pen") {
            QPointingDevice pen("UI direct chart pen", 731005, QInputDevice::DeviceType::Stylus,
                QPointingDevice::PointerType::Pen,
                QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
            tabletEvent(h.window(), pen, QEvent::TabletPress, point);
            tabletEvent(h.window(), pen, QEvent::TabletRelease, point);
        } else {
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, point.toPoint());
        }
        QTRY_VERIFY(editor->property("visible").toBool());
        QVERIFY(!otherEditor->property("visible").toBool());
        QVERIFY(!h.find<QObject>("pageItemDialog")->property("visible").toBool());
        QCOMPARE(h.window()->property("selectedPageItem").toString(), chart.value("id").toString());
        QCOMPARE(qmlMap(editor, linked ? "original" : "originalItem").value("id"), chart.value("id"));
        if (linked) {
            QCOMPARE(qmlMap(editor, "source").value("cells"), originalItems.first().toMap().value("cells"));
        } else {
            QCOMPARE(qmlList(editor, "points"), chart.value("points").toList());
            QVERIFY(editor->setProperty("chartKind", "bars")); // Cancel an actual chart draft change.
        }
        QCOMPARE(h.notebook.pageItems(), originalItems);
        QCOMPARE(h.notebook.strokes(), originalInk);
        QSignalSpy closed(editor, SIGNAL(closed()));
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QTRY_COMPARE(closed.count(), 1);
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        QCOMPARE(h.notebook.pageItems(), originalItems);
        QCOMPARE(h.notebook.strokes(), originalInk);
        if (!linked && input == "mouse") {
            QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, point.toPoint());
            QTRY_VERIFY(editor->property("visible").toBool());
            QVERIFY(editor->setProperty("chartKind", "bars"));
            const QVariant editedChart = qmlMap(editor, "chart");
            QVERIFY(QMetaObject::invokeMethod(editor, "saveRequested", Q_ARG(QVariant, editedChart)));
            QTRY_COMPARE(closed.count(), 2);
            const QVariantMap saved = h.notebook.pageItems().first().toMap();
            QCOMPARE(saved.value("id"), chart.value("id"));
            QCOMPARE(saved.value("chartKind").toString(), QStringLiteral("bars"));
            QCOMPARE(saved.value("points"), chart.value("points"));
            QCOMPARE(h.notebook.strokes(), originalInk);
        }
    }

    void chartPenAnnotationsAndNonEditingToolsDoNotOpenEditors()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        const QVariantMap chart = h.addChart(false);
        QVERIFY(!chart.isEmpty());
        const QVariantList originalItems = h.notebook.pageItems();
        auto *ink = h.find<InkItem>("inkPage");
        QVERIFY(ink);
        QTest::qWait(80);
        const QPointF point = ink->mapToScene(chartPoint(chart, .5, .5, ink));
        QPointingDevice pen("UI chart annotations pen", 731006, QInputDevice::DeviceType::Stylus,
            QPointingDevice::PointerType::Pen,
            QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
        tabletEvent(h.window(), pen, QEvent::TabletPress, point);
        tabletEvent(h.window(), pen, QEvent::TabletMove, point + QPointF(24, 8));
        tabletEvent(h.window(), pen, QEvent::TabletRelease, point + QPointF(40, 12));
        QTRY_COMPARE(h.notebook.strokes().size(), 1);
        QVERIFY(h.notebook.strokes().first().toMap().value("points").toList().size() >= 2);
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());

        tabletEvent(h.window(), pen, QEvent::TabletPress, point);
        QTest::qWait(QGuiApplication::styleHints()->mousePressAndHoldInterval() + 30);
        tabletEvent(h.window(), pen, QEvent::TabletRelease, point);
        QTRY_COMPARE(h.notebook.strokes().size(), 2);
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());

        QVERIFY(QMetaObject::invokeMethod(h.window(), "selectTool", Q_ARG(QString, QStringLiteral("highlighter"))));
        tabletEvent(h.window(), pen, QEvent::TabletPress, point);
        tabletEvent(h.window(), pen, QEvent::TabletRelease, point);
        QTRY_COMPARE(h.notebook.strokes().size(), 3);
        QCOMPARE(h.notebook.strokes().last().toMap().value("tool").toString(), QStringLiteral("highlighter"));
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        const QVariantList annotatedInk = h.notebook.strokes();
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setLasso", Q_ARG(bool, true)));
        tabletEvent(h.window(), pen, QEvent::TabletPress, point);
        tabletEvent(h.window(), pen, QEvent::TabletRelease, point);
        QVERIFY(!h.window()->property("editorPopupOpen").toBool());
        QCOMPARE(h.notebook.strokes(), annotatedInk);
        QCOMPARE(h.notebook.pageItems(), originalItems);
    }

    void chartHitTestingRespectsRotationForegroundTextAndControls()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(!h.addChart(false).isEmpty());
        const auto selection = h.notebook.selectContent(outline(), "contained", {}, "replace");
        QVERIFY(h.notebook.transformSelection(selection, 0, .12, .7, 30));
        const QVariantMap chart = h.notebook.pageItems().first().toMap();
        auto *ink = h.find<InkItem>("inkPage");
        QVERIFY(ink);
        QTest::qWait(80);
        const QPointF local = chartPoint(chart, .08, .12, ink);
        QCOMPARE(ink->chartItemAt(local), chart.value("id").toString());
        QVERIFY(ink->chartItemAt({-1, -1}).isEmpty());
        {
            auto *overlay = qvariant_cast<QQuickItem *>(ink->property("inputOverlay"));
            QVERIFY(overlay);
            QQuickItem blocker(overlay);
            blocker.setPosition(overlay->mapFromItem(ink, local) - QPointF(20, 20));
            blocker.setSize({40, 40});
            blocker.setProperty("blocksInk", true);
            QVERIFY(ink->chartItemAt(local).isEmpty());
        }
        QVERIFY(h.notebook.savePageItem({{"type", "text"}, {"text", "Text above the chart"}}));
        const QString textId = h.notebook.pageItems().last().toMap().value("id").toString();
        QVERIFY(h.notebook.movePageItem(textId, local.x() / ink->width() - .03, local.y() / ink->height() - .03));
        QVERIFY(ink->chartItemAt(local).isEmpty());
        QCOMPARE(ink->textItemAt(local), textId);
        const auto originalItems = h.notebook.pageItems();
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, ink->mapToScene(local).toPoint());
        auto *editor = h.find<QObject>("pageItemDialog");
        QVERIFY(editor);
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(editor->property("itemType").toString(), QStringLiteral("text"));
        QCOMPARE(h.window()->property("selectedPageItem").toString(), textId);
        QVERIFY(!h.find<QObject>("chartEditor")->property("visible").toBool());
        QVERIFY(h.notebook.strokes().isEmpty());
        QCOMPARE(h.notebook.pageItems(), originalItems);
    }

    void lassoOffersChartConversionInChartsWorkspace()
    {
        MainHarness h;
        QVERIFY(h.window());
        h.window()->resize(720, 760);
        QVERIFY(QTest::qWaitForWindowExposed(h.window()));
        const auto addInk = [&](std::initializer_list<QPointF> samples) {
            QVariantList points;
            for (const auto &point : samples)
                points.append(QVariantMap{{"x", point.x()}, {"y", point.y()}, {"p", .6}});
            return h.notebook.appendStroke({{"tool", "pen"}, {"color", "#222222"}, {"width", 2.5}, {"points", points}});
        };
        QVERIFY(addInk({{.2, .2}, {.2, .4}, {.2, .6}, {.2, .8}, {.4, .8}, {.6, .8}, {.8, .8}}));
        QVERIFY(addInk({{.25, .72}, {.35, .66}, {.46, .57}, {.57, .6}, {.68, .48}, {.76, .38}}));
        const auto originalInk = h.notebook.strokes();
        QVERIFY(QMetaObject::invokeMethod(h.window(), "selectEditorWorkspace", Q_ARG(QString, QStringLiteral("charts"))));
        QTRY_COMPARE(h.window()->property("penTool").toString(), QStringLiteral("select"));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "useLasso", Q_ARG(QVariant, QVariant(outline()))));
        auto *convert = h.find<QQuickItem>("selectionRecognizeChart");
        auto *editor = h.find<QObject>("chartEditor");
        QVERIFY(convert && editor);
        QTRY_VERIFY(convert->isVisible() && convert->isEnabled() && convert->width() > 40);
        QTest::qWait(80);
        h.screenshot("selection-chart-action");
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
            convert->mapToScene(QPointF(convert->width() / 2, convert->height() / 2)).toPoint());
        QTRY_VERIFY(editor->property("opened").toBool());
        QVERIFY2(editor->property("recognitionError").toString().isEmpty(), qPrintable(editor->property("recognitionError").toString()));
        auto chart = qmlMap(editor, "chart");
        QCOMPARE(chart.value("chartKind").toString(), QStringLiteral("line"));
        QVERIFY(chart.value("points").toList().size() >= 2);
        QVERIFY(QMetaObject::invokeMethod(editor, "saveRequested", Q_ARG(QVariant, QVariant(chart))));
        QTRY_COMPARE(h.notebook.pageItems().size(), 1);
        QCOMPARE(h.notebook.strokes(), originalInk);
    }

    void tableHandwritingReviewsCellsAndUndoRestoresInk()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(!h.addHandwrittenTable().isEmpty());
        const auto originalInk = h.notebook.strokes();
        const auto originalItems = h.notebook.pageItems();
        QVERIFY(QMetaObject::invokeMethod(h.window(), "useLasso", Q_ARG(QVariant, QVariant(outline()))));
        auto *button = h.find<QQuickItem>("selectionInsertIntoCells");
        auto *dialog = h.find<QObject>("tableHandwritingDialog");
        QVERIFY(button && dialog);
        QTRY_VERIFY(button->isVisible());
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "stopRecognition"));
        QCOMPARE(qmlList(dialog, "cellEntries").size(), 2);
        QCOMPARE(qmlList(dialog, "cellEntries")[0].toMap().value("address").toString(), QString("A1"));
        QCOMPARE(qmlList(dialog, "cellEntries")[1].toMap().value("address").toString(), QString("A2"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "setCellText", Q_ARG(int, 0), Q_ARG(QString, QString("12"))));
        QVERIFY(QMetaObject::invokeMethod(dialog, "setCellText", Q_ARG(int, 1), Q_ARG(QString, QString("-5"))));
        QCOMPARE(h.notebook.pageItems(), originalItems); // Review must not save partial values.
        QCOMPARE(h.notebook.strokes(), originalInk);
        h.screenshot("table-handwriting-review");
        QVERIFY(h.window()->setProperty("tabletMode", true));
        h.window()->setVisibility(QWindow::Windowed);
        h.window()->resize(1620, 2160);
        auto *keyboard = dialog->findChild<QQuickItem *>("tableHandwritingKeyboard");
        QVERIFY(keyboard);
        QTRY_VERIFY(keyboard->isVisible());
        h.screenshot("table-handwriting-tablet");
        QVERIFY(h.window()->setProperty("externalKeyboard", QVariantMap{{"connected", true}}));
        QTRY_VERIFY(!keyboard->isVisible());
        QVERIFY(QMetaObject::invokeMethod(dialog, "submit"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(!h.find<QQuickItem>("selectionOverlay")->property("active").toBool());
        const auto cells = h.notebook.pageItems().first().toMap().value("cells").toList();
        QCOMPARE(cells[0].toString(), QString("12"));
        QCOMPARE(cells[3].toString(), QString("-5"));
        QCOMPARE(cells[1].toString(), QString("=SUM(A1:A2)"));
        QCOMPARE(h.notebook.tableCellValues(h.notebook.pageItems().first().toMap())[1].toString(), QString("7"));
        QVERIFY(h.notebook.strokes().isEmpty());
        QVERIFY(h.notebook.undo());
        QCOMPARE(h.notebook.pageItems(), originalItems);
        QCOMPARE(h.notebook.strokes(), originalInk);
    }

    void tableHandwritingRequiresOverwriteChoiceAndCanKeepInk()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(!h.addHandwrittenTable(true).isEmpty());
        const auto originalInk = h.notebook.strokes();
        const auto originalItems = h.notebook.pageItems();
        auto *dialog = h.find<QObject>("tableHandwritingDialog");
        QVERIFY(dialog);
        const auto selection = h.notebook.selectContent(outline(), "contained", {}, "replace");
        QVERIFY(QMetaObject::invokeMethod(dialog, "openFor", Q_ARG(QVariant, QVariant(selection))));
        QVERIFY(QMetaObject::invokeMethod(dialog, "stopRecognition"));
        QCOMPARE(qmlList(dialog, "cellEntries").size(), 3);
        for (int i = 0; i < 3; ++i) {
            QVERIFY(QMetaObject::invokeMethod(dialog, "setCellIncluded", Q_ARG(int, i), Q_ARG(bool, i == 1)));
        }
        QCOMPARE(qmlList(dialog, "cellEntries")[1].toMap().value("address").toString(), QString("B1"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "setCellText", Q_ARG(int, 1), Q_ARG(QString, QString("25"))));
        QVERIFY(QMetaObject::invokeMethod(dialog, "submit"));
        QVERIFY(dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems(), originalItems);
        QCOMPARE(h.notebook.strokes(), originalInk);
        QVERIFY(dialog->setProperty("overwriteExisting", true));
        QVERIFY(dialog->setProperty("removeInk", false));
        QVERIFY(QMetaObject::invokeMethod(dialog, "submit"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(h.notebook.pageItems().first().toMap().value("cells").toList()[1].toString(), QString("25"));
        QCOMPARE(h.notebook.strokes(), originalInk);
        QVERIFY(h.notebook.undo());
        QCOMPARE(h.notebook.pageItems(), originalItems);
    }

    void tableHandwritingSerialRecognitionAndCancellation()
    {
        TestHandwritingHelper fixture;
        QVERIFY(fixture.ready);
        Handwriting availability;
        if (!availability.available()) QSKIP("Platform does not support the installed recognition helper.");
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(!h.addHandwrittenTable().isEmpty());
        const auto originalInk = h.notebook.strokes();
        const auto originalItems = h.notebook.pageItems();
        const auto selection = h.notebook.selectContent(outline(), "contained", {}, "replace");
        auto *dialog = h.find<QObject>("tableHandwritingDialog");
        QVERIFY(dialog);
        QVERIFY(QMetaObject::invokeMethod(dialog, "openFor", Q_ARG(QVariant, QVariant(selection))));
        QTRY_COMPARE(qmlList(dialog, "cellEntries")[0].toMap().value("text").toString(), QString("12"));
        QTRY_COMPARE(qmlList(dialog, "cellEntries")[1].toMap().value("text").toString(), QString("12"));
        QTRY_VERIFY(!dialog->property("processing").toBool());
        QCOMPARE(h.notebook.pageItems(), originalItems);
        QVERIFY(QMetaObject::invokeMethod(dialog, "retryRecognition", Q_ARG(int, 0)));
        QVERIFY(QMetaObject::invokeMethod(dialog, "clearPrivateState"));
        QTest::qWait(220); // A late subprocess result must not restore the cleared draft.
        QVERIFY(!dialog->property("visible").toBool());
        QVERIFY(qmlList(dialog, "cellEntries").isEmpty());
        QCOMPARE(h.notebook.pageItems(), originalItems);
        QCOMPARE(h.notebook.strokes(), originalInk);
    }

    void handwritingProseContextStaysOutOfCellsAndLabels()
    {
        TestHandwritingHelper fixture(false, true);
        QVERIFY(fixture.ready);
        MainHarness h;
        QVERIFY(h.window());
        auto *recognizer = qvariant_cast<Handwriting *>(h.window()->property("handwritingEngine"));
        QVERIFY(recognizer);
        if (!recognizer->available()) QSKIP("Platform does not support the installed recognition helper.");
        QVERIFY(h.addStroke());

        QSignalSpy completed(recognizer, &Handwriting::finished);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "useLasso", Q_ARG(QVariant, QVariant(outline()))));
        QTRY_COMPARE(completed.size(), 1);
        QCOMPARE(recognizer->result(), QStringLiteral("CIAO"));
        QCOMPARE(recognizer->alternatives(), (QStringList{QStringLiteral("CIAO"), QStringLiteral("CIA0")}));

        recognizer->recognize(h.notebook.strokes(), QStringLiteral("it"));
        QTRY_COMPARE(completed.size(), 2);
        QCOMPARE(recognizer->result(), QStringLiteral("12"));
        recognizer->recognizeDiagramLabel(h.notebook.strokes(), QStringLiteral("it"), QStringLiteral("auto"));
        QTRY_COMPARE(completed.size(), 3);
        QCOMPARE(recognizer->result(), QStringLiteral("12"));
    }

    void diagramLabelRecognitionEnlargesAndTurnsSmallVerticalInk_data()
    {
        QTest::addColumn<QString>("orientation");
        QTest::addColumn<bool>("downward");
        QTest::newRow("automatic-down") << QStringLiteral("auto") << true;
        QTest::newRow("automatic-up") << QStringLiteral("auto") << false;
        QTest::newRow("manual-clockwise") << QStringLiteral("clockwise") << false;
        QTest::newRow("manual-counterclockwise") << QStringLiteral("counterclockwise") << true;
        QTest::newRow("stacked-letters") << QStringLiteral("stacked") << true;
    }

    void diagramLabelRecognitionEnlargesAndTurnsSmallVerticalInk()
    {
        QFETCH(QString, orientation);
        QFETCH(bool, downward);
        TestHandwritingHelper fixture(true);
        QVERIFY(fixture.ready);
        Handwriting recognizer;
        if (!recognizer.available()) QSKIP("Platform does not support the installed recognition helper.");

        const auto stroke = [](qreal y) {
            return QVariantMap{{"tool", "pen"}, {"points", QVariantList{
                QVariantMap{{"x", .29}, {"y", y}, {"p", .7}},
                QVariantMap{{"x", .294}, {"y", y + .004}, {"p", .6}}}}};
        };
        const QVariantList strokes{stroke(downward ? .2 : .23), stroke(downward ? .23 : .2)};
        QSignalSpy completed(&recognizer, &Handwriting::finished);
        recognizer.recognizeDiagramLabel(strokes, QStringLiteral("it"), orientation);
        QTRY_COMPARE(completed.size(), 1);
        QVERIFY(completed.first().first().toBool());
        QFile captured(fixture.capturePath);
        QVERIFY(captured.open(QIODevice::ReadOnly));
        const auto ink = QJsonDocument::fromJson(captured.readAll()).array();
        QCOMPARE(ink.size(), 2);
        const auto first = ink.first().toArray().first().toArray();
        const auto last = ink.last().toArray().first().toArray();
        const auto firstEnd = ink.first().toArray().last().toArray();
        QVERIFY(last.at(0).toDouble() - first.at(0).toDouble() > (orientation == QStringLiteral("stacked") ? .02 : .15));
        QVERIFY(qAbs(last.at(1).toDouble() - first.at(1).toDouble()) < 1e-6);
        QVERIFY(qAbs(firstEnd.at(1).toDouble() - first.at(1).toDouble()) > .015);
        QCOMPARE(first.at(2).toDouble(), .7);
    }

    void chartEditorExposesSignedAxisMinima()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        auto *editor=h.find<QObject>("chartEditor");
        QVERIFY(editor);
        const QVariant item=QVariantMap{{"id","chart"},{"chartKind","line"},{"x",.1},{"y",.1},{"width",.8},{"height",.4},
            {"xMax",10.0},{"yMax",20.0},{"minimum",QVariantMap{{"x",-5.0},{"y",-10.0}}},
            {"points",QVariantList{QVariantMap{{"x",0.0},{"y",0.0}},QVariantMap{{"x",1.0},{"y",1.0}}}}};
        QVERIFY(QMetaObject::invokeMethod(editor,"openItem",Q_ARG(QVariant,item)));
        auto *low=editor->findChild<QObject *>("chartMinimumY");
        QVERIFY(low);
        QCOMPARE(low->property("text").toString(),QString("-10"));
        low->setProperty("text","-25");
        QVERIFY(editor->property("validScale").toBool());
        const auto chart=editor->property("chart").toMap();
        QCOMPARE(chart.value("minimum").toMap().value("y").toDouble(),-25.0);
        ChartRecognizer csv;
        QVERIFY(csv.csv(chart).contains("-5,-25"));
        low->setProperty("text","3");
        QVERIFY(!editor->property("validScale").toBool());
    }

    void fullPageUsesEntireScreenAndReturnsTools()
    {
        MainHarness h;
        QVERIFY(h.window());
        QTest::qWait(80);
        auto *viewport = h.find<QQuickItem>("paperViewport");
        auto *ink = h.find<InkItem>("inkPage");
        QVERIFY(viewport && ink);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));
        QTRY_VERIFY(qAbs(viewport->height() - h.window()->height()) < 1);
        QCOMPARE(qRound(viewport->width()), h.window()->width());
        QCOMPARE(viewport->mapToScene(QPointF()), QPointF());
        h.screenshot("fullpage");
        auto *button = h.find<QQuickItem>("collapsedTools");
        QVERIFY(button && button->isVisible());
        const QPointF buttonCenter = button->mapToScene(QPointF(button->width()/2, button->height()/2));
        QVERIFY(ink->textItemAt(ink->mapFromScene(buttonCenter)).isEmpty());
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, buttonCenter.toPoint());
        QTRY_VERIFY(!h.window()->property("fullPage").toBool());
        // Visibility changes schedule a Qt Quick layout pass; assert the resulting geometry after it runs.
        QTRY_VERIFY(viewport->height() < h.window()->height());
    }

    void sleepDisablesInkWithoutPinAndKeepsDocument()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QTest::qWait(80);
        QVERIFY(!h.pin.configured());
        QVERIFY(h.addStroke());
        const QString bookId = h.notebook.bookId();
        const QString pageId = h.notebook.pageId();
        const auto inkBefore = h.notebook.strokes();
        auto *ink = h.find<InkItem>("inkPage");
        QVERIFY(ink && ink->isEnabled());
        h.standby.suspend();
        QTRY_VERIFY(h.standby.sleeping());
        QVERIFY(!h.pin.locked());
        QVERIFY(!ink->isEnabled());
        auto *overlay = h.find<QQuickItem>("standbyOverlay");
        QVERIFY(overlay && overlay->isVisible());
        h.screenshot("standby");
        // The wake tap must never become an ink stroke.
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, QPoint(70, 90));
        QTRY_VERIFY(!h.standby.sleeping());
        QCOMPARE(h.notebook.strokes(), inkBefore);
        QCOMPARE(h.notebook.bookId(), bookId);
        QCOMPARE(h.notebook.pageId(), pageId);
        QVERIFY(h.window()->property("editing").toBool());
        QVERIFY(ink->isEnabled());
    }

    void selectionDragChangesOnlyAfterApply()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.addStroke());
        const auto before = h.notebook.strokes();
        QTest::qWait(80);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "useLasso", Q_ARG(QVariant, QVariant(outline()))));
        auto *selection = h.find<QQuickItem>("selectionOverlay");
        QVERIFY(selection);
        QTRY_VERIFY(selection->property("active").toBool());
        QVERIFY(!h.find<InkItem>("inkPage")->isEnabled());
        auto *frame = selection->findChild<QQuickItem *>("selectedFrame");
        QVERIFY(frame && frame->isVisible());
        const QPoint start = frame->mapToScene(QPointF(frame->width()/2, frame->height()/2)).toPoint();
        QTest::mousePress(h.window(), Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(h.window(), start + QPoint(40, 35), 20);
        QTest::mouseRelease(h.window(), Qt::LeftButton, Qt::NoModifier, start + QPoint(40, 35));
        QTRY_VERIFY(selection->property("changed").toBool());
        QCOMPARE(h.notebook.strokes(), before);
        h.screenshot("lasso-preview");
        QVERIFY(QMetaObject::invokeMethod(selection, "apply"));
        QTRY_VERIFY(!selection->property("active").toBool());
        QVERIFY(h.notebook.strokes() != before);
        QVERIFY(h.notebook.undo());
        QCOMPARE(h.notebook.strokes(), before);
    }

    void layerChangesClearSelectionWithoutChangingInk()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(h.addStroke());
        const QString originalLayer = h.notebook.activeLayer();
        const QString pageId = h.notebook.pageId();
        const auto initialInk = h.notebook.strokes();
        QTest::qWait(80);
        auto *selection = h.find<QQuickItem>("selectionOverlay");
        auto *ink = h.find<InkItem>("inkPage");
        QVERIFY(selection && ink);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "useLasso", Q_ARG(QVariant, QVariant(outline()))));
        QTRY_VERIFY(selection->property("active").toBool());
        QVERIFY(h.notebook.addLayer("Second layer"));
        QTRY_VERIFY(!selection->property("active").toBool());
        QVERIFY(ink->isEnabled());
        QCOMPARE(h.notebook.strokes(), initialInk);
        QCOMPARE(h.notebook.pageId(), pageId);

        QVERIFY(h.addStroke());
        const auto bothLayers = h.notebook.strokes();
        QVERIFY(QMetaObject::invokeMethod(h.window(), "useLasso", Q_ARG(QVariant, QVariant(outline()))));
        QTRY_VERIFY(selection->property("active").toBool());
        QVERIFY(h.notebook.setActiveLayer(originalLayer));
        QTRY_VERIFY(!selection->property("active").toBool());
        QVERIFY(ink->isEnabled());
        QCOMPARE(h.notebook.strokes(), bothLayers);
        QCOMPARE(h.notebook.pageId(), pageId);
    }

    void fullscreenToolsOpenFromTouchHandle_data()
    {
        QTest::addColumn<int>("angle");
        QTest::newRow("portrait") << 0;
        QTest::newRow("landscape") << 90;
    }

    void fullscreenToolsOpenFromTouchHandle()
    {
        QFETCH(int, angle);
        MainHarness h;
        QVERIFY(h.window());
        h.window()->resize(720, 760);
        QVERIFY(QTest::qWaitForWindowExposed(h.window()));
        QVERIFY(h.addBookBackground());
        QVERIFY(h.addStroke());
        const auto originalInk = h.notebook.strokes();
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences && preferences->setProperty("orientation", angle));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), angle);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));

        auto *handle = h.find<QQuickItem>("fullscreenToolsButton");
        auto *menu = h.find<QObject>("workspaceMoreMenu");
        QVERIFY(handle && menu);
        QTRY_VERIFY(handle->isVisible() && handle->isEnabled() && handle->width() > 40);
        QTest::qWait(100);
        auto *device = QTest::createTouchDevice();
        const auto point = [&](int offset) {
            return handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2 + offset)).toPoint();
        };
        const auto swipe = [&] {
            auto touch = QTest::touchEvent(h.window(), device, false);
            touch.press(0, point(0), h.window()).commit();
            for (int y : {-24, -55, -92, -136}) {
                touch.move(0, point(y), h.window()).commit();
                QTest::qWait(16);
            }
            touch.release(0, point(-136), h.window()).commit();
        };
        swipe();
        QTRY_VERIFY(menu->property("opened").toBool());
        QVERIFY(menu->property("fullPage").toBool());
        QVERIFY(h.window()->property("fullPage").toBool());
        QCOMPARE(h.notebook.strokes(), originalInk);
        QCOMPARE(h.notebook.page(), 0);

        QQuickItem *note = nullptr;
        QTRY_VERIFY((note = findVisualItem(h.window()->contentItem(), "workspaceToolsMenuAction_bookNote")));
        QTRY_VERIFY(note->isVisible() && note->isEnabled());
        QTest::qWait(100);
        h.screenshot(QStringLiteral("fullscreen-tools-%1").arg(angle));
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier,
            note->mapToScene(QPointF(note->width() / 2, note->height() / 2)).toPoint());
        QTRY_VERIFY(!menu->property("visible").toBool());
        QTRY_COMPARE(h.window()->property("bookAnchorKind").toString(), QStringLiteral("note"));
        QVERIFY(h.window()->setProperty("bookAnchorKind", QString()));

        QVERIFY(preferences->setProperty("gestureNavigationEnabled", false));
        swipe();
        QVERIFY(!menu->property("visible").toBool());
        QTest::mouseClick(h.window(), Qt::LeftButton, Qt::NoModifier, point(0));
        QTRY_VERIFY(menu->property("opened").toBool());
        QCOMPARE(h.notebook.strokes(), originalInk);
    }

    void fullscreenToolsPreserveFreehandInk_data()
    {
        QTest::addColumn<int>("angle");
        QTest::newRow("portrait") << 0;
        QTest::newRow("landscape") << 90;
    }

    void fullscreenToolsPreserveFreehandInk()
    {
        QFETCH(int, angle);
        MainHarness h;
        QVERIFY(h.window());
        h.window()->resize(720, 760);
        QVERIFY(QTest::qWaitForWindowExposed(h.window()));
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *ink = h.find<InkItem>("inkPage");
        auto *handle = h.find<QQuickItem>("fullscreenToolsButton");
        auto *menu = h.find<QObject>("workspaceMoreMenu");
        QVERIFY(preferences && ink && handle && menu);
        QVERIFY(preferences->setProperty("orientation", angle));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), angle);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));
        QTRY_VERIFY(handle->isVisible() && handle->isEnabled() && handle->width() > 40);
        QTest::qWait(80);

        QPointingDevice pen("Full-page pen", 731033, QInputDevice::DeviceType::Stylus, QPointingDevice::PointerType::Pen,
            QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
        const QPointF start = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2));
        const QPointF local = ink->mapFromScene(start);
        QVERIFY(ink->contains(local));
        const auto point = [&](qreal x, qreal y) { return ink->mapToScene(local + QPointF(x, y)); };
        tabletEvent(h.window(), pen, QEvent::TabletPress, start);
        QVERIFY(ink->drawing() && ink->touchBlocked());
        tabletEvent(h.window(), pen, QEvent::TabletMove, point(-9, -15));
        tabletEvent(h.window(), pen, QEvent::TabletMove, point(8, -30));
        tabletEvent(h.window(), pen, QEvent::TabletRelease, point(25, -43));
        QTRY_COMPARE(h.notebook.strokes().size(), 1);
        QCOMPARE(inkToolFromName(h.notebook.strokes().constFirst().toMap().value("tool").toString()), InkTool::Pen);
        const auto points = h.notebook.strokes().constFirst().toMap().value("points").toList();
        QCOMPARE(points.size(), 4);
        QVERIFY(qAbs(points.constFirst().toMap().value("x").toDouble() - local.x() / ink->width()) < 0.00001);
        QVERIFY(qAbs(points.constFirst().toMap().value("y").toDouble() - local.y() / ink->height()) < 0.00001);
        QVERIFY(!ink->drawing() && !ink->touchBlocked());
        QVERIFY(!menu->property("visible").toBool());
        QVERIFY(h.notebook.undo());
        QVERIFY(h.notebook.strokes().isEmpty());
        QVERIFY(h.notebook.redo());
        QCOMPARE(h.notebook.strokes().size(), 1);

        tabletEvent(h.window(), pen, QEvent::TabletPress, start);
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(h.window(), &leave);
        tabletEvent(h.window(), pen, QEvent::TabletRelease, start);
        QVERIFY(!ink->drawing() && !ink->touchBlocked());
        QVERIFY(!menu->property("visible").toBool());
        QCOMPARE(h.notebook.strokes().size(), 1);

        tabletEvent(h.window(), pen, QEvent::TabletPress, start);
        tabletEvent(h.window(), pen, QEvent::TabletRelease, start);
        QTRY_VERIFY(menu->property("opened").toBool());
        QCOMPARE(h.notebook.strokes().size(), 1);
    }

    void sustainedHandwritingSurvivesRotationAndReopening()
    {
        MainHarness h;
        QVERIFY(h.window());
        h.window()->resize(720, 760);
        QVERIFY(QTest::qWaitForWindowExposed(h.window()));
        auto *ink = h.find<InkItem>("inkPage");
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        auto *handle = h.find<QQuickItem>("fullscreenToolsButton");
        auto *menu = h.find<QObject>("workspaceMoreMenu");
        QVERIFY(ink && preferences && handle && menu);
        preferences->setPenPressureCurve("firm");
        QTRY_COMPARE(ink->pressureExponent(), 1.5);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));
        QTRY_VERIFY(ink->isEnabled() && handle->isVisible() && handle->width() > 40);

        QPointingDevice pen("Handwriting session pen", 731034, QInputDevice::DeviceType::Stylus,
            QPointingDevice::PointerType::Pen, QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
        const auto pressure = [](qsizetype index, qsizetype count) {
            const qreal progress = qreal(index) / qMax(qsizetype(1), count - 1);
            return .23 + .58 * (1 - qAbs(2 * progress - 1));
        };
        const auto traceStroke = [&](const QList<QPointF> &samples) {
            const auto strokeCount = h.notebook.strokes().size();
            const auto position = [&](qsizetype i) {
                return ink->mapToScene(QPointF(samples[i].x() * ink->width(), samples[i].y() * ink->height()));
            };
            tabletEvent(h.window(), pen, QEvent::TabletPress, position(0), pressure(0, samples.size()));
            QVERIFY(ink->drawing());
            for (qsizetype i = 1; i + 1 < samples.size(); ++i)
                tabletEvent(h.window(), pen, QEvent::TabletMove, position(i), pressure(i, samples.size()));
            tabletEvent(h.window(), pen, QEvent::TabletRelease, position(samples.size() - 1));
            QVERIFY(!ink->drawing());
            QCOMPARE(h.notebook.strokes().size(), strokeCount + 1);
            const auto stroke = h.notebook.strokes().last().toMap();
            QCOMPARE(inkToolFromName(stroke.value("tool").toString()), InkTool::Pen);
            const auto points = stroke.value("points").toList();
            QCOMPARE(points.size(), samples.size());
            for (qsizetype i = 0; i < points.size(); ++i) {
                const auto point = points[i].toMap();
                QVERIFY(qAbs(point.value("x").toDouble() - samples[i].x()) < 0.00001);
                QVERIFY(qAbs(point.value("y").toDouble() - samples[i].y()) < 0.00001);
                // Tablet release reports zero. The last contact sample supplies its pressure.
                const auto pressureIndex = qMin(i, qMax(qsizetype(0), points.size() - 2));
                QVERIFY(qAbs(point.value("p").toDouble() - std::pow(pressure(pressureIndex, points.size()), 1.5)) < 0.00001);
            }
        };
        const auto writeWord = [&](QPointF origin) {
            QPainterPath word(origin + QPointF(0, .03));
            word.cubicTo(origin + QPointF(.018, .025), origin + QPointF(.019, .002), origin + QPointF(.038, .004));
            word.cubicTo(origin + QPointF(.061, .008), origin + QPointF(.021, .044), origin + QPointF(.043, .045));
            word.cubicTo(origin + QPointF(.060, .047), origin + QPointF(.061, .011), origin + QPointF(.078, .015));
            word.cubicTo(origin + QPointF(.092, .019), origin + QPointF(.073, .048), origin + QPointF(.105, .038));
            QList<QPointF> samples;
            for (int point = 0; point <= 32; ++point)
                samples.append(word.pointAtPercent(point / 32.0));
            traceStroke(samples);
        };
        const QString bookId = h.notebook.bookId();
        const QString firstPage = h.notebook.pageId();
        for (const qreal y : {.19, .32, .48}) {
            for (const qreal x : {.18, .39, .62})
                writeWord(QPointF(x, y));
            traceStroke({QPointF(.74, y + .04)});
        }

        const auto acrossTools = [&](qreal x, qreal y) {
            const auto local = ink->mapFromScene(handle->mapToScene(QPointF(x, y)));
            return QPointF(local.x() / ink->width(), local.y() / ink->height());
        };
        const qreal half = handle->height() / 2;
        traceStroke({acrossTools(-12, half - 3), acrossTools(handle->width() / 2, half),
            acrossTools(handle->width() + 12, half - 4)});
        const auto firstPageInk = h.notebook.strokes();
        QCOMPARE(firstPageInk.size(), 13);
        QVERIFY(!menu->property("visible").toBool());
        QVERIFY(h.notebook.undo());
        QCOMPARE(h.notebook.strokes().size(), 12);
        QVERIFY(h.notebook.redo());
        QCOMPARE(h.notebook.strokes(), firstPageInk);

        QVERIFY(h.notebook.addPage());
        QVERIFY(h.notebook.strokes().isEmpty());
        const QString secondPage = h.notebook.pageId();
        QVERIFY(secondPage != firstPage);
        const qreal portraitWidth = ink->width();
        QVERIFY(preferences->setProperty("orientation", 90));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), 90);
        QTRY_VERIFY(ink->width() != portraitWidth);
        QVERIFY(h.window()->setProperty("canvasZoom", 1.4));
        QTRY_COMPARE(h.window()->property("pageZoom").toDouble(), 1.4);
        writeWord(QPointF(.30, .33));
        writeWord(QPointF(.46, .48));
        const auto secondPageInk = h.notebook.strokes();
        QCOMPARE(secondPageInk.size(), 2);
        QVERIFY(h.notebook.goToPage(0));
        QCOMPARE(h.notebook.pageId(), firstPage);
        QCOMPARE(h.notebook.strokes(), firstPageInk);

        Notebook reopened(h.directory.filePath("notebooks.json"));
        QCOMPARE(reopened.bookId(), bookId);
        QCOMPARE(reopened.pageId(), firstPage);
        QCOMPARE(reopened.strokes(), firstPageInk);
        QVERIFY(reopened.goToPage(1));
        QCOMPARE(reopened.pageId(), secondPage);
        QCOMPARE(reopened.strokes(), secondPageInk);
    }

    void pinchZoomAndSwipeChangePage()
    {
        MainHarness h;
        QVERIFY(h.window());
        QVERIFY2(h.addBookBackground(), qPrintable(h.notebook.error()));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));
        auto *gestures = h.find<QQuickItem>("readingGestures");
        QVERIFY(gestures);
        QTRY_VERIFY(gestures->isEnabled());
        QTest::qWait(100);
        auto *device = QTest::createTouchDevice();
        auto touch = QTest::touchEvent(h.window(), device, false);
        const QPoint center = gestures->mapToScene(QPointF(gestures->width()/2, gestures->height()/2)).toPoint();
        touch.press(0, center - QPoint(50,0), h.window()).press(1, center + QPoint(50,0), h.window()).commit();
        for (int distance = 60; distance <= 150; distance += 15) {
            touch.move(0, center - QPoint(distance,0), h.window()).move(1, center + QPoint(distance,0), h.window()).commit();
            QTest::qWait(15);
        }
        touch.release(0, center - QPoint(150,0), h.window()).release(1, center + QPoint(150,0), h.window()).commit();
        QTRY_VERIFY(h.window()->property("readingZoom").toDouble() > 1.1);
        QCOMPARE(h.notebook.page(), 0);
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setReadingZoom", Q_ARG(double, 1.0), Q_ARG(bool, false)));
        QTest::qWait(100);
        auto swipe = QTest::touchEvent(h.window(), device, false);
        swipe.press(0, center + QPoint(180,0), h.window()).commit();
        for (int distance = 130; distance >= -190; distance -= 40) {
            swipe.move(0, center + QPoint(distance,0), h.window()).commit();
            QTest::qWait(15);
        }
        swipe.release(0, center - QPoint(190,0), h.window()).commit();
        QTRY_COMPARE(h.notebook.page(), 1);
        QVERIFY(h.notebook.strokes().isEmpty());
    }

    void readerSwipeDoesNotTurnPageWhenSecondFingerJoins()
    {
        MainHarness h;
        QVERIFY(h.window());
        QVERIFY2(h.addBookBackground(), qPrintable(h.notebook.error()));
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));
        auto *gestures = h.find<QQuickItem>("readingGestures");
        QVERIFY(gestures && gestures->isEnabled());
        QTest::qWait(80);

        auto *device = QTest::createTouchDevice();
        auto touch = QTest::touchEvent(h.window(), device, false);
        const auto point = [&](int x) {
            return gestures->mapToScene(QPointF(gestures->width() / 2 + x, gestures->height() / 2)).toPoint();
        };
        touch.press(0, point(230), h.window()).commit();
        for (int x : {180, 130, 80, 30, -20}) {
            touch.move(0, point(x), h.window()).commit();
            QTest::qWait(15);
        }
        QTRY_VERIFY(gestures->property("active").toBool());
        QCOMPARE(h.notebook.page(), 0);
        touch.stationary(0).press(1, point(80), h.window()).commit();
        for (int distance : {20, 45, 70, 95}) {
            touch.move(0, point(-20 - distance), h.window()).move(1, point(80 + distance), h.window()).commit();
            QTest::qWait(15);
        }
        touch.release(0, point(-115), h.window()).release(1, point(175), h.window()).commit();
        QTRY_VERIFY(!gestures->property("active").toBool());
        QCOMPARE(h.notebook.page(), 0);
        QVERIFY(h.notebook.strokes().isEmpty());
    }

    void canvasPinchAndTwoFingerPanFollowPageAxes_data()
    {
        QTest::addColumn<int>("angle");
        QTest::newRow("portrait") << 0;
        QTest::newRow("landscape-right") << 90;
    }

    void canvasPinchAndTwoFingerPanFollowPageAxes()
    {
        QFETCH(int, angle);
        MainHarness h;
        QVERIFY(h.window());
        auto *preferences = h.find<TabletPreferences>("tabletPreferences");
        QVERIFY(preferences && preferences->setProperty("orientation", angle));
        QTRY_COMPARE(h.window()->property("appliedOrientation").toInt(), angle);
        QVERIFY(h.notebook.addPage());
        QVERIFY(h.notebook.goToPage(0));
        QVERIFY(h.addStroke());
        const auto originalInk = h.notebook.strokes();
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));
        auto *gestures = h.find<QQuickItem>("readingGestures");
        auto *viewport = h.find<QQuickItem>("paperViewport");
        QVERIFY(gestures && viewport && gestures->isEnabled());
        QTRY_VERIFY(viewport->isVisible() && viewport->width() > 380 && viewport->height() > 320);

        auto *device = QTest::createTouchDevice();
        const auto point = [&](int x, int y) {
            return gestures->mapToScene(QPointF(gestures->width() / 2 + x, gestures->height() / 2 + y)).toPoint();
        };
        auto swipe = QTest::touchEvent(h.window(), device, false);
        swipe.press(0, point(180, 0), h.window()).commit();
        for (int x : {130, 70, 10, -50, -110, -170}) swipe.move(0, point(x, 0), h.window()).commit();
        swipe.release(0, point(-170, 0), h.window()).commit();
        QCOMPARE(h.notebook.page(), 0);

        auto pinch = QTest::touchEvent(h.window(), device, false);
        pinch.press(0, point(-50, 0), h.window()).press(1, point(50, 0), h.window()).commit();
        for (int distance : {65, 90, 115, 140}) {
            pinch.move(0, point(-distance, 0), h.window()).move(1, point(distance, 0), h.window()).commit();
            QTest::qWait(15);
        }
        pinch.release(0, point(-140, 0), h.window()).release(1, point(140, 0), h.window()).commit();
        QTRY_VERIFY(!gestures->property("active").toBool() && h.window()->property("canvasZoom").toDouble() > 1.2);
        const qreal zoom = h.window()->property("canvasZoom").toDouble();

        auto pan = QTest::touchEvent(h.window(), device, false);
        pan.press(0, point(-60, 0), h.window()).press(1, point(60, 0), h.window()).commit();
        for (int y : {-25, -50})
            pan.move(0, point(-60, y), h.window()).move(1, point(60, y), h.window()).commit();
        QTRY_VERIFY(gestures->property("active").toBool());
        const qreal startY = viewport->property("contentY").toDouble();
        // The handler queues viewport changes. Observe a parallel movement after it has grabbed both
        // fingers and let that update reach the viewport before ending the recorded contact.
        for (int y : {-90, -125, -150})
            pan.move(0, point(-60, y), h.window()).move(1, point(60, y), h.window()).commit();
        QTRY_VERIFY(viewport->property("contentY").toDouble() > startY + 40);
        pan.release(0, point(-60, -150), h.window()).release(1, point(60, -150), h.window()).commit();
        QTRY_VERIFY(!gestures->property("active").toBool());
        QVERIFY(viewport->property("contentY").toDouble() > startY + 40);
        QVERIFY(qAbs(h.window()->property("canvasZoom").toDouble() - zoom) < .01);

        QVERIFY(QMetaObject::invokeMethod(h.window(), "setLasso", Q_ARG(bool, true)));
        QVERIFY(!gestures->isEnabled());
        QCOMPARE(h.notebook.page(), 0);
        QCOMPARE(h.notebook.strokes(), originalInk);
    }

#if defined(Q_OS_MACOS) && QT_CONFIG(gestures)
    void nativeTrackpadPinchZoomsCanvas()
    {
        MainHarness h(false);
        QVERIFY(h.window());
        QVERIFY(QMetaObject::invokeMethod(h.window(), "setFullPage", Q_ARG(bool, true)));
        auto *gestures = h.find<QQuickItem>("readingGestures");
        QVERIFY(gestures && gestures->isEnabled());
        QTest::qWait(80);
        const QPointF point = gestures->mapToScene(QPointF(gestures->width() / 2, gestures->height() / 2));
        const QPointingDevice pad("Synthetic trackpad", 731027, QInputDevice::DeviceType::TouchPad,
            QPointingDevice::PointerType::Finger, QInputDevice::Capability::Position, 2, 0);
        const auto send = [&](Qt::NativeGestureType type, qreal value) {
            QNativeGestureEvent event(type, &pad, 2, point, point, h.window()->mapToGlobal(point), value, {});
            QCoreApplication::sendEvent(h.window(), &event);
        };
        send(Qt::BeginNativeGesture, 0);
        send(Qt::ZoomNativeGesture, .25);
        send(Qt::ZoomNativeGesture, .25);
        send(Qt::EndNativeGesture, 0);
        QTRY_VERIFY(h.window()->property("canvasZoom").toDouble() > 1.2);
        QCOMPARE(h.notebook.page(), 0);
        QVERIFY(h.notebook.strokes().isEmpty());
    }
#endif
};

QTEST_MAIN(UiTests)
#include "ui_tests.moc"
