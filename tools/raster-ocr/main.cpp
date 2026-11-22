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

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <vector>

namespace {
constexpr qsizetype maximumRequestBytes = 8 * 1024 * 1024;
constexpr qsizetype maximumImageBytes = 1024 * 1024;
constexpr qsizetype maximumBase64Bytes = 4 * ((maximumImageBytes + 2) / 3);
constexpr int maximumImages = 136;
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

    QString line(const QImage &image)
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
        for (int64_t step = 0; step < shape[1]; ++step) {
            const auto *scores = probabilities + step * classCount;
            const int index = static_cast<int>(std::max_element(scores, scores + classCount) - scores);
            if (index != 0 && index != previous) {
                const auto &token = m_dictionary.at(index);
                if (text.size() + token.size() > maximumLineCharacters)
                    break;
                text += token;
            }
            previous = index;
        }
        return text.trimmed();
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
        const auto list = document.object().value(QStringLiteral("images"));
        if (!list.isArray() || list.toArray().isEmpty() || list.toArray().size() > maximumImages)
            return EXIT_FAILURE;

        Recognizer recognizer(directory);
        QJsonArray lines;
        const auto images = list.toArray();
        for (const auto &entry : images) {
            const auto image = readPng(entry);
            if (image.isNull())
                return EXIT_FAILURE;
            lines.append(recognizer.line(image));
        }
        return writeLines(lines) ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &) {
        return EXIT_FAILURE;
    }
}
