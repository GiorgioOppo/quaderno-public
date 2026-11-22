# Notes attached to a book page

**English** · [Italiano](../NOTE_LIBRI.md)

Quaderno 0.2.11 development adds numbered note markers to a specific point on a page. A note can contain keyboard text, handwriting, or both. The note belongs to that saved Quaderno page; its position follows zoom and rotation without being reattached to a different paragraph.

## Add, read and edit

In **Books**, choose **Post-it** (sticky note), then tap the point on the page where you want its marker. Write in the note's text field, draw in its handwriting area, or combine the two. Choose a note color and save. Cancelling a new draft leaves the page unchanged.

Tap the numbered marker to reopen its content. Editing the content keeps the marker in place. Use the page's **Items** mode to move it, or the note editor to delete it. Page Undo/Redo covers creating, editing, moving and deleting notes during the current editing session. A hidden active ink layer must be shown before modifying notes.

Markers are numbered in page order among that page's notes. Their numbers can change when a note is removed; the persistent note identifier does not change. Note text appears in page search, and pages containing notes appear among annotated pages. The handwriting stays editable as original strokes; it is not silently converted to keyboard text.

The editor uses a fixed 3:2 handwriting area. It supports up to 4,000 text characters and 2,000 strokes / 60,000 points per note, within the existing 120-item page limit. Changing pages while a draft is open, or editing a stale version of the same note, cannot silently overwrite another page or newer note content.

## Transfer between Paper Pro and Mac

Send the **Quaderno notebook**, not only its original EPUB/PDF, to preserve highlights, underlines, ink layers and note text/handwriting. The original book file alone does not contain Quaderno annotations. Independently importing the same source file creates a separate book; synchronize the same Quaderno library item on the two devices.

Notes use portable library format **11**. Both receiving and sending apps should be updated to 0.2.11 or later before transferring a notebook containing notes. Content that does not need the new format still exports using its minimum required version. The storage reader and wireless compatibility handshake share the supported-version ceiling. See [data compatibility](DATA.md) and [wireless synchronization](SYNC_PROTOCOL.md).

## PDF and Markdown exports

PDF export keeps the original selected book pages first, with numbered note markers. It then appends the notes in source-page order. Typed content is paginated without cutting off long notes; handwriting gets a separate page with its original 3:2 proportions. Headers identify the original page and note number. Exporting only the current page includes only that page's note appendices.

Plain Markdown includes typed note content under a heading with its source page and note number. **Markdown with assets** also includes a PNG of each note's handwriting. These exports are readable copies; use the Quaderno notebook format to transfer editable note strokes and markers.

## Verification status

Note checks passed on Mac and ARM, including reopening, stale-draft protection, undo and redo, full PDF appendices and Markdown assets. Integration tests exercised creation and reopening with mouse, touch and tablet events, plus note editing in portrait and landscape. Synthetic book transfers preserved highlights, underlines, typed text and editable handwriting. The physical pen and radio paths require separate checks.
