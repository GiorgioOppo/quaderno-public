# Quaderno 0.2.1: usability update

**English** · [Italiano](../USABILITA_0_2_1.md) · [README](../../README.md)

This development update adds the 31 improvements below to the Mac and Paper Pro interfaces. Builds, rendered startup and integrated suites have passed on the Mac and with the ARM firmware runtime. Version 0.2.1 has also been installed on a physical Paper Pro running firmware 3.29.1.0, with backups and a successful native rendered-startup check. Physical Marker, lasso, and gesture testing still awaits user feedback.

The [0.2 migration notes](UPGRADE_0_2.md) describe the earlier foundation: reversible handwriting conversion, mixed ink/object selection, Italian and English recognition, page-based storage, asynchronous document loading, resumable wireless transfers, persistent Apple delivery states, and coordinated sleep. Those capabilities already existed before this usability update.

## Writing and text entry

| No. | Improvement | How it helps |
| --- | --- | --- |
| 1 | Markdown writing and preview tabs | Switch from the Markdown source to its formatted preview without saving or leaving the editor. The preview is read-only. |
| 2 | Markdown formatting toolbar | Insert a heading, bold, italic, a bullet, or a task-list marker. Bold and italic wrap selected text; heading and list commands act on the current line. |
| 3 | Live text counts | See word and character counts while drafting a Markdown document. |
| 4 | Draft protection and Save shortcut | Cancelling, pressing Escape, or closing the application asks before discarding an edited Markdown draft. Use the platform Save shortcut, such as Cmd+S on Mac, to save explicitly. |
| 5 | On-screen cursor navigation and forward deletion | Move left or right, go to the start or end of the text, and delete the character after the cursor with `⌦`. Backspace remains available. Supplementary Unicode characters are kept intact when moving or deleting. |
| 6 | On-screen editing commands | Open **Edit** for Select all, Copy, Cut, Paste, Undo, and Redo. Commands follow the target field's selection, editability, and undo state; clipboard operations are disabled for masked password fields. |
| 7 | On-screen newline key | Use `↵` in multiline text editors. Single-line fields do not show this key. |

The keyboard continues to use the existing Italian and US English QWERTY layouts. Detection of a connected hardware keyboard still controls whether the on-screen keyboard appears.

## Lasso, templates, and layers

| No. | Improvement | How it helps |
| --- | --- | --- |
| 8 | Precise mixed-selection controls | Open **Precision** for directional movement, scale changes, and stepped rotation, with numerical feedback. Normal rotations use 15° steps; smart shapes with retained original strokes use 90° steps. |
| 9 | Reset and selection keyboard commands | **Reset** removes a pending transformation while retaining the selection. Arrow keys move by one logical page point, Shift+arrows by ten; Enter applies and Escape closes. These local commands do not take over arrows or Enter while typing into the transcription editor. |
| 10 | Selection summary and recognition cancellation | See the number of selected strokes and objects. **Stop recognition** cancels a running handwriting request while leaving the selection available. |
| 11 | Template preview before applying | Choose a template to inspect its larger preview, then select **Apply template**. Cancel leaves the current page template unchanged. Existing page content keeps its position. |
| 12 | Hidden active-layer recovery | The Layers dialog explains when the active layer is hidden and offers **Show active layer**, so writing can resume without guessing why the pen is inactive. |

Dragging, scaling, and rotating remain previews until **Apply**. Resetting a preview does not add a saved edit; an applied transformation can be undone. In-place OCR replacement and restoration of the original handwriting are existing 0.2 functions, described in the [lasso guide](LASSO.md).

## Tables and charts

| No. | Improvement | How it helps |
| --- | --- | --- |
| 13 | MIN, MAX, and numeric count | In addition to SUM/SOMMA and AVERAGE/MEDIA, use `MIN`, `MAX`, and `COUNT`/`CONTA.NUMERI`, including cell ranges. Quick formula buttons insert them in an eligible empty cell. |
| 14 | Spreadsheet-style coordinates | Row numbers, column letters, and active row/column highlighting make references such as `B3` easier to locate while editing. |
| 15 | Formula help and understandable errors | Open the formula help for supported syntax. The selected cell explains invalid references, circular dependencies, averages with no numbers, unknown functions, and malformed arguments. |
| 16 | Confirmation before shrinking a filled table | Reducing rows or columns asks for confirmation when the cells being removed contain data. The table draft is retained while the decision is pending. |
| 17 | Editable negative chart-axis bounds | Manually calibrated charts expose minimum and maximum values for both axes. The minimum may be negative or zero; the maximum must be positive. Negative values can therefore be represented explicitly during calibration and CSV export. |

For example, with `A1 = 10`, `A2 = -5`, and `A3 = 25`:

| Formula | Result |
| --- | --- |
| `=MIN(A1:A3)` | `-5` |
| `=MAX(A1:A3)` | `25` |
| `=COUNT(A1:A3)` or `=CONTA.NUMERI(A1:A3)` | `3` |

Text and empty cells in ranges are ignored; formula errors propagate. Use a decimal point in formula examples to avoid ambiguous separators. This remains a small table editor with up to 12 rows and 8 columns, rather than a full spreadsheet formula language. Notebook transfers retain editable formula strings. A chart linked to a table on the same page uses calculated cell values and updates when those cells change; this relationship predates 0.2.1.

## Reading and finding documents

| No. | Improvement | How it helps |
| --- | --- | --- |
| 18 | Direct page-number navigation | The page overview offers a number field plus First and Last commands. On Paper Pro, a numeric keypad is available when no hardware keyboard is connected. The chosen page opens only when confirmed with **Open**. |
| 19 | Bookmark and annotation filters | Filter the overview to All, Bookmarks, or Annotated, with counts. Annotation summaries include hidden ink layers and do not require decoding page backgrounds just to filter. |
| 20 | Double-tap reading zoom | With reading gestures enabled, a touch double-tap zooms to 2× around the touched point; another returns to the whole-page view. Marker input is not treated as this touch gesture. |
| 21 | More library sorting and format filters | Sort by original order, title A–Z/Z–A, or page count ascending/descending where applicable. Filter books by PDF, EPUB, or legacy/unknown format, and writing by Markdown or notebooks. |

Format filters use the stored document source. Older imports containing only page backgrounds can appear as legacy/unknown; this update does not reconstruct or silently reimport their original EPUB or PDF files.

## Importing on Mac

| No. | Improvement | How it helps |
| --- | --- | --- |
| 22 | Finder-drop preflight | Before importing, review the target folder and counts of supported files, duplicate paths within the batch, unsupported files, and unreadable files. Hover inspection reads local file metadata without parsing every document. |
| 23 | Per-file import report and targeted retry | See each file's result and retry failed files without importing successful ones again. Cancelling stops the current/queued batch while retaining already imported documents. |

The existing limit is 16 files per import batch. The report describes the last batch, is cleared on lock or explicit dismissal, and is not a persistent import history. Duplicate detection compares paths within that batch; it does not compare every book already in the library.

## Connections, Obsidian, and Apple handoff

| No. | Improvement | How it helps |
| --- | --- | --- |
| 24 | Remembered Wi-Fi address | The wireless dialog can prefill the last authenticated Wi-Fi IPv4 address, including after using Bluetooth. The field remains editable and Enter connects when the address is valid. |
| 25 | Clear connection state | Distinguish Not paired, Paired but disconnected, Connecting, and an authenticated Wi-Fi or Bluetooth connection. A saved pairing no longer looks like proof of a live connection. |
| 26 | Bluetooth accessory filters | On Paper Pro, filter the device finder to All, Keyboards, or Paired, with displayed/total counts and instructions when no matching devices are found. An unclassified keyboard may appear only under All. |
| 27 | Search and filter Obsidian notes | Search titles and paths without case sensitivity; filter All, Conflicts, or Changed notes with counts. Filtering does not start a cloud transfer. |
| 28 | Readable Apple delivery details | Queue rows show the destination name, exported file name/format, and creation date. |
| 29 | Apple queue filters | Switch between All, Pending, and Finished deliveries to find work still awaiting action. Completed and cancelled entries are included in Finished. |
| 30 | Reveal retained exports in Finder | On Mac, **Show in Finder** locates an existing export from its delivery row, including completed or cancelled deliveries. Archived entries no longer have a row, although their files remain on disk. |
| 31 | Transfer dialogs that fit smaller windows | USB import/send review can scroll in smaller windows, shows an activity indicator while busy, and supports keyboard navigation. The Apple sharing dialog also bounds its height and scrolls. |

Wi-Fi continues to use the existing local IPv4 connection policy and requires the tablet side to be started on the same network. Remembering an address does not start a connection automatically.

Apple handoff still goes through Quaderno on Mac: sync a notebook, choose the destination, and complete the import in the Apple application. A delivery is confirmed by the user. This update adds neither Apple Account sign-in on Paper Pro nor automatic cloud access to Notes or Journal. [Apple integration and supported workflows](USER_GUIDE.md#notes-books-and-icloud-on-mac)

## Verification status

- On 17 November 2026, **6/6 suites passed on macOS** (63 behavioral cases plus 12 setup/cleanup cases, 17.31 s) and **6/6 suites passed on ARM** (60 behavioral cases plus 12 setup/cleanup cases, 14.97 s). ARM skips three macOS-only cases as expected: Apple delivery confirmation, Finder batch imports and revealing exported files in Finder.
- Tests cover selection editing, text focus, keyboard commands, template cancellation/application, hidden-layer recovery, table formulas, and the other affected production interfaces. Rendered startup passed in desktop and tablet layouts, and the main panels were visually reviewed. ARM used ferrari 6.0.112 / Qt 6.10.3 SDK libraries in a VM.
- Version 0.2.1 was installed on the physical Paper Pro running firmware **3.29.1.0** over SSH via USB, with local and tablet-side backups. The installed binary's SHA checksum matches the package. The contents of all four original books were verified unchanged before and after adding a QA notebook.
- Native rendered startup passed with `QUADERNO_READY` and a **1620 × 2160** display geometry. Return to the stock reMarkable interface and restoration of automatic sleep were verified. After these checks, Quaderno was launched through AppLoad and observed running.
- On 17 November 2026, the user confirmed that the pen-driven QA notebook test passed: lasso selection, movement, Apply, and Undo. This feedback does not verify handwriting recognition or reading gestures. No real Bluetooth accessory trial, Wi-Fi transfer, or Apple application delivery has been performed for this update.
- The Mac 0.2.1 package has been verified and is ready. Local installation is deferred at the user's request to leave version 0.2.0 running.
- Local logs are `build-dev/test-usability-final.log`, `build-dev/startup-usability-final.log`, `build-arm/test-0.2.1-final.log`, `build-release/paper-pro-install-0.2.1.log`, and `build-release/paper-pro-native-startup-0.2.1.log`.
- Results previously reported for 0.2 apply to that earlier snapshot. See [0.2 validation notes](VALIDATION_0_2.md) for their scope.

## Follow-up work

The following items are proposals, not implemented features of this update:

1. **EPUB chapter navigation and internal links:** a table of contents with direct chapter/anchor navigation and return history.
2. **Heavy lasso geometry performance:** benchmark selections containing many points, cache polygon-edge bounds, and reduce repeated intersection work while preserving precise contained/touched semantics.
3. **Linked objects across documents:** introduce stable cross-document references and explicit conflict handling. Current live table-to-chart resolution is local to the same page; inserting reusable library content elsewhere creates a copy.

4. **Protect diagram drafts:** ask before discarding unsaved changes on Escape or Close, as in the Markdown editor.
5. **Avoid connector obstacles:** route orthogonal diagram edges around unrelated shapes.
6. **Preserve Markdown source URLs:** retain a web document’s original URL to resolve relative links and images.
