# Develop and build Quaderno

**English** · [Italiano](../SVILUPPO.md)

Quaderno is a C++17 and Qt Quick application. The build produces `quaderno.app` on macOS and a Linux AArch64 executable for the reMarkable Paper Pro. Run the commands in this guide from the repository root in Bash or Zsh. Use Bash on Linux for cross-compilation.

- [Install Quaderno and restore the Paper Pro interface](INSTALLATION.md)
- [User guide](USER_GUIDE.md)
- [Data, backups, and transfers](DATA.md)

## Architecture

| Component | Responsibility |
| --- | --- |
| [`qml/`](../../qml) and [`src/main.cpp`](../../src/main.cpp) | Shared interface, navigation, editor, dialogs, and service initialization. The build embeds the QML files in the `Quaderno` module. |
| [`Notebook`](../../src/notebook.cpp) | Library, pages, layers, ink history, JSON import, and export. It uses `QLockFile` to coordinate writes and `QSaveFile` to replace the data file atomically. |
| [`InkItem`](../../src/inkitem.cpp) and [`inkrender`](../../src/inkrender.cpp) | Mouse input, Qt tablet input, and Marker input on Linux. Strokes use normalized coordinates. On the Paper Pro, Marker pressure comes from `/dev/input` events. |
| [`PageContent`](../../src/pagecontent.cpp) and [`PagePreviewItem`](../../src/pagepreviewitem.cpp) | Render templates, page elements, thumbnails, and charts. PDF export uses the same C++ code to draw page content and strokes. |
| [`shaperecognition`](../../src/shaperecognition.cpp) and [`ChartRecognizer`](../../src/chartrecognizer.cpp) | Recognize shapes locally and reconstruct charts from selected strokes. |
| [`pdfimport`](../../src/pdfimport.cpp) and [`epubimport`](../../src/epubimport.cpp) | Import local documents. PDF import uses Core Graphics and PDFKit on macOS and PDFium on the Paper Pro. EPUB import uses libarchive. |
| [`AppleShare`](../../src/appleshare.cpp), [`UsbTransfer`](../../src/usbtransfer.cpp), and [`Handwriting`](../../src/handwriting.cpp) | Share PDFs through macOS, transfer notebooks over SSH, and transcribe handwriting locally with WritePad on Paper Pro or PencilKit and Vision on macOS. |
| [`AppLanguage`](../../src/applanguage.cpp) and [`translations/`](../../translations) | Save the interface language and load the embedded English catalogs. Italian strings are defined in the QML and C++ sources. |
| [`WirelessSync`](../../src/wirelesssync.cpp), [`SyncChannel`](../../src/syncchannel.cpp), and [`BleSync`](../../src/blesync.cpp) | Pair devices, authenticate a TLS 1.3 session over Bluetooth LE or local TCP, and manage notebook previews, updates, and conflicts. |
| [`BluetoothSettings`](../../src/bluetoothsettings.cpp) | Discover accessories on Paper Pro and manage pairing, codes, and connections through BlueZ. |
| [`DeviceInfo`](../../src/deviceinfo.cpp), [`WifiSettings`](../../src/wifisettings.cpp), and [`PinLock`](../../src/pinlock.cpp) | Read device status, manage Wi-Fi over D-Bus, and handle the app PIN. |
| [`scripts/`](../../scripts) | Install over SSH, start a temporary systemd trial session, and optionally add a launcher to the AppLoad menu. |

The Qt `epaper` platform enables the device services. The `--tablet` option enables the tablet layout and on-screen keyboard on macOS as well. During configuration, CMake adapts the QML accessibility properties to the firmware's Qt kit. [Qt QML modules with CMake](https://doc.qt.io/qt-6/qt-add-qml-module.html) · [Qt Quick on reMarkable tablets](https://developer.remarkable.com/documentation/qt_epaper)

## Diagram and orientation implementation (0.2.8)

The [Notebook model](../../src/notebook.cpp) treats diagrams as indexed library items alongside charts and tables. `createDiagramBook()` creates a genuine blank page and optional `workspace: "diagrams"` metadata. `diagramBook` identifies that workspace. `openLibraryItem()` returns `workspace: true` for its synthetic empty entry; navigation opens the page without creating an empty diagram object or opening its editor. Notebook updates preserve existing workspace metadata when older peers omit it.

[`DiagramRecognition`](../../src/diagramrecognizer.cpp) performs deterministic local geometry analysis. `Notebook::diagramHandwritingPlan()` adds current notebook/page/layer identity and the current manifest revision. `applyDiagramHandwriting()` revalidates the source and reviewed topology before one atomic ink/item edit. Only supported label text edits are accepted. Replacement removes the union of recognized indices plus explicitly accepted nonblank labels; overlapping candidates are deduplicated. Unassigned strokes remain. A bounded `handwritingSource` supports recovery after reopening; in-session undo is separate. Independent library copies omit that recovery source. [`DiagramExport`](../../src/diagramexport.cpp) shares its QIODevice writer between standalone `.drawio` and staged Markdown asset export.

[`ScreenOrientation`](../../src/screenorientation.cpp) reads the supported IIO accelerometer on Paper Pro through a worker, validates the mount matrix and filters ambiguous or unstable samples. Only one read is in flight. Failure resets candidate stability; repeated failures retry discovery without inventing an orientation. No Qt Sensors dependency is required. Main enables sampling only when automatic orientation is enabled, the app is awake and PIN-unlocked. Its existing logical scene/overlay rotation handles pen and touch coordinates. Fixed reading orientation takes precedence; changes defer while an interaction or modal operation is active. Manual angle selection disables automatic rotation. The preference defaults to off.

Automatic diagram proposals are separate from commits: a pause can expose a dismissible suggestion only in a diagram workspace. Explicit review is still required. See [Diagrams](DIAGRAMS.md) and [settings](SETTINGS.md). The first 0.2.8 build passed all 15 suites on Mac and ARM, geometry/UI regressions, package checks and native startup. The user confirmed automatic portrait-to-landscape rotation, then a real drawing exposed overly strict geometry matching. Revised tolerances cover bounded contour imperfections and additional arrowhead gestures; ambiguous geometry remains ink. A separate sync-notification correction passed the three affected suites on each platform. The corrected Mac and ARM geometry/UI suites passed 2/2 (34 and 17 checks), with strict startup; the corrected build is installed on Paper Pro and passed strict native/AppLoad startup at 1620 × 2160 without QML errors. On November 18, 2026, the user confirmed that recognition, applying the result and opening the editor to modify it work on the tested flowchart on Paper Pro; see the [verification status](SETTINGS.md#verification-boundary).

## Build on macOS

Install CMake 3.21.1 or later, the Xcode build tools, OpenSSL 3, zlib, and Qt 6.5 or later with support for your compiler and Apple SDK. Install Qt Quick, Qt Quick Controls, Qt Quick Dialogs, Qt Network, Qt D-Bus, and Qt Linguist Tools. The verified Mac build uses Apple silicon, the macOS 27.2 SDK, and Qt 6.8.3 with the AGL linking fix. The resulting ZIP requires macOS 27.2 or later. Set CMake’s `OPENSSL_ROOT_DIR` if OpenSSL is outside its default search paths.

Set `QT_PREFIX` to the path of your Qt installation for macOS and configure a Release build. The first line shows an example path for the Qt Online Installer. If the linker reports `framework 'AGL' not found`, use a Qt kit that includes the upstream fix and configure a new build directory. [Qt support for macOS](https://doc.qt.io/qt-6/macos.html) · [Upstream Qt fix for AGL](https://github.com/qt/qtbase/commit/cdb33c3d)

```bash
QT_PREFIX="$HOME/Qt/6.8.3/macos"
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_PREFIX" -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-mac --parallel
open build-mac/quaderno.app
```

With Swift and the macOS 27 SDK or later, CMake also builds `quaderno-handwriting` and copies it next to the main executable in `Contents/MacOS`. **Trascrivi in italiano** (Transcribe in Italian) is available on macOS 27 or later when the bundle contains the executable helper. The app's minimum macOS version depends on the Qt kit and deployment target used to build it. [Handwriting recognition with PencilKit](https://developer.apple.com/documentation/pencilkit/recognizing-handwriting-and-converting-to-text)

```bash
xcrun --sdk macosx --show-sdk-version
file build-mac/quaderno.app/Contents/MacOS/quaderno
xcrun vtool -show-build build-mac/quaderno.app/Contents/MacOS/quaderno
ls -l build-mac/quaderno.app/Contents/MacOS/quaderno-handwriting
```

## Build for Paper Pro 3.29.1.0

Use Linux or a Linux VM with CMake, Ninja, Bash, `file`, and `readelf`. Choose a `ferrari` SDK for the architecture of the Linux build host. The verified build targets firmware **3.29.1.0** and uses **Codex 6.0.112**. It produces a Linux AArch64 ELF executable. [Choose a reMarkable SDK](https://developer.remarkable.com/documentation/sdk) · [Ferrari 3.29.1.0 SDK for Linux AArch64 hosts](https://storage.googleapis.com/remarkable-codex-toolchain/3.29.1.0/remarkable-production-image-6.0.112-ferrari-public-aarch64-toolchain.sh)

Download and run the SDK installer, then choose an installation directory. Set the first line to that directory. Use a fresh build directory if you change SDKs or generators.

```bash
PAPER_PRO_SDK='/path/to/the/installed/sdk'
source "$PAPER_PRO_SDK/environment-setup-cortexa53-crypto-remarkable-linux"

cmake -S . -B build-paper-pro -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE" \
  -DQt6_DIR="$OECORE_TARGET_SYSROOT/usr/lib/cmake/Qt6" \
  -DQT_HOST_PATH="$OECORE_NATIVE_SYSROOT/usr" \
  -DCMAKE_SKIP_RPATH=ON
cmake --build build-paper-pro --parallel
file build-paper-pro/quaderno
readelf -l build-paper-pro/quaderno
readelf -d build-paper-pro/quaderno
```

Check that `file` reports a 64-bit AArch64 ELF executable. `readelf` shows the dynamic interpreter and dependencies. Check the `RPATH` and `RUNPATH` entries for paths specific to the build host. On the tablet, `try-on-device.sh --check` verifies the executable, library resolution, and the Qt e-paper plugins.

To enable offline Italian transcription on Paper Pro, build the separate WritePad helper with the same SDK. Place the helper and its Italian dictionary beside `quaderno` in the package. The installer checks that the helper can load the dictionary. Include the license notices and corresponding source when distributing the helper. [Build the handwriting helper](../../tools/writepad/README.md)

## Test changes

The repository includes the source code and installation scripts. Use sample notebooks, images, and test credentials when following the checks below. The 0.2.8 regression targets include `diagramrecognition_tests`, `diagramsintegration_tests` and `screenorientation_tests` for geometry, atomic conversion/recovery, library/UI workflows, and sensor filtering. Their presence is not a claim that a device acceptance run has passed.

GitHub Actions compiles a desktop build on Linux and checks startup in both desktop and tablet layouts. To run the same script checks locally, install ShellCheck and run:

```bash
sh scripts/check-shell.sh
```

To preview the tablet layout on macOS, launch the development executable from Terminal. Close other Quaderno instances first if you plan to use the same library.

```bash
build-mac/quaderno.app/Contents/MacOS/quaderno --tablet
```

The startup check uses a temporary profile and the Qt kit’s `offscreen` plugin. Pass the built executable and, on macOS, the Qt kit’s plugin directory.

```bash
QT_PLUGIN_PATH="$QT_PREFIX/plugins" \
  python3 scripts/check-startup.py build-mac/quaderno.app/Contents/MacOS/quaderno
# On Linux
python3 scripts/check-startup.py build-ci/quaderno
```

| Area | Manual check |
| --- | --- |
| Library and persistence | Create a notebook with several pages, tags, folders, and layers. Reopen the app and check the data. Import the same JSON file twice and test a conflict with an edited copy. |
| Pen and layers | Write, erase, undo, and redo. Hide and show a layer. Check smart shapes, the lasso, and restoration of the original stroke after reopening the notebook. |
| Charts and documents | Select two axes and either a curve or bars. Edit the data, set the scale, and compare the CSV with the preview. Import a PDF and a DRM-free EPUB, add annotations, and check the exported PDF. |
| Handwriting transcription | Use a short sample. Check that the preview appears automatically on Paper Pro. Review the text, save it to a new page, search for it, and verify that the original ink is still available. |
| Diagrams | Create an empty diagram notebook, reopen it, draw and review a flowchart, exclude one label, apply, undo/redo and recover original ink after restart. Insert a copy into Writing and verify its independence, Markdown PNG/`.drawio` and notebook transfer. |
| Automatic orientation | With a supported sensor, test all four stable angles, flat/ambiguous positions, input alignment, popup deferral, PIN/sleep, manual override and fixed book orientation. No new angle should be inferred from a read failure. |
| Physical Paper Pro | Check Marker coordinates and pressure, touch alignment, e-paper refresh and legibility, reading light, and Wi-Fi. Try **Esci** (Exit) and let a trial session expire. Check that the reMarkable interface returns in both cases. |

The firmware build was also checked in a Linux AArch64 VM and on a physical Paper Pro. Repeat device checks for each downloadable build. [Release and test status](RELEASE.md) · [Run a USB trial](INSTALLATION.md)

### Paper Pro e-paper refresh

Quaderno runs Qt Quick with the `epaper` backend. As you write, `InkItem` redraws the rectangle around the changed stroke. The eraser rebuilds the ink layer when it finds a stroke to remove. A Qt repaint updates the scene content. Clearing residual images, or ghosting, depends on the physical update of the Gallery 3 panel. [Qt Quick on reMarkable](https://developer.remarkable.com/documentation/qt_epaper) · [QQuickPaintedItem::update](https://doc.qt.io/qt-6/qquickpainteditem.html#update) · [Gallery 3 display](https://blog.eink.com/new-remarkable-paper-pro)

In `libqsgepaper.so` from firmware **3.29.1.0**, `scheduleGhostRemoval()` queues a full cleanup for the next physical update. If the pixels stay the same, the request stays pending. `clearGhosting()` immediately refreshes the entire panel with the current frame and waits for the controller. Both methods belong to the internal firmware plugin.

Quaderno enables **Refresh screen** on Paper Pro with firmware 3.29.1.0 when the loaded plugin exposes `clearGhosting()`. Page turns use Qt’s normal repaint. If Qt is slow to process a frame, the automatic option requests an update through `QWindow::requestUpdate()`. Tablet dialogs leave the background undimmed so opening a dialog repaints a smaller area. [QWindow::requestUpdate](https://doc.qt.io/qt-6/qwindow.html#requestUpdate)

On the verified firmware, Quaderno uses the plugin’s `UI` and `Content` modes for the regions Qt repaints. `UI` covers the interface and note pages. `Content` covers pages with a document, image, chart, or diagram. A mode change takes effect on the next frame, and the controller chooses the waveform for the panel’s conditions.

On a physical device, switch from a page with black and colored strokes to a blank page, page quickly through a PDF, and erase a group of strokes. Also try **Refresh screen** on a stationary page in both the editor and **Full page** view. Match entries from `QT_LOGGING_RULES='rm.framebuffer.updates.debug=true'` to a fixed-exposure video of the display. Measure the time from the tap to a readable page, residual images, and the number of flashes. Qt's `frameSwapped` signal marks frame submission. The video shows when the panel settles. [QQuickWindow::frameSwapped](https://doc.qt.io/qt-6/qquickwindow.html#frameSwapped)

### Bluetooth accessories on Paper Pro

Quaderno uses BlueZ for discovery and pairing. Its pairing agent handles codes for operations initiated by Quaderno. The firmware handles the HID profile and Qt receives keyboard events from the system. To test, put a keyboard in pairing mode, pair it, type in Markdown and in a table cell, then disconnect, reconnect, and forget it. [BlueZ Adapter API](https://github.com/bluez/bluez/blob/master/doc/org.bluez.Adapter.rst) · [BlueZ Agent API](https://github.com/bluez/bluez/blob/master/doc/org.bluez.Agent.rst)

## Package the builds

### Mac with Apple silicon

Run `macdeployqt` on a copy of the development bundle. The `-qmldir` option lets Qt include the QML plugins the app uses. If your Qt distribution includes optional plugins with missing dependencies, check which plugins Quaderno needs and remove unused plugins from the copy before signing. [Deploy Qt apps on macOS](https://doc.qt.io/qt-6/macos-deployment.html)

```bash
mkdir -p build-mac/distribuzione
APP='build-mac/distribuzione/Quaderno.app'
ditto build-mac/quaderno.app "$APP"
"$QT_PREFIX/bin/macdeployqt" "$APP" -qmldir="$PWD/qml"

# Ad hoc signing for local testing.
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"
ditto -c -k --keepParent "$APP" build-mac/Quaderno-macOS.zip
unzip -tq build-mac/Quaderno-macOS.zip
shasum -a 256 build-mac/Quaderno-macOS.zip
```

For public macOS distribution, document the signing status. If you use Developer ID, sign the app and its embedded components and follow Apple's notarization instructions. The ad hoc signature above is for local testing. [Sign and notarize macOS software](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)

### Paper Pro

Use the packaging script with a Linux AArch64 build and a fresh output directory. It requires `zip`, `unzip`, and a SHA-256 tool.

```bash
sh scripts/package-paper-pro.sh build-paper-pro/quaderno build-paper-pro/packages
```

To include the offline transcription helper, run the packaging script in the matching SDK environment and pass the pinned WritePad tarball as its third argument. Distribute the source companion it creates alongside the ZIP. [WritePad build and packaging](../../tools/writepad/README.md)

To verify the package, extract it on a test machine and run the USB installer with `--install-only`. Then start a trial session and check that the reMarkable interface returns when it ends. Record the commit, tested firmware version, and ZIP's SHA-256 checksum for each build you distribute.

## Incremental sync protocol

See [Incremental wireless sync](SYNC_PROTOCOL.md) for capability negotiation, page and asset manifests, durable chunk acknowledgments, resume behavior, cache limits and regression tests.
