#include "paperprosshaccess.h"

#include <QSignalSpy>
#include <QtTest/QTest>

namespace {
QByteArray listening(const QByteArray &interface)
{
    const QByteArray name = "dropbear-" + (interface == "wlan0" ? QByteArray("wlan") : interface) + ".socket";
    return "Id=" + name + "\nLoadState=loaded\nActiveState=active\nSubState=listening\n"
        + "FragmentPath=/usr/lib/systemd/system/" + name + "\nUnitFileState=enabled\nBindToDevice="
        + interface + "\nAccept=yes\nListen=[::]:22 (Stream)\n\n";
}

QByteArray masked(const QByteArray &interface)
{
    return "Id=dropbear-" + interface + ".socket\nLoadState=masked\nActiveState=inactive\n"
        "SubState=dead\nFragmentPath=/dev/null\nUnitFileState=masked-runtime\nBindToDevice=\nAccept=no\n\n";
}
}

class PaperProSshAccessTests : public QObject
{
    Q_OBJECT

private slots:
    void wifiAndCableStatusAreIndependent()
    {
        using State = PaperProSshAccess::State;
        const auto wifi = listening("wlan0");
        const auto usb = listening("usb0") + listening("usb1");
        auto policy = PaperProSshAccess::inspect(usb + wifi, true, true, false, false);
        QCOMPARE(policy.wifi, State::Enabled);
        QCOMPARE(policy.usb, State::Enabled);

        auto stoppedWifi = wifi;
        stoppedWifi.replace("ActiveState=active", "ActiveState=inactive");
        stoppedWifi.replace("SubState=listening", "SubState=dead");
        policy = PaperProSshAccess::inspect(wifi + usb, true, false, false, false);
        QCOMPARE(policy.wifi, State::Partial);
        QCOMPARE(policy.usb, State::Enabled);
        policy = PaperProSshAccess::inspect(usb + stoppedWifi, true, false, false, false);
        QCOMPARE(policy.wifi, State::Disabled);
        QCOMPARE(policy.usb, State::Enabled);

        policy = PaperProSshAccess::inspect(masked("usb0") + masked("usb1") + wifi, true, true, true, true);
        QCOMPARE(policy.wifi, State::Enabled);
        QCOMPARE(policy.usb, State::Disabled);
        policy = PaperProSshAccess::inspect(masked("usb0") + listening("usb1") + wifi, true, true, true, false);
        QCOMPARE(policy.wifi, State::Enabled);
        QCOMPARE(policy.usb, State::Partial);
    }

    void unknownOrModifiedSocketsAreReadOnly()
    {
        using State = PaperProSshAccess::State;
        auto unexpectedWifi = listening("wlan0");
        unexpectedWifi.replace("BindToDevice=wlan0", "BindToDevice=");
        auto policy = PaperProSshAccess::inspect(listening("usb0") + listening("usb1") + unexpectedWifi,
            true, false, false, false);
        QCOMPARE(policy.wifi, State::Unavailable);
        QCOMPARE(policy.usb, State::Enabled);

        // A reported mask without the matching runtime symlink stays read-only.
        policy = PaperProSshAccess::inspect(masked("usb0") + masked("usb1") + listening("wlan0"), false, true,
            false, true);
        QCOMPARE(policy.wifi, State::Unavailable);
        QCOMPARE(policy.usb, State::Unavailable);
    }

    void mutationsTargetOnlyTheRequestedStockSockets()
    {
        using Step = PaperProSshAccess::Step;
        const auto disableWifi = PaperProSshAccess::command(Step::WiFi, false);
        QCOMPARE(disableWifi.program, QStringLiteral("/usr/bin/rm-ssh-over-wlan"));
        QCOMPARE(disableWifi.arguments, QStringList{QStringLiteral("off")});
        QCOMPARE(PaperProSshAccess::command(Step::WiFi, true).arguments, QStringList{QStringLiteral("on")});

        const QStringList sockets{QStringLiteral("dropbear-usb0.socket"), QStringLiteral("dropbear-usb1.socket")};
        const auto disableUsb = PaperProSshAccess::command(Step::MaskUsb, false);
        QCOMPARE(disableUsb.program, QStringLiteral("/usr/bin/systemctl"));
        QCOMPARE(disableUsb.arguments, (QStringList{QStringLiteral("--no-ask-password"), QStringLiteral("mask"),
            QStringLiteral("--runtime"), QStringLiteral("--now")} + sockets));
        QCOMPARE(PaperProSshAccess::command(Step::UnmaskUsb, true).arguments,
            (QStringList{QStringLiteral("--no-ask-password"), QStringLiteral("unmask"), QStringLiteral("--runtime")} + sockets));
        QCOMPARE(PaperProSshAccess::command(Step::StartUsb, true).arguments,
            (QStringList{QStringLiteral("--no-ask-password"), QStringLiteral("start")} + sockets));
    }

    void nonDeviceCannotRunRequests()
    {
        PaperProSshAccess access(false);
        QSignalSpy changes(&access, &PaperProSshAccess::changed);
        access.setLocked(false);
        access.refresh();
        access.setEnabled(QStringLiteral("wifi"), true);
        access.setEnabled(QStringLiteral("usb"), false);
        access.setEnabled(QStringLiteral("bluetooth"), true);
        QVERIFY(!access.available());
        QVERIFY(!access.busy());
        QCOMPARE(changes.size(), 0);
        const auto bluetooth = access.transports().last().toMap();
        QCOMPARE(bluetooth.value(QStringLiteral("state")).toString(), QStringLiteral("unavailable"));
        QCOMPARE(bluetooth.value(QStringLiteral("editable")).toBool(), false);
    }
};

QTEST_GUILESS_MAIN(PaperProSshAccessTests)
#include "paperprosshaccess_tests.moc"
