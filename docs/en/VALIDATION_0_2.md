# Quaderno 0.2 validation record

[Italiano](../VERIFICA_0_2.md) · [Development notes](UPGRADE_0_2.md)

Recorded on **16 November 2026** for the development working tree. This record separates completed checks from checks still required for the final packages.

## Final build status

| Check | Status |
| --- | --- |
| Final macOS Release build and all six automated suites | **Passed: 6/6 suites, 45 test cases plus 12 setup/cleanup cases; 13.18 s** |
| macOS rendered startup in desktop and tablet layouts | **Passed in isolated profiles, including the bundled application** |
| macOS package | **Development ad-hoc signature verified; bundled dependencies checked. Built for macOS 27.2 or later, Apple Silicon** |
| Final Paper Pro SDK build and ARM automated suites | **Passed: 6/6 suites, 44 test cases plus 12 setup/cleanup cases; 11.11 s. One expected macOS-only Apple delivery test skipped** |
| ARM rendered startup in desktop and tablet layouts | **Passed with the firmware Qt runtime in a VM** |
| Installation and physical Paper Pro testing | **Pending device connection** |

The ARM build used the ferrari 6.0.112 SDK, Qt 6.10.3 and GCC 16.2 for firmware 3.29.1.0. Final test logs are retained locally in `build-dev/test-final.log` and `build-arm/test-final.log`.

No installation of the new build on a physical Paper Pro is claimed here. ARM execution in a VM and the tablet layout on a Mac do not establish actual display refresh, Marker, touch, Bluetooth, or device performance.

## Reader checks on macOS

The reader was tested on Apple Silicon with macOS 27.2 and Qt 6.8.3. The isolated validation harness used a **Debug** build.

A test EPUB produced **591 pages** in **3.839 seconds**. The import retained the original EPUB archive and the generated vector PDF layout. It prepared the layout without rasterizing every page.

Pages **1, 11, 76, and 151** were then requested individually and rendered at **1350 × 1800 pixels**. All four contained visible content. The cover and page 11 were also visually inspected: the cover illustration and page text were present.

The reader's focused QtTest run reported **8 passed, 0 failed, 0 skipped**, including setup and cleanup. It covered:

- A generated 161-page PDF, rendering the requested page, zoom resolution, and rejection of an invalid page index.
- EPUB text and an image wrapped in SVG markup.
- Cancellation and rejection of a source with an incorrect digest.
- Original-source retention and annotations after reopening, moving a page to another notebook, and transferring it to another model.
- An asynchronous import that preserves the current page and undo history, and cancellation that leaves no partial notebook.
- A responsive event loop while import preparation runs.

These results verify the tested rendering paths. They do not establish support for every EPUB layout or DRM-protected publication.

## Migration of a copy of the existing library

The migration harness copied a **62,513,311-byte** library, approximately **62.5 MB / 59.6 MiB**, and its reading-position sidecar into an isolated test directory. It loaded **11 existing notebooks**. The active book contained **471 pages**, and its stored reading index **9**, corresponding to page **10**, was preserved.

A new QA notebook and one pen stroke were created in the copy. After saving and reopening:

- All **11 original notebook content revision hashes** matched their pre-migration values.
- The QA notebook and stroke remained available.
- The SHA-256 of the actual user library file was unchanged.

| Operation in the copied library | Observed time on the Mac |
| --- | ---: |
| Initial load | 1.152 s |
| Migration and first save | 2.603 s |
| Subsequent incremental pen-stroke save | 0.140 s |
| Reopening the migrated library | 1.082 s |

These are single observations from the Mac validation harness, not repeated benchmarks or Paper Pro timings. The test did not modify the actual user library.

## Storage checks

The focused regression run reported **13 passed, 0 failed, 0 skipped**, including setup and cleanup. Storage checks covered legacy migration, concurrent writers, damaged and missing manifests, missing page objects, and preservation of the previous recoverable generation.

A dedicated incremental-save test used **12 MiB of unchanged image/source text**. The next edit encoded **zero blob bytes**, produced **one changed page object**, and left the existing image object's modification time unchanged. Another case rejected an oversized expanded library even when repeated books shared the same underlying assets. Cache entries were removed when their assets were no longer referenced.

## Evidence and remaining checks

Reproducible test sources are [reader tests](../../tests/reader_tests.cpp) and [regression tests](../../tests/regression_tests.cpp). The local run retained logs in the ignored `build-reader-check` directory: `reader-final.log`, `regression-final.log`, `blue-team.log`, and `migration.log`.

The personal EPUB, rendered previews, and library copy are local validation material and are excluded from the repository. Final package results belong in the status table above. Physical Paper Pro checks remain necessary for gestures, refresh behavior, native pen interaction, wireless transfer, and performance with the installed build.
