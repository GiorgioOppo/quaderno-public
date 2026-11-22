#include "tabletpreferences.h"

#include <QSettings>
#include <QTimeZone>
#include <algorithm>
#include <initializer_list>
#include <memory>

namespace {
constexpr int schemaVersion = 2;

std::unique_ptr<QSettings> settingsFor(const QString &file)
{
    if (!file.isEmpty())
        return std::make_unique<QSettings>(file, QSettings::IniFormat);
    return std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
        QStringLiteral("Quaderno"), QStringLiteral("Quaderno"));
}

bool allowed(int value, std::initializer_list<int> choices)
{
    return std::find(choices.begin(), choices.end(), value) != choices.end();
}

QVariantMap defaultValues()
{
    return {
        {QStringLiteral("orientation"), 0},
        {QStringLiteral("automaticOrientation"), false},
        {QStringLiteral("autoDiagramRecognition"), true},
        {QStringLiteral("interfaceScale"), 100},
        {QStringLiteral("idleMinutes"), 20},
        {QStringLiteral("keepAwakeWhileCharging"), false},
        {QStringLiteral("showStatusBar"), true},
        {QStringLiteral("showClock"), true},
        {QStringLiteral("showPenBattery"), true},
        {QStringLiteral("gestureNavigationEnabled"), true},
        {QStringLiteral("startFullPage"), false},
        {QStringLiteral("timeZoneId"), QStringLiteral("system")},
        {QStringLiteral("clock24Hour"), true},
        {QStringLiteral("usageProfile"), QStringLiteral("writing")},
        {QStringLiteral("handedness"), QStringLiteral("right")},
        {QStringLiteral("toolbarActions"), QStringList{QStringLiteral("pen"), QStringLiteral("highlighter"), QStringLiteral("eraser"), QStringLiteral("lasso"), QStringLiteral("shapes"), QStringLiteral("text"), QStringLiteral("table"), QStringLiteral("chart")}},
        {QStringLiteral("quickPanelEnabled"), true},
        {QStringLiteral("doubleTapAction"), QStringLiteral("zoom")},
        {QStringLiteral("swipeAction"), QStringLiteral("pages")},
        {QStringLiteral("penPressureCurve"), QStringLiteral("normal")},
        {QStringLiteral("palmRejection"), QStringLiteral("writing")},
        {QStringLiteral("lassoKeepInk"), false},
        {QStringLiteral("lassoInsertTarget"), QStringLiteral("ask")},
        {QStringLiteral("ocrLanguage"), QStringLiteral("auto")},
        {QStringLiteral("ocrNumericMode"), false},
        {QStringLiteral("ocrWordList"), QStringList{}},
        {QStringLiteral("ocrCorrections"), QStringList{}},
        {QStringLiteral("shapeTolerance"), QStringLiteral("normal")},
        {QStringLiteral("refreshProfile"), QStringLiteral("balanced")},
        {QStringLiteral("readingZoom"), 100},
        {QStringLiteral("readingMargins"), 0},
        {QStringLiteral("readingOrientation"), -1},
        {QStringLiteral("readingPanSensitivity"), 100},
        {QStringLiteral("highContrast"), false},
        {QStringLiteral("boldLabels"), false},
        {QStringLiteral("wifiAutoSync"), QStringLiteral("manual")},
        {QStringLiteral("syncTransport"), QStringLiteral("auto")},
        {QStringLiteral("wifiSyncMinutes"), 15},
        {QStringLiteral("wifiChargingOnly"), false},
        {QStringLiteral("trustedWifiSsids"), QStringList{}},
        {QStringLiteral("syncConflictPolicy"), QStringLiteral("ask")},
        {QStringLiteral("backupSchedule"), QStringLiteral("manual")},
        {QStringLiteral("backupRetention"), 5},
        {QStringLiteral("cacheLimitMiB"), 64},
        {QStringLiteral("appleWritingDestination"), QStringLiteral("notes")},
        {QStringLiteral("appleTableDestination"), QStringLiteral("numbers")},
        {QStringLiteral("appleChartDestination"), QStringLiteral("keynote")},
        {QStringLiteral("appleBookDestination"), QStringLiteral("books")}
    };
}

bool containsControl(const QString &text)
{
    for (const QChar ch : text)
        if (ch.category() == QChar::Other_Control) return true;
    return false;
}

bool normalizedValue(const QString &key, const QVariant &input, QVariant *output)
{
    if (key == QStringLiteral("orientation")
        || key == QStringLiteral("interfaceScale")
        || key == QStringLiteral("idleMinutes")
        || key == QStringLiteral("readingZoom")
        || key == QStringLiteral("readingMargins")
        || key == QStringLiteral("readingOrientation")
        || key == QStringLiteral("readingPanSensitivity")
        || key == QStringLiteral("wifiSyncMinutes")
        || key == QStringLiteral("backupRetention")
        || key == QStringLiteral("cacheLimitMiB")) {
        bool valid = false;
        const int number = input.toString().toInt(&valid);
        if (!valid) return false;
        if (key == QStringLiteral("orientation") && !allowed(number, {0, 90, 180, 270})) return false;
        if (key == QStringLiteral("interfaceScale") && !allowed(number, {100, 115, 130})) return false;
        if (key == QStringLiteral("idleMinutes") && !allowed(number, {2, 5, 10, 20, 30, 60})) return false;
        if (key == QStringLiteral("readingZoom") && !allowed(number, {100, 125, 150, 200})) return false;
        if (key == QStringLiteral("readingMargins") && !allowed(number, {0, 8, 16, 24})) return false;
        if (key == QStringLiteral("readingOrientation") && !allowed(number, {-1, 0, 90, 180, 270})) return false;
        if (key == QStringLiteral("readingPanSensitivity") && !allowed(number, {75, 100, 125})) return false;
        if (key == QStringLiteral("wifiSyncMinutes") && !allowed(number, {5, 15, 30, 60})) return false;
        if (key == QStringLiteral("backupRetention") && !allowed(number, {1, 5, 10, 20})) return false;
        if (key == QStringLiteral("cacheLimitMiB") && !allowed(number, {32, 64, 128, 256})) return false;
        *output = number;
        return true;
    }
    if (key == QStringLiteral("keepAwakeWhileCharging")
        || key == QStringLiteral("automaticOrientation")
        || key == QStringLiteral("autoDiagramRecognition")
        || key == QStringLiteral("showStatusBar")
        || key == QStringLiteral("showClock")
        || key == QStringLiteral("showPenBattery")
        || key == QStringLiteral("gestureNavigationEnabled")
        || key == QStringLiteral("startFullPage")
        || key == QStringLiteral("clock24Hour")
        || key == QStringLiteral("quickPanelEnabled")
        || key == QStringLiteral("lassoKeepInk")
        || key == QStringLiteral("ocrNumericMode")
        || key == QStringLiteral("highContrast")
        || key == QStringLiteral("boldLabels")
        || key == QStringLiteral("wifiChargingOnly")) {
        const QString text = input.toString().trimmed().toLower();
        if (text != QStringLiteral("true") && text != QStringLiteral("false")
            && text != QStringLiteral("1") && text != QStringLiteral("0")) return false;
        *output = text == QStringLiteral("true") || text == QStringLiteral("1");
        return true;
    }
    if (key == QStringLiteral("timeZoneId")) {
        const QString zone = input.toString();
        if (zone != QStringLiteral("system") && (zone.size() > 128 || !QTimeZone(zone.toUtf8()).isValid()))
            return false;
        *output = zone;
        return true;
    }
    if (key == QStringLiteral("usageProfile")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("reading"), QStringLiteral("writing"), QStringLiteral("drawing")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("handedness")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("right"), QStringLiteral("left")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("doubleTapAction")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("zoom"), QStringLiteral("fullPage"), QStringLiteral("refresh"), QStringLiteral("none")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("swipeAction")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("pages"), QStringLiteral("scroll"), QStringLiteral("none")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("penPressureCurve")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("light"), QStringLiteral("normal"), QStringLiteral("firm")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("palmRejection")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("off"), QStringLiteral("writing"), QStringLiteral("extended")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("lassoInsertTarget")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("text"), QStringLiteral("cells"), QStringLiteral("ask")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("ocrLanguage")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("auto"), QStringLiteral("it"), QStringLiteral("en")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("shapeTolerance")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("strict"), QStringLiteral("normal"), QStringLiteral("relaxed")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("refreshProfile")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("balanced"), QStringLiteral("clean"), QStringLiteral("fast")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("wifiAutoSync")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("manual"), QStringLiteral("onOpen"), QStringLiteral("interval"), QStringLiteral("onChange")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("syncTransport")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("auto"), QStringLiteral("wifi"), QStringLiteral("bluetooth")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("syncConflictPolicy")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("ask"), QStringLiteral("keepBoth")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("backupSchedule")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("manual"), QStringLiteral("daily"), QStringLiteral("weekly")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("appleWritingDestination")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("notes"), QStringLiteral("files")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("appleTableDestination")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("numbers"), QStringLiteral("files")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("appleChartDestination")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("keynote"), QStringLiteral("files")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("appleBookDestination")) {
        const QString value = input.toString();
        if (!QStringList{QStringLiteral("books"), QStringLiteral("files")}.contains(value)) return false;
        *output = value;
        return true;
    }
    if (key == QStringLiteral("toolbarActions") || key == QStringLiteral("ocrWordList")
        || key == QStringLiteral("ocrCorrections") || key == QStringLiteral("trustedWifiSsids")) {
        if (input.metaType().id() != QMetaType::QStringList && input.metaType().id() != QMetaType::QString)
            return false;
        const QStringList values = input.toStringList();
        const bool toolbar = key == QStringLiteral("toolbarActions");
        const bool ssids = key == QStringLiteral("trustedWifiSsids");
        const bool corrections = key == QStringLiteral("ocrCorrections");
        if (values.size() > (toolbar ? 8 : ssids ? 32 : 100) || (toolbar && values.isEmpty())) return false;
        QStringList normalized;
        const QStringList actions = defaultValues().value(QStringLiteral("toolbarActions")).toStringList();
        for (const QString &value : values) {
            // SSIDs are byte-sensitive names; their surrounding spaces are significant.
            const QString entry = ssids ? value : value.trimmed();
            if (entry.isEmpty() || containsControl(entry)) return false;
            if (ssids ? entry.toUtf8().size() > 32 : entry.size() > (corrections ? 257 : 128)) return false;
            if (toolbar && !actions.contains(entry)) return false;
            if (corrections) {
                const qsizetype separator = entry.indexOf(u'=');
                if (separator <= 0 || separator == entry.size() - 1
                    || entry.left(separator).trimmed().isEmpty() || entry.mid(separator + 1).trimmed().isEmpty()) return false;
            }
            if (!normalized.contains(entry)) normalized.append(entry);
        }
        *output = normalized;
        return true;
    }
    return false;
}
}

TabletPreferences::TabletPreferences(QObject *parent) : TabletPreferences(QString{}, parent) {}

TabletPreferences::TabletPreferences(const QString &settingsFile, QObject *parent)
    : QObject(parent), m_settingsFile(settingsFile), m_values(defaultValues())
{
    auto settings = settingsFor(m_settingsFile);
    settings->beginGroup(QStringLiteral("tablet"));
    // Version 1 used these same keys. Add new defaults in memory without rewriting
    // a user's existing settings. The next real mutation records schema version 2.
    for (auto it = m_values.begin(); it != m_values.end(); ++it) {
        if (!settings->contains(it.key())) continue;
        QVariant normalized;
        if (normalizedValue(it.key(), settings->value(it.key()), &normalized)) it.value() = normalized;
    }
}

bool TabletPreferences::saveValues(const QVariantMap &values)
{
    auto settings = settingsFor(m_settingsFile);
    settings->beginGroup(QStringLiteral("tablet"));
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        settings->setValue(it.key(), it.value());
    settings->setValue(QStringLiteral("schemaVersion"), std::max(schemaVersion, settings->value(QStringLiteral("schemaVersion"), 0).toInt()));
    settings->sync();
    if (settings->status() != QSettings::NoError) {
        setError(tr("Impossibile salvare le preferenze del tablet. Le impostazioni precedenti restano attive."));
        return false;
    }
    setError({});
    return true;
}

bool TabletPreferences::applyValues(const QVariantMap &values)
{
    QVariantMap changes;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        QVariant normalized;
        if (!normalizedValue(it.key(), it.value(), &normalized)) return false;
        if (m_values.value(it.key()) != normalized) changes.insert(it.key(), normalized);
    }
    if (changes.isEmpty()) return true;
    if (!saveValues(changes)) return false;
    // A profile/reset publishes a coherent snapshot before any observer is notified.
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) m_values.insert(it.key(), it.value());
    if (changes.contains(QStringLiteral("orientation"))) emit orientationChanged();
    if (changes.contains(QStringLiteral("automaticOrientation"))) emit automaticOrientationChanged();
    if (changes.contains(QStringLiteral("autoDiagramRecognition"))) emit autoDiagramRecognitionChanged();
    if (changes.contains(QStringLiteral("interfaceScale"))) emit interfaceScaleChanged();
    if (changes.contains(QStringLiteral("idleMinutes"))) emit idleMinutesChanged();
    if (changes.contains(QStringLiteral("keepAwakeWhileCharging"))) emit keepAwakeWhileChargingChanged();
    if (changes.contains(QStringLiteral("showStatusBar"))) emit showStatusBarChanged();
    if (changes.contains(QStringLiteral("showClock"))) emit showClockChanged();
    if (changes.contains(QStringLiteral("showPenBattery"))) emit showPenBatteryChanged();
    if (changes.contains(QStringLiteral("gestureNavigationEnabled"))) emit gestureNavigationEnabledChanged();
    if (changes.contains(QStringLiteral("startFullPage"))) emit startFullPageChanged();
    if (changes.contains(QStringLiteral("timeZoneId"))) emit timeZoneIdChanged();
    if (changes.contains(QStringLiteral("clock24Hour"))) emit clock24HourChanged();
    if (changes.contains(QStringLiteral("usageProfile"))) emit usageProfileChanged();
    if (changes.contains(QStringLiteral("handedness"))) emit handednessChanged();
    if (changes.contains(QStringLiteral("toolbarActions"))) emit toolbarActionsChanged();
    if (changes.contains(QStringLiteral("quickPanelEnabled"))) emit quickPanelEnabledChanged();
    if (changes.contains(QStringLiteral("doubleTapAction"))) emit doubleTapActionChanged();
    if (changes.contains(QStringLiteral("swipeAction"))) emit swipeActionChanged();
    if (changes.contains(QStringLiteral("penPressureCurve"))) emit penPressureCurveChanged();
    if (changes.contains(QStringLiteral("palmRejection"))) emit palmRejectionChanged();
    if (changes.contains(QStringLiteral("lassoKeepInk"))) emit lassoKeepInkChanged();
    if (changes.contains(QStringLiteral("lassoInsertTarget"))) emit lassoInsertTargetChanged();
    if (changes.contains(QStringLiteral("ocrLanguage"))) emit ocrLanguageChanged();
    if (changes.contains(QStringLiteral("ocrNumericMode"))) emit ocrNumericModeChanged();
    if (changes.contains(QStringLiteral("ocrWordList"))) emit ocrWordListChanged();
    if (changes.contains(QStringLiteral("ocrCorrections"))) emit ocrCorrectionsChanged();
    if (changes.contains(QStringLiteral("shapeTolerance"))) emit shapeToleranceChanged();
    if (changes.contains(QStringLiteral("refreshProfile"))) emit refreshProfileChanged();
    if (changes.contains(QStringLiteral("readingZoom"))) emit readingZoomChanged();
    if (changes.contains(QStringLiteral("readingMargins"))) emit readingMarginsChanged();
    if (changes.contains(QStringLiteral("readingOrientation"))) emit readingOrientationChanged();
    if (changes.contains(QStringLiteral("readingPanSensitivity"))) emit readingPanSensitivityChanged();
    if (changes.contains(QStringLiteral("highContrast"))) emit highContrastChanged();
    if (changes.contains(QStringLiteral("boldLabels"))) emit boldLabelsChanged();
    if (changes.contains(QStringLiteral("wifiAutoSync"))) emit wifiAutoSyncChanged();
    if (changes.contains(QStringLiteral("syncTransport"))) emit syncTransportChanged();
    if (changes.contains(QStringLiteral("wifiSyncMinutes"))) emit wifiSyncMinutesChanged();
    if (changes.contains(QStringLiteral("wifiChargingOnly"))) emit wifiChargingOnlyChanged();
    if (changes.contains(QStringLiteral("trustedWifiSsids"))) emit trustedWifiSsidsChanged();
    if (changes.contains(QStringLiteral("syncConflictPolicy"))) emit syncConflictPolicyChanged();
    if (changes.contains(QStringLiteral("backupSchedule"))) emit backupScheduleChanged();
    if (changes.contains(QStringLiteral("backupRetention"))) emit backupRetentionChanged();
    if (changes.contains(QStringLiteral("cacheLimitMiB"))) emit cacheLimitMiBChanged();
    if (changes.contains(QStringLiteral("appleWritingDestination"))) emit appleWritingDestinationChanged();
    if (changes.contains(QStringLiteral("appleTableDestination"))) emit appleTableDestinationChanged();
    if (changes.contains(QStringLiteral("appleChartDestination"))) emit appleChartDestinationChanged();
    if (changes.contains(QStringLiteral("appleBookDestination"))) emit appleBookDestinationChanged();
    return true;
}

void TabletPreferences::setError(const QString &message)
{
    if (m_error == message) return;
    m_error = message;
    emit errorChanged();
}

void TabletPreferences::setOrientation(int value)
{
    applyValues({{QStringLiteral("orientation"), value}});
}

void TabletPreferences::setAutomaticOrientation(bool value)
{
    applyValues({{QStringLiteral("automaticOrientation"), value}});
}

void TabletPreferences::setAutoDiagramRecognition(bool value)
{
    applyValues({{QStringLiteral("autoDiagramRecognition"), value}});
}

void TabletPreferences::setInterfaceScale(int value)
{
    applyValues({{QStringLiteral("interfaceScale"), value}});
}

void TabletPreferences::setIdleMinutes(int value)
{
    applyValues({{QStringLiteral("idleMinutes"), value}});
}

void TabletPreferences::setKeepAwakeWhileCharging(bool value)
{
    applyValues({{QStringLiteral("keepAwakeWhileCharging"), value}});
}

void TabletPreferences::setShowStatusBar(bool value)
{
    applyValues({{QStringLiteral("showStatusBar"), value}});
}

void TabletPreferences::setShowClock(bool value)
{
    applyValues({{QStringLiteral("showClock"), value}});
}

void TabletPreferences::setShowPenBattery(bool value)
{
    applyValues({{QStringLiteral("showPenBattery"), value}});
}

void TabletPreferences::setGestureNavigationEnabled(bool value)
{
    applyValues({{QStringLiteral("gestureNavigationEnabled"), value}});
}

void TabletPreferences::setStartFullPage(bool value)
{
    applyValues({{QStringLiteral("startFullPage"), value}});
}

void TabletPreferences::setTimeZoneId(QString value)
{
    applyValues({{QStringLiteral("timeZoneId"), value}});
}

void TabletPreferences::setClock24Hour(bool value)
{
    applyValues({{QStringLiteral("clock24Hour"), value}});
}

void TabletPreferences::setUsageProfile(QString value)
{
    applyUsageProfile(value);
}

void TabletPreferences::setHandedness(QString value)
{
    applyValues({{QStringLiteral("handedness"), value}});
}

void TabletPreferences::setToolbarActions(QStringList value)
{
    applyValues({{QStringLiteral("toolbarActions"), value}});
}

void TabletPreferences::setQuickPanelEnabled(bool value)
{
    applyValues({{QStringLiteral("quickPanelEnabled"), value}});
}

void TabletPreferences::setDoubleTapAction(QString value)
{
    applyValues({{QStringLiteral("doubleTapAction"), value}});
}

void TabletPreferences::setSwipeAction(QString value)
{
    applyValues({{QStringLiteral("swipeAction"), value}});
}

void TabletPreferences::setPenPressureCurve(QString value)
{
    applyValues({{QStringLiteral("penPressureCurve"), value}});
}

void TabletPreferences::setPalmRejection(QString value)
{
    applyValues({{QStringLiteral("palmRejection"), value}});
}

void TabletPreferences::setLassoKeepInk(bool value)
{
    applyValues({{QStringLiteral("lassoKeepInk"), value}});
}

void TabletPreferences::setLassoInsertTarget(QString value)
{
    applyValues({{QStringLiteral("lassoInsertTarget"), value}});
}

void TabletPreferences::setOcrLanguage(QString value)
{
    applyValues({{QStringLiteral("ocrLanguage"), value}});
}

void TabletPreferences::setOcrNumericMode(bool value)
{
    applyValues({{QStringLiteral("ocrNumericMode"), value}});
}

void TabletPreferences::setOcrWordList(QStringList value)
{
    applyValues({{QStringLiteral("ocrWordList"), value}});
}

void TabletPreferences::setOcrCorrections(QStringList value)
{
    applyValues({{QStringLiteral("ocrCorrections"), value}});
}

void TabletPreferences::setShapeTolerance(QString value)
{
    applyValues({{QStringLiteral("shapeTolerance"), value}});
}

void TabletPreferences::setRefreshProfile(QString value)
{
    applyValues({{QStringLiteral("refreshProfile"), value}});
}

void TabletPreferences::setReadingZoom(int value)
{
    applyValues({{QStringLiteral("readingZoom"), value}});
}

void TabletPreferences::setReadingMargins(int value)
{
    applyValues({{QStringLiteral("readingMargins"), value}});
}

void TabletPreferences::setReadingOrientation(int value)
{
    applyValues({{QStringLiteral("readingOrientation"), value}});
}

void TabletPreferences::setReadingPanSensitivity(int value)
{
    applyValues({{QStringLiteral("readingPanSensitivity"), value}});
}

void TabletPreferences::setHighContrast(bool value)
{
    applyValues({{QStringLiteral("highContrast"), value}});
}

void TabletPreferences::setBoldLabels(bool value)
{
    applyValues({{QStringLiteral("boldLabels"), value}});
}

void TabletPreferences::setWifiAutoSync(QString value)
{
    applyValues({{QStringLiteral("wifiAutoSync"), value}});
}

void TabletPreferences::setSyncTransport(QString value)
{
    applyValues({{QStringLiteral("syncTransport"), value}});
}

void TabletPreferences::setWifiSyncMinutes(int value)
{
    applyValues({{QStringLiteral("wifiSyncMinutes"), value}});
}

void TabletPreferences::setWifiChargingOnly(bool value)
{
    applyValues({{QStringLiteral("wifiChargingOnly"), value}});
}

void TabletPreferences::setTrustedWifiSsids(QStringList value)
{
    applyValues({{QStringLiteral("trustedWifiSsids"), value}});
}

void TabletPreferences::setSyncConflictPolicy(QString value)
{
    applyValues({{QStringLiteral("syncConflictPolicy"), value}});
}

void TabletPreferences::setBackupSchedule(QString value)
{
    applyValues({{QStringLiteral("backupSchedule"), value}});
}

void TabletPreferences::setBackupRetention(int value)
{
    applyValues({{QStringLiteral("backupRetention"), value}});
}

void TabletPreferences::setCacheLimitMiB(int value)
{
    applyValues({{QStringLiteral("cacheLimitMiB"), value}});
}

void TabletPreferences::setAppleWritingDestination(QString value)
{
    applyValues({{QStringLiteral("appleWritingDestination"), value}});
}

void TabletPreferences::setAppleTableDestination(QString value)
{
    applyValues({{QStringLiteral("appleTableDestination"), value}});
}

void TabletPreferences::setAppleChartDestination(QString value)
{
    applyValues({{QStringLiteral("appleChartDestination"), value}});
}

void TabletPreferences::setAppleBookDestination(QString value)
{
    applyValues({{QStringLiteral("appleBookDestination"), value}});
}

void TabletPreferences::resetDisplay()
{
    const QVariantMap defaults = defaultValues();
    QVariantMap restored;
    // Power, security, recognition, sync, backups and Apple destinations are not display settings.
    const QStringList keys = {
        QStringLiteral("orientation"), QStringLiteral("automaticOrientation"),
        QStringLiteral("interfaceScale"), QStringLiteral("showStatusBar"),
        QStringLiteral("showClock"), QStringLiteral("showPenBattery"), QStringLiteral("gestureNavigationEnabled"),
        QStringLiteral("startFullPage"), QStringLiteral("timeZoneId"), QStringLiteral("clock24Hour"),
        QStringLiteral("usageProfile"), QStringLiteral("handedness"), QStringLiteral("toolbarActions"),
        QStringLiteral("quickPanelEnabled"), QStringLiteral("doubleTapAction"), QStringLiteral("swipeAction"),
        QStringLiteral("readingZoom"), QStringLiteral("readingMargins"), QStringLiteral("readingOrientation"),
        QStringLiteral("readingPanSensitivity"), QStringLiteral("highContrast"), QStringLiteral("boldLabels")};
    for (const QString &key : keys) restored.insert(key, defaults.value(key));
    applyValues(restored);
}

void TabletPreferences::applyUsageProfile(const QString &profile)
{
    QVariant normalized;
    if (!normalizedValue(QStringLiteral("usageProfile"), profile, &normalized)) return;
    QStringList actions = defaultValues().value(QStringLiteral("toolbarActions")).toStringList();
    if (profile == QStringLiteral("reading"))
        actions = {QStringLiteral("pen"), QStringLiteral("highlighter"), QStringLiteral("eraser"), QStringLiteral("lasso")};
    else if (profile == QStringLiteral("drawing"))
        actions = {QStringLiteral("pen"), QStringLiteral("eraser"), QStringLiteral("lasso"), QStringLiteral("shapes")};
    if (applyValues({{QStringLiteral("usageProfile"), profile},
        {QStringLiteral("toolbarActions"), actions},
        {QStringLiteral("startFullPage"), profile != QStringLiteral("writing")},
        {QStringLiteral("gestureNavigationEnabled"), profile != QStringLiteral("drawing")}}))
        emit usageProfileApplied();
}

QString TabletPreferences::formatClock() const
{
    return formatDateTime(QDateTime::currentDateTimeUtc());
}

QString TabletPreferences::formatDateTime(const QDateTime &dateTime) const
{
    if (!dateTime.isValid()) return {};
    const QTimeZone zone = timeZoneId() == QStringLiteral("system")
        ? QTimeZone::systemTimeZone() : QTimeZone(timeZoneId().toUtf8());
    return dateTime.toTimeZone(zone).toString(clock24Hour() ? QStringLiteral("HH:mm") : QStringLiteral("h:mm AP"));
}
