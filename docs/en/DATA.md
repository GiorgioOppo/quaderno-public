# Data, backups, and privacy

**English** · [Italiano](../DATI.md)

## Where Quaderno stores files

| Platform | Default data folder |
| --- | --- |
| macOS | `~/Library/Application Support/Quaderno/` |
| Paper Pro | `/home/root/.local/share/Quaderno/` |

`quaderni.json` is the atomic local library manifest. Its pages, ink, images and retained document sources live in `quaderni.json.objects/`. `quaderni.json.previous` keeps the previous committed manifest; an initial migration also retains `quaderni.json.legacy`. The reader uses `document-sources/` as a regenerable source cache. Apple exports and their queue live in `apple-deliveries/`. `accesso.json` holds the PIN configuration when a PIN is set. Quaderno uses the app data location returned by Qt. The built-in USB import reads the default Paper Pro path shown above.

Quaderno saves successful changes automatically. Back up your library before upgrading. Earlier versions may reject a library with features introduced in a newer build, such as charts or the original strokes stored for smart shapes.

## Back up a library

1. Close Quaderno on the device you want to back up.
2. On Mac, open the data folder in Finder with **Go → Go to Folder** and copy the entire `Quaderno` data folder to a backup folder.
3. On Paper Pro in Developer Mode, copy the entire data folder over USB‑C with OpenSSH. Run this command from your backup folder and choose an unused filename.

```sh
scp -O -r root@10.11.99.1:/home/root/.local/share/Quaderno ./quaderno-paper-pro-backup
```

A manifest by itself is not a content backup. Keep the manifest, object directory and previous manifest together. The full data-folder backup also includes the PIN configuration and pairing state. Back up documents managed by the reMarkable interface separately.

## Restore or import

- To **restore the entire library**, close Quaderno and keep a copy of the current data folder. Restore the complete backed-up folder, then reopen Quaderno. A legacy self-contained JSON can still be restored as `quaderni.json`; it migrates on the next save.
- To **add notebooks to an existing library** on Mac, choose **Importa quaderni** (Import notebooks) and open the JSON file, or choose **Importa dal Paper Pro** (Import from Paper Pro) to read it over USB‑C. Quaderno recognizes unchanged notebooks. When versions diverge and both need to be kept, it adds a copy.
- Library imports accept files up to **64 MiB**. For a larger library, keep a full copy of the data folder. Local storage is bounded at 256 MiB of expanded library content. Restoring it replaces the library Quaderno opens.

To use your content in other apps, export pages as PDF, text as Markdown with any accompanying assets, table cells or calibrated chart data as CSV, and diagrams as `.drawio`. [File and transfer formats](FEATURES.md#choosing-a-file-or-transfer-method)

## Library format versions

Portable JSON has a `version` chosen from its contents. Local storage uses `format: "quaderno-pages-v1"`, with checksum-protected metadata and SHA-256 page/asset references; Quaderno reconstructs portable JSON for USB and wireless transfer. Do not edit the manifest or object files manually.

| Version | Added content |
| --- | --- |
| `1` | Notebooks and pages with ink strokes. |
| `2` | Page templates, text blocks, and tables. |
| `3` | Folders, favorites, tags, trash, layers, writing tools, and imported page backgrounds. |
| `4` | Charts and smart shapes with recoverable original strokes. |
| `5` | Page bookmarks, searchable PDF text, transcriptions on their original pages, and charts linked to tables. |
| `6` | Diagrams with shapes, labels, and attached arrows. |
| `7` | Diagrams with right-angle arrows. |
| `8` | PNG and JPEG images embedded as editable page items. |
| `9` | Retained PDF/EPUB sources, original handwriting for OCR recovery, item rotation/scaling and per-notebook recognition language. |
| `10` | Visual scale for extended diagrams (`styleScale`) and a recoverable baseline diagram for handwritten additions (`handwritingSource.baseDiagram`). |
| `11` | Anchored page notes (`type: "note"`) with editable text, handwriting in a separate note canvas and a marker color. |

The **0.2.11 development sources** read formats 1 through 11. A page note requires portable format 11. An item containing `styleScale` or `handwritingSource.baseDiagram` requires at least format 10; documents without newer content keep the minimum format required by their contents. New local saves instead write version 11 metadata in the `quaderno-pages-v1` manifest. The local format name and object layout do not change. The reader and wireless compatibility check share the same maximum supported version. [Library format implementation](../../src/notebook.cpp) · [Shared version limit](../../src/notebookformat.h)

Update both apps to **0.2.11 or later** before transferring notebooks with page notes through files, USB, Wi-Fi, Bluetooth or cloud services. Format 10 content requires at least 0.2.10. Do not lower a document's version number to force an import: older readers cannot preserve its newer content. Wireless synchronization now checks the receiver's format capability before sending content chunks; incremental peers without an explicit capability are treated conservatively as supporting format 9. [Synchronization protocol](SYNC_PROTOCOL.md)

The installed Mac app is **0.2.0**. Paper Pro runs **0.2.11**, deployed after an on-device backup with an identical library export before and after the update. Keep a complete backup from before upgrading if you need to return to an earlier app: a local manifest saved by 0.2.11 uses metadata version 11 even when an individual portable export can use an older version.

The optional `pageIds` and `lastViewedPageId` fields identify pages and store the last local position for each notebook. Older libraries get page identifiers on load. The reading position stays on the device, and navigation does not change the content revision.

## Page notes and annotated books

A Post-it is a page item with `type: "note"` and a stable `id`. Its `x`, `y`, `width` and `height` locate the marker in normalized page coordinates. `text` contains editable plain text; `noteColor` is `yellow`, `blue`, `green` or `pink`. `noteInk` stores validated strokes with normalized `x`, `y` and pressure `p` inside an independent 600 × 400 note canvas. Moving the marker changes its page position without rescaling the stored handwriting. Note numbers are derived from note order on each page; they are not permanent identifiers.

Each note accepts up to 4,000 text characters, 2,000 strokes and 60,000 points, within the existing limit of 120 items per page. Saving validates the active book, page and original item; a draft from a different page or an out-of-date note cannot overwrite current content. Creating, editing, moving or deleting a note participates in page Undo/Redo. Typed note text is searchable, and notes mark their pages as annotated. [Book notes](BOOK_NOTES.md)

Highlighting and underlining already use the notebook's regular page strokes. A native Quaderno book transfer carries its retained PDF/EPUB source, all pages, visible and hidden ink layers, page items and editable note text/handwriting. Copying only the original source file carries none of these added annotations. Importing the same PDF or EPUB independently on each device creates separate books; use whole-book synchronization to preserve the shared notebook identity.

Both apps must be open, unlocked and connected, with the intended notebook selected for automation and its first transfer reviewed. Opening-based automation runs on its configured triggers; use **After changes** to send saved annotations while the book remains open. A missing annotation therefore calls for checking the connection, trigger and matching Quaderno book, as well as version compatibility. These requirements apply to existing highlights as well as new notes. [Automatic synchronization](SYNC_PROTOCOL.md#automatic-synchronization-in-029)

PDF export keeps numbered markers on the selected source pages, followed by note appendices referenced to the original page and note number. Long typed notes are paginated; handwritten content gets a separate page. Markdown export includes the note text, and export with assets adds a PNG for its handwriting. These exports are readable copies; native Quaderno export or synchronization retains the editable note and individual strokes.

## Diagram workspaces and recoverable ink

Version 0.2.8 adds optional notebook metadata `workspace: "diagrams"`. An absent field keeps the earlier behavior. It identifies a dedicated diagram notebook even when its page is empty or only contains handwriting; the library generates an empty-workspace entry without adding fake diagram nodes to the document. Saved diagrams remain ordinary page items and can appear in the Diagrams index regardless of their source section.

The metadata travels in portable notebooks and local manifests. Updating an existing local diagram notebook from an older peer preserves its existing classification when the incoming copy omits the field. An older app can omit the classification on its first import/export, so update both devices to retain the dedicated workspace consistently. Page content and ink do not depend on this classification.

When recognition creates a diagram and replaces strokes, it stores the removed originals in its bounded `handwritingSource`, which alone requires portable format 9. Only recognized geometry and accepted nonblank label groups are removed; other ink remains on the page. **Restore original handwriting** can recover the stored source after reopening. This is distinct from current-page session Undo/Redo.

Version 0.2.10 can recognize new shapes or arrows alone using an existing diagram as context. Applying replaces the same item, retaining its ID and existing parts. The preview contains the complete diagram; multiple possible targets require an explicit choice unless the selection identifies exactly one. Creating a separate diagram remains available. Uncertain strokes or strokes matching existing parts are not consumed as new additions.

When an extension removes ink, `handwritingSource` stores `layerId`, the removed strokes and one validated copy of the previous diagram in `baseDiagram`. Subsequent extensions on the same layer accumulate strokes while retaining the same baseline, without building recursive diagram history. Recovery restores the editable baseline with the current item's ID and returns the additions as ink; it does not remove the preexisting diagram. When wider bounds require it, `styleScale` preserves the visual size of text and lines. Either of these new fields requires portable format 10.

With **Replace converted strokes with shapes and text** turned off, all original ink stays on the page and no new duplicate recovery source is attached; earlier recovery data is retained. Copying a diagram into another notebook creates independent geometry and labels without the original page's recovery source. Undo/Redo restores the item and strokes from a single application together in the current session; persistent recovery instead returns to the stored extension baseline.

Markdown export with assets writes each diagram as a PNG preview plus an editable `.drawio` companion and relative links. It does not embed editable Quaderno objects in Markdown syntax. Use a `.quaderno` export or notebook synchronization for editable page content and stored recovery ink. PDF export preserves appearance, not object editing. See [Diagrams](DIAGRAMS.md).

Automatic-orientation and automatic diagram-suggestion preferences remain device-local settings; they do not travel with these objects. Per-book reading preferences also remain separate from synchronized content. See [library maintenance](LIBRARY_MAINTENANCE.md) for local backup and cache behavior.

## Privacy and access

- The library is stored on your device. When you share or save a PDF, the destination receives a copy. Apple apps use their own iCloud settings.
- Mac transcription uses PencilKit and Vision. Quaderno passes the selected strokes to a local component. [Apple documentation](https://developer.apple.com/documentation/pencilkit/recognizing-handwriting-and-converting-to-text)
- The PIN protects access through the app interface. Quaderno stores a randomly salted PBKDF2 hash in the configuration file and limits attempts. Anyone with access to the data folder can read the manifest and its page/asset objects. Protection at rest depends on the operating system and where you store your backups.
- Quaderno prompts for the SSH password when authentication requires it. For imports from Mac, OpenSSH verifies the tablet against the host key in `known_hosts`. Quaderno sends Wi‑Fi requests to the system service on Paper Pro.

Use a sample notebook when reporting a security issue or attaching a reproduction to a bug report. [Security policy](../../SECURITY.md)
