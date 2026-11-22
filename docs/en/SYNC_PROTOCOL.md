# Incremental wireless sync

[Italiano](../PROTOCOLLO_SYNC.md)

`BleSync` retains the notebook preview, confirmation, baseline comparison, conflict-copy policy, and editor-busy guard. `DeltaTransfer` changes how the portable payload reaches that existing flow. It runs inside the same paired TLS 1.3 channel over local Wi-Fi or Bluetooth LE.

## Book annotations and page links

Highlights and underlines are saved as editable page strokes. They travel with the book's page identifiers, ink layers and visibility, bookmarks, typed items and retained PDF/EPUB sources. The same notebook transfer includes annotations on pages that are not currently open. Receiving an update refreshes the displayed ink and items while preserving the recipient's reading position when that page still exists. A hidden ink layer stays hidden after synchronization; its strokes are retained.

Anchored sticky notes introduced in 0.2.11 include the marker's page position, typed text, handwriting inside the note and note color. They use portable format **11** and require compatible Quaderno readers on both devices. Ordinary highlights in retained-source books use format 9. See [data format compatibility](DATA.md#library-format-versions).

Anchored page links introduced in **0.2.14** use portable format **12**. They carry the reference to the original chart, table or diagram. If the destination is in a different notebook, send that notebook as well or select it separately for automatic sync. Transfer the original notebook so the link can find its destination by ID.

UML shapes added in **0.2.23** require portable format **14**. The flowchart shapes added in **0.2.20** require format **13**. Both requirements apply to shapes stored in `handwritingSource.baseDiagram` for recovery. Before sending a notebook, the sender checks the format that notebook requires. [Data format compatibility](DATA.md#library-format-versions)

If an annotation has not appeared on the other device, check the connection and workflow before importing the book again:

1. Keep **Quaderno open and unlocked on both devices**. There is no background synchronization service when either app is closed.
2. Check that the correct book has **Sync this notebook** selected in the wireless panel. Selection is per notebook, not per library section.
3. Use **After changes** for saved highlighting to trigger a transfer after the writing pause. **On opening a notebook** does not trigger on each later annotation; reopen the selected book or use the manual preview/send action.
4. Review the first transfer, any pending preview, compatibility message or conflict. The last successful synchronization refers to the acknowledged notebook revision.
5. Open the synchronized copy. Importing the same PDF or EPUB independently creates a different book identity; matching titles do not merge annotations. Independent edits to the same synchronized book retain separate copies when they conflict.

This transfers annotations between **Quaderno** libraries. It does not write them into the Apple Books application's annotation database. Export an annotated PDF when you need a separate document copy.

The new protocol tests cover the reader-version handshake and rejection before asset transfer. Synthetic book tests cover highlights, underlines, hidden layers, multiple pages, reading position, sticky notes and reopening. Their execution and physical-device checks are recorded separately in the release validation; they do not establish live radio behavior.

<a id="automatic-synchronization-in-029"></a>

## Automatic synchronization

Bluetooth to Wi-Fi handoff requires 0.2.13 or later on both devices. Pair the devices and keep Quaderno open and unlocked on Mac and Paper Pro. Turn on Bluetooth on both devices. Connect Paper Pro to Wi-Fi and Mac to the same local subnet over Wi-Fi or Ethernet. In **Settings → Wi-Fi** or **Bluetooth**, select **Automatic (Bluetooth and Wi-Fi)** on both devices. Choose when to sync on each device and select the notebooks you want to send. A device can receive with an empty selection.

| Mode | Behavior |
| --- | --- |
| Manual | No automatic connections or notebook batches. This remains the default. |
| After changes | Coalesce saved edits and wait about 1.7 seconds without another edit before processing selected notebooks. Writing and open editors postpone the work. |
| On opening a notebook | Request a bounded batch when opening a selected notebook. Later changes wait for another trigger. |
| At regular intervals | Request a bounded batch every 5, 15, 30 or 60 minutes. |

When automation is enabled, the peer reconnects or the selection changes, Quaderno also checks selected notebooks for pending work. Multiple triggers are coalesced. Matching shared revisions need no data transfer; received changes do not need to be sent straight back.

### Bluetooth to Wi-Fi handoff

1. Mac opens the Bluetooth LE connection and authenticates it with the pairing secret. The apps exchange two booleans over TLS that indicate pending work and Wi-Fi permission. With empty queues, Bluetooth stays connected to check for later changes.
2. If either queue has work and both apps permit Wi-Fi, Paper Pro sends its current Wi-Fi IPv4 address. Mac tries fixed TCP port `47987` using a local Wi-Fi or Ethernet interface on the same subnet. Quaderno uses the existing network and exchanges no credentials, SSIDs, notebook names or edits during this check.
3. The second TLS session derives its key from the pairing secret with a label that includes the Bluetooth handoff session ID. The `ready` and `commit` messages confirm the session before enabling file transfer over Wi-Fi. Bluetooth stays connected as a fallback. Quaderno uses the Wi-Fi session for as long as it is available.
4. If the handoff fails or exceeds 6 seconds, the batch continues over Bluetooth. A later batch can try Wi-Fi again after both queues have settled. If Wi-Fi drops after handoff, recovery reuses saved chunks and prepares a fresh preview when possible. An uncertain apply result requires manual review.

The `syncRendezvous` control messages use version `1` JSON metadata and an empty payload. Each message includes a `phase` and a `session` of 32 lowercase hexadecimal digits generated by Mac. The `hello` and `work` phases add only the `work` and `wifi` booleans to those common fields. `offer` adds only `address`, a canonical local IPv4 address. `ready`, `commit` and `abort` use the common fields. The Paper Pro USB subnet is excluded.

An older app can still sync over Bluetooth using the existing protocol. The discovery window for handoff support is up to 4 seconds. If the Bluetooth connection fails, Mac can try direct Wi-Fi using the saved address, and Paper Pro can accept a compatible Wi-Fi connection while waiting. **Wi-Fi only**, **Bluetooth only** and manually started connections keep their existing workflows. **Disconnect** pauses reconnection for the session and **Resume connection** permits it again.

Trusted SSIDs constrain automatic Wi-Fi. An empty list allows any local SSID. With a configured list, the network must match and the system must be able to read its SSID. **Only while charging** applies to both connections on Paper Pro. PIN lock, sleep, notebook import, drawing and open editors close the automation gate. An apply already committing finishes its atomic operation before further work is paused.

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

`tests/syncrendezvous_tests.cpp` checks handoff message shapes, field types and allowed addresses. Handoff tests use test connections. The handoff still requires a physical test with the Mac and Paper Pro radios.
