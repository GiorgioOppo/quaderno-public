#include "syncrendezvous.h"
#include "wifisynclink.h"

#include <QJsonValue>
#include <QUuid>

namespace SyncRendezvous {
namespace {
using namespace Qt::Literals::StringLiterals;

constexpr auto type = "syncRendezvous"_L1;
constexpr auto typeKey = "type"_L1;
constexpr auto versionKey = "version"_L1;
constexpr auto phaseKey = "phase"_L1;
constexpr auto sessionKey = "session"_L1;
constexpr auto workKey = "work"_L1;
constexpr auto wifiKey = "wifi"_L1;
constexpr auto addressKey = "address"_L1;
constexpr int version = 1;
constexpr int baseFields = 4;

struct Phase
{
    Kind kind;
    QLatin1StringView name;
};

constexpr Phase phases[] = {
    {Kind::Hello, "hello"_L1}, {Kind::Work, "work"_L1}, {Kind::Offer, "offer"_L1},
    {Kind::Ready, "ready"_L1}, {Kind::Commit, "commit"_L1}, {Kind::Abort, "abort"_L1}
};

QString string(const QVariantMap &metadata, QLatin1StringView key)
{
    const auto value = metadata.value(key);
    return value.metaType().id() == QMetaType::QString ? value.toString() : QString{};
}

bool validSession(const QString &value)
{
    if (value.size() != 32)
        return false;
    const auto bytes = value.toLatin1();
    return QByteArray::fromHex(bytes).toHex() == bytes;
}

bool validAddress(const QString &value)
{
    return !value.isEmpty() && value.size() <= 15 && value == value.trimmed()
        && WifiSyncLink::isSupportedAddress(value);
}
}

QString newSessionId()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}

bool isControlMessage(const QVariantMap &metadata)
{
    return string(metadata, typeKey) == type;
}

QVariantMap encode(const Message &message)
{
    for (const auto &phase : phases) {
        if (phase.kind != message.kind)
            continue;
        QVariantMap metadata{{typeKey, QString(type)}, {versionKey, version},
                             {phaseKey, QString(phase.name)}, {sessionKey, message.session}};
        if (message.kind == Kind::Hello || message.kind == Kind::Work) {
            metadata.insert(workKey, message.work);
            metadata.insert(wifiKey, message.wifi);
        } else if (message.kind == Kind::Offer) {
            metadata.insert(addressKey, message.address);
        }
        return metadata;
    }
    return {};
}

std::optional<Message> decode(const QVariantMap &metadata, const QByteArray &payload)
{
    if (!payload.isEmpty() || metadata.size() < baseFields || metadata.size() > baseFields + 2
        || !isControlMessage(metadata) || QJsonValue::fromVariant(metadata.value(versionKey)).toInt() != version)
        return std::nullopt;
    const auto session = string(metadata, sessionKey);
    if (!validSession(session))
        return std::nullopt;
    const auto name = string(metadata, phaseKey);
    for (const auto &phase : phases) {
        if (phase.name != name)
            continue;
        Message message{phase.kind, session};
        if (phase.kind == Kind::Hello || phase.kind == Kind::Work) {
            const auto work = metadata.value(workKey);
            const auto wifi = metadata.value(wifiKey);
            if (metadata.size() != baseFields + 2 || work.metaType().id() != QMetaType::Bool
                || wifi.metaType().id() != QMetaType::Bool)
                return std::nullopt;
            message.work = work.toBool();
            message.wifi = wifi.toBool();
        } else if (phase.kind == Kind::Offer) {
            message.address = string(metadata, addressKey);
            if (metadata.size() != baseFields + 1 || !validAddress(message.address))
                return std::nullopt;
        } else if (metadata.size() != baseFields) {
            return std::nullopt;
        }
        return message;
    }
    return std::nullopt;
}
}
