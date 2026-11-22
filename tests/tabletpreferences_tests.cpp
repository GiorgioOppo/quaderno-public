#include "tabletpreferences.h"
#include "paperprostandby.h"
#include "pinlock.h"
#include "deviceinfo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {
QByteArray readBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
}

class TabletPreferencesTests : public QObject
{
    Q_OBJECT
    QTemporaryDir defaultSettings;

private slots:
    void initTestCase()
    {
        QVERIFY(defaultSettings.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, defaultSettings.path());
    }

    void defaultsDoNotCreateAFileAndDefaultConstructorUsesAppNamespace()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("preferences.ini");
        {
            TabletPreferences preferences(path);
            QCOMPARE(preferences.orientation(), 0);
            QCOMPARE(preferences.interfaceScale(), 100);
            QCOMPARE(preferences.idleMinutes(), 20);
            QVERIFY(!preferences.keepAwakeWhileCharging());
            QVERIFY(preferences.showStatusBar());
            QVERIFY(preferences.showClock());
            QVERIFY(preferences.showPenBattery());
            QVERIFY(preferences.gestureNavigationEnabled());
            QVERIFY(!preferences.startFullPage());
            QCOMPARE(preferences.wifiAutoSync(), QStringLiteral("manual"));
            QCOMPARE(preferences.syncTransport(), QStringLiteral("auto"));
            preferences.setOrientation(0);
            preferences.resetDisplay();
        }
        QVERIFY(!QFileInfo::exists(path));
        TabletPreferences defaults;
        defaults.setOrientation(180);
        QVERIFY(defaults.error().isEmpty());
        QSettings saved(QSettings::IniFormat, QSettings::UserScope, "Quaderno", "Quaderno");
        QCOMPARE(saved.value("tablet/orientation").toInt(), 180);
        TabletPreferences reopened;
        QCOMPARE(reopened.orientation(), 180);
    }

    void validPreferencesPersistAndUnchangedOrInvalidValuesDoNotRewrite()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("preferences.ini");
        TabletPreferences preferences(path);
        QSignalSpy orientationChanges(&preferences, &TabletPreferences::orientationChanged);
        preferences.setOrientation(90);
        preferences.setInterfaceScale(130);
        preferences.setIdleMinutes(60);
        preferences.setKeepAwakeWhileCharging(true);
        preferences.setShowStatusBar(false);
        preferences.setShowClock(false);
        preferences.setShowPenBattery(false);
        preferences.setGestureNavigationEnabled(false);
        preferences.setStartFullPage(true);
        QVERIFY(preferences.error().isEmpty());
        const QByteArray saved = readBytes(path);
        QVERIFY(!saved.isEmpty());
        preferences.setOrientation(90);
        for (int invalid : {-90, 45, 360}) preferences.setOrientation(invalid);
        for (int invalid : {0, 114, 120, 131}) preferences.setInterfaceScale(invalid);
        for (int invalid : {0, 1, 3, 120}) preferences.setIdleMinutes(invalid);
        preferences.setShowClock(false);
        QCOMPARE(orientationChanges.count(), 1);
        QCOMPARE(readBytes(path), saved);
        TabletPreferences reopened(path);
        QCOMPARE(reopened.orientation(), 90);
        QCOMPARE(reopened.interfaceScale(), 130);
        QCOMPARE(reopened.idleMinutes(), 60);
        QVERIFY(reopened.keepAwakeWhileCharging());
        QVERIFY(!reopened.showStatusBar());
        QVERIFY(!reopened.showClock());
        QVERIFY(!reopened.showPenBattery());
        QVERIFY(!reopened.gestureNavigationEnabled());
        QVERIFY(reopened.startFullPage());
    }

    void wirelessAutomationChoicesPersist_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<QString>("transport");
        for (const QString &mode : {QStringLiteral("manual"), QStringLiteral("onOpen"),
                                    QStringLiteral("interval"), QStringLiteral("onChange")}) {
            for (const QString &transport : {QStringLiteral("auto"), QStringLiteral("wifi"),
                                             QStringLiteral("bluetooth")}) {
                const QByteArray row = (mode + '-' + transport).toUtf8();
                QTest::newRow(row.constData()) << mode << transport;
            }
        }
    }

    void wirelessAutomationChoicesPersist()
    {
        QFETCH(QString, mode);
        QFETCH(QString, transport);
        QTemporaryDir directory;
        const QString path = directory.filePath("wireless.ini");
        TabletPreferences preferences(path);
        preferences.setWifiAutoSync(mode == "manual" ? "onOpen" : "manual");
        preferences.setSyncTransport(transport == "wifi" ? "auto" : "wifi");
        QSignalSpy modes(&preferences, &TabletPreferences::wifiAutoSyncChanged);
        QSignalSpy transports(&preferences, &TabletPreferences::syncTransportChanged);
        preferences.setWifiAutoSync(mode);
        preferences.setSyncTransport(transport);
        QVERIFY(preferences.error().isEmpty());
        QCOMPARE(preferences.wifiAutoSync(), mode);
        QCOMPARE(preferences.syncTransport(), transport);
        QCOMPARE(modes.count(), 1);
        QCOMPARE(transports.count(), 1);
        const QByteArray saved = readBytes(path);
        QVERIFY(!saved.isEmpty());
        preferences.setWifiAutoSync(mode);
        preferences.setSyncTransport(transport);
        for (const QString &invalid : {QString(), QStringLiteral("always"), QStringLiteral("onchange"),
                                       QStringLiteral("onChange ")})
            preferences.setWifiAutoSync(invalid);
        for (const QString &invalid : {QString(), QStringLiteral("cloud"), QStringLiteral("Bluetooth"),
                                       QStringLiteral("wifi ")})
            preferences.setSyncTransport(invalid);
        QCOMPARE(modes.count(), 1);
        QCOMPARE(transports.count(), 1);
        QCOMPARE(readBytes(path), saved);
        TabletPreferences reopened(path);
        QCOMPARE(reopened.wifiAutoSync(), mode);
        QCOMPARE(reopened.syncTransport(), transport);
    }

    void legacyWirelessAutomationUsesNewTransportDefaultWithoutRewriting()
    {
        QTemporaryDir directory;
        for (const QString &mode : {QStringLiteral("manual"), QStringLiteral("onOpen"),
                                    QStringLiteral("interval")}) {
            const QString path = directory.filePath(mode + ".ini");
            {
                QSettings saved(path, QSettings::IniFormat);
                saved.setValue("tablet/schemaVersion", 2);
                saved.setValue("tablet/wifiAutoSync", mode);
                saved.setValue("tablet/wifiSyncMinutes", 30);
                saved.setValue("tablet/trustedWifiSsids", QStringList{"Home"});
                saved.sync();
                QCOMPARE(saved.status(), QSettings::NoError);
            }
            const QByteArray original = readBytes(path);
            TabletPreferences preferences(path);
            QCOMPARE(preferences.wifiAutoSync(), mode);
            QCOMPARE(preferences.syncTransport(), QStringLiteral("auto"));
            QCOMPARE(preferences.wifiSyncMinutes(), 30);
            QCOMPARE(preferences.trustedWifiSsids(), QStringList{"Home"});
            QCOMPARE(readBytes(path), original);
        }
    }

    void malformedWirelessChoicesFallBackWithoutEnablingAutomation()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("malformed-wireless.ini");
        {
            QSettings saved(path, QSettings::IniFormat);
            saved.setValue("tablet/wifiAutoSync", "always");
            saved.setValue("tablet/syncTransport", "any-network");
            saved.setValue("tablet/wifiChargingOnly", true);
            saved.sync();
            QCOMPARE(saved.status(), QSettings::NoError);
        }
        const QByteArray original = readBytes(path);
        TabletPreferences preferences(path);
        QCOMPARE(preferences.wifiAutoSync(), QStringLiteral("manual"));
        QCOMPARE(preferences.syncTransport(), QStringLiteral("auto"));
        QVERIFY(preferences.wifiChargingOnly());
        QCOMPARE(readBytes(path), original);
        preferences.setWifiAutoSync("onChange");
        preferences.setSyncTransport("bluetooth");
        preferences.resetDisplay();
        TabletPreferences reopened(path);
        QCOMPARE(reopened.wifiAutoSync(), QStringLiteral("onChange"));
        QCOMPARE(reopened.syncTransport(), QStringLiteral("bluetooth"));
        QVERIFY(reopened.wifiChargingOnly());
    }

    void malformedStoredValuesUseSafeDefaultsWithoutOverwritingTheFile()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("preferences.ini");
        {
            QSettings saved(path, QSettings::IniFormat);
            saved.setValue("tablet/orientation", 89.6);
            saved.setValue("tablet/interfaceScale", "large");
            saved.setValue("tablet/idleMinutes", -1);
            saved.setValue("tablet/keepAwakeWhileCharging", "anything");
            saved.setValue("tablet/showStatusBar", 2);
            saved.setValue("tablet/showClock", "broken");
            saved.setValue("tablet/showPenBattery", "false");
            saved.setValue("tablet/gestureNavigationEnabled", "invalid");
            saved.setValue("tablet/startFullPage", "unknown");
            saved.sync();
            QCOMPARE(saved.status(), QSettings::NoError);
        }
        const QByteArray before = readBytes(path);
        {
            TabletPreferences preferences(path);
            QCOMPARE(preferences.orientation(), 0);
            QCOMPARE(preferences.interfaceScale(), 100);
            QCOMPARE(preferences.idleMinutes(), 20);
            QVERIFY(!preferences.keepAwakeWhileCharging());
            QVERIFY(preferences.showStatusBar());
            QVERIFY(preferences.showClock());
            QVERIFY(!preferences.showPenBattery()); // A valid false is distinct from a malformed value.
            QVERIFY(preferences.gestureNavigationEnabled());
            QVERIFY(!preferences.startFullPage());
        }
        QCOMPARE(readBytes(path), before);
    }

    void resetDisplayPreservesPowerPolicyAndUnrelatedSettings()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("preferences.ini");
        {
            QSettings saved(path, QSettings::IniFormat);
            saved.setValue("language", "en");
            saved.setValue("security/marker", "untouched");
            saved.sync();
        }
        TabletPreferences preferences(path);
        preferences.setOrientation(270);
        preferences.setInterfaceScale(115);
        preferences.setIdleMinutes(30);
        preferences.setKeepAwakeWhileCharging(true);
        preferences.setShowStatusBar(false);
        preferences.setShowClock(false);
        preferences.setShowPenBattery(false);
        preferences.setGestureNavigationEnabled(false);
        preferences.setStartFullPage(true);
        QSignalSpy orientationChanges(&preferences, &TabletPreferences::orientationChanged);
        QSignalSpy idleChanges(&preferences, &TabletPreferences::idleMinutesChanged);
        QSignalSpy chargingChanges(&preferences, &TabletPreferences::keepAwakeWhileChargingChanged);
        preferences.resetDisplay();
        preferences.resetDisplay();
        QVERIFY(preferences.error().isEmpty());
        QCOMPARE(orientationChanges.count(), 1);
        QCOMPARE(idleChanges.count(), 0);
        QCOMPARE(chargingChanges.count(), 0);
        TabletPreferences reopened(path);
        QCOMPARE(reopened.orientation(), 0);
        QCOMPARE(reopened.interfaceScale(), 100);
        QVERIFY(reopened.showStatusBar() && reopened.showClock() && reopened.showPenBattery());
        QVERIFY(reopened.gestureNavigationEnabled());
        QVERIFY(!reopened.startFullPage());
        QCOMPARE(reopened.idleMinutes(), 30);
        QVERIFY(reopened.keepAwakeWhileCharging());
        QSettings saved(path, QSettings::IniFormat);
        QCOMPARE(saved.value("language").toString(), QStringLiteral("en"));
        QCOMPARE(saved.value("security/marker").toString(), QStringLiteral("untouched"));
    }

    void failedSaveKeepsLiveValuesAndCanBeRetried()
    {
        QTemporaryDir directory;
        const QString blockedDirectory = directory.filePath("blocked");
        QFile blocker(blockedDirectory);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close(); // A file cannot contain preferences.ini, regardless of process privileges.
        const QString path = blockedDirectory + "/preferences.ini";
        TabletPreferences preferences(path);
        QSignalSpy changed(&preferences, &TabletPreferences::orientationChanged);
        QSignalSpy errors(&preferences, &TabletPreferences::errorChanged);
        preferences.setOrientation(90);
        QCOMPARE(preferences.orientation(), 0);
        QCOMPARE(changed.count(), 0);
        QVERIFY(!preferences.error().isEmpty());
        QCOMPARE(errors.count(), 1);
        QVERIFY(QFile::remove(blockedDirectory));
        QVERIFY(QDir().mkpath(blockedDirectory));
        preferences.setOrientation(90);
        QCOMPARE(preferences.orientation(), 90);
        QCOMPARE(changed.count(), 1);
        QVERIFY(preferences.error().isEmpty());
        QCOMPARE(errors.count(), 2);
        TabletPreferences reopened(path);
        QCOMPARE(reopened.orientation(), 90);
    }

    void legacyPreferencesGainDefaultsWithoutRewritingAndRoundTripExtendedValues()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("legacy.ini");
        {
            QSettings saved(path, QSettings::IniFormat);
            saved.setValue("tablet/orientation", 270);
            saved.setValue("tablet/idleMinutes", 30);
            saved.setValue("tablet/showClock", false);
            saved.setValue("security/policy", "preserve-me");
            saved.sync();
        }
        const QByteArray legacyBytes = readBytes(path);
        TabletPreferences preferences(path);
        QCOMPARE(preferences.orientation(), 270);
        QCOMPARE(preferences.idleMinutes(), 30);
        QVERIFY(!preferences.showClock());
        QCOMPARE(preferences.timeZoneId(), QStringLiteral("system"));
        QCOMPARE(preferences.doubleTapAction(), QStringLiteral("zoom"));
        QCOMPARE(preferences.swipeAction(), QStringLiteral("pages"));
        QCOMPARE(preferences.lassoInsertTarget(), QStringLiteral("ask"));
        QCOMPARE(preferences.cacheLimitMiB(), 64);
        QCOMPARE(readBytes(path), legacyBytes);

        preferences.setTimeZoneId("Europe/Rome");
        preferences.setClock24Hour(false);
        preferences.setHandedness("left");
        preferences.setToolbarActions({"lasso", "pen", "table"});
        preferences.setQuickPanelEnabled(false);
        preferences.setDoubleTapAction("fullPage");
        preferences.setSwipeAction("scroll");
        preferences.setPenPressureCurve("light");
        preferences.setPalmRejection("extended");
        preferences.setLassoKeepInk(true);
        preferences.setLassoInsertTarget("cells");
        preferences.setOcrLanguage("it");
        preferences.setOcrNumericMode(true);
        preferences.setOcrWordList({"Quaderno", "macOS"});
        preferences.setOcrCorrections({"Qvaderno=Quaderno", "Pr0=Pro"});
        preferences.setShapeTolerance("strict");
        preferences.setRefreshProfile("fast");
        preferences.setReadingZoom(150);
        preferences.setReadingMargins(16);
        preferences.setReadingOrientation(90);
        preferences.setReadingPanSensitivity(75);
        preferences.setHighContrast(true);
        preferences.setBoldLabels(true);
        preferences.setWifiAutoSync("interval");
        preferences.setWifiSyncMinutes(30);
        preferences.setWifiChargingOnly(true);
        preferences.setTrustedWifiSsids({" Studio ", "Casa"});
        preferences.setSyncConflictPolicy("keepBoth");
        preferences.setBackupSchedule("daily");
        preferences.setBackupRetention(10);
        preferences.setCacheLimitMiB(128);
        preferences.setAppleWritingDestination("files");
        preferences.setAppleTableDestination("files");
        preferences.setAppleChartDestination("files");
        preferences.setAppleBookDestination("files");
        QVERIFY(preferences.error().isEmpty());

        TabletPreferences reopened(path);
        QCOMPARE(reopened.orientation(), 270);
        QCOMPARE(reopened.idleMinutes(), 30);
        QCOMPARE(reopened.timeZoneId(), QStringLiteral("Europe/Rome"));
        QVERIFY(!reopened.clock24Hour());
        QCOMPARE(reopened.handedness(), QStringLiteral("left"));
        QCOMPARE(reopened.toolbarActions(), QStringList({"lasso", "pen", "table"}));
        QVERIFY(!reopened.quickPanelEnabled());
        QCOMPARE(reopened.doubleTapAction(), QStringLiteral("fullPage"));
        QCOMPARE(reopened.swipeAction(), QStringLiteral("scroll"));
        QCOMPARE(reopened.penPressureCurve(), QStringLiteral("light"));
        QCOMPARE(reopened.palmRejection(), QStringLiteral("extended"));
        QVERIFY(reopened.lassoKeepInk());
        QCOMPARE(reopened.lassoInsertTarget(), QStringLiteral("cells"));
        QCOMPARE(reopened.ocrLanguage(), QStringLiteral("it"));
        QVERIFY(reopened.ocrNumericMode());
        QCOMPARE(reopened.ocrWordList(), QStringList({"Quaderno", "macOS"}));
        QCOMPARE(reopened.ocrCorrections(), QStringList({"Qvaderno=Quaderno", "Pr0=Pro"}));
        QCOMPARE(reopened.shapeTolerance(), QStringLiteral("strict"));
        QCOMPARE(reopened.refreshProfile(), QStringLiteral("fast"));
        QCOMPARE(reopened.readingZoom(), 150);
        QCOMPARE(reopened.readingMargins(), 16);
        QCOMPARE(reopened.readingOrientation(), 90);
        QCOMPARE(reopened.readingPanSensitivity(), 75);
        QVERIFY(reopened.highContrast() && reopened.boldLabels());
        QCOMPARE(reopened.wifiAutoSync(), QStringLiteral("interval"));
        QCOMPARE(reopened.wifiSyncMinutes(), 30);
        QVERIFY(reopened.wifiChargingOnly());
        QCOMPARE(reopened.trustedWifiSsids(), QStringList({" Studio ", "Casa"}));
        QCOMPARE(reopened.syncConflictPolicy(), QStringLiteral("keepBoth"));
        QCOMPARE(reopened.backupSchedule(), QStringLiteral("daily"));
        QCOMPARE(reopened.backupRetention(), 10);
        QCOMPARE(reopened.cacheLimitMiB(), 128);
        QCOMPARE(reopened.appleWritingDestination(), QStringLiteral("files"));
        QCOMPARE(reopened.appleTableDestination(), QStringLiteral("files"));
        QCOMPARE(reopened.appleChartDestination(), QStringLiteral("files"));
        QCOMPARE(reopened.appleBookDestination(), QStringLiteral("files"));
        QSettings saved(path, QSettings::IniFormat);
        QCOMPARE(saved.value("tablet/schemaVersion").toInt(), 2);
        QCOMPARE(saved.value("security/policy").toString(), QStringLiteral("preserve-me"));
    }

    void timeZoneFormattingHandlesDaylightSavingAndInvalidInput()
    {
        QTemporaryDir directory;
        TabletPreferences preferences(directory.filePath("clock.ini"));
        preferences.setTimeZoneId("Europe/Rome");
        const QDateTime winter = QDateTime::fromString("2026-01-15T12:07:00Z", Qt::ISODate);
        const QDateTime summer = QDateTime::fromString("2026-07-15T12:07:00Z", Qt::ISODate);
        QCOMPARE(preferences.formatDateTime(winter), QStringLiteral("13:07"));
        QCOMPARE(preferences.formatDateTime(summer), QStringLiteral("14:07"));
        preferences.setClock24Hour(false);
        QCOMPARE(preferences.formatDateTime(summer), QStringLiteral("2:07 PM"));
        preferences.setTimeZoneId("UTC");
        QCOMPARE(preferences.formatDateTime(winter), QStringLiteral("12:07 PM"));
        preferences.setTimeZoneId("Not/A_Timezone");
        QCOMPARE(preferences.timeZoneId(), QStringLiteral("UTC"));
        QVERIFY(preferences.formatDateTime({}).isEmpty());
        preferences.setTimeZoneId("system");
        QCOMPARE(preferences.formatDateTime(winter), winter.toLocalTime().toString("h:mm AP"));
        QVERIFY(!preferences.formatClock().isEmpty());
    }

    void listsAndUnsupportedValuesAreValidatedBeforePersistence()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("validation.ini");
        TabletPreferences preferences(path);
        preferences.setOcrWordList({" Quaderno ", "macOS", "Quaderno"});
        QCOMPARE(preferences.ocrWordList(), QStringList({"Quaderno", "macOS"}));
        preferences.setTrustedWifiSsids({" Home ", "Home", " Home "});
        QCOMPARE(preferences.trustedWifiSsids(), QStringList({" Home ", "Home"}));
        preferences.setOcrCorrections({"foo=bar"});
        preferences.setToolbarActions({"lasso", "pen", "lasso"});
        QCOMPARE(preferences.toolbarActions(), QStringList({"lasso", "pen"}));
        const QByteArray saved = readBytes(path);
        QSignalSpy words(&preferences, &TabletPreferences::ocrWordListChanged);
        QSignalSpy toolbar(&preferences, &TabletPreferences::toolbarActionsChanged);
        preferences.setOcrWordList({QString(129, u'a')});
        preferences.setOcrWordList({"word\nsecond"});
        preferences.setOcrCorrections({"missing separator"});
        preferences.setOcrCorrections({"=replacement"});
        preferences.setOcrCorrections({"source= "});
        preferences.setTrustedWifiSsids({QString::fromUtf8("è").repeated(17)});
        preferences.setTrustedWifiSsids({"hidden\tcontrol"});
        preferences.setToolbarActions({"pen", "unknown"});
        preferences.setToolbarActions({});
        preferences.setReadingZoom(75);
        preferences.setReadingMargins(100);
        preferences.setReadingOrientation(360);
        preferences.setWifiAutoSync("always");
        preferences.setSyncConflictPolicy("overwrite");
        preferences.setBackupRetention(0);
        preferences.setCacheLimitMiB(1024);
        preferences.setAppleBookDestination("invalid-app");
        QCOMPARE(words.count(), 0);
        QCOMPARE(toolbar.count(), 0);
        QCOMPARE(readBytes(path), saved);
        preferences.setOcrWordList({});
        preferences.setOcrCorrections({});
        preferences.setTrustedWifiSsids({});
        TabletPreferences reopened(path);
        QVERIFY(reopened.ocrWordList().isEmpty());
        QVERIFY(reopened.ocrCorrections().isEmpty());
        QVERIFY(reopened.trustedWifiSsids().isEmpty());
    }

    void malformedExtendedSettingsFallBackAndFutureSchemaIsNotDowngraded()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("malformed.ini");
        {
            QSettings saved(path, QSettings::IniFormat);
            saved.setValue("tablet/schemaVersion", 99);
            saved.setValue("tablet/timeZoneId", "invalid-zone");
            saved.setValue("tablet/clock24Hour", "sometimes");
            saved.setValue("tablet/readingZoom", 125.5);
            saved.setValue("tablet/palmRejection", "aggressive");
            saved.setValue("tablet/toolbarActions", QStringList{"pen", "bad-action"});
            saved.setValue("tablet/ocrCorrections", QStringList{"broken"});
            saved.setValue("tablet/backupRetention", -2);
            saved.setValue("tablet/futureOption", "untouched");
            saved.sync();
        }
        const QByteArray original = readBytes(path);
        TabletPreferences preferences(path);
        QCOMPARE(preferences.timeZoneId(), QStringLiteral("system"));
        QVERIFY(preferences.clock24Hour());
        QCOMPARE(preferences.readingZoom(), 100);
        QCOMPARE(preferences.palmRejection(), QStringLiteral("writing"));
        QCOMPARE(preferences.toolbarActions().size(), 8);
        QVERIFY(preferences.ocrCorrections().isEmpty());
        QCOMPARE(preferences.backupRetention(), 5);
        QCOMPARE(readBytes(path), original);
        preferences.setHighContrast(true);
        QSettings saved(path, QSettings::IniFormat);
        QCOMPARE(saved.value("tablet/schemaVersion").toInt(), 99);
        QCOMPARE(saved.value("tablet/futureOption").toString(), QStringLiteral("untouched"));
    }

    void profilesPublishCoherentDefaultsAndDisplayResetPreservesOtherPolicies()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("profiles.ini");
        TabletPreferences preferences(path);
        preferences.setIdleMinutes(60);
        preferences.setKeepAwakeWhileCharging(true);
        preferences.setPenPressureCurve("firm");
        preferences.setOcrLanguage("en");
        preferences.setLassoKeepInk(true);
        preferences.setOcrWordList({"Quaderno"});
        preferences.setWifiAutoSync("interval");
        preferences.setBackupSchedule("weekly");
        preferences.setCacheLimitMiB(256);
        preferences.setAppleWritingDestination("files");
        bool coherentSnapshot = false;
        connect(&preferences, &TabletPreferences::usageProfileChanged, &preferences, [&] {
            if (preferences.usageProfile() == "drawing")
                coherentSnapshot = preferences.startFullPage() && !preferences.gestureNavigationEnabled()
                    && preferences.toolbarActions() == QStringList({"pen", "eraser", "lasso", "shapes"});
        });
        preferences.applyUsageProfile("drawing");
        QVERIFY(coherentSnapshot);
        preferences.setUsageProfile("reading");
        QVERIFY(preferences.startFullPage() && preferences.gestureNavigationEnabled());
        QCOMPARE(preferences.toolbarActions(), QStringList({"pen", "highlighter", "eraser", "lasso"}));
        preferences.applyUsageProfile("invalid");
        QCOMPARE(preferences.usageProfile(), QStringLiteral("reading"));
        preferences.setHighContrast(true);
        preferences.setBoldLabels(true);
        preferences.setTimeZoneId("UTC");
        preferences.setClock24Hour(false);
        preferences.setReadingMargins(24);
        preferences.setReadingOrientation(90);
        preferences.setReadingZoom(200);
        preferences.setDoubleTapAction("none");
        preferences.setSwipeAction("scroll");
        preferences.resetDisplay();
        TabletPreferences reopened(path);
        QCOMPARE(reopened.usageProfile(), QStringLiteral("writing"));
        QCOMPARE(reopened.toolbarActions().size(), 8);
        QCOMPARE(reopened.timeZoneId(), QStringLiteral("system"));
        QVERIFY(reopened.clock24Hour());
        QVERIFY(!reopened.highContrast() && !reopened.boldLabels() && !reopened.startFullPage());
        QVERIFY(reopened.gestureNavigationEnabled());
        QCOMPARE(reopened.readingMargins(), 0);
        QCOMPARE(reopened.readingOrientation(), -1);
        QCOMPARE(reopened.readingZoom(), 100);
        QCOMPARE(reopened.doubleTapAction(), QStringLiteral("zoom"));
        QCOMPARE(reopened.swipeAction(), QStringLiteral("pages"));
        QCOMPARE(reopened.idleMinutes(), 60);
        QVERIFY(reopened.keepAwakeWhileCharging());
        QCOMPARE(reopened.penPressureCurve(), QStringLiteral("firm"));
        QCOMPARE(reopened.ocrLanguage(), QStringLiteral("en"));
        QVERIFY(reopened.lassoKeepInk());
        QCOMPARE(reopened.ocrWordList(), QStringList({"Quaderno"}));
        QCOMPARE(reopened.wifiAutoSync(), QStringLiteral("interval"));
        QCOMPARE(reopened.backupSchedule(), QStringLiteral("weekly"));
        QCOMPARE(reopened.cacheLimitMiB(), 256);
        QCOMPARE(reopened.appleWritingDestination(), QStringLiteral("files"));
    }

    void failedProfileSaveDoesNotPartiallyChangeLivePreferences()
    {
        QTemporaryDir directory;
        const QString blocked = directory.filePath("blocked");
        QFile blocker(blocked);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        TabletPreferences preferences(blocked + "/preferences.ini");
        QSignalSpy profile(&preferences, &TabletPreferences::usageProfileChanged);
        QSignalSpy tools(&preferences, &TabletPreferences::toolbarActionsChanged);
        preferences.applyUsageProfile("drawing");
        QVERIFY(!preferences.error().isEmpty());
        QCOMPARE(preferences.usageProfile(), QStringLiteral("writing"));
        QCOMPARE(preferences.toolbarActions().size(), 8);
        QVERIFY(!preferences.startFullPage() && preferences.gestureNavigationEnabled());
        QCOMPARE(profile.count(), 0);
        QCOMPARE(tools.count(), 0);
    }

    void chargingPreferenceDoesNotDisableExplicitSleepOrPin()
    {
        QTemporaryDir directory;
        PinLock pin(directory.filePath("pin.json"));
        QVERIFY(pin.setPin("1234"));
        QVERIFY(pin.setAutoLockMinutes(1));
        DeviceInfo device(false);
        PaperProStandby standby(true, &pin, &device);
        QSignalSpy changed(&standby, &PaperProStandby::keepAwakeWhileChargingChanged);
        standby.setKeepAwakeWhileCharging(true);
        standby.setKeepAwakeWhileCharging(true);
        QCOMPARE(changed.count(), 1);
        QVERIFY(standby.keepAwakeWhileCharging());
        QCOMPARE(pin.autoLockMinutes(), 1);
        QVERIFY(!pin.locked());
        standby.suspend();
        QVERIFY(standby.sleeping());
        QVERIFY(pin.locked());
        standby.wake();
        QVERIFY(!standby.sleeping());
        QVERIFY(pin.locked());
        standby.setKeepAwakeWhileCharging(false);
        QVERIFY(pin.locked());
        QCOMPARE(pin.autoLockMinutes(), 1);
    }
};

QTEST_GUILESS_MAIN(TabletPreferencesTests)
#include "tabletpreferences_tests.moc"
