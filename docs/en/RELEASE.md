# Preparing a release

**English** · [Italiano](../RILASCIO.md)

## Source preview 0.2.19

Build 0.2.19 from the repository. The public `v0.1.0` tag identifies the historical source snapshot. The source tree includes Quaderno, the installer and guides in English and Italian. Quaderno uses the MIT License, and its optional recognizers carry separate terms. [Original source tag](https://github.com/GiorgioOppo/quaderno-public/tree/v0.1.0) · [License](../../LICENSE) · [Third-party notices](../../THIRD_PARTY_NOTICES.md) · [Changelog](../../CHANGELOG.md)

Before making a repository public, review the exact source tree and commit metadata, plus the visibility of issues, workflow logs, and release assets. Use sample data in screenshots and test logs. Verify the final source commit with the build and startup checks, then use that commit for the preview tag. [Development guide](DEVELOPMENT.md) · [GitHub repository visibility](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/setting-repository-visibility)

## Verification

| Platform or workflow | What has been checked | Checks for a downloadable build |
| --- | --- | --- |
| Mac with Apple silicon | App build and startup. Local handwriting transcription was verified with the macOS helper. | Test the final signed bundle on a clean Mac. Check the minimum macOS version, file imports, and library recovery. |
| reMarkable Paper Pro (`ferrari`), firmware 3.29.1.0 | ARM64 suite and rendered startup checks. Full-screen launch from an existing AppLoad installation and freehand Marker selection were checked on a physical tablet. | Repeat installation, update, exit, and session expiry with the release package. Check display legibility and Marker selection on the device. |
| Mixed-page recognition | Automated checks cover freehand outline input, editable output and internal links. An isolated device run exercised local OCR and created a single notebook with linked diagram and table pages. | Recheck prose, table cell placement and diagram labels with representative samples. Keep manual review of unclear handwriting, grid borders, punctuation and similar characters. |
| Keyboard | The tablet accepted input from a simulated Bluetooth HID keyboard. Layout changes and persistence for the on-screen keyboard were verified in tablet mode. | Test pairing, typing, and reconnecting with a physical Bluetooth keyboard. |

## Downloadable packages

For each binary archive, record the source commit, the toolchain, the target OS or firmware, and the checks performed on the exact build. Use a fresh version tag and new checksums when publishing a new build.

- **Paper Pro.** Build with the matching `ferrari` SDK. The base ZIP contains the app and installer scripts. The optional AppLoad menu entry uses compatible XOVI and AppLoad installed on the tablet. For WritePad, include the helper, dictionaries, copyright and license notices and the matching source archive. For image OCR, include the helper, pinned Latin-script model and dictionary, ONNX Runtime and their notices. [Build and package Paper Pro](DEVELOPMENT.md#paper-pro) · [WritePad source and packaging](../../tools/writepad/README.md) · [Image OCR helper](../../tools/raster-ocr/README.md)
- **Mac.** The local archive uses an ad hoc signature. For direct distribution, prepare Developer ID signing and notarization, then verify the signed result on a clean Mac. Inventory the shipped frameworks, libraries, and plugins. Include applicable license notices and the required access to exact corresponding Qt source, including any patches. [Apple notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution) · [Qt licensing obligations](https://www.qt.io/development/open-source-lgpl-obligations)

Verify each final ZIP with `unzip -tq <archive.zip>`. Run `shasum -a 256 <archive.zip>` and include the result with the download. Include the SHA-256 of the matching source archive if you ship the handwriting helper. Publish binaries only for the source commit used to build them. [GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)
