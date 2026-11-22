# Local backups, cache and reading preferences

**Version:** 0.2.7 development. [Italiano](../LIBRARY_MAINTENANCE.md).

These settings work on the current device. A backup created on the Paper Pro remains on the Paper Pro; creating it does not transfer it to the Mac or upload it to a cloud account.

## Back up the library

Open the backup controls in Settings and choose **Create backup now**. The operation runs in the background. Wait for the message confirming that the backup was saved locally before copying it elsewhere.

Choose manual, daily or weekly backups, and retain 1, 5, 10 or 20 versions. Manual is the default; the default retention setting is 5. Scheduled backups run only while Quaderno is open and maintenance is allowed. The application pauses new scheduled work while locked, asleep, importing or using an editor. A missed interval is checked when the application is available again; there is no operating-system task that starts Quaderno automatically.

A snapshot contains the complete saved library: books, embedded original documents, writing, annotations, tables, formulas, charts, folders and reading positions. It also keeps a separate copy of local per-book reading preferences when present. Account credentials, PIN configuration and global device preferences are excluded.

Backups are published only after their files have been written successfully. Old versions are considered for removal only after a new backup succeeds and a retention setting has been supplied. Removal verifies ownership, file hashes and contents. Changed backups, foreign files, symlinks and directories containing extra files are retained. Cleanup is bounded, so an unusually large archive can temporarily contain more versions than the requested count. Failure to create a new backup does not trigger retention cleanup.

Keep a copy on another device or storage medium to protect against loss of the tablet. Local backup scheduling and wireless notebook synchronization are separate features.

## Recover a saved library

Each snapshot has this layout next to the local library file:

```text
<library-file>.backups/snapshot-<UUID>/
    library.quaderno
    metadata.json
    reader-settings.json   # present when local reading overrides exist
```

Copy the snapshot folder before moving or editing its files. On macOS, **Import notebooks** accepts `library.quaderno` and JSON notebook files. Import uses Quaderno’s existing merge behavior; it does not replace the entire current library blindly. Changed content can produce separate conflict copies.

Explicit local-file imports accept up to 256 MiB, matching the local library budget. Network and portable-transfer input retain their separate 64 MiB limit. A large local snapshot may therefore require a file copy instead of a wireless library transfer.

The standard notebook import restores document content without applying the separate `reader-settings.json` to another device. That file is retained for local recovery; there is no automatic account/PIN restoration or one-click reader-sidecar restoration.

## Inspect and clear the cache

The storage controls distinguish the library manifest, immutable objects containing document data, rebuildable document copies, and backups. **Refresh storage** updates these figures asynchronously. Original documents and annotations belong to library storage, not to the disposable cache.

The cache choices are 32, 64, 128 and 256 MiB. The chosen value bounds the shared rendered-page cache in memory; the same budget is applied separately to decoded PDF copies on disk as documents are rendered. It is not a limit on total application memory: the visible page, in-progress rendering and notebook data also occupy memory.

**Clear cache** removes recognized, verified document copies and clears reusable page images. It preserves embedded originals, ink, annotations, formulas, local reading settings and backups. The next requested page is rebuilt from its stored original. Unrecognized files and symlinks are left alone. If a very large old cache exceeds one operation’s cleanup budget, the status asks you to repeat the operation.

## Reading preferences for each book

Global reader settings provide defaults. A book can override zoom, reading margin, orientation and swipe behavior without changing another book. Resetting that book’s overrides returns it to the global defaults.

| Preference | Local book values |
| --- | --- |
| Zoom | Integer percentages from 100% to 300%, including a saved pinch zoom |
| Margin | None, small, medium or large |
| Orientation | App default, portrait, landscape right, upside-down portrait or landscape left |
| Swipe | Change page, vertical scrolling or no action |

Overrides are stored in `<library-file>.reader-settings.json`, separately from notebook content. They survive application restarts and notebook synchronization on the same device. They do not change the notebook’s transfer revision and are not sent to another device. Partial updates preserve the book’s other overrides. Corrupt or concurrently changed settings are not silently overwritten; notebook content remains editable.

## Implementation and verification

`LibraryMaintenance` exposes asynchronous `backup()`, `clearCache()` and `refreshStatus()` operations. Its `allowed` property gates mutations and scheduled starts. A separate maintenance lock serializes these operations. Backups read an atomic manifest and verify immutable objects without acquiring the live notebook writer lock; concurrent object collection causes a failed backup instead of blocking a handwriting save.

Automated tests cover backup import and retention, tampered files, cache limits and regeneration, local-only reading overrides, scheduling gates, preservation of handwriting during OCR insertion, and separation of local-file and network import limits. Physical verification of the 0.2.7 UI and installation is reported separately in the release validation records.
