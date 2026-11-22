# Quaderno functional QA

[Full functional matrix in Italian](../COLLAUDO_FUNZIONALE.md) · [Function and workflow map](FUNCTIONS_AND_WORKFLOWS.md)

Record the candidate commit, installed app versions, macOS version, Paper Pro firmware and connection type. Test with synthetic documents on isolated libraries. Use the same compatible portable notebook format on both devices. Verify a backup before updating a physical device.

## Automated gate

Configure CMake with `BUILD_TESTING=ON`, then build all test targets and run the gate. It checks the registered CTest suites, runs them under temporary settings, checks rendered desktop and tablet layouts and writes `qa-results.xml` inside the build directory. Each JUnit entry represents one CTest suite. Read the QtTest log to identify individual skipped cases. [Build guide](DEVELOPMENT.md)

```sh
cmake --build build --parallel 2
python3 scripts/run-qa.py build --parallel 2
```

Use `--ctest` to select a CTest executable outside `PATH` and `--binary` to check an executable from an extracted package. Run the SDK build gate with the target Qt runtime available. [QA gate source](../../scripts/run-qa.py)

Run the optional handwriting engine against seven synthetic samples, including multiline text, lookalike O and zero, a leading minus and a decimal comma. WritePad adds a check for `IDX01`. [Synthetic samples](../../tests/fixtures/handwriting.json)

```sh
python3 scripts/check-handwriting.py /path/to/packaged/quaderno-handwriting
```

## End-to-end checks

| Area | Representative workflow and acceptance check |
| --- | --- |
| Library and writing | Create a notebook, organize and recover it, add pages, Markdown, images, ink layers and typed text. Verify changes, undo and content after reopening. |
| Books | Open synthetic EPUB and PDF, including pages beyond page 150. Verify zoom, pan, bookmarks, search, highlights, underlines and anchored notes. Edit a notebook link, return to the reader and check PDF note appendices. |
| Tables and charts | Import CSV/TSV with negative values and formulas. Review handwritten cells and reject unintended overwrite. Test charts with separately drawn bar edges and isolated notes, then verify updates to a same-page linked chart and independence of a cross-page copy. |
| Diagrams and mixed pages | Create flowchart and UML shapes. Round-trip draw.io. Review identified shapes and labels, extend a diagram, then convert a mixed page into a single notebook with editable linked pages. |
| Transfers and exports | Transfer a portable notebook, pair Wi‑Fi and Bluetooth, exercise fallback, interruption and conflict recovery. Verify PDF, PNG, Markdown assets, CSV and draw.io output alongside content and formulas in the native file. |
| Display and input | On Paper Pro, test the Marker, palm rejection, a physical keyboard, rotation, standby, PIN and E Ink refresh. In a matching stream-on and stream-off session, compare residue on the physical display after alternating inked and blank pages. |
| Integrations | Use dedicated accounts and samples to verify Apple app handoff, Obsidian Sync, OAuth imports and uploads, and SMTP delivery. Record simulator and live-service results independently. |
| Local data | Reopen modified documents, migrate and restore a synthetic library, check backup retention, clear derived caches and compare documents and reading positions. |

Record expected and actual results, screenshot or log references, and any prerequisite that blocks a case. The full matrix gives the steps, expected outcomes and evidence already gathered, including a hand-drawn 2×3 grid with empty cells and a row-based fallback when the dividers are absent. [Functional matrix and results](../COLLAUDO_FUNZIONALE.md)

### Workflows that span changes and reopening

| Session | Steps and acceptance check | CTest suite |
| --- | --- | --- |
| Take handwritten notes | Write continuous curved strokes and punctuation with different pressure. Include a stroke starting on the full-page menu button. Undo and redo, continue on a second page after changing the display orientation and zoom, then reload both pages from disk. Compare the strokes, pressure and page contents. | `ui` |
| Review a long PDF | Navigate straight to page 157 of a synthetic 162-page PDF. Add a Post-it with original ink and typed text, then advance one page. Start a second app session from the same library, use the annotated-page filter to find the Post-it and export that page. Verify the exported note appendix. | `booknotesintegration` |
| Update a report | Start from a report with linked editable pages. Update a source value, check the calculated table and its chart, edit the linked workflow diagram, then reopen the notebook from disk and follow the same links. | `workspaceintegration` |
| Exchange concurrent edits | Pair two library instances over a local TLS transport. Edit different parts of the same notebook while apart, then rejoin. Verify what the conflict review applies and what remains on the other branch, including editable formulas and notes. | `wirelesssync` |

## 3.0.0 results

| Environment | Result |
| --- | --- |
| macOS 27.2, Apple silicon, Qt 6.8.3 | All 24 CTest suites passed, with zero failed or skipped suites. Both desktop and tablet layouts rendered at startup. |
| Mac app, synthetic library | Opened all five sections and all 12 Settings categories. Advanced a PDF page, drew and used Undo and Redo, saved a negative table value, and added a diagram shape. |
| Tablet layout, simulated input | Opened all 13 Settings categories in portrait and landscape. In both orientations a pen drag from the full-page Tools button kept its stroke. A short pen tap opened Tools. Existing tests also covered pressure, palm rejection and gestures. |
| Paper Pro SDK, ARM64 | Version 3.0.0 built with the SDK for firmware 3.29.1.0 and rendered both layouts with the target runtime. |
| Physical Paper Pro, firmware 3.29.1.0 | AppLoad launched 3.0.0 at 1620 × 2160. User verified continuous Marker strokes both in a clear area and when starting over the full-page Tools button. A short Marker tap still opened Tools. |
| macOS handwriting transcription | Seven synthetic samples passed on retry, including a negative number and multiline text. The first Italian sample reached the 60 s timeout during a cold launch. |

Native Apple app handoff, live OAuth, Obsidian and SMTP accounts, Bluetooth accessories, and physical E Ink ghosting require separate runs. See the full matrix for on-device observations and previous evidence. [Detailed results](../COLLAUDO_FUNZIONALE.md#evidenze-registrate)
