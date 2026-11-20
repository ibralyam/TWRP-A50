# Hybrid TWRP for Galaxy A50

TWRP 3.7.1_12 for the Samsung Galaxy A50 (SM-A505F, a50dd, Exynos 9610).
It keeps the modern 3.7.1 features. It adds what old installers expect from TWRP v3.
It also ships with its own installer, written in C, that checks everything before it writes.

Version string: `3.7.1_12-0-UNOFFICIAL-@ibralyam`

This is not a renamed copy of another build. The next sections show what is inherited and what is new.

## Where each part comes from

| Part | Source |
| --- | --- |
| TWRP code, EROFS support, modern decryption, Mint kernel compatibility, AVB `vbmeta.img` | Inherited from the base: TeamWin TWRP and Peter's `twrp-12.1` tree |
| Everything in the "New in this build" lists | Written for this project by ibralyam |

## Three recoveries side by side

| Feature | TWRP v3 (3.7.0_9) | Peter's 3.7.1_12 | Hybrid |
| --- | :---: | :---: | :---: |
| TWRP base version | 3.7.0_9 | 3.7.1_12 | 3.7.1_12 |
| EROFS support | no | yes | yes |
| Modern decryption | no | yes | yes |
| `/system_root` mount point | yes | no | yes |
| `/system_root` for both ext4 and EROFS | no | no | yes |
| `xzcat` and `unxz` in recovery | no | no | yes |
| `busybox` applet in recovery | no | no | yes |
| `/sbin/sh` | yes | yes | yes |
| SELinux permissive at early boot | no | no | yes |
| `/tmp` remounted read-write at boot | no | no | yes |
| Marker prop `ro.ibralyam.twrp.hybrid=1` | no | no | yes |
| Build fails above 64 MB | no | no | yes |
| Ramdisk checks in CI | no | no | yes |
| C installer with checks and restore | no | no | yes |
| Zip under 30 MB with LZMA2 image | no | no | yes |

The "no" marks come from the files I could inspect: the v3 zip, the 3.7.1 zip, the device tree, and your recovery logs.
If you find a mistake, open an issue.

## New in this build

### Compatibility for old installers

- `/system_root` mount point for ext4 and EROFS. AnyKernel3 and MultiDisabler look for it.
- `xzcat`, `unxz` and `busybox` inside recovery. Installers do not need to carry their own copy.
- `ro.ibralyam.twrp.hybrid=1`, so an installer can detect this build.
- SELinux permissive and a read-write `/tmp`.

The tools are one static arm64 applet with a built-in LZMA decoder. It is not full BusyBox.
Any other applet name goes to `toybox`.

### The installer

The zip has no shell script. The `update-binary` is one static arm64 C program.

- **Device check.** Accepts a50 or a50dd with model SM-A505. Rejects a50s and A507.
- **File check.** Every file is checked by CRC32 and a SHA-256 value built into the installer.
- **Dry run.** The image is decoded once without writing. It needs the `ANDROID!` header, 64 MiB or less, and no extra data.
- **Verified write.** Backup to `/tmp`, write, read back, compare hash, flush.
- **Restore.** The backup is written back if the check fails.
- **vbmeta.** Written only if the partition exists, then read back.
- **Messages.** Progress goes to the TWRP screen.

### Build and release

- A GitHub Actions workflow builds everything from source.
- The build stops if `recovery.img` is over 67108864 bytes.
- The build stops if a tool, an rc line, `/system_root` or the version string is missing.
- 15 installer self tests run on every build.
- The zip is about 25 MB. The image is stored as `recovery.img.xz` (LZMA2, CRC32).
- The zip has no 7za and no busybox.

## Tested and not tested

Tested:
- Boots and works on a real A50.
- 15 installer self tests pass on an x86 host.
- First CI image: 63,494,144 bytes.

Not tested:
- Restore after a failed write on a real phone.
- The `/tmp` exec flag.
- The applet with every installer.
- Phones other than the author's.

## Quick install

1. Boot to your current TWRP.
2. Flash `TWRP_hybrid_A50.zip`.
3. Reboot to recovery.

Do not flash the outer zip from GitHub Actions. It holds the real zip inside.
Full steps: [INSTALL.md](INSTALL.md).

## Documents

| File | What it covers |
| --- | --- |
| [INSTALL.md](INSTALL.md) | Install, update, go back |
| [FEATURES.md](FEATURES.md) | Every change in detail |
| [BUILD.md](BUILD.md) | Build it yourself with GitHub Actions |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Errors and fixes |
| [CHANGELOG.md](CHANGELOG.md) | Release history |
| [SECURITY.md](SECURITY.md) | What the checks protect, and what they do not |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Bug reports and changes |
| [CREDITS.md](CREDITS.md) | Who made the parts |

## Repo layout

```
.github/workflows/build-twrp.yml   CI build
flasher/twrp_flasher.c             installer and applets
patches/0001-hybrid-twrp-a50.patch device tree patch
tree-patched/                      patched files, already merged
ci/pack.sh                         compress, hash, build, zip
tests/run_tests.sh                 installer self tests
zip/vbmeta.img                     AVB image (9296 bytes)
```

## Help

Telegram: https://t.me/ibralyam
