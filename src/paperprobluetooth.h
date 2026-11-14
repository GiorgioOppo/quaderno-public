#pragma once

#include <QProcess>

inline void startPaperProBluetooth(QProcess &process, bool loadHid = false)
{
    // This startup sequence is only called on firmware verified by main().
    const QString command = QStringLiteral(R"SH(set -eu
export LC_ALL=C
if [ "$1" = hid ] && [ ! -c /dev/uhid ]; then /usr/sbin/modprobe uhid; fi
/usr/sbin/modprobe btnxpuart
/usr/bin/systemctl start bluetooth.service
attempt=0
while ! /usr/bin/bluetoothctl list | /usr/bin/grep -q '^Controller '; do
    attempt=$((attempt + 1))
    if [ "$attempt" -ge 10 ]; then echo 'Bluetooth controller did not appear' >&2; exit 1; fi
    /usr/bin/sleep 1
done
/usr/bin/bluetoothctl power on
/usr/bin/bluetoothctl show | /usr/bin/grep -q 'Powered: yes'
)SH");
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), command,
                  QStringLiteral("quaderno-bluetooth"), loadHid ? QStringLiteral("hid") : QStringLiteral("radio")});
}
