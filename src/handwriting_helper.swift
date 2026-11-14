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
    }

    private enum InputError: Error {
        case tooLarge
        case malformed
    }

    static func main() async {
        let response: Response
        do {
            let data = try readInput()
            let paths = try JSONDecoder().decode([[[Double]]].self, from: data)
            let drawing = try makeDrawing(paths)
            let languages = [Locale.Language(identifier: "it"), Locale.Language(identifier: "en")]
            let recognizer = PKStrokeRecognizer(preferredLanguages: languages)
            await recognizer.updateDrawing(drawing)
            var text = await recognizer.recognizedText()?.trimmingCharacters(in: .whitespacesAndNewlines)
            if text?.isEmpty != false {
                text = try? recognizeImage(drawing)
            }
            if let text, !text.isEmpty {
                response = Response(text: text)
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

    private static func recognizeImage(_ drawing: PKDrawing) throws -> String? {
        let bounds = drawing.bounds.insetBy(dx: -9, dy: -9)
        guard !bounds.isEmpty else { return nil }

        let scale = 3.0
        guard let context = CGContext(data: nil, width: Int(ceil(bounds.width * scale)),
                                      height: Int(ceil(bounds.height * scale)), bitsPerComponent: 8,
                                      bytesPerRow: 0, space: CGColorSpaceCreateDeviceGray(),
                                      bitmapInfo: CGImageAlphaInfo.none.rawValue) else { return nil }
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

        guard let image = context.makeImage() else { return nil }
        let request = VNRecognizeTextRequest()
        request.recognitionLevel = .fast
        for (stage, devices) in try request.supportedComputeStageDevices {
            for device in devices {
                if case .cpu = device {
                    request.setComputeDevice(device, for: stage)
                    break
                }
            }
        }
        request.recognitionLanguages = ["it-IT", "en-US"]
        try VNImageRequestHandler(cgImage: image).perform([request])
        return request.results?.compactMap { $0.topCandidates(1).first?.string }
            .joined(separator: "\n").trimmingCharacters(in: .whitespacesAndNewlines)
    }
}
