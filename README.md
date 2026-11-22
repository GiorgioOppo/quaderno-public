# Quaderno

**English** · [Italiano](README.it.md)

Quaderno is an experimental native note-taking app for **reMarkable Paper Pro** and **Macs with Apple silicon**. It brings handwriting, text, tables, and document annotations together in a library stored on your device. **0.2.19 is a source preview.** Build this version from the repository. The `v0.1.0` tag identifies the original source preview. [Build instructions](docs/en/DEVELOPMENT.md) · [Release status](docs/en/RELEASE.md)

![Quaderno editor showing a vector chart on a sample page](docs/immagini/editor.png)

*Mac preview with sample data.*

## What you can do

- Browse **Books**, **Writing**, **Charts**, **Tables**, and **Diagrams**. Organize notebooks and books into folders, filter by favorites or tags, and restore items from the trash.
- Write Markdown alongside freehand ink, images, charts, and tables. Use five paper templates, ink layers, smart shapes, and the lasso.
- On Paper Pro, use the Marker to draw around diagram, table and text areas on a mixed page. The optional offline image OCR proposes Latin-script prose, table contents and labels for diagram shapes from PDFs and image backgrounds. Review the drafts, then save one editable notebook with links to the diagram and table. [Mixed-page recognition](docs/en/USER_GUIDE.md#recognize-a-mixed-page-on-paper-pro)
- Import CSV or TSV files into editable tables, calculate sums and averages, and export the results as CSV.
- Create line and bar charts from editable points or choose a table and its X/Y columns. A table on the same page updates its chart automatically. A source on another page supplies an editable copy of the current values. Convert hand-drawn sketches to editable charts and export calibrated data as CSV.
- Build flowcharts with a shape palette, canvas connectors and an inspector in **Diagrams**. Review a hand-drawn flowchart proposal, add new freehand shapes and arrows to an existing diagram, edit its shapes, labels and arrows, and reuse it on other pages. Export `.drawio` files or Markdown with a diagram PNG and editable `.drawio` attachment. [Diagrams](docs/en/DIAGRAMS.md)
- Highlight and underline PDFs and DRM-free, reflowable EPUBs. Place a **Post-it** at a point on the page, type or write inside it, and reopen its numbered marker to edit it. Add a link at a point on the page to open a chart, table or diagram. Zoom, bookmark pages, search text in newly imported PDFs, and export annotated pages with readable note appendices. [Book notes](docs/en/BOOK_NOTES.md)
- Import local PDF and DRM-free EPUB copies from Apple Books on Mac, then send the annotated copy to Paper Pro with Quaderno. [Apple Books on Mac](docs/en/USER_GUIDE.md#notes-books-and-icloud-on-mac)
- Import editable Markdown, including from a direct HTTPS URL. Export it with linked images for ink, charts, diagrams, and embedded photos. On Mac, save the text in an Obsidian vault. On Mac and Paper Pro, copy links to an Obsidian Publish site. [Markdown and Obsidian](docs/en/USER_GUIDE.md#markdown-and-obsidian)
- Sync Markdown notes directly between Paper Pro and Obsidian Sync over Wi‑Fi after one-time setup over USB‑C. [Obsidian Sync on Paper Pro](docs/en/USER_GUIDE.md#obsidian-sync-on-paper-pro)
- Transfer and sync whole Quaderno notebooks between Mac and Paper Pro over Bluetooth LE or local Wi-Fi, including book highlights, underlining and editable notes on every page. Choose the automatic connection and sync after edits, on opening a selected notebook or at intervals. Both apps must be open, unlocked and compatible with the notebook format. [Wireless synchronization](docs/en/SYNC_PROTOCOL.md) USB-C transfers remain available from the library and editor. Share PDF copies with Apple Notes from the Mac share menu, or save a PDF and import it in Apple Books.
- Scan for and pair Bluetooth accessories such as HID keyboards on Paper Pro. [Connect a Bluetooth keyboard](docs/en/USER_GUIDE.md#connect-a-bluetooth-keyboard)
- Transcribe Italian and English handwriting offline with the Paper Pro handwriting engine or on macOS 27 or later. Review the text, then search it to reopen the original ink page.
- Choose English or Italian for the interface. On Paper Pro, switch between Italian and US English QWERTY while typing, and edit existing text blocks with a tap.
- Customize reading and writing, toolbar actions, palm rejection, clock, Wi-Fi automation, backups and cache. Browsing Google Drive, Dropbox and OneDrive requires application OAuth registrations and provider sign-in. [Settings and connected services](docs/en/SETTINGS.md)
- On Paper Pro, optionally rotate the interface with a supported orientation sensor. Manual angles and per-book reading orientation remain available. [Rotation settings](docs/en/SETTINGS.md#rotation-reading-and-full-page-view)

## Controls for each section

The toolbar follows the task on the page. Frequent controls stay visible, with secondary actions under **More**.

| Section | Main actions |
| --- | --- |
| **Books** | Open in reading mode. Highlight, underline, place Post-its and links to library objects, then use **Back to book** to return to the source page. |
| **Writing** | Reach pen, color, thickness, favorites and lasso in a compact row. Edit Markdown, add text and images, and insert content from the library. |
| **Charts** | Choose manual points or a table source, select X/Y columns, review the preview, edit chart data and appearance, and export CSV. |
| **Tables** | Edit cells and formulas, create or import CSV tables, write into cells with the pen and lasso, build a chart, and export CSV. |
| **Diagrams** | Place shapes, connect nodes, edit properties in the inspector, recognize freehand flowcharts, and import or export `.drawio`. |

An action opens the only matching object directly. If a page contains several candidates, choose the intended one. Tables and linked charts, diagrams and writing can share a page. Open an object from its section to get the matching toolbar. [Function and workflow map](docs/en/FUNCTIONS_AND_WORKFLOWS.md)

For annotations to reach the other device, synchronize the **Quaderno book**, including its pages and original source. Copying the original PDF or EPUB alone does not carry Quaderno's annotations. With opening-based automation, changes made while a book stays open wait for another trigger; choose **After changes** for automatic synchronization after saved edits. New Post-its require **0.2.11 on both apps**. [Book notes and synchronization](docs/en/BOOK_NOTES.md)

## Compatibility and status

| Platform | Build status |
| --- | --- |
| Mac with Apple silicon | Tested on **macOS 27.2**. Requirements for source builds depend on the Qt kit and deployment target. |
| reMarkable Paper Pro (`ferrari`) | Tested with firmware **3.29.1.0**, including full-screen AppLoad startup and freehand Marker selection. |

Trying Quaderno on the Paper Pro requires Developer Mode. **Enabling Developer Mode resets the tablet to factory settings.** Back up your documents first. Quaderno runs in a separate session and the launcher restores the reMarkable interface when it ends. The optional menu entry requires an existing compatible XOVI and AppLoad installation. [Official Developer Mode guide](https://developer.remarkable.com/documentation/developer-mode) · [AppLoad setup](docs/en/INSTALLATION.md#start-from-the-appload-menu)

Quaderno stores its library in a local format. USB-C transfers run when you request them. A previously imported PDF needs to be imported again to index its embedded text.

## Getting started

On Paper Pro with firmware 3.29.1.0, **Settings → SSH access** controls when the tablet accepts new connections over Wi-Fi and USB-C. [SSH access](docs/en/SETTINGS.md#ssh-access-on-paper-pro)

Use 0.2.14 or later on both devices for notebooks containing page links (portable format 12), and transfer a link’s target notebook separately when it lives in another notebook. In automatic sync, paired devices check over Bluetooth for pending changes and use local Wi‑Fi for the transfer when reachable, with Bluetooth as a fallback. [Automatic sync](docs/en/SYNC_PROTOCOL.md#automatic-synchronization)

- **On Mac**, build for Apple silicon and open `Quaderno.app`. [Build instructions](docs/en/DEVELOPMENT.md) · [Installation](docs/en/INSTALLATION.md)
- **On Paper Pro**, check your model and firmware, build the corresponding package, and follow the USB-C installation steps. [Build and packaging](docs/en/DEVELOPMENT.md#paper-pro) · [Paper Pro installation](docs/en/INSTALLATION.md)
- **In Quaderno**, create a notebook, choose a page template, and start writing. Select the page count at the bottom to open thumbnails and page actions. [User guide](docs/en/USER_GUIDE.md)

## Documentation

| Guide | Contents |
| --- | --- |
| [Quaderno 0.2 migration](docs/en/UPGRADE_0_2.md) | Migration from older builds and data recovery |
| [0.2.1 usability update](docs/en/USABILITY_0_2_1.md) | 31 new improvements, workflows, validation status, and follow-up work |
| [Installation](docs/en/INSTALLATION.md) | Mac, USB-C, AppLoad, updates, and recovery |
| [User guide](docs/en/USER_GUIDE.md) | Library, writing, charts, transcription, and documents |
| [Settings and connected services](docs/en/SETTINGS.md) | Twenty settings groups, automatic rotation, diagram recognition, reading preferences, automation and platform limits |
| [Cloud account setup](docs/en/CLOUD_ACCOUNTS.md) | OAuth registrations, sign-in assistant, manual transfers, security and pending live checks |
| [Local library maintenance](docs/en/LIBRARY_MAINTENANCE.md) | Scheduled backups, verified retention, cache limits and local reading preferences |
| [Sections and file workflows](docs/en/FEATURES.md) | Books, Writing, Charts, Tables, Diagrams, file formats, and how content moves between sections |
| [Diagrams](docs/en/DIAGRAMS.md) | Hand-drawn recognition, label review, editable shapes, original ink recovery and reuse |
| [Book notes](docs/en/BOOK_NOTES.md) | Anchored Post-its, keyboard and handwriting, whole-book sync, PDF appendices and Markdown assets |
| [Wireless synchronization](docs/en/SYNC_PROTOCOL.md) | Wi-Fi/Bluetooth automation, annotated books, version checks and transfer recovery |
| [Data and backups](docs/en/DATA.md) | File locations, backups, importing, and the app PIN |
| [Development](docs/en/DEVELOPMENT.md) | Architecture, builds, and testing |
| [Release preparation](docs/en/RELEASE.md) | Package status, testing, and distribution |
| [Changelog](CHANGELOG.md) | Features in the development preview |

## Contributing and security

See the [contribution guide](CONTRIBUTING.md) for fixes and bug reports and the [security policy](SECURITY.md) for vulnerability reports.

Quaderno is an independent project. The application code is available under the [MIT License](LICENSE). The optional WritePad helper uses GPL-3.0-or-later. [Third-party notices](THIRD_PARTY_NOTICES.md)
