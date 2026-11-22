#include "chartrecognizer.h"

#include <QPointF>
#include <QtTest/QTest>

namespace {
QVariantMap stroke(std::initializer_list<QPointF> samples)
{
    QVariantList points;
    for (const auto &point : samples)
        points.append(QVariantMap{{"x", point.x()}, {"y", point.y()}});
    return {{"tool", "pen"}, {"points", points}};
}

QVariantList axes()
{
    return {stroke({{.18, .82}, {.37, .82}, {.63, .82}, {.82, .82}}),
            stroke({{.18, .20}, {.18, .35}, {.18, .58}, {.18, .82}})};
}

QVariantMap curve()
{
    return stroke({{.244, .727}, {.372, .634}, {.500, .572}, {.628, .386}, {.756, .324}});
}
}

class ChartRecognitionTests : public QObject
{
    Q_OBJECT

private slots:
    void recognizesMixedContinuousAndSplitBars()
    {
        auto strokes = axes();
        // Reverse some endpoints and shuffle the split edges. The cap is not perfectly aligned.
        strokes.append(stroke({{.364, .635}, {.364, .82}}));
        strokes.append(stroke({{.46, .82}, {.46, .448}, {.54, .448}, {.54, .82}}));
        strokes.append(stroke({{.362, .638}, {.32, .637}, {.286, .637}}));
        strokes.append(stroke({{.284, .82}, {.284, .634}}));
        strokes.append(stroke({{.636, .82}, {.636, .324}, {.716, .324}, {.716, .82}}));

        const auto result = ChartRecognizer().recognize(strokes);
        QVERIFY2(result.contains("item"), qPrintable(result.value("error").toString()));
        const auto chart = result.value("item").toMap();
        QCOMPARE(chart.value("chartKind").toString(), QStringLiteral("bars"));
        const auto points = chart.value("points").toList();
        QCOMPARE(points.size(), 3);
        const QList<QPointF> expected{{.225, .3}, {.5, .6}, {.775, .8}};
        for (qsizetype index = 0; index < points.size(); ++index) {
            const auto point = points.at(index).toMap();
            QVERIFY(qAbs(point.value("x").toDouble() - expected.at(index).x()) < .015);
            QVERIFY(qAbs(point.value("y").toDouble() - expected.at(index).y()) < .015);
        }
    }

    void recognizesOneBarDrawnWithThreeEdges()
    {
        auto strokes = axes();
        strokes.append(stroke({{.284, .82}, {.284, .634}}));
        strokes.append(stroke({{.284, .634}, {.364, .634}}));
        strokes.append(stroke({{.364, .634}, {.364, .82}}));
        const auto result = ChartRecognizer().recognize(strokes);
        QVERIFY2(result.contains("item"), qPrintable(result.value("error").toString()));
        const auto chart = result.value("item").toMap();
        QCOMPARE(chart.value("chartKind").toString(), QStringLiteral("bars"));
        QCOMPARE(chart.value("points").toList().size(), 1);
    }

    void rejectsAnAmbiguousSplitBar()
    {
        auto strokes = axes();
        strokes.append(stroke({{.28, .82}, {.28, .62}}));
        strokes.append(stroke({{.282, .82}, {.282, .62}}));
        strokes.append(stroke({{.365, .82}, {.365, .62}}));
        strokes.append(stroke({{.28, .62}, {.365, .62}}));
        strokes.append(stroke({{.46, .82}, {.46, .448}, {.54, .448}, {.54, .82}}));
        const auto result = ChartRecognizer().recognize(strokes);
        QVERIFY(!result.contains("item"));
        QVERIFY(!result.value("error").toString().isEmpty());
    }

    void ignoresASeparatedAnnotationWithinTheAxes()
    {
        auto strokes = axes();
        strokes.append(curve());
        const ChartRecognizer recognizer;
        const auto expected = recognizer.recognize(strokes);
        QCOMPARE(expected.value("item").toMap().value("chartKind").toString(), QStringLiteral("line"));
        strokes.append(stroke({{.35, .40}, {.37, .38}, {.38, .40}, {.40, .38}, {.42, .39}, {.44, .38}, {.46, .39}}));
        QCOMPARE(recognizer.recognize(strokes), expected);
    }

    void rejectsAmbiguousExtraInk_data()
    {
        QTest::addColumn<QVariantMap>("extra");
        QTest::newRow("second series") << stroke({{.26, .58}, {.44, .42}, {.53, .35}, {.73, .26}});
        QTest::newRow("annotation overlaps data")
            << stroke({{.35, .65}, {.37, .63}, {.38, .65}, {.40, .63}, {.42, .64}, {.44, .63}, {.46, .64}});
    }

    void rejectsAmbiguousExtraInk()
    {
        QFETCH(QVariantMap, extra);
        auto strokes = axes();
        strokes.append(curve());
        strokes.append(extra);
        const auto result = ChartRecognizer().recognize(strokes);
        QVERIFY(!result.contains("item"));
        QVERIFY(!result.value("error").toString().isEmpty());
    }
};

QTEST_GUILESS_MAIN(ChartRecognitionTests)
#include "chartrecognition_tests.moc"
