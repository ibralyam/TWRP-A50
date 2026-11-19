# Hybrid TWRP for Galaxy A50 (`a50dd`)

TWRP 3.7.1_12 for the Samsung Galaxy A50 (SM-A505F, `a50dd`, Exynos 9610).
It keeps the modern features of the 3.7.1 build and adds what older installers expect from TWRP v3.

Version string: `3.7.1_12-0-UNOFFICIAL-@ibralyam`

## What you get

- EROFS support and modern decryption (from the base tree).
- A `/system_root` mount point for old installers.
- `xzcat`, `unxz`, `busybox`, and `/sbin/sh` inside recovery.
- SELinux permissive and a hybrid marker prop for installers.
- A static C installer (`twrp_flasher`) that verifies the device and every file before writing.
- A small flashable zip (about 25 MB).

Read [FEATURES.md](FEATURES.md) for the full list and honest limits.

## Quick install

1. Boot into your current TWRP.
2. Flash `TWRP_hybrid_A50.zip`.
3. Reboot to recovery.

Full steps: [INSTALL.md](INSTALL.md).

## Download

Go to the **Releases** page and download `TWRP_hybrid_A50.zip` along with `SHA256SUMS.txt`.
Do not flash the outer zip from GitHub Actions directly—extract it first to get the real flashable zip inside.

## Documents

| File | What it covers |
| --- | --- |
| [INSTALL.md](INSTALL.md) | Install, update, and go back |
| [FEATURES.md](FEATURES.md) | Every change, and what is not tested |
| [BUILD.md](BUILD.md) | Build it yourself with GitHub Actions |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Errors and fixes |
| [CHANGELOG.md](CHANGELOG.md) | Release history |
| [SECURITY.md](SECURITY.md) | What the checks protect, and what they do not |
| [CONTRIBUTING.md](CONTRIBUTING.md) | How to report bugs and send changes |
| [CREDITS.md](CREDITS.md) | Who made the parts |

## Repo layout

```text
.github/workflows/build-twrp.yml   CI workflow (copy to repo root .github/workflows/ if vendored)
flasher/twrp_flasher.c             update-binary installer and xzcat, unxz, busybox applets
patches/0001-hybrid-twrp-a50.patch patch for PeterKnecht93/android_device_samsung_a50 (twrp-12.1)
tree-patched/                      the four patched files, already merged
ci/pack.sh                         compress, bake hashes, build flasher, make zip
tests/run_tests.sh                 flasher self tests (x86 host)
zip/vbmeta.img                     AVB image (9296 bytes, from the 3.7.1 zip)
