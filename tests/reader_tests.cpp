#include "documentreader.h"
#include "epubimport.h"
#include "notebook.h"
#include "pagestore.h"
#include "macdocumentexchange.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QTimer>
#include <QtEndian>
#include <QtTest>
#include <zlib.h>

namespace {
QByteArray makePdf(int count)
{
    QByteArray bytes;
    QBuffer output(&bytes);
    output.open(QIODevice::WriteOnly);
    {
        QPdfWriter writer(&output);
        writer.setResolution(72);
        writer.setPageSize(QPageSize(QSizeF(300, 400), QPageSize::Point));
        QPainter painter(&writer);
        for (int page = 0; page < count; ++page) {
            if (page)
                writer.newPage();
            painter.fillRect(QRect(30, 30, 120, 120), page + 1 == count ? Qt::black : Qt::white);
            painter.drawText(30, 210, QStringLiteral("Page %1").arg(page + 1));
        }
        painter.end();
    }
    return bytes;
}
void put16(QByteArray &bytes, quint16 value) { const auto le = qToLittleEndian(value); bytes.append(reinterpret_cast<const char *>(&le), 2); }
void put32(QByteArray &bytes, quint32 value) { const auto le = qToLittleEndian(value); bytes.append(reinterpret_cast<const char *>(&le), 4); }
QByteArray storedZip(const QList<QPair<QByteArray, QByteArray>> &files)
{
    QByteArray result, central;
    for (const auto &file : files) {
        const quint32 start = result.size();
        const auto sum = crc32(0, reinterpret_cast<const Bytef *>(file.second.constData()), file.second.size());
        put32(result, 0x04034b50); put16(result, 20); put16(result, 0);
        put16(result, 0); put16(result, 0); put16(result, 0); put32(result, sum);
        put32(result, file.second.size()); put32(result, file.second.size()); put16(result, file.first.size()); put16(result, 0);
        result += file.first; result += file.second;
        put32(central, 0x02014b50); put16(central, 20); put16(central, 20); put16(central, 0);
        put16(central, 0); put16(central, 0); put16(central, 0); put32(central, sum);
        put32(central, file.second.size()); put32(central, file.second.size()); put16(central, file.first.size());
        put16(central, 0); put16(central, 0); put16(central, 0); put16(central, 0); put32(central, 0); put32(central, start);
        central += file.first;
    }
    const quint32 offset = result.size();
    result += central;
    put32(result, 0x06054b50); put16(result, 0); put16(result, 0); put16(result, files.size()); put16(result, files.size());
    put32(result, central.size()); put32(result, offset); put16(result, 0);
    return result;
}
QByteArray makeEpub()
{
    QImage illustration(600, 300, QImage::Format_RGB32);
    illustration.fill(Qt::black);
    QByteArray png;
    QBuffer image(&png);
    image.open(QIODevice::WriteOnly);
    illustration.save(&image, "PNG");
    return storedZip({
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml", "<container><rootfiles><rootfile full-path=\"OPS/package.opf\"/></rootfiles></container>"},
        {"OPS/package.opf", "<package><manifest><item id=\"chapter\" href=\"chapter.xhtml\" media-type=\"application/xhtml+xml\"/></manifest><spine><itemref idref=\"chapter\"/></spine></package>"},
        {"OPS/chapter.xhtml", "<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>Blue Team</title></head><body><h1>Blue Team</h1><p>Preserved text and illustration.</p><svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\"><image xlink:href=\"image.png\"/></svg></body></html>"},
        {"OPS/image.png", png}});
}
int darkPixels(const QImage &image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            count += qGray(image.pixel(x, y)) < 80;
    return count;
}
}

class DocumentReaderTest : public QObject
{
    Q_OBJECT
private slots:
    void pdfBeyond150AndRequestedPage()
    {
        QString error;
        const QByteArray pdf = makePdf(161);
        const auto index = inspectPdfData(pdf, error);
#ifndef Q_OS_MACOS
        if (!error.isEmpty() && error.contains(QStringLiteral("non è disponibile")))
            QSKIP("PDFium is absent on this host; this test runs with the tablet runtime and on macOS.");
#endif
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(index.pageCount, 161);
        const QImage first = renderPdfPage(pdf, 0, 800, error);
        QVERIFY2(!first.isNull(), qPrintable(error));
        const QImage last = renderPdfPage(pdf, 160, 1600, error);
        QVERIFY2(!last.isNull(), qPrintable(error));
        QCOMPARE(qMax(last.width(), last.height()), 1600);
        QVERIFY(darkPixels(last) > darkPixels(first) * 5);
        QVERIFY(renderPdfPage(pdf, 161, 800, error).isNull());
        QVERIFY(!error.isEmpty());
    }
    void epubKeepsSvgWrappedImagesAndText()
    {
        QString error;
        const QByteArray pdf = convertEpubToPdf(makeEpub(), error);
        QVERIFY2(!pdf.isEmpty(), qPrintable(error));
        const auto index = inspectPdfData(pdf, error);
#ifndef Q_OS_MACOS
        if (!error.isEmpty() && error.contains(QStringLiteral("non è disponibile")))
            QSKIP("PDFium is absent on this host; vector EPUB conversion succeeded.");
#endif
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(index.pageCount >= 1);
        QVERIFY(index.pageText.join(u' ').contains(QStringLiteral("Blue Team")));
        const QImage page = renderPdfPage(pdf, 0, 1200, error);
        QVERIFY2(!page.isNull(), qPrintable(error));
        QVERIFY(darkPixels(page) > 50000);
    }
    void cancellationAndCorruptSource()
    {
        QString error;
        auto cancelled = std::make_shared<std::atomic_bool>(true);
        QVERIFY(convertEpubToPdf(makeEpub(), error, cancelled).isEmpty());
        QVariantMap source{{QStringLiteral("asset"), QString(64, u'a')}, {QStringLiteral("kind"), QStringLiteral("pdf")},
                           {QStringLiteral("page"), 0}, {QStringLiteral("data"), QString::fromLatin1(makePdf(1).toBase64())}};
        QVERIFY(renderDocumentPage(source, 800, error).isNull());
        QVERIFY(!error.isEmpty());
    }
    void sourceAndAnnotationsSurviveReopenTransferAndPageMove()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString epubPath = directory.filePath(QStringLiteral("illustrated.epub"));
        QFile input(epubPath);
        QVERIFY(input.open(QIODevice::WriteOnly));
        const QByteArray original = makeEpub();
        QCOMPARE(input.write(original), original.size());
        input.close();
        const QString path = directory.filePath(QStringLiteral("library.json"));
        QString importedId;
        QByteArray portable;
        {
            Notebook model(path);
            const bool imported = model.importDocument(QUrl::fromLocalFile(epubPath), {});
#ifndef Q_OS_MACOS
            if (!imported && model.error().contains(QStringLiteral("non è disponibile")))
                QSKIP("PDFium is absent on this host.");
#endif
            QVERIFY2(imported, qPrintable(model.error()));
            importedId = model.bookId();
            QCOMPARE(model.books().last().toMap().value("format").toString(), QStringLiteral("epub"));
            QVERIFY(model.hasPageBackground());
            QVERIFY(model.pageBackground().isEmpty());
            const QVariantMap ink{{"color", "#222222"}, {"width", 2.5}, {"tool", "pen"}, {"points", QVariantList{
                QVariantMap{{"x", .2}, {"y", .2}, {"p", .5}}, QVariantMap{{"x", .4}, {"y", .3}, {"p", .7}}}}};
            QVERIFY(model.appendStroke(ink));
            portable = model.exportBookForTransfer(importedId);
            const auto book = QJsonDocument::fromJson(portable).object().value("books").toArray().first().toObject();
            const auto sources = book.value("sourceDocuments").toObject();
            QCOMPARE(sources.size(), 2);
            const QString origin = QString::fromLatin1(QCryptographicHash::hash(original, QCryptographicHash::Sha256).toHex());
            QCOMPARE(QByteArray::fromBase64(sources.value(origin).toObject().value("data").toString().toLatin1()), original);
            const QByteArray serializedPage = QJsonDocument(book.value("pages").toArray().first().toObject()).toJson();
            QVERIFY(!serializedPage.contains("cacheDirectory"));
            QVERIFY(!serializedPage.contains("background"));
        }
        QVERIFY(QFile::remove(epubPath));
        {
            Notebook reopened(path);
            QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
            QCOMPARE(reopened.bookId(), importedId);
            QCOMPARE(reopened.strokes().size(), 1);
            QString error;
            const auto descriptor = reopened.pageRenderContent().value("source").toMap();
            QVERIFY2(!renderDocumentPage(descriptor, 1200, error).isNull(), qPrintable(error));
            QVERIFY(reopened.createBook("Moved page"));
            const QString destination = reopened.bookId();
            QVERIFY(reopened.openBook(importedId));
            QVERIFY(reopened.transferPage(0, destination));
            QVERIFY(reopened.openBook(destination));
            QVERIFY(reopened.goToPage(1));
            QVERIFY(reopened.hasPageBackground());
            QCOMPARE(reopened.strokes().size(), 1);
            QVERIFY2(!renderDocumentPage(reopened.pageRenderContent().value("source").toMap(), 1200, error).isNull(), qPrintable(error));
        }
        Notebook target(directory.filePath(QStringLiteral("receiver.json")));
        const auto preview = target.receiveBookTransfer(portable);
        QVERIFY2(!preview.contains("error"), qPrintable(preview.value("error").toString()));
        const auto received = target.receiveBookTransfer(portable, preview.value("token").toByteArray());
        QVERIFY2(!received.contains("error"), qPrintable(received.value("error").toString()));
        QCOMPARE(target.strokes().size(), 1);
        QString error;
        QVERIFY2(!renderDocumentPage(target.pageRenderContent().value("source").toMap(), 1200, error).isNull(), qPrintable(error));
    }
    void backgroundImportPreservesCurrentEditAndCancellation()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("incoming.pdf"));
        QFile input(path);
        QVERIFY(input.open(QIODevice::WriteOnly));
        input.write(makePdf(161));
        input.close();
        Notebook model(directory.filePath(QStringLiteral("library.json")));
        QVERIFY(model.createBook("Writing while importing"));
        const QString writingId = model.bookId();
        QSignalSpy completed(&model, &Notebook::documentImported);
        QVERIFY(model.startDocumentImport(QUrl::fromLocalFile(path), {}));
        QVERIFY(model.addPage());
        const QVariantMap ink{{"color", "#222222"}, {"width", 2.5}, {"tool", "pen"}, {"points", QVariantList{
            QVariantMap{{"x", .2}, {"y", .2}, {"p", .5}}, QVariantMap{{"x", .4}, {"y", .3}, {"p", .7}}}}};
        QVERIFY(model.appendStroke(ink));
        QTRY_VERIFY_WITH_TIMEOUT(!model.importing(), 10000);
#ifndef Q_OS_MACOS
        if (model.error().contains(QStringLiteral("non è disponibile")))
            QSKIP("PDFium is absent on this host.");
#endif
        QVERIFY2(model.error().isEmpty(), qPrintable(model.error()));
        QCOMPARE(model.bookId(), writingId);
        QCOMPARE(model.page(), 1);
        QCOMPARE(model.strokes().size(), 1);
        QVERIFY(model.canUndo());
        QCOMPARE(completed.size(), 1);
        QVERIFY(completed.first().first().toString() != writingId);
        const int books = model.books().size();
        QVERIFY(model.startDocumentImport(QUrl::fromLocalFile(path), {}));
        model.cancelDocumentImport();
        QTRY_VERIFY_WITH_TIMEOUT(!model.importing(), 10000);
        QCOMPARE(model.books().size(), books);
        QCOMPARE(model.bookId(), writingId);
        QCOMPARE(completed.size(), 1);
    }
    void asynchronousImportLeavesEventLoopResponsive()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("many-pages.pdf"));
        QFile input(path);
        QVERIFY(input.open(QIODevice::WriteOnly));
        input.write(makePdf(161));
        input.close();
        bool finished = false;
        int ticks = 0;
        QTimer heartbeat;
        heartbeat.setInterval(0);
        connect(&heartbeat, &QTimer::timeout, this, [&] { ++ticks; });
        heartbeat.start();
        PreparedDocument prepared;
        const auto cancel = startDocumentPreparation(path, this, [](int, int) {}, [&](PreparedDocument result) {
            prepared = std::move(result);
            finished = true;
        });
        QVERIFY(!finished);
        QTRY_VERIFY_WITH_TIMEOUT(finished, 10000);
        QVERIFY(ticks > 0);
#ifndef Q_OS_MACOS
        if (!prepared.error.isEmpty() && prepared.error.contains(QStringLiteral("non è disponibile")))
            QSKIP("PDFium is absent on this host; asynchronous error delivery succeeded.");
#endif
        QVERIFY2(prepared.error.isEmpty(), qPrintable(prepared.error));
        QCOMPARE(prepared.pages.size(), 161);
        QCOMPARE(prepared.sources.size(), 1);
    }

    void pageSummariesTrackAnnotationsWithoutBackgroundPayloads()
    {
        QTemporaryDir directory;
        Notebook model(directory.filePath("library.json"));
        QVERIFY(model.createBook("Summary fixture"));
        QVERIFY(model.addPage());
        QVERIFY(model.togglePageBookmark());
        QVERIFY(model.addLayer("Hidden annotations"));
        const auto layer = model.activeLayer();
        const QVariantMap ink{{"color", "#222222"}, {"width", 2.5}, {"tool", "pen"}, {"points", QVariantList{
            QVariantMap{{"x", .2}, {"y", .2}, {"p", .5}}, QVariantMap{{"x", .4}, {"y", .3}, {"p", .7}}}}};
        QVERIFY(model.appendStroke(ink));
        QVERIFY(model.setLayerVisible(layer, false));
        const auto summaries = model.pageSummaries();
        QCOMPARE(summaries.size(), 2);
        QVERIFY(!summaries.first().toMap().value("annotated").toBool());
        QCOMPARE(summaries.last().toMap().value("index").toInt(), 1);
        QCOMPARE(summaries.last().toMap().value("pageId").toString(), model.pageId());
        QVERIFY(summaries.last().toMap().value("bookmarked").toBool());
        QVERIFY(summaries.last().toMap().value("annotated").toBool());
        for (const auto &entry : summaries) {
            const auto summary = entry.toMap();
            QCOMPARE(summary.keys(), QStringList({"annotated", "bookmarked", "index", "pageId", "title", "workspace"}));
            QCOMPARE(summary.value("workspace").toString(), QStringLiteral("writing"));
            QVERIFY(summary.value("title").toString().isEmpty());
        }
        QCOMPARE(model.books().last().toMap().value("format").toString(), QStringLiteral("notebook"));
        QVERIFY(model.importMarkdownText("# Writing", "Markdown fixture", {}));
        QCOMPARE(model.books().last().toMap().value("format").toString(), QStringLiteral("markdown"));
    }

    void importPreviewIdentifiesSkippedAndUnreadableFiles()
    {
        QTemporaryDir directory;
        Notebook model(directory.filePath("library.json"));
        MacDocumentExchange exchange(&model, false);
        QFile input(directory.filePath("valid.md"));
        QVERIFY(input.open(QIODevice::WriteOnly)); input.write("# Fixture"); input.close();
        const QUrl valid = QUrl::fromLocalFile(input.fileName());
        const auto preview = exchange.importPreview({valid, valid, QUrl::fromLocalFile(directory.filePath("missing.pdf")),
                                                   QUrl::fromLocalFile(directory.filePath("unsupported.csv"))});
        QCOMPARE(preview.value("accepted").toInt(), 1);
        QCOMPARE(preview.value("duplicates").toInt(), 1);
        QCOMPARE(preview.value("unreadable").toInt(), 1);
        QCOMPARE(preview.value("unsupported").toInt(), 1);
        QCOMPARE(preview.value("entries").toList().size(), 4);
        QList<QUrl> excessive;
        for (int index = 0; index < 17; ++index) excessive.append(valid);
        QVERIFY(!exchange.importPreview(excessive).value("allowed").toBool());
    }

    void importReportRetriesOnlyFailuresAndCancelsPendingBatch()
    {
#ifndef Q_OS_MACOS
        QSKIP("Finder batch import is available only in the macOS application.");
#endif
        QTemporaryDir directory;
        Notebook model(directory.filePath("library.json"));
        QVERIFY(model.createBook("Existing work"));
        const QVariantMap ink{{"color", "#222222"}, {"width", 2.5}, {"tool", "pen"}, {"points", QVariantList{
            QVariantMap{{"x", .2}, {"y", .2}, {"p", .5}}, QVariantMap{{"x", .4}, {"y", .3}, {"p", .7}}}}};
        QVERIFY(model.appendStroke(ink));
        const int initialBooks = model.books().size();
        const QString activeBook = model.bookId();
        MacDocumentExchange exchange(&model, false);
        QFile input(directory.filePath("valid.md"));
        QVERIFY(input.open(QIODevice::WriteOnly)); input.write("# Fixture"); input.close();
        const QUrl valid = QUrl::fromLocalFile(input.fileName());
        const QUrl missing = QUrl::fromLocalFile(directory.filePath("missing.md"));
        QVERIFY(exchange.importFiles({valid, valid, missing}, {}));
        QTRY_VERIFY_WITH_TIMEOUT(!exchange.importing(), 10000);
        const auto report = exchange.importReport();
        QCOMPARE(report.at(0).toMap().value("state").toString(), QStringLiteral("imported"));
        QCOMPARE(report.at(1).toMap().value("state").toString(), QStringLiteral("duplicate"));
        QCOMPARE(report.at(2).toMap().value("state").toString(), QStringLiteral("failed"));
        QCOMPARE(model.books().size(), initialBooks + 1);
        QCOMPARE(model.bookId(), activeBook);
        QVERIFY(model.canUndo());
        QCOMPARE(model.strokes().size(), 1);
        QVERIFY(exchange.canRetryImports());
        QFile restored(missing.toLocalFile());
        QVERIFY(restored.open(QIODevice::WriteOnly)); restored.write("# Recovered"); restored.close();
        QVERIFY(exchange.retryFailedImports());
        QTRY_VERIFY_WITH_TIMEOUT(!exchange.importing(), 10000);
        QCOMPARE(model.books().size(), initialBooks + 2);
        QCOMPARE(exchange.importReport().at(2).toMap().value("state").toString(), QStringLiteral("imported"));
        QVERIFY(!exchange.canRetryImports());
        QVERIFY(exchange.importFiles({valid, missing}, {}));
        exchange.cancelImports();
        QTRY_VERIFY_WITH_TIMEOUT(!exchange.importing(), 10000);
        QCOMPARE(model.books().size(), initialBooks + 2);
        for (const auto &entry : exchange.importReport())
            QCOMPARE(entry.toMap().value("state").toString(), QStringLiteral("cancelled"));
        exchange.setLocked(true);
        QVERIFY(exchange.importReport().isEmpty());
        QVERIFY(!exchange.retryFailedImports());
    }
};

QTEST_MAIN(DocumentReaderTest)
#include "reader_tests.moc"
