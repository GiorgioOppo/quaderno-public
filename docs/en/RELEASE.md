# Preparing a release

**English** · [Italiano](../RILASCIO.md)

## Quaderno 3.0.0

The `v3.0.0` release provides source code for Mac and Paper Pro, installation scripts, and guides in English and Italian. Build for your platform using the development guide. Quaderno uses the MIT License, and its optional recognizers carry separate terms. [GitHub release](https://github.com/GiorgioOppo/quaderno-public/releases/tag/v3.0.0) · [Build instructions](DEVELOPMENT.md) · [License](../../LICENSE) · [Third-party notices](../../THIRD_PARTY_NOTICES.md) · [Changelog](../../CHANGELOG.md)

Before publishing, review the exact source tree, commit metadata, issues, workflow logs and release assets. Use sample data in screenshots and test logs. Complete the build, startup and functional checks before tagging that same commit. [Functional QA](FUNCTIONAL_QA.md) · [GitHub repository visibility](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/setting-repository-visibility)

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
