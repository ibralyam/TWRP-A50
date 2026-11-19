# Hybrid TWRP for Galaxy A50

TWRP 3.7.1_12 for the Samsung Galaxy A50 (SM-A505F, a50dd, Exynos 9610).
It keeps the modern features of the 3.7.1 build. It adds what old installers expect from TWRP v3.

Version string: `3.7.1_12-0-UNOFFICIAL-@ibralyam`

## What you get

- EROFS support and modern decryption (from the base tree).
- A `/system_root` mount point for old installers.
- `xzcat`, `unxz`, `busybox` and `/sbin/sh` inside recovery.
- SELinux permissive and a hybrid marker prop for installers.
- A C installer that checks the device and every file before it writes.
- A small zip (about 25 MB).

Read [FEATURES.md](FEATURES.md) for the full list and the honest limits.

## Quick install

1. Boot to your current TWRP.
2. Flash `TWRP_hybrid_A50.zip`.
3. Reboot to recovery.

Full steps: [INSTALL.md](INSTALL.md).

## Download

Go to the Releases page. Download `TWRP_hybrid_A50.zip` and `SHA256SUMS.txt`.
Do not flash the outer zip from GitHub Actions. It holds the real zip inside.

## Documents

| File | What it covers |
| --- | --- |
| [INSTALL.md](INSTALL.md) | Install, update, go back |
| [FEATURES.md](FEATURES.md) | Every change, and what is not tested |
| [BUILD.md](BUILD.md) | Build it yourself with GitHub Actions |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Errors and fixes |
| [CHANGELOG.md](CHANGELOG.md) | Release history |
| [SECURITY.md](SECURITY.md) | What the checks protect, and what they do not |
| [CONTRIBUTING.md](CONTRIBUTING.md) | How to report bugs and send changes |
| [CREDITS.md](CREDITS.md) | Who made the parts |

## Repo layout

```
.github/workflows/build-twrp.yml   CI build
flasher/twrp_flasher.c             installer and applets
patches/0001-hybrid-twrp-a50.patch device tree patch
tree-patched/                      patched files, already merged
ci/pack.sh                         compress, hash, build, zip
tests/run_tests.sh                 flasher self tests
zip/vbmeta.img                     AVB image (9296 bytes)
```

## Help

Telegram: https://t.me/ibralyam
