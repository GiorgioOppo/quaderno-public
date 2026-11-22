#include "notebook.h"

#include "pagecontent.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QStringConverter>
#include <QUrl>

#include <array>
#include <tuple>

namespace {
constexpr qsizetype maxCsvBytes = 256 * 1024;
constexpr std::array<QChar, 3> separators{u',', u';', u'\t'};

struct ParsedCsv {
    QList<QStringList> rows;
    qsizetype rowCount = 0;
    qsizetype columnCount = 0;
    qsizetype longestCell = 0;
    qsizetype firstDataWidth = 0;
    qsizetype dataRows = 0;
    qsizetype matchingRows = 0;
    qsizetype decimalCommas = 0;
    bool validQuotes = true;

    auto separatorScore() const
    {
        // Prefer a complete parse over a matching prefix when both candidates have columns.
        return std::make_tuple(columnCount > 1, validQuotes, matchingRows - (dataRows - matchingRows),
                               firstDataWidth > 1, matchingRows, decimalCommas);
    }
};

bool hasDecimalComma(QStringView field)
{
    field = field.trimmed();
    const qsizetype comma = field.indexOf(u',');
    if (comma <= 0 || comma + 1 == field.size())
        return false;
    const qsizetype start = field.startsWith(u'+') || field.startsWith(u'-') ? 1 : 0;
    if (comma == start)
        return false;
    for (qsizetype index = start; index < field.size(); ++index) {
        if (index != comma && !field.at(index).isDigit())
            return false;
    }
    return true;
}

bool readField(QStringView input, QChar separator, qsizetype &offset, QString &field)
{
    if (offset == input.size() || input.at(offset) != u'"') {
        const qsizetype start = offset;
        while (offset < input.size() && input.at(offset) != separator && input.at(offset) != u'\n') {
            if (input.at(offset) == u'"')
                return false;
            ++offset;
        }
        field = input.mid(start, offset - start).toString();
        return true;
    }

    ++offset;
    while (offset < input.size()) {
        const qsizetype quote = input.indexOf(u'"', offset);
        if (quote < 0)
            return false;
        field.append(input.mid(offset, quote - offset));
        offset = quote + 1;
        if (offset == input.size() || input.at(offset) != u'"')
            return offset == input.size() || input.at(offset) == separator || input.at(offset) == u'\n';
        field.append(u'"');
        ++offset;
    }
    return false;
}

ParsedCsv parseCsv(QStringView input, QChar separator)
{
    ParsedCsv result;
    QHash<qsizetype, qsizetype> widthFrequency;
    qsizetype offset = 0;
    while (offset < input.size()) {
        QStringList row;
        qsizetype width = 0;
        bool hasContent = false;
        ++result.rowCount;
        for (;;) {
            QString field;
            ++width;
            result.validQuotes = readField(input, separator, offset, field);
            if (!result.validQuotes)
                break;
            hasContent |= !field.trimmed().isEmpty();
            if (separator != u',' && hasDecimalComma(field))
                ++result.decimalCommas;
            result.longestCell = qMax(result.longestCell, field.size());
            if (result.rowCount <= PageContent::maxTableRows && width <= PageContent::maxTableColumns)
                row.append(std::move(field));
            if (offset == input.size() || input.at(offset++) == u'\n')
                break;
        }
        result.columnCount = qMax(result.columnCount, width);
        if (hasContent || width > 1) {
            if (++result.dataRows == 1)
                result.firstDataWidth = width;
            if (width > 1)
                result.matchingRows = qMax(result.matchingRows, ++widthFrequency[width]);
        }
        if (!result.validQuotes)
            return result;
        if (result.rowCount <= PageContent::maxTableRows)
            result.rows.append(std::move(row));
    }
    return result;
}

ParsedCsv parseTable(QStringView input, bool tabSeparated)
{
    const qsizetype lineEnd = input.indexOf(u'\n');
    const QStringView firstLine = lineEnd < 0 ? input : input.first(lineEnd);
    if (firstLine.size() == 5 && firstLine.startsWith(QLatin1String("sep="), Qt::CaseInsensitive)) {
        for (const QChar separator : separators) {
            if (firstLine.back() == separator)
                return parseCsv(lineEnd < 0 ? QStringView{} : input.sliced(lineEnd + 1), separator);
        }
    }
    if (tabSeparated)
        return parseCsv(input, u'\t');

    ParsedCsv best;
    for (const QChar separator : separators) {
        ParsedCsv candidate = parseCsv(input, separator);
        if (best.rowCount == 0 || candidate.separatorScore() > best.separatorScore())
            best = std::move(candidate);
    }
    return best;
}
}

QVariantMap Notebook::readTableCsv(const QUrl &file) const
{
    const auto error = [](const QString &message) {
        return QVariantMap{{QStringLiteral("error"), message}};
    };
    const QFileInfo info(file.toLocalFile());
    const QString extension = info.suffix().toLower();
    if (!file.isLocalFile() || (extension != QStringLiteral("csv") && extension != QStringLiteral("tsv")))
        return error(tr("Scegli un file CSV salvato sul dispositivo."));

    QFile input(info.absoluteFilePath());
    if (!info.isFile() || !input.open(QIODevice::ReadOnly))
        return error(tr("Impossibile leggere il file CSV."));
    const QByteArray bytes = input.read(maxCsvBytes + 1);
    if (input.error() != QFileDevice::NoError)
        return error(tr("Impossibile leggere il file CSV."));
    if (bytes.size() > maxCsvBytes)
        return error(tr("Il file CSV supera il limite di 256 KiB."));

    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString contents = decoder(bytes);
    if (decoder.hasError() || contents.contains(QChar::Null))
        return error(tr("Il CSV deve essere un file di testo UTF-8."));
    contents.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    contents.replace(u'\r', u'\n');
    if (contents.trimmed().isEmpty() && !contents.contains(u'\t'))
        return error(tr("Il file CSV è vuoto."));

    const ParsedCsv parsed = parseTable(contents, extension == QStringLiteral("tsv"));
    if (!parsed.validQuotes)
        return error(tr("Le virgolette del CSV non sono valide."));
    if (parsed.rowCount == 0)
        return error(tr("Il file CSV è vuoto."));
    if (parsed.rowCount > PageContent::maxTableRows || parsed.columnCount > PageContent::maxTableColumns)
        return error(tr("La tabella CSV supera il limite di %1 righe o %2 colonne.")
                         .arg(PageContent::maxTableRows).arg(PageContent::maxTableColumns));
    if (parsed.longestCell > PageContent::maxTableCellLength)
        return error(tr("Una cella del CSV supera il limite di %1 caratteri.").arg(PageContent::maxTableCellLength));

    QVariantList cells;
    cells.reserve(parsed.rowCount * parsed.columnCount);
    for (const auto &row : parsed.rows) {
        for (qsizetype column = 0; column < parsed.columnCount; ++column)
            cells.append(row.value(column));
    }
    return {{QStringLiteral("rows"), int(parsed.rowCount)}, {QStringLiteral("cols"), int(parsed.columnCount)},
            {QStringLiteral("cells"), cells}};
}

QVariantList Notebook::tableCellValues(const QVariantMap &table) const
{
    return PageContent::resolvedTableCells(table);
}

QByteArray Notebook::tableCsvData(const QVariantMap &table)
{
    // The draft may not have an item ID or a position on the page yet.
    const auto typeKey = QStringLiteral("type");
    QVariantMap draft = table;
    draft.insert(QStringLiteral("id"), QStringLiteral("csv"));
    draft.insert(QStringLiteral("x"), 0.0);
    draft.insert(QStringLiteral("y"), 0.0);
    draft.insert(QStringLiteral("width"), 1.0);
    draft.insert(QStringLiteral("height"), 1.0);
    const QVariantMap valid = PageContent::validatedItem(draft);
    if (valid.value(typeKey) != QStringLiteral("table"))
        return {};

    const int rows = valid.value(QStringLiteral("rows")).toInt();
    const int columns = valid.value(QStringLiteral("cols")).toInt();
    const QVariantList cells = PageContent::resolvedTableCells(valid);
    QString csv;
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            QString value = cells.at(row * columns + column).toString();
            if (!value.isValidUtf16() || value.contains(QChar::Null))
                return {};
            if (column > 0)
                csv += u',';
            csv += u'"' + value.replace(u'"', QStringLiteral("\"\"")) + u'"';
        }
        csv += QStringLiteral("\r\n");
    }

    return csv.toUtf8();
}

bool Notebook::exportTableCsv(const QUrl &file, const QVariantMap &table)
{
    const QFileInfo output(file.toLocalFile());
    if (!file.isLocalFile() || output.suffix().compare(QStringLiteral("csv"), Qt::CaseInsensitive) != 0
        || output.absoluteFilePath() == m_filePath
        || (output.exists() && output.canonicalFilePath() == QFileInfo(m_filePath).canonicalFilePath()))
        return fail(tr("Scegli un file .csv per salvare la tabella."));

    const QByteArray bytes = tableCsvData(table);
    if (bytes.isEmpty()) return fail(tr("La tabella non contiene celle valide da esportare."));
    QSaveFile destination(output.absoluteFilePath());
    if (!destination.open(QIODevice::WriteOnly) || destination.write(bytes) != bytes.size() || !destination.commit())
        return fail(tr("Impossibile salvare il CSV. %1").arg(destination.errorString()));
    setError({});
    return true;
}
