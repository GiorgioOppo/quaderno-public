# Quaderno 0.2 development notes

This update develops the ten improvements reviewed for the Paper Pro and Mac apps. Both peers need the updated build to exchange documents using the new recovery, rotation, language, or native reader metadata. Old portable notebooks remain readable.

## Lasso and handwriting

1. **Recover original handwriting.** Converting a selection stores its original pen strokes and layer in the resulting text block. The restore action remains available after changing pages, reopening Quaderno, and transferring the notebook. Editing the recognized text keeps this recovery data. Undo and redo are also available during the current editing session.
2. **Work on the page.** The selection appears on the sheet with movement, resize and rotation controls. It can contain ink, text, images, tables and charts. Transforms and deletion operate as a single reversible edit. A selection is checked against its original page and content before it can change the notebook.
3. **Choose the selection rule.** Contained mode selects complete strokes and objects inside the contour. Touched mode includes intersected elements. Add and subtract refine an existing selection without redrawing the entire contour.
4. **Review recognition in place.** Choose Italian or English for each notebook. The recognized text retains line breaks and can be corrected before replacement. Candidates appear when the recognition engine supplies them. On Paper Pro, the optional WritePad build must include both dictionaries. Recognition remains an explicit replacement with retained source ink.

See [Lasso](LASSO.md) for the interaction details.

## Documents, storage and transfer

5. **Save individual pages and assets.** Local storage uses an atomic manifest selecting immutable page and asset objects. Unchanged page content and source files are reused. The previous committed generation is retained for recovery, and migration keeps the original JSON. A concurrent instance cannot silently overwrite a more recent save.
6. **Render the requested page.** PDF/EPUB imports run in a background worker with progress and cancellation. PDFs retain their original data. EPUBs retain the original archive and a vector PDF layout, so annotations refer to identical pages on both platforms. The reader rasterizes the requested page at the zoom resolution, uses a bounded cache and discards obsolete results. Two fingers zoom; one finger pans a zoomed page or changes pages at the fitted size. Marker input remains dedicated to ink and controls.
7. **Resume wireless transfers.** Updated peers exchange manifests and only missing page/asset objects. Chunks are saved before acknowledgement and incomplete transfers resume from their saved offsets. A reconnected transfer requests a fresh preview and confirmation. Existing conflict copies and stale-preview checks still protect local edits. See [Wireless protocol](SYNC_PROTOCOL.md).

Keep the **whole application data directory** when backing up local storage, including the object directory and previous manifest. A single local manifest is not a portable notebook. The USB export command and wireless transfer reconstruct portable JSON with its assets. The legacy import reader remains available. A library-wide USB export is limited to 64 MiB; use individual notebook transfers for larger libraries. Native source imports retain their original files, so transfer size still depends on the document, even though only the displayed page is rasterized.

## Apple, standby and verification

8. **Track Apple exports honestly.** Notes receives a prepared Markdown file and assets; Books receives PDF; Numbers receives the current page's first table as CSV values; Keynote receives a prepared PDF; Journal uses the macOS sharing picker. Prepared files and the delivery queue are persistent. Opening an app or a picker leaves the entry awaiting confirmation. Finish the import in the destination app and choose **I imported the file**, or reopen/cancel the entry. Cancelling preserves the exported files. Numbers CSV is an interchange of values; Quaderno-to-Quaderno transfer retains editable formulas.
9. **Coordinate pen input and idle sleep.** Native Marker activity resets the inactivity timer. Sleep is deferred during drawing, imports, recognition and modal editing. The application sleep screen blocks ink even without a configured PIN. Waking returns to the same document and still requires the PIN when one is configured. AppLoad uses a persistent session; exiting restores the stock interface and its autosleep setting. The app sleep screen does not suspend the kernel.
10. **Run regressions rather than an idle timeout.** The suites cover selection geometry, recovery after reopening, mixed transformations, formula preservation, stale approval, storage recovery, source-backed rendering, interrupted transfer and corrupt data. The startup check requires a rendered frame followed by a responsive event loop. CI runs desktop tests on macOS and Linux ARM and provides a manually triggered build with the matching Paper Pro SDK.

Run `ctest --test-dir build --output-on-failure` in a desktop build and `python3 scripts/check-startup.py /path/to/quaderno`. Build the tablet with `-DBUILD_TESTING=OFF` when its SDK lacks Qt Test. A desktop tablet-layout check does not establish physical screen, Marker or Bluetooth behavior; record those separately for each installed build.

See the [0.2 validation record](VALIDATION_0_2.md) for completed checks, measured Mac timings, and pending physical-device testing.
