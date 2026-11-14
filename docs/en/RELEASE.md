# Preparing a release

**English** · [Italiano](../RILASCIO.md)

## Source preview 0.1.0

The source preview includes Quaderno, installation scripts, and the English and Italian guides. Quaderno uses the MIT License. The optional WritePad helper has its own GPL-3.0-or-later license. [License](../../LICENSE) · [Third-party notices](../../THIRD_PARTY_NOTICES.md) · [Changelog](../../CHANGELOG.md)

Before making a repository public, review the exact source tree and commit metadata, plus the visibility of issues, workflow logs, and release assets. Use sample data in screenshots and test logs. Verify the final source commit with the build and startup checks, then use that commit for the preview tag. [Development guide](DEVELOPMENT.md) · [GitHub repository visibility](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/setting-repository-visibility)

## Verification

| Platform or workflow | What has been checked | Checks for a downloadable build |
| --- | --- | --- |
| Mac with Apple silicon | App build and startup. Local handwriting transcription was verified with the macOS helper. | Test the final signed bundle on a clean Mac. Check the minimum macOS version, file imports, and library recovery. |
| reMarkable Paper Pro 3.29.1.0 | ARM64 build, AppLoad launch, native Marker events, regional updates during navigation, and a manual full-screen refresh. | Repeat installation, update, exit, and session expiry with the release package. Check display legibility on the device. |
| Keyboard | The tablet accepted input from a simulated Bluetooth HID keyboard. Layout changes and persistence for the on-screen keyboard were verified in tablet mode. | Test pairing, typing, and reconnecting with a physical Bluetooth keyboard. |

## Downloadable packages

Downloadable binaries are a separate step from the source preview. For each package, record the source commit, the toolchain, the target OS or firmware, and the checks performed on the exact build.

- **Paper Pro.** Build with the matching `ferrari` SDK. The base ZIP contains the app and installer scripts. The optional WritePad edition requires the helper, dictionary, copyright and license notices, plus the matching source archive offered with the binary. The device provides the firmware and Qt runtime. [Build and package Paper Pro](DEVELOPMENT.md#paper-pro) · [WritePad source and packaging](../../tools/writepad/README.md)
- **Mac.** The local archive uses an ad hoc signature. For direct distribution, prepare Developer ID signing and notarization, then verify the signed result on a clean Mac. Inventory the shipped frameworks, libraries, and plugins. Include applicable license notices and the required access to exact corresponding Qt source, including any patches. [Apple notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution) · [Qt licensing obligations](https://www.qt.io/development/open-source-lgpl-obligations)

Verify each final ZIP with `unzip -tq <archive.zip>`. Run `shasum -a 256 <archive.zip>` and include the result with the download. Include the SHA-256 of the matching source archive if you ship the handwriting helper. Publish binaries only for the source commit used to build them. [GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)
