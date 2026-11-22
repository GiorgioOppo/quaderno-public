# Lasso, handwriting and editable selections

The lasso works on the active visible ink layer and on page objects: text, images, tables, charts and diagrams. It does not select strokes from hidden or inactive layers.

## Select and refine

1. Open a document and choose **Lasso**.
2. Choose **Contained elements** to include only whole strokes and objects, or **Touched elements** to include anything the outline crosses. Dots and accents are selectable, including a single pen point.
3. Draw a closed outline. The selection remains on the page with a frame and manipulation handles.
4. Use **Add** or **Subtract** to draw another outline and refine the selection.

The contained mode checks the whole path, including segments crossing an indentation in a concave outline. Dense outlines are simplified with a small geometric tolerance to keep selection responsive. An excessively complex outline is rejected with a message rather than approximated arbitrarily.

## Move, resize and rotate

Drag the selection frame to move it. Drag the lower handle to resize and the upper handle to rotate in 15-degree increments. **Apply** saves ink and selected objects together. **Cancel** discards the preview. A transformation that would move content outside the page is rejected without changing the document.

Text, tables and images keep their editable data. Table cells retain formulas and linked charts retain their source-table references. Ink and objects can be selected and moved together. Corrected rectangles and ellipses retaining original freehand ink accept quarter-turn rotations; other unsupported transformations are rejected on Apply.

Use **Actions → Delete selection** to remove the selected content together. Undo and redo apply to the entire mixed selection. A selection whose content has changed is rejected rather than applied to different strokes or objects.

## Convert handwriting in place

The recognition language is stored with the document. Choose **Italian** or **English** in the lasso controls or Actions. Available recognition depends on the installed local engine and dictionaries. Paper Pro uses the optional WritePad helper; macOS 27 and later use PencilKit with Vision as a fallback.

On Paper Pro, handwriting recognition starts after a selection containing pen, pencil or marker strokes. On either platform, **Transcribe** starts it manually. Highlights, geometric shapes and page objects are not converted into text.

The result appears over the original writing position. Recognition preserves line breaks. You can type directly into the preview, or choose **Correct** to open the text editor and its on-screen keyboard when a hardware keyboard is not connected. Engine-provided alternatives appear when available; Quaderno does not invent replacement suggestions. **Insert here** replaces only the recognized handwriting with editable text. The editor also offers **Save on a new page**.

Starting a transformation cancels pending recognition, so a late result cannot overwrite a movement preview. Navigation, locking and screen sleep clear the active selection.

## Insert handwriting into table cells

The 0.2.2 development version can update an **existing editable Quaderno table on the page**, including one imported from CSV. Write directly inside its cells with the pen, pencil or marker on the active visible layer. Page-template grids, hand-drawn tables, and tables contained in a PDF or image are not editable tables for this feature.

1. Write one or more values inside their respective cells, keeping the strokes within each cell's borders.
2. Draw a lasso around the strokes and choose **Insert into cells**. Apply or cancel any pending selection transformation first.
3. Check the destination table. Quaderno suggests the table with the most selected strokes assignable to cells; you can choose another in the dialog. Recognition runs separately for each cell, using the document's Italian or English language and the available local engine.
4. Review the cell addresses, such as `B2`, their previous values and the recognized text. Correct the text and choose which cells to include. If recognition fails, you can enter the value manually; each cell accepts up to 500 characters. The on-screen keyboard follows hardware-keyboard detection.
5. To change occupied cells, explicitly enable replacement of existing values, including formulas. Check the option to remove converted strokes, which starts enabled, then apply the changes. Cancelling the dialog leaves both table and ink unchanged.

Assignment uses stroke positions relative to the grid, including rotated tables. A stroke crossing a border or lying outside the table is not assigned to a guessed cell: the dialog reports how many strokes remain unassigned. Those strokes and strokes belonging to excluded cells stay on the page. Other layers, highlights, shapes and selected objects are not converted.

Applying updates only the included cells and optionally removes their associated strokes. Other cells retain their text and formulas, table geometry stays unchanged, and charts linked to the same table update. If the selection or table changes during review, the operation is rejected without partial changes; make a fresh selection.

**Undo and Redo handle the cell values and strokes together in the current session.** History resets when changing page or layer and when closing the app. To keep the handwriting beyond that point, disable stroke removal before applying: conversion into cells does not store a permanent recovery field or offer **Restore original handwriting**. Cell data remains compatible with format 9 and editable after notebook transfer.

Version 0.2.2 was built on Mac and with the Paper Pro ARM SDK. On 17 November 2026, all six suites passed on both platforms (70 behavioral cases on Mac and 67 on ARM, with three expected macOS-only skips). Four new model cases and three UI cases cover cell assignment, rotation, layers, stale selections, formula protection, persistence and transfer, linked charts, undo, serial OCR, cancellation, and keyboard visibility. OCR tests use controlled responses to verify the workflow, not recognition accuracy on real handwriting. Rendered startup and visual review of the Mac and tablet layouts also passed; UI suites and startup checks were repeated after the final corrections.

On 17 November 2026, version 0.2.2 was installed on the Paper Pro running firmware 3.29.1.0 over USB and registered with AppLoad. Local and tablet-side backups were created; the installed binary matches the package, and native startup reached `QUADERNO_READY` at 1620 × 2160. Adding the separate **QA Celle 0.2.2** notebook left all five existing notebooks unchanged. The installed Mac 0.2.0 app was kept unchanged at the user's request.

On November 18, 2026, the user confirmed recognition of `12` in A2 and `7` in A3, insertion into those cells, and results B2 = 24, B3 = 14, and C2 = 19 on Paper Pro. This feedback covers that specific test; Undo has not been confirmed in the physical test. A subsequent issue was confirmed: tapping cells directly on the page in writing mode did not open the editor. In 0.2.2, open the table through **Items**, then select a cell in the editor.

Local logs are `build-dev/test-table-ink-final.log`, `build-dev/test-table-ink-ui-final.log`, `build-arm/test-0.2.2-final.log`, `build-arm/test-0.2.2-ui-final.log`, `build-release/paper-pro-install-0.2.2.log`, and `build-release/paper-pro-native-startup-0.2.2.log`.

## Edit a table cell after conversion

The fix in development for **0.2.3** opens the editor at the cell tapped directly on the page, without first enabling Items. With a writing tool selected and the lasso closed:

- **Finger:** tap an empty or occupied cell to edit it.
- **Mouse on Mac:** click an empty or occupied cell to edit it.
- **Pen/Marker:** a brief tap on an occupied cell opens the editor. In empty cells, the pen continues writing, preserving dots too. Moving or holding the pen in an occupied cell lets you continue writing.
- **Items mode:** tap a cell to open the editor at its row and column. Dragging the table still moves it on the page.

In the editor, the grid selects a cell and the **Cell text** field edits its content. For a formula cell, the field shows the original formula, such as `=SUM(A2:A3)`, while the grid shows its result. Use a physical or on-screen keyboard, then choose **Save**: the value or formula remains editable in the table.

On November 18, 2026, 0.2.3 was built on Mac and with the Paper Pro ARM SDK. All six suites passed on both platforms: 74 behavioral cases and 12 setup/cleanup checks on Mac; 71 behavioral cases and 12 setup/cleanup checks on ARM, with three expected macOS-only skips. On Mac, the five non-UI suites passed in the full run and the UI suite passed in its final rerun; on ARM, all six passed in the final run. Rendered Mac startup in desktop and tablet layouts and both ARM SDK startups also passed.

On November 18, 2026, 0.2.3 was installed on Paper Pro over USB after fresh local and tablet-side backups. The native startup probe reached `QUADERNO_READY` at 1620 × 2160; restoration of the stock interface and the automatic-sleep setting after the probe also passed. Quaderno was then started through AppLoad, with the running binary’s checksum verified against the new package. The before/after comparison confirmed unchanged canonical content hashes for the five notebooks present, including **QA Celle 0.2.2**, and unchanged folder contents. The installed Mac 0.2.0 binary and property-list file remain unchanged.

On November 18, 2026, the user confirmed in 0.2.3 that finger and pen taps open the editor and that editing and formulas work correctly. The proposed test in **QA Celle 0.2.2** was to change A2 to `15`, keep A3 = 7, and check B2 = 30 and C2 = 22. This feedback covers that scenario on the connected Paper Pro; it does not establish physical validation of every input mode or other devices. Undo remains unconfirmed in the physical test.

## Open tables and charts from the library

The fix in development for **0.2.4** opens the source page when you choose an existing item in **Tables** or **Charts**, without automatically opening an editing dialog. You can view and annotate the page before choosing what to edit.

To edit a table, tap a cell using the gestures described above; direct cell editing introduced in 0.2.3 remains available. To edit a chart in 0.2.4, enable **Items** and tap the chart; 0.2.5 in development adds [direct chart taps](#edit-a-chart-with-a-tap). Commands to create new tables or charts still open their respective editors.

On November 18, 2026, 0.2.4 passed builds and all six complete suites on both platforms: 77 behavioral cases and 12 setup/cleanup checks on Mac in 21.55 seconds; 74 behavioral cases and 12 setup/cleanup checks with the ARM SDK in 20.18 seconds, with three expected macOS-only skips. Three new UI cases cover a table, a standalone chart, and a table-linked chart: clicking the actual library row, explicit editing afterward, programmatic opening, and unchanged data. All three failed on the previous behavior specifically because the editor opened automatically; all pass with the fix. Rendered startup in desktop and tablet layouts also passed on both platforms.

On November 18, 2026, 0.2.4 was installed on Paper Pro over USB after fresh local and tablet-side backups. The installed binary’s checksum matches the package. Canonical content hashes for all five notebooks present and folder contents remained unchanged; the export comparison also verified unchanged byte counts. The native startup probe reached `QUADERNO_READY` at 1620 × 2160, followed by restoration of the stock interface and the automatic-sleep setting. The installed Mac 0.2.0 binary and property-list file remain unchanged.

On November 18, 2026, the user confirmed in 0.2.4 that opening an existing item from **Tables** and then **Charts** shows the page without automatically opening the editor. This physical feedback covers navigation in those two sections on the connected Paper Pro.

## Edit a chart with a tap

Version **0.2.5** in development lets you open the editor by tapping the chart directly on the page, without first enabling Items. With the pen, pencil, or marker tool selected and the lasso closed, the intended behavior is:

- **Finger or mouse:** tap or click the chart to open its editor.
- **Pen/Marker:** a brief tap opens the editor; moving or holding the pen continues writing on the page.
- **Standalone chart:** the chart’s properties open, including its data and settings.
- **Table-linked chart:** the chart’s settings and selection of X and Y columns from its source table open. Saving retains the table link.

Lasso, eraser, highlighter, and shape tools do not trigger direct chart editing. **Items** remains available for editing or moving objects. Library navigation retains the 0.2.4 behavior: opening an entry in Charts shows the page; the editor opens after a separate explicit gesture on the chart.

On November 18, 2026, 0.2.5 passed builds and all six suites on both platforms: 85 behavioral cases and 12 setup/cleanup checks on Mac in 24.83 seconds; 82 behavioral cases and 12 setup/cleanup checks with the ARM SDK in 25.02 seconds, with three expected macOS-only skips. The UI suite summary reports 39 passing checks on Mac and 38 passing checks with one skip on ARM. All eight new chart cases passed: six input/chart-type combinations, annotation tools, and rotated hit testing with occlusion. The navigation regressions introduced in 0.2.4 remain green. Rendered startup in desktop and tablet layouts also passed on both platforms.

On November 18, 2026, 0.2.5 was installed on Paper Pro over USB after fresh local and tablet-side backups. The installed binary’s checksum matches the package; AppLoad was also verified to be running the new binary. Canonical content hashes for all five notebooks present and folder contents remained unchanged; the export comparison also verified unchanged byte counts. The native startup probe reached `QUADERNO_READY` at 1620 × 2160, followed by restoration of the stock interface and the automatic-sleep setting. The installed Mac 0.2.0 binary and property-list file remain unchanged.

On November 18, 2026, the user confirmed in 0.2.5 that a finger tap and a brief pen tap open the existing chart’s editor, with **Pen** selected, the lasso closed, and Items mode disabled. This physical feedback covers opening the chart tested on the connected Paper Pro.

## Recover the original ink later

Converted text stores its original strokes and the source-layer identifier inside the document. Recovery survives page changes, closing and reopening Quaderno, and transfer to another updated Quaderno installation.

Tap converted text and choose **Restore original handwriting**, or select that one text object with the lasso and use the same action in **Actions**. Select and show its original layer first. If that layer has been deleted, recovery uses the current visible layer. Recovery replaces the text object with its original ink and is itself undoable. Moving or rotating the text with the lasso also moves its stored source ink, so recovery follows the new placement.

Editing the recognized text preserves the stored ink. Deleting the text object deletes its attached source too, subject to the current session's undo history. This source is a recovery record, not an unlimited document version history.

## Format and validation

The extended document format stores `handwritingSource` on converted text, optional `rotation` and `textScale` on page objects, and `handwritingLanguage` on books. Older app versions may not understand this format. Use updated Quaderno on both devices to preserve editable recovery data.

Regression coverage includes tiny strokes and concave outlines, add/subtract selection, stale-selection rejection, OCR recovery after reopening and text editing, transfer of original ink and language, mixed transforms/deletion/undo, rotated recovery data, and actual QML selection, fullscreen, standby and reader gestures. See [development](DEVELOPMENT.md) for building and running tests. Physical e-paper behavior still requires testing on the target tablet.
