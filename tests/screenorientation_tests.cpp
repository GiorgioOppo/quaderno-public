#include "screenorientation.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>
#include <limits>

namespace {
bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
}

bool createSensor(const QString &root)
{
    const QDir directory(QDir(root).filePath("iio:device0"));
    return QDir().mkpath(directory.path())
        && writeFile(directory.filePath("name"), "lis2dw12_accel\n")
        && writeFile(directory.filePath("in_accel_mount_matrix"), "0, 1, 0; 1, 0, 0; 0, 0, -1\n")
        && writeFile(directory.filePath("in_accel_x_raw"), "0\n")
        && writeFile(directory.filePath("in_accel_y_raw"), "1000\n")
        && writeFile(directory.filePath("in_accel_z_raw"), "0\n");
}
}

class ScreenOrientationTests final : public QObject
{
    Q_OBJECT
private slots:
    void gravityClassificationRejectsFlatDiagonalInvalidAndAbsentSamples_data()
    {
        QTest::addColumn<QVector3D>("sample");
        QTest::addColumn<int>("angle");
        QTest::newRow("portrait") << QVector3D(0, 1000, 0) << 0;
        QTest::newRow("landscape-right") << QVector3D(1000, 0, 0) << 90;
        QTest::newRow("portrait-inverted") << QVector3D(0, -1000, 0) << 180;
        QTest::newRow("landscape-left") << QVector3D(-1000, 0, 0) << 270;
        QTest::newRow("flat") << QVector3D(50, 20, 1000) << -1;
        QTest::newRow("diagonal") << QVector3D(700, 700, 0) << -1;
        QTest::newRow("zero") << QVector3D() << -1;
        QTest::newRow("nan") << QVector3D(std::numeric_limits<float>::quiet_NaN(), 1000, 0) << -1;
        QTest::newRow("infinity") << QVector3D(0, 1000, std::numeric_limits<float>::infinity()) << -1;
    }

    void gravityClassificationRejectsFlatDiagonalInvalidAndAbsentSamples()
    {
        QFETCH(QVector3D, sample);
        QFETCH(int, angle);
        QCOMPARE(OrientationFilter::classify(sample), angle);
    }

    void stableOrientationNeedsDurationAndSamplesAndSurvivesMovement()
    {
        OrientationFilter filter;
        const QVector3D portrait(0, 1000, 0), landscape(1000, 0, 0);
        QVERIFY(!filter.update(portrait, 0));
        QVERIFY(!filter.update(portrait, 900)); // Two samples alone are insufficient.
        QCOMPARE(filter.angle, -1);
        QVERIFY(filter.update(portrait, 1000));
        QCOMPARE(filter.angle, 0);
        QVERIFY(!filter.update(landscape, 1100));
        QVERIFY(!filter.update(landscape, 1400));
        QVERIFY(!filter.update(landscape, 1700));
        QCOMPARE(filter.angle, 0); // Three samples alone are insufficient.
        QVERIFY(!filter.update(QVector3D(0, 0, 1000), 2000));
        QCOMPARE(filter.angle, 0); // Flat placement preserves the last stable angle.
        QVERIFY(!filter.update(landscape, 2100));
        QVERIFY(!filter.update(QVector3D(4000, 0, 0), 2400)); // A jolt resets the candidate.
        QVERIFY(!filter.update(landscape, 2700)); // Returning magnitude is also unstable.
        QVERIFY(!filter.update(landscape, 3000));
        QVERIFY(!filter.update(landscape, 3500));
        QVERIFY(filter.update(landscape, 4000));
        QCOMPARE(filter.angle, 90);
        QVERIFY(!filter.update(landscape, 4300));
        filter.reset();
        QCOMPARE(filter.angle, -1);
        QVERIFY(!filter.update(portrait, 100));
        QVERIFY(!filter.update(portrait, 50)); // Backward time must restart the stability gate.
        QVERIFY(!filter.update(portrait, 600));
        QVERIFY(filter.update(portrait, 950));
    }

    void interruptedSamplingRequiresANewStableSequence()
    {
        OrientationFilter filter;
        const QVector3D portrait(0, 1000, 0), landscape(1000, 0, 0);
        QVERIFY(!filter.update(portrait, 0));
        QVERIFY(!filter.update(portrait, 400));
        QVERIFY(filter.update(portrait, 900));
        QVERIFY(!filter.update(landscape, 1200));
        QVERIFY(!filter.update(landscape, 1600));
        QVERIFY(!filter.update(landscape, 4000));
        QCOMPARE(filter.angle, 0);
        QVERIFY(!filter.update(landscape, 4400));
        QVERIFY(filter.update(landscape, 4900));
        QCOMPARE(filter.angle, 90);
    }

    void mountMatrixAcceptsVerifiedPermutationAndRejectsInvalidCalibration()
    {
        std::array<double, 9> matrix{};
        QVERIFY(ScreenOrientation::parseMountMatrix("0,1,0;1,0,0;0,0,-1", &matrix));
        QCOMPARE(matrix[1], 1.0);
        QCOMPARE(matrix[3], 1.0);
        QCOMPARE(matrix[8], -1.0);
        const auto before = matrix;
        for (const QString &invalid : {QString("1,0,0;1,0,0;0,0,1"), QString("1,0,0;0,0,0;0,0,1"),
             QString("1,0,0;0,1,0;0,0,0.5"), QString("1,0;0,1"), QString("1,0,0;0,nan,0;0,0,1")}) {
            QVERIFY(!ScreenOrientation::parseMountMatrix(invalid, &matrix));
            QCOMPARE(matrix, before);
        }
        QVERIFY(!ScreenOrientation::parseMountMatrix("1,0,0;0,1,0;0,0,1", nullptr));
    }

    void sensorIsReadOnlyOptInAppliesMountingAndIgnoresQueuedReadAfterDisabling()
    {
        QTemporaryDir directory;
        QVERIFY(createSensor(directory.path()));
        const QString sensorPath = directory.filePath("iio:device0");
        QVERIFY(writeFile(QDir(sensorPath).filePath("sampling_frequency"), "12\n"));
        ScreenOrientation sensor(true, nullptr, directory.path());
        QVERIFY(sensor.available());
        QVERIFY(!sensor.enabled());
        QCOMPARE(sensor.orientation(), -1);
        QSignalSpy requests(&sensor, &ScreenOrientation::sampleRequested);
        QTest::qWait(400);
        QCOMPARE(requests.size(), 0);
        sensor.setEnabled(true);
        QTRY_COMPARE_WITH_TIMEOUT(sensor.orientation(), 90, 3500);
        QVERIFY(requests.size() >= 3);
        QFile frequency(QDir(sensorPath).filePath("sampling_frequency"));
        QVERIFY(frequency.open(QIODevice::ReadOnly));
        QCOMPARE(frequency.readAll(), QByteArray("12\n"));
        sensor.setEnabled(false);
        QCOMPARE(sensor.orientation(), 90);
        const auto count = requests.size();
        QTest::qWait(700);
        QCOMPARE(requests.size(), count);
        sensor.setEnabled(true); // Queue a read and revoke immediately.
        sensor.setEnabled(false);
        QTest::qWait(200);
        QCOMPARE(sensor.orientation(), 90);
        QVERIFY(!sensor.enabled());
        ScreenOrientation mac(false, nullptr, directory.path());
        mac.setEnabled(true);
        QVERIFY(!mac.enabled());
        QVERIFY(!mac.available());
    }

    void missingOrFailedSensorPreservesLastStableAngleAndReportsUnavailable()
    {
        QTemporaryDir directory;
        ScreenOrientation absent(true, nullptr, directory.path());
        QVERIFY(!absent.available());
        QVERIFY(!absent.status().isEmpty());
        QVERIFY(createSensor(directory.path()));
        ScreenOrientation sensor(true, nullptr, directory.path());
        sensor.setEnabled(true);
        QTRY_COMPARE_WITH_TIMEOUT(sensor.orientation(), 90, 3500);
        QVERIFY(writeFile(directory.filePath("iio:device0/in_accel_y_raw"), "unavailable\n"));
        QTRY_VERIFY_WITH_TIMEOUT(!sensor.available(), 2500);
        QCOMPARE(sensor.orientation(), 90);
        QVERIFY(!sensor.status().isEmpty());
    }
};

QTEST_GUILESS_MAIN(ScreenOrientationTests)
#include "screenorientation_tests.moc"
