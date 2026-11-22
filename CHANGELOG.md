# Changelog

**English** · [Italiano](CHANGELOG.it.md)

## 0.2.23 development

- Add an editable UML palette with classes, interfaces, objects, actors, use cases, components, packages, deployment nodes and lifelines. The shapes can be exchanged as draw.io files. Quaderno notebooks containing UML shapes use portable format 14.
- Improve recognition of short handwritten labels close to small diagram shapes in PDFs and images. Fit labels to their shapes when rendering a diagram on the page.

## 0.2.22 development

- Unify sharing and exports on Paper Pro and Mac. The format picker offers annotated PDF for a page or the entire notebook, PNG for the current page, Markdown with optional attachments, editable draw.io, CSV, the native Quaderno file, and original PDF or EPUB when retained.
- Use the native macOS share and print dialogs. Sharing Markdown with attachments bundles them into a ZIP. On Paper Pro, open wireless synchronization to transfer the notebook to a Mac for printing or save an annotated PDF.

## 0.2.21 development

- Improve recognition of small, sideways and vertically stacked handwritten labels in diagrams. For Marker strokes, the review can retry an individual label with a different reading orientation. For PDFs and images, the updated optional OCR helper also evaluates upright stacked letters in vertical crops.

## 0.2.20 development

- Improve how handwritten captions are associated with their shapes. Review and edit the suggested captions before accepting the diagram.
- Add database, subprocess, display, manual input, preparation, delay, off-page connector and cloud to the editable palette and compatible draw.io import and export. Native notebooks with those shapes require portable format 13. Diagrams using the original palette keep the minimum format required by their contents.

See [Diagrams](docs/en/DIAGRAMS.md).

## 0.2.19 development

- Draw freehand outlines with the Marker when selecting the diagram, table or text in a mixed page. Quaderno closes the outlines when you lift the pen.
- Extend optional local OCR to the **Table** area of a mixed PDF page or a page with an image background on Paper Pro. A clear grid can yield editable cells and preserve blank positions. For layouts without a clear grid, review the proposed rows and add column separators as needed.
- Use the optional on-device OCR engine to propose editable labels for shapes recognized in PDF and image diagrams.

All 22 CTest suites and the desktop and tablet startup checks passed on macOS and with the Paper Pro ARM64 SDK. Physical Paper Pro testing on firmware 3.29.1.0 covered full-screen AppLoad launch and Marker outlines. An isolated run exercised OCR and saved one notebook with links to editable table and diagram pages. Review OCR drafts, especially similar characters, labels and cell positions. [User guide](docs/en/USER_GUIDE.md#recognize-a-mixed-page-on-paper-pro) · [Release status](docs/en/RELEASE.md)

## 0.2.18 development

- Add an optional on-device OCR engine for the **Text** area of mixed-page recognition on Paper Pro. It reads Latin-script text from rendered PDF pages and images. Review the draft against the source, especially links, paths, names and punctuation.

See [Recognize a mixed page on Paper Pro](docs/en/USER_GUIDE.md#recognize-a-mixed-page-on-paper-pro).

## 0.2.17 development

- Save mixed-page conversions as one editable notebook in Writing. Its first page links to the diagram and table on dedicated pages in the same notebook. One Quaderno file or sync operation carries the text and both editable objects.
- Duplicating a notebook or keeping a conflict copy preserves its links to its own pages.
- Improve detection of nested boxes and arrows in handwritten PDFs. Table borders produce fewer stray rows. Transcriptions and labels can still be reviewed in the draft.

See [Recognize a mixed page on Paper Pro](docs/en/USER_GUIDE.md#recognize-a-mixed-page-on-paper-pro).

## 0.2.16 development

- On Paper Pro, select diagram, table and text areas on a notebook page or imported PDF. Review the locally generated drafts and save three editable notebooks together. The source page stays available.
- In Writing, preview linked objects and open or edit their originals. Send the Writing notebook and its target notebooks separately to keep the links usable on the other device.

See [Recognize a mixed page on Paper Pro](docs/en/USER_GUIDE.md#recognize-a-mixed-page-on-paper-pro).

## 0.2.15 development

- Add SSH access settings on Paper Pro with firmware 3.29.1.0. Wi-Fi uses the persistent reMarkable setting. The USB-C control blocks or allows new connections for the current boot. The Bluetooth row reports that the firmware has no dedicated SSH listener.

See [SSH access on Paper Pro](docs/en/SETTINGS.md#ssh-access-on-paper-pro).

## 0.2.14 development

- Give Books, Writing, Charts, Tables and Diagrams separate page menus. Books open in reading mode and offer highlights, underlines, sticky notes and links to library objects; Writing keeps pen settings and the lasso close at hand.
- Add anchored links from a book page to the original chart, table or diagram and a return action to the source page. Page links require portable format 12. Transfer notebooks containing linked targets separately.
- Let chart authors pick X and Y columns from tables. A source on the same page stays linked; a source on another page provides an editable copy of its calculated values. The table menu groups formulas, CSV, handwriting conversion and chart creation.
- Build flowcharts with a shape palette, drag-to-place controls, connectors and an inspector. The diagram menu groups sketch recognition and draw.io import and export.

See [the five sections and their workflows](docs/en/FUNCTIONS_AND_WORKFLOWS.md#the-five-sections).

## 0.2.13 development

- Start automatic sync on paired Bluetooth LE. Check for pending changes before trying the Paper Pro’s current local Wi-Fi address. Transfer over Wi-Fi when the connection is authenticated and fall back to Bluetooth if Wi-Fi is unavailable or drops.
- Keep the notebook queue, saved chunks and conflict checks across transport changes. An interrupted apply with an uncertain result requires fresh review. The handoff between physical devices remains to be tested.

See the [automatic sync protocol](docs/en/SYNC_PROTOCOL.md#automatic-synchronization).

## 0.2.12 development

- Add a bilingual map of the five sections, file formats, cross-section workflows, device features and integrations.
- Keep the most useful section actions visible. Open secondary actions from **More**. Keep the object picker when several items match. Reading controls wrap to two rows on the smallest widths.
- Choose CSV and draw.io files before opening an editor. Canceling the file chooser returns to the sheet. An import queued for a different page is ignored.
- Pinch and pan with two fingers on writable sheets, including in landscape. Tap the zoom percentage to fit the sheet. On Mac, Quaderno accepts native trackpad pinch events. Book gestures retain their reading preferences, and adding a second finger to a swipe does not turn the page.

The five affected suites and rendered startup passed on the final Mac and ARM builds. Checks include a 380 px window at 130%, landscape layout, keyboard navigation, CSV and draw.io import, and zoom and pan. Physical gesture validation remains open.

See the [function and workflow map](docs/en/FUNCTIONS_AND_WORKFLOWS.md).

## 0.2.11 development

- Add **Post-its anchored to a point on a book page**. Combine editable keyboard text and freehand ink, choose a color, reopen a numbered marker, move it or delete it. Saving checks the original book, page and note to prevent a stale draft from overwriting changed content. Create/edit/move/delete operations support page Undo/Redo, and typed note text participates in search.
- Keep the full note readable in exports: PDF places numbered markers on the original pages and appends paginated text and separate handwriting pages; Markdown with assets includes text and a handwriting PNG. Native Quaderno export and synchronization retain editable text and individual strokes.
- Give **Books, Writing, Charts, Tables and Diagrams** their own context toolbar. Books exposes annotation and whole-book sync actions; Writing offers Markdown/text/images/library content; Charts exposes data and table-derived charts; Tables exposes cells, formulas and CSV; Diagrams exposes recognition, shapes and `.drawio` import. Actions target the only matching item directly or ask which item to use when several exist.
- Make whole-book annotation transfer explicit and show its synchronization status in Books. Highlights and underlining already belong to normal page content; transfers preserve all pages and hidden layers as well as new notes. Document the need for both apps to remain open and unlocked, a matching native Quaderno book, and the appropriate automation trigger.
- Introduce portable format **11** for page notes and version 11 metadata for new local saves. Portable exports without new content continue using their minimum required format. Advertise the supported notebook format during wireless negotiation and reject incompatible content before sending chunks; a previous incremental peer without an explicit capability is treated as supporting format 9. Update both apps for new notes.
- Recover hand-drawn ovals and process boxes with small gaps or irregular sides. Recognize explicit arrowheads drawn with a straight or elbow shaft and separate arrowheads with a short stem. Leave unheaded or ambiguous connectors as ink.
- **Validation:** The macOS suite and affected ARM64 checks passed. Device startup via AppLoad preserved the library. Replay checks covered additional shapes and explicit arrowheads.

See [book notes](docs/en/BOOK_NOTES.md), [section workflows](docs/en/FEATURES.md), [synchronization](docs/en/SYNC_PROTOCOL.md) and [data format compatibility](docs/en/DATA.md#library-format-versions).

## 0.2.10 development

- Recognize handwritten additions in the context of an already converted flowchart. The preview contains the complete updated diagram, including new shapes or arrows between existing nodes; applying updates the same page item rather than creating a second fragment.
- Preserve existing node/edge identifiers, edited labels, connector routes and page geometry. Existing labels remain read-only during recognition; review OCR only for new parts and use the diagram editor to change previous labels. Preserve visual text and line size when the diagram bounds expand.
- Choose the target explicitly when several diagrams are possible, or create a separate diagram. Keep uncertain ink and prevent duplicate geometry from retained original strokes. A proposal without valid additions cannot be applied.
- Apply the item replacement and optional ink removal in one page Undo/Redo operation. Persistent recovery retains an editable baseline diagram and removed addition strokes; later extensions on the same layer share that baseline. Recovery restores the original diagram instead of removing it.
- Introduce portable format **10** when `styleScale` or `handwritingSource.baseDiagram` is required, and use metadata version 10 for new local saves. Update both apps before transferring affected notebooks; older-content portable exports still choose their minimum required format.
- **Validation:** The affected Mac and ARM64 suites and rendered startup checks passed. Device checks confirmed that an added shape and arrow update the existing diagram and retain its labels.

See [extending diagrams](docs/en/DIAGRAMS.md#extend-an-already-converted-diagram) and [data format compatibility](docs/en/DATA.md#library-format-versions).

## 0.2.9 development

- Extend automatic notebook synchronization to local Wi-Fi and paired Bluetooth LE. Choose automatic selection with Wi-Fi preferred, Wi-Fi only, or Bluetooth only. Automatic work does not enable a disabled Bluetooth radio.
- Add synchronization after saved changes, with a quiet period after the last edit. Keep opening-based and interval modes, and catch up selected notebooks when automation is enabled or the peer reconnects. New installations remain manual by default.
- Restrict trusted SSID rules to Wi-Fi; the optional tablet-charging requirement applies to both connections. Keep immediate PIN, sleep and editor guards, explicit first-transfer review, conflict policy and manual review for an uncertain apply result.
- Improve recovery with increasing waits between connection attempts, bounded transfer retries, and a saved checkpoint for safely interrupted automatic work. Reuse matching received chunks through a fresh preview, without replaying an earlier approval.
- Treat legacy pending transfers without a saved phase/revision and interrupted manual applies as uncertain; automatic catch-up cannot resend them without a fresh manual review. The 11 focused migration/recovery checks passed on both Mac and ARM.
- **Validation:** The affected Mac and ARM64 suites and rendered startups passed. The tablet exposed all three connection choices and after-changes synchronization. End-to-end physical transfers required separate testing.

See [automatic synchronization](docs/en/SYNC_PROTOCOL.md#automatic-synchronization-in-029) and [settings](docs/en/SETTINGS.md).

## 0.2.8 development

- Add **Diagrams** as a fifth library section, including blank diagram notebooks that remain accessible before their first saved diagram. Opening an entry shows the source page without automatically opening an editor.
- Recognize supported hand-drawn flowchart shapes and arrows locally. A drawing pause can offer a nonmodal suggestion in a diagram notebook; explicit lasso recognition remains available. Review labels and the editable diagram before applying. Unrecognized strokes and rejected labels stay as ink; conversion supports one-step page undo and stored original-ink recovery when replacing strokes.
- Broaden recognition for overtraced oval seams and small tails, bounded contour gaps, slightly tilted two-stroke diamonds, gently curved connector shafts, V/two-wing heads, closed or retraced triangular heads, and a supported continuous shaft-to-triangle gesture. Ambiguous custom shapes and uncertain connectors remain ink.
- Use dedicated paired-device and pinned-notebook notifications for sync automation, avoiding unrelated status changes retriggering those bindings. The affected settings/BLE/sync suites passed 3/3 on both Mac and ARM.
- Reuse editable diagrams through **Insert from library**. Copies are independent; editing from an indexed entry changes its original object. Markdown with assets now includes a diagram PNG and editable `.drawio` companion; notebook transfers preserve editable geometry and recovery ink.
- Add optional automatic Paper Pro orientation from a supported sensor, with stability filtering and deferred changes during interaction. Manual orientation remains available; fixed reading orientation takes precedence. Automatic rotation is off by default.
- **Validation:** Mac and ARM64 startup, geometry and UI checks passed after the recognition corrections. Device checks confirmed automatic rotation and reviewing and editing a tested flowchart.

See [Diagrams](docs/en/DIAGRAMS.md) and [settings](docs/en/SETTINGS.md).

## 0.2.7 development

- Twenty additional settings cover usage profiles, toolbar order and handedness, clock time zone, contrast, gestures, per-book reading preferences, pressure response, palm rejection, lasso insertion, OCR corrections, shape tolerance, e-paper update profiles, trusted Wi-Fi and scheduled sync, conflict handling, backups, cache size and preferred Apple destinations. See [settings](docs/en/SETTINGS.md).
- Google Drive, Dropbox and OneDrive account dialogs use official OAuth sign-in. Browse folders, download supported documents, upload new copies and transfer editable `.quaderno` notebooks with an import preview. Google sign-in on Paper Pro uses the Mac browser through a temporary SSH loopback forward; subsequent cloud transfers run directly on the tablet over Wi-Fi.
- Cloud tokens are excluded from notebook exports and backups. macOS uses Keychain; Paper Pro uses private files. Locking or sleeping cancels pending authorization and transfers.
- Provider sign-in requires OAuth registrations for Quaderno. The tablet account dialogs were verified. Live sign-in and cloud transfers required separate testing.

The automated Mac checks passed. Follow-up UI regressions covered the empty account state and opening the dialogs from tablet settings.

## 0.2.6 development

- Searchable settings categories with quick controls, four manual display orientations, three interface sizes, optional status bar/clock/pen indicators, full-page entry and book gesture preferences. Display reset preserves notebooks, accounts, security and power preferences.
- Separate tablet and Marker battery information, with asynchronous firmware readings and stale/unknown percentage handling. Configurable Quaderno sleep and an option to stay awake while charging preserve the independent PIN policy and explicit Sleep now action.
- Accounts and services hub for Obsidian, Apple/iCloud, Google Drive, Dropbox, OneDrive and Quaderno wireless, showing direct Paper Pro functions separately from Mac browser/file workflows. No new cloud OAuth login or direct iCloud access is claimed.
- English and Italian [settings documentation](docs/en/SETTINGS.md), updated navigation paths, and preference/interaction regressions.

The Mac and ARM64 suites and package startup checks passed. Device checks covered Marker battery reporting and tapping tables and charts in landscape right. Automated coverage includes all four orientations. [Validation status](docs/en/SETTINGS.md#verification-boundary)

## 0.2.5 development

Direct chart editing from the page: a finger tap, mouse click, or brief pen/Marker tap opens the chart editor. Standalone charts open their properties; table-linked charts open their settings and source-column selection while retaining the table link. Moving or holding the pen continues writing. Lasso, eraser, highlighter, and shape tools keep their existing behavior, and Items remains available for moving and editing objects. Opening an item from the library still shows only its page.

Mac and ARM64 builds, tests and rendered startup checks passed. Finger and brief Marker taps opened the chart editor on Paper Pro. [Direct chart editing](docs/en/LASSO.md#edit-a-chart-with-a-tap)

## 0.2.4 development

Opening an existing item from **Tables** or **Charts** shows its source page. Tap a table cell to edit it, or enable **Items** and tap a chart. The Mac and ARM64 suites and rendered startup checks passed. Device checks confirmed that the library opens the item’s page. [Library navigation](docs/en/LASSO.md#open-tables-and-charts-from-the-library)

## 0.2.3 development

Fix direct editing of table cells after handwriting conversion. Tapping a cell opens the editor at its row and column and exposes the original formula for editing. Finger taps and mouse clicks also open empty cells; brief pen taps open occupied cells while empty-cell pen contacts keep producing ink. Moving or holding the pen continues writing, and Items mode opens the selected cell too.

Mac and ARM64 builds, tests and rendered startup checks passed. Device checks covered finger and Marker input, editing cells and recalculating formulas. [Cell editing and validation](docs/en/LASSO.md#edit-a-table-cell-after-conversion)

## 0.2.2 development

Handwriting into table cells: the lasso groups strokes by position, local recognition transcribes each cell separately, and a review dialog supports correcting values, excluding cells, and explicitly allowing replacement of existing data or formulas. Updates are atomic, with optional stroke removal and session-local Undo/Redo; unaffected cells and linked charts retain their behavior. Data remains compatible with format version 9. See [usage, limits and validation status](docs/en/LASSO.md#insert-handwriting-into-table-cells).

## 0.2.1 development

Markdown preview and formatting, protected drafts, on-screen editing commands, precise mixed-lasso controls, template previews, and hidden-layer recovery. New table aggregates and formula guidance, negative chart-axis calibration, faster page/library navigation, import reports with targeted retry, clearer wireless and Bluetooth controls, and searchable/filterable Obsidian and Apple handoff views. See [31 usability improvements and validation status](docs/en/USABILITY_0_2_1.md).

## 0.2.0 development

Reversible on-page handwriting conversion; mixed lasso selection, transforms and selection rules; Italian/English recognition; transactional page storage; asynchronous native-source reader; resumable wireless deltas; persistent Apple delivery states; coordinated application sleep; desktop and firmware regression suites. See [upgrade and migration notes](docs/en/UPGRADE_0_2.md).

## 0.1.0 preview

The preview source includes the following features.

### Library and writing

- Local library with nested folders, favorites, tags, search, sorting, and a trash folder with restore.
- Page overview with thumbnails and controls to duplicate, reorder, and transfer pages between notebooks.
- Page templates, writing tools, ink layers, undo and redo, text blocks, and tables.
- Tap a text block to edit it on the page. Import CSV or TSV into tables with editable cells, sum and average formulas, and chart links.
- Smart shapes with a hold gesture, a preview, and a way to restore the original stroke. The lasso moves, resizes, rotates, and copies strokes from the active layer, with a preview before applying changes.
- On-device sketch conversion into single-line or bar charts, with point editing and CSV export after calibration.
- Line and bar charts linked to table columns on the same page. Editing the cells refreshes the chart.
- Flowchart editor with resizable shapes, grid snapping, right-angle arrows, a starter template, and `.drawio` import and export. Diagrams are included in notebook transfers and PDF exports.
- Lasso selection and group movement, alignment and distribution, zoom, and panning in the diagram editor.

### Documents and platforms

- Import and annotate PDFs and DRM-free, reflowable EPUBs. Zoom, fit to width, save bookmarks, and search embedded text in newly imported PDFs. Export annotated pages as PDFs.
- Import editable Markdown files from disk, Finder, or a direct HTTPS URL and export notebook text as Markdown. Save new notes to an Obsidian vault on Mac and open or copy Obsidian Publish links.
- Connect Paper Pro directly to a remote Obsidian Sync vault over Wi-Fi. Open linked Markdown notes, send text edits explicitly, and preserve a separate cloud copy when edits conflict.
- Import the Paper Pro library and send individual notebooks back over USB-C, with a preview before changes in either direction and separate copies on conflict. Share PDFs through the macOS share menu.
- Transfer and sync selected notebooks over Bluetooth LE or local Wi-Fi between the two Quaderno apps, with code-based pairing, an encrypted connection, and separate copies on conflict.
- Continue from the same page on Mac and Paper Pro and see each notebook’s sync state and last successful transfer.
- Drag documents in from Finder and drag the current page or entire notebook out as a PDF.
- Offline Italian handwriting transcription on Paper Pro with the optional WritePad helper, and on macOS 27 or later with the handwriting helper. Replace the selected ink in place or save to a new page.
- English and Italian interface options, and independently selectable Italian and US English QWERTY layouts for the on-screen keyboard.
- Paper Pro settings for Wi-Fi, USB-C diagnostics, Bluetooth accessory pairing, front light control when available, and automatic display refresh. A manual full refresh is available in the editor. The on-screen keyboard hides when a Bluetooth or USB keyboard is detected.
- Scripts for a temporary USB-C trial and an optional AppLoad shortcut.

### Documentation

- English and Italian guides for installation, usage, backups, development, and release preparation.
- Instructions for contributing and reporting vulnerabilities.
