#pragma once

#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QTimer>
#include <functional>

class BlePipe final : public QObject
{
    Q_OBJECT
public:
    explicit BlePipe(bool server, QObject *parent = nullptr);
    void reset();
    void setOutput(std::function<bool(const QByteArray &)> write, std::function<qsizetype()> chunkSize);
    void enqueue(const QByteArray &bytes);
    void receive(const QByteArray &packet);
    void writable();

signals:
    void received(const QByteArray &bytes);
    void failed();

private:
    void pump();
    void fail();

    const bool m_server;
    std::function<bool(const QByteArray &)> m_write;
    std::function<qsizetype()> m_chunkSize;
    QQueue<QByteArray> m_output;
    QQueue<quint16> m_waiting;
    QQueue<QByteArray> m_controls;
    QTimer m_deadline;
    qsizetype m_queued = 0;
    qsizetype m_offset = 0;
    qsizetype m_peerChunkSize = 20;
    quint16 m_nextSend = 0;
    quint16 m_nextReceive = 0;
    bool m_announced = false;
    bool m_failed = false;
};
