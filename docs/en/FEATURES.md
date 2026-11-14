# Quaderno sections and file workflows

**English** · [Italiano](../FUNZIONALITA.md)

Quaderno has four library sections on Mac and Paper Pro. **Books** and **Writing** open notebooks. **Charts** and **Tables** index individual items stored on notebook pages. A chart or table can appear in its section and on its original page at the same time. Opening it from either place edits the same item.

| Section | Start with | Save or share |
| --- | --- | --- |
| [Books](#books) | Local PDF or DRM-free reflowable EPUB | An editable annotated notebook in Quaderno, or a PDF for reading and sharing |
| [Writing](#writing) | A blank Markdown document or an existing Markdown file | An editable Quaderno notebook, a Markdown file with optional assets, or a PDF |
| [Charts](#charts) | Points entered in the chart editor, a table on the same page, or a hand-drawn sketch | An editable chart, calibrated CSV data, or a chart image in a Markdown export |
| [Tables](#tables) | A blank grid or a UTF‑8 CSV or TSV file | An editable table, a CSV of its cells, or a chart linked to the table |

**Files**, **Favorites** and **Tags** filter the active section. Chart and table entries use the folder and favorite status of their notebook and can match notebook or page tags. **Trash** shows notebooks across sections. [Library and pages](USER_GUIDE.md#library-and-pages)

## Books

1. Choose **Books → Import document** and select a PDF or EPUB. On Mac, choose **Import from Apple Books** for two actions. **Open Apple Books** opens Books. **Choose PDFs or EPUBs…** selects local copies, including EPUB packages shown by Finder as a single book. For a PDF that opens in Preview, use **File → Export** in Preview to save a local copy. The Books library in iCloud and protected titles stay in Apple Books. [PDFs in Apple Books](https://support.apple.com/en-ge/guide/books/ibkseed72068/9.0/mac/27) · [Export from Preview](https://support.apple.com/guide/preview/export-pdfs-and-images-prvw0e8da223/mac)
2. Open the imported book. Use **Highlight** to mark a passage by hand, or **Underline** and drag between the endpoints of a horizontal line. Use **Add note** to place a text block on the page. To edit or move a saved note, turn on **Items** and select or drag the block.
3. Use zoom, **Fit to width** and bookmarks to navigate. Quaderno remembers the last page on each device. **Find text** searches embedded text that a PDF makes available. Scanned pages require a text layer for PDF search, and an older Quaderno import may need to be imported again to index that text.
4. Choose **Insert from library** to add an editable copy of a chart or table. Use **Save PDF** on Paper Pro or **Share → Save PDF** on Mac for a copy of the annotated book. Send the notebook through Quaderno to continue editing annotations on another device.

The source document is imported as page backgrounds. Highlights and underlines are ink marks over those backgrounds. EPUB import lays out reflowable text with headings, emphasis and lists. The original layout, images and embedded fonts are outside the imported copy. Imports allow up to **150 pages** and **30 MiB** of generated page images. The source limits are **48 MiB** for a PDF and **64 MiB** for an EPUB archive. PDF export renders the visible layers and page items together. The original PDF text layer and Quaderno annotation objects are unavailable as separate elements in that export. [Reading and annotating documents](USER_GUIDE.md#read-and-annotate-pdfs-and-epubs)

## Writing

1. Choose **Writing → New Markdown document**. Enter a title and type the source text or save an empty document. To start from a file, use **Import document** with a local `.md` or `.markdown` file. The Markdown and Obsidian panel also accepts a direct HTTPS link to a Markdown file. Imports accept UTF‑8 up to **256 KiB** and **200 pages**.
2. Choose **Edit Markdown** to change the document source. Quaderno displays the Markdown syntax as text on its pages and repaginates when the text changes. Existing annotations remain on their pages. If the source gets shorter, a page with annotations can stay in the notebook even after its source text moves or disappears.
3. Write on a page with the Marker on Paper Pro or drag with the pointer on Mac. Choose from five page templates, three stroke widths and ink layers. The lasso can move, rotate, resize or copy selected strokes. Use **Text** for an editable block separate from the Markdown source editor. On Paper Pro with the handwriting engine installed, the lasso starts Italian transcription. On a supported Mac, choose **Transcribe in Italian**. [Handwriting transcription](USER_GUIDE.md#turn-handwriting-into-text)
4. Use **Insert image** for a local PNG or JPEG. Quaderno embeds its own copy. In **Items**, select the image to change its size or delete it, or drag it to move it. A source image can be up to **16 MiB**. The embedded copy is limited to **1,200 pixels** on its longest edge and **768 KiB**. A page can contain up to **12 images**.
5. Use **Insert from library** for editable chart and table copies, or add a table or a flowchart directly on the page. In the diagram editor, draw.io import reads the first diagram from a compatible `.drawio` or `.xml` file. Export as `.drawio` to keep supported shapes and connectors editable in draw.io. [Flowcharts](USER_GUIDE.md#flowcharts)

**Markdown and Obsidian → Export Markdown** saves source text and text blocks. When present, it adds relative links to embedded images and PNGs of charts and visible ink in a sibling folder. Move that folder with the Markdown file. Importing the exported file brings back the text and links. Export tables separately as CSV and flowcharts as `.drawio`. To recover the editable objects on another device, transfer the Quaderno notebook. Obsidian vault integration exchanges Markdown text. [Markdown and Obsidian](USER_GUIDE.md#markdown-and-obsidian)

## Charts

- **Enter points manually.** In **Charts → New chart**, name the notebook, then start from two editable points. Choose line or bar, edit the title and axis labels, and add, remove or adjust points. The point editor uses coordinates from **0 to 100%**. **Numeric scale** sets the axis maxima. A line requires at least two points, bars require at least one with distinct X values, and the editor supports up to **128 points**.
- **Build from a table.** Open a saved table and choose **Chart**. Select two different numeric columns and optionally **Use first row as headers**. Decimal commas and decimal points are accepted. Numeric formula results can feed the chart. Line charts need at least two numeric pairs. Bars need at least one pair and distinct X values. Rows with both selected cells empty are skipped. Fill a missing value or correct nonnumeric data to restore an invalid preview. The saved chart recalculates when you save changes to its source table on the same page.
- **Convert an ink sketch.** Draw two axes that meet at the lower left and one curve or a set of vertical bars. Select the sketch with the **Lasso** and choose **Convert to chart**. Review the points, calibrate the scale if needed and save the chart on the same or a new page. [Convert a sketch to a chart](USER_GUIDE.md#turn-a-sketch-into-a-chart)

A saved chart opens in its editor from **Charts**. **Insert from library** copies the plotted values and settings into the target page. For a chart linked to a table, the copy uses the current valid data and has its own editable points. Updates to the table continue to affect the original linked chart.

**Copy CSV** and **Save CSV** are available in the independent chart editor once the numeric scale is set. The CSV uses the calibrated axis limits and preserves negative values when the chart is a copy of a table-linked chart. To import chart data as a table, use a CSV with up to 12 rows, including its header.

## Tables

1. In **Tables**, choose **New table**, enter a notebook name and open the grid editor. From a page, **Table** creates an item in the current notebook.
2. In a new table, choose **Import CSV** and select a local `.csv` or `.tsv` file. Quaderno reads UTF‑8 files up to **256 KiB** and recognizes comma, semicolon or tab separators in CSV. Quoted cells can contain separators, line breaks and escaped quotes. The first row becomes editable cells, and shorter rows are padded with empty cells.
3. Select a cell to edit it. Use **+** and **−** under **Rows** and **Columns** to change the grid. **Tab** moves to the next cell, **Shift+Tab** goes back and **Enter** moves down. A table supports up to **12 rows × 8 columns** and **500 characters per cell**.
4. Choose **Save** to add the table to the library index. **Export CSV** writes the current grid, including edits in the open draft, as comma-separated UTF‑8. Formula cells export their displayed result or error code. Transfer the notebook to keep formulas editable and chart links intact.

To calculate a total or average, select an empty cell below the data and choose **Sum** or **Average**. The button suggests the range from the first row to the previous row in the selected column. Edit the references in the cell field. Use references such as `A1` or rectangular ranges such as `A1:B3`. For example, enter `=SUM(B2:B4)` in `B5` and `=AVERAGE(B2:B4)` in `B6`. `SOMMA` and `MEDIA` also work. Separate multiple arguments with commas or semicolons. Use a decimal point in numbers typed directly in a formula to avoid ambiguity.

Quaderno also evaluates recognized formulas from imported CSV and TSV files. The grid and page show the result, and the cell field keeps the editable expression. Quaderno skips text and blank cells in ranges. An average without numbers shows `#DIV/0!`, an out-of-bounds reference shows `#REF!`, and a circular reference shows `#CYCLE!`.

On Paper Pro, choose a file already stored on the tablet. If the CSV is on Mac, you can import it there and send the notebook. To reuse the table inside Quaderno, choose **Insert from library** on the destination page. This creates a separate table you can edit or use as the source of a new chart.

## How content moves between sections

| From | To | What happens |
| --- | --- | --- |
| Tables | Charts | **Chart** creates a live link to two columns in a table on the same page. The linked chart recalculates after saved edits. |
| Charts or Tables | Writing or Books | **Insert from library** places an independent, editable copy on the destination page. |
| Writing or Books | Charts | Select a suitable ink sketch on the page and convert it. The saved chart appears in **Charts** and the ink stays on the page. |
| Charts | Tables | Export calibrated data to CSV and import it in a new table. The file carries values and column labels. |
| Writing | Books | Export the notebook as PDF and import that file in **Books**. Use this for a reading copy with the original page appearance. |
| Any notebook | Another notebook | Open page thumbnails and use **Move to…**. The whole page moves, including its annotations and same-page table and chart relationships. |

For example, import a `sales.csv` with columns **Month** and **Sales** into **Tables**. Use numeric months such as `1` and `2`. Save and reopen the table, choose **Chart**, select the two columns and use the first row as headers. Open a report in **Writing** and insert a copy of the chart. Later edits to the table update the original chart. Insert a fresh copy if the report needs the new values.

## Choosing a file or transfer method

| Method | What the recipient can work with |
| --- | --- |
| Quaderno notebook transfer | Page objects remain editable, including Markdown source, ink and layers, text notes, embedded images, tables with formulas, charts and diagram shapes. Same-page table links, book backgrounds and bookmarks travel with the notebook. |
| Markdown and its assets folder | Editable text and linked image files. Charts and handwriting are images. |
| PDF | A reading or sharing copy of the rendered pages, including visible annotations and objects. |
| Table CSV or chart CSV | Cell values, formula results, or calibrated X/Y data. Import the CSV in **Tables** to work with those values again. |
| draw.io file | Compatible diagram shapes, labels and connectors that can be edited in a draw.io workflow. |

For Mac and Paper Pro, use **Wireless sync → Preview notebook transfer** and confirm the send. Bluetooth LE supports up to **8 MiB** per serialized notebook and local Wi‑Fi up to **64 MiB**. Use Wi‑Fi for larger books and notebooks with images. After a shared revision exists, you can enable **Sync automatically** and **Sync this notebook**. Changes made independently may produce a conflict copy. When updating an existing notebook, the destination keeps its folder, favorite status, notebook tags and local reading position. **Continue on Mac** and **Continue on Paper Pro** can request a particular page. [Wireless and USB‑C transfers](USER_GUIDE.md#wireless-sync)

Update both apps before sending notebooks with embedded images, which use Quaderno library format **8**. For a full library backup or restore, use `quaderni.json`. [Data and backups](DATA.md)
