#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class TabletPreferences : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(bool automaticOrientation READ automaticOrientation WRITE setAutomaticOrientation NOTIFY automaticOrientationChanged)
    Q_PROPERTY(bool autoDiagramRecognition READ autoDiagramRecognition WRITE setAutoDiagramRecognition NOTIFY autoDiagramRecognitionChanged)
    Q_PROPERTY(int interfaceScale READ interfaceScale WRITE setInterfaceScale NOTIFY interfaceScaleChanged)
    Q_PROPERTY(int idleMinutes READ idleMinutes WRITE setIdleMinutes NOTIFY idleMinutesChanged)
    Q_PROPERTY(bool keepAwakeWhileCharging READ keepAwakeWhileCharging WRITE setKeepAwakeWhileCharging NOTIFY keepAwakeWhileChargingChanged)
    Q_PROPERTY(bool showStatusBar READ showStatusBar WRITE setShowStatusBar NOTIFY showStatusBarChanged)
    Q_PROPERTY(bool showClock READ showClock WRITE setShowClock NOTIFY showClockChanged)
    Q_PROPERTY(bool showPenBattery READ showPenBattery WRITE setShowPenBattery NOTIFY showPenBatteryChanged)
    Q_PROPERTY(bool gestureNavigationEnabled READ gestureNavigationEnabled WRITE setGestureNavigationEnabled NOTIFY gestureNavigationEnabledChanged)
    Q_PROPERTY(bool startFullPage READ startFullPage WRITE setStartFullPage NOTIFY startFullPageChanged)
    Q_PROPERTY(QString timeZoneId READ timeZoneId WRITE setTimeZoneId NOTIFY timeZoneIdChanged)
    Q_PROPERTY(bool clock24Hour READ clock24Hour WRITE setClock24Hour NOTIFY clock24HourChanged)
    Q_PROPERTY(QString usageProfile READ usageProfile WRITE setUsageProfile NOTIFY usageProfileChanged)
    Q_PROPERTY(QString handedness READ handedness WRITE setHandedness NOTIFY handednessChanged)
    Q_PROPERTY(QStringList toolbarActions READ toolbarActions WRITE setToolbarActions NOTIFY toolbarActionsChanged)
    Q_PROPERTY(bool quickPanelEnabled READ quickPanelEnabled WRITE setQuickPanelEnabled NOTIFY quickPanelEnabledChanged)
    Q_PROPERTY(QString doubleTapAction READ doubleTapAction WRITE setDoubleTapAction NOTIFY doubleTapActionChanged)
    Q_PROPERTY(QString swipeAction READ swipeAction WRITE setSwipeAction NOTIFY swipeActionChanged)
    Q_PROPERTY(QString penPressureCurve READ penPressureCurve WRITE setPenPressureCurve NOTIFY penPressureCurveChanged)
    Q_PROPERTY(QString palmRejection READ palmRejection WRITE setPalmRejection NOTIFY palmRejectionChanged)
    Q_PROPERTY(bool lassoKeepInk READ lassoKeepInk WRITE setLassoKeepInk NOTIFY lassoKeepInkChanged)
    Q_PROPERTY(QString lassoInsertTarget READ lassoInsertTarget WRITE setLassoInsertTarget NOTIFY lassoInsertTargetChanged)
    Q_PROPERTY(QString ocrLanguage READ ocrLanguage WRITE setOcrLanguage NOTIFY ocrLanguageChanged)
    Q_PROPERTY(bool ocrNumericMode READ ocrNumericMode WRITE setOcrNumericMode NOTIFY ocrNumericModeChanged)
    Q_PROPERTY(QStringList ocrWordList READ ocrWordList WRITE setOcrWordList NOTIFY ocrWordListChanged)
    Q_PROPERTY(QStringList ocrCorrections READ ocrCorrections WRITE setOcrCorrections NOTIFY ocrCorrectionsChanged)
    Q_PROPERTY(QString shapeTolerance READ shapeTolerance WRITE setShapeTolerance NOTIFY shapeToleranceChanged)
    Q_PROPERTY(QString refreshProfile READ refreshProfile WRITE setRefreshProfile NOTIFY refreshProfileChanged)
    Q_PROPERTY(int readingZoom READ readingZoom WRITE setReadingZoom NOTIFY readingZoomChanged)
    Q_PROPERTY(int readingMargins READ readingMargins WRITE setReadingMargins NOTIFY readingMarginsChanged)
    Q_PROPERTY(int readingOrientation READ readingOrientation WRITE setReadingOrientation NOTIFY readingOrientationChanged)
    Q_PROPERTY(int readingPanSensitivity READ readingPanSensitivity WRITE setReadingPanSensitivity NOTIFY readingPanSensitivityChanged)
    Q_PROPERTY(bool highContrast READ highContrast WRITE setHighContrast NOTIFY highContrastChanged)
    Q_PROPERTY(bool boldLabels READ boldLabels WRITE setBoldLabels NOTIFY boldLabelsChanged)
    Q_PROPERTY(QString wifiAutoSync READ wifiAutoSync WRITE setWifiAutoSync NOTIFY wifiAutoSyncChanged)
    Q_PROPERTY(QString syncTransport READ syncTransport WRITE setSyncTransport NOTIFY syncTransportChanged)
    Q_PROPERTY(int wifiSyncMinutes READ wifiSyncMinutes WRITE setWifiSyncMinutes NOTIFY wifiSyncMinutesChanged)
    Q_PROPERTY(bool wifiChargingOnly READ wifiChargingOnly WRITE setWifiChargingOnly NOTIFY wifiChargingOnlyChanged)
    Q_PROPERTY(QStringList trustedWifiSsids READ trustedWifiSsids WRITE setTrustedWifiSsids NOTIFY trustedWifiSsidsChanged)
    Q_PROPERTY(QString syncConflictPolicy READ syncConflictPolicy WRITE setSyncConflictPolicy NOTIFY syncConflictPolicyChanged)
    Q_PROPERTY(QString backupSchedule READ backupSchedule WRITE setBackupSchedule NOTIFY backupScheduleChanged)
    Q_PROPERTY(int backupRetention READ backupRetention WRITE setBackupRetention NOTIFY backupRetentionChanged)
    Q_PROPERTY(int cacheLimitMiB READ cacheLimitMiB WRITE setCacheLimitMiB NOTIFY cacheLimitMiBChanged)
    Q_PROPERTY(QString appleWritingDestination READ appleWritingDestination WRITE setAppleWritingDestination NOTIFY appleWritingDestinationChanged)
    Q_PROPERTY(QString appleTableDestination READ appleTableDestination WRITE setAppleTableDestination NOTIFY appleTableDestinationChanged)
    Q_PROPERTY(QString appleChartDestination READ appleChartDestination WRITE setAppleChartDestination NOTIFY appleChartDestinationChanged)
    Q_PROPERTY(QString appleBookDestination READ appleBookDestination WRITE setAppleBookDestination NOTIFY appleBookDestinationChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit TabletPreferences(QObject *parent = nullptr);
    explicit TabletPreferences(const QString &settingsFile, QObject *parent = nullptr);

    int orientation() const { return m_values.value(QStringLiteral("orientation")).toInt(); }
    bool automaticOrientation() const { return m_values.value(QStringLiteral("automaticOrientation")).toBool(); }
    bool autoDiagramRecognition() const { return m_values.value(QStringLiteral("autoDiagramRecognition")).toBool(); }
    int interfaceScale() const { return m_values.value(QStringLiteral("interfaceScale")).toInt(); }
    int idleMinutes() const { return m_values.value(QStringLiteral("idleMinutes")).toInt(); }
    bool keepAwakeWhileCharging() const { return m_values.value(QStringLiteral("keepAwakeWhileCharging")).toBool(); }
    bool showStatusBar() const { return m_values.value(QStringLiteral("showStatusBar")).toBool(); }
    bool showClock() const { return m_values.value(QStringLiteral("showClock")).toBool(); }
    bool showPenBattery() const { return m_values.value(QStringLiteral("showPenBattery")).toBool(); }
    bool gestureNavigationEnabled() const { return m_values.value(QStringLiteral("gestureNavigationEnabled")).toBool(); }
    bool startFullPage() const { return m_values.value(QStringLiteral("startFullPage")).toBool(); }
    QString timeZoneId() const { return m_values.value(QStringLiteral("timeZoneId")).toString(); }
    bool clock24Hour() const { return m_values.value(QStringLiteral("clock24Hour")).toBool(); }
    QString usageProfile() const { return m_values.value(QStringLiteral("usageProfile")).toString(); }
    QString handedness() const { return m_values.value(QStringLiteral("handedness")).toString(); }
    QStringList toolbarActions() const { return m_values.value(QStringLiteral("toolbarActions")).toStringList(); }
    bool quickPanelEnabled() const { return m_values.value(QStringLiteral("quickPanelEnabled")).toBool(); }
    QString doubleTapAction() const { return m_values.value(QStringLiteral("doubleTapAction")).toString(); }
    QString swipeAction() const { return m_values.value(QStringLiteral("swipeAction")).toString(); }
    QString penPressureCurve() const { return m_values.value(QStringLiteral("penPressureCurve")).toString(); }
    QString palmRejection() const { return m_values.value(QStringLiteral("palmRejection")).toString(); }
    bool lassoKeepInk() const { return m_values.value(QStringLiteral("lassoKeepInk")).toBool(); }
    QString lassoInsertTarget() const { return m_values.value(QStringLiteral("lassoInsertTarget")).toString(); }
    QString ocrLanguage() const { return m_values.value(QStringLiteral("ocrLanguage")).toString(); }
    bool ocrNumericMode() const { return m_values.value(QStringLiteral("ocrNumericMode")).toBool(); }
    QStringList ocrWordList() const { return m_values.value(QStringLiteral("ocrWordList")).toStringList(); }
    QStringList ocrCorrections() const { return m_values.value(QStringLiteral("ocrCorrections")).toStringList(); }
    QString shapeTolerance() const { return m_values.value(QStringLiteral("shapeTolerance")).toString(); }
    QString refreshProfile() const { return m_values.value(QStringLiteral("refreshProfile")).toString(); }
    int readingZoom() const { return m_values.value(QStringLiteral("readingZoom")).toInt(); }
    int readingMargins() const { return m_values.value(QStringLiteral("readingMargins")).toInt(); }
    int readingOrientation() const { return m_values.value(QStringLiteral("readingOrientation")).toInt(); }
    int readingPanSensitivity() const { return m_values.value(QStringLiteral("readingPanSensitivity")).toInt(); }
    bool highContrast() const { return m_values.value(QStringLiteral("highContrast")).toBool(); }
    bool boldLabels() const { return m_values.value(QStringLiteral("boldLabels")).toBool(); }
    QString wifiAutoSync() const { return m_values.value(QStringLiteral("wifiAutoSync")).toString(); }
    QString syncTransport() const { return m_values.value(QStringLiteral("syncTransport")).toString(); }
    int wifiSyncMinutes() const { return m_values.value(QStringLiteral("wifiSyncMinutes")).toInt(); }
    bool wifiChargingOnly() const { return m_values.value(QStringLiteral("wifiChargingOnly")).toBool(); }
    QStringList trustedWifiSsids() const { return m_values.value(QStringLiteral("trustedWifiSsids")).toStringList(); }
    QString syncConflictPolicy() const { return m_values.value(QStringLiteral("syncConflictPolicy")).toString(); }
    QString backupSchedule() const { return m_values.value(QStringLiteral("backupSchedule")).toString(); }
    int backupRetention() const { return m_values.value(QStringLiteral("backupRetention")).toInt(); }
    int cacheLimitMiB() const { return m_values.value(QStringLiteral("cacheLimitMiB")).toInt(); }
    QString appleWritingDestination() const { return m_values.value(QStringLiteral("appleWritingDestination")).toString(); }
    QString appleTableDestination() const { return m_values.value(QStringLiteral("appleTableDestination")).toString(); }
    QString appleChartDestination() const { return m_values.value(QStringLiteral("appleChartDestination")).toString(); }
    QString appleBookDestination() const { return m_values.value(QStringLiteral("appleBookDestination")).toString(); }
    QString error() const { return m_error; }

    void setOrientation(int value);
    void setAutomaticOrientation(bool value);
    void setAutoDiagramRecognition(bool value);
    void setInterfaceScale(int value);
    void setIdleMinutes(int value);
    void setKeepAwakeWhileCharging(bool value);
    void setShowStatusBar(bool value);
    void setShowClock(bool value);
    void setShowPenBattery(bool value);
    void setGestureNavigationEnabled(bool value);
    void setStartFullPage(bool value);
    void setTimeZoneId(QString value);
    void setClock24Hour(bool value);
    void setUsageProfile(QString value);
    void setHandedness(QString value);
    void setToolbarActions(QStringList value);
    void setQuickPanelEnabled(bool value);
    void setDoubleTapAction(QString value);
    void setSwipeAction(QString value);
    void setPenPressureCurve(QString value);
    void setPalmRejection(QString value);
    void setLassoKeepInk(bool value);
    void setLassoInsertTarget(QString value);
    void setOcrLanguage(QString value);
    void setOcrNumericMode(bool value);
    void setOcrWordList(QStringList value);
    void setOcrCorrections(QStringList value);
    void setShapeTolerance(QString value);
    void setRefreshProfile(QString value);
    void setReadingZoom(int value);
    void setReadingMargins(int value);
    void setReadingOrientation(int value);
    void setReadingPanSensitivity(int value);
    void setHighContrast(bool value);
    void setBoldLabels(bool value);
    void setWifiAutoSync(QString value);
    void setSyncTransport(QString value);
    void setWifiSyncMinutes(int value);
    void setWifiChargingOnly(bool value);
    void setTrustedWifiSsids(QStringList value);
    void setSyncConflictPolicy(QString value);
    void setBackupSchedule(QString value);
    void setBackupRetention(int value);
    void setCacheLimitMiB(int value);
    void setAppleWritingDestination(QString value);
    void setAppleTableDestination(QString value);
    void setAppleChartDestination(QString value);
    void setAppleBookDestination(QString value);
    Q_INVOKABLE void resetDisplay();
    Q_INVOKABLE void applyUsageProfile(const QString &profile);
    Q_INVOKABLE QString formatClock() const;
    Q_INVOKABLE QString formatDateTime(const QDateTime &dateTime) const;

signals:
    void orientationChanged();
    void automaticOrientationChanged();
    void autoDiagramRecognitionChanged();
    void interfaceScaleChanged();
    void idleMinutesChanged();
    void keepAwakeWhileChargingChanged();
    void showStatusBarChanged();
    void showClockChanged();
    void showPenBatteryChanged();
    void gestureNavigationEnabledChanged();
    void startFullPageChanged();
    void timeZoneIdChanged();
    void clock24HourChanged();
    void usageProfileChanged();
    void usageProfileApplied();
    void handednessChanged();
    void toolbarActionsChanged();
    void quickPanelEnabledChanged();
    void doubleTapActionChanged();
    void swipeActionChanged();
    void penPressureCurveChanged();
    void palmRejectionChanged();
    void lassoKeepInkChanged();
    void lassoInsertTargetChanged();
    void ocrLanguageChanged();
    void ocrNumericModeChanged();
    void ocrWordListChanged();
    void ocrCorrectionsChanged();
    void shapeToleranceChanged();
    void refreshProfileChanged();
    void readingZoomChanged();
    void readingMarginsChanged();
    void readingOrientationChanged();
    void readingPanSensitivityChanged();
    void highContrastChanged();
    void boldLabelsChanged();
    void wifiAutoSyncChanged();
    void syncTransportChanged();
    void wifiSyncMinutesChanged();
    void wifiChargingOnlyChanged();
    void trustedWifiSsidsChanged();
    void syncConflictPolicyChanged();
    void backupScheduleChanged();
    void backupRetentionChanged();
    void cacheLimitMiBChanged();
    void appleWritingDestinationChanged();
    void appleTableDestinationChanged();
    void appleChartDestinationChanged();
    void appleBookDestinationChanged();
    void errorChanged();

private:
    bool applyValues(const QVariantMap &values);
    bool saveValues(const QVariantMap &values);
    void setError(const QString &message);
    const QString m_settingsFile;
    QVariantMap m_values;
    QString m_error;
};
