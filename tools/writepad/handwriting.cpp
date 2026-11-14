// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <json-c/json.h>

#include "RecognizerWrapper.h"

namespace {
constexpr size_t maxInputBytes = 8 * 1024 * 1024;
constexpr size_t maxStrokes = 4'000;
constexpr size_t maxPoints = 120'000;
constexpr size_t maxResultLength = 4'000;
constexpr size_t maxEngineStrokes = 128;
constexpr size_t maxEnginePoints = 24'000;
constexpr double pageWidth = 486;
constexpr double pageHeight = 648;
constexpr const char *tooLarge = "La selezione è troppo grande. Seleziona un passaggio più breve.";
constexpr const char *invalidInk = "La selezione contiene un tratto non valido.";
constexpr const char *emptyResult = "Non è stato riconosciuto alcun testo.";
constexpr const char *badDictionary = "Impossibile caricare il motore di trascrizione sul Paper Pro.";

struct Stroke {
    std::vector<CGTracePoint> points;
    double left = pageWidth;
    double right = 0;
    double top = pageHeight;
    double bottom = 0;

    double height() const { return bottom - top; }
    double centerY() const { return (top + bottom) / 2; }
};

struct Line {
    std::vector<size_t> strokes;
    double centerY = 0;
    double left = pageWidth;
    double right = 0;
};

using Json = std::unique_ptr<json_object, decltype(&json_object_put)>;

int respond(const char *key, const std::string &value)
{
    Json response(json_object_new_object(), json_object_put);
    if (!response)
        return EXIT_FAILURE;
    auto *field = json_object_new_string_len(value.data(), static_cast<int>(value.size()));
    if (!field || json_object_object_add(response.get(), key, field) != 0) {
        if (field)
            json_object_put(field);
        return EXIT_FAILURE;
    }
    return std::puts(json_object_to_json_string_ext(response.get(), JSON_C_TO_STRING_PLAIN)) < 0
        ? EXIT_FAILURE : EXIT_SUCCESS;
}

bool coordinate(json_object *entry, double &value)
{
    const auto type = json_object_get_type(entry);
    if (type != json_type_double && type != json_type_int)
        return false;
    value = json_object_get_double(entry);
    return value >= 0 && value <= 1;
}

const char *readInk(std::vector<Stroke> &strokes)
{
    std::string input;
    char buffer[16'384];
    size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), stdin)) != 0) {
        if (input.size() + count > maxInputBytes)
            return tooLarge;
        input.append(buffer, count);
    }
    if (std::ferror(stdin))
        return invalidInk;

    std::unique_ptr<json_tokener, decltype(&json_tokener_free)> parser(json_tokener_new_ex(4), json_tokener_free);
    if (!parser)
        return invalidInk;
    json_tokener_set_flags(parser.get(), JSON_TOKENER_STRICT);
    Json ink(json_tokener_parse_ex(parser.get(), input.c_str(), static_cast<int>(input.size())), json_object_put);
    if (json_tokener_get_error(parser.get()) != json_tokener_success || !json_object_is_type(ink.get(), json_type_array))
        return invalidInk;

    const size_t strokeCount = json_object_array_length(ink.get());
    if (strokeCount == 0)
        return emptyResult;
    if (strokeCount > maxStrokes)
        return tooLarge;

    size_t totalPoints = 0;
    strokes.reserve(strokeCount);
    for (size_t index = 0; index < strokeCount; ++index) {
        auto *path = json_object_array_get_idx(ink.get(), index);
        if (!json_object_is_type(path, json_type_array))
            return invalidInk;
        const size_t pointCount = json_object_array_length(path);
        if (pointCount == 0)
            return invalidInk;
        totalPoints += pointCount;
        if (totalPoints > maxPoints)
            return tooLarge;

        auto &stroke = strokes.emplace_back();
        stroke.points.reserve(pointCount);
        for (size_t sample = 0; sample < pointCount; ++sample) {
            auto *point = json_object_array_get_idx(path, sample);
            double x = 0;
            double y = 0;
            double pressure = 0;
            if (!json_object_is_type(point, json_type_array) || json_object_array_length(point) != 3
                || !coordinate(json_object_array_get_idx(point, 0), x)
                || !coordinate(json_object_array_get_idx(point, 1), y)
                || !coordinate(json_object_array_get_idx(point, 2), pressure))
                return invalidInk;
            x *= pageWidth;
            y *= pageHeight;
            if (!stroke.points.empty() && x == stroke.points.back().pt.x && y == stroke.points.back().pt.y)
                continue;
            const int force = static_cast<int>(MIN_PRESSURE + pressure * (MAX_PRESSURE - MIN_PRESSURE) + 0.5);
            stroke.points.push_back({{x, y}, force});
            stroke.left = std::min(stroke.left, x);
            stroke.right = std::max(stroke.right, x);
            stroke.top = std::min(stroke.top, y);
            stroke.bottom = std::max(stroke.bottom, y);
        }
    }
    return nullptr;
}

void addStroke(Line &line, const Stroke &stroke, size_t index)
{
    line.strokes.push_back(index);
    line.left = std::min(line.left, stroke.left);
    line.right = std::max(line.right, stroke.right);
}

std::vector<Line> readLines(const std::vector<Stroke> &strokes)
{
    std::vector<double> heights;
    for (const auto &stroke : strokes) {
        if (stroke.height() >= 2)
            heights.push_back(stroke.height());
    }
    if (heights.empty())
        return {};
    // Estimate letter height from the strokes themselves. Defer dots, accents and crossbars
    // until the larger strokes have established the line centers.
    auto median = heights.begin() + heights.size() / 2;
    std::nth_element(heights.begin(), median, heights.end());
    const double lineTolerance = std::max(3.0, *median * 0.70);
    const double smallStrokeHeight = std::max(2.0, *median * 0.32);
    std::vector<size_t> letters;
    std::vector<size_t> marks;
    for (size_t i = 0; i < strokes.size(); ++i) {
        auto &target = strokes[i].height() < smallStrokeHeight ? marks : letters;
        target.push_back(i);
    }
    std::stable_sort(letters.begin(), letters.end(), [&strokes](size_t first, size_t second) {
        return strokes[first].centerY() < strokes[second].centerY();
    });

    std::vector<Line> lines;
    for (const auto index : letters) {
        const auto &stroke = strokes[index];
        const double center = stroke.centerY();
        if (lines.empty() || center - lines.back().centerY > lineTolerance)
            lines.push_back({{}, center});
        auto &line = lines.back();
        line.centerY += (center - line.centerY) / (line.strokes.size() + 1);
        addStroke(line, stroke, index);
    }
    for (const auto index : marks) {
        const auto &stroke = strokes[index];
        auto nearest = lines.begin();
        double nearestDistance = std::numeric_limits<double>::infinity();
        for (auto line = lines.begin(); line != lines.end(); ++line) {
            const double gap = std::max({line->left - stroke.right, stroke.left - line->right, 0.0});
            const double distance = std::abs(line->centerY - stroke.centerY()) + 0.25 * gap;
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = line;
            }
        }
        addStroke(*nearest, stroke, index);
    }
    for (auto &line : lines)
        std::sort(line.strokes.begin(), line.strokes.end());
    return lines;
}

std::string asUtf8(const UCHR *result, size_t &resultLength)
{
    static_assert(sizeof(UCHR) == 2, "WritePad must provide UTF-16 recognition results");
    std::string text;
    for (size_t i = 0; result && result[i] && resultLength <= maxResultLength; ++i) {
        unsigned code = result[i];
        ++resultLength;
        if (code >= 0xd800 && code <= 0xdbff && result[i + 1] >= 0xdc00 && result[i + 1] <= 0xdfff) {
            code = 0x10000 + ((code - 0xd800) << 10) + (result[++i] - 0xdc00);
            ++resultLength;
        } else if (code >= 0xd800 && code <= 0xdfff) {
            code = 0xfffd;
        }
        if (code < 0x20)
            continue;
        if (code < 0x80) {
            text += static_cast<char>(code);
        } else if (code < 0x800) {
            text += static_cast<char>(0xc0 | (code >> 6));
            text += static_cast<char>(0x80 | (code & 0x3f));
        } else if (code < 0x10000) {
            text += static_cast<char>(0xe0 | (code >> 12));
            text += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
            text += static_cast<char>(0x80 | (code & 0x3f));
        } else {
            text += static_cast<char>(0xf0 | (code >> 18));
            text += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
            text += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
            text += static_cast<char>(0x80 | (code & 0x3f));
        }
    }
    return text;
}

const char *transcribe(RECOGNIZER_PTR recognizer, const std::vector<Stroke> &strokes, std::string &text)
{
    size_t resultLength = 0;
    const auto lines = readLines(strokes);
    for (const auto &line : lines) {
        if (line.strokes.size() > maxEngineStrokes)
            return tooLarge;
        std::unique_ptr<void, decltype(&INK_FreeData)> ink(INK_InitData(), INK_FreeData);
        if (!ink)
            return emptyResult;
        INK_EnableUndo(ink.get(), false);
        size_t pointCount = 0;
        for (const auto index : line.strokes)
            pointCount += strokes[index].points.size();
        const size_t step = std::max(size_t{1}, (pointCount + maxEnginePoints - 1) / maxEnginePoints);
        for (const auto index : line.strokes) {
            const auto &points = strokes[index].points;
            const size_t length = std::min<size_t>(MAX_TRACE_LENGTH, (points.size() + step - 1) / step);
            std::vector<CGTracePoint> path;
            path.reserve(std::max(size_t{2}, length));
            path.push_back(points.front());
            for (size_t i = 1; i + 1 < length; ++i)
                path.push_back(points[i * (points.size() - 1) / (length - 1)]);
            if (points.size() > 1)
                path.push_back(points.back());
            if (INK_AddStroke(ink.get(), path.data(), static_cast<int>(path.size()), 1, 0) < 0)
                return emptyResult;
        }
        const auto *result = HWR_RecognizeInkData(recognizer, ink.get(), 0, -1, false, false, false, false);
        const std::string lineText = asUtf8(result, resultLength);
        if (!lineText.empty()) {
            if (!text.empty()) {
                text += '\n';
                ++resultLength;
            }
            text += lineText;
        }
        if (resultLength > maxResultLength)
            return "La trascrizione supera 4.000 caratteri. Seleziona un passaggio più breve.";
    }
    return text.empty() ? emptyResult : nullptr;
}
}

int main(int argc, char *argv[])
{
    const bool checking = argc == 2 && std::strcmp(argv[1], "--check") == 0;
    if (argc != 1 && !checking)
        return EXIT_FAILURE;

    std::error_code error;
    const auto executable = std::filesystem::canonical(argv[0], error);
    if (error)
        return checking ? EXIT_FAILURE : respond("error", badDictionary);
    const auto dictionary = executable.parent_path() / "quaderno-handwriting-it.dct";
    if (!std::filesystem::is_regular_file(dictionary, error))
        return checking ? EXIT_FAILURE : respond("error", badDictionary);

    int flags = 0;
    const auto freeRecognizer = [](RECOGNIZER_PTR recognizer) {
        HWR_FreeRecognizer(recognizer, nullptr, nullptr, nullptr);
    };
    std::unique_ptr<void, decltype(freeRecognizer)> recognizer(
        HWR_InitRecognizer(dictionary.c_str(), nullptr, nullptr, nullptr, LANGUAGE_ITALIAN, &flags), freeRecognizer);
    if (!recognizer || !(flags & FLAG_MAINDICT))
        return checking ? EXIT_FAILURE : respond("error", badDictionary);
    if (checking)
        return EXIT_SUCCESS;
    HWR_EnablePhatCalc(recognizer.get(), false);

    std::vector<Stroke> strokes;
    if (const char *message = readInk(strokes))
        return respond("error", message);
    std::string text;
    if (const char *message = transcribe(recognizer.get(), strokes, text))
        return respond("error", message);
    return respond("text", text);
}
