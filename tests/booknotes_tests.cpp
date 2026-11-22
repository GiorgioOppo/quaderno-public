#include "notebook.h"
#include "pagecontent.h"
#include "inkselection.h"
#include "pdfimport.h"
#include "notebookformat.h"

#include <QFile>
#include <QDir>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>

namespace {
QVariantMap stroke()
{
    return {{"tool", "pen"}, {"color", "#202833"}, {"width", 3.0},
        {"points", QVariantList{QVariantMap{{"x", .1}, {"y", .2}, {"p", .6}},
                                 QVariantMap{{"x", .85}, {"y", .8}, {"p", .7}}}}};
}
QVariantMap draft(QString text = QStringLiteral("A note at this paragraph"))
{
    return {{"type", "note"}, {"x", .63}, {"y", .42}, {"width", .05}, {"height", .0375},
        {"text", text}, {"noteColor", "blue"}, {"noteInk", QVariantList{stroke()}}};
}
QVariantMap storedNote()
{
    auto note = draft(); note.insert("id", "test-note"); return note;
}
QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
}

class BookNotesTests : public QObject
{
    Q_OBJECT
private slots:
    void anchoredMixedNotePersistsSearchesAndUsesFormatEleven()
    {
        QTemporaryDir directory;
        const QString file = directory.filePath("library.json");
        QVariantMap original;
        QByteArray portable;
        {
            Notebook notebook(file);
            QVERIFY(notebook.createBook("Annotated book"));
            QVERIFY(notebook.savePageNote(draft(), notebook.bookId(), notebook.pageId()));
            QCOMPARE(notebook.pageItems().size(), 1);
            original = notebook.pageItems().first().toMap();
            QCOMPARE(original.value("x").toDouble(), .63);
            QCOMPARE(original.value("y").toDouble(), .42);
            QCOMPARE(original.value("noteInk").toList(), QVariantList{stroke()});
            QCOMPARE(notebook.searchPages("paragraph").size(), 1);
            QVERIFY(notebook.pageSummaries().first().toMap().value("annotated").toBool());
            portable = notebook.exportBookForTransfer(notebook.bookId());
            QCOMPARE(QJsonDocument::fromJson(portable).object().value("version").toInt(), 11);
        }
        Notebook reopened(file);
        QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
        QCOMPARE(reopened.pageItems(), QVariantList{original});
        Notebook receiver(directory.filePath("receiver.json"));
        QVERIFY2(receiver.importFromData(portable), qPrintable(receiver.error()));
        QCOMPARE(receiver.pageItems(), QVariantList{original});
        auto downgraded = QJsonDocument::fromJson(portable).object();
        downgraded.insert("version", 10);
        const auto before = receiver.exportLibraryForTransfer();
        QVERIFY(!receiver.importFromData(QJsonDocument(downgraded).toJson()));
        QCOMPARE(receiver.exportLibraryForTransfer(), before);
        QVERIFY(NotebookFormat::maximumSupportedVersion >= 11);
    }

    void createEditMoveDeleteAreUndoableAndKeepLocalHandwriting()
    {
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("library.json"));
        QVERIFY(notebook.createBook("Undo notes"));
        const auto book = notebook.bookId(), page = notebook.pageId();
        QVERIFY(notebook.savePageNote(draft(), book, page));
        const auto original = notebook.pageItems().first().toMap();
        const auto id = original.value("id").toString();
        QVERIFY(notebook.undo()); QVERIFY(notebook.pageItems().isEmpty());
        QVERIFY(notebook.redo()); QCOMPARE(notebook.pageItems(), QVariantList{original});
        auto edited = original;
        edited.insert("text", "Keyboard edit"); edited.insert("x", .1); edited.insert("width", .4);
        QVERIFY(notebook.savePageNote(edited, book, page, original));
        const auto changed = notebook.pageItems().first().toMap();
        QCOMPARE(changed.value("text").toString(), QString("Keyboard edit"));
        QCOMPARE(changed.value("x"), original.value("x"));
        QCOMPARE(changed.value("width"), original.value("width"));
        QVERIFY(notebook.undo()); QCOMPARE(notebook.pageItems(), QVariantList{original});
        QVERIFY(notebook.redo()); QCOMPARE(notebook.pageItems(), QVariantList{changed});
        QVERIFY(notebook.movePageItem(id, .2, .3));
        const auto moved = notebook.pageItems().first().toMap();
        QCOMPARE(moved.value("noteInk"), original.value("noteInk"));
        QCOMPARE(moved.value("x").toDouble(), .2);
        QVERIFY(notebook.undo()); QCOMPARE(notebook.pageItems(), QVariantList{changed});
        QVERIFY(notebook.redo()); QCOMPARE(notebook.pageItems(), QVariantList{moved});
        QVERIFY(notebook.removePageNote(id, book, page, moved));
        QVERIFY(notebook.pageItems().isEmpty());
        QVERIFY(notebook.undo()); QCOMPARE(notebook.pageItems(), QVariantList{moved});
        QVERIFY(notebook.redo()); QVERIFY(notebook.pageItems().isEmpty());
    }

    void staleAndCrossPageDraftsNeverOverwriteNotes()
    {
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("library.json"));
        QVERIFY(notebook.createBook("Guarded notes"));
        const auto book = notebook.bookId(), page = notebook.pageId();
        QVERIFY(notebook.savePageNote(draft(), book, page));
        const auto original = notebook.pageItems().first().toMap();
        auto changed = original; changed.insert("text", "Newer content");
        QVERIFY(notebook.savePageNote(changed, book, page, original));
        const auto before = notebook.exportLibraryForTransfer();
        QVERIFY(!notebook.savePageNote(original, book, page, original));
        QVERIFY(!notebook.removePageNote(original.value("id").toString(), book, page, original));
        QCOMPARE(notebook.exportLibraryForTransfer(), before);
        QVERIFY(notebook.addPage());
        const auto otherPage = notebook.exportLibraryForTransfer();
        QVERIFY(!notebook.savePageNote(draft(), book, page));
        QVERIFY(!notebook.savePageNote(changed, book, page, changed));
        QVERIFY(!notebook.removePageNote(original.value("id").toString(), book, page, changed));
        QCOMPARE(notebook.exportLibraryForTransfer(), otherPage);
        QVERIFY(notebook.goToPage(0));
        QVERIFY(notebook.setLayerVisible(notebook.activeLayer(), false));
        const auto hidden = notebook.exportLibraryForTransfer();
        QVERIFY(!notebook.savePageNote(draft(), notebook.bookId(), notebook.pageId()));
        QCOMPARE(notebook.exportLibraryForTransfer(), hidden);
    }

    void validatesBoundedLocalInkAndRejectsMalformedNotes_data()
    {
        QTest::addColumn<QString>("scenario");
        for (const char *name : {"empty", "too-long-text", "invalid-color", "too-many-strokes", "too-many-points",
                "nonnumeric-x", "nan-pressure", "outside-ink", "eraser-stroke", "nested-original", "invalid-rect"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }

    void validatesBoundedLocalInkAndRejectsMalformedNotes()
    {
        QFETCH(QString, scenario);
        auto note = storedNote();
        QVERIFY(!PageContent::validatedItem(note).isEmpty());
        if (scenario == "empty") { note.insert("text", "  "); note.insert("noteInk", QVariantList{}); }
        if (scenario == "too-long-text") note.insert("text", QString(4001, u'x'));
        if (scenario == "invalid-color") note.insert("noteColor", "orange");
        if (scenario == "invalid-rect") note.insert("x", 1.0);
        QVariantList ink = note.value("noteInk").toList();
        if (scenario == "too-many-strokes") {
            ink.clear(); for (int i = 0; i < 2001; ++i) ink.append(stroke());
        } else if (!ink.isEmpty()) {
            auto first = ink.first().toMap();
            auto points = first.value("points").toList();
            if (scenario == "too-many-points") {
                const auto point = points.first(); points.clear();
                for (int i = 0; i < 60001; ++i) points.append(point);
            }
            auto point = points.first().toMap();
            if (scenario == "nonnumeric-x") point.insert("x", "0.1");
            if (scenario == "nan-pressure") point.insert("p", std::numeric_limits<double>::quiet_NaN());
            if (scenario == "outside-ink") point.insert("y", 1.1);
            points[0] = point;
            first.insert("points", points);
            if (scenario == "eraser-stroke") first.insert("tool", "eraser");
            if (scenario == "nested-original") first.insert("original", stroke());
            ink[0] = first;
        }
        note.insert("noteInk", ink);
        QVERIFY(PageContent::validatedItem(note).isEmpty());
    }

    void noteMarkerTransformsWithoutChangingLocalContents()
    {
        const auto note = PageContent::validatedItem(storedNote());
        const auto moved = InkSelectionGeometry::transformedItems({note}, {.65, .44}, {-.2, -.1}, 1.2, 20);
        QCOMPARE(moved.size(), 1);
        const auto after = moved.first().toMap();
        QCOMPARE(after.value("noteInk"), note.value("noteInk"));
        QCOMPARE(after.value("text"), note.value("text"));
        QVERIFY(after.value("x") != note.value("x"));
        QImage image(QSize(600, 800), QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white);
        QPainter painter(&image); PageContent::paintItems(painter, image.size(), {note}); painter.end();
        QVERIFY(image.pixelColor(385, 340) != QColor(Qt::white));
        QCOMPARE(image.pixelColor(20, 20), QColor(Qt::white));
    }

    void pdfAppendicesKeepLongTextAndHandwritingReadable()
    {
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("library.json"));
        QVERIFY(notebook.createBook("Export notes"));
        QVERIFY(notebook.setPageTemplate("blank"));
        QString text;
        for (int line = 1; line < 300; ++line) text += QStringLiteral("Line %1\n").arg(line);
        text += QStringLiteral("Final note line 300");
        QVERIFY(text.size() <= 4000);
        QVERIFY(notebook.savePageNote(draft(text), notebook.bookId(), notebook.pageId()));
        const QString path = directory.filePath("notes.pdf");
        QVERIFY2(notebook.exportPdf(path), qPrintable(notebook.error()));
        QString error;
        const auto pdf = readFile(path);
        const auto info = inspectPdfData(pdf, error);
#ifndef Q_OS_MACOS
        if (!error.isEmpty() && error.contains(QStringLiteral("non è disponibile")))
            QSKIP("PDFium is unavailable on this host; tablet runtime and macOS validate exported note appendices.");
#endif
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(info.pageCount > 5);
        const QString extracted = info.pageText.join('\n');
        // PDFKit and PDFium can infer different spaces between adjacent PDF glyphs.
        QVERIFY(extracted.contains(QRegularExpression(QStringLiteral("\\bFinal\\s*note\\s*line\\s*300\\b"))));
        for (int line = 1; line < 300; ++line) {
            auto matches = QRegularExpression(QStringLiteral("\\bLine\\s*%1\\b").arg(line)).globalMatch(extracted);
            int count = 0;
            while (matches.hasNext()) { matches.next(); ++count; }
            QCOMPARE(count, 1);
        }
        const auto inkPage = renderPdfPage(pdf, info.pageCount - 1, 800, error);
        QVERIFY2(!inkPage.isNull(), qPrintable(error));
        int dark = 0;
        for (int y = inkPage.height() / 5; y < inkPage.height() * 3 / 5; ++y)
            for (int x = inkPage.width() / 6; x < inkPage.width() * 5 / 6; ++x)
                dark += qGray(inkPage.pixel(x, y)) < 100;
        QVERIFY(dark > 150);
        QVERIFY(notebook.addPage());
        const QString only = directory.filePath("current.pdf");
        QVERIFY(notebook.exportPdf(only, true));
        QCOMPARE(inspectPdfData(readFile(only), error).pageCount, 1);
    }

    void markdownExportsNoteTextAndLocalInkAssets()
    {
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("library.json"));
        QVERIFY(notebook.createBook("Markdown note export"));
        QVERIFY(notebook.savePageNote(draft("Remember this detail"), notebook.bookId(), notebook.pageId()));
        const QUrl plain = QUrl::fromLocalFile(directory.filePath("plain.md"));
        QVERIFY(notebook.exportMarkdown(plain));
        QVERIFY(readFile(plain.toLocalFile()).contains("Remember this detail"));
        const QUrl full = QUrl::fromLocalFile(directory.filePath("with-assets.md"));
        QVERIFY2(notebook.exportMarkdownWithAssets(full), qPrintable(notebook.error()));
        const auto markdown = readFile(full.toLocalFile());
        QVERIFY(markdown.contains("Remember this detail"));
        QVERIFY(markdown.contains("page-1-note-1.png"));
        const auto folders = QDir(directory.path()).entryList({"quaderno-assets-*"}, QDir::Dirs);
        QCOMPARE(folders.size(), 1);
        const QImage image(directory.filePath(folders.first() + "/page-1-note-1.png"));
        QCOMPARE(image.size(), QSize(900, 600));
    }
};

QTEST_MAIN(BookNotesTests)
#include "booknotes_tests.moc"
