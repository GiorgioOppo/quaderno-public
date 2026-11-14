#include "markdownweb.h"

#include "notebook.h"

#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringDecoder>

#include <utility>

namespace {
bool validUrl(const QUrl &url)
{
    return url.isValid() && url.scheme() == QStringLiteral("https") && !url.host().isEmpty()
        && !url.authority(QUrl::FullyEncoded).contains(QLatin1Char('@'));
}

bool markdownPath(const QString &path)
{
    const auto suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown");
}
}

MarkdownWeb::MarkdownWeb(Notebook *notebook, QObject *parent)
    : QObject(parent), m_notebook(notebook)
{
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        reset();
        report(tr("Il download del documento Markdown è scaduto."));
    });
}

bool MarkdownWeb::busy() const
{
    return !m_reply.isNull();
}

void MarkdownWeb::setLocked(bool locked)
{
    m_locked = locked;
    if (locked)
        cancel();
}

void MarkdownWeb::importFromUrl(const QString &address, const QString &folderId)
{
    if (m_locked || busy())
        return;
    const QUrl url(address.trimmed(), QUrl::StrictMode);
    if (address.size() > 4096 || !validUrl(url)) {
        report(tr("Inserisci un indirizzo HTTPS diretto a un file Markdown."));
        return;
    }

    QNetworkRequest request(url.adjusted(QUrl::RemoveFragment));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    request.setRawHeader("Accept", "text/markdown, text/plain;q=0.9, application/octet-stream;q=0.5");
    request.setMaximumRedirectsAllowed(4);
    request.setTransferTimeout(10000);
    m_folderId = folderId;
    auto *reply = m_network.get(request);
    m_reply = reply;
    reply->setReadBufferSize(32768);
    connect(reply, &QNetworkReply::redirected, this, [this, reply](const QUrl &target) {
        if (m_reply != reply)
            return;
        if (validUrl(reply->url().resolved(target)))
            reply->redirectAllowed();
        else {
            reset();
            report(tr("Il collegamento rimanda a un indirizzo non supportato."));
        }
    });
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] { read(reply); });
    connect(reply, &QNetworkReply::finished, this, [this, reply] { finish(reply); });
    m_deadline.start(20000);
    report(tr("Scarico il documento Markdown…"));
}

void MarkdownWeb::read(QNetworkReply *reply)
{
    if (m_reply != reply)
        return;
    const auto limit = Notebook::maximumMarkdownBytes;
    if (reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() > limit
        || reply->bytesAvailable() > limit - m_data.size()) {
        reset();
        report(tr("Il documento Markdown supera il limite di 256 KiB."));
        return;
    }
    m_data.append(reply->readAll());
}

void MarkdownWeb::finish(QNetworkReply *reply)
{
    read(reply);
    if (m_reply != reply)
        return;
    const auto error = reply->error();
    const auto contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString()
                                 .section(QLatin1Char(';'), 0, 0).trimmed().toLower();
    const auto url = reply->url();
    const auto fileName = url.fileName(QUrl::FullyDecoded);
    const auto fileInfo = QFileInfo(fileName);
    const auto folder = m_folderId;
    const auto data = std::move(m_data);
    reset();

    if (error != QNetworkReply::NoError) {
        report(tr("Impossibile scaricare il documento Markdown. Controlla il collegamento."));
        return;
    }
    const bool typeIsMarkdown = contentType == QStringLiteral("text/markdown")
        || contentType == QStringLiteral("text/x-markdown");
    const bool typeIsPlain = contentType == QStringLiteral("text/plain");
    const bool typeIsFile = contentType.isEmpty() || contentType == QStringLiteral("application/octet-stream");
    if (!typeIsMarkdown && !typeIsPlain && !(typeIsFile && markdownPath(fileName))) {
        report(tr("Il collegamento deve restituire un file Markdown, per esempio il link Raw."));
        return;
    }

    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless | QStringDecoder::Flag::ConvertInitialBom);
    const QString source = decoder(data);
    if (decoder.hasError() || source.contains(QChar::Null)) {
        report(tr("Il documento Markdown deve essere in UTF-8."));
        return;
    }
    QStringView content(source);
    if (content.startsWith(QChar::ByteOrderMark))
        content = content.sliced(1);
    const auto prefix = content.trimmed().left(128).toString().toLower();
    if (prefix.startsWith(QStringLiteral("<!doctype html")) || prefix.startsWith(QStringLiteral("<html"))) {
        report(tr("Il collegamento deve restituire un file Markdown, per esempio il link Raw."));
        return;
    }
    const QString title = fileInfo.completeBaseName().trimmed().left(80);
    if (!m_notebook->importMarkdownText(source, title.isEmpty() ? tr("Documento Markdown") : title, folder)) {
        report(m_notebook->error());
        return;
    }
    report(tr("Documento Markdown importato."));
    emit imported(m_notebook->bookId());
}

void MarkdownWeb::cancel()
{
    reset();
    report({});
}

void MarkdownWeb::reset()
{
    m_deadline.stop();
    if (auto *reply = m_reply.data()) {
        m_reply.clear();
        reply->disconnect(this);
        if (!reply->isFinished())
            reply->abort();
        reply->deleteLater();
    }
    m_data.clear();
    m_folderId.clear();
}

void MarkdownWeb::report(const QString &message)
{
    m_status = message;
    emit changed();
}
