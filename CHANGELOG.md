# Changelog

**English** · [Italiano](CHANGELOG.it.md)

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
- **Validation:** All 20 macOS suites passed. The final checks for book notes, the PDF appendix, synchronization and diagram recognition passed on Mac and ARM. Both builds passed rendered desktop and tablet startup. Installed on Paper Pro over Wi-Fi after a verified on-device backup. The library export had the same SHA-256 before and after the update. Native 1620 × 2160 e-paper startup and the AppLoad session passed. A local replay of a real sample recovered four more shapes and four explicit arrows while leaving a line without an arrowhead in ink. Physical drawing and wireless document transfer require separate checks.

See [book notes](docs/en/BOOK_NOTES.md), [section workflows](docs/en/FEATURES.md), [synchronization](docs/en/SYNC_PROTOCOL.md) and [data format compatibility](docs/en/DATA.md#library-format-versions).

## 0.2.10 development

- Recognize handwritten additions in the context of an already converted flowchart. The preview contains the complete updated diagram, including new shapes or arrows between existing nodes; applying updates the same page item rather than creating a second fragment.
- Preserve existing node/edge identifiers, edited labels, connector routes and page geometry. Existing labels remain read-only during recognition; review OCR only for new parts and use the diagram editor to change previous labels. Preserve visual text and line size when the diagram bounds expand.
- Choose the target explicitly when several diagrams are possible, or create a separate diagram. Keep uncertain ink and prevent duplicate geometry from retained original strokes. A proposal without valid additions cannot be applied.
- Apply the item replacement and optional ink removal in one page Undo/Redo operation. Persistent recovery retains an editable baseline diagram and removed addition strokes; later extensions on the same layer share that baseline. Recovery restores the original diagram instead of removing it.
- Introduce portable format **10** when `styleScale` or `handwritingSource.baseDiagram` is required, and use metadata version 10 for new local saves. Update both apps before transferring affected notebooks; older-content portable exports still choose their minimum required format.
- **Validation:** Mac and ARM builds passed all seven affected suites: regression, lasso, diagram recognition, contextual recognition, incremental sync, UI and diagram integration (86.20 s on Mac, 81.91 s on ARM; two expected macOS-only skips on ARM). Strict rendered desktop/tablet and extracted-package startups passed. Installed on Paper Pro after a fresh backup and verified native e-paper/AppLoad startup at 1620 × 2160. All six notebooks retained their canonical hashes, folder contents and exported byte count. On November 18, 2026, the user confirmed on Paper Pro that adding a hand-drawn shape and arrow updates the complete diagram as one editable item while preserving previous labels. The live AppLoad session was also observed for 72.36 seconds without QML runtime errors. The installed Mac **0.2.0** app remains unchanged; a separate 0.2.10 package is ready. Update both apps before transferring format 10 notebooks.

See [extending diagrams](docs/en/DIAGRAMS.md#extend-an-already-converted-diagram) and [data format compatibility](docs/en/DATA.md#library-format-versions).

## 0.2.9 development

- Extend automatic notebook synchronization to local Wi-Fi and paired Bluetooth LE. Choose automatic selection with Wi-Fi preferred, Wi-Fi only, or Bluetooth only. Automatic work does not enable a disabled Bluetooth radio.
- Add synchronization after saved changes, with a quiet period after the last edit. Keep opening-based and interval modes, and catch up selected notebooks when automation is enabled or the peer reconnects. New installations remain manual by default.
- Restrict trusted SSID rules to Wi-Fi; the optional tablet-charging requirement applies to both connections. Keep immediate PIN, sleep and editor guards, explicit first-transfer review, conflict policy and manual review for an uncertain apply result.
- Improve recovery with increasing waits between connection attempts, bounded transfer retries, and a saved checkpoint for safely interrupted automatic work. Reuse matching received chunks through a fresh preview, without replaying an earlier approval.
- Treat legacy pending transfers without a saved phase/revision and interrupted manual applies as uncertain; automatic catch-up cannot resend them without a fresh manual review. The 11 focused migration/recovery checks passed on both Mac and ARM.
- **Validation:** Mac and ARM builds, six affected suites on each platform (preferences, BLE sync, incremental sync, wireless connection, UI and settings integration) and strict rendered desktop/tablet startups passed. ARM finished 6/6 suites in 136.73 seconds, with one expected macOS-only UI skip. Installed on Paper Pro after a fresh backup, with native e-paper/AppLoad startup at 1620 × 2160 verified. All six notebooks retained canonical hashes, folder contents and exported byte count. On November 18, 2026, the user confirmed that the after-changes mode and all three connection choices are visible in the Paper Pro Wi-Fi/Bluetooth settings. This confirms the controls, not end-to-end transfers; physical Wi-Fi/Bluetooth transfer checks remain pending. The installed Mac 0.2.0 app remains unchanged; a separate 0.2.9 Mac package is ready. Both apps must be updated, open and unlocked for the new workflow; this is local app synchronization, not an Internet cloud service or a background daemon.

See [automatic synchronization](docs/en/SYNC_PROTOCOL.md#automatic-synchronization-in-029) and [settings](docs/en/SETTINGS.md).

## 0.2.8 development

- Add **Diagrams** as a fifth library section, including blank diagram notebooks that remain accessible before their first saved diagram. Opening an entry shows the source page without automatically opening an editor.
- Recognize supported hand-drawn flowchart shapes and arrows locally. A drawing pause can offer a nonmodal suggestion in a diagram notebook; explicit lasso recognition remains available. Review labels and the editable diagram before applying. Unrecognized strokes and rejected labels stay as ink; conversion supports one-step page undo and stored original-ink recovery when replacing strokes.
- Broaden recognition for overtraced oval seams and small tails, bounded contour gaps, slightly tilted two-stroke diamonds, gently curved connector shafts, V/two-wing heads, closed or retraced triangular heads, and a supported continuous shaft-to-triangle gesture. Ambiguous custom shapes and uncertain connectors remain ink.
- Use dedicated paired-device and pinned-notebook notifications for sync automation, avoiding unrelated status changes retriggering those bindings. The affected settings/BLE/sync suites passed 3/3 on both Mac and ARM.
- Reuse editable diagrams through **Insert from library**. Copies are independent; editing from an indexed entry changes its original object. Markdown with assets now includes a diagram PNG and editable `.drawio` companion; notebook transfers preserve editable geometry and recovery ink.
- Add optional automatic Paper Pro orientation from a supported sensor, with stability filtering and deferred changes during interaction. Manual orientation remains available; fixed reading orientation takes precedence. Automatic rotation is off by default.
- **Validation:** the first 0.2.8 build passed 15 suites on Mac (129.56 seconds) and ARM (130.23 seconds), 24 geometry checks, 17 UI checks (38.82 seconds), strict startups and extracted-package checks. Its Paper Pro installation passed native startup at 1620 × 2160 without QML errors; five notebooks retained canonical hashes, folders and exported byte count. On November 18, 2026, the user confirmed automatic portrait-to-landscape rotation. Physical recognition initially returned no shapes for a real drawing, prompting the tolerance fixes above. The corrected Mac and ARM builds passed 34 geometry checks and 17 UI checks (2/2 suites: 37.43 seconds on Mac, 37.37 seconds on ARM), with strict desktop/tablet startup. The corrected build is installed on Paper Pro and passed strict native/AppLoad startup at 1620 × 2160 without QML errors. A fresh backup was made and all six current notebooks retained canonical hashes, folder contents and exported byte count. On November 18, 2026, the user confirmed that recognition, applying the result and opening the editor to modify it work on the tested flowchart on Paper Pro; earlier archives are preserved separately. The installed Mac 0.2.0 app remains unchanged. Live OAuth accounts and transfers still require maintainer registrations and validation.

See [Diagrams](docs/en/DIAGRAMS.md) and [settings](docs/en/SETTINGS.md).

## 0.2.7 development

- Twenty additional settings cover usage profiles, toolbar order and handedness, clock time zone, contrast, gestures, per-book reading preferences, pressure response, palm rejection, lasso insertion, OCR corrections, shape tolerance, e-paper update profiles, trusted Wi-Fi and scheduled sync, conflict handling, backups, cache size and preferred Apple destinations. See [settings](docs/en/SETTINGS.md).
- Google Drive, Dropbox and OneDrive account dialogs use official OAuth sign-in. Browse folders, download supported documents, upload new copies and transfer editable `.quaderno` notebooks with an import preview. Google sign-in on Paper Pro uses the Mac browser through a temporary SSH loopback forward; subsequent cloud transfers run directly on the tablet over Wi-Fi.
- Cloud tokens are excluded from notebook exports and backups. macOS uses Keychain; Paper Pro uses private files. Locking or sleeping cancels pending authorization and transfers.
- Maintainer-owned OAuth registrations are still required before real account sign-in. No developer client identifiers are borrowed or fabricated. The user has confirmed that all three cloud account windows open on Paper Pro; live provider sign-in and cloud transfers remain unverified. The installed Mac app is unchanged.

The initial Mac build passed all 12 automated CTest suites (34.47 seconds). Subsequent corrections normalize the cloud dialog’s Boolean bindings and attach it to the window overlay, so a zero-size Loader cannot hide it. Added regressions cover both an empty account state and the actual Main/settings/Loader path. Rebuilt Mac and Paper Pro packages passed the affected tests and hardened startup checks; the corrected native Paper Pro installation starts cleanly. On November 18, 2026, the user confirmed that the Google Drive, Dropbox and OneDrive windows all appear. This does not verify provider sign-in or transfers, which still need official OAuth registrations and real-account testing. Earlier archives and reports are retained as superseded evidence. The installed Mac 0.2.0 executable and version information remain unchanged.

## 0.2.6 development

- Searchable settings categories with quick controls, four manual display orientations, three interface sizes, optional status bar/clock/pen indicators, full-page entry and book gesture preferences. Display reset preserves notebooks, accounts, security and power preferences.
- Separate tablet and Marker battery information, with asynchronous firmware readings and stale/unknown percentage handling. Configurable Quaderno sleep and an option to stay awake while charging preserve the independent PIN policy and explicit Sleep now action.
- Accounts and services hub for Obsidian, Apple/iCloud, Google Drive, Dropbox, OneDrive and Quaderno wireless, showing direct Paper Pro functions separately from Mac browser/file workflows. No new cloud OAuth login or direct iCloud access is claimed.
- English and Italian [settings documentation](docs/en/SETTINGS.md), updated navigation paths, and preference/interaction regressions.

All eight automated suites passed on Mac and ARM, along with package startup checks; affected UI/startup checks were repeated successfully after the final correction. Installed and opened through AppLoad on Paper Pro, with full 1620 × 2160 rendering, a clean strict QML-error check, and all five notebooks, folders and notebook exports preserved. Native Marker telemetry was verified and rechecked after restart. The user confirmed **100% / Fully charged** in the pen display and correct finger/brief Marker taps on tables and charts in **landscape right (90°)**. Automated tests cover all four angles; the physical rotation confirmation covers 90° right. The clock follows the device's system time zone; Quaderno has no time-zone selector. The installed Mac 0.2.0 app remains unchanged. See the [validation status](docs/en/SETTINGS.md#verification-boundary).

## 0.2.5 development

Direct chart editing from the page: a finger tap, mouse click, or brief pen/Marker tap opens the chart editor. Standalone charts open their properties; table-linked charts open their settings and source-column selection while retaining the table link. Moving or holding the pen continues writing. Lasso, eraser, highlighter, and shape tools keep their existing behavior, and Items remains available for moving and editing objects. Opening an item from the library still shows only its page.

Builds and all six suites passed on Mac (85 behavioral cases plus 12 setup/cleanup checks) and ARM (82 plus 12, with three expected macOS-only skips), along with rendered desktop/tablet startups on both platforms. All eight new chart cases and the existing 0.2.4 navigation regressions pass. Installed on Paper Pro over USB on November 18, 2026, with native startup, the running AppLoad binary, backups, and notebook integrity verified. That day, the user confirmed that finger and brief pen taps open the tested existing chart’s editor on Paper Pro without enabling Items. The installed Mac 0.2.0 app remains unchanged. See [direct chart editing](docs/en/LASSO.md#edit-a-chart-with-a-tap).

## 0.2.4 development

Opening an existing item from **Tables** or **Charts** shows its source page without automatically opening an editor. Tap a table cell to edit it, or enable **Items** and tap a chart. Builds and all six suites passed on Mac (77 behavioral cases plus 12 setup/cleanup checks) and ARM (74 plus 12, with three expected macOS-only skips), along with rendered desktop/tablet startups on both platforms. Three new UI cases reproduce the original bug and pass with the fix. Installed on Paper Pro over USB on November 18, 2026, with native startup, the installed binary, backups, and notebook integrity verified. That day, the user confirmed that opening existing items from Tables and Charts shows the page without automatically opening an editor on Paper Pro. The installed Mac 0.2.0 app remains unchanged. See [library navigation](docs/en/LASSO.md#open-tables-and-charts-from-the-library).

## 0.2.3 development

Fix direct editing of table cells after handwriting conversion. Tapping a cell opens the editor at its row and column and exposes the original formula for editing. Finger taps and mouse clicks also open empty cells; brief pen taps open occupied cells while empty-cell pen contacts keep producing ink. Moving or holding the pen continues writing, and Items mode opens the selected cell too.

Mac and ARM SDK builds passed, along with six suites on each platform: 74 behavioral cases on Mac and 71 on ARM, plus 12 setup/cleanup checks per platform and three expected macOS-only skips on ARM. Rendered Mac startup in desktop and tablet layouts and both ARM SDK startups passed. Installed over USB on Paper Pro on November 18, 2026, with native startup, the running AppLoad binary, backups, and notebook integrity verified. That day, the user confirmed finger and pen taps, editing, and formula recalculation in the **QA Celle 0.2.2** physical test scenario. The installed Mac 0.2.0 app remains unchanged. See [cell editing and validation status](docs/en/LASSO.md#edit-a-table-cell-after-conversion).

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
