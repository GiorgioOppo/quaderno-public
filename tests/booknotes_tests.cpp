#include "notebook.h"
#include "diagramexport.h"
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
QVariantMap mixedDiagram()
{
    const QVariantMap node{{"id", "step"}, {"shape", "process"}, {"text", "Check"},
        {"x", .2}, {"y", .1}, {"width", .4}, {"height", .3}};
    return {{"type", "diagram"}, {"nodes", QVariantList{node}}, {"edges", QVariantList{}}};
}
QVariantMap mixedTable()
{
    return {{"type", "table"}, {"rows", 2}, {"cols", 2},
        {"cells", QVariantList{"Input", "Value", "2", "=SUM(A2:A2)"}}};
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

    void extendedDiagramShapesRequireNewPortableVersion_data()
    {
        QTest::addColumn<QString>("shape");
        QTest::addColumn<int>("version");
        QTest::newRow("flowchart") << QStringLiteral("manual-input") << NotebookFormat::extendedDiagramShapesVersion;
        QTest::newRow("uml") << QStringLiteral("uml-class") << NotebookFormat::umlDiagramShapesVersion;
    }

    void extendedDiagramShapesRequireNewPortableVersion()
    {
        QFETCH(QString, shape);
        QFETCH(int, version);
        QTemporaryDir directory;
        const auto library = directory.filePath("library.json");
        Notebook notebook(library);
        QVERIFY(notebook.createBookWithItem("Flowchart", {}, mixedDiagram()));
        const auto basic = QJsonDocument::fromJson(notebook.exportBookForTransfer(notebook.bookId())).object();
        QVERIFY(basic.value("version").toInt() < NotebookFormat::extendedDiagramShapesVersion);

        auto diagram = notebook.pageItems().first().toMap();
        auto node = diagram.value("nodes").toList().first().toMap();
        node.insert("shape", shape);
        diagram.insert("nodes", QVariantList{node});
        QVERIFY2(notebook.savePageItem(diagram), qPrintable(notebook.error()));
        const auto saved = notebook.pageItems();

        Notebook reopened(library);
        QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
        QCOMPARE(reopened.pageItems(), saved);

        const auto payload = notebook.exportBookForTransfer(notebook.bookId());
        auto downgraded = QJsonDocument::fromJson(payload).object();
        QCOMPARE(downgraded.value("version").toInt(), version);
        downgraded.insert("version", version - 1);
        QVERIFY(!Notebook::validateBackupData(QJsonDocument(downgraded).toJson()).isEmpty());

        Notebook receiver(directory.filePath("receiver.json"));
        QVERIFY2(receiver.importFromData(payload), qPrintable(receiver.error()));
        QCOMPARE(receiver.pageItems(), saved);
    }

    void shareExportsEditableLinksAndRejectsAChangedPage()
    {
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("library.json"));
        QVERIFY(notebook.createBook("Source"));
        const auto source = notebook.mixedPageSource();
        QVERIFY2(notebook.createMixedDocument(notebook.bookId(), notebook.pageId(), source.value("revision").toString(),
                     "Share sample", "# Testo da condividere", mixedDiagram(), mixedTable()), qPrintable(notebook.error()));
        QCOMPARE(notebook.page(), 0);

        QMap<QString, QVariantMap> options;
        for (const auto &entry : notebook.shareOptions()) {
            const auto option = entry.toMap();
            options.insert(option.value("format").toString(), option);
        }
        const auto save = [&](QString format, QString name) {
            return notebook.exportShareFile(options.value(format), QUrl::fromLocalFile(directory.filePath(name)));
        };
        QVERIFY2(save("drawio", "diagram.drawio"), qPrintable(notebook.error()));
        const auto diagram = DiagramExport().load(QUrl::fromLocalFile(directory.filePath("diagram.drawio")));
        const auto nodes = diagram.value("diagram").toMap().value("nodes").toList();
        QCOMPARE(nodes.size(), 1);
        QCOMPARE(nodes.first().toMap().value("text").toString(), QStringLiteral("Check"));
        QVERIFY2(save("csv", "table.csv"), qPrintable(notebook.error()));
        QVERIFY(readFile(directory.filePath("table.csv")).contains("\"2\",\"2\""));

        QVERIFY2(save("markdown-assets", "writing.md"), qPrintable(notebook.error()));
        const auto markdown = QString::fromUtf8(readFile(directory.filePath("writing.md")));
        QVERIFY(markdown.contains("# Testo da condividere"));
        const auto tableLink = QRegularExpression(QStringLiteral("\\((quaderno-assets-[^/]+/page-\\d+-table-\\d+\\.csv)\\)"))
            .match(markdown);
        QVERIFY(tableLink.hasMatch());
        QVERIFY(readFile(directory.filePath(tableLink.captured(1))).contains("\"2\",\"2\""));

        QVERIFY2(save("png", "page.png"), qPrintable(notebook.error()));
        QCOMPARE(QImage(directory.filePath("page.png")).size(), QSize(1200, 1600));
        QVERIFY2(save("pdf", "book.pdf"), qPrintable(notebook.error()));
        QVERIFY2(save("pdf-page", "page.pdf"), qPrintable(notebook.error()));
        const auto allPages = readFile(directory.filePath("book.pdf"));
        const auto onePage = readFile(directory.filePath("page.pdf"));
        QVERIFY(allPages.startsWith("%PDF"));
        QVERIFY(onePage.startsWith("%PDF"));
        QString pdfError;
        const int fullPageCount = inspectPdfData(allPages, pdfError).pageCount;
        if (fullPageCount > 0) {
            QCOMPARE(fullPageCount, 3);
            QCOMPARE(inspectPdfData(onePage, pdfError).pageCount, 1);
        }

        QVERIFY2(save("quaderno", "share.quaderno"), qPrintable(notebook.error()));
        const auto portable = readFile(directory.filePath("share.quaderno"));
        QCOMPARE(portable, notebook.exportBookForTransfer(notebook.bookId()));
        QVERIFY(notebook.goToPage(1));
        QVERIFY(!save("drawio", "stale.drawio"));
        QVERIFY(!QFile::exists(directory.filePath("stale.drawio")));
        if (fullPageCount > 0) {
            QVERIFY2(notebook.importDocument(QUrl::fromLocalFile(directory.filePath("book.pdf")), {}),
                     qPrintable(notebook.error()));
            for (const auto &entry : notebook.shareOptions()) {
                const auto option = entry.toMap();
                options.insert(option.value("format").toString(), option);
            }
            QVERIFY2(save("source", "original.pdf"), qPrintable(notebook.error()));
            QCOMPARE(readFile(directory.filePath("original.pdf")), allPages);
        }
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

    void pageLinkPersistsAndOpensExactObjectWithoutCopying()
    {
        QTemporaryDir directory;
        const QString file = directory.filePath("library.json");
        Notebook notebook(file);
        const QVariantMap table{{"type", "table"}, {"rows", 2}, {"cols", 2},
            {"cells", QVariantList{"Year", "Value", "2025", "12"}}};
        QVERIFY(notebook.createBookWithItem("Reference table", {}, table));
        const QVariantMap destination = notebook.libraryItems().first().toMap();
        const auto targetBook = notebook.bookId();
        const auto targetPage = notebook.pageId();
        const auto targetId = destination.value("itemId").toString();
        QVERIFY(notebook.createBook("Reader"));
        const auto sourceBook = notebook.bookId();
        const auto sourcePage = notebook.pageId();
        QVERIFY(!notebook.savePageLink(destination, {.4, .6}, sourceBook, targetPage));
        QVERIFY(!notebook.savePageLink(destination, {-.4, .6}, sourceBook, sourcePage));
        QVERIFY(notebook.savePageLink(destination, {.4, .6}, sourceBook, sourcePage));
        const QVariantMap marker = notebook.pageItems().first().toMap();
        QCOMPARE(marker.value("type").toString(), QStringLiteral("link"));
        QCOMPARE(marker.value("targetItemId").toString(), targetId);
        const auto markerId = marker.value("id").toString();
        QVERIFY(notebook.removePageLink(markerId, sourceBook, sourcePage));
        QVERIFY(notebook.pageItems().isEmpty());
        QVERIFY(notebook.undo());
        QCOMPARE(notebook.pageItems(), QVariantList{marker});

        Notebook reopened(file);
        QVERIFY(reopened.openBook(sourceBook));
        QCOMPARE(reopened.pageItems(), QVariantList{marker});
        const auto item = reopened.openPageLink(markerId);
        QCOMPARE(reopened.bookId(), targetBook);
        QCOMPARE(reopened.pageId(), targetPage);
        QCOMPARE(item.value("id").toString(), targetId);
        QCOMPARE(reopened.libraryItems().size(), 1);
    }

    void transfersKeepMissingLinkTargetsAndRequireLinkFormat()
    {
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("sender.json"));
        const QVariantMap table{{"type", "table"}, {"rows", 1}, {"cols", 1}, {"cells", QVariantList{"Value"}}};
        QVERIFY(notebook.createBookWithItem("Target", {}, table));
        const QVariantMap destination = notebook.libraryItems().first().toMap();
        const auto targetBook = notebook.bookId();
        QVERIFY(notebook.createBook("Reader"));
        const auto sourceBook = notebook.bookId();
        const auto sourcePage = notebook.pageId();
        QVERIFY(notebook.savePageLink(destination, {1, 1}, sourceBook, sourcePage));
        const QVariantMap marker = notebook.pageItems().first().toMap();
        const auto markerId = marker.value("id").toString();
        auto malformed = marker;
        malformed.insert("targetPageId", QStringLiteral("file:///tmp/external-file"));
        QVERIFY(PageContent::validatedItem(malformed).isEmpty());

        const auto portable = notebook.exportBookForTransfer(sourceBook);
        QJsonObject claimedOlder = QJsonDocument::fromJson(portable).object();
        QCOMPARE(claimedOlder.value("version").toInt(), NotebookFormat::pageLinksVersion);
        claimedOlder.insert("version", NotebookFormat::pageLinksVersion - 1);
        QVERIFY(!Notebook::validateBackupData(QJsonDocument(claimedOlder).toJson()).isEmpty());

        Notebook receiver(directory.filePath("receiver.json"));
        QVERIFY2(receiver.importFromData(portable), qPrintable(receiver.error()));
        QVERIFY(receiver.openBook(sourceBook));
        QCOMPARE(receiver.pageItems().first().toMap(), marker);
        QVERIFY(receiver.openPageLink(markerId).isEmpty());
        QCOMPARE(receiver.bookId(), sourceBook);
        QCOMPARE(receiver.pageId(), sourcePage);
        QVERIFY(!receiver.error().isEmpty());

        const auto targetTransfer = notebook.exportBookForTransfer(targetBook);
        const auto preview = receiver.receiveBookTransfer(targetTransfer);
        QCOMPARE(preview.value("action").toString(), QStringLiteral("create"));
        const auto received = receiver.receiveBookTransfer(targetTransfer, preview.value("token").toByteArray());
        QVERIFY2(!received.contains("error"), qPrintable(received.value("error").toString()));
        QVERIFY(receiver.openBook(sourceBook));
        QCOMPARE(receiver.openPageLink(markerId).value("id"), marker.value("targetItemId"));
        QCOMPARE(receiver.bookId(), targetBook);
    }

    void mixedDocumentKeepsEditableTargetsInOneTransferableBook()
    {
        QTemporaryDir directory;
        const QString file = directory.filePath("library.json");
        Notebook notebook(file);
        QVERIFY(notebook.createFolder("Examples", {}));
        const auto folderId = notebook.folders().first().toMap().value("id").toString();

        QVERIFY(notebook.createBookWithItem("Previous diagram", folderId, mixedDiagram()));
        const auto previousDiagram = notebook.libraryItems().first().toMap();
        QVERIFY(notebook.createBookWithItem("Previous table", folderId, mixedTable()));
        const auto previousTableId = notebook.bookId();
        QVariantMap previousTable;
        for (const auto &entry : notebook.libraryItems()) {
            const auto item = entry.toMap();
            if (item.value("bookId").toString() == previousTableId)
                previousTable = item;
        }
        QVERIFY(notebook.importMarkdownText("Previous edited text", "Previous writing", folderId));
        const auto previousWriting = notebook.bookId();
        QVERIFY(notebook.savePageLink(previousDiagram, {.09, .966}, previousWriting, notebook.pageId()));
        QVERIFY(notebook.savePageLink(previousTable, {.17, .966}, previousWriting, notebook.pageId()));

        QVERIFY(notebook.createBookInFolder("Scanned notes", folderId));
        QVERIFY(notebook.appendStroke(stroke()));
        const auto sourceBook = notebook.bookId();
        const auto sourcePage = notebook.pageId();
        QMap<QString, QByteArray> originals;
        for (const auto &value : notebook.books()) {
            const auto id = value.toMap().value("id").toString();
            const auto exported = notebook.exportBookForTransfer(id);
            QVERIFY(!exported.isEmpty());
            originals.insert(id, exported);
        }
        const auto source = notebook.mixedPageSource();
        QCOMPARE(source.value("bookId").toString(), sourceBook);
        QCOMPARE(source.value("pageId").toString(), sourcePage);
        QCOMPARE(source.value("content").toMap(), notebook.pagePreview(0));
        const QString revision = source.value("revision").toString();
        QVERIFY(!revision.isEmpty());

        const QString markdown = QStringLiteral("# Procedure\n\nCheck the editable diagram and table.");
        const qsizetype originalCount = notebook.books().size();
        QSignalSpy commits(&notebook, &Notebook::booksChanged);
        QVERIFY2(notebook.createMixedDocument(sourceBook, sourcePage, revision, "Procedure", markdown,
                                             mixedDiagram(), mixedTable()), qPrintable(notebook.error()));
        QCOMPARE(commits.count(), 1);
        QCOMPARE(notebook.books().size(), originalCount + 1);
        QCOMPARE(notebook.books().last().toMap().value("folderId").toString(), folderId);
        QCOMPARE(notebook.pageCount(), 3);
        QCOMPARE(notebook.markdownText(), markdown);
        for (auto entry = originals.cbegin(); entry != originals.cend(); ++entry)
            QCOMPARE(notebook.exportBookForTransfer(entry.key()), entry.value());
        const auto writingBook = notebook.bookId();
        const auto writingPage = notebook.pageId();

        const auto pageItems = notebook.pageItems();
        QCOMPARE(pageItems.size(), 3);
        const auto text = pageItems.at(0).toMap();
        QCOMPARE(text.value("type").toString(), QStringLiteral("text"));
        QMap<QString, QVariantMap> links;
        QSet<int> targetPages;
        for (const auto &value : pageItems.sliced(1)) {
            const auto link = value.toMap();
            QCOMPARE(link.value("type").toString(), QStringLiteral("link"));
            QCOMPARE(link.value("targetBookId").toString(), writingBook);
            QVERIFY(link.value("y").toDouble() > text.value("y").toDouble() + text.value("height").toDouble());
            const auto target = notebook.linkedPageItem(link.value("id").toString());
            QCOMPARE(target.value("type"), link.value("targetType"));
            QCOMPARE(target.value("id"), link.value("targetItemId"));
            QCOMPARE(target.value("bookId"), link.value("targetBookId"));
            QCOMPARE(target.value("pageId"), link.value("targetPageId"));
            const int number = notebook.bookPageNumber(writingBook, target.value("pageId").toString());
            QCOMPARE(target.value("pageNumber").toInt(), number);
            QCOMPARE(number, target.value("type").toString() == QStringLiteral("diagram") ? 2 : 3);
            targetPages.insert(number);
            QVERIFY(!target.value("displayTitle").toString().isEmpty());
            links.insert(target.value("type").toString(), link);
        }
        QCOMPARE(targetPages, (QSet<int>{2, 3}));
        QCOMPARE(links.keys(), (QStringList{QStringLiteral("diagram"), QStringLiteral("table")}));
        const auto diagramLink = links.value("diagram");
        const auto tableLink = links.value("table");
        const auto diagramLinkId = diagramLink.value("id").toString();
        const auto tableLinkId = tableLink.value("id").toString();
        QVERIFY(notebook.linkedPageItem("missing").isEmpty());
        QCOMPARE(notebook.bookId(), writingBook);
        QCOMPARE(notebook.pageId(), writingPage);

        Notebook reopened(file);
        QCOMPARE(reopened.pageItems(), pageItems);
        auto diagram = reopened.openPageLink(diagramLinkId);
        QCOMPARE(reopened.bookId(), writingBook);
        QCOMPARE(reopened.pageId(), diagramLink.value("targetPageId").toString());
        QCOMPARE(diagram.value("nodes"), mixedDiagram().value("nodes"));
        auto nodes = diagram.value("nodes").toList();
        auto node = nodes.first().toMap();
        node.insert("text", QStringLiteral("Verified"));
        nodes[0] = node;
        diagram.insert("nodes", nodes);
        QVERIFY2(reopened.savePageItem(diagram), qPrintable(reopened.error()));

        QVERIFY(reopened.openPage(writingPage));
        auto table = reopened.openPageLink(tableLinkId);
        QCOMPARE(reopened.bookId(), writingBook);
        QCOMPARE(reopened.pageId(), tableLink.value("targetPageId").toString());
        auto cells = table.value("cells").toList();
        QCOMPARE(cells, mixedTable().value("cells").toList());
        cells[2] = QStringLiteral("12");
        table.insert("cells", cells);
        QVERIFY2(reopened.savePageItem(table), qPrintable(reopened.error()));
        QVERIFY(reopened.openPage(writingPage));
        QCOMPARE(reopened.linkedPageItem(tableLinkId).value("cells").toList(), cells);
        QVERIFY(reopened.updateMarkdownText(QStringLiteral("Updated procedure")));
        QCOMPARE(reopened.linkedPageItem(tableLinkId).value("cells").toList(), cells);
        QCOMPARE(reopened.linkedPageItem(diagramLinkId).value("nodes").toList(), nodes);

        const auto document = reopened.exportBookForTransfer(writingBook);
        const auto portable = QJsonDocument::fromJson(document).object();
        QCOMPARE(portable.value("version").toInt(), NotebookFormat::pageLinksVersion);
        const auto books = portable.value("books").toArray();
        QCOMPARE(books.size(), 1);
        QCOMPARE(books.first().toObject().value("pages").toArray().size(), 3);
        Notebook receiver(directory.filePath("receiver.json"));
        const auto preview = receiver.receiveBookTransfer(document);
        QCOMPARE(preview.value("action").toString(), QStringLiteral("create"));
        QCOMPARE(preview.value("sourcePages").toInt(), 3);
        const auto received = receiver.receiveBookTransfer(document, preview.value("token").toByteArray());
        QVERIFY2(!received.contains("error"), qPrintable(received.value("error").toString()));
        QCOMPARE(receiver.books().size(), 1);
        QCOMPARE(receiver.bookId(), writingBook);
        QCOMPARE(receiver.pageId(), writingPage);
        QCOMPARE(receiver.markdownText(), QStringLiteral("Updated procedure"));
        const auto transferredTable = receiver.linkedPageItem(tableLinkId);
        QCOMPARE(transferredTable.value("cells").toList(), cells);
        QCOMPARE(receiver.tableCellValues(transferredTable).at(3).toString(), QStringLiteral("12"));
        QCOMPARE(receiver.linkedPageItem(diagramLinkId).value("nodes").toList(), nodes);
        for (const auto &value : links) {
            QVERIFY(!receiver.openPageLink(value.value("id").toString()).isEmpty());
            QCOMPARE(receiver.bookId(), writingBook);
            QCOMPARE(receiver.pageId(), value.value("targetPageId").toString());
            QVERIFY(receiver.openPage(writingPage));
        }
        for (auto entry = originals.cbegin(); entry != originals.cend(); ++entry)
            QCOMPARE(reopened.exportBookForTransfer(entry.key()), entry.value());
    }

    void copiedMixedBookLinksResolveToTheirOwnPages_data()
    {
        QTest::addColumn<QString>("operation");
        QTest::newRow("duplicate") << QStringLiteral("duplicate");
        QTest::newRow("transfer-conflict") << QStringLiteral("transfer");
        QTest::newRow("file-import-conflict") << QStringLiteral("import");
    }

    void copiedMixedBookLinksResolveToTheirOwnPages()
    {
        QFETCH(QString, operation);
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("library.json"));
        QVERIFY(notebook.createBookWithItem("External table", {}, mixedTable()));
        const auto externalTable = notebook.libraryItems().first().toMap();
        QVERIFY(notebook.createBook("Source"));
        const auto source = notebook.mixedPageSource();
        QVERIFY(notebook.createMixedDocument(notebook.bookId(), notebook.pageId(), source.value("revision").toString(),
                                             "Mixed", "Text", mixedDiagram(), mixedTable()));
        const auto originalBook = notebook.bookId();
        QVERIFY(notebook.savePageLink(externalTable, {.25, .966}, originalBook, notebook.pageId()));
        const auto links = notebook.pageItems().sliced(1);
        QCOMPARE(links.size(), 3);
        const auto incoming = notebook.exportBookForTransfer(originalBook);
        QVERIFY(!incoming.isEmpty());

        if (operation == QStringLiteral("duplicate")) {
            QVERIFY(notebook.duplicateBook(originalBook));
        } else {
            QVERIFY(notebook.updateMarkdownText(QStringLiteral("Local edit")));
            if (operation == QStringLiteral("import")) {
                QVERIFY(notebook.importFromData(incoming));
            } else {
                const auto preview = notebook.receiveBookTransfer(incoming);
                QCOMPARE(preview.value("action").toString(), QStringLiteral("conflict"));
                const auto result = notebook.receiveBookTransfer(incoming, preview.value("token").toByteArray());
                QVERIFY2(!result.contains("error"), qPrintable(result.value("error").toString()));
            }
        }
        QCOMPARE(notebook.books().size(), 4);
        const auto copy = notebook.books().last().toMap().value("id").toString();
        QVERIFY(copy != originalBook);
        QCOMPARE(notebook.markdownTextForBook(originalBook), operation == QStringLiteral("duplicate")
            ? QStringLiteral("Text") : QStringLiteral("Local edit"));
        QVERIFY(notebook.openBook(copy));
        QCOMPARE(notebook.markdownText(), QStringLiteral("Text"));
        QCOMPARE(notebook.pageCount(), 3);
        const auto rewrittenLinks = notebook.pageItems().sliced(1);
        QCOMPARE(rewrittenLinks.size(), links.size());
        for (qsizetype index = 0; index < links.size(); ++index) {
            const auto before = links.at(index).toMap();
            const auto after = rewrittenLinks.at(index).toMap();
            if (before.value("targetBookId").toString() == originalBook) {
                QCOMPARE(after.value("targetBookId").toString(), copy);
                QVERIFY(after.value("targetPageId") != before.value("targetPageId"));
                const auto resolved = notebook.linkedPageItem(after.value("id").toString());
                QCOMPARE(resolved.value("bookId").toString(), copy);
                QCOMPARE(resolved.value("pageId"), after.value("targetPageId"));
                QCOMPARE(resolved.value("type"), before.value("targetType"));
            } else {
                QCOMPARE(after, before);
            }
        }
    }

    void mixedDocumentRejectsStaleOrInvalidDraftsWithoutWrites()
    {
        QTemporaryDir directory;
        const QString file = directory.filePath("library.json");
        Notebook notebook(file);
        QVERIFY(notebook.createBook("Scanned notes"));
        const auto sourceBook = notebook.bookId();
        const auto sourcePage = notebook.pageId();
        const auto earlier = notebook.mixedPageSource().value("revision").toString();
        QVERIFY(notebook.appendStroke(stroke()));
        const auto current = notebook.mixedPageSource().value("revision").toString();
        QVERIFY(current != earlier);
        const auto library = notebook.exportLibraryForTransfer();
        const auto manifest = readFile(file);
        QSignalSpy changes(&notebook, &Notebook::booksChanged);
        QVERIFY(!notebook.createMixedDocument(sourceBook, sourcePage, earlier, "Mixed", "Text", mixedDiagram(), mixedTable()));
        auto invalidTable = mixedTable();
        invalidTable.insert("rows", 3);
        QVERIFY(!notebook.createMixedDocument(sourceBook, sourcePage, current, "Mixed", "Text", mixedDiagram(), invalidTable));
        QCOMPARE(changes.count(), 0);
        QCOMPARE(notebook.exportLibraryForTransfer(), library);
        QCOMPARE(readFile(file), manifest);
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
