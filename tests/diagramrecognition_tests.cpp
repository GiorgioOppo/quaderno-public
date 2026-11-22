#include "diagramrecognizer.h"
#include "diagramexport.h"
#include "diagramitem.h"
#include "notebook.h"
#include "pagecontent.h"
#include "inkselection.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest/QTest>
#include <cmath>
#include <limits>

namespace {
QVariantMap drawnPath(const QList<QPointF> &vertices, int samplesPerSegment = 8)
{
    QVariantList points;
    if (vertices.isEmpty()) return {};
    for (qsizetype segment = 1; segment < vertices.size(); ++segment) {
        for (int sample = segment == 1 ? 0 : 1; sample <= samplesPerSegment; ++sample) {
            const qreal fraction = qreal(sample) / samplesPerSegment;
            const QPointF point = vertices[segment - 1] * (1 - fraction) + vertices[segment] * fraction;
            points.append(QVariantMap{{"x", point.x()}, {"y", point.y()}, {"p", .6}});
        }
    }
    return {{"tool", "pen"}, {"color", "#222222"}, {"width", 2.5}, {"points", points}};
}

QVariantMap drawnRectangle(const QRectF &rect)
{
    return drawnPath({rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft(), rect.topLeft()});
}

QVariantMap drawnDiamond(const QRectF &rect)
{
    return drawnPath({{rect.center().x(), rect.top()}, {rect.right(), rect.center().y()},
        {rect.center().x(), rect.bottom()}, {rect.left(), rect.center().y()}, {rect.center().x(), rect.top()}});
}

QVariantMap drawnEllipse(const QRectF &rect)
{
    QList<QPointF> points;
    for (int index = 0; index <= 64; ++index) {
        const qreal angle = index * 2 * 3.141592653589793 / 64;
        points.append(rect.center() + QPointF(rect.width() / 2 * std::cos(angle), rect.height() / 2 * std::sin(angle)));
    }
    return drawnPath(points, 1);
}

QVariantMap ovalArc(const QRectF &rect, qreal turns)
{
    QList<QPointF> points;
    const int steps = qRound(64 * turns);
    for (int index = 0; index <= steps; ++index) {
        const qreal angle = index * 2 * 3.141592653589793 / 64;
        points.append(rect.center() + QPointF(rect.width() / 2 * std::cos(angle), rect.height() / 2 * std::sin(angle)));
    }
    return drawnPath(points, 1);
}

QVariantMap curvedArrowHead(bool continuous, bool retraced)
{
    const QPointF tip(.62, .21), upper(.59, .198), lower(.59, .222);
    QList<QPointF> points;
    if (continuous) points = {{.32, .21}, {.40, .197}, {.49, .198}, {.56, .205}, tip};
    else points = {tip};
    points.append(upper);
    points.append(lower);
    points.append(tip);
    if (retraced) points.append(upper);
    return drawnPath(points, 5);
}

QVariantList splitRectangle(const QRectF &rect)
{
    return {drawnPath({rect.topLeft(), rect.topRight()}), drawnPath({rect.topRight(), rect.bottomRight()}),
        drawnPath({rect.bottomRight(), rect.bottomLeft()}), drawnPath({rect.bottomLeft(), rect.topLeft()})};
}

QVariantList arrowDiagram(bool reverseHead = false, bool separateWings = false)
{
    QVariantList strokes{drawnRectangle(QRectF(.10, .15, .22, .12)), drawnRectangle(QRectF(.62, .15, .22, .12)),
        drawnPath({{.32, .21}, {.62, .21}})};
    if (separateWings) {
        strokes.append(drawnPath({{.585, .195}, {.62, .21}}));
        strokes.append(drawnPath({{.585, .225}, {.62, .21}}));
    } else {
        strokes.append(drawnPath({{.585, .195}, {.62, .21}, {.585, .225}}));
    }
    if (reverseHead) strokes.append(drawnPath({{.355, .195}, {.32, .21}, {.355, .225}}));
    return strokes;
}

QVariantList fullPageSelection()
{
    return {QVariantMap{{"x", 0.0}, {"y", 0.0}}, QVariantMap{{"x", 1.0}, {"y", 0.0}},
        QVariantMap{{"x", 1.0}, {"y", 1.0}}, QVariantMap{{"x", 0.0}, {"y", 1.0}}};
}

QVariantList diagramExtension(bool second = false)
{
    if (second) return {drawnRectangle(QRectF(.10, .50, .22, .12)),
        drawnPath({{.62, .56}, {.32, .56}}), drawnPath({{.355, .545}, {.32, .56}, {.355, .575}})};
    return {drawnRectangle(QRectF(.62, .50, .22, .12)),
        drawnPath({{.73, .27}, {.73, .50}}), drawnPath({{.71, .472}, {.73, .50}, {.75, .472}})};
}

bool appendInk(Notebook &notebook, const QVariantList &strokes)
{
    for (const QVariant &stroke : strokes)
        if (!notebook.appendStroke(stroke.toMap())) return false;
    return true;
}

bool convertInk(Notebook &notebook, bool removeInk = true, const QString &target = {})
{
    const QVariantMap selection = notebook.selectInk(fullPageSelection());
    const QVariantMap plan = notebook.diagramHandwritingPlan(selection, target);
    return plan.value("error").toString().isEmpty()
        && notebook.applyDiagramHandwriting(selection, plan, plan.value("diagram").toMap(), {}, removeInk);
}

QSet<int> indexSet(const QVariantList &indexes)
{
    QSet<int> result;
    for (const QVariant &index : indexes) result.insert(index.toInt());
    return result;
}

QVariantList candidatesOfKind(const QVariantMap &result, const QString &kind)
{
    QVariantList filtered;
    for (const QVariant &candidate : result.value("candidates").toList())
        if (candidate.toMap().value("kind").toString() == kind) filtered.append(candidate);
    return filtered;
}

QVariantMap editableDiagram()
{
    return {{"type", "diagram"}, {"nodes", QVariantList{
        QVariantMap{{"id", "start"}, {"shape", "terminal"}, {"text", "Start"}, {"x", .08}, {"y", .1}, {"width", .25}, {"height", .18}},
        QVariantMap{{"id", "step"}, {"shape", "process"}, {"text", "Review"}, {"x", .64}, {"y", .1}, {"width", .28}, {"height", .18}}}},
        {"edges", QVariantList{QVariantMap{{"id", "flow"}, {"from", "start"}, {"to", "step"}, {"text", "Continue"}}}}};
}

QByteArray fileBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
}

class DiagramRecognitionTests final : public QObject
{
    Q_OBJECT
private slots:
    void closedFreehandShapesProduceEditableNodes_data()
    {
        QTest::addColumn<QString>("shape");
        QTest::addColumn<QVariantMap>("stroke");
        const QRectF bounds(.2, .2, .28, .16);
        QTest::newRow("rectangle-process") << QStringLiteral("process") << drawnRectangle(bounds);
        QTest::newRow("diamond-decision") << QStringLiteral("decision") << drawnDiamond(bounds);
        QTest::newRow("ellipse-terminal") << QStringLiteral("terminal") << drawnEllipse(bounds);
    }

    void closedFreehandShapesProduceEditableNodes()
    {
        QFETCH(QString, shape);
        QFETCH(QVariantMap, stroke);
        const QVariantList ink{stroke};
        const auto result = DiagramRecognition::recognize(ink);
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        auto diagram = result.value("diagram").toMap();
        const auto nodes = diagram.value("nodes").toList();
        QCOMPARE(nodes.size(), 1);
        QCOMPARE(nodes.first().toMap().value("shape").toString(), shape);
        QCOMPARE(candidatesOfKind(result, "node").size(), 1);
        QCOMPARE(indexSet(candidatesOfKind(result, "node").first().toMap().value("indexes").toList()), QSet<int>{0});
        QVERIFY(result.value("unassignedIndexes").toList().isEmpty());
        diagram.insert("id", "recognized");
        QVERIFY2(!PageContent::validatedItem(diagram).isEmpty(), "Recognized geometry must be accepted by the real editable diagram model.");
        QCOMPARE(ink.first().toMap(), stroke);
    }

    void fourSeparateSidesBecomeOneNodeAndSmallHandwritingStaysALabel()
    {
        const QRectF bounds(.1, .1, .32, .2);
        QVariantList ink = splitRectangle(bounds);
        ink.append(drawnEllipse(QRectF(.18, .175, .024, .036)));
        ink.append(drawnPath({{.23, .17}, {.23, .23}, {.26, .23}}));
        const auto result = DiagramRecognition::recognize(ink);
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        QCOMPARE(result.value("diagram").toMap().value("nodes").toList().size(), 1);
        const auto candidates = candidatesOfKind(result, "node");
        QCOMPARE(candidates.size(), 1);
        QCOMPARE(indexSet(candidates.first().toMap().value("indexes").toList()), QSet<int>({0, 1, 2, 3}));
        QSet<int> labelIndexes;
        for (const QVariant &label : result.value("labelGroups").toList()) labelIndexes.unite(indexSet(label.toMap().value("indexes").toList()));
        QVERIFY(labelIndexes.contains(4) && labelIndexes.contains(5));
        QVERIFY(result.value("diagram").toMap().value("edges").toList().isEmpty());
    }

    void explicitArrowheadsChooseDirectionAndAllowTwoWayConnections_data()
    {
        QTest::addColumn<bool>("reverseHead");
        QTest::addColumn<bool>("separateWings");
        QTest::newRow("single-V") << false << false;
        QTest::newRow("two-separate-wings") << false << true;
        QTest::newRow("two-way") << true << false;
    }

    void explicitArrowheadsChooseDirectionAndAllowTwoWayConnections()
    {
        QFETCH(bool, reverseHead);
        QFETCH(bool, separateWings);
        const QVariantList ink = arrowDiagram(reverseHead, separateWings);
        const auto result = DiagramRecognition::recognize(ink);
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        const auto diagram = result.value("diagram").toMap();
        const auto nodes = diagram.value("nodes").toList();
        const auto edges = diagram.value("edges").toList();
        QCOMPARE(nodes.size(), 2);
        QCOMPARE(edges.size(), reverseHead ? 2 : 1);
        const bool firstIsLeft = nodes[0].toMap().value("x").toDouble() < nodes[1].toMap().value("x").toDouble();
        const QString left = nodes[firstIsLeft ? 0 : 1].toMap().value("id").toString();
        const QString right = nodes[firstIsLeft ? 1 : 0].toMap().value("id").toString();
        QSet<QString> directions;
        for (const QVariant &edge : edges) {
            const auto values = edge.toMap();
            directions.insert(values.value("from").toString() + ":" + values.value("to").toString());
        }
        QVERIFY(directions.contains(left + ":" + right));
        if (reverseHead) QVERIFY(directions.contains(right + ":" + left));
        QVERIFY(result.value("unassignedIndexes").toList().isEmpty());
    }

    void missingOrAmbiguousArrowheadsPreserveTheUncertainInk()
    {
        QVariantList noHead = arrowDiagram();
        noHead.removeLast();
        auto result = DiagramRecognition::recognize(noHead);
        QVERIFY(result.value("error").toString().isEmpty());
        QCOMPARE(result.value("diagram").toMap().value("nodes").toList().size(), 2);
        QVERIFY(result.value("diagram").toMap().value("edges").toList().isEmpty());
        QVERIFY(indexSet(result.value("unassignedIndexes").toList()).contains(2));
        QVERIFY(!result.value("warnings").toList().isEmpty());

        const QVariantList ambiguous{
            drawnRectangle(QRectF(.1, .2, .18, .1)), drawnRectangle(QRectF(.62, .12, .22, .1)),
            drawnRectangle(QRectF(.62, .28, .22, .1)), drawnPath({{.28, .25}, {.62, .25}}),
            drawnPath({{.585, .235}, {.62, .25}, {.585, .265}})};
        result = DiagramRecognition::recognize(ambiguous);
        QVERIFY(result.value("error").toString().isEmpty());
        QCOMPARE(result.value("diagram").toMap().value("nodes").toList().size(), 3);
        QVERIFY(result.value("diagram").toMap().value("edges").toList().isEmpty());
        QVERIFY(indexSet(result.value("unassignedIndexes").toList()).contains(3));
    }

    void handwritingAndScribblesAreNotPromotedToDiagramShapes()
    {
        const QVariantList handwriting{
            drawnEllipse(QRectF(.18, .175, .024, .036)),
            drawnPath({{.23, .17}, {.23, .23}, {.26, .23}}),
            drawnPath({{.3, .22}, {.32, .18}, {.34, .22}, {.36, .18}, {.38, .22}})};
        const auto result = DiagramRecognition::recognize(handwriting);
        QVERIFY(!result.value("error").toString().isEmpty());
        QVERIFY(result.value("diagram").toMap().value("nodes").toList().isEmpty());
    }

    void invalidInputAndResourceCapsFailBeforePartialConversion()
    {
        QVariantList tooManyStrokes;
        const auto line = drawnPath({{.1, .1}, {.2, .2}}, 1);
        for (int index = 0; index < 4001; ++index) tooManyStrokes.append(line);
        QVERIFY(!DiagramRecognition::recognize(tooManyStrokes).value("error").toString().isEmpty());
        QVariantList tooManyPoints;
        tooManyPoints.reserve(120001);
        const QVariantMap point{{"x", .2}, {"y", .2}, {"p", .5}};
        for (int index = 0; index < 120001; ++index) tooManyPoints.append(point);
        QVERIFY(!DiagramRecognition::recognize({QVariantMap{{"tool", "pen"}, {"points", tooManyPoints}}})
            .value("error").toString().isEmpty());
        QVariantList tooManyNodes;
        for (int index = 0; index < 65; ++index)
            tooManyNodes.append(drawnRectangle(QRectF(.03 + (index % 8) * .12, .03 + (index / 8) * .105, .075, .065)));
        QVERIFY(!DiagramRecognition::recognize(tooManyNodes).value("error").toString().isEmpty());
        const QVariantMap invalid{{"tool", "pen"}, {"points", QVariantList{
            QVariantMap{{"x", std::numeric_limits<double>::quiet_NaN()}, {"y", .2}, {"p", .5}},
            QVariantMap{{"x", .3}, {"y", .3}, {"p", .5}}}}};
        QVERIFY(!DiagramRecognition::recognize({invalid}).value("error").toString().isEmpty());
    }

    void emptyDiagramWorkspaceSurvivesReopenTransferAndLibraryNavigation()
    {
        QTemporaryDir directory;
        const QString sourcePath = directory.filePath("source.json");
        QString id;
        QString pageId;
        {
            Notebook source(sourcePath);
            QVERIFY(source.createDiagramBook("Empty flow", {}));
            QVERIFY(source.diagramBook());
            QCOMPARE(source.pageCount(), 1);
            QVERIFY(source.pageItems().isEmpty());
            QVERIFY(source.strokes().isEmpty());
            id = source.bookId();
            pageId = source.pageId();
            const auto rows = source.libraryItems();
            QCOMPARE(rows.size(), 1);
            QCOMPARE(rows.first().toMap().value("type").toString(), QStringLiteral("diagram"));
            QVERIFY(rows.first().toMap().value("empty").toBool());
            QVERIFY(rows.first().toMap().value("itemId").toString().isEmpty());
            QVERIFY(source.createBook("Writing"));
            QVERIFY(!source.diagramBook());
            const auto opened = source.openLibraryItem(id, pageId, {});
            QVERIFY(opened.value("workspace").toBool());
            QCOMPARE(source.bookId(), id);
            QCOMPARE(source.pageId(), pageId);
            QVERIFY(source.diagramBook());
        }
        Notebook reopened(sourcePath);
        QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
        QVERIFY(reopened.openBook(id));
        QVERIFY(reopened.diagramBook());
        const auto exported = reopened.exportBookForTransfer(id);
        QVERIFY(!exported.isEmpty());
        const auto exportedBook = QJsonDocument::fromJson(exported).object().value("books").toArray().first().toObject();
        QCOMPARE(exportedBook.value("workspace").toString(), QStringLiteral("diagrams"));
        Notebook receiver(directory.filePath("receiver.json"));
        const auto preview = receiver.receiveBookTransfer(exported);
        QCOMPARE(preview.value("action").toString(), QStringLiteral("create"));
        const auto applied = receiver.receiveBookTransfer(exported, preview.value("token").toByteArray());
        QVERIFY2(applied.value("error").toString().isEmpty(), qPrintable(applied.value("error").toString()));
        QVERIFY(receiver.openBook(id));
        QVERIFY(receiver.diagramBook());
        QCOMPARE(receiver.pageId(), pageId);
        QVERIFY(receiver.pageItems().isEmpty());
        QCOMPARE(receiver.bookTransferRevision(id), reopened.bookTransferRevision(id));
        QCOMPARE(receiver.libraryItems().size(), 1);
    }

    void conversionIsOneUndoableOperationAndOriginalInkCanBeRestoredAfterReopen_data()
    {
        QTest::addColumn<bool>("removeInk");
        QTest::newRow("keep-original-ink") << false;
        QTest::newRow("replace-and-recover-original-ink") << true;
    }

    void conversionIsOneUndoableOperationAndOriginalInkCanBeRestoredAfterReopen()
    {
        QFETCH(bool, removeInk);
        QTemporaryDir directory;
        const QString path = directory.filePath("source.json");
        QVariantList original;
        QString id;
        QString diagramId;
        {
            Notebook source(path);
            QVERIFY(source.createDiagramBook("Recognized flow", {}));
            for (const QVariant &stroke : arrowDiagram()) QVERIFY(source.appendStroke(stroke.toMap()));
            original = source.strokes();
            id = source.bookId();
            const auto selection = source.selectInk(fullPageSelection());
            const auto plan = source.diagramHandwritingPlan(selection);
            QVERIFY2(plan.value("error").toString().isEmpty(), qPrintable(plan.value("error").toString()));
            QVERIFY2(source.applyDiagramHandwriting(selection, plan, plan.value("diagram").toMap(), {}, removeInk), qPrintable(source.error()));
            QCOMPARE(source.pageItems().size(), 1);
            QCOMPARE(source.strokes().size(), removeInk ? 0 : original.size());
            const auto converted = source.pageItems();
            diagramId = converted.first().toMap().value("id").toString();
            QCOMPARE(converted.first().toMap().value("nodes").toList().size(), 2);
            QCOMPARE(converted.first().toMap().value("edges").toList().size(), 1);
            QVERIFY(source.undo());
            QVERIFY(source.pageItems().isEmpty());
            QCOMPARE(source.strokes(), original);
            QVERIFY(source.redo());
            QCOMPARE(source.pageItems(), converted);
            QCOMPARE(source.strokes().size(), removeInk ? 0 : original.size());
        }
        Notebook reopened(path);
        QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
        QVERIFY(reopened.openBook(id));
        QCOMPARE(reopened.pageItems().size(), 1);
        if (removeInk) {
            QVERIFY(reopened.pageItems().first().toMap().contains("handwritingSource"));
            QVERIFY2(reopened.restoreOriginalHandwriting(diagramId), qPrintable(reopened.error()));
            QCOMPARE(reopened.strokes(), original);
            QVERIFY(reopened.pageItems().isEmpty());
            QVERIFY(reopened.undo());
            QVERIFY(reopened.strokes().isEmpty());
            QCOMPARE(reopened.pageItems().size(), 1);
        } else {
            QCOMPARE(reopened.strokes(), original);
            QVERIFY(!reopened.pageItems().first().toMap().contains("handwritingSource"));
        }
    }

    void labelAcceptanceRemovesOnlyReviewedInkAndKeepsUncertainMarks()
    {
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        QVERIFY(source.createDiagramBook("Labels", {}));
        QVERIFY(source.appendStroke(drawnRectangle(QRectF(.1, .1, .32, .2))));
        QVERIFY(source.appendStroke(drawnPath({{.23, .17}, {.23, .23}, {.26, .23}})));
        QVERIFY(source.appendStroke(drawnPath({{.7, .7}, {.75, .73}, {.7, .76}, {.75, .79}})));
        const auto original = source.strokes();
        const auto selection = source.selectInk(fullPageSelection());
        const auto plan = source.diagramHandwritingPlan(selection);
        QVERIFY2(plan.value("error").toString().isEmpty(), qPrintable(plan.value("error").toString()));
        QCOMPARE(plan.value("labelGroups").toList().size(), 1);
        auto reviewed = plan.value("diagram").toMap();
        auto nodes = reviewed.value("nodes").toList();
        auto node = nodes.first().toMap();
        node.insert("text", "Reviewed label");
        nodes[0] = node;
        reviewed.insert("nodes", nodes);
        const auto labelId = plan.value("labelGroups").toList().first().toMap().value("id").toString();
        QVERIFY(!labelId.isEmpty());
        QVERIFY(!source.applyDiagramHandwriting(selection, plan, reviewed, {}, true));
        QCOMPARE(source.strokes(), original);
        QVERIFY(source.pageItems().isEmpty());
        QVERIFY(!source.applyDiagramHandwriting(selection, plan, reviewed, {labelId, labelId}, true));
        QVERIFY(!source.applyDiagramHandwriting(selection, plan, reviewed, {QStringLiteral("unknown-label")}, true));
        QCOMPARE(source.strokes(), original);
        QVERIFY2(source.applyDiagramHandwriting(selection, plan, reviewed, {labelId}, true), qPrintable(source.error()));
        QCOMPARE(source.strokes(), QVariantList{original.last()});
        QCOMPARE(source.pageItems().first().toMap().value("nodes").toList().first().toMap().value("text").toString(), QStringLiteral("Reviewed label"));
        QVERIFY(source.undo());
        QCOMPARE(source.strokes(), original);
        QVERIFY(source.pageItems().isEmpty());
        const auto secondSelection = source.selectInk(fullPageSelection());
        const auto secondPlan = source.diagramHandwritingPlan(secondSelection);
        QVERIFY2(source.applyDiagramHandwriting(secondSelection, secondPlan, secondPlan.value("diagram").toMap(), {}, true), qPrintable(source.error()));
        QCOMPARE(source.strokes(), QVariantList({original[1], original[2]}));
    }

    void staleCrossPageHiddenLayerAndTamperedPlansNeverChangeContent_data()
    {
        QTest::addColumn<QString>("scenario");
        for (const auto *name : {"append-after-plan", "cross-page", "hidden-layer", "different-layer", "tampered-geometry", "tampered-plan"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }

    void staleCrossPageHiddenLayerAndTamperedPlansNeverChangeContent()
    {
        QFETCH(QString, scenario);
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        QVERIFY(source.createDiagramBook("Guarded conversion", {}));
        QVERIFY(source.appendStroke(drawnRectangle(QRectF(.2, .2, .28, .16))));
        const auto selection = source.selectInk(fullPageSelection());
        auto plan = source.diagramHandwritingPlan(selection);
        QVERIFY2(plan.value("error").toString().isEmpty(), qPrintable(plan.value("error").toString()));
        auto reviewed = plan.value("diagram").toMap();
        if (scenario == "append-after-plan") QVERIFY(source.appendStroke(drawnPath({{.7, .7}, {.8, .8}})));
        if (scenario == "cross-page") QVERIFY(source.addPage());
        if (scenario == "hidden-layer") QVERIFY(source.setLayerVisible(source.activeLayer(), false));
        if (scenario == "different-layer") QVERIFY(source.addLayer("Other layer"));
        if (scenario == "tampered-geometry") {
            auto nodes = reviewed.value("nodes").toList();
            auto node = nodes.first().toMap();
            node.insert("x", node.value("x").toDouble() + .1);
            nodes[0] = node;
            reviewed.insert("nodes", nodes);
        }
        if (scenario == "tampered-plan") {
            auto candidates = plan.value("candidates").toList();
            auto candidate = candidates.first().toMap();
            candidate.insert("indexes", QVariantList{});
            candidates[0] = candidate;
            plan.insert("candidates", candidates);
        }
        const auto before = source.exportLibraryForTransfer();
        QVERIFY(!source.applyDiagramHandwriting(selection, plan, reviewed, {}, true));
        QVERIFY(!source.error().isEmpty());
        QCOMPARE(source.exportLibraryForTransfer(), before);
    }

    void extensionsReplaceSameDiagramAndUndoGeometryWithOnlyRecognizedInk()
    {
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        QVERIFY(source.createDiagramBook("Incremental flow", {}));
        QVERIFY(appendInk(source, arrowDiagram()));
        QVERIFY2(convertInk(source), qPrintable(source.error()));
        auto base = source.pageItems().first().toMap();
        auto nodes = base.value("nodes").toList();
        auto first = nodes.first().toMap(); first.insert("text", "Existing label"); nodes[0] = first;
        base.insert("nodes", nodes);
        auto edges = base.value("edges").toList();
        auto edge = edges.first().toMap(); edge.insert("text", "Existing route"); edge.insert("route", "orthogonal"); edges[0] = edge;
        base.insert("edges", edges);
        QVERIFY(source.savePageItem(base));
        base = source.pageItems().first().toMap();
        const auto extension = diagramExtension();
        QVERIFY(appendInk(source, extension));
        const auto uncertain = drawnPath({{.20, .75}, {.24, .78}, {.20, .81}, {.24, .84}});
        QVERIFY(source.appendStroke(uncertain));
        const auto inkBefore = source.strokes();
        const auto selection = source.selectInk(fullPageSelection());
        const auto plan = source.diagramHandwritingPlan(selection);
        QVERIFY2(plan.value("error").toString().isEmpty(), qPrintable(plan.value("error").toString()));
        QCOMPARE(plan.value("targetItemId"), base.value("id"));
        QVERIFY(plan.value("extendsExisting").toBool());
        QVERIFY2(source.applyDiagramHandwriting(selection, plan, plan.value("diagram").toMap(), {}, true), qPrintable(source.error()));
        QCOMPARE(source.pageItems().size(), 1);
        const auto merged = source.pageItems().first().toMap();
        QCOMPARE(merged.value("id"), base.value("id"));
        QCOMPARE(merged.value("nodes").toList().size(), 3);
        QCOMPARE(merged.value("edges").toList().size(), 2);
        QCOMPARE(merged.value("nodes").toList().first().toMap().value("text"), first.value("text"));
        QCOMPARE(merged.value("edges").toList().first().toMap(), edge);
        QCOMPARE(source.strokes(), QVariantList{inkBefore.last()});
        QCOMPARE(merged.value("handwritingSource").toMap().value("baseDiagram").toMap(), base);
        QVERIFY(source.undo());
        QCOMPARE(source.pageItems(), QVariantList{base});
        QCOMPARE(source.strokes(), inkBefore);
        QVERIFY(source.redo());
        QCOMPARE(source.pageItems(), QVariantList{merged});
        QCOMPARE(source.strokes(), QVariantList{inkBefore.last()});
    }

    void repeatedExtensionsRecoverEditableBaseAfterReopen_data()
    {
        QTest::addColumn<bool>("baseHasInk");
        QTest::newRow("previously-converted") << true;
        QTest::newRow("manual-or-imported") << false;
    }

    void repeatedExtensionsRecoverEditableBaseAfterReopen()
    {
        QFETCH(bool, baseHasInk);
        QTemporaryDir directory;
        const QString path = directory.filePath("source.json");
        QVariantMap base, merged;
        QVariantList additions;
        QString id;
        {
            Notebook source(path);
            QVERIFY(source.createDiagramBook("Recovery", {}));
            QVERIFY(appendInk(source, arrowDiagram()));
            QVERIFY2(convertInk(source, baseHasInk), qPrintable(source.error()));
            if (!baseHasInk) QVERIFY(source.eraseStrokes(source.selectInk(fullPageSelection()).value("indexes").toList()));
            base = source.pageItems().first().toMap();
            id = base.value("id").toString();
            QVERIFY(appendInk(source, diagramExtension()));
            additions.append(source.strokes());
            QVERIFY2(convertInk(source), qPrintable(source.error()));
            QVERIFY(appendInk(source, diagramExtension(true)));
            additions.append(source.strokes());
            QVERIFY2(convertInk(source), qPrintable(source.error()));
            merged = source.pageItems().first().toMap();
            QCOMPARE(merged.value("nodes").toList().size(), 4);
            QCOMPARE(merged.value("edges").toList().size(), 3);
            const auto recovery = merged.value("handwritingSource").toMap();
            QCOMPARE(recovery.value("baseDiagram").toMap(), base);
            QCOMPARE(recovery.value("strokes").toList().size(), additions.size());
            QVERIFY(!recovery.value("baseDiagram").toMap().value("handwritingSource").toMap().contains("baseDiagram"));
            const auto portable = QJsonDocument::fromJson(source.exportLibraryForTransfer()).object();
            QCOMPARE(portable.value("version").toInt(), 10);
            auto downgraded = portable; downgraded.insert("version", 9);
            Notebook peer(directory.filePath("peer.json"));
            const auto rejected = peer.receiveBookTransfer(QJsonDocument(downgraded).toJson(QJsonDocument::Compact));
            QVERIFY(!rejected.value("error").toString().isEmpty());
        }
        Notebook reopened(path);
        QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
        QCOMPARE(reopened.pageItems(), QVariantList{merged});
        QVERIFY2(reopened.restoreOriginalHandwriting(id), qPrintable(reopened.error()));
        QCOMPARE(reopened.pageItems(), QVariantList{base});
        QCOMPARE(reopened.strokes().size(), additions.size());
        QVERIFY(reopened.undo());
        QCOMPARE(reopened.pageItems(), QVariantList{merged});
        QVERIFY(reopened.strokes().isEmpty());
        QVERIFY(reopened.redo());
        QCOMPARE(reopened.pageItems(), QVariantList{base});
        if (baseHasInk) {
            QVERIFY(reopened.restoreOriginalHandwriting(id));
            QVERIFY(reopened.pageItems().isEmpty());
            QCOMPARE(reopened.strokes().size(), additions.size() + arrowDiagram().size());
        } else {
            QVERIFY(!reopened.restoreOriginalHandwriting(id));
            QCOMPARE(reopened.pageItems(), QVariantList{base});
        }
    }

    void multipleDiagramsRequireExplicitTargetAndAllowSeparateCreation()
    {
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        QVERIFY(source.createDiagramBook("Multiple flows", {}));
        QVERIFY(appendInk(source, arrowDiagram()));
        QVERIFY(convertInk(source));
        const QString firstId = source.pageItems().first().toMap().value("id").toString();
        QVERIFY(appendInk(source, {drawnRectangle(QRectF(.10, .50, .22, .12))}));
        QVERIFY2(convertInk(source, true, "new"), qPrintable(source.error()));
        QCOMPARE(source.pageItems().size(), 2);
        QVERIFY(appendInk(source, diagramExtension()));
        const auto selection = source.selectInk(fullPageSelection());
        const auto pending = source.diagramHandwritingPlan(selection);
        QVERIFY(pending.value("requiresTarget").toBool());
        QCOMPARE(pending.value("targetChoices").toList().size(), 2);
        const auto before = source.exportLibraryForTransfer();
        QVERIFY(!source.applyDiagramHandwriting(selection, pending, {}, {}, true));
        QCOMPARE(source.exportLibraryForTransfer(), before);
        const auto targeted = source.diagramHandwritingPlan(selection, firstId);
        QVERIFY2(targeted.value("error").toString().isEmpty(), qPrintable(targeted.value("error").toString()));
        QVERIFY(!targeted.value("requiresTarget").toBool());
        QVERIFY(source.applyDiagramHandwriting(selection, targeted, targeted.value("diagram").toMap(), {}, true));
        QCOMPARE(source.pageItems().size(), 2);
        QCOMPARE(source.pageItems().first().toMap().value("nodes").toList().size(), 3);
    }

    void keepingExtensionInkDoesNotDuplicateAlreadyConvertedGeometry()
    {
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        QVERIFY(source.createDiagramBook("Keep additions", {}));
        QVERIFY(appendInk(source, arrowDiagram()));
        QVERIFY(convertInk(source));
        QVERIFY(appendInk(source, diagramExtension()));
        const auto originalInk = source.strokes();
        QVERIFY2(convertInk(source, false), qPrintable(source.error()));
        QCOMPARE(source.strokes(), originalInk);
        QCOMPARE(source.pageItems().size(), 1);
        QCOMPARE(source.pageItems().first().toMap().value("nodes").toList().size(), 3);
        const auto selection = source.selectInk(fullPageSelection());
        const auto repeated = source.diagramHandwritingPlan(selection);
        QVERIFY(!repeated.value("hasChanges").toBool());
        const auto before = source.exportLibraryForTransfer();
        QVERIFY(!source.applyDiagramHandwriting(selection, repeated, repeated.value("diagram").toMap(), {}, false));
        QCOMPARE(source.exportLibraryForTransfer(), before);
    }

    void extensionRejectsStaleOrChangedExistingLabels()
    {
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        QVERIFY(source.createDiagramBook("Guard existing", {}));
        QVERIFY(appendInk(source, arrowDiagram()));
        QVERIFY(convertInk(source));
        QVERIFY(appendInk(source, diagramExtension()));
        const auto selection = source.selectInk(fullPageSelection());
        const auto plan = source.diagramHandwritingPlan(selection);
        auto reviewed = plan.value("diagram").toMap();
        auto nodes = reviewed.value("nodes").toList();
        auto node = nodes.first().toMap(); node.insert("text", "Forged old label"); nodes[0] = node;
        reviewed.insert("nodes", nodes);
        const auto before = source.exportLibraryForTransfer();
        QVERIFY(!source.applyDiagramHandwriting(selection, plan, reviewed, {}, true));
        QCOMPARE(source.exportLibraryForTransfer(), before);
        auto existing = source.pageItems().first().toMap();
        auto existingNodes = existing.value("nodes").toList();
        node = existingNodes.first().toMap(); node.insert("text", "Changed in editor"); existingNodes[0] = node;
        existing.insert("nodes", existingNodes);
        QVERIFY(source.savePageItem(existing));
        const auto changed = source.exportLibraryForTransfer();
        QVERIFY(!source.applyDiagramHandwriting(selection, plan, plan.value("diagram").toMap(), {}, true));
        QCOMPARE(source.exportLibraryForTransfer(), changed);
    }

    void recoveryMetadataIsBoundedAndTransformsWithDiagram()
    {
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        QVERIFY(source.createDiagramBook("Moved recovery", {}));
        QVERIFY(appendInk(source, arrowDiagram()));
        QVERIFY(convertInk(source));
        const auto base = source.pageItems().first().toMap();
        QVERIFY(appendInk(source, diagramExtension()));
        QVERIFY(convertInk(source));
        const auto merged = source.pageItems().first().toMap();
        auto invalid = merged;
        auto recovery = invalid.value("handwritingSource").toMap();
        recovery.insert("baseDiagram", merged); invalid.insert("handwritingSource", recovery);
        QVERIFY(PageContent::validatedItem(invalid).isEmpty());
        invalid = merged; recovery = invalid.value("handwritingSource").toMap();
        QVariantList tooMany;
        for (int i = 0; i < 4001; ++i) tooMany.append(diagramExtension().first());
        recovery.insert("strokes", tooMany); invalid.insert("handwritingSource", recovery);
        QVERIFY(PageContent::validatedItem(invalid).isEmpty());
        const QPointF pivot(.5, .5), offset(.01, .01);
        const auto moved = InkSelectionGeometry::transformedItems({merged}, pivot, offset, .8, 4);
        const auto movedBase = InkSelectionGeometry::transformedItems({base}, pivot, offset, .8, 4);
        QCOMPARE(moved.size(), 1);
        QCOMPARE(movedBase.size(), 1);
        QCOMPARE(moved.first().toMap().value("handwritingSource").toMap().value("baseDiagram"), movedBase.first());
        const auto expectedInk = InkSelectionGeometry::transformed(
            merged.value("handwritingSource").toMap().value("strokes").toList(), pivot, offset, .8, 4);
        QCOMPARE(moved.first().toMap().value("handwritingSource").toMap().value("strokes").toList(), expectedInk);
    }

    void extensionPreservesRenderedOriginalNodeOnPage()
    {
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        QVERIFY(source.createDiagramBook("Preserved typography", {}));
        QVERIFY(appendInk(source, arrowDiagram()));
        QVERIFY(convertInk(source));
        auto base = source.pageItems().first().toMap();
        auto nodes = base.value("nodes").toList();
        auto node = nodes.first().toMap(); node.insert("text", "Existing label"); nodes[0] = node;
        base.insert("nodes", nodes);
        QVERIFY(source.savePageItem(base));
        base = source.pageItems().first().toMap();
        QVERIFY(appendInk(source, diagramExtension()));
        QVERIFY2(convertInk(source), qPrintable(source.error()));
        const auto merged = source.pageItems().first().toMap();
        const auto render = [](const QVariantMap &item) {
            QImage image(1200, 1600, QImage::Format_RGB32);
            image.fill(Qt::white);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            PageContent::paintItems(painter, image.size(), {item});
            return image;
        };
        const auto before = render(base), after = render(merged);
        // The left node is untouched by the new vertical connector on the
        // right. Compare its real rendered label, outline and page position.
        const QRect region(110, 230, 285, 212);
        int changed = 0, inkPixels = 0;
        for (int y = region.top(); y <= region.bottom(); ++y) {
            for (int x = region.left(); x <= region.right(); ++x) {
                const int oldGray = qGray(before.pixel(x, y)), newGray = qGray(after.pixel(x, y));
                inkPixels += oldGray < 220;
                changed += qAbs(oldGray - newGray) > 8;
            }
        }
        QVERIFY(inkPixels > 100);
        QVERIFY2(changed <= 4, qPrintable(QStringLiteral("Original node changed at %1 rendered pixels").arg(changed)));
    }

    void deletedRecoveryLayerAllowsFurtherExtensionAndRecovery()
    {
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        QVERIFY(source.createDiagramBook("Removed layer", {}));
        QVERIFY(appendInk(source, arrowDiagram()));
        QVERIFY(convertInk(source));
        const auto base = source.pageItems().first().toMap();
        QVERIFY(source.addLayer("Additions"));
        const QString additionsLayer = source.activeLayer();
        QVERIFY(appendInk(source, diagramExtension()));
        QVERIFY(convertInk(source));
        QVERIFY(source.deleteLayer(additionsLayer));
        QVERIFY(source.activeLayer() != additionsLayer);
        QVERIFY(appendInk(source, diagramExtension(true)));
        QVERIFY2(convertInk(source), qPrintable(source.error()));
        const auto merged = source.pageItems().first().toMap();
        QCOMPARE(merged.value("nodes").toList().size(), 4);
        QCOMPARE(merged.value("handwritingSource").toMap().value("layerId").toString(), source.activeLayer());
        QVERIFY(source.restoreOriginalHandwriting(merged.value("id").toString()));
        QCOMPARE(source.pageItems(), QVariantList{base});
        QCOMPARE(source.strokes().size(), diagramExtension().size() + diagramExtension(true).size());
    }

    void reusableLibraryCopiesAndMarkdownKeepDiagramsEditable()
    {
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        QVERIFY(source.createBookWithItem("Reusable flow", {}, editableDiagram()));
        QVERIFY(source.diagramBook());
        const QString originalId = source.bookId();
        const QString originalPage = source.pageId();
        const auto originalItem = source.pageItems().first().toMap();
        const auto originalRevision = source.bookTransferRevision(originalId);
        const auto copy = source.libraryItemCopy(originalId, originalPage, originalItem.value("id").toString());
        QVERIFY(!copy.isEmpty());
        QVERIFY(!copy.contains("id"));
        QVERIFY(!copy.contains("x"));
        QVERIFY(!copy.contains("sourceTable"));
        QVERIFY(source.createBook("Writing with a diagram"));
        QVERIFY(source.savePageItem(copy));
        auto placed = source.pageItems().first().toMap();
        QVERIFY(placed.value("id") != originalItem.value("id"));
        QCOMPARE(placed.value("nodes"), originalItem.value("nodes"));
        auto nodes = placed.value("nodes").toList();
        auto node = nodes.first().toMap();
        node.insert("text", "Independent copy");
        nodes[0] = node;
        placed.insert("nodes", nodes);
        QVERIFY(source.savePageItem(placed));
        QCOMPARE(source.bookTransferRevision(originalId), originalRevision);
        const auto file = QUrl::fromLocalFile(directory.filePath("writing.md"));
        QVERIFY2(source.exportMarkdownWithAssets(file), qPrintable(source.error()));
        const QString markdown = QString::fromUtf8(fileBytes(file.toLocalFile()));
        const auto pngMatch = QRegularExpression(QStringLiteral("\\(([^)]+\\.png)\\)")).match(markdown);
        const auto diagramMatch = QRegularExpression(QStringLiteral("\\(([^)]+\\.drawio)\\)")).match(markdown);
        QVERIFY2(pngMatch.hasMatch(), qPrintable(markdown));
        QVERIFY2(diagramMatch.hasMatch(), qPrintable(markdown));
        const QImage image(directory.filePath(pngMatch.captured(1)));
        QVERIFY(!image.isNull());
        bool nonwhite = false;
        for (int y = 0; y < image.height() && !nonwhite; ++y)
            for (int x = 0; x < image.width() && !nonwhite; ++x)
                nonwhite = image.pixelColor(x, y).lightness() < 200;
        QVERIFY2(nonwhite, "Exported diagram preview must contain visible geometry.");
        DiagramExport exporter;
        const auto imported = exporter.load(QUrl::fromLocalFile(directory.filePath(diagramMatch.captured(1))));
        QVERIFY2(imported.value("error").toString().isEmpty(), qPrintable(imported.value("error").toString()));
        const auto importedDiagram = imported.value("diagram").toMap();
        QCOMPARE(importedDiagram.value("nodes").toList().size(), 2);
        QCOMPARE(importedDiagram.value("edges").toList().size(), 1);
        QCOMPARE(importedDiagram.value("nodes").toList().first().toMap().value("text").toString(), QStringLiteral("Independent copy"));
    }

    void largeLabelGroupsPreserveEveryStrokeBelowTheLimit()
    {
        QVariantList ink{drawnRectangle(QRectF(.1, .1, .32, .2))};
        const auto mark = drawnPath({{.2, .2}, {.215, .2}}, 1);
        for (int index = 0; index < 3998; ++index) ink.append(mark);
        const auto result = DiagramRecognition::recognize(ink);
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        const auto groups = result.value("labelGroups").toList();
        QCOMPARE(groups.size(), 1);
        const auto group = groups.first().toMap();
        QCOMPARE(group.value("strokes").toList().size(), 3998);
        const auto indexes = indexSet(group.value("indexes").toList());
        QCOMPARE(indexes.size(), 3998);
        QVERIFY(indexes.contains(1) && indexes.contains(3998));
        QCOMPARE(candidatesOfKind(result, "node").size(), 1);
        QVERIFY(result.value("unassignedIndexes").toList().isEmpty());
    }

    void flowchartShapesHaveVisibleOutlinesAndRoundTripThroughDrawio()
    {
        const QStringList shapes{QStringLiteral("database"), QStringLiteral("subprocess"),
            QStringLiteral("display"), QStringLiteral("manual-input"), QStringLiteral("preparation"),
            QStringLiteral("delay"), QStringLiteral("off-page"), QStringLiteral("cloud")};
        const QRectF specimen(20, 30, 140, 80);
        QVariantList nodes;
        for (qsizetype i = 0; i < shapes.size(); ++i) {
            const auto &shape = shapes.at(i);
            const auto path = diagramNodeOutline(shape, specimen);
            QVERIFY2(!path.isEmpty() && path.contains(specimen.center()), qPrintable(shape));
            QVERIFY2(specimen.adjusted(-.001, -.001, .001, .001).contains(path.boundingRect()), qPrintable(shape));
            QVERIFY(diagramShapeRequiresNewFormat(shape));
            nodes.append(QVariantMap{{"id", QStringLiteral("node-%1").arg(i)}, {"shape", shape},
                {"text", shape}, {"x", .05 + (i % 4) * .24}, {"y", .10 + (i / 4) * .48},
                {"width", .18}, {"height", .22}});
        }
        for (const auto &shape : {"process", "terminal", "decision", "data", "note"})
            QVERIFY(!diagramShapeRequiresNewFormat(QString::fromLatin1(shape)));

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto url = QUrl::fromLocalFile(directory.filePath(QStringLiteral("flowchart.drawio")));
        const QVariantMap diagram{{"type", "diagram"}, {"nodes", nodes},
            {"edges", QVariantList{QVariantMap{{"id", "edge-1"}, {"from", "node-0"},
                {"to", "node-7"}, {"text", "Archive"}}}}};
        DiagramExport exporter;
        const QString saveError = exporter.save(url, diagram, QStringLiteral("Flowchart shapes"));
        QVERIFY2(saveError.isEmpty(), qPrintable(saveError));
        const auto imported = exporter.load(url);
        QVERIFY2(imported.value("error").toString().isEmpty(), qPrintable(imported.value("error").toString()));
        const auto result = imported.value("diagram").toMap();
        const auto importedNodes = result.value("nodes").toList();
        QCOMPARE(importedNodes.size(), shapes.size());
        for (qsizetype i = 0; i < shapes.size(); ++i) {
            QCOMPARE(importedNodes.at(i).toMap().value("shape").toString(), shapes.at(i));
            QCOMPARE(importedNodes.at(i).toMap().value("text").toString(), shapes.at(i));
        }
        const auto edges = result.value("edges").toList();
        QCOMPARE(edges.size(), 1);
        QCOMPARE(edges.first().toMap().value("text").toString(), QStringLiteral("Archive"));
        QCOMPARE(edges.first().toMap().value("from"), importedNodes.first().toMap().value("id"));
        QCOMPARE(edges.first().toMap().value("to"), importedNodes.last().toMap().value("id"));
    }

    void umlShapesAndEditableCompartmentsRoundTripThroughDrawio()
    {
        const QList<QPair<QString, QString>> specimens{
            {QStringLiteral("uml-class"), QStringLiteral("Book\n\n-title: String\n-author: String\n\n+open()")},
            {QStringLiteral("uml-interface"), QStringLiteral("Editable\n\n\n\n+save()\n+undo()")},
            {QStringLiteral("uml-object"), QStringLiteral("copy: Book\n\ntitle = <First edition>")},
            {QStringLiteral("uml-actor"), QStringLiteral("Reader")},
            {QStringLiteral("uml-use-case"), QStringLiteral("Open book")},
            {QStringLiteral("uml-component"), QStringLiteral("Library")},
            {QStringLiteral("uml-package"), QStringLiteral("Model")},
            {QStringLiteral("uml-deployment"), QStringLiteral("Paper Pro")},
            {QStringLiteral("uml-lifeline"), QStringLiteral("Session")},
            {QStringLiteral("uml-class"), QStringLiteral("Heading only")},
            {QStringLiteral("uml-interface"), QString(200, u'I')},
        };
        QVariantList nodes;
        for (qsizetype index = 0; index < specimens.size(); ++index) {
            const auto &[shape, label] = specimens.at(index);
            nodes.append(QVariantMap{{"id", QStringLiteral("node-%1").arg(index)}, {"shape", shape},
                {"text", label}, {"x", .05 + (index % 4) * .24}, {"y", .04 + (index / 4) * .32},
                {"width", .18}, {"height", .25}});
        }
        const QVariantMap diagram{{"type", "diagram"}, {"nodes", nodes},
            {"edges", QVariantList{QVariantMap{{"id", "edge-1"}, {"from", "node-0"},
                {"to", "node-4"}, {"text", "opens"}}}}};
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto url = QUrl::fromLocalFile(directory.filePath(QStringLiteral("uml.drawio")));
        DiagramExport exporter;
        const QString saveError = exporter.save(url, diagram, QStringLiteral("UML"));
        QVERIFY2(saveError.isEmpty(), qPrintable(saveError));
        const auto imported = exporter.load(url);
        QVERIFY2(imported.value("error").toString().isEmpty(), qPrintable(imported.value("error").toString()));
        QVERIFY2(imported.value("warnings").toString().isEmpty(), qPrintable(imported.value("warnings").toString()));
        const auto result = imported.value("diagram").toMap();
        const auto importedNodes = result.value("nodes").toList();
        QCOMPARE(importedNodes.size(), specimens.size());
        for (qsizetype index = 0; index < specimens.size(); ++index) {
            QCOMPARE(importedNodes.at(index).toMap().value("shape").toString(), specimens.at(index).first);
            QCOMPARE(importedNodes.at(index).toMap().value("text").toString(), specimens.at(index).second);
        }
        const auto edges = result.value("edges").toList();
        QCOMPARE(edges.size(), 1);
        QCOMPARE(edges.first().toMap().value("from"), importedNodes.at(0).toMap().value("id"));
        QCOMPARE(edges.first().toMap().value("to"), importedNodes.at(4).toMap().value("id"));
    }

    void importsDrawioClassifierRowsWithoutReinterpretingGenericEllipses()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile source(directory.filePath(QStringLiteral("external-uml.drawio")));
        const QByteArray xml = R"XML(
            <mxGraphModel pageWidth="500" pageHeight="400"><root>
              <mxCell id="0"/><mxCell id="1" parent="0"/>
              <mxCell id="container" value="&amp;laquo;interface&amp;raquo;&lt;br&gt;&lt;b&gt;Storage&lt;/b&gt;"
                style="swimlane;html=1;horizontal=1;startSize=40;childLayout=stackLayout;horizontalStack=0;collapsible=0;"
                vertex="1" parent="1"><mxGeometry as="geometry" x="10" y="10" width="160" height="160"/></mxCell>
              <mxCell id="field" value="key: String" style="html=1;strokeColor=none;fillColor=none;"
                vertex="1" parent="container"><mxGeometry as="geometry" y="40" width="160" height="24"/></mxCell>
              <mxCell id="separator" style="line;strokeColor=#202020;"
                vertex="1" parent="container"><mxGeometry as="geometry" y="64" width="160" height="0"/></mxCell>
              <mxCell id="method" value="save()" style="text;html=1;"
                vertex="1" parent="container"><mxGeometry as="geometry" y="64" width="160" height="24"/></mxCell>
              <mxCell id="oval" value="Flowchart end" style="ellipse;whiteSpace=wrap;"
                vertex="1" parent="1"><mxGeometry as="geometry" x="200" y="10" width="120" height="60"/></mxCell>
            </root></mxGraphModel>
        )XML";
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(xml), xml.size());
        source.close();
        DiagramExport exporter;
        const auto result = exporter.load(QUrl::fromLocalFile(source.fileName()));
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        QVERIFY2(result.value("warnings").toString().isEmpty(), qPrintable(result.value("warnings").toString()));
        const auto nodes = result.value("diagram").toMap().value("nodes").toList();
        QCOMPARE(nodes.size(), 2);
        QCOMPARE(nodes.at(0).toMap().value("shape").toString(), QStringLiteral("uml-interface"));
        QCOMPARE(nodes.at(0).toMap().value("text").toString(), QStringLiteral("«interface»\nStorage\n\nkey: String\n\nsave()"));
        QCOMPARE(nodes.at(1).toMap().value("shape").toString(), QStringLiteral("terminal"));
    }

    void overshotOvalsAndShortPenTailsBecomeOneEditableTerminal_data()
    {
        QTest::addColumn<bool>("tail");
        QTest::newRow("extra-pass") << false;
        QTest::newRow("extra-pass-with-tail") << true;
    }

    void overshotOvalsAndShortPenTailsBecomeOneEditableTerminal()
    {
        QFETCH(bool, tail);
        auto stroke = ovalArc(QRectF(.18, .18, .28, .15), 1.09);
        if (tail) {
            auto points = stroke.value("points").toList();
            const auto last = points.last().toMap();
            points.append(QVariantMap{{"x", last.value("x").toDouble() + .04},
                {"y", last.value("y").toDouble() + .015}, {"p", .6}});
            stroke.insert("points", points);
        }
        const auto original = stroke;
        const auto result = DiagramRecognition::recognize({stroke});
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        const auto nodes = result.value("diagram").toMap().value("nodes").toList();
        QCOMPARE(nodes.size(), 1);
        QCOMPARE(nodes.first().toMap().value("shape").toString(), QStringLiteral("terminal"));
        QCOMPARE(indexSet(candidatesOfKind(result, "node").first().toMap().value("indexes").toList()), QSet<int>{0});
        QVERIFY(result.value("unassignedIndexes").toList().isEmpty());
        QCOMPARE(stroke, original);
    }

    void tiltedTwoStrokeRhombusWithSmallGapsIsAWholeDecisionNode()
    {
        const QPointF center(.25, .59);
        const auto rotate = [center](QPointF point) {
            const qreal radians = 10 * 3.141592653589793 / 180;
            const qreal x = (point.x() - center.x()) * 600;
            const qreal y = (point.y() - center.y()) * 800;
            return center + QPointF((x * std::cos(radians) - y * std::sin(radians)) / 600,
                (x * std::sin(radians) + y * std::cos(radians)) / 800);
        };
        const QVariantList ink{
            drawnPath({rotate({.25, .50}), rotate({.386, .594}), rotate({.25, .68})}),
            drawnPath({rotate({.254, .678}), rotate({.12, .59}), rotate({.247, .502})}),
            drawnPath({{.345, .6075}, {.351, .613}})};
        const auto result = DiagramRecognition::recognize(ink);
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        const auto nodes = result.value("diagram").toMap().value("nodes").toList();
        QCOMPARE(nodes.size(), 1);
        QCOMPARE(nodes.first().toMap().value("shape").toString(), QStringLiteral("decision"));
        QCOMPARE(indexSet(candidatesOfKind(result, "node").first().toMap().value("indexes").toList()), QSet<int>({0, 1}));
        const auto labels = result.value("labelGroups").toList();
        QCOMPARE(labels.size(), 1);
        QCOMPARE(labels.first().toMap().value("id"), nodes.first().toMap().value("id"));
        QCOMPARE(indexSet(labels.first().toMap().value("indexes").toList()), QSet<int>{2});
        QVERIFY(result.value("unassignedIndexes").toList().isEmpty());
    }

    void irregularSketchedContoursRemainEditableWithoutPromotingOpenMarks()
    {
        QList<QPointF> taperedOval;
        for (int i = 0; i <= 64; ++i) {
            const qreal angle = 3.141592653589793 * (1.94 * i / 64 - 0.5);
            taperedOval.append({.26 + .17 * std::cos(angle),
                .23 + .059 * std::sin(angle) * (1 - .42 * std::cos(angle))});
        }
        const QVariantList ink{
            drawnPath(taperedOval, 1),
            // A partly open side and a slanted, slightly curved opposite side.
            drawnPath({{.56, .43}, {.70, .418}, {.88, .423}, {.852, .456},
                {.849, .522}, {.72, .513}, {.555, .508}, {.553, .46}}),
            // An open U and a tiny loop are not flowchart nodes.
            drawnPath({{.13, .60}, {.13, .71}, {.31, .71}, {.34, .61}}),
            drawnEllipse(QRectF(.66, .45, .02, .022))};
        const auto result = DiagramRecognition::recognize(ink);
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        const auto nodes = result.value("diagram").toMap().value("nodes").toList();
        QCOMPARE(nodes.size(), 2);
        QCOMPARE(nodes.at(0).toMap().value("shape").toString(), QStringLiteral("terminal"));
        QCOMPARE(nodes.at(1).toMap().value("shape").toString(), QStringLiteral("process"));
        const auto geometry = candidatesOfKind(result, "node");
        QCOMPARE(geometry.size(), 2);
        QCOMPARE(indexSet(geometry.at(0).toMap().value("indexes").toList()), QSet<int>{0});
        QCOMPARE(indexSet(geometry.at(1).toMap().value("indexes").toList()), QSet<int>{1});
        QCOMPARE(indexSet(result.value("unassignedIndexes").toList()), QSet<int>{2});
        const auto labels = result.value("labelGroups").toList();
        QCOMPARE(labels.size(), 1);
        QCOMPARE(indexSet(labels.first().toMap().value("indexes").toList()), QSet<int>{3});
    }

    void curvedShaftAndClosedTriangularHeadProduceADirectedEditableEdge_data()
    {
        QTest::addColumn<bool>("continuous");
        QTest::addColumn<bool>("retraced");
        QTest::newRow("separate-triangle") << false << false;
        QTest::newRow("retraced-triangle") << false << true;
        QTest::newRow("one-stroke-shaft-and-triangle") << true << false;
    }

    void curvedShaftAndClosedTriangularHeadProduceADirectedEditableEdge()
    {
        QFETCH(bool, continuous);
        QFETCH(bool, retraced);
        QVariantList ink{drawnRectangle(QRectF(.10, .15, .22, .12)), drawnRectangle(QRectF(.62, .15, .22, .12))};
        if (!continuous) ink.append(drawnPath({{.32, .21}, {.40, .197}, {.49, .198}, {.56, .205}, {.62, .21}}));
        ink.append(curvedArrowHead(continuous, retraced));
        const auto result = DiagramRecognition::recognize(ink);
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        const auto diagram = result.value("diagram").toMap();
        const auto nodes = diagram.value("nodes").toList();
        const auto edges = diagram.value("edges").toList();
        QCOMPARE(nodes.size(), 2);
        QCOMPARE(edges.size(), 1);
        const bool firstIsLeft = nodes[0].toMap().value("x").toDouble() < nodes[1].toMap().value("x").toDouble();
        QCOMPARE(edges.first().toMap().value("from"), nodes[firstIsLeft ? 0 : 1].toMap().value("id"));
        QCOMPARE(edges.first().toMap().value("to"), nodes[firstIsLeft ? 1 : 0].toMap().value("id"));
        QCOMPARE(indexSet(candidatesOfKind(result, "edge").first().toMap().value("indexes").toList()),
            continuous ? QSet<int>{2} : QSet<int>({2, 3}));
        QVERIFY(result.value("unassignedIndexes").toList().isEmpty());
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("curved-arrow.json"));
        QVERIFY2(notebook.createBookWithItem("Curved arrow", {}, diagram), qPrintable(notebook.error()));
        QCOMPARE(notebook.pageItems().first().toMap().value("edges").toList().size(), 1);
    }

    void openOvalsCrossingScribblesAndCompetingHeadsRemainHandwriting()
    {
        const auto open = ovalArc(QRectF(.2, .2, .28, .15), .75);
        const auto crossed = drawnPath({{.2, .2}, {.4, .4}, {.2, .4}, {.4, .2}, {.2, .2}});
        for (const auto &stroke : {open, crossed}) {
            const auto result = DiagramRecognition::recognize({stroke});
            QVERIFY(!result.value("error").toString().isEmpty());
            QVERIFY(result.value("diagram").toMap().value("nodes").toList().isEmpty());
        }
        auto ambiguous = arrowDiagram();
        ambiguous.removeLast();
        ambiguous.append(curvedArrowHead(false, false));
        ambiguous.append(drawnPath({{.618, .211}, {.588, .199}, {.588, .223}, {.618, .211}}, 5));
        const auto result = DiagramRecognition::recognize(ambiguous);
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        QCOMPARE(result.value("diagram").toMap().value("nodes").toList().size(), 2);
        QVERIFY(result.value("diagram").toMap().value("edges").toList().isEmpty());
        const auto unassigned = indexSet(result.value("unassignedIndexes").toList());
        QVERIFY(unassigned.contains(2) && unassigned.contains(3) && unassigned.contains(4));
        QVERIFY(!result.value("warnings").toStringList().isEmpty());
    }

    void uncertainShapeOnTheRightAndItsConnectorArePreserved()
    {
        const QVariantList ink{
            drawnRectangle(QRectF(.1, .15, .22, .12)),
            ovalArc(QRectF(.62, .15, .22, .12), .75),
            drawnPath({{.32, .21}, {.62, .21}}),
            curvedArrowHead(false, false)};
        const auto result = DiagramRecognition::recognize(ink);
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        QCOMPARE(result.value("diagram").toMap().value("nodes").toList().size(), 1);
        QVERIFY(result.value("diagram").toMap().value("edges").toList().isEmpty());
        const auto unassigned = indexSet(result.value("unassignedIndexes").toList());
        QVERIFY(unassigned.contains(1) && unassigned.contains(2) && unassigned.contains(3));
    }

    void smallRoundMarksAtConnectorEndsAreNotArrowheads_data()
    {
        QTest::addColumn<qreal>("radius");
        QTest::newRow("five-pixel-radius") << qreal(5);
        QTest::newRow("seven-pixel-radius") << qreal(7);
    }

    void smallRoundMarksAtConnectorEndsAreNotArrowheads()
    {
        QFETCH(qreal, radius);
        auto ink = arrowDiagram();
        ink.removeLast();
        ink.append(drawnEllipse(QRectF(.62 - radius / 600, .21 - radius / 800,
            2 * radius / 600, 2 * radius / 800)));
        const auto result = DiagramRecognition::recognize(ink);
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        QCOMPARE(result.value("diagram").toMap().value("nodes").toList().size(), 2);
        QVERIFY(result.value("diagram").toMap().value("edges").toList().isEmpty());
        const auto unassigned = indexSet(result.value("unassignedIndexes").toList());
        QVERIFY(unassigned.contains(2) && unassigned.contains(3));
    }
};

QTEST_MAIN(DiagramRecognitionTests)
#include "diagramrecognition_tests.moc"
