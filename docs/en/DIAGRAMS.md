# Diagrams

**English** · [Italiano](../DIAGRAMMI.md)

Quaderno’s **Diagrams** section collects editable flowcharts alongside Books, Writing, Charts and Tables. A diagram contains shapes, their labels and directed connections. It can share a notebook page with handwriting, images, text, charts and tables.

This guide includes extending existing diagrams in **0.2.10 development**. Mac and ARM builds passed all seven affected automated suites and strict rendered desktop/tablet startup checks. The added cases cover full-diagram review, target choice, preserved labels and positions, retained ink, undo and recovery after reopening. On November 18, 2026, the user confirmed the extension on Paper Pro: the preview and applied result contain the complete updated diagram as one editable item, with previous labels preserved. The installed Mac app remains at 0.2.0.

Earlier verification covers the 0.2.8 recognizer: its first installation passed automated/native checks, and the user confirmed automatic portrait-to-landscape rotation. A real freehand drawing exposed an overly strict recognition case; the revised tolerances below passed Mac and ARM geometry/UI and strict startup checks. The corrected build was installed on Paper Pro and passed strict native/AppLoad startup at 1620 × 2160 without QML errors. On November 18, 2026, the user confirmed that recognition, applying the result and opening the editor to modify it work on the tested flowchart on Paper Pro. The subsequent 0.2.9 synchronization update has already been verified and installed; these confirmations do not verify the 0.2.10 extension. See the [verification status](SETTINGS.md#verification-boundary) and [changelog](../../CHANGELOG.md).

## Create and edit

Open **Diagrams** to create a diagram notebook or find diagrams already present in other notebooks. An empty diagram workspace remains accessible before its first recognized shape. Opening a library entry shows its page; use the editing action to open its diagram editor.

You can draw on the page using the pen, or construct a diagram in the existing editor. The editor provides process boxes, start/end shapes, decisions, data shapes and notes. It supports editable labels, directed connectors, straight or orthogonal routing, moving and resizing nodes, selection and lasso selection, duplication, grid snapping, alignment, distribution, zoom, and undo/redo for its draft.

Saving the editor updates the diagram object. Page-level undo and the editor’s draft history are separate mechanisms.

## Recognize a hand-drawn diagram

1. Draw closed shapes with enough space between them, then connect them with lines and visible arrowheads. Write labels inside the shapes.
2. In a diagram notebook, automatic analysis after a drawing pause can offer a nonmodal recognition suggestion for the current page. It does not open a dialog or alter the page by itself. **Close** hides the suggestion until the ink changes or you change page/layer context; turning the automatic-recognition preference off and on also allows a new suggestion. You can always select the drawing with the lasso and request recognition explicitly.
3. Open the suggestion or lasso action to review the proposed diagram. Recognized shapes and connections appear in the preview. The original page remains unchanged during this review.
4. Review each shape or connection label. The available local handwriting engine processes assigned label groups sequentially. Correct text using the on-screen or external keyboard, retry a label, or stop recognition and enter text yourself.
5. Choose whether to replace the converted strokes or retain all original ink, then press **Apply diagram**.

Recognition maps a rectangle to a process, a diamond to a decision, and an oval to the existing start/end shape. The latter is rendered as a rounded capsule, rather than preserving an exact ellipse. Original positions and relative node sizes are retained; alignment or rearrangement remains an explicit editing action.

A node outline may be drawn in one stroke or two to four segments whose endpoints are close and unambiguous. Limited gaps are tolerated. Oval outlines can overlap at the closing seam or end in a small tail; a slightly tilted diamond drawn in two strokes can still become a decision. These tolerances do not turn arbitrary loops or custom shapes into nodes.

Connectors may be straight, follow a simple orthogonal path, or have a gentle curve that can be simplified to a straight shaft. Supported heads include a separate V, two separate wings, or a closed/retraced triangle. A continuous gesture that draws a shaft and then a recognizable triangular head is also supported; the head must establish a clear direction near a valid destination. Broad curves, ambiguous loops and unsupported one-stroke gestures remain ink. Clear heads at both ends produce two directed connections. A crossing between lines does not create a junction.

A line without a clear arrowhead, an ambiguous destination, unsupported geometry or an unrelated stroke remains handwriting. The recognizer does not invent a direction from the order in which the line was drawn. Self-connections and duplicate connections with the same direction are not added.

Version 0.2.11 also recognizes some imperfect ovals and four-sided process boxes with a small gap at the seam. A single stroke can include an elbow and an explicit arrowhead. A separate arrowhead may start with a short continuation of the shaft. Review the shapes and connections before applying. Lines without clear direction stay as ink.

## Extend an already converted diagram

Draw additional shapes beside the diagram, or draw just an arrow between two existing nodes. Recognize the new ink: the preview contains **the complete updated diagram**, including the previous shapes and connections alongside the additions. A summary reports the number of new shapes and connections.

If the page has one diagram, it becomes the target. A selection containing exactly one diagram can also identify the target. When several targets are possible, choose the diagram to update in the dialog; **Apply diagram** stays disabled until the choice is resolved. You can choose to create a separate diagram when the drawing is independent. Other diagrams on the page remain unchanged.

Applying updates the same object, preserving existing identifiers, labels, connector routes and the page positions and sizes of its existing parts. When the overall bounds expand, existing text and line sizes remain visually consistent. Analysis accounts for rotation of the existing object; this does not add recognition of arbitrary rotated hand-drawn shapes.

Existing labels are read-only during this review and OCR does not overwrite them. To edit those labels, apply the additions and open the diagram editor. Labels for new parts can be corrected before applying. When there are no valid new shapes or connections, an empty conversion cannot be applied. Previously recognized outlines or connections are not duplicated when original ink has been kept; uncertain ink remains on the page.

## Labels and original ink

Text recognition is separate from shape recognition and uses the handwriting engine already installed with Quaderno. The geometric analysis works offline without a new AI model or cloud account. Label OCR availability and languages depend on the installed handwriting helper and dictionaries.

Strokes fully inside an identified shape are proposed as that shape’s label. A small nearby group can be proposed for a connection only when its association is unambiguous. Shapes and arrowheads are excluded from label OCR.

Each label can be edited or excluded. If OCR fails, is unavailable, or is stopped, a blank label does not prevent the shapes from being applied: the untranscribed ink remains on the page. Excluded labels also retain their original ink. Manually entering a nonblank accepted label permits replacement of its assigned ink when stroke replacement is enabled.

The dialog uses an independent recognizer. Closing it, changing page or layer, or locking Quaderno cancels pending recognition. Late results cannot update another page. If the underlying document changed after the preview was created, applying it fails safely and a new selection is required.

## Apply, undo and recovery

Application validates the selection and reviewed diagram, then commits creation or replacement of the object and any stroke removals as one operation. **Undo on the current page** restores that conversion together, including the previous diagram and newly drawn ink when extending a diagram; Redo applies it again. This history belongs to the current page/layer session and is not a durable history across navigation or restart.

When replacement is enabled, removed original strokes are also retained as the diagram’s handwriting source. The separate restore-original-handwriting action can recover them after saving and reopening the notebook. This stored source is distinct from session undo. For an extended diagram, it keeps an editable baseline from before the additions and collects ink removed by subsequent extensions on the same layer. Recovery restores that baseline and returns the additions as ink; it is not a permanent history of every step. Use page Undo to reverse only the last extension in the current session. If recoverable additions belong to another layer, return to that layer before recognizing more additions.

With **Replace converted strokes with shapes and text** turned off, the strokes remain directly on the page and no new duplicate recovery source is attached; an earlier stored source is retained. Applying a diagram suppresses the current automatic suggestion, including when you keep the ink, so the same unchanged sketch is not immediately offered again. Later ink changes or a change of page/layer context can enable another proposal when valid additions are recognized; manual recognition remains available.

Unrecognized strokes and rejected or empty label groups are never removed by the conversion.

## Use diagrams elsewhere

- **Writing:** copy a diagram into a writing notebook as an editable page object. Markdown export with assets includes a rendered image and a `.drawio` attachment; plain Markdown cannot itself store editable Quaderno geometry.
- **Books, Charts and Tables:** diagrams can accompany these objects on a page. Copying an object creates an independent copy; it is not a live link between documents.
- **PDF:** page export renders the diagram with the other page content.
- **draw.io:** import supported `.drawio`/XML diagrams and export editable `.drawio` files. Unsupported external elements may be omitted or simplified; review import notices.
- **Transfer and backup:** the portable `.quaderno` document and existing notebook transfer/backup flows retain the editable diagram model. Transferring files does not require a new diagram service.

Update **both apps to 0.2.10 or later** before transferring extended diagrams that require **portable format 10**. This format preserves the diagram’s visual scale and the editable baseline used to recover additions. The minimum format is chosen from the contents: notebooks without these new metadata fields can retain an earlier version. The installed Mac 0.2.0 app has not been updated and cannot import format 10; version 0.2.9 also predates this support. See [data versions and recovery](DATA.md#library-format-versions).

Apps older than 0.2.8 may also omit the dedicated workspace classification or the diagram’s stored original-handwriting metadata when importing and exporting. A newer app preserves an existing local workspace when an older peer omits the classification on an update, but that does not make a round trip through an older app fully lossless. Keep a current-version notebook backup before using an older peer. Copying a diagram into another notebook intentionally copies its editable geometry and labels without the original page’s recovery-ink source.

## Limits and drawing guidance

The editable model allows **64 nodes** and **128 directed connections** per diagram, with **200 characters per node label** and **120 per connection label**. Recognition accepts at most **4,000 strokes** and **120,000 points**, and rejects drawings with more than **256 candidate open outline segments**. Limits fail before conversion rather than applying a partial oversized diagram.

Shape matching is deliberately conservative. In the canonical 600 × 800 page coordinates, node bounds must be at least 30 × 24 units with area at least 1,200 square units. Larger, clearly closed shapes work better than small letter-sized marks. These coordinates are independent of the display’s physical pixel count. Complex sketches, arbitrary curves, rotated boxes, free-floating connectors, one-stroke gestures without a clearly recognized triangular head and exact custom routes remain outside the recognition contract. The supported single-stroke shaft-to-triangle pattern does not imply recognition of every continuous arrow gesture.

Use the structured editor to correct or extend the accepted diagram. Automatic recognition is an editable proposal, not a guarantee that every drawing will be interpreted correctly.
