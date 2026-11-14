# Data, backups, and privacy

**English** · [Italiano](../DATI.md)

## Where Quaderno stores files

| Platform | Default data folder |
| --- | --- |
| macOS | `~/Library/Application Support/Quaderno/` |
| Paper Pro | `/home/root/.local/share/Quaderno/` |

`quaderni.json` contains your Quaderno library, including folders, pages, layers, tags, strokes, text, and imported document backgrounds. `accesso.json` holds the PIN configuration when a PIN is set. Quaderno uses the app data location returned by Qt. The built-in USB import reads the default Paper Pro path shown above.

Quaderno saves successful changes automatically. Back up your library before upgrading. Earlier versions may reject a library with features introduced in a newer build, such as charts or the original strokes stored for smart shapes.

## Back up a library

1. Close Quaderno on the device you want to back up.
2. On Mac, open the data folder in Finder with **Go → Go to Folder** and copy `quaderni.json` to a backup folder.
3. On Paper Pro in Developer Mode, copy the same file over USB‑C with OpenSSH. Run this command from your backup folder and choose an unused filename.

```sh
scp -O root@10.11.99.1:/home/root/.local/share/Quaderno/quaderni.json ./quaderni-paper-pro-backup.json
```

The content backup is `quaderni.json`. To preserve the PIN and its settings, store a separate copy of `accesso.json` in a secure location. Back up documents managed by the reMarkable interface separately.

## Restore or import

- To **restore the entire library**, close Quaderno and keep a copy of the current file. Copy the backup into the data folder as `quaderni.json`, then reopen Quaderno. If the data folder does not exist, create it or launch and close Quaderno before restoring the file.
- To **add notebooks to an existing library** on Mac, choose **Importa quaderni** (Import notebooks) and open the JSON file, or choose **Importa dal Paper Pro** (Import from Paper Pro) to read it over USB‑C. Quaderno recognizes unchanged notebooks. When versions diverge and both need to be kept, it adds a copy.
- Library imports accept files up to **64 MiB**. For a larger library, keep a full copy of the file. Restoring it replaces the library Quaderno opens.

To use your content in other apps, export pages as PDF, text as Markdown with any accompanying assets, table cells or calibrated chart data as CSV, and diagrams as `.drawio`. [File and transfer formats](FEATURES.md#choosing-a-file-or-transfer-method)

## Library format versions

The `version` field in `quaderni.json` identifies its format. Quaderno chooses the format based on the contents of the library.

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

The current build reads formats 1 through 8. Update both apps before transferring notebooks with embedded images. [Library format implementation](../../src/notebook.cpp)

The optional `pageIds` and `lastViewedPageId` fields identify pages and store the last local position for each notebook. Older libraries get page identifiers on load. The reading position stays on the device, and navigation does not change the content revision.

## Privacy and access

- The library is stored on your device. When you share or save a PDF, the destination receives a copy. Apple apps use their own iCloud settings.
- Mac transcription uses PencilKit and Vision. Quaderno passes the selected strokes to a local component. [Apple documentation](https://developer.apple.com/documentation/pencilkit/recognizing-handwriting-and-converting-to-text)
- The PIN protects access through the app interface. Quaderno stores a randomly salted PBKDF2 hash in the configuration file and limits attempts. Anyone with access to the data folder can read `quaderni.json`. Protection at rest depends on the operating system and where you store your backups.
- Quaderno prompts for the SSH password when authentication requires it. For imports from Mac, OpenSSH verifies the tablet against the host key in `known_hosts`. Quaderno sends Wi‑Fi requests to the system service on Paper Pro.

Use a sample notebook when reporting a security issue or attaching a reproduction to a bug report. [Security policy](../../SECURITY.md)
