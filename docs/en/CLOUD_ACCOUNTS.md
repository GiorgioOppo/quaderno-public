# Cloud accounts

[Italiano](../CLOUD_ACCOUNTS.md)

Quaderno 0.2.7 includes direct Dropbox, Google Drive and OneDrive connections. The account panel offers **Sign in with Google**, **Sign in with Dropbox** and **Sign in with Microsoft**. Passwords are entered only on the provider's official page.

The connection code is implemented and covered by offline protocol tests. **No production OAuth registrations are bundled, and this release work has not signed in to real provider accounts.** A maintainer must supply registrations owned by this project before the corresponding SSO button can authenticate. Missing registration, unavailable private storage and failed authorization are shown as unavailable or errors; a website opening never counts as a successful connection.

## What the connection does

- Browse folders and load additional result pages, up to 2,000 entries in the current folder.
- Download PDF, EPUB, Markdown, CSV, PNG, JPEG, WebP and `.quaderno` files over HTTPS, then use Quaderno's normal import flow. Portable notebooks require an import preview and confirmation.
- Upload a local file on macOS or export the current notebook as a new `.quaderno` copy on either device.
- Refresh an expiring access token, remove an account from this device, and request provider-side revocation where supported.
- Cancel an operation. PIN locking also aborts requests, closes the OAuth callback listener and clears transient login codes and file listings.

Transfers are explicit operations, not automatic two-way synchronization. Existing cloud files are never deleted. Dropbox uploads use add/rename semantics, Google creates a new file, and OneDrive rejects name conflicts. Quaderno does not merge remote changes or replace the source document.

The transfer limit is **100 MiB per file**, with **64 MiB for `.quaderno`**. Format-specific import limits still apply. Google Workspace-native documents are not converted or exported by this connection; download a supported format first. Cloud storage quota, organizational policies, sharing permissions and service availability can also restrict operations.

## Maintainer setup: register the applications

Registration is installation configuration, separate from the ordinary account flow. Do not borrow client IDs from other applications, commit tokens or distribute a confidential application's secret in a desktop build.

### Google Drive

Create a Google Cloud project, enable the Drive API, configure its OAuth consent screen and create a **Desktop app** OAuth client. The implementation uses authorization code with PKCE S256 and a temporary `http://127.0.0.1:<port>/oauth/callback` listener. Google's obsolete out-of-band copy/paste flow is not used. The optional `clientSecret` field is for the matching Desktop registration when needed by that registration. [Google desktop OAuth documentation](https://developers.google.com/identity/protocols/oauth2/native-app).

Requested scopes are `https://www.googleapis.com/auth/drive.readonly` and `https://www.googleapis.com/auth/drive.file`: read existing accessible files and create app-owned copies. Read access is a restricted Drive scope, so public distribution can require provider review. Configure test users and publishing status in the project's consent settings before expecting a login to work. Google device authorization does not offer the same existing-file browsing scope, so the tablet uses the desktop loopback flow through SSH instead. [Drive scopes](https://developers.google.com/workspace/drive/api/guides/api-specific-auth), [device authorization scope limitations](https://developers.google.com/identity/protocols/oauth2/limited-input-device).

### Dropbox

Register a scoped app in the [Dropbox developer console](https://www.dropbox.com/developers/apps), allow public clients and enable `files.metadata.read`, `files.content.read` and `files.content.write`. Choose Full Dropbox access to browse existing files outside the application's folder; an App Folder registration limits visibility to that folder.

Quaderno uses the public app key as `clientId`, PKCE S256 and `token_access_type=offline`. It omits `redirect_uri`, so Dropbox displays a one-use authorization code. Paste that code into Quaderno, or into the Mac SSH assistant's hidden prompt. A Dropbox app secret is not used. [Dropbox OAuth guide](https://docs.dropboxapi.com/dropbox-api/docs/oauth).

### Microsoft OneDrive

Register an application in Microsoft Entra, select the account audience you intend to support, and enable **Allow public client flows**. Supply its Application/client ID; no client secret is required. The tenant value is `common`, `consumers`, `organizations`, or the registration's tenant GUID. Requested delegated scopes are `Files.ReadWrite` and `offline_access`.

Quaderno obtains a device code, shows the user code and waits while the user signs in on Microsoft's page. Polling follows the returned interval and expiration, including slowdown responses. Tenant policies may disallow device authorization; Quaderno reports the failure instead of bypassing it. [Microsoft device flow](https://learn.microsoft.com/en-us/entra/identity-platform/v2-oauth2-device-code), [public-client configuration](https://learn.microsoft.com/en-us/entra/identity-platform/scenario-desktop-app-configuration).

## Install the registration configuration

Create a private JSON file with only the providers configured for this installation. Replace the placeholders with registrations you own:

```json
{
  "googleDrive": {
    "clientId": "YOUR_DESKTOP_CLIENT_ID.apps.googleusercontent.com"
  },
  "dropbox": {
    "clientId": "YOUR_DROPBOX_PUBLIC_APP_KEY"
  },
  "oneDrive": {
    "clientId": "YOUR_MICROSOFT_APPLICATION_ID",
    "tenant": "common"
  }
}
```

The destination is `oauth-registration.json` inside `QStandardPaths::AppConfigLocation/cloud`. On Paper Pro this is `/home/root/.config/Quaderno/cloud/`; on macOS it is normally `~/Library/Preferences/Quaderno/cloud/`. The directory must be private (`0700`) and the file owner-readable/writable only (`0600`). The account panel never asks ordinary users to type these developer settings.

To configure Paper Pro from a Mac, with the tablet's SSH access enabled:

```sh
chmod 600 /private/path/oauth-registration.json
python3 scripts/cloud-auth-paper-pro.py --host 10.11.99.1 \
  --registration /private/path/oauth-registration.json
```

Use the tablet's private Wi-Fi address instead of `10.11.99.1` for wireless setup. SSH verifies the host normally; the helper does not disable host-key checks. Enter the SSH password in Terminal if requested, never in chat. The helper verifies that the remote machine identifies as Paper Pro before writing configuration. Reopen the account dialog afterward.

On macOS, install the same registration JSON privately in the local configuration directory. No registration helper starts a provider login by itself, and no account password belongs in this file.

## Sign in on Paper Pro

1. Open Settings → Accounts, choose a configured service, and press its SSO button.
2. On the Mac, explicitly start the assistant:

   ```sh
   python3 scripts/cloud-auth-paper-pro.py --host 10.11.99.1
   ```

3. Complete the provider's page in the browser. For Microsoft, enter the code displayed on Paper Pro. For Dropbox, paste the provider's authorization code into the assistant's hidden Terminal prompt or into the tablet's code field.
4. Check Quaderno's result and file list. The assistant ending means the request ended, which can also happen after cancellation or failure.

Google's temporary SSH forward connects the Mac's loopback callback port to the tablet's loopback listener. The tablet exchanges the code with Google and stores the tokens locally. The helper reads only expiring authorization metadata; it never reads access or refresh tokens. Opening a tablet-generated Google URL on another device without this forward cannot complete the callback.

`--control /path/to/socket` optionally reuses an already authenticated SSH control socket. The assistant removes only its own port forward and never closes a reused SSH master. After authentication, normal cloud transfers run directly from the Paper Pro over HTTPS and do not require the Mac or a cable.

On macOS, pressing SSO opens the official browser directly. Google returns to the local app automatically; Dropbox still supplies a code; Microsoft uses its displayed device code.

## Storage, cancellation and revocation

macOS stores tokens in Quaderno's Keychain item. Keychain queries disable authentication popups during startup; inaccessible storage fails visibly and never falls back to plaintext. Paper Pro stores tokens atomically in a `0600` file inside its `0700` configuration directory. This protects filesystem access by other users, but does not encrypt the tablet's filesystem or prevent access by root. Cloud credentials are separate from notebook exports and local library snapshots.

A nonblocking private lock prevents concurrent Quaderno processes from replacing each other's saved credentials or OAuth handoff. Token and authorization responses are never copied into status messages, logs, or notebook data. The browser handoff has a short expiration and is removed on completion, cancellation or PIN lock. Completed downloads are private staging files. The import flow removes its tracked file after cancellation or completed processing; asynchronous imports retain it until processing finishes. Cleanup rejects foreign paths and symlinks and does not recursively delete directories.

Local disconnect removes this device's saved credentials without changing cloud files. Dropbox and Google also expose explicit remote revocation. Google's revocation may affect other clients/scopes belonging to the same project, so it requires a separate choice in the confirmation. Microsoft session-wide revocation is deliberately not called: use Microsoft's account permissions page to revoke that application. [Dropbox revoke endpoint](https://docs.dropboxapi.com/dropbox-api/api-reference/user-endpoints/auth/token-revoke), [Google token revocation](https://developers.google.com/identity/protocols/oauth2/limited-input-device#tokenrevoke), [Microsoft session revocation scope](https://learn.microsoft.com/en-us/graph/api/user-revokesigninsessions?view=graph-rest-1.0).

## Verification and remaining live checks

`cloud_tests` uses a fake network manager and private temporary stores; it does not contact providers or use the user's Keychain. It covers PKCE, callback state, device polling, refresh token rotation, cancellation, stale callbacks, response limits, permissions, storage locks, path validation, upload conflict policy and redirects without bearer-header forwarding. `python3 tests/cloud_helper_tests.py` verifies handoff validation and SSH cleanup entirely offline.

Before claiming a provider connection is production-ready, test an owned registration with a real account: initial consent, refresh after expiry, folder pagination, one download, one new-copy upload, cancellation and revocation/local disconnect. Verify Google through the tablet SSH helper as well as the Mac callback, and verify macOS Keychain access for the actual signed app. These live account checks remain pending until registrations and user authentication are supplied.
