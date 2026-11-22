#include "smtpmail.h"

#include <QDir>
#include <QPointer>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

namespace {
QString configDirectory(const QString &supplied)
{
    if (!supplied.isEmpty())
        return supplied;
    const QString configuration = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return configuration.isEmpty() ? QString{} : QDir(configuration).filePath(QStringLiteral("smtp"));
}
} // namespace

SmtpMail::SmtpMail(const QString &settingsDirectory, QObject *parent)
    : QObject(parent), m_store(configDirectory(settingsDirectory), settingsDirectory.isEmpty())
{
    bool ok = false;
    const auto stored = !m_store.directory().isEmpty() && m_store.prepare() ? m_store.load(&ok) : QJsonObject{};
    if (!stored.isEmpty() && ok && stored.value(QStringLiteral("version")).toInt() == 1) {
        SmtpClient::Configuration configuration;
        configuration.host = stored.value(QStringLiteral("host")).toString();
        configuration.port = stored.value(QStringLiteral("port")).toInt();
        configuration.security = stored.value(QStringLiteral("security")).toString();
        configuration.username = stored.value(QStringLiteral("username")).toString();
        configuration.password = stored.value(QStringLiteral("password")).toString();
        configuration.sender = stored.value(QStringLiteral("sender")).toString();
        if (SmtpClient::validConfiguration(configuration))
            m_configuration = configuration;
        else
            ok = false;
    } else if (!stored.isEmpty()) {
        ok = false;
    }
    if (!ok)
        m_status = tr("Impossibile leggere le impostazioni SMTP salvate.");
    connect(&m_client, &SmtpClient::finished, this, &SmtpMail::complete);
}

SmtpMail::~SmtpMail() = default;

bool SmtpMail::configured() const
{
    return !m_locked && !m_configuration.host.isEmpty();
}

QVariantMap SmtpMail::settings() const
{
    return {{QStringLiteral("host"), m_configuration.host},
            {QStringLiteral("port"), m_configuration.port},
            {QStringLiteral("security"), m_configuration.security},
            {QStringLiteral("username"), m_configuration.username},
            {QStringLiteral("sender"), m_configuration.sender},
            {QStringLiteral("hasPassword"), !m_configuration.password.isEmpty()}};
}

bool SmtpMail::saveSettings(const QVariantMap &values, const QString &password)
{
    if (m_busy || m_locked)
        return false;
    SmtpClient::Configuration updated;
    updated.host = SmtpClient::normalizedHost(values.value(QStringLiteral("host")).toString());
    bool portOk = false;
    const auto portValue = values.value(QStringLiteral("port"));
    updated.port = portValue.toString().trimmed().toInt(&portOk);
    updated.security = values.value(QStringLiteral("security")).toString();
    updated.username = values.value(QStringLiteral("username")).toString().trimmed();
    updated.sender = values.value(QStringLiteral("sender")).toString().trimmed();
    updated.password = password;
    if (password.isEmpty() && updated.host == m_configuration.host && updated.port == m_configuration.port
        && updated.security == m_configuration.security && updated.username == m_configuration.username) {
        updated.password = m_configuration.password;
    }
    if (!portOk || !SmtpClient::validConfiguration(updated)) {
        setState(false, tr("Controlla server, porta, mittente e credenziali SMTP."));
        return false;
    }

    const QJsonObject data{{QStringLiteral("version"), 1}, {QStringLiteral("host"), updated.host},
                           {QStringLiteral("port"), updated.port}, {QStringLiteral("security"), updated.security},
                           {QStringLiteral("username"), updated.username}, {QStringLiteral("sender"), updated.sender},
                           {QStringLiteral("password"), updated.password}};
    if (m_store.directory().isEmpty() || !m_store.prepare() || !m_store.save(data)) {
        setState(false, tr("Impossibile salvare la configurazione SMTP nello spazio privato del dispositivo."));
        return false;
    }
    m_configuration = updated;
    setState(false, tr("Configurazione SMTP salvata."));
    return true;
}

bool SmtpMail::clearSettings()
{
    if (m_busy || m_locked)
        return false;
    if (m_store.directory().isEmpty() || !m_store.prepare() || !m_store.save({})) {
        setState(false, tr("Impossibile rimuovere la configurazione SMTP salvata."));
        return false;
    }
    m_configuration = {};
    setState(false, tr("Configurazione SMTP rimossa."));
    return true;
}

void SmtpMail::sendPdf(Notebook *notebook, const QVariantMap &option, const QString &recipient,
                       const QString &subject, const QString &message)
{
    if (m_busy)
        return;
    if (!configured()) {
        complete(false, m_locked ? tr("Sblocca Quaderno per inviare email.") : tr("Configura prima il server SMTP nelle impostazioni."));
        return;
    }
    const QString format = option.value(QStringLiteral("format")).toString();
    if (!notebook || (format != QStringLiteral("pdf") && format != QStringLiteral("pdf-page"))) {
        complete(false, tr("Scegli il PDF della pagina o del quaderno da inviare."));
        return;
    }
    SmtpClient::Message draft;
    draft.recipient = recipient.trimmed();
    draft.subject = subject.trimmed();
    draft.text = message;
    draft.attachmentName = notebook->suggestedShareFile(option).fileName();
    if (draft.subject.isEmpty())
        draft.subject = tr("Documento Quaderno");
    if (!SmtpClient::validMailbox(draft.recipient)) {
        complete(false, tr("Inserisci un solo indirizzo email valido."));
        return;
    }
    if (!SmtpClient::validMessage(draft)) {
        complete(false, tr("Controlla l’oggetto, il messaggio e il nome del PDF."));
        return;
    }

    const quint64 operation = ++m_operation;
    setState(true, tr("Preparazione PDF per l’email…"));
    const QPointer<Notebook> model(notebook);
    QTimer::singleShot(0, this, [this, model, option, draft, operation]() mutable {
        if (operation != m_operation || !m_busy || m_locked)
            return;
        if (!model) {
            complete(false, tr("Il quaderno selezionato non è disponibile."));
            return;
        }
        m_export = std::make_unique<QTemporaryDir>(QDir(QDir::tempPath()).filePath(QStringLiteral("quaderno-mail-XXXXXX")));
        if (!m_export->isValid()) {
            complete(false, tr("Impossibile preparare il PDF per l’email."));
            return;
        }
        draft.attachmentPath = m_export->filePath(QStringLiteral("allegato.pdf"));
        if (!model->exportShareFile(option, QUrl::fromLocalFile(draft.attachmentPath))) {
            complete(false, model->error());
            return;
        }
        setState(true, tr("Invio email tramite SMTP…"));
        if (operation == m_operation && !m_locked)
            m_client.start(m_configuration, draft);
    });
}

void SmtpMail::cancel()
{
    if (m_client.active())
        m_client.cancel();
    else if (m_busy)
        complete(false, tr("Invio email annullato."));
}

void SmtpMail::setLocked(bool locked)
{
    if (m_locked == locked)
        return;
    m_locked = locked;
    if (locked && m_busy)
        cancel();
    else
        emit changed();
}

void SmtpMail::setState(bool busy, const QString &status)
{
    m_busy = busy;
    m_status = status;
    emit changed();
}

void SmtpMail::complete(bool success, const QString &status)
{
    ++m_operation;
    m_export.reset();
    setState(false, status);
    emit finished(success);
}
