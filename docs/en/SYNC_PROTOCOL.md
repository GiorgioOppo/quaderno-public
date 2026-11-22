# Incremental wireless sync

[Italiano](../PROTOCOLLO_SYNC.md)

`BleSync` retains the notebook preview, confirmation, baseline comparison, conflict-copy policy, and editor-busy guard. `DeltaTransfer` changes how the portable payload reaches that existing flow. It runs inside the same paired TLS 1.3 channel over local Wi-Fi or Bluetooth LE.

## Book annotations and reader compatibility in 0.2.11

Highlights and underlines are saved as editable page strokes. They travel with the book's page identifiers, ink layers and visibility, bookmarks, typed items and retained PDF/EPUB sources. The same notebook transfer includes annotations on pages that are not currently open. Receiving an update refreshes the displayed ink and items while preserving the recipient's reading position when that page still exists. A hidden ink layer stays hidden after synchronization; its strokes are retained.

Anchored sticky notes introduced in 0.2.11 include the marker's page position, typed text, handwriting inside the note and note color. They use portable format **11** and require compatible Quaderno readers on both devices. Ordinary highlights in retained-source books still use format 9; adding an annotation does not require format 11 unless it uses the new note type. See [data format compatibility](DATA.md#library-format-versions).

If an annotation has not appeared on the other device, check the connection and workflow before importing the book again:

1. Keep **Quaderno open and unlocked on both devices**. There is no background synchronization service when either app is closed.
2. Check that the correct book has **Sync this notebook** selected in the wireless panel. Selection is per notebook, not per library section.
3. Use **After changes** for saved highlighting to trigger a transfer after the writing pause. **On opening a notebook** does not trigger on each later annotation; reopen the selected book or use the manual preview/send action.
4. Review the first transfer, any pending preview, compatibility message or conflict. The last successful synchronization refers to the acknowledged notebook revision.
5. Open the synchronized copy. Importing the same PDF or EPUB independently creates a different book identity; matching titles do not merge annotations. Independent edits to the same synchronized book retain separate copies when they conflict.

This transfers annotations between **Quaderno** libraries. It does not write them into the Apple Books application's annotation database. Export an annotated PDF when you need a separate document copy.

The new protocol tests cover the reader-version handshake and rejection before asset transfer. Synthetic book tests cover highlights, underlines, hidden layers, multiple pages, reading position, sticky notes and reopening. Their execution and physical-device checks are recorded separately in the release validation; they do not establish live radio behavior.

## Automatic synchronization in 0.2.9

The 0.2.9 development workflow requires both Quaderno apps to be updated, open and unlocked, with a saved pairing and notebooks selected for synchronization. It does not run when Quaderno is closed or suspended and does not use an Internet relay or cloud account. Mac and ARM builds, six affected suites per platform and strict rendered startups passed. Version 0.2.9 is installed on Paper Pro and passed native e-paper/AppLoad startup at 1620 × 2160. The user confirmed the visible after-changes mode and three connection choices on Paper Pro. End-to-end Wi-Fi/Bluetooth transfers remain untested; the installed Mac 0.2.0 app remains unchanged. A separate Mac 0.2.9 package is ready, but it has not replaced that installation.

In **Settings → Wi-Fi** or **Bluetooth**, open **Automatic synchronization** and choose when to send changes:

| Mode | Behavior |
| --- | --- |
| Manual | No automatic connections or notebook batches. This remains the default. |
| After changes | Coalesce saved edits and wait about 1.7 seconds without another edit before processing selected notebooks. Writing and open editors postpone the work. |
| On opening a notebook | Request a bounded batch when opening a selected notebook. Later changes wait for another trigger. |
| At regular intervals | Request a bounded batch every 5, 15, 30 or 60 minutes. |

When automation is enabled, the peer reconnects or the selection changes, Quaderno also checks selected notebooks for pending work. Multiple triggers are coalesced. Matching shared revisions need no data transfer; received changes do not need to be sent straight back.

Choose **Prefer Wi-Fi, otherwise Bluetooth**, **Wi-Fi only** or **Bluetooth only**. The automatic choice prefers permitted local Wi-Fi and can try the paired Bluetooth peer when Wi-Fi is unavailable. On Paper Pro, automatic mode can listen on both permitted transports while Bluetooth is already enabled, then select the first accepted connection. On Mac it attempts Wi-Fi first and can try Bluetooth after Wi-Fi fails. An authenticated connection is retained until it ends, including when Wi-Fi later becomes available; automatic preference does not interrupt it to switch transports. Wi-Fi uses the saved local IPv4 address on Mac; a changed tablet address may still require correction in the connection panel. Bluetooth must already be enabled: automatic recovery does not activate a disabled radio. A manual **Disconnect** pauses reconnection for the session; **Resume connection** permits it again.

Trusted SSIDs constrain automatic Wi-Fi only, including when Wi-Fi is the preferred transport. An empty list means no SSID restriction; a configured list with an unavailable or different SSID blocks automatic Wi-Fi. Bluetooth remains subject to pairing and the selected transport policy. **Only while charging** applies to both connections on Paper Pro. PIN lock, sleep, notebook import, drawing and open editors close the automation gate immediately. An apply already committing finishes its atomic operation before further work is paused.

Connection retries wait 3 seconds on Paper Pro or 8 seconds on Mac initially, then increase the wait up to 120 seconds. They continue only while automatic reconnection is permitted. Transfer recovery allows at most three additional attempts for the same revision, after 2, 4 and 8 seconds, for recognized transient failures before apply. This budget survives reconnects and policy pauses in the running app; it resets for a new source revision, an explicit off/on change or an app restart. Automatic interval/catch-up triggers do not reset an exhausted budget. Manual retry opens a fresh review. Scheduled batches contain at most 20 selected notebooks and preserve their requested revisions across restarts. New explicit triggers merge the latest requested revisions into the queue. A saved checkpoint allows safely interrupted automatic work to prepare a new preview after reconnection or reopening. If the pre-apply checkpoint cannot be saved, Quaderno does not send the apply request. Matching received chunks are reused; an earlier approval is never replayed. Canceling, disabling automation or removing a notebook from the selection must not leave an active retry for that work. An uncertain apply result requires manual review. This includes pending transfers from older versions that lack a saved phase or valid revision, and interrupted manual applies: automatic catch-up must not resend them. A fresh manual preview remains available; the final migration/recovery checks passed on Mac and ARM (11 checks per platform). The first transfer also requires review; later conflicts follow **Ask** or the explicitly selected **Keep both** policy, which creates a separate copy rather than overwriting an independently changed notebook.

The wireless panel reports the active connection, pause or retry state, pending work and each notebook’s last successful synchronization. Manual notebook transfer remains available. USB-C file transfer, Obsidian Sync and provider cloud imports retain their separate workflows; these automation controls do not turn them into a combined cloud sync service.

## Negotiation and compatibility

Connected peers exchange `syncHello` / `syncHelloReply` with `deltaVersion: 1`. The reply echoes the initiating 32-character request identifier. Starting with 0.2.11, both messages also advertise `maxNotebookVersion`, using the same ceiling as the local storage reader. The sender checks the portable version and extended fields before sending a manifest or asset bytes. An incompatible reader receives no book content, and the sender reports the required format and asks for an update; local annotations remain saved.

For historical compatibility, a delta-v1 peer without this field is treated as supporting format 9, the common baseline of the earlier incremental readers. This is conservative for 0.2.10, which also omitted the field: update both peers to exchange its format-10 extensions through this capability check. An explicitly malformed capability fails closed. After 1.5 seconds without delta support, the sender falls back to the earlier full-payload `preview` protocol with a format-8 ceiling and the existing 8 MiB Bluetooth or 64 MiB Wi-Fi limit. Source-backed books and newer item metadata are not downgraded or stripped to pass that limit.

New `deltaOffer` messages carry `notebookVersion` as well. A receiver rejects an unsupported declared format before requesting assets. Notebook schema validation remains mandatory after assembly, including for older offers without this field.

With incremental sync, the reconstructed portable JSON remains limited to 64 MiB. Individual chunk payloads are at most 64 KiB. Large original documents still count toward the portable limit even if already present on the receiver.

## Manifest and objects

`SyncPackage::split` produces a deterministic `quaderno-objects-v1` manifest and immutable objects identified by SHA-256. A root descriptor records the library metadata and ordered page references. Each page has its own descriptor. Strings of 4,096 or more characters, including page backgrounds, embedded images and original PDF/EPUB data, are separated into shared asset objects. Repeated asset contents are stored once. Formulas, ink, typed text, item properties and page identifiers remain intact.

The manifest contains every object's hash and byte size, the root hash, and the reconstructed payload's hash and size. The parser bounds the manifest, object count, nesting, and aggregate size. Object keys are hexadecimal hashes, never paths supplied by the peer. Assembly verifies all referenced object hashes and the final canonical JSON hash before handing the payload to `Notebook::receiveBookTransfer`.

## Transfer and interruption

1. The sender sends `deltaOffer` with the manifest and the usual request, book ID and shared baseline.
2. The receiver seeds its cache from matching content in the current local notebook and checks existing complete and partial objects.
3. `deltaNeed` requests a missing hash at a byte offset. Each request also acknowledges the preceding durable write. The sender answers with a `deltaChunk` containing the offset and a chunk hash.
4. The receiver validates the range and hash, appends to a partial file, flushes and synchronizes it before requesting the next part. Completed objects are checked against their full SHA-256 and committed atomically. Duplicate chunks are compared with stored bytes.
5. After assembly, the ordinary preview reply returns the action and token. The sender must still confirm the apply. The destination revalidates the token and editor state.

Partials are keyed by content hash and persist across reconnects and app restarts. The sender remembers the pending notebook, then prepares a **new preview** on reconnect. It never replays a previous approval. Changed source content receives a new manifest and only matching cached objects are reused. The user can also select **Resume transfer**. A chunk acknowledgment timeout retries at most three times before leaving the transfer resumable.

Applied request/token receipts are stored per peer before replying. Repeating an acknowledged apply returns its receipt without reapplying the notebook. A process interruption between the notebook commit and receipt storage can still leave the sender uncertain; reconnect requires a fresh preview and does not automatically replay the apply.

## Cache and tests

Objects and partials live under the app-local `sync-objects/<paired-peer-id>` directory with owner-only permissions. They are scoped to the paired peer, retained for at most 30 days when unused, and pruned toward a 256 MiB cache budget between transfers. **Forget pairing** removes the peer's cache and preferences. Active transfer objects are retained until completion or the next offer, so temporary disk usage can exceed the cache target by one transfer.

`tests/sync_tests.cpp` covers exact reconstruction, multi-book and empty libraries, asset deduplication, formula preservation, changed-page-only transfer, reuse of a local version, resuming an interrupted transfer after destroying and recreating both endpoints, malformed manifests, corrupt chunks, and legacy fallback. Those tests use isolated temporary caches and a queued in-process transport; they do not prove physical Bluetooth throughput or radio reconnect behavior.
