#pragma once

#include <QVariantMap>
#include <optional>

namespace SyncRendezvous {
enum class Kind { Hello, Work, Offer, Ready, Commit, Abort };

struct Message
{
    Kind kind;
    QString session;
    bool work = false;
    bool wifi = false;
    QString address;
};

QString newSessionId();
bool isControlMessage(const QVariantMap &metadata);
QVariantMap encode(const Message &message);
std::optional<Message> decode(const QVariantMap &metadata, const QByteArray &payload);
}
