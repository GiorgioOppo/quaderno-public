# Install Quaderno

**English** · [Italiano](../INSTALLAZIONE.md)

Quaderno runs on Mac and as a temporary preview on the reMarkable Paper Pro. The tablet build targets model `ferrari` on firmware **3.29.1.0**. AppLoad startup, native Marker events, and regional and manual display refresh were checked on a physical tablet. [Release and test status](RELEASE.md)

| Platform | Package | Requirements |
| --- | --- | --- |
| Mac | `Quaderno-macOS.zip` | Apple silicon and macOS 27.2 or later for the current preview package. Handwriting transcription in source builds requires macOS 27 or later. |
| reMarkable Paper Pro | `Quaderno-Paper-Pro-3.29.1.0.zip` | Model `ferrari`, firmware 3.29.1.0, and Developer Mode. |

If you already have the package for your platform, follow the steps below. To build a package from source, see the [development guide](DEVELOPMENT.md).

## macOS

1. Extract `Quaderno-macOS.zip` in Finder.
2. Move `Quaderno.app` to **Applications** and open it.
3. Create a test notebook. Close the app, reopen it, and check that you can open the notebook again.

The Mac preview uses an ad hoc signature. If macOS reports an unidentified developer, check where you obtained the ZIP. Apple explains how to allow an individual app in **System Settings → Privacy & Security**. [Safely open apps on your Mac](https://support.apple.com/en-au/102445)

To update, quit Quaderno and replace `Quaderno.app` in **Applications**. The app bundle and library live in separate directories. For help backing up `quaderni.json` and your settings, see [Data and backups](DATA.md).

## Paper Pro over USB‑C

### Prepare the tablet

> [!IMPORTANT]
> Enabling Developer Mode resets the Paper Pro to factory settings and reduces system protections. Sync or export your documents first and check that you can open the copies. To leave Developer Mode later, follow reMarkable's official recovery procedure. [reMarkable Developer Mode](https://developer.remarkable.com/documentation/developer-mode)

1. Check the tablet model and full firmware version before running the installer. This guide uses the package for **reMarkable Paper Pro 3.29.1.0**. The script checks the model, architecture, and dependencies.
2. If Developer Mode is already enabled, connect the tablet over USB. To enable it, follow the reMarkable guide for **Settings → General → Paper tablet → Software → Advanced → Developer Mode**. Labels may vary with the system language. [Enabling Developer Mode](https://developer.remarkable.com/documentation/developer-mode#enabling-developer-mode)
3. Turn on and unlock the tablet, then connect it to your Mac with a USB‑C data cable. On the Paper Pro, find the SSH credentials under **Settings → General → Help → About → Copyright and licenses**. Use the SSH password shown there. [Using SSH in Developer Mode](https://developer.remarkable.com/documentation/developer-mode#using-developer-mode)

Check the connection from your Mac. On the first connection, SSH displays the tablet's host key fingerprint. Verify the tablet's identity before accepting it.

```sh
ssh root@10.11.99.1
```

Run `exit` to return to the Mac terminal.

### Install and start Quaderno

Extract `Quaderno-Paper-Pro-3.29.1.0.zip` and open Terminal in the extracted directory. Keep `quaderno` and the scripts together in that directory.

To enable transcription on the tablet, keep `quaderno-handwriting` and `quaderno-handwriting-it.dct` alongside `quaderno`. The installer transfers and verifies both files when present.

```sh
sh install-paper-pro.sh
```

Enter the SSH password in Terminal when prompted. Password input may be invisible. The script transfers the files, checks the AArch64 loader, libraries, and e‑ink plugins, then prints the installation path and the command to start another preview session.

To install and run the checks while leaving the reMarkable interface open, use this option.

```sh
sh install-paper-pro.sh --install-only
```

Each successful installation creates a `/home/root/quaderno/installation-…` directory on the data partition. Keep the command printed by the installer to start that build again. On the tablet, `try-on-device.sh --check` verifies the preview prerequisites and `try-on-device.sh --stop` ends an active session.

### Connect Obsidian Sync

Direct sync requires an active Obsidian Sync subscription and an existing remote vault. Each Paper Pro connects to one vault and keeps a separate local copy. Back up the vault before connecting the tablet. Quaderno uses the official Obsidian Headless client, which is currently in beta. [Headless Sync documentation](https://obsidian.md/help/sync/headless)

1. Install the updated Quaderno build. Connect Paper Pro over USB‑C and join a Wi‑Fi network with internet access on the tablet. On Mac, open Terminal in the extracted package directory and run this command. The installer downloads Node.js, Obsidian Headless, and their dependencies separately, then verifies their checksums before transferring them to the tablet.

   ```sh
   sh obsidian-sync-paper-pro.sh install
   ```

2. Run the interactive setup in Terminal. Enter your Obsidian credentials and two-factor code when prompted. Select the remote vault using the ID shown by the client. For an end-to-end encrypted vault, enter the vault password when prompted. If you already know the ID, you can pass it as an argument after `setup`.

   ```sh
   sh obsidian-sync-paper-pro.sh setup
   ```

3. Open Quaderno on the tablet and choose **Markdown and Obsidian → Obsidian Sync → Sync now**. After setup, Paper Pro uses its own Wi‑Fi connection to sync with the vault. [Obsidian Sync user guide](USER_GUIDE.md#obsidian-sync-on-paper-pro)

Obsidian Headless 0.0.14 may ask for a password for a managed-encryption vault, which has no vault encryption password. [Obsidian Headless issue](https://github.com/obsidianmd/obsidian-headless/issues/51)

### Return to reMarkable

Tap **Esci** (Exit) in Quaderno. The temporary `quaderno-demo.service` stops `xochitl` during the preview and requests a restart when Quaderno exits. A session can last up to ten minutes. If USB disconnects, the service continues until the app closes or the session expires.

To end the preview from the Mac, reconnect the tablet if necessary and run the following command.

```sh
ssh root@10.11.99.1 'systemctl stop quaderno-demo.service'
```

If the reMarkable interface is still missing and SSH responds, check both services. Start `xochitl` once you have confirmed that the preview session has ended.

```sh
ssh root@10.11.99.1 'systemctl status quaderno-demo.service xochitl.service --no-pager'
ssh root@10.11.99.1 'systemctl start xochitl.service'
```

If the tablet fails to boot or you want to leave Developer Mode, follow the official Paper Pro recovery procedure. Back up any documents you can still access before starting recovery. [reMarkable Recovery Mode](https://developer.remarkable.com/documentation/recovery-mode)

### Start from the AppLoad menu

This option requires a working installation of XOVI and AppLoad on the tablet. Quaderno registers its own menu entry. Consult the extension documentation for your model and firmware before setting them up. [XOVI](https://github.com/asivery/rm-xovi-extensions#to-install-xovi) · [AppLoad](https://github.com/asivery/rm-appload#external-applications)

Run the following from the extracted package directory on your Mac.

```sh
sh install-paper-pro.sh --appload
```

On the tablet, open **AppLoad**, tap **Reload** if the menu was already open, and select **Quaderno**. This starts the same temporary session as the USB installer. The entry works while XOVI is active. If you run XOVI in temporary mode, activate it again after rebooting the tablet.

To remove the entry, use the removal command printed by the installer, then tap **Reload** in AppLoad. The installed binary and your notebooks stay on the tablet.

### Update or return to a previous build

1. Exit the preview session and back up your library. [Data and backups](DATA.md)
2. Check the firmware version and use the matching package. Install the new build with `--install-only` for a USB preview or `--appload` to update the menu entry too.
3. Keep the previous installation directory and its launch command until you have tested the new build. To start the previous build, use the command printed when you installed it. To reconnect it to AppLoad, run `install-appload-quaderno.sh` from the installation created with `--appload` and pass the full path to the previous `installation-…` directory on the tablet.

Before opening your library with an earlier build, check that it can read the format used by the newer version. Keep a separate copy of the updated library if you restore an earlier backup.

### Uninstall Quaderno

On Mac, quit Quaderno and move `Quaderno.app` to the Trash. On the Paper Pro, end the preview and remove the AppLoad entry, if present, with the command printed by the installer. You can then delete the build directory at `/home/root/quaderno/installation-…`. The library lives in the data directory. [Data and backups](DATA.md)

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| SSH times out | Use a data cable, try another USB port, and make sure the tablet is on and unlocked. Check Developer Mode and retry `ssh root@10.11.99.1`. |
| SSH denies access | Check the password shown under **Copyright and licenses** on the tablet. |
| SSH reports that the host key changed | Check that the USB address belongs to your tablet and whether the tablet was reset. Reconnect after you have verified the new fingerprint. |
| The library or e‑ink plugin check fails | Check the firmware, model, and ZIP integrity. For a different firmware version, build with the matching SDK and verify that combination before starting a preview. |
| AppLoad shows an old entry or cannot find Quaderno | Check that XOVI and AppLoad are active, rerun the installer with `--appload`, and tap **Reload**. |
| Quaderno closes on the tablet | Check the preview log. If you launched it from AppLoad, also check the launcher service. |

```sh
ssh root@10.11.99.1 'journalctl -b -u quaderno-demo.service -u quaderno-launch.service -n 80 --no-pager'
```

Once Quaderno is installed, see the [user guide](USER_GUIDE.md).
