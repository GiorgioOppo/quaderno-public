#include "diagramrecognizer.h"
#include "pagecontent.h"

#include <QLineF>
#include <QPolygonF>
#include <QSet>
#include <QTransform>
#include <QtTest/QTest>
#include <cmath>

namespace {
QVariantMap path(const QList<QPointF> &vertices)
{
    QVariantList points;
    for (qsizetype segment = 1; segment < vertices.size(); ++segment)
        for (int sample = segment == 1 ? 0 : 1; sample <= 12; ++sample) {
            const qreal fraction = qreal(sample) / 12;
            const QPointF point = vertices.at(segment - 1) * (1 - fraction) + vertices.at(segment) * fraction;
            points.append(QVariantMap{{"x", point.x()}, {"y", point.y()}, {"p", .6}});
        }
    return {{"tool", "pen"}, {"color", "#222222"}, {"width", 2.5}, {"points", points}};
}

QVariantMap rectangle(const QRectF &r)
{
    return path({r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft(), r.topLeft()});
}

QVariantList originalInk()
{
    return {rectangle({.10, .15, .22, .12}), rectangle({.62, .15, .22, .12}),
        path({{.32, .21}, {.62, .21}}), path({{.585, .195}, {.62, .21}, {.585, .225}})};
}

QVariantMap originalDiagram()
{
    QVariantMap item = DiagramRecognition::recognize(originalInk()).value("diagram").toMap();
    item.insert("id", "existing-flowchart");
    item.insert("textScale", 1.25);
    auto nodes = item.value("nodes").toList();
    for (int i = 0; i < nodes.size(); ++i) {
        auto node = nodes.at(i).toMap();
        node.insert("text", i == 0 ? "Start" : "Existing step");
        nodes[i] = node;
    }
    auto edges = item.value("edges").toList();
    for (QVariant &entry : edges) {
        auto edge = entry.toMap();
        edge.insert("text", "Continue");
        edge.insert("route", "orthogonal");
        entry = edge;
    }
    item.insert("nodes", nodes); item.insert("edges", edges);
    item.insert("handwritingSource", QVariantMap{{"layerId", "layer-1"}, {"strokes", originalInk()}});
    return item;
}

QVariantList extensionInk()
{
    return {rectangle({.62, .50, .22, .12}), path({{.73, .27}, {.73, .50}}),
        path({{.71, .472}, {.73, .50}, {.75, .472}})};
}

QRectF bounds(const QVariantMap &item)
{
    return {item.value("x").toDouble() * 600, item.value("y").toDouble() * 800,
            item.value("width").toDouble() * 600, item.value("height").toDouble() * 800};
}

QTransform rotation(const QVariantMap &item, bool inverse = false)
{
    const auto center = bounds(item).center();
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(item.value("rotation").toDouble() * (inverse ? -1 : 1));
    transform.translate(-center.x(), -center.y());
    return transform;
}

QPolygonF nodeCorners(const QVariantMap &item, int index)
{
    const auto node = item.value("nodes").toList().at(index).toMap();
    const auto r = bounds(item);
    const QRectF local(r.x() + node.value("x").toDouble() * r.width(),
        r.y() + node.value("y").toDouble() * r.height(), node.value("width").toDouble() * r.width(),
        node.value("height").toDouble() * r.height());
    return rotation(item).map(QPolygonF(local));
}

QVariantList rotatedInk(QVariantList ink, const QVariantMap &item)
{
    const auto transform = rotation(item);
    for (QVariant &entry : ink) {
        auto stroke = entry.toMap();
        auto points = stroke.value("points").toList();
        for (QVariant &pointEntry : points) {
            auto point = pointEntry.toMap();
            const QPointF next = transform.map(QPointF(point.value("x").toDouble() * 600, point.value("y").toDouble() * 800));
            point.insert("x", next.x() / 600); point.insert("y", next.y() / 800);
            pointEntry = point;
        }
        stroke.insert("points", points); entry = stroke;
    }
    return ink;
}

QSet<int> candidateIndexes(const QVariantMap &plan)
{
    QSet<int> result;
    for (const QVariant &candidate : plan.value("candidates").toList())
        for (const QVariant &index : candidate.toMap().value("indexes").toList()) result.insert(index.toInt());
    return result;
}

qreal renderScale(const QVariantMap &item)
{
    const auto r = bounds(item);
    return qMax(.1, qMin(r.width(), r.height()) / 350) * item.value("styleScale", 1.0).toDouble();
}
}

class DiagramContextTests final : public QObject
{
    Q_OBJECT
private slots:
    void extendsExistingAndPreservesProperties()
    {
        const auto original = originalDiagram();
        QVERIFY(!PageContent::validatedItem(original).isEmpty());
        const auto plan = DiagramRecognition::recognize(extensionInk(), original);
        QVERIFY2(plan.value("error").toString().isEmpty(), qPrintable(plan.value("error").toString()));
        QVERIFY(plan.value("extendsExisting").toBool());
        QVERIFY(plan.value("hasChanges").toBool());
        QCOMPARE(plan.value("addedNodeCount").toInt(), 1);
        QCOMPARE(plan.value("addedEdgeCount").toInt(), 1);
        const auto merged = plan.value("diagram").toMap();
        QVERIFY(!PageContent::validatedItem(merged).isEmpty());
        QCOMPARE(merged.value("id"), original.value("id"));
        QCOMPARE(merged.value("textScale"), original.value("textScale"));
        QCOMPARE(merged.value("handwritingSource"), original.value("handwritingSource"));
        QCOMPARE(merged.value("nodes").toList().size(), 3);
        QCOMPARE(merged.value("edges").toList().size(), 2);
        QCOMPARE(merged.value("edges").toList().first(), original.value("edges").toList().first());
        for (int i = 0; i < 2; ++i) {
            const auto oldNode = original.value("nodes").toList().at(i).toMap();
            const auto newNode = merged.value("nodes").toList().at(i).toMap();
            QCOMPARE(newNode.value("id"), oldNode.value("id"));
            QCOMPARE(newNode.value("text"), oldNode.value("text"));
            const auto oldCorners = nodeCorners(original, i), newCorners = nodeCorners(merged, i);
            for (int c = 0; c < oldCorners.size(); ++c) QVERIFY(QLineF(oldCorners.at(c), newCorners.at(c)).length() < 1e-8);
        }
        QVERIFY(qAbs(renderScale(original) - renderScale(merged)) < 1e-10);
        QCOMPARE(candidateIndexes(plan), (QSet<int>{0, 1, 2}));
        QCOMPARE(plan.value("existingNodeIds").toList().size(), 2);
        QCOMPARE(plan.value("existingEdgeIds").toList().size(), 1);
    }

    void arrowOnlyConnectsSavedNodes()
    {
        auto original = originalDiagram();
        original.insert("edges", QVariantList{});
        const auto ink = originalInk().mid(2);
        const auto plan = DiagramRecognition::recognize(ink, original);
        QVERIFY2(plan.value("error").toString().isEmpty(), qPrintable(plan.value("error").toString()));
        QCOMPARE(plan.value("addedNodeCount").toInt(), 0);
        QCOMPARE(plan.value("addedEdgeCount").toInt(), 1);
        const auto merged = plan.value("diagram").toMap();
        QCOMPARE(merged.value("nodes"), original.value("nodes"));
        QCOMPARE(bounds(merged), bounds(original));
        const auto edge = merged.value("edges").toList().first().toMap();
        QCOMPARE(edge.value("from").toString(), "node-1");
        QCOMPARE(edge.value("to").toString(), "node-2");
        QCOMPARE(candidateIndexes(plan), (QSet<int>{0, 1}));
    }

    void freehandConnectorHeadVariants_data()
    {
        QTest::addColumn<QVariantList>("ink");
        QTest::addColumn<bool>("orthogonal");
        QTest::addColumn<bool>("recognized");
        const auto box = [](qreal x, qreal y, qreal w, qreal h) {
            return rectangle({x / 600, y / 800, w / 600, h / 800});
        };
        const auto draw = [](QList<QPointF> vertices) {
            for (auto &point : vertices) point = {point.x() / 600, point.y() / 800};
            return path(vertices);
        };
        const auto left = box(65, 361, 105, 78), right = box(272, 362, 105, 76);
        const auto horizontalShaft = draw({{170, 400}, {242, 400}});
        QTest::newRow("separate-head-with-short-stem")
            << QVariantList{left, right, horizontalShaft,
                draw({{243, 400}, {266, 399}, {253, 382}, {252, 417}, {266, 399}})}
            << false << true;
        const auto top = box(63, 114, 117, 82), lower = box(272, 195, 96, 77);
        QTest::newRow("single-stroke-elbow-and-triangle")
            << QVariantList{top, lower, draw({{180, 155}, {320, 154}, {310, 195},
                {296, 181}, {328, 181}, {310, 195}})} << true << true;
        const auto upperRight = box(390, 95, 100, 57), lowerLeft = box(290, 208, 105, 76);
        QTest::newRow("rounded-elbow-and-flat-head")
            << QVariantList{upperRight, lowerLeft, draw({{440, 152}, {440, 183}, {431, 192}, {351, 196},
                {336, 208}, {328, 195}, {353, 212}, {336, 208}})} << true << true;
        QTest::newRow("elbow-with-only-a-flick")
            << QVariantList{upperRight, lowerLeft, draw({{440, 152}, {440, 183}, {431, 192}, {351, 196},
                {336, 208}, {328, 195}})} << false << false;
    }

    void freehandConnectorHeadVariants()
    {
        QFETCH(QVariantList, ink);
        QFETCH(bool, orthogonal);
        QFETCH(bool, recognized);
        QVariantMap existing = DiagramRecognition::recognize(ink.first(2)).value("diagram").toMap();
        existing.insert("id", "test-connector-context");
        const auto nodes = existing.value("nodes").toList();
        QCOMPARE(nodes.size(), 2);
        const auto plan = DiagramRecognition::recognize(ink.sliced(2), existing);
        QVERIFY2(plan.value("error").toString().isEmpty(), qPrintable(plan.value("error").toString()));
        QCOMPARE(plan.value("addedNodeCount").toInt(), 0);
        QCOMPARE(plan.value("addedEdgeCount").toInt(), recognized ? 1 : 0);
        if (!recognized) {
            QVERIFY(candidateIndexes(plan).isEmpty());
            QVERIFY(plan.value("unassignedIndexes").toList().contains(0));
            return;
        }
        const auto edges = plan.value("diagram").toMap().value("edges").toList();
        const auto edge = edges.first().toMap();
        QCOMPARE(edge.value("from"), nodes.first().toMap().value("id"));
        QCOMPARE(edge.value("to"), nodes.last().toMap().value("id"));
        QCOMPARE(edge.value("route").toString() == "orthogonal", orthogonal);
        QCOMPARE(candidateIndexes(plan), ink.size() == 4 ? (QSet<int>{0, 1}) : QSet<int>{0});
    }

    void retainedOriginalsDoNotDuplicateOrGetConsumed()
    {
        const auto original = originalDiagram();
        auto ink = originalInk();
        const auto unchanged = DiagramRecognition::recognize(ink, original);
        QVERIFY2(unchanged.value("error").toString().isEmpty(), qPrintable(unchanged.value("error").toString()));
        QVERIFY(!unchanged.value("hasChanges").toBool());
        QCOMPARE(unchanged.value("diagram").toMap(), original);
        QVERIFY(unchanged.value("candidates").toList().isEmpty());
        QCOMPARE(unchanged.value("unassignedIndexes").toList().size(), ink.size());
        ink.append(extensionInk());
        const auto extended = DiagramRecognition::recognize(ink, original);
        QVERIFY2(extended.value("error").toString().isEmpty(), qPrintable(extended.value("error").toString()));
        QCOMPARE(extended.value("addedNodeCount").toInt(), 1);
        QCOMPARE(extended.value("addedEdgeCount").toInt(), 1);
        QCOMPARE(candidateIndexes(extended), (QSet<int>{4, 5, 6}));
    }

    void bidirectionalArrowAddsOnlyMissingReturnConnection()
    {
        const auto original = originalDiagram();
        auto ink = originalInk().mid(2);
        ink.append(path({{.355, .195}, {.32, .21}, {.355, .225}}));
        const auto plan = DiagramRecognition::recognize(ink, original);
        QVERIFY2(plan.value("error").toString().isEmpty(), qPrintable(plan.value("error").toString()));
        QCOMPARE(plan.value("addedNodeCount").toInt(), 0);
        QCOMPARE(plan.value("addedEdgeCount").toInt(), 1);
        const auto edges = plan.value("diagram").toMap().value("edges").toList();
        QCOMPARE(edges.size(), 2);
        QCOMPARE(edges.first(), original.value("edges").toList().first());
        const auto added = edges.last().toMap();
        QCOMPARE(added.value("from").toString(), "node-2");
        QCOMPARE(added.value("to").toString(), "node-1");
        QCOMPARE(candidateIndexes(plan), (QSet<int>{0, 2}));
        QVERIFY(plan.value("unassignedIndexes").toList().contains(1));
    }

    void ambiguousLineStaysInkAndExistingLabelsAreNotOcrCandidates()
    {
        auto original = originalDiagram();
        original.insert("edges", QVariantList{});
        const QVariantList ink{path({{.32, .21}, {.62, .21}}), path({{.15, .19}, {.17, .23}, {.19, .19}})};
        const auto plan = DiagramRecognition::recognize(ink, original);
        QVERIFY(plan.value("error").toString().isEmpty());
        QVERIFY(!plan.value("hasChanges").toBool());
        QVERIFY(plan.value("labelGroups").toList().isEmpty());
        QCOMPARE(plan.value("unassignedIndexes").toList().size(), 2);
        QCOMPARE(plan.value("diagram").toMap(), original);
    }

    void rotatedExtensionPreservesWorldPositions_data()
    {
        QTest::addColumn<double>("angle");
        QTest::newRow("clockwise") << 90.0;
        QTest::newRow("counterclockwise") << -90.0;
        QTest::newRow("arbitrary-angle") << 17.0;
    }

    void rotatedExtensionPreservesWorldPositions()
    {
        QFETCH(double, angle);
        QVariantMap original{{"id", "rotated"}, {"type", "diagram"}, {"x", .25}, {"y", .25},
            {"width", .5}, {"height", .35}, {"rotation", angle}, {"styleScale", .8},
            {"nodes", QVariantList{
                QVariantMap{{"id", "left"}, {"shape", "process"}, {"text", "Left"},
                    {"x", .1}, {"y", .1}, {"width", .2}, {"height", .2}},
                QVariantMap{{"id", "right"}, {"shape", "process"}, {"text", "Right"},
                    {"x", .7}, {"y", .1}, {"width", .2}, {"height", .2}}}}, {"edges", QVariantList{}}};
        QVERIFY(!PageContent::validatedItem(original).isEmpty());
        const QVariantList local{rectangle({.60, .66, .10, .07}), path({{.65, .355}, {.65, .66}}),
            path({{.63, .632}, {.65, .66}, {.67, .632}})};
        const auto plan = DiagramRecognition::recognize(rotatedInk(local, original), original);
        QVERIFY2(plan.value("error").toString().isEmpty(), qPrintable(plan.value("error").toString()));
        QCOMPARE(plan.value("addedNodeCount").toInt(), 1);
        QCOMPARE(plan.value("addedEdgeCount").toInt(), 1);
        const auto merged = plan.value("diagram").toMap();
        QVERIFY(!PageContent::validatedItem(merged).isEmpty());
        QCOMPARE(merged.value("rotation"), original.value("rotation"));
        for (int i = 0; i < 2; ++i) {
            const auto before = nodeCorners(original, i), after = nodeCorners(merged, i);
            for (int c = 0; c < before.size(); ++c) QVERIFY(QLineF(before.at(c), after.at(c)).length() < 1e-8);
        }
        QVERIFY(qAbs(renderScale(original) - renderScale(merged)) < 1e-10);
    }

    void supportedSavedContours_data()
    {
        QTest::addColumn<QString>("shape");
        for (const char *shape : {"process", "decision", "terminal", "data", "note"})
            QTest::newRow(shape) << QString::fromLatin1(shape);
    }

    void supportedSavedContours()
    {
        QFETCH(QString, shape);
        auto original = originalDiagram();
        auto nodes = original.value("nodes").toList();
        for (QVariant &entry : nodes) {
            auto node = entry.toMap(); node.insert("shape", shape); entry = node;
        }
        original.insert("nodes", nodes); original.insert("edges", QVariantList{});
        const auto plan = DiagramRecognition::recognize(originalInk().mid(2), original);
        QVERIFY2(plan.value("error").toString().isEmpty(), qPrintable(plan.value("error").toString()));
        QCOMPARE(plan.value("addedEdgeCount").toInt(), 1);
    }

    void newIdsAvoidBothNodeAndEdgeIds()
    {
        auto original = originalDiagram();
        auto edges = original.value("edges").toList();
        auto oldEdge = edges.first().toMap(); oldEdge.insert("id", "node-3"); edges[0] = oldEdge;
        original.insert("edges", edges);
        const auto plan = DiagramRecognition::recognize(extensionInk(), original);
        QVERIFY(plan.value("error").toString().isEmpty());
        const auto merged = plan.value("diagram").toMap();
        QCOMPARE(merged.value("nodes").toList().last().toMap().value("id").toString(), "node-4");
        QSet<QString> ids;
        for (const auto &key : {"nodes", "edges"}) for (const QVariant &entry : merged.value(key).toList()) {
            const auto id = entry.toMap().value("id").toString();
            QVERIFY(!ids.contains(id)); ids.insert(id);
        }
    }

    void invalidSeedFailsWithoutCandidates()
    {
        auto original = originalDiagram();
        original.insert("width", -1.0);
        const auto plan = DiagramRecognition::recognize(extensionInk(), original);
        QVERIFY(!plan.value("error").toString().isEmpty());
        QVERIFY(plan.value("diagram").toMap().isEmpty());
        QVERIFY(plan.value("candidates").toList().isEmpty());
        QVERIFY(!plan.value("hasChanges").toBool());
        QCOMPARE(plan.value("unassignedIndexes").toList().size(), 3);
    }

    void combinedNodeLimitIsEnforced()
    {
        QVariantList nodes;
        for (int i = 0; i < 64; ++i)
            nodes.append(QVariantMap{{"id", QStringLiteral("n%1").arg(i)}, {"shape", "process"}, {"text", ""},
                {"x", .03 + .04 * (i % 8)}, {"y", .03 + .04 * (i / 8)}, {"width", .025}, {"height", .025}});
        const QVariantMap original{{"id", "full"}, {"type", "diagram"}, {"x", 0.0}, {"y", 0.0},
            {"width", 1.0}, {"height", 1.0}, {"nodes", nodes}, {"edges", QVariantList{}}};
        QVERIFY(!PageContent::validatedItem(original).isEmpty());
        const auto plan = DiagramRecognition::recognize({rectangle({.7, .65, .2, .1})}, original);
        QVERIFY(plan.value("error").toString().contains("64"));
        QVERIFY(plan.value("diagram").toMap().isEmpty());
        QVERIFY(plan.value("candidates").toList().isEmpty());
    }

    void combinedEdgeLimitIsEnforced()
    {
        QVariantList nodes{
            QVariantMap{{"id", "n0"}, {"shape", "process"}, {"text", ""},
                {"x", .1}, {"y", .15}, {"width", .22}, {"height", .12}},
            QVariantMap{{"id", "n1"}, {"shape", "process"}, {"text", ""},
                {"x", .62}, {"y", .15}, {"width", .22}, {"height", .12}}};
        for (int i = 2; i < 17; ++i)
            nodes.append(QVariantMap{{"id", QStringLiteral("n%1").arg(i)}, {"shape", "process"}, {"text", ""},
                {"x", .03 + .04 * (i - 2)}, {"y", .7}, {"width", .025}, {"height", .025}});
        QVariantList edges;
        for (int i = 0; i < 17 && edges.size() < 128; ++i)
            for (int j = 0; j < 17 && edges.size() < 128; ++j) {
                if (i == j || (i == 0 && j == 1)) continue;
                edges.append(QVariantMap{{"id", QStringLiteral("e%1").arg(edges.size())},
                    {"from", QStringLiteral("n%1").arg(i)}, {"to", QStringLiteral("n%1").arg(j)}, {"text", ""}});
            }
        const QVariantMap original{{"id", "full"}, {"type", "diagram"}, {"x", 0.0}, {"y", 0.0},
            {"width", 1.0}, {"height", 1.0}, {"nodes", nodes}, {"edges", edges}};
        QVERIFY(!PageContent::validatedItem(original).isEmpty());
        const auto plan = DiagramRecognition::recognize(originalInk().mid(2), original);
        QVERIFY(plan.value("error").toString().contains("128"));
        QVERIFY(plan.value("diagram").toMap().isEmpty());
        QVERIFY(plan.value("candidates").toList().isEmpty());
    }
};

QTEST_GUILESS_MAIN(DiagramContextTests)
#include "diagramcontext_tests.moc"
