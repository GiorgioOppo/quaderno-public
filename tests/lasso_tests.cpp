#include <QtTest/QTest>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QTransform>
#include "notebook.h"
#include "inkselection.h"

namespace {
QVariantMap point(double x, double y)
{
    return {{"x", x}, {"y", y}, {"p", .6}};
}
QVariantMap stroke(std::initializer_list<QPointF> points)
{
    QVariantList samples;
    for (const QPointF &p : points) samples.append(point(p.x(), p.y()));
    return {{"tool", "pen"}, {"color", "#222222"}, {"width", 2.5}, {"points", samples}};
}
QVariantList polygon(double left = 0, double top = 0, double right = 1, double bottom = 1)
{
    return {point(left, top), point(right, top), point(right, bottom), point(left, bottom)};
}
QVariantMap selectAll(Notebook &book)
{
    return book.selectContent(polygon(), "contained", {}, "replace");
}
QPointF cellPoint(const QVariantMap &table, int row, int column, qreal x = .3, qreal y = .3)
{
    const qreal left = table.value("x").toDouble() * 600;
    const qreal top = table.value("y").toDouble() * 800;
    const qreal width = table.value("width").toDouble() * 600;
    const qreal height = table.value("height").toDouble() * 800;
    const QPointF center(left + width / 2, top + height / 2);
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(table.value("rotation").toDouble());
    transform.translate(-center.x(), -center.y());
    const QPointF placed = transform.map(QPointF(left + (column + x) * width / table.value("cols").toInt(),
                                               top + (row + y) * height / table.value("rows").toInt()));
    return {placed.x() / 600, placed.y() / 800};
}
QVariantMap tableItem(const Notebook &book)
{
    for (const QVariant &entry : book.pageItems())
        if (entry.toMap().value("type") == "table") return entry.toMap();
    return {};
}
}

class LassoTests : public QObject
{
    Q_OBJECT
private slots:
    void tableHandwritingUpdatesMappedCellsAtomically()
    {
        QTemporaryDir temp;
        const QString path = temp.filePath("notebooks.json");
        Notebook book(path);
        QVERIFY(book.createBook("Handwritten cells"));
        QVERIFY(book.savePageItem({{"type","table"},{"rows",2},{"cols",3},
            {"cells",QVariantList{"1","2","=SUM(A1:B1)","3","4","=AVERAGE(A2:B2)"}}}));
        const auto table = tableItem(book);
        const QString id = table.value("id").toString();
        QVERIFY(book.saveTableChart({{"sourceTable",id},{"xColumn",1},{"yColumn",2},{"hasHeader",false},
            {"chartKind","line"},{"title","Linked chart"}}));
        QVERIFY(book.appendStroke(stroke({cellPoint(table,0,0),cellPoint(table,0,0,.7,.7)})));
        QVERIFY(book.appendStroke(stroke({cellPoint(table,1,1),cellPoint(table,1,1,.7,.7)})));
        QVERIFY(book.appendStroke(stroke({cellPoint(table,0,0,.5,.15)}))); // A dot or accent stays in A1.
        QVERIFY(book.appendStroke(stroke({{.98,.98}}))); // Outside the table.
        QVERIFY(book.appendStroke(stroke({cellPoint(table,0,0),cellPoint(table,0,1)}))); // Crosses a border.
        auto highlighter = stroke({cellPoint(table,0,0),cellPoint(table,0,0,.6,.7)});
        highlighter["tool"] = "highlighter";
        QVERIFY(book.appendStroke(highlighter));
        auto line = highlighter; line["tool"] = "line";
        QVERIFY(book.appendStroke(line));
        const auto originalInk = book.strokes();
        const auto originalItems = book.pageItems();
        const auto selection = selectAll(book);
        const auto plan = book.tableHandwritingPlan(selection, id);
        QVERIFY2(!plan.contains("error"), qPrintable(plan.value("error").toString()));
        const auto cells = plan.value("cells").toList();
        QCOMPARE(cells.size(), 2);
        QCOMPARE(cells[0].toMap().value("address").toString(), QString("A1"));
        QCOMPARE(cells[0].toMap().value("indexes").toList(), QVariantList({0,2}));
        QCOMPARE(cells[1].toMap().value("address").toString(), QString("B2"));
        QCOMPARE(plan.value("unassignedCount").toInt(), 2);
        const QVariantList updates{QVariantMap{{"row",0},{"column",0},{"text","10"}},
                                   QVariantMap{{"row",1},{"column",1},{"text","-5"}}};
        QVERIFY(!book.applyTableHandwriting(selection, plan, updates, true, false));
        QCOMPARE(book.strokes(), originalInk);
        QCOMPARE(book.pageItems(), originalItems);
        QVERIFY2(book.applyTableHandwriting(selection, plan, updates, true, true), qPrintable(book.error()));
        QCOMPARE(book.strokes(), QVariantList({originalInk[3],originalInk[4],originalInk[5],originalInk[6]}));
        const auto updatedTable = tableItem(book);
        const QVariantList expected{"10","2","=SUM(A1:B1)","3","-5","=AVERAGE(A2:B2)"};
        QCOMPARE(updatedTable.value("cells").toList(), expected);
        QCOMPARE(book.tableCellValues(updatedTable)[2].toString(), QString("12"));
        QCOMPARE(book.tableCellValues(updatedTable)[5].toString(), QString("-1"));
        for (const char *key : {"x","y","width","height"}) QCOMPARE(updatedTable.value(key), table.value(key));
        QVERIFY(book.pageItems()[1] != originalItems[1]); // The chart follows the new numeric cells.
        const auto updatedItems = book.pageItems();
        QVERIFY(book.undo());
        QCOMPARE(book.strokes(), originalInk);
        QCOMPARE(book.pageItems(), originalItems);
        QVERIFY(book.redo());
        QCOMPARE(book.pageItems(), updatedItems);
        Notebook reopened(path);
        QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
        QCOMPARE(tableItem(reopened).value("cells").toList(), expected);
        QCOMPARE(reopened.strokes(), book.strokes());
        Notebook receiver(temp.filePath("received.json"));
        QVERIFY(receiver.importFromData(book.exportBookForTransfer(book.bookId())));
        QVERIFY(receiver.openBook(book.bookId()));
        QCOMPARE(tableItem(receiver).value("cells").toList(), expected);
    }

    void tableHandwritingRespectsRotationAndActiveLayer()
    {
        QTemporaryDir temp;
        Notebook book(temp.filePath("notebooks.json"));
        QVERIFY(book.createBook("Rotated cells"));
        QVERIFY(book.savePageItem({{"type","table"},{"rows",2},{"cols",2},{"cells",QVariantList{"","","",""}}}));
        QVERIFY(book.transformSelection(selectAll(book), 0, .25, .55, 25));
        const auto table = tableItem(book);
        QVERIFY(book.appendStroke(stroke({cellPoint(table,1,1),cellPoint(table,1,1,.6,.7)})));
        const auto lowerInk = book.strokes();
        QVERIFY(book.addLayer("Input cells"));
        const QString inputLayer = book.activeLayer();
        QVERIFY(book.appendStroke(stroke({cellPoint(table,0,1),cellPoint(table,0,1,.6,.7)})));
        const auto plan = book.tableHandwritingPlan(selectAll(book), table.value("id").toString());
        const auto cells = plan.value("cells").toList();
        QCOMPARE(cells.size(), 1);
        QCOMPARE(cells[0].toMap().value("address").toString(), QString("B1"));
        QCOMPARE(cells[0].toMap().value("indexes").toList(), QVariantList{1});
        auto upright = table; upright["rotation"] = 0;
        const auto recognizedPoint = cells[0].toMap().value("strokes").toList()[0].toMap().value("points").toList()[0].toMap();
        const auto expectedPoint = cellPoint(upright, 0, 1);
        QVERIFY(qAbs(recognizedPoint.value("x").toDouble() - expectedPoint.x()) < 1e-9);
        QVERIFY(qAbs(recognizedPoint.value("y").toDouble() - expectedPoint.y()) < 1e-9);
        const auto selection = selectAll(book);
        QVERIFY(book.applyTableHandwriting(selection, plan, {QVariantMap{{"row",0},{"column",1},{"text","é 12"}}}, true, false));
        QCOMPARE(book.strokes(), lowerInk);
        QVERIFY(book.undo());
        QCOMPARE(book.strokes().size(), 2);
        QVERIFY(book.setLayerVisible(inputLayer, false));
        QVERIFY(book.tableHandwritingPlan(selection, table.value("id").toString()).contains("error"));
    }

    void tableHandwritingRejectsStaleAndInvalidUpdates()
    {
        QTemporaryDir temp;
        Notebook book(temp.filePath("notebooks.json"));
        QVERIFY(book.createBook("Review before applying"));
        QVERIFY(book.savePageItem({{"type","table"},{"rows",1},{"cols",2},{"cells",QVariantList{"=SUM(B1)",""}}}));
        auto table = tableItem(book);
        const QString id = table.value("id").toString();
        QVERIFY(book.appendStroke(stroke({cellPoint(table,0,0),cellPoint(table,0,0,.6,.7)})));
        auto selection = selectAll(book);
        selection["items"] = QVariantList{}; // The table need not itself be inside the lasso.
        const auto plan = book.tableHandwritingPlan(selection, id);
        const auto before = book.exportBookForTransfer(book.bookId());
        const QVariantMap update{{"row",0},{"column",0},{"text","7"}};
        QVERIFY(!book.applyTableHandwriting(selection, plan, {update}, true, false)); // Formula protected.
        QVERIFY(!book.applyTableHandwriting(selection, plan, {update,update}, true, true));
        auto invalid = update; invalid["row"] = .5;
        QVERIFY(!book.applyTableHandwriting(selection, plan, {invalid}, true, true));
        invalid["row"] = false;
        QVERIFY(!book.applyTableHandwriting(selection, plan, {invalid}, true, true));
        invalid["row"] = "0";
        QVERIFY(!book.applyTableHandwriting(selection, plan, {invalid}, true, true));
        invalid = update; invalid["column"] = 1;
        QVERIFY(!book.applyTableHandwriting(selection, plan, {invalid}, true, true)); // No mapped ink in B1.
        invalid = update; invalid["text"] = QString(501, u'a');
        QVERIFY(!book.applyTableHandwriting(selection, plan, {invalid}, true, true));
        invalid["text"] = "   ";
        QVERIFY(!book.applyTableHandwriting(selection, plan, {invalid}, true, true));
        QVERIFY(!book.applyTableHandwriting(selection, plan, {}, true, true));
        QCOMPARE(book.exportBookForTransfer(book.bookId()), before);
        // A table changed outside the selected items still invalidates the saved plan.
        table["cells"] = QVariantList{"=SUM(B1)","20"};
        QVERIFY(book.savePageItem(table));
        const auto changed = book.exportBookForTransfer(book.bookId());
        QVERIFY(!book.applyTableHandwriting(selection, plan, {update}, true, true));
        QCOMPARE(book.exportBookForTransfer(book.bookId()), changed);
        const auto refreshed = book.tableHandwritingPlan(selection, id);
        const auto preservedInk = book.strokes();
        auto tampered = refreshed;
        tampered["cells"] = QVariantList{QVariantMap{{"row",0},{"column",1},{"indexes",QVariantList{999}}}};
        // Only the current geometry controls destination groups; supplied plan indexes are ignored.
        invalid = update; invalid["column"] = 1;
        QVERIFY(!book.applyTableHandwriting(selection, tampered, {invalid}, true, true));
        QVERIFY(book.applyTableHandwriting(selection, refreshed, {update}, false, true));
        QCOMPARE(book.strokes(), preservedInk);
        QVERIFY(book.undo());
        QCOMPARE(book.strokes(), preservedInk);
        QCOMPARE(tableItem(book).value("cells").toList(), QVariantList({"=SUM(B1)","20"}));
        QVERIFY(book.eraseStrokes({0}));
        const auto erased = book.exportBookForTransfer(book.bookId());
        QVERIFY(!book.applyTableHandwriting(selection, refreshed, {update}, true, true));
        QCOMPARE(book.exportBookForTransfer(book.bookId()), erased);
    }

    void tableHandwritingUnchangedValuesAndBorderDots()
    {
        QTemporaryDir temp;
        Notebook book(temp.filePath("notebooks.json"));
        QVERIFY(book.createBook("Same value"));
        QVERIFY(book.savePageItem({{"type","table"},{"rows",1},{"cols",2},{"cells",QVariantList{"7",""}}}));
        const auto table = tableItem(book);
        QVERIFY(book.appendStroke(stroke({cellPoint(table,0,0)})));
        QVERIFY(book.appendStroke(stroke({cellPoint(table,0,0,1,.5)}))); // Exact internal border.
        const auto selection = selectAll(book);
        const auto plan = book.tableHandwritingPlan(selection, table.value("id").toString());
        QCOMPARE(plan.value("unassignedCount").toInt(), 1);
        QCOMPARE(plan.value("cells").toList().size(), 1);
        const auto original = book.strokes();
        const QVariantList updates{QVariantMap{{"row",0},{"column",0},{"text","7"}}};
        QVERIFY(book.applyTableHandwriting(selection, plan, updates, false, false));
        QCOMPARE(book.strokes(), original);
        QVERIFY(book.applyTableHandwriting(selection, plan, updates, true, false));
        QCOMPARE(book.strokes(), QVariantList{original[1]});
        QVERIFY(book.undo());
        QCOMPARE(book.strokes(), original);
        QCOMPARE(tableItem(book), table);
    }

    void exactSelectionAndAccents()
    {
        const QPolygonF square{{.2, .2}, {.4, .2}, {.4, .4}, {.2, .4}};
        QVERIFY(InkSelectionGeometry::intersectsStroke(square, {{.25, .25}}, true));
        QVERIFY(InkSelectionGeometry::intersectsStroke(square, {{.2, .3}}, true));
        QVERIFY(InkSelectionGeometry::intersectsStroke(square, {{.1, .3}, {.5, .3}}, false));
        QVERIFY(!InkSelectionGeometry::intersectsStroke(square, {{.1, .3}, {.5, .3}}, true));
        // Both endpoints inside a concave polygon, while the middle crosses its indentation.
        const QPolygonF concave{{.1,.1},{.8,.1},{.8,.8},{.6,.8},{.6,.3},{.3,.3},{.3,.8},{.1,.8}};
        QVERIFY(!InkSelectionGeometry::intersectsStroke(concave, {{.2,.6},{.7,.6}}, true));
        QVERIFY(InkSelectionGeometry::intersectsStroke(concave, {{.2,.6},{.7,.6}}, false));
        QTemporaryDir temp;
        Notebook book(temp.filePath("notebooks.json"));
        QVERIFY(book.createBook("Accents"));
        QVERIFY(book.appendStroke(stroke({{.2001,.2001}})));
        const auto tiny = book.selectContent(polygon(.2,.2,.2002,.2002), "contained", {}, "replace");
        QCOMPARE(tiny.value("strokes").toList().size(), 1);
    }

    void addSubtractAndStaleSelection()
    {
        QTemporaryDir temp;
        Notebook book(temp.filePath("notebooks.json"));
        QVERIFY(book.createBook("Selection"));
        QVERIFY(book.appendStroke(stroke({{.2,.2},{.25,.25}})));
        QVERIFY(book.appendStroke(stroke({{.6,.6},{.65,.65}})));
        auto selection = book.selectContent(polygon(.1,.1,.3,.3), "contained", {}, "replace");
        QCOMPARE(selection.value("strokes").toList().size(), 1);
        selection = book.selectContent(polygon(.5,.5,.7,.7), "contained", selection, "add");
        QCOMPARE(selection.value("strokes").toList().size(), 2);
        selection = book.selectContent(polygon(.1,.1,.3,.3), "contained", selection, "subtract");
        QCOMPARE(selection.value("indexes").toList(), QVariantList{1});
        QVERIFY(book.eraseStrokes({0}));
        QVERIFY(!book.transformSelection(selection, .01, 0, 1, 0));
        QCOMPARE(book.strokes().size(), 1);
    }

    void handwritingSurvivesReopenAndTextEditing()
    {
        QTemporaryDir temp;
        const QString path = temp.filePath("notebooks.json");
        QVariantList original;
        QString bookId, pageId, itemId;
        {
            Notebook book(path);
            QVERIFY(book.createBook("Handwriting"));
            QVERIFY(book.appendStroke(stroke({{.2,.2},{.3,.25},{.4,.2}})));
            QVERIFY(book.appendStroke(stroke({{.22,.3},{.32,.35}})));
            original = book.strokes();
            bookId = book.bookId(); pageId = book.pageId();
            QVERIFY(book.replaceHandwriting(selectAll(book), "Prima riga\nSeconda riga"));
            QCOMPARE(book.strokes().size(), 0);
            QVariantMap text = book.pageItems().first().toMap();
            itemId = text.value("id").toString();
            QVERIFY(text.contains("handwritingSource"));
            // The editor only submits ordinary text fields. Original ink must be retained.
            text.remove("handwritingSource");
            text.insert("text", "Testo corretto\nSeconda riga");
            QVERIFY(book.savePageItem(text));
            QVERIFY(book.addPage());
            QVERIFY(book.openPage(pageId));
        }
        Notebook reopened(path);
        QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
        QVERIFY(reopened.openBook(bookId));
        QVERIFY(reopened.openPage(pageId));
        QVERIFY(reopened.restoreOriginalHandwriting(itemId));
        QCOMPARE(reopened.strokes(), original);
        QCOMPARE(reopened.pageItems().size(), 0);
        QVERIFY(reopened.undo());
        QCOMPARE(reopened.strokes().size(), 0);
        QCOMPARE(reopened.pageItems().first().toMap().value("text").toString(), QString("Testo corretto\nSeconda riga"));
        QVERIFY(reopened.redo());
        QCOMPARE(reopened.strokes(), original);
    }

    void oversizedRecoveryCannotWriteUnreadableDocument()
    {
        QTemporaryDir temp;
        const QString path = temp.filePath("notebooks.json");
        Notebook book(path);
        QVERIFY(book.createBook("Large ink"));
        QVariantList points;
        points.reserve(120001);
        for (int i = 0; i < 120001; ++i) points.append(point(.3, .4));
        QVERIFY(book.appendStroke({{"tool", "pen"}, {"color", "#222222"}, {"width", 2.5}, {"points", points}}));
        QVERIFY(!book.replaceHandwriting(selectAll(book), "A manual transcript"));
        QCOMPARE(book.strokes().size(), 1);
        QVERIFY(book.pageItems().isEmpty());
        Notebook reopened(path);
        QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
        QCOMPARE(reopened.strokes().size(), 1);
    }

    void mixedSelectionIsAtomicAndUndoable()
    {
        QTemporaryDir temp;
        Notebook book(temp.filePath("notebooks.json"));
        QVERIFY(book.createBook("Mixed"));
        QVERIFY(book.savePageItem({{"type", "text"}, {"text", "Editable"}}));
        QVERIFY(book.savePageItem({{"type", "table"}, {"rows", 2}, {"cols", 2},
                                   {"cells", QVariantList{"10", "20", "=SUM(A1:B1)", "=AVERAGE(A1:B1)"}}}));
        QVERIFY(book.appendStroke(stroke({{.2,.6},{.4,.7}})));
        const auto selection = selectAll(book);
        const auto initialInk = book.strokes();
        const auto initialItems = book.pageItems();
        QCOMPARE(selection.value("items").toList().size(), 2);
        QVERIFY(book.transformSelection(selection, 0, .02, .8, 0));
        QVERIFY(book.strokes() != initialInk);
        QVERIFY(book.pageItems() != initialItems);
        QVERIFY(book.undo());
        QCOMPARE(book.strokes(), initialInk);
        QCOMPARE(book.pageItems(), initialItems);
        QVERIFY(book.redo());
        const auto transformed = selectAll(book);
        QVERIFY(book.removeSelection(transformed));
        QVERIFY(book.strokes().isEmpty());
        QVERIFY(book.pageItems().isEmpty());
        QVERIFY(book.undo());
        QCOMPARE(book.strokes().size(), 1);
        QCOMPARE(book.pageItems().size(), 2);
        // A failed out-of-page transform leaves both ink and editable table cells intact.
        const auto beforeFailure = book.exportBookForTransfer(book.bookId());
        QVERIFY(!book.transformSelection(selectAll(book), 5, 0, 1, 0));
        QCOMPARE(book.exportBookForTransfer(book.bookId()), beforeFailure);
    }

    void deletingMarkdownSelectionKeepsFileValid()
    {
        QTemporaryDir temp;
        const QString path = temp.filePath("notebooks.json");
        Notebook book(path);
        QVERIFY(book.importMarkdownText("# Notes\n\nA paragraph", "Markdown", ""));
        QVERIFY(book.markdownBook());
        QVERIFY(book.removeSelection(selectAll(book)));
        QVERIFY(book.pageItems().isEmpty());
        QVERIFY(book.undo());
        QVERIFY(book.markdownBook());
        QVERIFY(book.removeSelection(selectAll(book)));
        Notebook reopened(path);
        QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
        QVERIFY(reopened.pageItems().isEmpty());
    }

    void originalInkAndLanguageTravelWithBook()
    {
        QTemporaryDir temp;
        Notebook sender(temp.filePath("sender.json"));
        QVERIFY(sender.createBook("English"));
        QVERIFY(sender.setHandwritingLanguage("en"));
        QVERIFY(!sender.setHandwritingLanguage("fr"));
        QVERIFY(sender.appendStroke(stroke({{.3,.3},{.4,.4}})));
        QVERIFY(sender.replaceHandwriting(selectAll(sender), "Recognized text"));
        const QByteArray data = sender.exportBookForTransfer(sender.bookId());
        Notebook receiver(temp.filePath("receiver.json"));
        QVERIFY2(receiver.importFromData(data), qPrintable(receiver.error()));
        QVERIFY(receiver.openBook(sender.bookId()));
        QCOMPARE(receiver.handwritingLanguage(), QString("en"));
        const auto item = receiver.pageItems().first().toMap();
        QVERIFY(receiver.restoreOriginalHandwriting(item.value("id").toString()));
        QCOMPARE(receiver.strokes().size(), 1);
    }

    void rotatedTextSourceFollowsItsPlacement()
    {
        QTemporaryDir temp;
        Notebook book(temp.filePath("notebooks.json"));
        QVERIFY(book.createBook("Transform OCR"));
        QVERIFY(book.appendStroke(stroke({{.4,.4},{.5,.45}})));
        QVERIFY(book.replaceHandwriting(selectAll(book), "Testo"));
        QVERIFY(book.transformSelection(selectAll(book), .02, .02, 1, 15));
        const auto item = book.pageItems().first().toMap();
        QCOMPARE(item.value("rotation").toDouble(), 15.0);
        const auto storedOriginal = item.value("handwritingSource").toMap().value("strokes").toList();
        QVERIFY(book.restoreOriginalHandwriting(item.value("id").toString()));
        QCOMPARE(book.strokes(), storedOriginal);
    }
};

QTEST_MAIN(LassoTests)
#include "lasso_tests.moc"
