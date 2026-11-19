# Install Guide

For the Samsung Galaxy A50 (SM-A505F, a50dd) only.
The installer refuses a50s, A507 and any other model.

## Before you start

- The bootloader must be unlocked.
- You must already have a working TWRP.
- Battery must be above 50 percent.
- Keep a copy of your current recovery. You need it to go back.
- Back up your data.

## Install

1. Download `TWRP_hybrid_A50.zip` and `SHA256SUMS.txt` from Releases.
2. Check the file (see below).
3. Copy `TWRP_hybrid_A50.zip` to the SD card or internal storage.
4. Boot to TWRP.
5. Tap Install. Pick the zip. Swipe to flash.
6. Wait for the "done" message.
7. Tap Reboot, then Recovery.
8. The screen should show `3.7.1_12-0-UNOFFICIAL-@ibralyam`.

Do not flash the outer `twrp-hybrid-a50.zip` from GitHub Actions.
It has no META-INF at the top level. TWRP shows "Invalid zip format".
Unpack it and flash the inner `TWRP_hybrid_A50.zip`.

## Check your download

```
sha256sum TWRP_hybrid_A50.zip
```

The value must match `SHA256SUMS.txt`.

## What the installer does

1. Checks the device (a50 or a50dd, model SM-A505).
2. Checks every file in the zip (CRC32 and SHA-256).
3. Decodes the image once without writing. It must be a valid boot image of 64 MiB or less.
4. Backs up the current recovery to `/tmp`.
5. Writes the new recovery and reads it back.
6. Writes `vbmeta.img` if the vbmeta partition exists.
7. Restores the backup if the write check fails.

If a check fails before step 4, nothing is written.

## Go back

Flash your old recovery again. Use the old TWRP zip, or Odin with your old recovery file.

## Next step

Keep your ROM installers as they are. They now find `/system_root`, `xzcat`, `unxz` and `busybox` in recovery.
