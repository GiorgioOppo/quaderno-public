import CoreGraphics
import CoreML
import Foundation
import PencilKit
import Vision

@main
struct QuadernoHandwriting {
    private struct Response: Encodable {
        var text: String?
        var error: String?
        var alternatives: [String]?
    }

    private enum InputError: Error {
        case tooLarge
        case malformed
    }

    static func main() async {
        let response: Response
        do {
            let arguments = CommandLine.arguments
            let language = arguments.count == 1 ? "it" : (arguments.count == 3 && arguments[1] == "--language" ? arguments[2] : "")
            guard ["it", "en"].contains(language) else { throw InputError.malformed }
            let data = try readInput()
            let paths = try JSONDecoder().decode([[[Double]]].self, from: data)
            let drawing = try makeDrawing(paths)
            let alternatives = await recognize(drawing, language: language)
            if let text = alternatives.first {
                response = Response(text: text, alternatives: alternatives)
            } else {
                response = Response(error: "Non è stato riconosciuto alcun testo.")
            }
        } catch InputError.tooLarge {
            response = Response(error: "La selezione è troppo grande. Seleziona un passaggio più breve.")
        } catch {
            response = Response(error: "La selezione contiene tratti non validi.")
        }

        if let data = try? JSONEncoder().encode(response) {
            try? FileHandle.standardOutput.write(contentsOf: data)
        }
    }

    private static func readInput() throws -> Data {
        let limit = 8 * 1024 * 1024
        var input = Data()
        while let chunk = try FileHandle.standardInput.read(upToCount: 65_536), !chunk.isEmpty {
            if input.count + chunk.count > limit {
                throw InputError.tooLarge
            }
            input.append(chunk)
        }
        return input
    }

    private static func makeDrawing(_ paths: [[[Double]]]) throws -> PKDrawing {
        guard !paths.isEmpty, paths.count <= 4_000 else { throw InputError.malformed }

        let ink = PKInk(.pen)
        let pageSize = CGSize(width: 486, height: 648)
        let start = Date()
        var startOffset: TimeInterval = 0
        var pointCount = 0
        var strokes: [PKStroke] = []
        strokes.reserveCapacity(paths.count)

        for path in paths {
            guard !path.isEmpty else { throw InputError.malformed }
            pointCount += path.count
            guard pointCount <= 120_000 else { throw InputError.tooLarge }

            var points: [PKStrokePoint] = []
            points.reserveCapacity(path.count)
            for sample in path {
                guard sample.count == 3, sample.allSatisfy({ (0...1).contains($0) }) else {
                    throw InputError.malformed
                }
                let position = CGPoint(x: sample[0] * pageSize.width, y: sample[1] * pageSize.height)
                if position == points.last?.location { continue }
                let point = PKStrokePoint(
                    location: position,
                    timeOffset: Double(points.count) / 120,
                    size: CGSize(width: 1.8, height: 1.8),
                    opacity: 1,
                    force: CGFloat(max(0.1, sample[2])),
                    azimuth: 0,
                    altitude: .pi / 2
                )
                points.append(point)
            }
            let strokePath = PKStrokePath(controlPoints: points, creationDate: start.addingTimeInterval(startOffset))
            strokes.append(PKStroke(ink: ink, path: strokePath))
            startOffset += Double(points.count) / 120 + 0.05
        }

        return PKDrawing(strokes: strokes)
    }

    private static func recognize(_ drawing: PKDrawing, language: String) async -> [String] {
        let magnitude = magnitudeAfterLeadingMinus(drawing)
        if let magnitude {
            let candidates = numericCandidates(magnitude, language: language)
            if !candidates.isEmpty { return candidates.map { "-" + $0 } }
        }

        let recognizer = PKStrokeRecognizer(preferredLanguages: [Locale.Language(identifier: language)])
        await recognizer.updateDrawing(drawing)
        let text = await recognizer.recognizedText()?.trimmingCharacters(in: .whitespacesAndNewlines)
        let fullCandidates = if let text, !text.isEmpty { [text] }
            else { (try? recognizeImage(drawing, language: language)) ?? [] }
        if let first = fullCandidates.first {
            if isUnsignedNumber(first), magnitude != nil {
                return fullCandidates.filter(isUnsignedNumber).map { "-" + $0 }
            }
            return fullCandidates
        }

        let numeric = numericCandidates(drawing, language: language)
        return numeric.map { magnitude == nil ? $0 : "-" + $0 }
    }

    private static func numericCandidates(_ drawing: PKDrawing, language: String) -> [String] {
        // The fast model reads isolated numerals even when the prose model discards them.
        let candidates = (try? recognizeImage(drawing, language: language, numericFallback: true)) ?? []
        return candidates.filter(isUnsignedNumber)
    }

    private static func isUnsignedNumber(_ text: String) -> Bool {
        text.range(of: #"\A[0-9]+(?:[.,][0-9]+)?\z"#, options: .regularExpression) != nil
    }

    private static func magnitudeAfterLeadingMinus(_ drawing: PKDrawing) -> PKDrawing? {
        var strokes = drawing.strokes
        guard strokes.count > 1, let index = strokes.indices.min(by: {
            strokes[$0].renderBounds.minX < strokes[$1].renderBounds.minX
        }) else { return nil }
        let path = strokes.remove(at: index).path
        guard let first = path.first?.location, let last = path.last?.location else { return nil }
        let xs = path.map { $0.location.x }
        let ys = path.map { $0.location.y }
        guard let left = xs.min(), let right = xs.max(), let top = ys.min(), let bottom = ys.max() else { return nil }
        let width = right - left
        guard width >= 4, bottom - top <= width * 0.2, abs(last.x - first.x) >= width * 0.8 else { return nil }

        let magnitude = PKDrawing(strokes: strokes)
        let body = magnitude.bounds
        let gap = body.minX - right
        let centerY = (top + bottom) / 2
        guard width >= body.height * 0.14, width <= body.height * 1.1,
            gap >= -1, gap <= body.height * 0.8,
            centerY >= body.minY + body.height * 0.2, centerY <= body.maxY - body.height * 0.2 else { return nil }
        return magnitude
    }

    private static func recognizeImage(_ drawing: PKDrawing, language: String, numericFallback: Bool = false) throws -> [String] {
        let bounds = drawing.bounds.insetBy(dx: -9, dy: -9)
        guard !bounds.isEmpty else { return [] }

        let scale = 3.0
        guard let context = CGContext(data: nil, width: Int(ceil(bounds.width * scale)),
                                      height: Int(ceil(bounds.height * scale)), bitsPerComponent: 8,
                                      bytesPerRow: 0, space: CGColorSpaceCreateDeviceGray(),
                                      bitmapInfo: CGImageAlphaInfo.none.rawValue) else { return [] }
        context.setFillColor(gray: 1, alpha: 1)
        context.fill(CGRect(x: 0, y: 0, width: context.width, height: context.height))
        context.scaleBy(x: scale, y: -scale)
        context.translateBy(x: -bounds.minX, y: -bounds.maxY)
        context.setStrokeColor(gray: 0, alpha: 1)
        context.setFillColor(gray: 0, alpha: 1)
        context.setLineCap(.round)
        context.setLineJoin(.round)
        context.setLineWidth(1.7)
        for stroke in drawing.strokes {
            guard let start = stroke.path.first?.location else { continue }
            if stroke.path.count == 1 {
                context.fillEllipse(in: CGRect(x: start.x - 0.85, y: start.y - 0.85, width: 1.7, height: 1.7))
                continue
            }
            context.move(to: start)
            for point in stroke.path.dropFirst() {
                context.addLine(to: point.location)
            }
            context.strokePath()
        }

        guard let image = context.makeImage() else { return [] }
        let request = VNRecognizeTextRequest()
        request.recognitionLevel = numericFallback ? .fast : .accurate
        request.usesLanguageCorrection = !numericFallback
        if numericFallback { request.minimumTextHeight = 0.01 }
        for (stage, devices) in try request.supportedComputeStageDevices {
            for device in devices {
                if case .cpu = device {
                    request.setComputeDevice(device, for: stage)
                    break
                }
            }
        }
        request.recognitionLanguages = [language == "it" ? "it-IT" : "en-US"]
        try VNImageRequestHandler(cgImage: image).perform([request])
        let lines = (request.results ?? []).map { $0.topCandidates(3).map(\.string) }
        let best = lines.compactMap(\.first)
        guard best.count == lines.count, !best.isEmpty else { return [] }
        var alternatives = [best.joined(separator: "\n")]
        for (index, candidates) in lines.enumerated() {
            for candidate in candidates.dropFirst() {
                var variant = best
                variant[index] = candidate
                let text = variant.joined(separator: "\n")
                if !alternatives.contains(text) { alternatives.append(text) }
                if alternatives.count == 3 { return alternatives }
            }
        }
        return alternatives
    }
}
