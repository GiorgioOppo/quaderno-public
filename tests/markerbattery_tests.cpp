#include "deviceinfo.h"
#include "markerbattery.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {
bool writeAttribute(const QString &root, const QString &supply, const QString &name, const QByteArray &value)
{
    const QString directory = QDir(root).filePath(supply);
    if (!QDir().mkpath(directory))
        return false;
    QFile file(QDir(directory).filePath(name));
    return file.open(QIODevice::WriteOnly) && file.write(value) == value.size();
}

bool supply(const QString &root, const QString &name, const QByteArray &present,
            const QByteArray &capacity, const QByteArray &status, const QByteArray &online = {})
{
    return writeAttribute(root, name, QStringLiteral("type"), "Wireless\n")
        && writeAttribute(root, name, QStringLiteral("present"), present)
        && writeAttribute(root, name, QStringLiteral("capacity"), capacity)
        && writeAttribute(root, name, QStringLiteral("status"), status)
        && (online.isEmpty() || writeAttribute(root, name, QStringLiteral("online"), online));
}

const QString elan = QStringLiteral("elants-marker-battery");
const QString nfc = QStringLiteral("nfc-marker-battery");
}

class MarkerBatteryTests : public QObject {
    Q_OBJECT
private slots:
    void missingAndUnrelatedSuppliesAreUnavailable()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto reading = MarkerBattery::readPowerSupplies(directory.path());
        QVERIFY(!reading.available);
        QCOMPARE(reading.percent, -1);
        QVERIFY(supply(directory.path(), QStringLiteral("max1726x_battery"), "1", "100", "Charging"));
        QVERIFY(supply(directory.path(), QStringLiteral("other-wireless-device"), "1", "70", "Charging"));
        QVERIFY(writeAttribute(directory.path(), elan, QStringLiteral("type"), "Battery"));
        reading = MarkerBattery::readPowerSupplies(directory.path());
        QVERIFY(!reading.available);
        QVERIFY(!reading.charging);
    }

    void absentIgnoresFirmwareStoredPercentages()
    {
        QTemporaryDir directory;
        QVERIFY(supply(directory.path(), elan, "0", "95", "Unknown"));
        QVERIFY(supply(directory.path(), nfc, "0", "0", "Unknown", "0"));
        auto reading = MarkerBattery::readPowerSupplies(directory.path());
        QVERIFY(reading.available);
        QVERIFY(!reading.present);
        QCOMPARE(reading.percent, -1);
        QCOMPARE(reading.state, MarkerBattery::State::Absent);
        // Even a contradictory stored NFC online/charging value cannot make an
        // explicitly absent Marker appear present or give it a percentage.
        QVERIFY(writeAttribute(directory.path(), nfc, QStringLiteral("online"), "1"));
        QVERIFY(writeAttribute(directory.path(), nfc, QStringLiteral("status"), "Charging"));
        reading = MarkerBattery::readPowerSupplies(directory.path());
        QCOMPARE(reading.percent, -1);
        QVERIFY(!reading.charging);
        QVERIFY(!reading.present);
    }

    void liveZeroIsDifferentFromUnknown()
    {
        QTemporaryDir directory;
        QVERIFY(supply(directory.path(), elan, "1", "0", "Discharging"));
        auto reading = MarkerBattery::readPowerSupplies(directory.path());
        QVERIFY(reading.present);
        QCOMPARE(reading.percent, 0);
        QCOMPARE(reading.state, MarkerBattery::State::Discharging);
        QVERIFY(writeAttribute(directory.path(), elan, QStringLiteral("present"), "0"));
        reading = MarkerBattery::readPowerSupplies(directory.path());
        QCOMPARE(reading.percent, -1);
        QVERIFY(!reading.present);
    }

    void invalidCapacity_data()
    {
        QTest::addColumn<QByteArray>("capacity");
        QTest::newRow("missing") << QByteArray{};
        QTest::newRow("negative") << QByteArray("-1");
        QTest::newRow("above100") << QByteArray("101");
        QTest::newRow("float") << QByteArray("12.5");
        QTest::newRow("text") << QByteArray("Unknown");
        QTest::newRow("signed") << QByteArray("+5");
        QTest::newRow("embedded-null") << QByteArray("95\0", 3);
        QTest::newRow("oversized") << QByteArray(97, '9');
    }

    void invalidCapacity()
    {
        QFETCH(QByteArray, capacity);
        QTemporaryDir directory;
        QVERIFY(supply(directory.path(), elan, "1", capacity, "Discharging"));
        const auto reading = MarkerBattery::readPowerSupplies(directory.path());
        QVERIFY(reading.present);
        QCOMPARE(reading.percent, -1);
    }

    void unknownPresenceNeverUsesCapacity()
    {
        QTemporaryDir directory;
        QVERIFY(supply(directory.path(), elan, "?", "95", "Charging"));
        const auto reading = MarkerBattery::readPowerSupplies(directory.path());
        QVERIFY(reading.available);
        QVERIFY(!reading.present);
        QVERIFY(!reading.charging);
        QCOMPARE(reading.percent, -1);
        QCOMPARE(reading.state, MarkerBattery::State::Unknown);
    }

    void nfcNeedsOnlineAndTakesPriorityOnlyWhenCurrent()
    {
        QTemporaryDir directory;
        QVERIFY(supply(directory.path(), elan, "1", "61", "Discharging"));
        QVERIFY(supply(directory.path(), nfc, "1", "80", "Charging", "0"));
        auto reading = MarkerBattery::readPowerSupplies(directory.path());
        QCOMPARE(reading.percent, 61);
        QVERIFY(!reading.charging);
        QVERIFY(writeAttribute(directory.path(), nfc, QStringLiteral("online"), "1"));
        reading = MarkerBattery::readPowerSupplies(directory.path());
        QCOMPARE(reading.percent, 80);
        QVERIFY(reading.charging);
        QCOMPARE(reading.state, MarkerBattery::State::Charging);
        QVERIFY(writeAttribute(directory.path(), nfc, QStringLiteral("capacity"), "100"));
        QVERIFY(writeAttribute(directory.path(), nfc, QStringLiteral("status"), "Full"));
        reading = MarkerBattery::readPowerSupplies(directory.path());
        QCOMPARE(reading.percent, 100);
        QVERIFY(!reading.charging);
        QCOMPARE(reading.state, MarkerBattery::State::Full);
        // Do not combine a current charger status with another source's older
        // percentage when NFC explicitly says its capacity is unknown.
        QVERIFY(writeAttribute(directory.path(), nfc, QStringLiteral("capacity_level"), "Unknown"));
        reading = MarkerBattery::readPowerSupplies(directory.path());
        QCOMPARE(reading.percent, -1);
    }

    void offlineNfcAloneHasUnknownCapacity()
    {
        QTemporaryDir directory;
        QVERIFY(supply(directory.path(), nfc, "1", "95", "Charging", "0"));
        const auto reading = MarkerBattery::readPowerSupplies(directory.path());
        QVERIFY(reading.available);
        QVERIFY(reading.present);
        QVERIFY(!reading.charging);
        QCOMPARE(reading.percent, -1);
        QCOMPARE(reading.state, MarkerBattery::State::Unknown);
    }

    void dockedDaemonUsesFirmwareDisplayValue()
    {
        const MarkerBattery::Reading stale{true, false, -1, false, MarkerBattery::State::Absent};
        auto reading = MarkerBattery::fromService(100, true, false, stale);
        QVERIFY(reading.available);
        QVERIFY(reading.present);
        QCOMPARE(reading.percent, 100);
        QVERIFY(!reading.charging);
        QCOMPARE(reading.state, MarkerBattery::State::Full);
        reading = MarkerBattery::fromService(0, true, true, stale);
        QCOMPARE(reading.percent, 0);
        QVERIFY(reading.charging);
        QCOMPARE(reading.state, MarkerBattery::State::Charging);
    }

    void malformedServiceCapacity_data()
    {
        QTest::addColumn<QVariant>("capacity");
        QTest::newRow("missing") << QVariant{};
        QTest::newRow("negative") << QVariant(-1);
        QTest::newRow("overflow") << QVariant(101);
        QTest::newRow("string") << QVariant(QStringLiteral("95"));
        QTest::newRow("boolean") << QVariant(true);
        QTest::newRow("double") << QVariant(95.0);
    }

    void malformedServiceCapacity()
    {
        QFETCH(QVariant, capacity);
        const auto reading = MarkerBattery::fromService(capacity, true, false, {});
        QVERIFY(reading.available);
        QVERIFY(reading.present);
        QCOMPARE(reading.percent, -1);
    }

    void undockedDaemonNeverPublishesCachedCapacity()
    {
        auto reading = MarkerBattery::fromService(100, false, true, {});
        QVERIFY(reading.available);
        QVERIFY(!reading.present);
        QVERIFY(!reading.charging);
        QCOMPARE(reading.percent, -1);
        const MarkerBattery::Reading liveElan{true, true, 63, false, MarkerBattery::State::Discharging};
        reading = MarkerBattery::fromService(100, false, false, liveElan);
        QCOMPARE(reading.percent, 63);
        QCOMPARE(reading.state, MarkerBattery::State::Discharging);
    }

    void serviceErrorsAndInvalidTypesFallBackWithoutCoercion()
    {
        const MarkerBattery::Reading fallback{true, true, 42, false, MarkerBattery::State::Discharging};
        QCOMPARE(MarkerBattery::fromService({}, {}, {}, fallback).percent, 42);
        QCOMPARE(MarkerBattery::fromService(100, QStringLiteral("true"), true, fallback).percent, 42);
        const auto reading = MarkerBattery::fromService(80, true, QStringLiteral("true"), fallback);
        QCOMPARE(reading.percent, 80);
        QVERIFY(!reading.charging);
        QCOMPARE(reading.state, MarkerBattery::State::Unknown);
    }

    void unchangedRefreshDoesNotNotifyButNewErrorDoes()
    {
        DeviceInfo device(false);
        QSignalSpy changes(&device, &DeviceInfo::changed);
        QVERIFY(!device.markerAvailable());
        QCOMPARE(device.markerBatteryPercent(), -1);
        device.refresh();
        device.refreshMarker();
        QCOMPARE(changes.count(), 0);
        QVERIFY(!device.setFrontlightLevel(2));
        QCOMPARE(changes.count(), 1);
        QVERIFY(!device.error().isEmpty());
        device.refresh();
        device.refreshMarker();
        QCOMPARE(changes.count(), 1);
    }
};

QTEST_GUILESS_MAIN(MarkerBatteryTests)
#include "markerbattery_tests.moc"
