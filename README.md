# Quaderno

**English** · [Italiano](README.it.md)

Quaderno is an experimental native note-taking app for **reMarkable Paper Pro** and **Macs with Apple silicon**. It brings handwriting, text, tables, and document annotations together in a library stored on your device.

![Quaderno editor showing a vector chart on a sample page](docs/immagini/editor.png)

*Mac preview with sample data.*

## What you can do

- Browse **Books**, **Writing**, **Charts**, and **Tables**. Organize notebooks and books into folders, filter by favorites or tags, and restore items from the trash.
- Write Markdown alongside freehand ink, images, charts, and tables. Use five paper templates, ink layers, smart shapes, and the lasso.
- Import CSV or TSV files into editable tables, calculate sums and averages, and export the results as CSV.
- Create line and bar charts from editable points or link them to a table on the same page. Convert hand-drawn sketches to editable charts and export calibrated data as CSV.
- Draw flowcharts with editable shapes and arrows and export them as `.drawio` files. [Flowcharts](docs/en/USER_GUIDE.md#flowcharts)
- Highlight, underline, and add editable notes to PDFs and DRM-free, reflowable EPUBs. Zoom, bookmark pages, search text in newly imported PDFs, and export the annotated copy as PDF.
- Import local PDF and DRM-free EPUB copies from Apple Books on Mac, then send the annotated copy to Paper Pro with Quaderno. [Apple Books on Mac](docs/en/USER_GUIDE.md#notes-books-and-icloud-on-mac)
- Import editable Markdown, including from a direct HTTPS URL. Export it with linked images for ink, charts, and embedded photos. On Mac, save the text in an Obsidian vault. On Mac and Paper Pro, copy links to an Obsidian Publish site. [Markdown and Obsidian](docs/en/USER_GUIDE.md#markdown-and-obsidian)
- Sync Markdown notes directly between Paper Pro and Obsidian Sync over Wi‑Fi after one-time setup over USB‑C. [Obsidian Sync on Paper Pro](docs/en/USER_GUIDE.md#obsidian-sync-on-paper-pro)
- Transfer and sync Quaderno notebooks between Mac and Paper Pro over Bluetooth LE or local Wi-Fi. USB-C transfers are available from the library and editor. Share PDF copies with Apple Notes from the Mac share menu, or save a PDF and import it in Apple Books.
- Scan for and pair Bluetooth accessories such as HID keyboards on Paper Pro. [Connect a Bluetooth keyboard](docs/en/USER_GUIDE.md#connect-a-bluetooth-keyboard)
- Transcribe Italian handwriting offline with the Paper Pro handwriting engine or on macOS 27 or later. Review the text, then search it to reopen the original ink page.
- Choose English or Italian for the interface. On Paper Pro, switch between Italian and US English QWERTY while typing, and edit existing text blocks with a tap.

## Compatibility and status

| Platform | Build status |
| --- | --- |
| Mac with Apple silicon | The current local build targets **macOS 27.2 or later**. Requirements for source builds depend on the Qt kit and deployment target. |
| reMarkable Paper Pro (`ferrari`) | Firmware **3.29.1.0**. Quaderno has run on a physical tablet with AppLoad, native Marker input, and e-paper refresh. |

Trying Quaderno on the Paper Pro requires Developer Mode. **Enabling Developer Mode resets the tablet to factory settings.** Back up your documents first. Quaderno runs in a temporary session, and the scripts restart the reMarkable interface when that session ends. [Official Developer Mode guide](https://developer.remarkable.com/documentation/developer-mode)

Quaderno stores its library in a local format. USB-C transfers run when you request them. A previously imported PDF needs to be imported again to index its embedded text.

## Getting started

Version **0.1.0** is a source preview. See the release guide for the status of downloadable packages. [Release preparation](docs/en/RELEASE.md)

- **On Mac**, open `Quaderno.app` from the Apple silicon package if you have it, or build from source. [Installation](docs/en/INSTALLATION.md) · [Build instructions](docs/en/DEVELOPMENT.md)
- **On Paper Pro**, check your model and firmware, then follow the USB-C installation steps. [Paper Pro installation](docs/en/INSTALLATION.md)
- **In Quaderno**, create a notebook, choose a page template, and start writing. Select the page count at the bottom to open thumbnails and page actions. [User guide](docs/en/USER_GUIDE.md)

## Documentation

| Guide | Contents |
| --- | --- |
| [Installation](docs/en/INSTALLATION.md) | Mac, USB-C, AppLoad, updates, and recovery |
| [User guide](docs/en/USER_GUIDE.md) | Library, writing, charts, transcription, and documents |
| [Sections and file workflows](docs/en/FEATURES.md) | Books, Writing, Charts, Tables, file formats, and how content moves between sections |
| [Data and backups](docs/en/DATA.md) | File locations, backups, importing, and the app PIN |
| [Development](docs/en/DEVELOPMENT.md) | Architecture, builds, and testing |
| [Release preparation](docs/en/RELEASE.md) | Package status, testing, and distribution |
| [Changelog](CHANGELOG.md) | Features in the development preview |

## Contributing and security

See the [contribution guide](CONTRIBUTING.md) for fixes and bug reports and the [security policy](SECURITY.md) for vulnerability reports.

Quaderno is an independent project. The application code is available under the [MIT License](LICENSE). The optional WritePad helper uses GPL-3.0-or-later. [Third-party notices](THIRD_PARTY_NOTICES.md)
