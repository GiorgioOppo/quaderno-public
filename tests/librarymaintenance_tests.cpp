#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QPainter>
#include "documentreader.h"
#include "librarymaintenance.h"
#include "notebook.h"
#include "pagestore.h"

namespace {
bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
QVariantMap stroke()
{
    return {{"tool", "pen"}, {"width", 2.5}, {"color", "#222222"},
        {"points", QVariantList{QVariantMap{{"x", .2}, {"y", .3}, {"p", .7}},
            QVariantMap{{"x", .4}, {"y", .5}, {"p", .7}}}}};
}
QVariantList outline()
{
    return {QVariantMap{{"x", 0}, {"y", 0}}, QVariantMap{{"x", 1}, {"y", 0}},
        QVariantMap{{"x", 1}, {"y", 1}}, QVariantMap{{"x", 0}, {"y", 1}}};
}
}

class LibraryMaintenanceTests final : public QObject
{
    Q_OBJECT
private slots:
    void cleanup() { DocumentCache::clearMemory(); DocumentCache::setLimitMiB(64); }

    void readingOverridesStayLocalAndSurviveReopening()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("notebooks.json");
        Notebook notebook(path);
        QVERIFY(notebook.createBook("One"));
        const QString first = notebook.bookId();
        QVERIFY(notebook.appendStroke(stroke()));
        const QByteArray portable = notebook.exportLibraryForTransfer(), manifest = readFile(path);
        const QString revision = notebook.bookTransferRevision(first);
        QSignalSpy changes(&notebook, &Notebook::localReadingPreferencesChanged);
        QVERIFY(notebook.setReadingPreferences({{"readingZoom", 150}, {"readingMargins", 16},
            {"readingOrientation", 90}, {"swipeAction", "scroll"}}));
        QCOMPARE(changes.count(), 1);
        QCOMPARE(notebook.exportLibraryForTransfer(), portable);
        QCOMPARE(readFile(path), manifest);
        QCOMPARE(notebook.bookTransferRevision(first), revision);
        QVERIFY(!notebook.setReadingPreferences({{"readingZoom", 999}}));
        QVERIFY(!notebook.setReadingPreferences({{"unexpected", "value"}}));
        QCOMPARE(notebook.localReadingPreferences().value("readingZoom").toInt(), 150);
        QVERIFY(notebook.setReadingPreferences({{"readingZoom", 173}}));
        QCOMPARE(notebook.localReadingPreferences().value("readingZoom").toInt(), 173);
        QCOMPARE(notebook.localReadingPreferences().value("readingMargins").toInt(), 16);
        QCOMPARE(notebook.localReadingPreferences().value("swipeAction").toString(), QString("scroll"));
        QVERIFY(notebook.createBook("Two"));
        const QString second = notebook.bookId();
        QVERIFY(notebook.localReadingPreferences().isEmpty());
        QVERIFY(notebook.setReadingPreferences({{"readingZoom", 125}}));
        QVERIFY(notebook.openBook(first));
        QCOMPARE(notebook.localReadingPreferences().value("readingOrientation").toInt(), 90);
        Notebook reopened(path);
        QCOMPARE(reopened.readingPreferencesForBook(first), notebook.localReadingPreferences());
        QCOMPARE(reopened.readingPreferencesForBook(second).value("readingZoom").toInt(), 125);
        Notebook imported(directory.filePath("imported.json"));
        QVERIFY(imported.importFromData(notebook.exportLibraryForTransfer()));
        QVERIFY(imported.readingPreferencesForBook(first).isEmpty());
        QVERIFY(reopened.clearReadingPreferences());
        QVERIFY(reopened.localReadingPreferences().isEmpty());
        QCOMPARE(reopened.readingPreferencesForBook(second).value("readingZoom").toInt(), 125);
    }

    void conflictingOrCorruptReadingSidecarsDoNotOverwriteData()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("notebooks.json");
        Notebook first(path);
        QVERIFY(first.createBook("Reading"));
        Notebook stale(path);
        QVERIFY(first.setReadingPreferences({{"readingZoom", 125}}));
        QVERIFY(!stale.setReadingPreferences({{"readingZoom", 200}}));
        const QByteArray library = readFile(path);
        QVERIFY(writeFile(first.readingPreferencesPath(), "invalid local settings"));
        Notebook corrupt(path);
        QVERIFY(corrupt.setPageTemplate("grid"));
        QVERIFY(!corrupt.setReadingPreferences({{"readingZoom", 150}}));
        QCOMPARE(readFile(first.readingPreferencesPath()), QByteArray("invalid local settings"));
        QVERIFY(readFile(path) != library); // Notebook edits remain usable.
    }

    void backupIsPortableAndDoesNotTakeTheLiveWriterLock()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("notebooks.json");
        Notebook notebook(path);
        QVERIFY(notebook.createBook("Backup ink"));
        QVERIFY(notebook.appendStroke(stroke()));
        QVERIFY(notebook.savePageItem({{"type", "table"}, {"rows", 2}, {"cols", 2},
            {"cells", QVariantList{"12", "=SUM(A1:A2)", "7", ""}}}));
        QVERIFY(notebook.setReadingPreferences({{"readingZoom", 150}}));
        const QByteArray libraryBefore = readFile(path);
        const QString revision = notebook.bookTransferRevision(notebook.bookId());
        LibraryMaintenance maintenance;
        maintenance.setNotebook(&notebook);
        QTRY_VERIFY(!maintenance.busy());
        QLockFile writerLock(path + ".lock");
        QVERIFY(writerLock.tryLock(0));
        QSignalSpy completed(&maintenance, &LibraryMaintenance::finished);
        QVERIFY(maintenance.backup());
        QVERIFY(!maintenance.backup());
        QTRY_COMPARE(completed.count(), 1);
        QVERIFY2(completed.first().first().toBool(), qPrintable(maintenance.error()));
        writerLock.unlock();
        QCOMPARE(readFile(path), libraryBefore);
        QVERIFY(QFileInfo::exists(maintenance.lastBackupPath()));
        QVERIFY(QFileInfo::exists(QFileInfo(maintenance.lastBackupPath()).dir().filePath("reader-settings.json")));
        Notebook restored(directory.filePath("restored.json"));
        QVERIFY(restored.importFromFile(maintenance.lastBackupPath()));
        QVERIFY(restored.openBook(notebook.bookId()));
        QCOMPARE(restored.bookTransferRevision(notebook.bookId()), revision);
        QCOMPARE(restored.strokes(), notebook.strokes());
        QCOMPARE(restored.pageItems(), notebook.pageItems());
        QVERIFY(restored.localReadingPreferences().isEmpty());
    }

    void retentionOnlyRemovesVerifiedOwnSnapshotsAfterSuccess()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("notebooks.json");
        Notebook notebook(path);
        QVERIFY(notebook.createBook("Versions"));
        LibraryMaintenance maintenance;
        maintenance.setBackupRetention(5);
        maintenance.setNotebook(&notebook);
        QTRY_VERIFY(!maintenance.busy());
        QVERIFY(maintenance.backup()); QTRY_VERIFY(!maintenance.busy());
        QVERIFY2(maintenance.error().isEmpty(), qPrintable(maintenance.error()));
        const QString first = maintenance.lastBackupPath();
        QByteArray altered = readFile(first);
        altered[altered.size() / 2] = altered.at(altered.size() / 2) == 'x' ? 'y' : 'x';
        QVERIFY(writeFile(first, altered));
        QVERIFY(notebook.appendStroke(stroke()));
        QVERIFY(maintenance.backup()); QTRY_VERIFY(!maintenance.busy());
        const QString second = maintenance.lastBackupPath();
        QVERIFY(second != first);
        const QString foreign = path + ".backups/notes.txt";
        QVERIFY(writeFile(foreign, "keep me"));
        maintenance.setBackupRetention(1);
        QVERIFY(maintenance.backup()); QTRY_VERIFY(!maintenance.busy());
        QVERIFY2(maintenance.error().isEmpty(), qPrintable(maintenance.error()));
        QVERIFY(QFileInfo::exists(first)); // Tampering invalidates ownership verification.
        QVERIFY(!QFileInfo::exists(second));
        QCOMPARE(readFile(foreign), QByteArray("keep me"));
        const QString last = maintenance.lastBackupPath();
        QVERIFY(writeFile(path, "damaged live manifest"));
        QVERIFY(maintenance.backup()); QTRY_VERIFY(!maintenance.busy());
        QVERIFY(!maintenance.error().isEmpty());
        QVERIFY(QFileInfo::exists(last));
        QVERIFY(QFileInfo::exists(first));
    }

    void cacheLimitsAndCleanupPreserveOriginalsAndUnownedFiles()
    {
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("notebooks.json"));
        QVERIFY(notebook.createBook("Keep originals"));
        QVERIFY(notebook.appendStroke(stroke()));
        const QByteArray before = notebook.exportLibraryForTransfer();
        QVERIFY(QDir().mkpath(notebook.sourceCacheDirectory()));
        const QByteArray source("rebuildable document copy");
        const QString name = QString::fromLatin1(QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex()) + ".pdf";
        const QString cached = QDir(notebook.sourceCacheDirectory()).filePath(name);
        QVERIFY(writeFile(cached, source));
        const QString original = directory.filePath("original.pdf");
        QVERIFY(writeFile(original, "user original"));
        const QString unknown = QDir(notebook.sourceCacheDirectory()).filePath("notes.txt");
        QVERIFY(writeFile(unknown, "unowned"));
        const QString link = QDir(notebook.sourceCacheDirectory()).filePath(QString(64, 'a') + ".pdf");
        QVERIFY(QFile::link(original, link));
        DocumentCache::setLimitMiB(32);
        QImage image(4096, 1280, QImage::Format_RGB32);
        image.fill(Qt::white);
        const auto generation = DocumentCache::generation();
        DocumentCache::insertImage("first", image, generation);
        DocumentCache::insertImage("second", image, generation);
        QVERIFY(DocumentCache::memoryStatus().value("memoryCacheBytes").toLongLong() <= 32 * 1024 * 1024);
        QCOMPARE(DocumentCache::memoryStatus().value("cachedPages").toInt(), 1);
        LibraryMaintenance maintenance;
        maintenance.setNotebook(&notebook);
        QTRY_VERIFY(!maintenance.busy());
        QVERIFY(maintenance.clearCache()); QTRY_VERIFY(!maintenance.busy());
        QVERIFY2(maintenance.error().isEmpty(), qPrintable(maintenance.error()));
        QVERIFY(!QFileInfo::exists(cached));
        QCOMPARE(readFile(original), QByteArray("user original"));
        QCOMPARE(readFile(unknown), QByteArray("unowned"));
        QVERIFY(QFileInfo(link).isSymLink());
        DocumentCache::insertImage("stale job", image, generation);
        QCOMPARE(DocumentCache::memoryStatus().value("cachedPages").toInt(), 0);
        QCOMPARE(notebook.exportLibraryForTransfer(), before);
        Notebook reopened(notebook.storagePath());
        QCOMPARE(reopened.strokes(), notebook.strokes());
    }

    void scheduleRespectsAllowedAndDoesNotRepeatFreshBackup()
    {
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("notebooks.json"));
        QVERIFY(notebook.createBook("Scheduled"));
        LibraryMaintenance maintenance;
        maintenance.setNotebook(&notebook); QTRY_VERIFY(!maintenance.busy());
        QSignalSpy completed(&maintenance, &LibraryMaintenance::finished);
        maintenance.checkSchedule();
        QVERIFY(!maintenance.busy());
        maintenance.setBackupSchedule("daily");
        maintenance.setAllowed(false);
        maintenance.checkSchedule();
        QVERIFY(!maintenance.busy());
        QVERIFY(!maintenance.backup());
        QVERIFY(!maintenance.clearCache());
        maintenance.setAllowed(true);
        maintenance.checkSchedule();
        QTRY_COMPARE(completed.count(), 1);
        QVERIFY2(maintenance.error().isEmpty(), qPrintable(maintenance.error()));
        maintenance.checkSchedule();
        QVERIFY(!maintenance.busy());
        QCOMPARE(maintenance.backups().size(), 1);
    }

    void clearedDocumentCopyIsRebuiltFromThePreservedOriginal()
    {
        QTemporaryDir directory;
        const QString pdfPath = directory.filePath("source.pdf");
        {
            QPdfWriter writer(pdfPath);
            writer.setResolution(72);
            QPainter painter(&writer);
            painter.fillRect(QRect(30, 30, 120, 120), Qt::black);
            painter.drawText(30, 210, "Original survives cache cleanup");
        }
        Notebook notebook(directory.filePath("notebooks.json"));
        const bool imported = notebook.importDocument(QUrl::fromLocalFile(pdfPath), {});
#ifndef Q_OS_MACOS
        if (!imported && notebook.error().contains(QStringLiteral("non è disponibile")))
            QSKIP("PDFium is absent on this host.");
#endif
        QVERIFY2(imported, qPrintable(notebook.error()));
        QVERIFY(notebook.appendStroke(stroke()));
        const QByteArray portable = notebook.exportLibraryForTransfer();
        const QVariantMap source = notebook.pageRenderContent().value("source").toMap();
        const QString cachePath = QDir(notebook.sourceCacheDirectory()).filePath(source.value("asset").toString() + ".pdf");
        QString error;
        const QImage before = renderDocumentPage(source, 800, error);
        QVERIFY2(!before.isNull(), qPrintable(error));
        QVERIFY(QFileInfo::exists(cachePath));
        LibraryMaintenance maintenance;
        maintenance.setNotebook(&notebook); QTRY_VERIFY(!maintenance.busy());
        QVERIFY(maintenance.clearCache()); QTRY_VERIFY(!maintenance.busy());
        QVERIFY2(maintenance.error().isEmpty(), qPrintable(maintenance.error()));
        QVERIFY(!QFileInfo::exists(cachePath));
        QVERIFY(QFileInfo::exists(pdfPath));
        QCOMPARE(notebook.exportLibraryForTransfer(), portable);
        QCOMPARE(renderDocumentPage(source, 800, error), before);
        QVERIFY(QFileInfo::exists(cachePath));
    }

    void retainingHandwritingCreatesOneUndoableTextInsertion()
    {
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("notebooks.json"));
        QVERIFY(notebook.createBook("Keep handwriting"));
        QVERIFY(notebook.appendStroke(stroke()));
        const auto original = notebook.strokes();
        const auto selection = notebook.selectInk(outline());
        QVERIFY(notebook.replaceHandwriting(selection, "Recognized", true));
        QCOMPARE(notebook.strokes(), original);
        QCOMPARE(notebook.pageItems().size(), 1);
        const auto text = notebook.pageItems().first().toMap();
        QCOMPARE(text.value("text").toString(), QString("Recognized"));
        QVERIFY(!text.contains("handwritingSource"));
        QVERIFY(notebook.undo());
        QCOMPARE(notebook.strokes(), original);
        QVERIFY(notebook.pageItems().isEmpty());
        QVERIFY(notebook.redo());
        QCOMPARE(notebook.strokes(), original);
        QCOMPARE(notebook.pageItems().first().toMap(), text);
    }

    void explicitLocalRecoveryKeepsTheNetworkImportBudgetSeparate()
    {
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        QVERIFY(source.createBook("Local recovery"));
        QByteArray payload = source.exportLibraryForTransfer();
        payload.append(QByteArray(64 * 1024 * 1024 + 1, ' '));
        const QString backup = directory.filePath("library.quaderno");
        QVERIFY(writeFile(backup, payload));
        Notebook destination(directory.filePath("destination.json"));
        QVERIFY(!destination.importFromData(payload));
        QVERIFY(destination.error().contains("64 MB"));
        QVERIFY2(destination.importFromFile(backup), qPrintable(destination.error()));
        QVERIFY(destination.hasBookId(source.bookId()));
        QFile excessive(directory.filePath("oversized.quaderno"));
        QVERIFY(excessive.open(QIODevice::WriteOnly));
        QVERIFY(excessive.resize(PageStore::maximumLibraryBytes + 1));
        excessive.close();
        QVERIFY(!destination.importFromFile(excessive.fileName()));
        QVERIFY(destination.error().contains("256 MB"));
        QVERIFY(destination.hasBookId(source.bookId()));
    }
};

QTEST_MAIN(LibraryMaintenanceTests)
#include "librarymaintenance_tests.moc"
