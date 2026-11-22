#include "syncpackage.h"
#include "deltatransfer.h"
#include "notebookformat.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <memory>

namespace {
QByteArray bytes(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QByteArray book(int revision = 1, int assetBytes = 256 * 1024)
{
    const QString image(assetBytes, QLatin1Char('A'));
    const QJsonObject table{{QStringLiteral("type"), QStringLiteral("table")},
        {QStringLiteral("cells"), QJsonArray{QJsonArray{QStringLiteral("=SUM(A1:A4)"), revision}}}};
    const QJsonObject page1{{QStringLiteral("items"), QJsonArray{table}}, {QStringLiteral("background"), image}};
    const QJsonObject page2{{QStringLiteral("items"), QJsonArray{QJsonObject{
        {QStringLiteral("text"), QString::fromUtf8("Caffè — line one\nSeconda riga")},
        {QStringLiteral("imageData"), image}}}}};
    const QJsonObject source{{QStringLiteral("kind"), QStringLiteral("epub")}, {QStringLiteral("data"), image}};
    const QJsonObject document{
        {QStringLiteral("id"), QStringLiteral("book-one")}, {QStringLiteral("title"), QStringLiteral("Book")},
        {QStringLiteral("pages"), QJsonArray{page1, page2}},
        {QStringLiteral("sourceDocuments"), QJsonObject{{QStringLiteral("source-one"), source}}}};
    return bytes({{QStringLiteral("version"), 5}, {QStringLiteral("books"), QJsonArray{document}}});
}

QVariantMap previewMetadata()
{
    return {{QStringLiteral("type"), QStringLiteral("preview")},
        {QStringLiteral("request"), QUuid::createUuid().toString(QUuid::Id128)},
        {QStringLiteral("bookId"), QStringLiteral("book-one")}, {QStringLiteral("base"), QString{}}};
}

struct Link {
    std::unique_ptr<DeltaTransfer> sender;
    std::unique_ptr<DeltaTransfer> receiver;
    QByteArray received;
    QByteArray local;
    qint64 sentBytes = 0;
    int chunks = 0;
    int allowedChunks = -1;
    bool resumed = false;
    bool corruptChunk = false;
    int failed = 0;

    explicit Link(const QString &directory)
    {
        sender = std::make_unique<DeltaTransfer>(nullptr, nullptr, directory + QStringLiteral("/sender"));
        receiver = std::make_unique<DeltaTransfer>([this](const QString &) { return local; }, nullptr,
                                                  directory + QStringLiteral("/receiver"));
        QObject::connect(sender.get(), &DeltaTransfer::sendMessage, receiver.get(), [this](QVariantMap metadata, QByteArray payload) {
            if (metadata.value(QStringLiteral("type")).toString() == QStringLiteral("deltaChunk")) {
                if (allowedChunks >= 0 && chunks >= allowedChunks)
                    return;
                ++chunks;
                sentBytes += payload.size();
                if (corruptChunk && !payload.isEmpty())
                    payload[0] = char(payload[0] ^ 1);
            }
            receiver->receiveMessage(metadata, payload);
        }, Qt::QueuedConnection);
        QObject::connect(receiver.get(), &DeltaTransfer::sendMessage, sender.get(), [this](const QVariantMap &metadata, const QByteArray &payload) {
            if (metadata.value(QStringLiteral("type")).toString() == QStringLiteral("deltaNeed")
                && metadata.value(QStringLiteral("offset")).toLongLong() > 0)
                resumed = true;
            sender->receiveMessage(metadata, payload);
        }, Qt::QueuedConnection);
        QObject::connect(receiver.get(), &DeltaTransfer::previewReady, receiver.get(), [this](const QVariantMap &metadata, const QByteArray &payload) {
            received = payload;
            sender->cancelOutgoing(metadata.value(QStringLiteral("request")).toString());
        });
        QObject::connect(sender.get(), &DeltaTransfer::failed, sender.get(), [this] { ++failed; });
        sender->setConnected(true, QString(32, QLatin1Char('a')));
        receiver->setConnected(true, QString(32, QLatin1Char('b')));
    }
};
}

class SyncTests final : public QObject
{
    Q_OBJECT
private slots:
    void advertisesNotebookReaderVersion()
    {
        QTemporaryDir directory;
        DeltaTransfer transfer(nullptr, nullptr, directory.path());
        QSignalSpy sent(&transfer, &DeltaTransfer::sendMessage);
        transfer.setConnected(true, QString(32, QLatin1Char('c')));
        QTRY_VERIFY(!sent.isEmpty());
        const auto hello = sent.first().first().toMap();
        QCOMPARE(hello.value("type").toString(), QStringLiteral("syncHello"));
        QCOMPARE(hello.value("maxNotebookVersion").toInt(), NotebookFormat::maximumSupportedVersion);
    }

    void negotiatesReaderBeforeSendingAssets_data()
    {
        QTest::addColumn<QVariant>("peerVersion");
        QTest::addColumn<int>("payloadVersion");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("historical-delta-reader-nine") << QVariant{} << 9 << true;
        QTest::newRow("historical-delta-reader-cannot-read-ten") << QVariant{} << 10 << false;
        QTest::newRow("explicit-nine-cannot-read-notes") << QVariant{9} << 11 << false;
        QTest::newRow("explicit-ten-cannot-read-notes") << QVariant{10} << 11 << false;
        QTest::newRow("current-reader-notes") << QVariant{11} << 11 << true;
        QTest::newRow("future-reader-current-notes") << QVariant{12} << 11 << true;
        QTest::newRow("flowchart-reader-cannot-read-uml")
            << QVariant{NotebookFormat::extendedDiagramShapesVersion} << NotebookFormat::umlDiagramShapesVersion << false;
        QTest::newRow("uml-reader")
            << QVariant{NotebookFormat::umlDiagramShapesVersion} << NotebookFormat::umlDiagramShapesVersion << true;
        QTest::newRow("reader-version-string-fails-closed") << QVariant{"11"} << 9 << false;
        QTest::newRow("reader-version-zero-fails-closed") << QVariant{0} << 9 << false;
        QTest::newRow("reader-version-boolean-fails-closed") << QVariant{true} << 9 << false;
        QTest::newRow("reader-version-fraction-fails-closed") << QVariant{11.5} << 9 << false;
    }

    void negotiatesReaderBeforeSendingAssets()
    {
        QFETCH(QVariant, peerVersion);
        QFETCH(int, payloadVersion);
        QFETCH(bool, accepted);
        QTemporaryDir directory;
        DeltaTransfer transfer(nullptr, nullptr, directory.path());
        transfer.setConnected(true, QString(32, QLatin1Char('c')));
        QVariantMap hello{{"type", "syncHello"}, {"request", QString(32, QLatin1Char('a'))}, {"deltaVersion", 1}};
        if (peerVersion.isValid()) hello.insert("maxNotebookVersion", peerVersion);
        QVERIFY(transfer.receiveMessage(hello, {}));
        QSignalSpy sent(&transfer, &DeltaTransfer::sendMessage);
        QSignalSpy failed(&transfer, &DeltaTransfer::failed);
        auto root = QJsonDocument::fromJson(book()).object();
        root.insert("version", payloadVersion);
        transfer.sendPreview(previewMetadata(), bytes(root), 8 * 1024 * 1024);
        if (accepted) {
            QCOMPARE(failed.size(), 0);
            QCOMPARE(sent.size(), 1);
            const auto offer = sent.first().first().toMap();
            QCOMPARE(offer.value("type").toString(), QStringLiteral("deltaOffer"));
            QCOMPARE(offer.value("notebookVersion").toInt(), payloadVersion);
        } else {
            QCOMPARE(failed.size(), 1);
            QVERIFY(!failed.first().at(1).toString().isEmpty());
            QVERIFY(sent.isEmpty()); // No manifest or book assets leave for an incompatible reader.
        }
    }

    void extendedFieldsCannotHideBehindOlderEnvelope_data()
    {
        QTest::addColumn<QJsonObject>("item");
        QTest::addColumn<QVariant>("peerVersion");
        QTest::addColumn<int>("declaredVersion");
        QTest::newRow("diagram-style") << QJsonObject{{"type", "diagram"}, {"styleScale", .5}} << QVariant{} << 9;
        QTest::newRow("diagram-recovery")
            << QJsonObject{{"type", "diagram"}, {"handwritingSource", QJsonObject{{"baseDiagram", QJsonObject{}}}}} << QVariant{} << 9;
        QTest::newRow("anchored-note") << QJsonObject{{"type", "note"}, {"text", "Keep this annotation"}} << QVariant{} << 9;
        constexpr int previousVersion = NotebookFormat::extendedDiagramShapesVersion;
        const QJsonObject uml{{"type", "diagram"}, {"nodes", QJsonArray{QJsonObject{{"shape", "uml-class"}}}}};
        QTest::newRow("uml-node") << uml << QVariant{previousVersion} << previousVersion;
        QTest::newRow("uml-recovery-node")
            << QJsonObject{{"type", "diagram"}, {"handwritingSource", QJsonObject{{"baseDiagram", uml}}}}
            << QVariant{previousVersion} << previousVersion;
    }

    void extendedFieldsCannotHideBehindOlderEnvelope()
    {
        QFETCH(QJsonObject, item);
        QFETCH(QVariant, peerVersion);
        QFETCH(int, declaredVersion);
        QTemporaryDir directory;
        DeltaTransfer transfer(nullptr, nullptr, directory.path());
        transfer.setConnected(true, QString(32, QLatin1Char('c')));
        QVariantMap hello{{"type", "syncHello"}, {"request", QString(32, QLatin1Char('a'))}, {"deltaVersion", 1}};
        if (peerVersion.isValid()) hello.insert("maxNotebookVersion", peerVersion);
        QVERIFY(transfer.receiveMessage(hello, {}));
        QSignalSpy sent(&transfer, &DeltaTransfer::sendMessage);
        QSignalSpy failed(&transfer, &DeltaTransfer::failed);
        const QJsonObject document{{"id", "book-one"}, {"title", "Annotations"},
            {"pages", QJsonArray{QJsonObject{{"items", QJsonArray{item}}}}}};
        transfer.sendPreview(previewMetadata(), bytes({{"version", declaredVersion}, {"books", QJsonArray{document}}}), 8 * 1024 * 1024);
        QCOMPARE(failed.size(), 1);
        QVERIFY(sent.isEmpty());
    }

    void incompatibleIncomingOfferDoesNotRequestAssets()
    {
        QTemporaryDir directory;
        DeltaTransfer receiver(nullptr, nullptr, directory.path());
        receiver.setConnected(true, QString(32, QLatin1Char('c')));
        QSignalSpy sent(&receiver, &DeltaTransfer::sendMessage);
        auto metadata = previewMetadata();
        metadata.insert("type", "deltaOffer");
        metadata.insert("notebookVersion", NotebookFormat::maximumSupportedVersion + 1);
        QVERIFY(receiver.receiveMessage(metadata, SyncPackage::split(book()).manifest));
        QCOMPARE(sent.size(), 1);
        const auto response = sent.first().first().toMap();
        QCOMPARE(response.value("type").toString(), QStringLiteral("deltaError"));
        QCOMPARE(response.value("errorCode").toString(), QStringLiteral("format"));
    }

    void packageRoundTrip()
    {
        const QByteArray original = book();
        QString error;
        const auto package = SyncPackage::split(original, &error);
        QVERIFY2(package.isValid(), qPrintable(error));
        QCOMPARE(package.manifest, SyncPackage::split(original).manifest);
        int assets = 0;
        for (const auto &object : package.objects) {
            if (object.size() >= 256 * 1024)
                ++assets;
        }
        QCOMPARE(assets, 1); // Identical image/source bytes are stored once.
        const QByteArray reconstructed = SyncPackage::assemble(package.manifest, [&](const QString &hash) {
            return package.objects.value(hash);
        }, &error);
        QCOMPARE(reconstructed, original);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const auto next = SyncPackage::split(book(2));
        int changed = 0;
        qint64 changedBytes = 0;
        for (auto it = next.objects.cbegin(); it != next.objects.cend(); ++it) {
            if (!package.objects.contains(it.key())) {
                ++changed;
                changedBytes += it.value().size();
            }
        }
        QCOMPARE(changed, 2); // Edited page and root descriptor, no image copy.
        QVERIFY(changedBytes < 4096);
    }

    void multiBookAndEmptyLibrary()
    {
        QJsonObject object = QJsonDocument::fromJson(book()).object();
        QJsonArray books = object.value(QStringLiteral("books")).toArray();
        books.append(books.first());
        object.insert(QStringLiteral("books"), books);
        for (const auto &payload : {bytes(object), bytes({{QStringLiteral("books"), QJsonArray{}}})}) {
            const auto package = SyncPackage::split(payload);
            QVERIFY(package.isValid());
            QCOMPARE(SyncPackage::assemble(package.manifest, [&](const QString &hash) { return package.objects.value(hash); }), payload);
        }
    }

    void rejectsCorruptionAndBounds()
    {
        const auto package = SyncPackage::split(book());
        QString error;
        QVERIFY(SyncPackage::assemble(package.manifest, [](const QString &) { return QByteArray("corrupt"); }, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(SyncPackage::split(book(), &error, 1000).manifest.isEmpty());
        QJsonObject invalid = QJsonDocument::fromJson(package.manifest).object();
        auto entries = invalid.value(QStringLiteral("objects")).toArray();
        entries.append(entries.first());
        invalid.insert(QStringLiteral("objects"), entries);
        QVERIFY(SyncPackage::objectSizes(bytes(invalid), &error).isEmpty());
        invalid = QJsonDocument::fromJson(package.manifest).object();
        invalid.insert(QStringLiteral("payloadBytes"), 1e15);
        QVERIFY(SyncPackage::objectSizes(bytes(invalid), &error).isEmpty());
        invalid = QJsonDocument::fromJson(package.manifest).object();
        invalid.insert(QStringLiteral("payloadHash"), QString(64, QLatin1Char('a')));
        QVERIFY(SyncPackage::assemble(bytes(invalid), [&](const QString &hash) { return package.objects.value(hash); }, &error).isEmpty());
        invalid.insert(QStringLiteral("root"), QStringLiteral("../../outside"));
        QVERIFY(SyncPackage::objectSizes(bytes(invalid), &error).isEmpty());
    }

    void incrementalTransfer()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Link link(directory.path());
        QTRY_VERIFY(link.sender->incremental());
        link.sender->sendPreview(previewMetadata(), book(), 8 * 1024 * 1024);
        QTRY_COMPARE(link.received, book());
        const qint64 initial = link.sentBytes;
        QVERIFY(initial >= 256 * 1024);
        link.received.clear();
        link.sentBytes = 0;
        link.sender->sendPreview(previewMetadata(), book(2), 8 * 1024 * 1024);
        QTRY_COMPARE(link.received, book(2));
        QVERIFY(link.sentBytes < 4096);
        QVERIFY(link.sentBytes < initial / 20);
    }

    void seedsExistingPages()
    {
        QTemporaryDir directory;
        Link link(directory.path());
        link.local = book();
        QTRY_VERIFY(link.sender->incremental());
        link.sender->sendPreview(previewMetadata(), book(2), 8 * 1024 * 1024);
        QTRY_COMPARE(link.received, book(2));
        QVERIFY(link.sentBytes < 4096);
    }

    void resumesAfterProcessRestart()
    {
        QTemporaryDir directory;
        const QByteArray payload = book(1, 1024 * 1024);
        qint64 firstBytes = 0;
        {
            Link link(directory.path());
            link.allowedChunks = 2;
            QTRY_VERIFY(link.sender->incremental());
            link.sender->sendPreview(previewMetadata(), payload, 8 * 1024 * 1024);
            QTRY_COMPARE(link.chunks, 2);
            // Wait for durable writes and their next chunk request.
            QTest::qWait(100);
            QVERIFY(link.received.isEmpty());
            firstBytes = link.sentBytes;
            link.sender->setConnected(false, QString(32, QLatin1Char('a')));
            link.receiver->setConnected(false, QString(32, QLatin1Char('b')));
        }
        Link restarted(directory.path());
        QTRY_VERIFY(restarted.sender->incremental());
        restarted.sender->sendPreview(previewMetadata(), payload, 8 * 1024 * 1024);
        QTRY_COMPARE(restarted.received, payload);
        QVERIFY(restarted.resumed);
        QVERIFY(restarted.sentBytes < 1024 * 1024);
        QVERIFY(firstBytes + restarted.sentBytes < 1024 * 1024 + 4096);
    }

    void damagedChunkDoesNotReachPreview()
    {
        QTemporaryDir directory;
        Link link(directory.path());
        link.corruptChunk = true;
        QTRY_VERIFY(link.sender->incremental());
        link.sender->sendPreview(previewMetadata(), book(), 8 * 1024 * 1024);
        QTRY_COMPARE(link.failed, 1);
        QVERIFY(link.received.isEmpty());
    }

    void legacyFallback()
    {
        QTemporaryDir directory;
        DeltaTransfer transfer(nullptr, nullptr, directory.path());
        QByteArray received;
        connect(&transfer, &DeltaTransfer::sendMessage, this, [&](const QVariantMap &metadata, const QByteArray &payload) {
            if (metadata.value(QStringLiteral("type")).toString() == QStringLiteral("preview"))
                received = payload;
        });
        auto original = QJsonDocument::fromJson(book()).object();
        auto books = original.value(QStringLiteral("books")).toArray();
        auto simple = books.first().toObject();
        simple.remove(QStringLiteral("sourceDocuments"));
        books.replace(0, simple);
        original.insert(QStringLiteral("books"), books);
        const QByteArray compatible = bytes(original);
        transfer.setConnected(true, QString(32, QLatin1Char('c')));
        transfer.sendPreview(previewMetadata(), compatible, 8 * 1024 * 1024);
        QTRY_COMPARE_WITH_TIMEOUT(received, compatible, 3000);
        QVERIFY(!transfer.incremental());
        received.clear();
        QSignalSpy failed(&transfer, &DeltaTransfer::failed);
        transfer.sendPreview(previewMetadata(), book(), 8 * 1024 * 1024);
        QCOMPARE(failed.size(), 1);
        QVERIFY(received.isEmpty()); // Old clients must never receive blank source-backed books.
    }
};

QTEST_GUILESS_MAIN(SyncTests)
#include "sync_tests.moc"
