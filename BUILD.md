# Build Guide

The build runs in GitHub Actions. It takes about 30 minutes.

## Steps

1. Fork or clone this repo.
2. Open the Actions tab.
3. Pick "Build hybrid TWRP for Galaxy A50".
4. Tap Run workflow.
5. Download the `twrp-hybrid-a50` artifact when it ends.

The artifact is a zip. Inside it:

```
TWRP_hybrid_A50.zip   flash this one
recovery.img.xz
SHA256SUMS.txt
```

## What the workflow does

1. Runs the 15 installer self tests.
2. Builds static liblzma 5.4.5 for arm64.
3. Builds the applet.
4. Syncs the `twrp-12.1` sources.
5. Applies `patches/0001-hybrid-twrp-a50.patch`.
6. Builds `recoveryimage` for `twrp_a50-eng`.
7. Fails if `recovery.img` is over 67108864 bytes.
8. Checks the ramdisk (tools, rc lines, `/system_root`, version string).
9. Compresses, bakes the hashes into the installer, and makes the zip.

If a step fails, the `ramdisk-listing` artifact lists the ramdisk files.

## Local build

Build the installer for arm64:

```
aarch64-linux-gnu-gcc -static -O2 -s -fvisibility=hidden -Wall -Wextra \
  -I$XZ/include \
  -DEXPECT_XZ_SHA256='"<sha256 of recovery.img.xz>"' \
  -DEXPECT_RAW_SHA256='"<sha256 of recovery.img>"' \
  -DEXPECT_VBMETA_SHA256='"<sha256 of vbmeta.img>"' \
  -o update-binary flasher/twrp_flasher.c $XZ/lib/liblzma.a
```

Without the three `-D` values, the same source builds only the applets.

To pack a zip from an image you already have:

```
RECOVERY_IMG=recovery.img LZMA_A=/path/liblzma.a LZMA_INC=/path/include bash ci/pack.sh
```

## Change the name

Edit `TW_DEVICE_VERSION` in `patches/0001-hybrid-twrp-a50.patch`. Then rebuild.
The splash and the home screen show the same string.
