#include "notebook.h"
#include "pagestore.h"
#include "syncpackage.h"
#include "pagecontent.h"
#include "paperprostandby.h"
#include "pinlock.h"
#include "deviceinfo.h"
#include "appleshare.h"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QSet>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

namespace {
QByteArray readBytes(const QString &path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
bool writeBytes(const QString &path, const QByteArray &bytes) { QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); }
QVariantMap stroke(double y = .2) {
    return {{"color", "#222222"}, {"width", 2.5}, {"tool", "pen"}, {"points", QVariantList{
        QVariantMap{{"x", .2}, {"y", y}, {"p", .5}}, QVariantMap{{"x", .4}, {"y", y + .1}, {"p", .7}}}}};
}
QVariantMap table() {
    return {{"type", "table"}, {"x", .1}, {"y", .1}, {"width", .8}, {"height", .2},
        {"rows", 2}, {"cols", 3}, {"cells", QVariantList{"-2", "4", "=SUM(A1:B1)", "6", "=AVERAGE(A1:A2)", "=MEDIA(A1:B1)"}}};
}
}

class RegressionTests : public QObject {
    Q_OBJECT
private slots:
    void expandedTableAggregatesAndErrorsRemainEditable() {
        QVariantMap draft{{"type","table"},{"rows",4},{"cols",3},{"cells",QVariantList{
            "-5", "text", "=MIN(A1:A3)",
            "2.5", "", "=MAX(A1:A3)",
            "10", "", "=CONTA.NUMERI(A1:B3)",
            "=COUNT(B1:B3)", "=MIN(B1:B3)", "=SUM(C1:C3)"}}};
        const auto values = PageContent::resolvedTableCells(draft);
        QCOMPARE(values[2].toString(), QString("-5"));
        QCOMPARE(values[5].toString(), QString("10"));
        QCOMPARE(values[8].toString(), QString("3"));
        QCOMPARE(values[9].toString(), QString("0"));
        QCOMPARE(values[10].toString(), QString("0"));
        QCOMPARE(values[11].toString(), QString("8"));
        QTemporaryDir dir;
        const QString path = dir.filePath("library.json");
        { Notebook model(path); QVERIFY(model.createBook("Aggregate functions")); QVERIFY(model.savePageItem(draft)); }
        Notebook reopened(path);
        QCOMPARE(reopened.pageItems()[0].toMap().value("cells"), draft.value("cells"));
        QCOMPARE(reopened.tableCellValues(reopened.pageItems()[0].toMap()), values);
        draft["rows"] = 2;
        draft["cols"] = 3;
        draft["cells"] = QVariantList{"=MIN(A1:A2)", "=MAX(Z1:Z2)", "=COUNT(A1)", "4", "=MEAN(A2)", "=AVERAGE(B2)"};
        const auto failures = PageContent::resolvedTableCells(draft);
        QCOMPARE(failures[0].toString(), QString("#CYCLE!"));
        QCOMPARE(failures[1].toString(), QString("#REF!"));
        QCOMPARE(failures[2].toString(), QString("#CYCLE!"));
        QCOMPARE(failures[4].toString(), QString("#NAME?"));
        QCOMPARE(failures[5].toString(), QString("#NAME?"));
    }
    void matchingLegacyReadingPositionIsPreserved() {
        QTemporaryDir dir;
        const QString path = dir.filePath("library.json");
        const QString bookId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString first = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString second = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QByteArray legacy = QJsonDocument(QJsonObject{{"version", 1}, {"books", QJsonArray{
            QJsonObject{{"id",bookId}, {"title","Positions"}, {"pages",QJsonArray{QJsonArray{},QJsonArray{}}},
                {"pageIds",QJsonArray{first,second}}}}}, {"activeBook",bookId}, {"page",0}}).toJson(QJsonDocument::Compact);
        QVERIFY(writeBytes(path, legacy));
        const auto sidecar = QJsonDocument(QJsonObject{{"libraryRevision",SyncPackage::digest(legacy)},
            {"activeBook",bookId}, {"positions",QJsonObject{{bookId,second}}}}).toJson();
        QVERIFY(writeBytes(path + ".reading.json", sidecar));
        { Notebook model(path); QCOMPARE(model.pageId(), second); QVERIFY(model.appendStroke(stroke())); }
        Notebook reopened(path);
        QCOMPARE(reopened.pageId(), second);
        QCOMPARE(reopened.strokes().size(), 1);
    }
    void legacyMigratesAndReopens() {
        QTemporaryDir dir;
        const QString path = dir.filePath("quaderni.json");
        const QByteArray legacy = R"({"version":1,"books":[{"id":"legacy","title":"Original","pages":[[]]}],"activeBook":"legacy","page":0})";
        QVERIFY(writeBytes(path, legacy));
        {
            Notebook model(path);
            QVERIFY2(model.error().isEmpty(), qPrintable(model.error()));
            QVERIFY(model.appendStroke(stroke()));
            QVERIFY(PageStore::isManifest(readBytes(path)));
            QCOMPARE(readBytes(path + ".legacy"), legacy);
        }
        Notebook reopened(path);
        QCOMPARE(reopened.title(), QStringLiteral("Original"));
        QCOMPARE(reopened.strokes().size(), 1);
        QVERIFY(reopened.error().isEmpty());
    }
    void immutablePagesAndRecovery() {
        QTemporaryDir dir;
        const QString path = dir.filePath("quaderni.json");
        Notebook model(path);
        QVERIFY(model.createBook("Recovery"));
        QVERIFY(model.appendStroke(stroke()));
        QVERIFY(model.addPage());
        const QByteArray before = model.exportLibraryForTransfer();
        QVERIFY(model.appendStroke(stroke(.5)));
        const auto beforePackage = SyncPackage::split(before);
        const auto afterPackage = SyncPackage::split(model.exportLibraryForTransfer());
        int shared = 0;
        for (auto it = beforePackage.objects.cbegin(); it != beforePackage.objects.cend(); ++it)
            shared += afterPackage.objects.contains(it.key());
        QVERIFY(shared > 0);
        QVERIFY(writeBytes(path, "broken manifest"));
        {
            Notebook recovered(path);
            QVERIFY(recovered.error().contains(QStringLiteral("Recuperata")));
            QCOMPARE(recovered.exportLibraryForTransfer(), before);
            QVERIFY(recovered.appendStroke(stroke(.6)));
            QCOMPARE(readBytes(path + ".damaged"), QByteArray("broken manifest"));
        }
        Notebook reopened(path);
        QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
        QCOMPARE(reopened.strokes().size(), 1);
    }
    void missingObjectRecoversPreviousGeneration() {
        QTemporaryDir dir;
        const QString path = dir.filePath("quaderni.json");
        Notebook model(path);
        QVERIFY(model.createBook("Missing page"));
        const auto previous = QJsonDocument::fromJson(readBytes(path)).object().value("objects").toArray();
        QVERIFY(model.appendStroke(stroke()));
        const auto current = QJsonDocument::fromJson(readBytes(path)).object().value("objects").toArray();
        QSet<QString> previousHashes;
        for (const auto &object : previous) previousHashes.insert(object.toObject().value("hash").toString());
        bool removed = false;
        for (const auto &object : current) {
            const QString hash = object.toObject().value("hash").toString();
            if (!previousHashes.contains(hash)) {
                QVERIFY(QFile::remove(path + ".objects/" + hash)); removed = true; break;
            }
        }
        QVERIFY(removed);
        const auto recovered = PageStore::read(path);
        QVERIFY2(recovered.error.isEmpty(), qPrintable(recovered.error));
        QVERIFY(recovered.recovered);
        Notebook reopened(path);
        QCOMPARE(reopened.strokes().size(), 0);
    }
    void missingManifestRecoversAndKeepsPreviousReadable() {
        QTemporaryDir dir;
        const QString path = dir.filePath("quaderni.json");
        {
            Notebook model(path);
            QVERIFY(model.createBook("Missing manifest"));
            QVERIFY(model.appendStroke(stroke()));
        }
        QVERIFY(QFile::remove(path));
        {
            Notebook recovered(path);
            QVERIFY(recovered.error().contains(QStringLiteral("Recuperata")));
            QCOMPARE(recovered.title(), QStringLiteral("Missing manifest"));
            QCOMPARE(recovered.strokes().size(), 0);
            QVERIFY(recovered.appendStroke(stroke(.4)));
        }
        QVERIFY(writeBytes(path, "damaged replacement"));
        Notebook recoveredAgain(path);
        QVERIFY(recoveredAgain.error().contains(QStringLiteral("Recuperata")));
        QCOMPARE(recoveredAgain.title(), QStringLiteral("Missing manifest"));
        QCOMPARE(recoveredAgain.strokes().size(), 0);
    }
    void incrementalStorageDoesNotEncodeUnchangedLargeAssets() {
        QTemporaryDir dir;
        const QString path = dir.filePath("quaderni.json");
        PageStore::BlobCache cache;
        const QString image(4 * 1024 * 1024, u'x');
        const QString source(8 * 1024 * 1024, u'y');
        const QVariantMap page{{"background", image}, {"items", QVariantList{}}, {"strokes", QVariantList{}}};
        const auto metadata = [&](const QString &hash) {
            return QVariantMap{{"version", 9}, {"books", QVariantList{QVariantMap{{"id", "book"},
                {"pageIds", QStringList{"page"}}, {"pages", QVariantList{hash}},
                {"sourceDocuments", QVariantMap{{"source", QVariantMap{{"kind", "pdf"}, {"data", source}}}}}}}}};
        };
        QString error;
        PageStore::Builder first({}, &cache);
        const QString pageHash = first.addPage(page, &error);
        PageStore::Update original;
        QVERIFY(first.finish(metadata(pageHash), &original, &error));
        QCOMPARE(first.encodedBlobBytes(), image.size() + source.size());
        QByteArray manifest;
        QVERIFY2(PageStore::writeUpdate(path, original, {}, &manifest, &error), qPrintable(error));
        const QString assetHash = SyncPackage::digest(image.toUtf8());
        const auto modified = QFileInfo(path + ".objects/" + assetHash).lastModified();
        PageStore::Builder second(manifest, &cache);
        QCOMPARE(second.reusePage("book", "page"), pageHash);
        auto edited = page;
        edited["strokes"] = QVariantList{stroke()};
        const QString editedHash = second.addPage(edited, &error);
        PageStore::Update changed;
        QVERIFY(second.finish(metadata(editedHash), &changed, &error));
        QCOMPARE(second.encodedBlobBytes(), 0);
        QCOMPARE(changed.changedObjects.size(), 1);
        QVERIFY(changed.changedObjects.contains(editedHash));
        const QByteArray previous = manifest;
        QVERIFY2(PageStore::writeUpdate(path, changed, previous, &manifest, &error), qPrintable(error));
        QCOMPARE(QFileInfo(path + ".objects/" + assetHash).lastModified(), modified);
        const auto read = PageStore::read(path);
        QVERIFY2(read.error.isEmpty(), qPrintable(read.error));
        QVERIFY(!read.payload.isEmpty());
        PageStore::Builder empty(manifest, &cache);
        PageStore::Update removed;
        QVERIFY(empty.finish(QVariantMap{{"version", 9}, {"books", QVariantList{}}}, &removed, &error));
        QVERIFY(cache.entries.isEmpty());
        PageStore::Builder oversized(manifest, &cache);
        QVariantList repeatedBooks;
        const QString reused = oversized.reusePage("book", "page");
        QVERIFY(!reused.isEmpty());
        for (int i = 0; i < 80; ++i) {
            auto book = metadata(reused).value("books").toList().first().toMap();
            book["id"] = QString::number(i);
            repeatedBooks.append(book);
        }
        PageStore::Update rejected;
        QVERIFY(!oversized.finish(QVariantMap{{"version", 9}, {"books", repeatedBooks}}, &rejected, &error));
        QVERIFY(error.contains("size limit"));
        QCOMPARE(readBytes(path), manifest);
    }
    void concurrentWriterCannotOverwrite() {
        QTemporaryDir dir;
        const QString path = dir.filePath("quaderni.json");
        Notebook first(path);
        QVERIFY(first.createBook("Concurrent"));
        Notebook stale(path);
        QVERIFY(first.appendStroke(stroke()));
        QVERIFY(!stale.appendStroke(stroke(.6)));
        Notebook reopened(path);
        QCOMPARE(reopened.strokes().size(), 1);
        QCOMPARE(reopened.strokes().first().toMap().value("points").toList().first().toMap().value("y").toDouble(), .2);
    }
    void formulasRemainEditableAfterTransfer() {
        QTemporaryDir dir;
        Notebook source(dir.filePath("source.json"));
        Notebook target(dir.filePath("target.json"));
        QVERIFY(source.createBook("Formulas"));
        QVERIFY(source.savePageItem(table()));
        const QByteArray payload = source.exportBookForTransfer(source.bookId());
        const auto preview = target.receiveBookTransfer(payload);
        QVERIFY2(!preview.contains("error"), qPrintable(preview.value("error").toString()));
        const auto received = target.receiveBookTransfer(payload, preview.value("token").toByteArray());
        QVERIFY2(!received.contains("error"), qPrintable(received.value("error").toString()));
        QCOMPARE(target.pageItems().first().toMap().value("cells"), source.pageItems().first().toMap().value("cells"));
        QCOMPARE(target.tableCellValues(target.pageItems().first().toMap()).at(2).toString(), QStringLiteral("2"));
        auto edited = target.pageItems().first().toMap();
        auto cells = edited.value("cells").toList(); cells[0] = "-4"; edited["cells"] = cells;
        QVERIFY(target.savePageItem(edited));
        QCOMPARE(target.tableCellValues(target.pageItems().first().toMap()).at(2).toString(), QStringLiteral("0"));
        Notebook reopened(dir.filePath("target.json"));
        QCOMPARE(reopened.pageItems().first().toMap().value("cells").toList().at(2).toString(), QStringLiteral("=SUM(A1:B1)"));
    }
    void staleSyncApprovalIsRejected() {
        QTemporaryDir dir;
        Notebook source(dir.filePath("source.json")); Notebook target(dir.filePath("target.json"));
        QVERIFY(source.createBook("Incoming")); QVERIFY(target.createBook("Local"));
        const auto payload = source.exportBookForTransfer(source.bookId());
        const auto preview = target.receiveBookTransfer(payload, {}, QByteArray{});
        QVERIFY(target.appendStroke(stroke()));
        const auto result = target.receiveBookTransfer(payload, preview.value("token").toByteArray(), QByteArray{});
        QCOMPARE(result.value("errorCode").toString(), QStringLiteral("stale"));
        QCOMPARE(target.books().size(), 1);
    }
    void negativeChartValuesSurviveExport() {
        QTemporaryDir dir; Notebook model(dir.filePath("library.json"));
        const QString path = dir.filePath("chart.csv");
        QVERIFY(model.exportChartCsv(QUrl::fromLocalFile(path), "Loss,-2\nGain,3"));
        const QByteArray bytes = readBytes(path);
        QVERIFY(bytes.contains("-2")); QVERIFY(!bytes.contains("'-2"));
    }
    void standbyDoesNotDependOnPin() {
        QTemporaryDir dir; PinLock pin(dir.filePath("pin.json")); DeviceInfo device(false);
        PaperProStandby standby(true, &pin, &device);
        QVERIFY(!pin.configured());
        standby.setDrawing(true); standby.suspend(); QVERIFY(!standby.sleeping());
        standby.setDrawing(false); standby.suspend(); QVERIFY(standby.sleeping());
        QVERIFY(!pin.locked());
        standby.recordActivity(); QVERIFY(standby.sleeping());
        standby.wake(); QVERIFY(!standby.sleeping());
        QVERIFY(pin.setPin("1234")); standby.suspend(); QVERIFY(pin.locked());
        standby.wake(); QVERIFY(pin.locked());
    }
    void appleDeliveryRequiresExplicitConfirmation() {
#ifdef Q_OS_MACOS
        QTemporaryDir dir;
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QByteArray queue = QJsonDocument(QJsonArray{QJsonObject{{"id", id}, {"file", "note.md"},
            {"state", "awaitingConfirmation"}, {"destination", "notes"}, {"title", "Test"}}}).toJson();
        QVERIFY(writeBytes(dir.filePath("queue.json"), queue));
        {
            AppleShare share(false, nullptr, dir.path());
            QCOMPARE(share.deliveries().first().toMap().value("state").toString(), QStringLiteral("awaitingConfirmation"));
            QVERIFY(share.confirmDelivery(id));
        }
        AppleShare reopened(false, nullptr, dir.path());
        QCOMPARE(reopened.deliveries().first().toMap().value("state").toString(), QStringLiteral("completed"));
        QVERIFY(!reopened.confirmDelivery(id));
        QVERIFY(reopened.archiveCompletedDeliveries());
        QVERIFY(reopened.deliveries().isEmpty());
        AppleShare archived(false, nullptr, dir.path());
        QVERIFY(archived.deliveries().isEmpty());
#else
        QSKIP("Apple delivery UI is macOS-only");
#endif
    }
};

QTEST_MAIN(RegressionTests)
#include "regression_tests.moc"
