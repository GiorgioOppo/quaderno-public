# Offline recognition of text lines on Paper Pro

`quaderno-raster-ocr` reads line crops prepared by Quaderno and returns editable text. It uses the Latin PP-OCRv5 mobile recognition model and the ONNX Runtime CPU provider on the Paper Pro. The executable loads the model and dictionary from its own directory and disables runtime telemetry. It accepts file-free input through standard input.

## Build

Source the reMarkable SDK environment, then pass the pinned runtime package, model, dictionary, license and notice to the build script. The script verifies their SHA-256 hashes before compiling. Build tools include CMake, Ninja, unzip and the Qt 6 SDK.

```sh
sh tools/raster-ocr/build.sh onnxruntime-1.32.0.nupkg \
  latin_PP-OCRv5_mobile_rec.onnx ppocrv5_latin_dict.txt \
  PaddleOCR-LICENSE.txt PaddleOcrNet-NOTICE.txt \
  build-arm/raster-ocr/work build-arm/raster-ocr/stage
```

The output contains the helper, `quaderno-raster-ocr.onnx`, `quaderno-raster-ocr.txt`, the ARM shared library `libonnxruntime.so.1` and the corresponding notices. Put all four runtime files in the same application directory. The Paper Pro package accepts the library as a regular file. Run `./quaderno-raster-ocr --check` in the target environment to load the data files and perform a blank-line inference.

## Protocol

Send a JSON object with an `images` array of standard base64 PNG strings in reading order. The helper accepts up to 136 crops with a combined encoded request limit of 8 MiB. Each PNG is limited to 1 MiB, 1600 × 2133 pixels and two million pixels. The helper outputs a JSON object with a `lines` string array in the same order. Empty strings represent individual crops that produced no text. A malformed request or an unavailable model results in a nonzero exit code.

The optional `tryHalfTurn` array accepts up to 32 distinct zero-based crop indices. For those crops, the helper compares the supplied orientation with a 180° turn. It also tries rearranging individually upright letters written from top to bottom when it can separate at least three plausible glyphs. Quaderno uses this for vertical diagram labels that it has already turned clockwise. An object such as `{"index": 0, "stacked": true}` supplies an upright layout hint for the same index. The helper gives a well-supported reflowed reading priority when the hint matches the recognized glyphs. Earlier helpers read the supplied orientation and ignore the optional field.

`tinyLabels` optionally marks up to 32 distinct horizontal crop indices. These crops contain tightly framed grayscale ink from small diagram labels. The helper compares the original crop with two adjusted views and returns a single suggestion. The same index can appear in only one of the two hint arrays.

## Third-party sources

- Microsoft publishes the native Linux ARM64 ONNX Runtime shared library and C++ headers in the 1.32.0 CPU NuGet package under the MIT license. The build includes the package license and third-party notices. [Pinned runtime package](https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/1.32.0/microsoft.ml.onnxruntime.1.32.0.nupkg)
- PaddleOCR provides the Latin character dictionary and the original PP-OCRv5 weights under Apache 2.0. The build verifies the dictionary and license against the hashes of these revisions. [Pinned Latin dictionary](https://raw.githubusercontent.com/PaddlePaddle/PaddleOCR/dab3fe35379033fdcb2d0e9572fac0b36c9a9ebf/ppocr/utils/dict/ppocrv5_latin_dict.txt) · [Pinned PaddleOCR license](https://raw.githubusercontent.com/PaddlePaddle/PaddleOCR/dab3fe35379033fdcb2d0e9572fac0b36c9a9ebf/LICENSE)
- PaddleOcrNet provides the ONNX conversion. The model repository lists Apache 2.0 and MIT. The converter's published notice distinguishes the Apache 2.0 model exports and dictionaries from PaddleOcrNet's MIT source code. The build includes its model attribution with the PaddleOCR license. [Pinned ONNX model](https://huggingface.co/PaddleOcrNet/PaddleOcrNet-models/resolve/5f3eda91ba055237a9af24d1f8d77b46d4d93f41/latin_PP-OCRv5_mobile_rec.onnx) · [Pinned PaddleOcrNet notice](https://github.com/FarhanLodi/PaddleOcrNet/blob/da71ab03ac1552a77ee66fbea2fae9b6739c5829/NOTICE)
