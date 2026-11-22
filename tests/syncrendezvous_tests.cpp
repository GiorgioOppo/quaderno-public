#include "syncrendezvous.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using SyncRendezvous::Kind;
using SyncRendezvous::Message;

class SyncRendezvousTests final : public QObject
{
    Q_OBJECT

private slots:
    void roundTripThroughChannelFormat()
    {
        const auto session = SyncRendezvous::newSessionId();
        QVERIFY(session != SyncRendezvous::newSessionId());
        const Message messages[] = {
            {Kind::Hello, session, true, false}, {Kind::Work, session, false, true},
            {Kind::Offer, session, false, false, QStringLiteral("192.168.50.20")},
            {Kind::Ready, session}, {Kind::Commit, session}, {Kind::Abort, session}
        };
        for (const auto &message : messages) {
            const auto metadata = SyncRendezvous::encode(message);
            const auto json = QJsonDocument(QJsonObject::fromVariantMap(metadata)).toJson(QJsonDocument::Compact);
            const auto received = QJsonDocument::fromJson(json).object().toVariantMap();
            QVERIFY(SyncRendezvous::isControlMessage(received));
            const auto decoded = SyncRendezvous::decode(received, {});
            QVERIFY(decoded.has_value());
            QCOMPARE(decoded->kind, message.kind);
            QCOMPARE(decoded->session, message.session);
            QCOMPARE(decoded->work, message.work);
            QCOMPARE(decoded->wifi, message.wifi);
            QCOMPARE(decoded->address, message.address);
        }
    }

    void rejectsMalformedControlMessages()
    {
        const auto hello = SyncRendezvous::encode({Kind::Hello, SyncRendezvous::newSessionId(), true, true});
        QVERIFY(!SyncRendezvous::decode(hello, QByteArray("unexpected")));
        const QList<QPair<QString, QVariant>> changes{
            {QStringLiteral("version"), 2},
            {QStringLiteral("version"), 1.5},
            {QStringLiteral("version"), QStringLiteral("1")},
            {QStringLiteral("phase"), QStringLiteral("unknown")},
            {QStringLiteral("session"), QString(32, u'A')},
            {QStringLiteral("session"), QString(32, u'z')},
            {QStringLiteral("session"), QString(33, u'a')},
            {QStringLiteral("session"), QString(31, u'a') + u'\n'},
            {QStringLiteral("work"), 1},
            {QStringLiteral("wifi"), QStringLiteral("true")},
            {QStringLiteral("port"), 47987},
            {QStringLiteral("ssid"), QStringLiteral("LAN")}
        };
        for (const auto &[field, value] : changes) {
            auto mutated = hello;
            mutated.insert(field, value);
            QVERIFY(SyncRendezvous::isControlMessage(mutated));
            QVERIFY2(!SyncRendezvous::decode(mutated, {}), qPrintable(field));
        }
        auto commit = SyncRendezvous::encode({Kind::Commit, SyncRendezvous::newSessionId()});
        commit.insert(QStringLiteral("work"), true);
        QVERIFY(!SyncRendezvous::decode(commit, {}));

        auto swapFields = hello;
        swapFields.remove(QStringLiteral("work"));
        swapFields.insert(QStringLiteral("address"), QStringLiteral("192.168.50.20"));
        QVERIFY(!SyncRendezvous::decode(swapFields, {}));
        QVERIFY(!SyncRendezvous::isControlMessage({{QStringLiteral("type"), QStringLiteral("syncHello")}}));
    }

    void restrictsOffersToCanonicalLocalWifiAddresses()
    {
        const auto session = SyncRendezvous::newSessionId();
        const auto offer = [&session](const QString &address) {
            return SyncRendezvous::decode(SyncRendezvous::encode({Kind::Offer, session, false, false, address}), {});
        };
        for (const auto address : {"192.168.50.20", "10.42.8.10", "172.31.43.25", "169.254.24.2"})
            QVERIFY2(offer(QString::fromLatin1(address)), address);
        for (const auto address : {"", "tablet.local", "8.8.8.8", "127.0.0.1", "0.0.0.0", "10.11.99.1",
                                   "::ffff:192.168.50.20", "192.168.50.20:80", "192.168.50.020", " 192.168.50.20"})
            QVERIFY2(!offer(QString::fromLatin1(address)), address);
    }
};

QTEST_GUILESS_MAIN(SyncRendezvousTests)
#include "syncrendezvous_tests.moc"
