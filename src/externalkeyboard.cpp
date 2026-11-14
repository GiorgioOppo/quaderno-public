#include "externalkeyboard.h"

#ifdef Q_OS_LINUX
#include <QDir>
#include <QFile>
#include <QTimer>

#include <array>
#include <climits>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
const QString inputDirectory = QStringLiteral("/dev/input");
constexpr int bitsPerWord = sizeof(unsigned long) * CHAR_BIT;
using KeyBits = std::array<unsigned long, (KEY_CNT + bitsPerWord - 1) / bitsPerWord>;

bool hasKey(const KeyBits &keys, int code)
{
    return (keys[code / bitsPerWord] & (1UL << (code % bitsPerWord))) != 0;
}

bool isExternalKeyboard(const QString &path)
{
    const QByteArray device = QFile::encodeName(path);
    const int fd = ::open(device.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return false;

    input_id id{};
    KeyBits keys{};
    const bool keyboard = ::ioctl(fd, EVIOCGID, &id) >= 0
        && (id.bustype == BUS_BLUETOOTH || id.bustype == BUS_USB)
        && ::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys.data()) >= 0
        && hasKey(keys, KEY_A) && hasKey(keys, KEY_Z)
        && hasKey(keys, KEY_SPACE) && hasKey(keys, KEY_ENTER);
    ::close(fd);
    return keyboard;
}
}
#endif

ExternalKeyboard::ExternalKeyboard(bool tablet, QObject *parent) : QObject(parent)
{
#ifdef Q_OS_LINUX
    if (!tablet)
        return;

    connect(&m_devices, &QFileSystemWatcher::directoryChanged, this, &ExternalKeyboard::updateConnected);
    if (!m_devices.addPath(inputDirectory)) {
        auto *poll = new QTimer(this);
        connect(poll, &QTimer::timeout, this, &ExternalKeyboard::updateConnected);
        poll->start(1000);
    }
    updateConnected();
#else
    Q_UNUSED(tablet);
#endif
}

#ifdef Q_OS_LINUX
void ExternalKeyboard::updateConnected()
{
    const QDir input(inputDirectory);
    const auto devices = input.entryList({QStringLiteral("event*")}, QDir::Files | QDir::System);
    bool connected = false;
    for (const QString &device : devices) {
        if (isExternalKeyboard(input.filePath(device))) {
            connected = true;
            break;
        }
    }
    if (m_connected == connected)
        return;
    m_connected = connected;
    emit connectedChanged();
}
#endif
