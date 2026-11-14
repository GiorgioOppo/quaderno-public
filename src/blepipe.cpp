#include "blepipe.h"

#include <QtEndian>
#include <utility>

namespace {
constexpr int headerSize = 3;
constexpr int notifyWindow = 4;
constexpr qsizetype minChunkSize = 20;
constexpr qsizetype maxChunkSize = 244;
constexpr qsizetype queuedLimit = 9 * 1024 * 1024;
constexpr char dataType = 1;
constexpr char acknowledgement = 2;
constexpr char mtuAnnouncement = 3;

QByteArray header(char type, quint16 number)
{
    QByteArray packet(headerSize, Qt::Uninitialized);
    packet[0] = type;
    qToBigEndian(number, packet.data() + 1);
    return packet;
}
}

BlePipe::BlePipe(bool server, QObject *parent) : QObject(parent), m_server(server)
{
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, &BlePipe::fail);
}

void BlePipe::reset()
{
    m_deadline.stop();
    m_output.clear();
    m_waiting.clear();
    m_controls.clear();
    m_queued = 0;
    m_offset = 0;
    m_peerChunkSize = 20;
    m_nextSend = 0;
    m_nextReceive = 0;
    m_announced = false;
    m_failed = false;
}

void BlePipe::setOutput(std::function<bool(const QByteArray &)> write, std::function<qsizetype()> chunkSize)
{
    m_write = std::move(write);
    m_chunkSize = std::move(chunkSize);
}

void BlePipe::enqueue(const QByteArray &bytes)
{
    if (m_failed || bytes.isEmpty())
        return;
    if (bytes.size() > queuedLimit - m_queued) {
        fail();
        return;
    }
    m_output.enqueue(bytes);
    m_queued += bytes.size();
    pump();
}

void BlePipe::receive(const QByteArray &packet)
{
    if (m_failed)
        return;
    if (packet.size() < headerSize || packet.size() > 244) {
        fail();
        return;
    }
    const auto number = qFromBigEndian<quint16>(packet.constData() + 1);
    if (packet.front() == mtuAnnouncement && !m_server && !m_announced && packet.size() == headerSize
        && number >= 20 && number <= 244) {
        m_peerChunkSize = number;
        m_announced = true;
    } else if (packet.front() == acknowledgement && m_server && packet.size() == headerSize
               && !m_waiting.isEmpty() && number == m_waiting.head()) {
        m_waiting.dequeue();
        m_deadline.stop();
    } else if (packet.front() == dataType && packet.size() > headerSize && number == m_nextReceive) {
        ++m_nextReceive;
        if (m_server && !m_announced) {
            m_announced = true;
            m_controls.enqueue(header(mtuAnnouncement, quint16(qBound(minChunkSize, m_chunkSize(), maxChunkSize))));
        }
        if (!m_server) {
            if (m_controls.size() >= notifyWindow) {
                fail();
                return;
            }
            m_controls.enqueue(header(acknowledgement, number));
        }
        emit received(packet.sliced(headerSize));
    } else {
        fail();
        return;
    }
    m_deadline.stop();
    pump();
}

void BlePipe::writable()
{
    m_deadline.stop();
    pump();
}

void BlePipe::pump()
{
    if (m_failed || !m_write || !m_chunkSize)
        return;
    while (!m_controls.isEmpty() && m_write(m_controls.head()))
        m_controls.dequeue();
    if (m_controls.isEmpty()) {
        const auto local = qBound(minChunkSize, m_chunkSize(), maxChunkSize);
        const auto limit = (m_server ? local : qMin(local, m_peerChunkSize)) - headerSize;
        while (!m_output.isEmpty() && (!m_server || m_waiting.size() < notifyWindow)) {
            const QByteArray &bytes = m_output.head();
            const auto count = qMin(limit, bytes.size() - m_offset);
            auto packet = header(dataType, m_nextSend);
            packet.append(bytes.constData() + m_offset, count);
            if (!m_write(packet))
                break;
            if (m_server)
                m_waiting.enqueue(m_nextSend);
            ++m_nextSend;
            m_offset += count;
            m_queued -= count;
            if (m_offset == bytes.size()) {
                m_output.dequeue();
                m_offset = 0;
            }
        }
    }
    if (!m_controls.isEmpty() || !m_output.isEmpty() || !m_waiting.isEmpty()) {
        if (!m_deadline.isActive())
            m_deadline.start(15000);
    } else {
        m_deadline.stop();
    }
}

void BlePipe::fail()
{
    reset();
    m_failed = true;
    emit failed();
}
