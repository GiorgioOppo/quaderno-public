#include "syncchannel.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <cstring>

namespace {
constexpr qsizetype headerBytes = 8;
constexpr qsizetype metadataLimit = 16 * 1024;
constexpr char identity[] = "Quaderno sync v1";
constexpr unsigned char suite[] = {0x13, 0x01}; // TLS_AES_128_GCM_SHA256

SSL_SESSION *session(SSL *ssl)
{
    const auto *key = static_cast<QByteArray *>(SSL_get_app_data(ssl));
    const SSL_CIPHER *cipher = SSL_CIPHER_find(ssl, suite);
    SSL_SESSION *candidate = SSL_SESSION_new();
    if (candidate && key && cipher && SSL_SESSION_set_protocol_version(candidate, TLS1_3_VERSION)
        && SSL_SESSION_set_cipher(candidate, cipher)
        && SSL_SESSION_set1_master_key(candidate, reinterpret_cast<const unsigned char *>(key->constData()), key->size()))
        return candidate;
    SSL_SESSION_free(candidate);
    return nullptr;
}

int clientKey(SSL *ssl, const EVP_MD *digest, const unsigned char **id, size_t *length, SSL_SESSION **result)
{
    *result = nullptr;
    if (digest && !EVP_MD_is_a(digest, "SHA256"))
        return 0;
    *id = reinterpret_cast<const unsigned char *>(identity);
    *length = sizeof(identity) - 1;
    *result = session(ssl);
    return *result != nullptr;
}

int serverKey(SSL *ssl, const unsigned char *id, size_t length, SSL_SESSION **result)
{
    *result = nullptr;
    if (length != sizeof(identity) - 1 || std::memcmp(id, identity, length) != 0)
        return 0;
    *result = session(ssl);
    return *result != nullptr;
}

void cleanse(QByteArray &bytes)
{
    if (!bytes.isEmpty())
        OPENSSL_cleanse(bytes.data(), bytes.size());
    bytes.clear();
}
}

struct SyncChannel::Tls {
    ~Tls()
    {
        SSL_free(ssl);
        cleanse(key);
    }
    SSL *ssl = nullptr;
    QByteArray key;
};

SyncChannel::SyncChannel(QObject *parent) : QObject(parent)
{
    m_handshakeDeadline.setSingleShot(true);
    connect(&m_handshakeDeadline, &QTimer::timeout, this, [this] {
        fail(tr("L’autenticazione del collegamento è scaduta."));
    });
}

SyncChannel::~SyncChannel() = default;

void SyncChannel::start(bool server, const QByteArray &key, qsizetype payloadLimit)
{
    stop();
    if (key.size() != 32 || payloadLimit <= 0 || payloadLimit > 64 * 1024 * 1024) {
        fail(tr("Impossibile configurare il collegamento protetto."));
        return;
    }
    const std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> context(SSL_CTX_new(TLS_method()), SSL_CTX_free);
    if (!context || !SSL_CTX_set_min_proto_version(context.get(), TLS1_3_VERSION)
        || !SSL_CTX_set_max_proto_version(context.get(), TLS1_3_VERSION)
        || !SSL_CTX_set_ciphersuites(context.get(), "TLS_AES_128_GCM_SHA256")
        || !SSL_CTX_set1_groups_list(context.get(), "X25519")
        || !SSL_CTX_set_num_tickets(context.get(), 0)) {
        fail(tr("Impossibile configurare il collegamento protetto."));
        return;
    }
    SSL_CTX_set_session_cache_mode(context.get(), SSL_SESS_CACHE_OFF);
    SSL_CTX_set_options(context.get(), SSL_OP_NO_TICKET | SSL_OP_NO_RENEGOTIATION);
    SSL_CTX_set_psk_use_session_callback(context.get(), clientKey);
    SSL_CTX_set_psk_find_session_callback(context.get(), serverKey);

    auto tls = std::make_unique<Tls>();
    tls->key = key;
    tls->ssl = SSL_new(context.get());
    const std::unique_ptr<BIO, decltype(&BIO_free)> input(BIO_new(BIO_s_mem()), BIO_free);
    const std::unique_ptr<BIO, decltype(&BIO_free)> output(BIO_new(BIO_s_mem()), BIO_free);
    if (!tls->ssl || !input || !output || !SSL_set_app_data(tls->ssl, &tls->key)) {
        fail(tr("Impossibile configurare il collegamento protetto."));
        return;
    }
    // Once installed, both BIOs are owned by SSL.
    BIO_up_ref(input.get());
    BIO_up_ref(output.get());
    SSL_set_bio(tls->ssl, input.get(), output.get());
    if (server)
        SSL_set_accept_state(tls->ssl);
    else
        SSL_set_connect_state(tls->ssl);
    m_payloadLimit = payloadLimit;
    m_tls = std::move(tls);
    m_handshakeDeadline.start(20000);
    pump();
}

void SyncChannel::stop()
{
    m_handshakeDeadline.stop();
    m_tls.reset();
    m_plaintext.clear();
    if (m_established) {
        m_established = false;
        emit establishedChanged();
    }
}

bool SyncChannel::established() const
{
    return m_established;
}

void SyncChannel::receive(const QByteArray &encrypted)
{
    if (!m_tls || encrypted.isEmpty())
        return;
    BIO *bio = SSL_get_rbio(m_tls->ssl);
    if (BIO_pending(bio) + encrypted.size() > 256 * 1024 || BIO_write(bio, encrypted.data(), int(encrypted.size())) != encrypted.size()) {
        fail(tr("Dati del collegamento protetto non validi."));
        return;
    }
    pump();
}

void SyncChannel::pump()
{
    if (!m_established) {
        const int result = SSL_do_handshake(m_tls->ssl);
        if (result != 1) {
            if (SSL_get_error(m_tls->ssl, result) != SSL_ERROR_WANT_READ) {
                fail(tr("Autenticazione non riuscita. Controlla il codice di abbinamento su entrambi i dispositivi."));
                return;
            }
            flushEncrypted();
            return;
        }
        if (!SSL_session_reused(m_tls->ssl) || !SSL_get_negotiated_group(m_tls->ssl)) {
            fail(tr("Autenticazione non riuscita. Controlla il codice di abbinamento su entrambi i dispositivi."));
            return;
        }
        m_handshakeDeadline.stop();
        m_established = true;
        flushEncrypted();
        if (!m_tls)
            return;
        emit establishedChanged();
    }
    if (m_tls)
        readPlaintext();
}

void SyncChannel::flushEncrypted()
{
    // Keep each emission small enough for the transport's read-side bound.
    while (m_tls) {
        auto *bio = SSL_get_wbio(m_tls->ssl);
        const auto remaining = BIO_pending(bio);
        if (remaining <= 0)
            return;
        QByteArray bytes(qMin<qsizetype>(remaining, 32768), Qt::Uninitialized);
        const int size = BIO_read(bio, bytes.data(), int(bytes.size()));
        if (size <= 0) {
            fail(tr("Dati del collegamento protetto non validi."));
            return;
        }
        bytes.resize(size);
        emit encryptedReady(bytes);
    }
}

void SyncChannel::sendMessage(const QVariantMap &metadata, const QByteArray &payload)
{
    if (!m_established)
        return;
    const QByteArray json = QJsonDocument(QJsonObject::fromVariantMap(metadata)).toJson(QJsonDocument::Compact);
    if (json.size() > metadataLimit || payload.size() > m_payloadLimit) {
        fail(tr("Il messaggio supera il limite del collegamento."));
        return;
    }
    QByteArray header(headerBytes, Qt::Uninitialized);
    qToBigEndian<quint32>(quint32(json.size()), header.data());
    qToBigEndian<quint32>(quint32(payload.size()), header.data() + 4);
    for (const QByteArray *part : {static_cast<const QByteArray *>(&header), &json, &payload}) {
        for (qsizetype offset = 0; m_tls && offset < part->size();) {
            const int length = int(qMin<qsizetype>(16384, part->size() - offset));
            const int written = SSL_write(m_tls->ssl, part->constData() + offset, length);
            if (written <= 0) {
                fail(tr("Dati del collegamento protetto non validi."));
                return;
            }
            offset += written;
            flushEncrypted();
        }
    }
}

void SyncChannel::readPlaintext()
{
    char buffer[16384];
    while (m_tls) {
        const int count = SSL_read(m_tls->ssl, buffer, int(sizeof(buffer)));
        if (count <= 0) {
            if (SSL_get_error(m_tls->ssl, count) != SSL_ERROR_WANT_READ)
                fail(tr("Dati del collegamento protetto non validi."));
            else
                flushEncrypted();
            return;
        }
        m_plaintext.append(buffer, count);
        while (m_plaintext.size() >= headerBytes) {
            const auto jsonSize = qFromBigEndian<quint32>(m_plaintext.constData());
            const auto payloadSize = qFromBigEndian<quint32>(m_plaintext.constData() + 4);
            if (jsonSize < 2 || jsonSize > metadataLimit || payloadSize > m_payloadLimit) {
                fail(tr("Il messaggio supera il limite del collegamento."));
                return;
            }
            const qsizetype size = headerBytes + jsonSize + payloadSize;
            if (m_plaintext.size() < size)
                break;
            const auto json = QJsonDocument::fromJson(m_plaintext.sliced(headerBytes, jsonSize));
            if (!json.isObject()) {
                fail(tr("Dati del collegamento protetto non validi."));
                return;
            }
            const auto payload = m_plaintext.sliced(headerBytes + jsonSize, payloadSize);
            m_plaintext.remove(0, size);
            emit messageReceived(json.object().toVariantMap(), payload);
        }
    }
}

void SyncChannel::fail(const QString &message)
{
    stop();
    emit failed(message);
}
