# Quaderno sections and file workflows

**English** · [Italiano](../FUNZIONALITA.md)

Quaderno has five library sections on Mac and Paper Pro. **Books** and **Writing** open notebooks. **Charts**, **Tables** and **Diagrams** index editable items stored on notebook pages. Diagrams also keeps an empty diagram notebook accessible before it contains its first diagram. Opening an indexed entry shows its original page without opening an editor. Editing it there changes the same object seen from its source notebook; **Insert from library** instead creates an independent copy. The fifth section and new recognition workflow are part of 0.2.8 in development. On November 18, 2026, the user confirmed recognition, application and editing on the tested Paper Pro flowchart; see the [verification scope](SETTINGS.md#verification-boundary).

| Section | Start with | Save or share |
| --- | --- | --- |
| [Books](#books) | Local PDF or DRM-free reflowable EPUB | An editable annotated notebook in Quaderno, or a PDF for reading and sharing |
| [Writing](#writing) | A blank Markdown document or an existing Markdown file | An editable Quaderno notebook, a Markdown file with optional assets, or a PDF |
| [Charts](#charts) | Points entered in the chart editor, a table on the same page, or a hand-drawn sketch | An editable chart, calibrated CSV data, or a chart image in a Markdown export |
| [Tables](#tables) | A blank grid or a UTF‑8 CSV or TSV file | An editable table, a CSV of its cells, or a chart linked to the table |
| [Diagrams](#diagrams) | A blank diagram notebook, a hand-drawn flowchart, editor shapes or a supported draw.io file | An editable diagram, PDF, or PNG plus `.drawio` in a Markdown export |

**Files**, **Favorites** and **Tags** filter the active section. Chart, table and diagram entries use the folder and favorite status of their notebook and can match notebook or page tags. **Trash** shows notebooks across sections. [Library and pages](USER_GUIDE.md#library-and-pages)

## Books

1. Choose **Books → Import document** and select a PDF or EPUB. On Mac, choose **Import from Apple Books** for two actions. **Open Apple Books** opens Books. **Choose PDFs or EPUBs…** selects local copies, including EPUB packages shown by Finder as a single book. For a PDF that opens in Preview, use **File → Export** in Preview to save a local copy. The Books library in iCloud and protected titles stay in Apple Books. [PDFs in Apple Books](https://support.apple.com/en-ge/guide/books/ibkseed72068/9.0/mac/27) · [Export from Preview](https://support.apple.com/guide/preview/export-pdfs-and-images-prvw0e8da223/mac)
2. Open the imported book. Use **Highlight** to mark a passage by hand, or **Underline** and drag between the endpoints of a horizontal line. Use **Add note** to place a text block on the page. To edit or move a saved note, turn on **Items** and select or drag the block.
3. Use zoom, **Fit to width** and bookmarks to navigate. Quaderno remembers the last page on each device. **Find text** searches embedded text that a PDF makes available. Scanned pages require a text layer for PDF search, and an older Quaderno import may need to be imported again to index that text.
4. Choose **Insert from library** to add an editable copy of a chart, table or diagram. Use **Save PDF** on Paper Pro or **Share → Save PDF** on Mac for a copy of the annotated book. Send the notebook through Quaderno to continue editing annotations on another device.

Quaderno preserves the original PDF or EPUB and prepares documents in the background, with progress and cancellation. Only requested pages are rendered, at the resolution needed for zoom. EPUB import preserves text, headings, lists, tables and supported local images, including images wrapped in SVG; it uses a fixed reading layout rather than reproducing every original font or style. The old 150-page and 30 MiB raster-image limits no longer apply. Source limits remain **48 MiB** for PDF and **64 MiB** for EPUB; the prepared EPUB PDF must fit within **48 MiB**, and library/transfer size limits still apply. Highlights, underlines and notes remain separate editable annotations inside Quaderno. Exported PDFs combine visible layers and page objects; original PDF text and Quaderno objects are not retained as separately editable elements. [Reading and annotating documents](USER_GUIDE.md#read-and-annotate-pdfs-and-epubs)

## Writing

1. Choose **Writing → New Markdown document**. Enter a title and type the source text or save an empty document. To start from a file, use **Import document** with a local `.md` or `.markdown` file. The Markdown and Obsidian panel also accepts a direct HTTPS link to a Markdown file. Imports accept UTF‑8 up to **256 KiB** and **200 pages**.
2. Choose **Edit Markdown** to change the document source. Quaderno displays the Markdown syntax as text on its pages and repaginates when the text changes. Existing annotations remain on their pages. If the source gets shorter, a page with annotations can stay in the notebook even after its source text moves or disappears.
3. Write on a page with the Marker on Paper Pro or drag with the pointer on Mac. Choose from five page templates, three stroke widths and ink layers. The lasso can move, rotate, resize or copy selected strokes. Use **Text** for an editable block separate from the Markdown source editor. On Paper Pro with the handwriting engine installed, the lasso starts Italian transcription. On a supported Mac, choose **Transcribe in Italian**. [Handwriting transcription](USER_GUIDE.md#turn-handwriting-into-text)
4. Use **Insert image** for a local PNG or JPEG. Quaderno embeds its own copy. In **Items**, select the image to change its size or delete it, or drag it to move it. A source image can be up to **16 MiB**. The embedded copy is limited to **1,200 pixels** on its longest edge and **768 KiB**. A page can contain up to **12 images**.
5. Use **Insert from library** for editable chart, table and diagram copies, or add a table or a flowchart directly on the page. In the diagram editor, draw.io import reads the first diagram from a compatible `.drawio` or `.xml` file. Export as `.drawio` to keep supported shapes and connectors editable in draw.io. [Flowcharts](USER_GUIDE.md#flowcharts)

**Markdown and Obsidian → Export Markdown** saves source text and text blocks. When present, it adds relative links to embedded images and PNGs of charts, diagrams and visible ink in a sibling folder. Each diagram also has an editable `.drawio` companion linked from the Markdown. Keep the folder and file together. Importing the Markdown restores text and links, not the original editable Quaderno objects. Export table values separately as CSV; transfer a `.quaderno` notebook or use notebook synchronization to retain editable objects and recovery ink. Obsidian vault integration exchanges Markdown text. [Markdown and Obsidian](USER_GUIDE.md#markdown-and-obsidian)

## Charts

- **Enter points manually.** In **Charts → New chart**, name the notebook, then start from two editable points. Choose line or bar, edit the title and axis labels, and add, remove or adjust points. The point editor uses coordinates from **0 to 100%**. **Numeric scale** sets the axis maxima. A line requires at least two points, bars require at least one with distinct X values, and the editor supports up to **128 points**.
- **Build from a table.** Open a saved table and choose **Chart**. Select two different numeric columns and optionally **Use first row as headers**. Decimal commas and decimal points are accepted. Numeric formula results can feed the chart. Line charts need at least two numeric pairs. Bars need at least one pair and distinct X values. Rows with both selected cells empty are skipped. Fill a missing value or correct nonnumeric data to restore an invalid preview. The saved chart recalculates when you save changes to its source table on the same page.
- **Convert an ink sketch.** Draw two axes that meet at the lower left and one curve or a set of vertical bars. Select the sketch with the **Lasso** and choose **Convert to chart**. Review the points, calibrate the scale if needed and save the chart on the same or a new page. [Convert a sketch to a chart](USER_GUIDE.md#turn-a-sketch-into-a-chart)

Opening a saved chart from **Charts** shows its page. Tap the chart with a finger, mouse, or brief pen contact to edit it; **Items** remains available for editing and moving. **Insert from library** copies the plotted values and settings into the target page. For a chart linked to a table, the copy uses the current valid data and has its own editable points. Updates to the table continue to affect the original linked chart.

**Copy CSV** and **Save CSV** are available in the independent chart editor once the numeric scale is set. The CSV uses the calibrated axis limits and preserves negative values when the chart is a copy of a table-linked chart. To import chart data as a table, use a CSV with up to 12 rows, including its header.

## Tables

1. In **Tables**, choose **New table**, enter a notebook name and open the grid editor. From a page, **Table** creates an item in the current notebook.
2. In a new table, choose **Import CSV** and select a local `.csv` or `.tsv` file. Quaderno reads UTF‑8 files up to **256 KiB** and recognizes comma, semicolon or tab separators in CSV. Quoted cells can contain separators, line breaks and escaped quotes. The first row becomes editable cells, and shorter rows are padded with empty cells.
3. Select a cell to edit it. Use **+** and **−** under **Rows** and **Columns** to change the grid. **Tab** moves to the next cell, **Shift+Tab** goes back and **Enter** moves down. A table supports up to **12 rows × 8 columns** and **500 characters per cell**.
4. Choose **Save** to add the table to the library index. **Export CSV** writes the current grid, including edits in the open draft, as comma-separated UTF‑8. Formula cells export their displayed result or error code. Transfer the notebook to keep formulas editable and chart links intact.

To calculate a total or average, select an empty cell below the data and choose **Sum** or **Average**. The button suggests the range from the first row to the previous row in the selected column. Edit the references in the cell field. Use references such as `A1` or rectangular ranges such as `A1:B3`. For example, enter `=SUM(B2:B4)` in `B5` and `=AVERAGE(B2:B4)` in `B6`. `SOMMA` and `MEDIA` also work. Separate multiple arguments with commas or semicolons. Use a decimal point in numbers typed directly in a formula to avoid ambiguity.

`MIN`, `MAX` and `COUNT`/`CONTA.NUMERI` are also available, with quick-insert buttons and a formula guide. Grid headers show column letters and row numbers, and selected-cell errors have explanations below the formula. Shrinking a table with populated cells requires confirmation. Update both apps to 0.2.1 to calculate the new functions.

Quaderno also evaluates recognized formulas from imported CSV and TSV files. The grid and page show the result, and the cell field keeps the editable expression. Quaderno skips text and blank cells in ranges. An average without numbers shows `#DIV/0!`, an out-of-bounds reference shows `#REF!`, and a circular reference shows `#CYCLE!`.

On Paper Pro, choose a file already stored on the tablet. If the CSV is on Mac, you can import it there and send the notebook. To reuse the table inside Quaderno, choose **Insert from library** on the destination page. This creates a separate table you can edit or use as the source of a new chart.

## Diagrams

1. Choose **Diagrams → New diagram** and name the notebook. It opens a blank page for drawing; no placeholder shapes or automatic editor are inserted. The workspace remains in the section while empty or containing only handwriting.
2. Draw process rectangles, decision diamonds, start/end ovals and connecting arrows. In diagram notebooks, automatic recognition can offer a suggestion after a pause. Open it to review; **Close** dismisses it until a later drawing change. You can also select ink with the lasso and request diagram recognition explicitly on another notebook page.
3. Review the detected shapes and arrows, then transcribe or type their labels. Exclude a label to leave its ink untouched. Apply the reviewed diagram, choosing whether to replace converted strokes or keep all original ink. Ambiguous strokes are retained. Replacement also stores the removed originals for later recovery, separately from session undo.
4. Tap a saved diagram to open its editor, or use **Items**. Change labels, shapes, connections and layout; import supported draw.io files, or export `.drawio`. **Insert from library** copies a saved diagram to another notebook as an independent editable object.

Geometry recognition is local; text recognition uses the handwriting engine available on that device. It does not require a new cloud account. The model supports 64 nodes and 128 directed connections. See [Diagrams](DIAGRAMS.md) for drawing guidance, limits and recovery behavior.

## How content moves between sections

| From | To | What happens |
| --- | --- | --- |
| Tables | Charts | **Chart** creates a live link to two columns in a table on the same page. The linked chart recalculates after saved edits. |
| Charts, Tables or Diagrams | Writing or Books | **Insert from library** places an independent, editable copy on the destination page. |
| Writing or Books | Charts | Select a suitable ink sketch on the page and convert it. The saved chart appears in **Charts** and the ink stays on the page. |
| Any page with ink | Diagrams | Review a lasso conversion; the saved object appears in the diagram index while staying on its original page. |
| Diagrams | Pages containing charts or tables | Insert an independent editable diagram beside the other page items. |
| Charts | Tables | Export calibrated data to CSV and import it in a new table. The file carries values and column labels. |
| Writing | Books | Export the notebook as PDF and import that file in **Books**. Use this for a reading copy with the original page appearance. |
| Any notebook | Another notebook | Open page thumbnails and use **Move to…**. The whole page moves, including its annotations and same-page table and chart relationships. |

For example, import a `sales.csv` with columns **Month** and **Sales** into **Tables**. Use numeric months such as `1` and `2`. Save and reopen the table, choose **Chart**, select the two columns and use the first row as headers. Open a report in **Writing** and insert a copy of the chart. Later edits to the table update the original chart. Insert a fresh copy if the report needs the new values.

## Choosing a file or transfer method

| Method | What the recipient can work with |
| --- | --- |
| Quaderno notebook transfer | Page objects remain editable, including Markdown source, ink and layers, text notes, embedded images, tables with formulas, charts and diagram shapes. Same-page table links, book backgrounds and bookmarks travel with the notebook. |
| Markdown and its assets folder | Editable text and linked image files. Charts and handwriting are images; diagrams include both PNG previews and editable `.drawio` companions. |
| PDF | A reading or sharing copy of the rendered pages, including visible annotations and objects. |
| Table CSV or chart CSV | Cell values, formula results, or calibrated X/Y data. Import the CSV in **Tables** to work with those values again. |
| draw.io file | Compatible diagram shapes, labels and connectors that can be edited in a draw.io workflow. |

For Mac and Paper Pro, use **Wireless sync → Preview notebook transfer** and confirm the send. When both apps are updated, Bluetooth LE and local Wi‑Fi support up to **64 MiB** per serialized notebook. Only missing pages and assets are transferred, and saved chunks survive interruptions. Prefer Wi‑Fi for the first transfer of a large book. Older clients use full transfers with the previous 8 MiB Bluetooth limit; new document properties require both apps to be updated. After a shared revision exists, you can enable **Sync automatically** and **Sync this notebook**. The 0.2.9 development controls add automatic Wi-Fi/Bluetooth selection and synchronization after edits; see [automatic synchronization](SYNC_PROTOCOL.md#automatic-synchronization-in-029) for prerequisites and verification status. Changes made independently may produce a conflict copy. When updating an existing notebook, the destination keeps its folder, favorite status, notebook tags and local reading position. **Continue on Mac** and **Continue on Paper Pro** can request a particular page. [Wireless and USB‑C transfers](USER_GUIDE.md#wireless-sync)

Update both apps before transferring documents with native sources, which use portable library format **9**. A local `quaderni.json` is now a manifest: a complete backup also needs its `.objects` directory and recovery/reading-position sidecars. Use Quaderno’s portable export for a standalone transfer file. [Data and backups](DATA.md)
