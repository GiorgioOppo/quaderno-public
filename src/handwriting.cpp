#include "handwriting.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOperatingSystemVersion>
#include <QRegularExpression>

namespace {
constexpr qsizetype maxPoints = 120'000;
constexpr qsizetype maxStrokes = 4'000;
constexpr qsizetype maxInputBytes = 8 * 1024 * 1024;
constexpr qsizetype maxResultLength = 4'000;

bool isCoordinate(double value)
{
    return value >= 0 && value <= 1;
}

QJsonArray validatedInk(const QVariantList &selection, QString &error)
{
    if (selection.size() > maxStrokes) {
        error = QObject::tr("La selezione è troppo grande. Seleziona un passaggio più breve.");
        return {};
    }

    QJsonArray ink;
    qsizetype totalPoints = 0;
    for (const QVariant &entry : selection) {
        const QVariantMap stroke = entry.toMap();
        const QString tool = stroke.value(QStringLiteral("tool")).toString();
        if (!tool.isEmpty() && tool != QStringLiteral("pen") && tool != QStringLiteral("pencil")
            && tool != QStringLiteral("marker"))
            continue;

        const QVariantList points = stroke.value(QStringLiteral("points")).toList();
        totalPoints += points.size();
        if (totalPoints > maxPoints) {
            error = QObject::tr("La selezione è troppo grande. Seleziona un passaggio più breve.");
            return {};
        }
        if (points.isEmpty())
            continue;

        QJsonArray path;
        for (const QVariant &entry : points) {
            const QVariantMap point = entry.toMap();
            const double x = QJsonValue::fromVariant(point.value(QStringLiteral("x"))).toDouble(-1);
            const double y = QJsonValue::fromVariant(point.value(QStringLiteral("y"))).toDouble(-1);
            const double p = QJsonValue::fromVariant(point.value(QStringLiteral("p"))).toDouble(-1);
            if (!isCoordinate(x) || !isCoordinate(y) || !isCoordinate(p)) {
                error = QObject::tr("La selezione contiene un tratto non valido.");
                return {};
            }
            path.append(QJsonArray{x, y, p});
        }
        ink.append(path);
    }

    if (ink.isEmpty())
        error = QObject::tr("Seleziona un tratto di scrittura a mano.");
    return ink;
}
}

Handwriting::Handwriting(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        m_timedOut = true;
        m_process.kill();
    });
    connect(&m_process, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
        if (state != QProcess::NotRunning || !m_waitingForExit)
            return;
        // The previous process can still emit finished or error before its events unwind.
        QTimer::singleShot(0, this, [this] {
            if (m_waitingForExit && m_process.state() == QProcess::NotRunning)
                startProcess();
        });
    });
    connect(&m_process, &QProcess::started, this, [this] {
        if (!m_busy || m_waitingForExit) {
            m_process.kill();
            return;
        }
        m_process.write(m_input);
        m_input.clear();
        m_process.closeWriteChannel();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (m_busy && !m_waitingForExit && error == QProcess::FailedToStart)
            complete({}, tr("Impossibile avviare il riconoscimento della scrittura."));
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QByteArray output = m_process.readAllStandardOutput();
        if (!m_busy || m_waitingForExit)
            return;
        if (m_timedOut) {
            complete({}, tr("Il riconoscimento ha impiegato troppo tempo. Riprova con una selezione più piccola."));
            return;
        }
        if (status != QProcess::NormalExit || code != 0 || output.size() > 64 * 1024) {
            complete({}, tr("Il riconoscimento della scrittura si è interrotto."));
            return;
        }

        const QJsonObject response = QJsonDocument::fromJson(output).object();
        const QString text = response.value(QStringLiteral("text")).toString().trimmed();
        const QByteArray sourceError = response.value(QStringLiteral("error")).toString().toUtf8();
        const QString error = sourceError.isEmpty() ? QString{} : tr(sourceError.constData());
        if (text.size() > maxResultLength) {
            complete({}, tr("La trascrizione supera 4.000 caratteri. Seleziona un passaggio più breve."));
        } else if (text.isEmpty()) {
            complete({}, error.isEmpty() ? tr("Non è stato riconosciuto alcun testo.") : error);
        } else {
            m_alternatives = {text};
            for (const QJsonValue &entry : response.value(QStringLiteral("alternatives")).toArray()) {
                const QString candidate = entry.toString().trimmed();
                if (!candidate.isEmpty() && candidate.size() <= maxResultLength && !m_alternatives.contains(candidate))
                    m_alternatives.append(candidate);
                if (m_alternatives.size() >= 3)
                    break;
            }
            complete(text, {});
        }
    });
}

Handwriting::~Handwriting()
{
    disconnect(&m_process, nullptr, this, nullptr);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

QString Handwriting::helperPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("quaderno-handwriting"));
}

QStringList Handwriting::availableLanguages() const
{
    const QFileInfo helper(helperPath());
    if (!helper.isFile() || !helper.isExecutable())
        return {};
#ifdef Q_OS_MACOS
    return QOperatingSystemVersion::current().majorVersion() >= 27 ? QStringList{QStringLiteral("it"), QStringLiteral("en")} : QStringList{};
#elif defined(Q_OS_LINUX)
    QStringList result;
    for (const QString &language : {QStringLiteral("it"), QStringLiteral("en")}) {
        const QFileInfo dictionary(QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("quaderno-handwriting-%1.dct").arg(language)));
        if (dictionary.isFile() && dictionary.isReadable() && dictionary.size() > 0)
            result.append(language);
    }
    return result;
#else
    return {};
#endif
}

bool Handwriting::available() const
{
    return !availableLanguages().isEmpty();
}

void Handwriting::recognize(const QVariantList &strokes, const QString &language)
{
    if (m_busy)
        return;
    if (!available()) {
#ifdef Q_OS_MACOS
        complete({}, tr("La trascrizione offline è disponibile con Quaderno su macOS 27 o successivo."));
#else
        complete({}, tr("Il motore di trascrizione non è installato su questo dispositivo."));
#endif
        return;
    }

    if (!availableLanguages().contains(language)) {
        m_alternatives.clear();
        complete({}, tr("Il dizionario per la lingua scelta non è installato."));
        return;
    }
    m_language = language;
    m_alternatives.clear();
    QString error;
    const QJsonArray ink = validatedInk(strokes, error);
    if (ink.isEmpty()) {
        complete({}, error);
        return;
    }
    m_input = QJsonDocument(ink).toJson(QJsonDocument::Compact);
    if (m_input.size() > maxInputBytes) {
        complete({}, tr("La selezione è troppo grande. Seleziona un passaggio più breve."));
        return;
    }

    m_timedOut = false;
    m_error.clear();
    m_result.clear();
    m_originalResult.clear();
    m_alternatives.clear();
    m_busy = true;
    m_waitingForExit = m_process.state() != QProcess::NotRunning;
    if (!m_waitingForExit)
        startProcess();
    emit changed();
}

void Handwriting::startProcess()
{
    m_waitingForExit = false;
    m_process.setStandardErrorFile(QProcess::nullDevice());
    m_timeout.start(60'000);
    // No arguments retains compatibility with the previous Italian-only helper.
    const QStringList arguments = m_language == QStringLiteral("it") ? QStringList{}
        : QStringList{QStringLiteral("--language"), m_language};
    m_process.start(helperPath(), arguments);
}

void Handwriting::cancel()
{
    const bool wasBusy = m_busy;
    const bool hadResult = !m_result.isEmpty() || !m_error.isEmpty();
    m_timeout.stop();
    m_busy = false;
    m_waitingForExit = false;
    m_input.clear();
    m_error.clear();
    m_result.clear();
    m_originalResult.clear();
    m_alternatives.clear();
    if (m_process.state() != QProcess::NotRunning)
        m_process.kill();
    if (wasBusy || hadResult)
        emit changed();
}

void Handwriting::complete(const QString &text, const QString &error)
{
    m_timeout.stop();
    m_busy = false;
    m_input.clear();
    m_originalResult = text;
    m_result = applyTextPreferences(text);
    if (!text.isEmpty() && m_result != text) {
        m_alternatives.removeAll(m_result);
        m_alternatives.prepend(m_result);
        if (!m_alternatives.contains(text)) m_alternatives.append(text);
    }
    m_error = error;
    if (!error.isEmpty()) m_alternatives.clear();
    emit changed();
    emit finished(!text.isEmpty());
}

QString Handwriting::applyTextPreferences(const QString &text) const
{
    if (text.size() > maxResultLength) return text;
    QString result = text;
    // Literal, whole-word corrections only. Replacement text never becomes a
    // regular expression/backreference and the source transcript stays available.
    const auto replaceWord = [](QString input, const QString &word, const QString &replacement, bool insensitive) {
        if (word.isEmpty() || word.size() > 80 || replacement.isEmpty() || replacement.size() > 120)
            return input;
        QRegularExpression pattern(QStringLiteral("(?<![\\p{L}\\p{N}_])") + QRegularExpression::escape(word)
            + QStringLiteral("(?![\\p{L}\\p{N}_])"), insensitive ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption);
        QString output;
        qsizetype offset = 0;
        auto matches = pattern.globalMatch(input);
        while (matches.hasNext()) {
            const auto match = matches.next();
            output += input.mid(offset, match.capturedStart() - offset) + replacement;
            offset = match.capturedEnd();
            if (output.size() > maxResultLength) return input;
        }
        output += input.mid(offset);
        return output.size() <= maxResultLength ? output : input;
    };
    for (const QString &word : m_wordList.mid(0, 100))
        result = replaceWord(result, word.trimmed(), word.trimmed(), true);
    for (const QString &rule : m_corrections.mid(0, 100)) {
        const qsizetype split = rule.indexOf(QLatin1Char('='));
        if (split > 0)
            result = replaceWord(result, rule.left(split).trimmed(), rule.mid(split + 1).trimmed(), false);
    }
    if (m_numericMode) {
        QStringList lines = result.split(QLatin1Char('\n'));
        const QRegularExpression numeric(QStringLiteral("^[0-9OoIl|.,+\\-/%()=\\s]+$"));
        for (QString &line : lines) {
            if (numeric.match(line).hasMatch())
                line.replace(QLatin1Char('O'), QLatin1Char('0')).replace(QLatin1Char('o'), QLatin1Char('0'))
                    .replace(QLatin1Char('I'), QLatin1Char('1')).replace(QLatin1Char('l'), QLatin1Char('1'))
                    .replace(QLatin1Char('|'), QLatin1Char('1'));
        }
        result = lines.join(QLatin1Char('\n'));
    }
    return result;
}
