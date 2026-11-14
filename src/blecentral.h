#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <memory>

class BleCentralPrivate;

class BleCentral : public QObject
{
    Q_OBJECT

public:
    explicit BleCentral(QObject *parent = nullptr);
    ~BleCentral() override;

    void start();
    void stop();
    qsizetype maximumChunkSize() const;
    bool writeChunk(const QByteArray &chunk);

signals:
    void ready();
    void received(const QByteArray &chunk);
    void writable();
    void disconnected(const QString &message);
    void statusChanged(const QString &message);

private:
    std::unique_ptr<BleCentralPrivate> m_private;
};
