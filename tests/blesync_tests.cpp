#include "blesync.h"
#include "notebook.h"

#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include <QCryptographicHash>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTest>
#include <memory>

namespace {
QVariantMap stroke(double y = .2)
{
    return {{"color", "#222222"}, {"width", 2.5}, {"tool", "pen"}, {"points", QVariantList{
        QVariantMap{{"x", .2}, {"y", y}, {"p", .5}}, QVariantMap{{"x", .4}, {"y", y + .1}, {"p", .7}}}}};
}

QByteArray retainedBook()
{
    QByteArray pdf;
    QBuffer output(&pdf);
    output.open(QIODevice::WriteOnly);
    {
        QPdfWriter writer(&output);
        writer.setResolution(72);
        QPainter painter(&writer);
        painter.drawText(40, 80, QStringLiteral("First book page"));
        writer.newPage();
        painter.drawText(40, 80, QStringLiteral("Second book page"));
        painter.end();
    }
    const QString asset = QString::fromLatin1(QCryptographicHash::hash(pdf, QCryptographicHash::Sha256).toHex());
    QJsonArray pages;
    for (int page = 0; page < 2; ++page)
        pages.append(QJsonObject{{"template", "blank"}, {"strokes", QJsonArray{}}, {"items", QJsonArray{}},
            {"source", QJsonObject{{"kind", "pdf"}, {"asset", asset}, {"page", page}}},
            {"pdfText", QStringLiteral("Book page %1").arg(page + 1)}});
    const QJsonObject book{{"id", "annotations-book"}, {"title", "Retained book annotations"},
        {"pages", pages}, {"pageIds", QJsonArray{"11111111-1111-4111-8111-111111111111", "22222222-2222-4222-8222-222222222222"}},
        {"sourceDocuments", QJsonObject{{asset, QJsonObject{{"kind", "pdf"}, {"data", QString::fromLatin1(pdf.toBase64())}}}}}};
    return QJsonDocument(QJsonObject{{"version", 9}, {"books", QJsonArray{book}}, {"folders", QJsonArray{}},
        {"activeBook", "annotations-book"}, {"page", 0}}).toJson(QJsonDocument::Compact);
}

struct Pair {
    const QString directory;
    Notebook source;
    Notebook destination;
    BleSync sender;
    std::unique_ptr<BleSync> receiver;
    QVariantMap lastApply;
    bool dropApplyReply = false;
    int advertisedReceiverVersion = 0;

    explicit Pair(const QString &path)
        : directory(path), source(path + "/source.json"), destination(path + "/destination.json"),
          sender(&source, nullptr, path + "/sender")
    {
        sender.setPeerIdentity(QString(32, QLatin1Char('a')));
        restartReceiver();
        sender.setConnected(true);
    }

    void restartReceiver()
    {
        if (receiver)
            receiver->setConnected(false);
        receiver = std::make_unique<BleSync>(&destination, nullptr, directory + "/receiver");
        receiver->setPeerIdentity(QString(32, QLatin1Char('b')));
        QObject::connect(&sender, &BleSync::sendMessage, receiver.get(), [this](const QVariantMap &metadata, const QByteArray &payload) {
            if (metadata.value("type") == QStringLiteral("apply"))
                lastApply = metadata;
            receiver->receiveMessage(metadata, payload);
        }, Qt::QueuedConnection);
        QObject::connect(receiver.get(), &BleSync::sendMessage, &sender, [this](const QVariantMap &metadata, const QByteArray &payload) {
            if (!dropApplyReply || metadata.value("type") != QStringLiteral("applyReply")) {
                QVariantMap received = metadata;
                if (advertisedReceiverVersion > 0
                    && (received.value("type") == QStringLiteral("syncHello") || received.value("type") == QStringLiteral("syncHelloReply")))
                    received.insert("maxNotebookVersion", advertisedReceiverVersion);
                sender.receiveMessage(received, payload);
            }
        }, Qt::QueuedConnection);
        receiver->setConnected(true);
    }
};
}

class BleSyncTests final : public QObject
{
    Q_OBJECT
private slots:
    void bookHighlightsAndAnchoredNotesSyncAcrossPagesAndReopening_data()
    {
        QTest::addColumn<bool>("includeNote");
        QTest::newRow("highlights-and-underlines-format-nine") << false;
        QTest::newRow("highlights-and-editable-sticky-note-format-eleven") << true;
    }

    void bookHighlightsAndAnchoredNotesSyncAcrossPagesAndReopening()
    {
        QFETCH(bool, includeNote);
        QTemporaryDir directory;
        Pair pair(directory.path());
        if (!includeNote) pair.advertisedReceiverVersion = 9;
        const QByteArray imported = retainedBook();
        const auto importedPreview = pair.source.previewImportFromData(imported);
        QVERIFY2(!importedPreview.contains("error"), qPrintable(importedPreview.value("error").toString()));
        QVERIFY(!pair.source.applyImportFromData(imported, importedPreview.value("token").toString()).contains("error"));
        const QString book = pair.source.bookId();
        QCOMPARE(pair.source.pageCount(), 2);
        const QString firstPage = pair.source.pageId();
        const auto originalSources = QJsonDocument::fromJson(pair.source.exportBookForTransfer(book))
            .object().value("books").toArray().first().toObject().value("sourceDocuments");
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        QCOMPARE(pair.destination.books().size(), 1);

        auto highlight = stroke(.22);
        highlight.insert("tool", "highlighter"); highlight.insert("color", "#f0bf32"); highlight.insert("width", 9);
        QVERIFY(pair.source.appendStroke(highlight));
        const QString visibleLayer = pair.source.activeLayer();
        QVERIFY(pair.source.addLayer("Hidden ink"));
        const QString hiddenLayer = pair.source.activeLayer();
        QVERIFY(pair.source.appendStroke(stroke(.5)));
        QVERIFY(pair.source.setLayerVisible(hiddenLayer, false));
        QVERIFY(pair.source.setActiveLayer(visibleLayer));
        QVERIFY(pair.source.goToPage(1));
        const QString secondPage = pair.source.pageId();
        auto underline = stroke(.4); underline.insert("tool", "underline");
        QVERIFY(pair.source.appendStroke(underline));
        if (includeNote)
            QVERIFY(pair.source.savePageNote({{"x", .47}, {"y", .38}, {"text", "Check this paragraph"},
                {"noteColor", "yellow"}, {"noteInk", QVariantList{stroke(.3)}}}, book, secondPage));
        const QVariantMap note = includeNote ? pair.source.pageItems().first().toMap() : QVariantMap{};
        const QString noteId = note.value("id").toString();
        const auto expectedFirst = pair.source.pagePreview(0), expectedSecond = pair.source.pagePreview(1);
        const QString editedRevision = pair.source.bookTransferRevision(book);
        QVERIFY(pair.source.goToPage(0)); // Edits on a noncurrent page must still participate.
        QVERIFY(pair.destination.openBook(book));
        QVERIFY(pair.destination.goToPage(1));
        QSignalSpy destinationInk(&pair.destination, &Notebook::strokesChanged);
        QSignalSpy destinationContent(&pair.destination, &Notebook::pageContentChanged);
        pair.sender.setPinned(book, true);
        pair.sender.setAutoSync(true);
        QTRY_COMPARE_WITH_TIMEOUT(pair.destination.bookTransferRevision(book), editedRevision, 6000);
        QTRY_VERIFY(pair.sender.completed());
        QCOMPARE(pair.destination.books().size(), 1); // Update the same book, not a title-matched duplicate.
        QCOMPARE(pair.destination.pageId(), secondPage); // Preserve the Mac's current reading position.
        QVERIFY(!destinationInk.isEmpty());
        QVERIFY(!destinationContent.isEmpty());
        QCOMPARE(pair.destination.pagePreview(0), expectedFirst);
        QCOMPARE(pair.destination.pagePreview(1), expectedSecond);
        if (includeNote) QCOMPARE(pair.destination.pageItems().first().toMap(), note);
        else QVERIFY(pair.destination.pageItems().isEmpty());
        QCOMPARE(pair.destination.strokes(), QVariantList{underline});
        const auto transferred = QJsonDocument::fromJson(pair.destination.exportBookForTransfer(book)).object();
        QCOMPARE(transferred.value("version").toInt(), includeNote ? 11 : 9);
        QCOMPARE(transferred.value("books").toArray().first().toObject().value("sourceDocuments"), originalSources);
        for (const auto &entry : pair.destination.pageSummaries()) QVERIFY(entry.toMap().value("annotated").toBool());

        Notebook reopened(directory.filePath("destination.json"));
        QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
        QCOMPARE(reopened.bookTransferRevision(book), editedRevision);
        QCOMPARE(reopened.pagePreview(0), expectedFirst);
        QCOMPARE(reopened.pagePreview(1), expectedSecond);
        QVERIFY(reopened.openPage(firstPage));
        QCOMPARE(reopened.strokes(), QVariantList{highlight}); // Hidden-layer ink is not lost or made visible.
        QVERIFY(reopened.setLayerVisible(hiddenLayer, true));
        QCOMPARE(reopened.strokes().size(), 2);
        QVERIFY(reopened.openPage(secondPage));
        if (includeNote) {
            auto changedNote = reopened.pageItems().first().toMap();
            QCOMPARE(changedNote.value("id").toString(), noteId);
            changedNote.insert("text", "Edited on the Mac");
            QVERIFY(reopened.savePageNote(changedNote, book, secondPage, note));
            QCOMPARE(reopened.pageItems().first().toMap().value("noteInk"), note.value("noteInk"));
            QCOMPARE(reopened.pageItems().first().toMap().value("x"), note.value("x"));
        }
    }

    void defaultNotebookKeepsLegacyRevision()
    {
        QTemporaryDir directory;
        Notebook notebook(directory.filePath("legacy-compatible.json"));
        QVERIFY(notebook.createBook("Legacy revision"));
        QVERIFY(notebook.appendStroke(stroke()));
        const QJsonObject root = QJsonDocument::fromJson(notebook.exportBookForTransfer(notebook.bookId())).object();
        const QJsonObject book = root.value("books").toArray().first().toObject();
        const QByteArray legacy = QJsonDocument(QJsonArray{book.value("title"), book.value("pages")}).toJson(QJsonDocument::Compact);
        QCOMPARE(notebook.bookTransferRevision(notebook.bookId()), SyncPackage::digest(legacy));
    }

    void previewNeverMutatesAndEditorBusyBlocksApply()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Preview"));
        QVERIFY(pair.source.appendStroke(stroke()));
        const QByteArray before = pair.destination.exportLibraryForTransfer();
        pair.sender.preview(pair.source.bookId());
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        QCOMPARE(pair.destination.exportLibraryForTransfer(), before);
        pair.receiver->setEditorBusy(true);
        pair.sender.apply();
        QTRY_VERIFY(!pair.sender.busy());
        QVERIFY(!pair.sender.completed());
        QCOMPARE(pair.destination.exportLibraryForTransfer(), before);
    }

    void sourceChangesInvalidateApproval()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Changed source"));
        pair.sender.preview(pair.source.bookId());
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        const QByteArray before = pair.destination.exportLibraryForTransfer();
        QVERIFY(pair.source.appendStroke(stroke()));
        pair.sender.apply();
        QVERIFY(!pair.sender.busy());
        QVERIFY(!pair.sender.completed());
        QVERIFY(pair.lastApply.isEmpty());
        QCOMPARE(pair.destination.exportLibraryForTransfer(), before);
    }

    void destinationChangesInvalidateApproval()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Shared"));
        const QString book = pair.source.bookId();
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        QVERIFY(pair.source.appendStroke(stroke()));
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        QCOMPARE(pair.sender.previewInfo().value("action").toString(), QStringLiteral("replace"));
        QVERIFY(pair.destination.openBook(book));
        QVERIFY(pair.destination.appendStroke(stroke(.6)));
        const QByteArray changed = pair.destination.exportBookForTransfer(book);
        pair.sender.apply();
        QTRY_VERIFY(!pair.sender.busy());
        QVERIFY(!pair.sender.completed());
        QCOMPARE(pair.destination.exportBookForTransfer(book), changed);
    }

    void conflictReceiptSurvivesRestartWithoutDuplicateCopy()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Conflict"));
        const QString book = pair.source.bookId();
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        QVERIFY(pair.source.appendStroke(stroke()));
        QVERIFY(pair.destination.openBook(book));
        QVERIFY(pair.destination.appendStroke(stroke(.6)));
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        QCOMPARE(pair.sender.previewInfo().value("action").toString(), QStringLiteral("conflict"));
        const int before = pair.destination.books().size();
        pair.dropApplyReply = true;
        pair.sender.apply();
        QTRY_COMPARE(pair.destination.books().size(), before + 1);
        QTRY_VERIFY(!pair.lastApply.isEmpty());
        QVERIFY(pair.sender.applying());
        const QVariantMap committedRequest = pair.lastApply;
        pair.restartReceiver();
        pair.dropApplyReply = false;
        pair.receiver->receiveMessage(committedRequest, {});
        QTRY_VERIFY(pair.sender.completed());
        QCOMPARE(pair.destination.books().size(), before + 1);
    }

    void scheduledSyncRequiresTriggerAndGateButManualTransfersRemainAvailable()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Pinned scheduled book"));
        const QString book = pair.source.bookId();
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        pair.sender.setPinned(book, true);
        const QByteArray initial = pair.destination.exportLibraryForTransfer();
        QTest::qWait(1900);
        QVERIFY(pair.sender.previewInfo().isEmpty());
        QCOMPARE(pair.destination.exportLibraryForTransfer(), initial);
        pair.sender.setAutomaticAllowed(false);
        QVERIFY(!pair.sender.requestAutomaticSync());
        pair.sender.preview(book); // The automation policy does not remove explicit user actions.
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        QVERIFY(pair.destination.hasBookId(book));
        QVERIFY(pair.source.appendStroke(stroke()));
        const QString editedRevision = pair.source.bookTransferRevision(book);
        QVERIFY(!pair.sender.requestAutomaticSync());
        pair.sender.setAutomaticAllowed(true);
        QVERIFY(pair.sender.requestAutomaticSync());
        QTRY_COMPARE_WITH_TIMEOUT(pair.destination.bookTransferRevision(book), editedRevision, 5000);
        QTRY_VERIFY(pair.sender.completed());
        QCOMPARE(pair.sender.pinnedBooks(), QStringList{book});
    }

    void triggeredBatchDrainsOnlyPinnedSnapshotAndDoesNotBecomeContinuous()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QStringList books;
        for (const QString &title : {QStringLiteral("First"), QStringLiteral("Second")}) {
            QVERIFY(pair.source.createBook(title));
            books.append(pair.source.bookId());
            pair.sender.preview(pair.source.bookId());
            QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
            pair.sender.apply();
            QTRY_VERIFY(pair.sender.completed());
        }
        pair.sender.setTriggeredAutomaticSync(true);
        for (const QString &book : books) pair.sender.setPinned(book, true);
        pair.sender.setAutoSync(true);
        QStringList revisions;
        for (const QString &book : books) {
            QVERIFY(pair.source.openBook(book));
            QVERIFY(pair.source.appendStroke(stroke()));
            revisions.append(pair.source.bookTransferRevision(book));
        }
        QVERIFY(pair.source.createBook("Not pinned"));
        const QString unpinned = pair.source.bookId();
        QVERIFY(pair.sender.requestAutomaticSync());
        QTRY_COMPARE_WITH_TIMEOUT(pair.destination.bookTransferRevision(books[0]), revisions[0], 5000);
        QTRY_COMPARE_WITH_TIMEOUT(pair.destination.bookTransferRevision(books[1]), revisions[1], 5000);
        QTRY_VERIFY(pair.sender.completed());
        QVERIFY(!pair.destination.hasBookId(unpinned));
        QVERIFY(pair.source.openBook(books[0]));
        QVERIFY(pair.source.appendStroke(stroke(.5)));
        QTest::qWait(1900);
        QCOMPARE(pair.destination.bookTransferRevision(books[0]), revisions[0]);
        QVERIFY(pair.sender.requestAutomaticSync());
        QTRY_COMPARE_WITH_TIMEOUT(pair.destination.bookTransferRevision(books[0]), pair.source.bookTransferRevision(books[0]), 5000);
        QVERIFY(!pair.destination.hasBookId(unpinned));
    }

    void revokingPolicyCancelsAutomaticReviewAndEditorPauseDefersWork()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Needs first approval"));
        const QString book = pair.source.bookId();
        const QByteArray original = pair.destination.exportLibraryForTransfer();
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        pair.sender.setPinned(book, true);
        pair.sender.setEditorBusy(true);
        QVERIFY(!pair.sender.requestAutomaticSync());
        pair.sender.setEditorBusy(false);
        QVERIFY(pair.sender.requestAutomaticSync());
        QTRY_COMPARE_WITH_TIMEOUT(pair.sender.previewInfo().value("action").toString(), QStringLiteral("create"), 5000);
        QVERIFY(pair.sender.previewInfo().value("automatic").toBool());
        QVERIFY(pair.lastApply.isEmpty()); // A first transfer still requires confirmation.
        pair.sender.setAutomaticAllowed(false);
        QVERIFY(pair.sender.previewInfo().isEmpty());
        QVERIFY(!pair.sender.busy());
        QCOMPARE(pair.destination.exportLibraryForTransfer(), original);
        QTest::qWait(1900);
        QVERIFY(pair.sender.previewInfo().isEmpty());
        pair.sender.setAutomaticAllowed(true);
        QTRY_COMPARE_WITH_TIMEOUT(pair.sender.previewInfo().value("action").toString(), QStringLiteral("create"), 5000);
        pair.sender.setEditorBusy(true);
        QVERIFY(pair.sender.previewInfo().isEmpty());
        pair.sender.setAutoSync(false);
        pair.sender.setEditorBusy(false);
        QTest::qWait(1900);
        QVERIFY(pair.sender.previewInfo().isEmpty());
        QCOMPARE(pair.destination.exportLibraryForTransfer(), original);
    }

    void keepBothCopiesConflictWithoutOverwritingOrDuplicatingOnNextTrigger()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Diverged"));
        const QString book = pair.source.bookId();
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        QVERIFY(pair.source.appendStroke(stroke(.2)));
        QVERIFY(pair.destination.openBook(book));
        QVERIFY(pair.destination.appendStroke(stroke(.6)));
        const QByteArray localOriginal = pair.source.exportBookForTransfer(book);
        const QByteArray remoteOriginal = pair.destination.exportBookForTransfer(book);
        const int initialBooks = pair.destination.books().size();
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        pair.sender.setPinned(book, true);
        QVERIFY(pair.sender.requestAutomaticSync());
        QTRY_COMPARE_WITH_TIMEOUT(pair.sender.previewInfo().value("action").toString(), QStringLiteral("conflict"), 5000);
        QCOMPARE(pair.destination.books().size(), initialBooks); // Default policy remains ask.
        QCOMPARE(pair.destination.exportBookForTransfer(book), remoteOriginal);
        pair.sender.setAutomaticAllowed(false);
        pair.sender.setConflictPolicy("keepBoth");
        pair.sender.setAutomaticAllowed(true);
        QTRY_COMPARE_WITH_TIMEOUT(pair.destination.books().size(), initialBooks + 1, 5000);
        QTRY_VERIFY(pair.sender.completed());
        QCOMPARE(pair.destination.exportBookForTransfer(book), remoteOriginal);
        QCOMPARE(pair.source.exportBookForTransfer(book), localOriginal);
        QVERIFY(pair.sender.requestAutomaticSync());
        QTest::qWait(1900);
        QCOMPARE(pair.destination.books().size(), initialBooks + 1);
        // Reload the paired state as on restart. The same local revision must not
        // generate another conflict copy at every interval after reconnecting.
        pair.sender.setPeerIdentity({});
        pair.sender.setPeerIdentity(QString(32, QLatin1Char('a')));
        pair.sender.setConnected(true);
        QVERIFY(pair.sender.requestAutomaticSync());
        QTest::qWait(1900);
        QCOMPARE(pair.destination.books().size(), initialBooks + 1);
        QVERIFY(pair.sender.previewInfo().isEmpty());
    }

    void lostAutomaticConflictAcknowledgementRequiresManualReviewBeforeAnotherCopy()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Uncertain copy"));
        const QString book = pair.source.bookId();
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        QVERIFY(pair.source.appendStroke(stroke()));
        QVERIFY(pair.destination.openBook(book));
        QVERIFY(pair.destination.appendStroke(stroke(.6)));
        const int initialBooks = pair.destination.books().size();
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        pair.sender.setConflictPolicy("keepBoth");
        pair.sender.setPinned(book, true);
        pair.dropApplyReply = true;
        QVERIFY(pair.sender.requestAutomaticSync());
        QTRY_COMPARE_WITH_TIMEOUT(pair.destination.books().size(), initialBooks + 1, 5000);
        QVERIFY(pair.sender.applying());
        pair.sender.setConnected(false); // The receiver committed but the sender did not see its receipt.
        pair.receiver->setConnected(false);
        pair.sender.setPeerIdentity({});
        pair.sender.setPeerIdentity(QString(32, QLatin1Char('a')));
        pair.receiver->setConnected(true);
        pair.sender.setConnected(true);
        pair.dropApplyReply = false;
        QVERIFY(pair.sender.requestAutomaticSync());
        QTest::qWait(2100);
        QCOMPARE(pair.destination.books().size(), initialBooks + 1);
        QVERIFY(pair.sender.previewInfo().isEmpty());
        pair.sender.preview(book); // Recovery remains available as an explicit, reviewable user action.
        QTRY_COMPARE(pair.sender.previewInfo().value("action").toString(), QStringLiteral("conflict"));
        QVERIFY(!pair.sender.previewInfo().value("automatic").toBool());
        pair.sender.cancel();
        QCOMPARE(pair.destination.books().size(), initialBooks + 1);
    }

    void policyRevocationLetsAnAtomicApplyAlreadyInFlightFinish()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Already committing"));
        const QString book = pair.source.bookId();
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        QVERIFY(pair.source.appendStroke(stroke()));
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        pair.sender.setPinned(book, true);
        pair.dropApplyReply = true;
        QVERIFY(pair.sender.requestAutomaticSync());
        QTRY_VERIFY_WITH_TIMEOUT(pair.sender.applying(), 5000);
        QTRY_COMPARE(pair.destination.bookTransferRevision(book), pair.source.bookTransferRevision(book));
        pair.sender.setAutomaticAllowed(false);
        QVERIFY(pair.sender.applying());
        pair.dropApplyReply = false;
        pair.receiver->receiveMessage(pair.lastApply, {});
        QTRY_VERIFY(pair.sender.completed());
        QVERIFY(!pair.sender.busy());
        QCOMPARE(pair.destination.bookTransferRevision(book), pair.source.bookTransferRevision(book));
    }

    void reconnectRequiresFreshPreviewAndConfirmation()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Reconnect"));
        pair.sender.preview(pair.source.bookId());
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        const QByteArray before = pair.destination.exportLibraryForTransfer();
        pair.sender.setConnected(false);
        pair.receiver->setConnected(false);
        QVERIFY(pair.sender.hasPendingTransfer());
        pair.sender.setConnected(true);
        pair.receiver->setConnected(true);
        QTRY_VERIFY_WITH_TIMEOUT(!pair.sender.previewInfo().isEmpty(), 4000);
        QVERIFY(!pair.sender.completed());
        QVERIFY(pair.lastApply.isEmpty());
        QCOMPARE(pair.destination.exportLibraryForTransfer(), before);
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        QVERIFY(!pair.sender.hasPendingTransfer());
    }

    void continuousSyncWaitsForEditorAndDoesNotEchoAcknowledgedChanges()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Continuous changes"));
        const QString book = pair.source.bookId();
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        pair.sender.setAutoSync(true);
        pair.sender.setPinned(book, true);
        pair.sender.setEditorBusy(true);
        QVERIFY(pair.source.appendStroke(stroke()));
        const QString changedRevision = pair.source.bookTransferRevision(book);
        const QString before = pair.destination.bookTransferRevision(book);
        QTest::qWait(1900);
        QCOMPARE(pair.destination.bookTransferRevision(book), before);
        pair.sender.setEditorBusy(false);
        QTRY_COMPARE_WITH_TIMEOUT(pair.destination.bookTransferRevision(book), changedRevision, 5000);
        QTRY_VERIFY(pair.sender.completed());
        // The receiver can enable the same policy without echoing the just-
        // acknowledged revision back to the source or showing another review.
        pair.receiver->setPinned(book, true);
        pair.receiver->setAutoSync(true);
        QSignalSpy outgoing(pair.receiver.get(), &BleSync::sendMessage);
        QTest::qWait(1900);
        QCOMPARE(outgoing.size(), 0);
        QCOMPARE(pair.source.bookTransferRevision(book), changedRevision);
        QCOMPARE(pair.sender.pendingAutomaticCount(), 0);
    }

    void transientRemoteEditorBusyRetriesWithFreshPreviewAndPolicyGate()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Retry after editing"));
        const QString book = pair.source.bookId();
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        const QByteArray before = pair.destination.exportBookForTransfer(book);
        QVERIFY(pair.source.appendStroke(stroke()));
        pair.receiver->setEditorBusy(true);
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        pair.sender.setPinned(book, true);
        QVERIFY(pair.sender.requestAutomaticSync());
        QTRY_VERIFY_WITH_TIMEOUT(pair.sender.automaticRetryPending(), 5000);
        QCOMPARE(pair.sender.pendingAutomaticCount(), 1);
        QCOMPARE(pair.destination.exportBookForTransfer(book), before);
        const QString rejectedRequest = pair.lastApply.value("request").toString();
        QVERIFY(!rejectedRequest.isEmpty());
        pair.sender.setAutomaticAllowed(false);
        pair.receiver->setEditorBusy(false);
        QTest::qWait(2200); // The first retry is now due, but the gate remains closed.
        QCOMPARE(pair.lastApply.value("request").toString(), rejectedRequest);
        QCOMPARE(pair.destination.exportBookForTransfer(book), before);
        pair.sender.setAutomaticAllowed(true);
        QTRY_COMPARE_WITH_TIMEOUT(pair.destination.bookTransferRevision(book), pair.source.bookTransferRevision(book), 5000);
        QTRY_VERIFY(pair.sender.completed());
        QVERIFY(pair.lastApply.value("request").toString() != rejectedRequest);
        QVERIFY(!pair.sender.automaticRetryPending());
        QCOMPARE(pair.sender.pendingAutomaticCount(), 0);
    }

    void restoredAutomaticPreviewRespectsGateAndNeverReplaysApproval()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Pending automatic first transfer"));
        const QString book = pair.source.bookId();
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        pair.sender.setPinned(book, true);
        QVERIFY(pair.sender.requestAutomaticSync());
        QTRY_COMPARE_WITH_TIMEOUT(pair.sender.previewInfo().value("action").toString(), QStringLiteral("create"), 5000);
        const QByteArray before = pair.destination.exportLibraryForTransfer();

        BleSync restored(&pair.source, nullptr, directory.filePath("sender"));
        restored.setAutomaticAllowed(false);
        restored.setPeerIdentity(QString(32, QLatin1Char('a')));
        restored.setTriggeredAutomaticSync(true); // Same order as startup QML bindings.
        QCOMPARE(restored.pendingAutomaticCount(), 1);
        connect(&restored, &BleSync::sendMessage, pair.receiver.get(), [&](const QVariantMap &m, const QByteArray &p) {
            pair.receiver->receiveMessage(m, p);
        }, Qt::QueuedConnection);
        connect(pair.receiver.get(), &BleSync::sendMessage, &restored, [&](const QVariantMap &m, const QByteArray &p) {
            restored.receiveMessage(m, p);
        }, Qt::QueuedConnection);
        restored.setConnected(true);
        QTest::qWait(1900);
        QVERIFY(restored.previewInfo().isEmpty());
        QCOMPARE(pair.destination.exportLibraryForTransfer(), before);
        restored.setAutomaticAllowed(true);
        QTRY_COMPARE_WITH_TIMEOUT(restored.previewInfo().value("action").toString(), QStringLiteral("create"), 5000);
        QVERIFY(restored.previewInfo().value("automatic").toBool());
        QCOMPARE(pair.destination.exportLibraryForTransfer(), before);
        QVERIFY(pair.lastApply.isEmpty());
    }

    void transientRetriesAreBoundedAcrossReconnectAndManualRecoveryRemainsAvailable()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Bounded retry"));
        const QString book = pair.source.bookId();
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        const QByteArray before = pair.destination.exportBookForTransfer(book);
        QVERIFY(pair.source.appendStroke(stroke()));
        pair.receiver->setEditorBusy(true);
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        pair.sender.setPinned(book, true);
        int applyRequests = 0;
        connect(&pair.sender, &BleSync::sendMessage, &pair.sender, [&](const QVariantMap &m, const QByteArray &) {
            if (m.value("type") == QStringLiteral("apply")) ++applyRequests;
        });
        QVERIFY(pair.sender.requestAutomaticSync());
        QTRY_VERIFY_WITH_TIMEOUT(pair.sender.automaticRetryPending(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(applyRequests, 4, 18000); // Initial attempt, then 2/4/8-second retries.
        QTRY_VERIFY(!pair.sender.busy() && !pair.sender.automaticRetryPending());
        QCOMPARE(pair.sender.pendingAutomaticCount(), 0);
        QCOMPARE(pair.destination.exportBookForTransfer(book), before);
        pair.sender.setConnected(false);
        pair.receiver->setConnected(false);
        pair.receiver->setConnected(true);
        pair.sender.setConnected(true);
        QVERIFY(pair.sender.requestAutomaticSync()); // Same reconnect catch-up used by QML.
        QTest::qWait(2100);
        QCOMPARE(applyRequests, 4);
        pair.receiver->setEditorBusy(false);
        pair.sender.retryPendingTransfer();
        QTRY_COMPARE(pair.sender.previewInfo().value("action").toString(), QStringLiteral("replace"));
        QVERIFY(!pair.sender.previewInfo().value("automatic").toBool());
        QCOMPARE(pair.destination.exportBookForTransfer(book), before);
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        QCOMPARE(pair.destination.bookTransferRevision(book), pair.source.bookTransferRevision(book));
    }

    void removingBookClearsContinuousRetryWithoutDeletingRemoteCopy()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Removed while waiting"));
        const QString book = pair.source.bookId();
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        const QByteArray before = pair.destination.exportBookForTransfer(book);
        QVERIFY(pair.source.appendStroke(stroke()));
        pair.receiver->setEditorBusy(true);
        pair.sender.setPinned(book, true);
        pair.sender.setAutoSync(true); // Continuous mode.
        QTRY_VERIFY_WITH_TIMEOUT(pair.sender.automaticRetryPending(), 5000);
        QVERIFY(pair.source.trashBook(book));
        QTRY_VERIFY_WITH_TIMEOUT(!pair.sender.automaticRetryPending(), 5000);
        QCOMPARE(pair.sender.pendingAutomaticCount(), 0);
        QVERIFY(!pair.sender.busy());
        QCOMPARE(pair.destination.exportBookForTransfer(book), before);
    }

    void overlappingTriggersMergeNewPinsAndLatestRequestedRevisions()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QStringList books;
        for (const QString &title : {QStringLiteral("Queued A"), QStringLiteral("Queued B"), QStringLiteral("New pin C")}) {
            QVERIFY(pair.source.createBook(title));
            books.append(pair.source.bookId());
            pair.sender.preview(pair.source.bookId());
            QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
            pair.sender.apply();
            QTRY_VERIFY(pair.sender.completed());
        }
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        for (int i = 0; i < 2; ++i) {
            pair.sender.setPinned(books[i], true);
            QVERIFY(pair.source.openBook(books[i]));
            QVERIFY(pair.source.appendStroke(stroke()));
        }
        QVERIFY(pair.sender.requestAutomaticSync());
        QCOMPARE(pair.sender.pendingAutomaticCount(), 2);
        QVERIFY(pair.source.openBook(books[1]));
        QVERIFY(pair.source.appendStroke(stroke(.5)));
        QVERIFY(pair.source.openBook(books[2]));
        QVERIFY(pair.source.appendStroke(stroke(.6)));
        pair.sender.setPinned(books[2], true);
        QVERIFY(pair.sender.requestAutomaticSync()); // Accepted while the previous batch is still queued.
        QCOMPARE(pair.sender.pendingAutomaticCount(), 3);
        for (const auto &book : books)
            QTRY_COMPARE_WITH_TIMEOUT(pair.destination.bookTransferRevision(book), pair.source.bookTransferRevision(book), 6500);
        QTRY_VERIFY(pair.sender.completed());
        QCOMPARE(pair.sender.pendingAutomaticCount(), 0);
        const QString sentRevision = pair.source.bookTransferRevision(books[1]);
        QVERIFY(pair.source.openBook(books[1]));
        QVERIFY(pair.source.appendStroke(stroke(.8)));
        QTest::qWait(1900);
        QCOMPARE(pair.destination.bookTransferRevision(books[1]), sentRevision);
    }

    void failedApplyCheckpointLeavesReviewAndDoesNotSendUntilStorageRecovers()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Checkpoint failure"));
        const QString book = pair.source.bookId();
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        const QVariantMap previousApply = pair.lastApply;
        const QByteArray before = pair.destination.exportBookForTransfer(book);
        QVERIFY(pair.source.appendStroke(stroke()));
        const QString preferences = directory.filePath("sender/preferences.ini");
        const QString saved = preferences + QStringLiteral(".saved");
        bool storageBlocked = false;
        connect(&pair.sender, &BleSync::bookStatusChanged, &pair.sender, [&] {
            if (storageBlocked || pair.sender.previewInfo().value("action") != QStringLiteral("replace")) return;
            if (QFile::rename(preferences, saved)) storageBlocked = QDir().mkdir(preferences);
        });
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setPinned(book, true);
        pair.sender.setAutoSync(true);
        QVERIFY(pair.sender.requestAutomaticSync());
        QTRY_VERIFY_WITH_TIMEOUT(storageBlocked && !pair.sender.previewInfo().isEmpty(), 5000);
        QVERIFY(!pair.sender.applying());
        QVERIFY(!pair.sender.status().isEmpty());
        QCOMPARE(pair.lastApply, previousApply);
        QCOMPARE(pair.destination.exportBookForTransfer(book), before);
        QVERIFY(QDir().rmdir(preferences));
        QVERIFY(QFile::rename(saved, preferences));
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        QCOMPARE(pair.destination.bookTransferRevision(book), pair.source.bookTransferRevision(book));
    }

    void queuedSnapshotSurvivesRestartButChangedSourceNeedsNewTrigger()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Offline snapshot"));
        const QString book = pair.source.bookId();
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        pair.sender.setPinned(book, true);
        QVERIFY(pair.sender.requestAutomaticSync());
        QCOMPARE(pair.sender.pendingAutomaticCount(), 1);
        pair.sender.setConnected(false); // Disconnect before the initial delayed preview.
        QVERIFY(pair.source.appendStroke(stroke()));
        BleSync restored(&pair.source, nullptr, directory.filePath("sender"));
        restored.setPeerIdentity(QString(32, QLatin1Char('a')));
        restored.setTriggeredAutomaticSync(true);
        QCOMPARE(restored.pendingAutomaticCount(), 1);
        int previewsSent = 0;
        connect(&restored, &BleSync::sendMessage, &restored, [&](const QVariantMap &m, const QByteArray &) {
            if (m.value("type") == QStringLiteral("preview") || m.value("type") == QStringLiteral("deltaOffer"))
                ++previewsSent;
        });
        restored.setConnected(true);
        QTRY_COMPARE_WITH_TIMEOUT(restored.pendingAutomaticCount(), 0, 5000);
        QCOMPARE(previewsSent, 0); // Neither the old nor an unrequested new revision is sent.
        QVERIFY(!pair.destination.hasBookId(book));
        QVERIFY(restored.requestAutomaticSync());
        QCOMPARE(restored.pendingAutomaticCount(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(previewsSent, 1, 5000);
    }

    void forgettingConnectedPeerRemovesPersistedAutomaticQueue()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Forgotten peer"));
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        pair.sender.setPinned(pair.source.bookId(), true);
        QVERIFY(pair.sender.requestAutomaticSync());
        QCOMPARE(pair.sender.pendingAutomaticCount(), 1);
        pair.sender.forgetPeer();
        QVERIFY(!pair.sender.connected());
        QCOMPARE(pair.sender.pendingAutomaticCount(), 0);
        BleSync restored(&pair.source, nullptr, directory.filePath("sender"));
        restored.setPeerIdentity(QString(32, QLatin1Char('a')));
        QVERIFY(!restored.autoSync());
        QVERIFY(restored.pinnedBooks().isEmpty());
        QCOMPARE(restored.pendingAutomaticCount(), 0);
        QVERIFY(!restored.hasPendingTransfer());
    }

    void crashDuringAutomaticApplyRequiresReviewBeforeAnotherConflictCopy()
    {
        QTemporaryDir directory;
        Pair pair(directory.path());
        QVERIFY(pair.source.createBook("Crash checkpoint"));
        const QString book = pair.source.bookId();
        pair.sender.preview(book);
        QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
        pair.sender.apply();
        QTRY_VERIFY(pair.sender.completed());
        QVERIFY(pair.source.appendStroke(stroke()));
        QVERIFY(pair.destination.openBook(book));
        QVERIFY(pair.destination.appendStroke(stroke(.6)));
        const QByteArray remoteOriginal = pair.destination.exportBookForTransfer(book);
        const int originalCount = pair.destination.books().size();
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        pair.sender.setPinned(book, true);
        pair.sender.setConflictPolicy("keepBoth");
        pair.dropApplyReply = true;
        QVERIFY(pair.sender.requestAutomaticSync());
        QTRY_COMPARE_WITH_TIMEOUT(pair.destination.books().size(), originalCount + 1, 5000);
        QVERIFY(pair.sender.applying());
        // Deliberately omit setConnected(false): a crash cannot run its cleanup.
        BleSync restored(&pair.source, nullptr, directory.filePath("sender"));
        restored.setPeerIdentity(QString(32, QLatin1Char('a')));
        restored.setTriggeredAutomaticSync(true);
        restored.setConflictPolicy("keepBoth");
        connect(&restored, &BleSync::sendMessage, pair.receiver.get(), [&](const QVariantMap &m, const QByteArray &p) {
            pair.receiver->receiveMessage(m, p);
        }, Qt::QueuedConnection);
        connect(pair.receiver.get(), &BleSync::sendMessage, &restored, [&](const QVariantMap &m, const QByteArray &p) {
            restored.receiveMessage(m, p);
        }, Qt::QueuedConnection);
        restored.setConnected(true);
        QVERIFY(restored.requestAutomaticSync());
        QTest::qWait(2100);
        QVERIFY(restored.previewInfo().isEmpty());
        QCOMPARE(restored.pendingAutomaticCount(), 0);
        QCOMPARE(pair.destination.books().size(), originalCount + 1);
        QCOMPARE(pair.destination.exportBookForTransfer(book), remoteOriginal);
        restored.retryPendingTransfer(); // Explicit recovery is always a fresh manual review.
        QTRY_COMPARE(restored.previewInfo().value("action").toString(), QStringLiteral("conflict"));
        QVERIFY(!restored.previewInfo().value("automatic").toBool());
        QCOMPARE(pair.destination.books().size(), originalCount + 1);
    }

    void interruptedApplyCheckpointNeverAllowsAutomaticDuplicate_data()
    {
        QTest::addColumn<bool>("automatic");
        QTest::addColumn<bool>("legacy");
        QTest::newRow("legacy-automatic") << true << true;
        QTest::newRow("legacy-manual") << false << true;
        QTest::newRow("current-automatic") << true << false;
        QTest::newRow("current-manual") << false << false;
    }

    void interruptedApplyCheckpointNeverAllowsAutomaticDuplicate()
    {
        QFETCH(bool, automatic);
        QFETCH(bool, legacy);
        QTemporaryDir directory;
        Pair pair(directory.path());
        QStringList books;
        for (const QString &title : {QStringLiteral("Uncertain old apply"), QStringLiteral("Other selected notebook")}) {
            QVERIFY(pair.source.createBook(title));
            books.append(pair.source.bookId());
            pair.sender.preview(pair.source.bookId());
            QTRY_VERIFY(!pair.sender.previewInfo().isEmpty());
            pair.sender.apply();
            QTRY_VERIFY(pair.sender.completed());
        }
        QVERIFY(pair.source.openBook(books[0]));
        QVERIFY(pair.source.appendStroke(stroke()));
        QVERIFY(pair.destination.openBook(books[0]));
        QVERIFY(pair.destination.appendStroke(stroke(.6)));
        const QByteArray remoteOriginal = pair.destination.exportBookForTransfer(books[0]);
        QVERIFY(pair.source.openBook(books[1]));
        QVERIFY(pair.source.appendStroke(stroke(.8)));
        pair.sender.setTriggeredAutomaticSync(true);
        pair.sender.setAutoSync(true);
        pair.sender.setConflictPolicy("keepBoth");
        for (const auto &book : books) pair.sender.setPinned(book, true);
        const int originalCount = pair.destination.books().size();
        pair.dropApplyReply = true;
        if (automatic) {
            QVERIFY(pair.sender.requestAutomaticSync());
        } else {
            pair.sender.preview(books[0]);
            QTRY_COMPARE(pair.sender.previewInfo().value("action").toString(), QStringLiteral("conflict"));
            pair.sender.apply();
        }
        QTRY_COMPARE_WITH_TIMEOUT(pair.destination.books().size(), originalCount + 1, 5000);
        QVERIFY(pair.sender.applying());
        if (legacy) {
            QSettings state(directory.filePath("sender/preferences.ini"), QSettings::IniFormat);
            state.beginGroup(QStringLiteral("nearbySync/") + QString(32, QLatin1Char('a')));
            // 0.2.8 persisted only the pending book and its automatic/manual flag.
            state.remove("pendingTransferApplying");
            state.remove("pendingTransferRevision");
            state.remove("automaticQueue");
            state.remove("automaticBatchRevisions");
            state.sync();
            QCOMPARE(state.status(), QSettings::NoError);
        }
        BleSync restored(&pair.source, nullptr, directory.filePath("sender"));
        restored.setPeerIdentity(QString(32, QLatin1Char('a')));
        restored.setTriggeredAutomaticSync(true);
        restored.setConflictPolicy("keepBoth");
        connect(&restored, &BleSync::sendMessage, pair.receiver.get(), [&](const QVariantMap &m, const QByteArray &p) {
            pair.receiver->receiveMessage(m, p);
        }, Qt::QueuedConnection);
        connect(pair.receiver.get(), &BleSync::sendMessage, &restored, [&](const QVariantMap &m, const QByteArray &p) {
            restored.receiveMessage(m, p);
        }, Qt::QueuedConnection);
        restored.setConnected(true);
        QVERIFY(restored.requestAutomaticSync());
        QTRY_COMPARE_WITH_TIMEOUT(pair.destination.bookTransferRevision(books[1]), pair.source.bookTransferRevision(books[1]), 5000);
        QTRY_VERIFY(restored.completed());
        QCOMPARE(pair.destination.books().size(), originalCount + 1);
        QCOMPARE(pair.destination.exportBookForTransfer(books[0]), remoteOriginal);
        QCOMPARE(restored.pendingAutomaticCount(), 0);
        restored.preview(books[0]); // Recovery is available through a fresh manual review.
        QTRY_COMPARE(restored.previewInfo().value("action").toString(), QStringLiteral("conflict"));
        QVERIFY(!restored.previewInfo().value("automatic").toBool());
        QCOMPARE(pair.destination.books().size(), originalCount + 1);
    }
};

QTEST_MAIN(BleSyncTests)
#include "blesync_tests.moc"
