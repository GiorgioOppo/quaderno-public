// SPDX-License-Identifier: MIT

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QStringConverter>
#include <QTransform>

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
constexpr qsizetype maximumRequestBytes = 8 * 1024 * 1024;
constexpr qsizetype maximumImageBytes = 1024 * 1024;
constexpr qsizetype maximumBase64Bytes = 4 * ((maximumImageBytes + 2) / 3);
constexpr int maximumImages = 136;
constexpr int maximumHalfTurns = 32;
constexpr int maximumTinyLabels = 32;
constexpr int maximumWidth = 1600;
constexpr int maximumHeight = 2133;
constexpr int maximumPixels = 2'000'000;
constexpr int maximumLineCharacters = 512;
constexpr int channels = 3;
constexpr int imageHeight = 48;
constexpr int minimumTensorWidth = 320;
constexpr int maximumTensorWidth = 1472;
constexpr int classCount = 838;

QByteArray readRequest()
{
    QByteArray result;
    std::array<char, 16384> buffer;
    while (result.size() <= maximumRequestBytes) {
        const auto ready = std::fread(buffer.data(), 1, buffer.size(), stdin);
        if (ready > static_cast<std::size_t>(maximumRequestBytes - result.size()))
            return {};
        result.append(buffer.data(), static_cast<qsizetype>(ready));
        if (ready != buffer.size())
            break;
    }
    return std::ferror(stdin) ? QByteArray{} : result;
}

QImage readPng(const QJsonValue &value)
{
    const QString encoded = value.toString();
    if (encoded.isEmpty() || encoded.size() > maximumBase64Bytes)
        return {};
    const auto decoded = QByteArray::fromBase64Encoding(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() > maximumImageBytes)
        return {};
    QBuffer buffer;
    buffer.setData(decoded.decoded);
    if (!buffer.open(QIODevice::ReadOnly))
        return {};
    QImageReader reader(&buffer, "png");
    reader.setAutoDetectImageFormat(false);
    const QSize size = reader.size();
    if (size.width() < 1 || size.height() < 1 || size.width() > maximumWidth || size.height() > maximumHeight
        || size.width() * size.height() > maximumPixels)
        return {};
    return reader.read();
}

QList<QString> characters(const QDir &directory)
{
    QFile input(directory.filePath(QStringLiteral("quaderno-raster-ocr.txt")));
    if (!input.open(QIODevice::ReadOnly) || input.size() > 32 * 1024)
        throw std::runtime_error("dictionary");
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString content = decoder(input.readAll());
    if (decoder.hasError())
        throw std::runtime_error("dictionary");

    auto dictionary = content.split(u'\n');
    if (!dictionary.isEmpty() && dictionary.constLast().isEmpty())
        dictionary.removeLast();
    for (auto &token : dictionary) {
        if (token.endsWith(u'\r'))
            token.chop(1);
        if (token.isEmpty() || token.size() > 8 || token.contains(QChar::Null))
            throw std::runtime_error("dictionary");
    }
    dictionary.prepend(QString{}); // CTC blank.
    dictionary.append(QStringLiteral(" "));
    if (dictionary.size() != classCount)
        throw std::runtime_error("dictionary");
    return dictionary;
}

QImage grayscale(QImage image)
{
    if (image.hasAlphaChannel()) {
        QImage white(image.size(), QImage::Format_RGB32);
        white.fill(Qt::white);
        QPainter painter(&white);
        painter.drawImage(0, 0, image);
        painter.end();
        image = std::move(white);
    }
    return image.convertToFormat(QImage::Format_Grayscale8);
}

QImage lightlyThinInk(const QImage &gray)
{
    QImage result(gray.size(), QImage::Format_Grayscale8);
    if (result.isNull())
        return {};
    for (int y = 0; y < gray.height(); ++y) {
        auto *out = result.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            uchar brightest = 0;
            for (int row = qMax(0, y - 1); row <= qMin(gray.height() - 1, y + 1); ++row) {
                const auto *pixels = gray.constScanLine(row);
                for (int column = qMax(0, x - 1); column <= qMin(gray.width() - 1, x + 1); ++column)
                    brightest = qMax(brightest, pixels[column]);
            }
            out[x] = brightest;
        }
    }
    return result;
}

QImage paddedLabel(const QImage &image, int margin, double widthScale = 1.)
{
    if (image.isNull())
        return {};
    const QSize body(qMax(1, qRound(image.width() * widthScale)), image.height());
    QImage result(body + QSize(2 * margin, 2 * margin), QImage::Format_RGB32);
    if (result.isNull())
        return {};
    result.fill(Qt::white);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(QRect(QPoint(margin, margin), body), image);
    return result;
}

QImage slantedLabel(const QImage &image, int degrees, int margin, double widthScale = 1.)
{
    const auto alpha = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const auto gray = grayscale(alpha.transformed(QTransform().rotate(degrees), Qt::SmoothTransformation));
    int left = gray.width(), right = -1, top = gray.height(), bottom = -1;
    for (int y = 0; y < gray.height(); ++y) {
        const auto *pixels = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            if (pixels[x] >= 180)
                continue;
            left = qMin(left, x);
            right = qMax(right, x);
            top = qMin(top, y);
            bottom = y;
        }
    }
    if (right < 0)
        return {};
    return paddedLabel(gray.copy(QRect(QPoint(left, top), QPoint(right, bottom))), margin, widthScale);
}

QImage stackedLine(const QImage &horizontal, qsizetype &glyphCount)
{
    // The caller always turns the page-space crop clockwise. Reverse that
    // turn to collect the upright letters of a top-to-bottom caption.
    const QImage column = grayscale(horizontal.transformed(QTransform().rotate(-90)));
    QList<QRect> glyphs;
    QRect current;
    for (int y = 0; y < column.height(); ++y) {
        const auto *pixels = column.constScanLine(y);
        int left = column.width(), right = -1;
        for (int x = 0; x < column.width(); ++x) {
            if (pixels[x] >= 195)
                continue;
            left = qMin(left, x);
            right = x;
        }
        if (right >= left) {
            current |= QRect(left, y, right - left + 1, 1);
        } else if (!current.isEmpty()) {
            glyphs.append(current);
            current = {};
        }
    }
    if (!current.isEmpty())
        glyphs.append(current);
    // Isolated pixels are common in low-resolution scans.
    glyphs.removeIf([](const QRect &glyph) { return glyph.size() == QSize(1, 1); });
    if (glyphs.size() < 3 || glyphs.size() > 24)
        return {};

    QList<int> heights;
    for (const auto &glyph : glyphs)
        heights.append(glyph.height());
    std::sort(heights.begin(), heights.end());
    const int typicalHeight = heights.at(heights.size() / 2);
    if (typicalHeight < 3 || heights.constLast() > 2 * typicalHeight + 2)
        return {};

    QList<int> spaces;
    for (qsizetype index = 1; index < glyphs.size(); ++index)
        spaces.append(glyphs.at(index).top() - glyphs.at(index - 1).bottom() - 1);
    auto sortedSpaces = spaces;
    std::sort(sortedSpaces.begin(), sortedSpaces.end());
    const int typicalSpace = sortedSpaces.at(sortedSpaces.size() / 2);
    const int wordSpace = qMax(typicalHeight, qRound(typicalSpace * 1.8));
    const int gap = std::clamp(typicalHeight / 7, 1, 4);
    const int margin = std::max(2, gap);
    int lineWidth = 2 * margin, lineHeight = 0;
    QList<QRect> placements;
    for (qsizetype index = 0; index < glyphs.size(); ++index) {
        const auto &glyph = glyphs.at(index);
        if (index > 0)
            lineWidth += spaces.at(index - 1) > wordSpace ? gap + typicalHeight / 3 : gap;
        const double scale = std::clamp(typicalHeight / double(glyph.height()), .8, 1.5);
        const QSize size(qMax(1, qRound(glyph.width() * scale)), qRound(glyph.height() * scale));
        placements.append(QRect(QPoint(lineWidth - margin, 0), size));
        lineWidth += size.width();
        lineHeight = qMax(lineHeight, size.height());
    }
    if (lineWidth > maximumWidth || (lineHeight + 2 * margin) * lineWidth > maximumPixels)
        return {};
    QImage line(lineWidth, lineHeight + 2 * margin, QImage::Format_RGB32);
    if (line.isNull())
        return {};
    line.fill(Qt::white);
    QPainter painter(&line);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (qsizetype index = 0; index < glyphs.size(); ++index) {
        auto placement = placements.at(index);
        placement.moveTop(margin + lineHeight - placement.height());
        painter.drawImage(placement, column, glyphs.at(index));
    }
    painter.end();
    glyphCount = glyphs.size();
    return line;
}

struct Recognition {
    QString text;
    double confidence = 0;
    int glyphs = 0;
};

double orientationScore(const Recognition &result)
{
    // A half-turn can yield a short fragment whose few symbols have high
    // individual confidence. Factor in the amount of recognized script.
    return result.confidence * std::pow(qMax(1, result.glyphs), .25);
}

class Recognizer
{
public:
    explicit Recognizer(const QDir &directory)
        : m_dictionary(characters(directory)), m_environment(ORT_LOGGING_LEVEL_ERROR, "QuadernoRasterOcr")
    {
        m_environment.DisableTelemetryEvents();
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(2);
        options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        const auto model = QFile::encodeName(directory.filePath(QStringLiteral("quaderno-raster-ocr.onnx")));
        m_session = Ort::Session(m_environment, model.constData(), options);
        if (m_session.GetInputCount() != 1 || m_session.GetOutputCount() != 1)
            throw std::runtime_error("model layout");
        const auto inputType = m_session.GetInputTypeInfo(0);
        const auto input = inputType.GetTensorTypeAndShapeInfo();
        const auto shape = input.GetShape();
        if (input.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || shape.size() != 4
            || (shape[0] != 1 && shape[0] != -1) || shape[1] != channels || shape[2] != imageHeight || shape[3] != -1)
            throw std::runtime_error("model layout");
        Ort::AllocatorWithDefaultOptions allocator;
        m_input = m_session.GetInputNameAllocated(0, allocator).get();
        m_output = m_session.GetOutputNameAllocated(0, allocator).get();
    }

    Recognition line(const QImage &image)
    {
        const int idealWidth = (image.width() * imageHeight + image.height() - 1) / image.height();
        const int renderedWidth = std::clamp(idealWidth, 1, maximumTensorWidth);
        const int tensorWidth = std::max(minimumTensorWidth, ((renderedWidth + 7) / 8) * 8);
        const auto resized = grayscale(image).scaled(renderedWidth, imageHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (resized.isNull())
            throw std::runtime_error("image");
        const auto plane = tensorWidth * imageHeight;
        std::vector<float> data(channels * plane, 0);
        for (int y = 0; y < imageHeight; ++y) {
            const auto *pixels = resized.constScanLine(y);
            for (int x = 0; x < renderedWidth; ++x) {
                const float value = pixels[x] * (2.f / 255.f) - 1;
                for (int c = 0; c < channels; ++c)
                    data[c * plane + y * tensorWidth + x] = value;
            }
        }

        const std::array<int64_t, 4> dimensions{1, channels, imageHeight, tensorWidth};
        const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const auto tensor = Ort::Value::CreateTensor<float>(memory, data.data(), data.size(), dimensions.data(), dimensions.size());
        const char *inputs[]{m_input.c_str()};
        const char *outputs[]{m_output.c_str()};
        const auto result = m_session.Run(Ort::RunOptions{nullptr}, inputs, &tensor, 1, outputs, 1);
        const auto layout = result.front().GetTensorTypeAndShapeInfo();
        const auto shape = layout.GetShape();
        if (layout.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || shape.size() != 3
            || shape[0] != 1 || shape[1] < 1 || shape[1] > maximumTensorWidth || shape[2] != classCount)
            throw std::runtime_error("model output");
        const auto *probabilities = result.front().GetTensorData<float>();
        QString text;
        int previous = 0;
        float peak = 0;
        double confidence = 0;
        int glyphs = 0;
        const auto finishToken = [&] {
            const auto &token = m_dictionary.at(previous);
            if (std::any_of(token.cbegin(), token.cend(), [](QChar ch) { return ch.isLetterOrNumber(); })) {
                confidence += peak;
                ++glyphs;
            }
        };
        for (int64_t step = 0; step < shape[1]; ++step) {
            const auto *scores = probabilities + step * classCount;
            const int index = static_cast<int>(std::max_element(scores, scores + classCount) - scores);
            if (index == previous) {
                peak = std::max(peak, scores[index]);
                continue;
            }
            finishToken();
            previous = index;
            peak = scores[index];
            if (index != 0) {
                const auto &token = m_dictionary.at(index);
                if (text.size() + token.size() > maximumLineCharacters) {
                    previous = 0;
                    break;
                }
                text += token;
            }
        }
        finishToken();
        return {text.trimmed(), glyphs ? confidence / glyphs : 0, glyphs};
    }

    Recognition tinyLabel(const QImage &image)
    {
        if (image.width() < 8 || image.width() > 320 || image.height() < 6 || image.height() > 64)
            return line(image);
        const auto gray = grayscale(image);
        const auto base = paddedLabel(gray, 1);
        auto best = line(base.isNull() ? image : base);
        // Two bounded alternatives preserve short, crowded labels while
        // accounting for slight changes in handwriting slant and weight.
        const std::array alternatives{
            slantedLabel(gray, -5, 3, .85),
            slantedLabel(lightlyThinInk(gray), 4, 1),
        };
        for (const auto &variant : alternatives) {
            if (variant.isNull())
                continue;
            auto result = line(variant);
            if (orientationScore(result) > orientationScore(best))
                best = std::move(result);
        }
        return best;
    }

private:
    const QList<QString> m_dictionary;
    Ort::Env m_environment;
    Ort::Session m_session{nullptr};
    std::string m_input;
    std::string m_output;
};

bool writeLines(const QJsonArray &lines)
{
    auto bytes = QJsonDocument(QJsonObject{{QStringLiteral("lines"), lines}}).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return std::fwrite(bytes.constData(), 1, std::size_t(bytes.size()), stdout) == std::size_t(bytes.size())
        && std::fflush(stdout) == 0;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const bool checking = argc == 2 && QByteArray(argv[1]) == "--check";
    if (argc != 1 && !checking)
        return EXIT_FAILURE;
    const auto supplied = QFileInfo(QString::fromLocal8Bit(argv[0])).canonicalFilePath();
    const QDir directory(QFileInfo(supplied.isEmpty() ? app.applicationFilePath() : supplied).absolutePath());
    try {
        if (checking) {
            Recognizer recognizer(directory);
            QImage blank(144, 48, QImage::Format_Grayscale8);
            blank.fill(255);
            recognizer.line(blank);
            return EXIT_SUCCESS;
        }

        const auto document = QJsonDocument::fromJson(readRequest());
        const auto request = document.object();
        const auto images = request.value(QStringLiteral("images")).toArray();
        if (images.isEmpty() || images.size() > maximumImages)
            return EXIT_FAILURE;

        enum class ImageHint { Supplied, Rotate, PreferStacked, TinyLabel };
        std::array<ImageHint, maximumImages> hints{};
        if (request.contains(QStringLiteral("tryHalfTurn"))) {
            const auto turns = request.value(QStringLiteral("tryHalfTurn"));
            if (!turns.isArray() || turns.toArray().size() > maximumHalfTurns)
                return EXIT_FAILURE;
            for (const auto &entry : turns.toArray()) {
                const auto hint = entry.toObject();
                const int index = entry.isObject() ? hint.value(QStringLiteral("index")).toInt(-1) : entry.toInt(-1);
                if (index < 0 || index >= images.size() || hints.at(index) != ImageHint::Supplied)
                    return EXIT_FAILURE;
                hints.at(index) = hint.value(QStringLiteral("stacked")).toBool()
                    ? ImageHint::PreferStacked : ImageHint::Rotate;
            }
        }
        if (request.contains(QStringLiteral("tinyLabels"))) {
            const auto labels = request.value(QStringLiteral("tinyLabels"));
            if (!labels.isArray() || labels.toArray().size() > maximumTinyLabels)
                return EXIT_FAILURE;
            for (const auto &entry : labels.toArray()) {
                const int index = entry.toInt(-1);
                if (index < 0 || index >= images.size() || hints.at(index) != ImageHint::Supplied)
                    return EXIT_FAILURE;
                hints.at(index) = ImageHint::TinyLabel;
            }
        }

        Recognizer recognizer(directory);
        QJsonArray lines;
        for (qsizetype index = 0; index < images.size(); ++index) {
            const auto image = readPng(images.at(index));
            if (image.isNull())
                return EXIT_FAILURE;
            const auto hint = hints.at(index);
            auto recognized = hint == ImageHint::TinyLabel ? recognizer.tinyLabel(image) : recognizer.line(image);
            if (hint == ImageHint::Rotate || hint == ImageHint::PreferStacked) {
                auto alternative = recognizer.line(image.transformed(QTransform().rotate(180)));
                if (orientationScore(alternative) > orientationScore(recognized))
                    recognized = std::move(alternative);
                qsizetype glyphs = 0;
                const auto stacked = stackedLine(image, glyphs);
                if (!stacked.isNull()) {
                    alternative = recognizer.line(stacked);
                    const auto missing = std::abs(alternative.glyphs - glyphs);
                    const auto straightMissing = std::abs(recognized.glyphs - glyphs);
                    const bool matchesBands = missing == 0 || (missing == 1 && missing < straightMissing);
                    const bool supported = hint == ImageHint::PreferStacked
                        && alternative.confidence >= .65;
                    if (matchesBands && (supported || orientationScore(alternative) > orientationScore(recognized)))
                        recognized = std::move(alternative);
                }
            }
            lines.append(recognized.text);
        }
        return writeLines(lines) ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &) {
        return EXIT_FAILURE;
    }
}
