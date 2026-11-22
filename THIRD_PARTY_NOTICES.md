# Third-party software and names

Quaderno’s own code and documentation are under the [MIT License](LICENSE). The optional handwriting helper in `tools/writepad/` uses GPL-3.0-or-later. Third-party components retain their own terms.

| Component | How Quaderno uses it |
| --- | --- |
| [Qt 6](https://doc.qt.io/qt-6/licensing.html) | Quaderno links to Qt Quick, Qt Quick Controls, Qt DBus, and Qt Network. Mac deployment can bundle Qt frameworks and plugins. Include notices and corresponding source information for the exact Qt components and patches you distribute. [Qt licensing obligations](https://www.qt.io/development/open-source-lgpl-obligations) |
| [OpenSSL](https://openssl-library.org/source/license/) and [zlib](https://zlib.net/zlib_license.html) | Quaderno links to these libraries for encrypted transfer and compressed formats. Include the applicable notices for any copies included in a package. |
| [reMarkable SDK and runtime](https://developer.remarkable.com/documentation/sdk) | The tablet build uses the firmware’s Qt e-paper runtime, PDFium, and libarchive. The SDK, firmware, and system libraries are obtained separately. |
| [XOVI extensions](https://github.com/asivery/rm-xovi-extensions) and [AppLoad](https://github.com/asivery/rm-appload) | Independently installed software for the optional menu entry. The Quaderno package includes its own launcher script and icon. |
| [WritePad Handwriting Recognition Engine](https://github.com/phatware/WritePad-Handwriting-Recognition-Engine) | Optional separate executable with Italian and English dictionaries. Copyright © 2001–2019 PhatWare Corp. The helper and engine source use GPL-3.0-or-later. In the source repository, see the [helper build and source distribution](tools/writepad/README.md) and the [GPL license text](tools/writepad/COPYING). Packages with the helper carry its license and copyright notice. |
| PaddleOCR and ONNX Runtime | The optional Paper Pro image OCR helper uses an ONNX conversion of the Latin PP-OCRv5 mobile recognition model and the CPU runtime. Its package includes `PaddleOCR-LICENSE.txt`, `ONNXRuntime-LICENSE.txt` and `ONNXRuntime-ThirdPartyNotices.txt`. [PaddleOCR model](https://paddlepaddle.github.io/PaddleX/latest/en/module_usage/tutorials/ocr_modules/text_recognition.html) · [PaddleOCR Apache 2.0 license](https://github.com/PaddlePaddle/PaddleOCR/blob/main/LICENSE) · [ONNX Runtime MIT license](https://github.com/microsoft/onnxruntime/blob/main/LICENSE) |
| [Obsidian Headless](https://obsidian.md/help/sync/headless) | Optional setup script downloads the client, Node.js, and dependencies for the user’s tablet. |

Quaderno is an independent project. Product and company names such as reMarkable, Paper Pro, Apple, macOS, Apple Books, Obsidian, and draw.io identify compatible products or formats. They belong to their respective owners.
