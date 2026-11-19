# Features

Base: TWRP 3.7.1_12, device tree `PeterKnecht93/android_device_samsung_a50`, branch `twrp-12.1`.

## Compared with TWRP v3 (3.7.0_9)

| Item | v3 | Hybrid |
| --- | --- | --- |
| TWRP base | 3.7.0_9 | 3.7.1_12 |
| `xzcat`, `unxz`, `busybox` | no | yes |
| `/sbin/sh` | no | yes |
| `/system_root` mount point | yes | yes |

The tools are one static arm64 program with a built-in LZMA decoder.
It is not full BusyBox. Any other applet name goes to `toybox`.

## Compared with Peter's TWRP 3.7.1_12

| Item | Peter | Hybrid |
| --- | --- | --- |
| `/system_root` mount (ext4 and EROFS) | no | yes |
| `xzcat`, `unxz`, `busybox` | no | yes |
| `/sbin/sh` symlink | no | yes |
| SELinux permissive at early boot | no | yes |
| `/tmp` remounted read-write | no | yes |
| Marker prop `ro.ibralyam.twrp.hybrid=1` | no | yes |
| Version name `UNOFFICIAL-@ibralyam` | no | yes |
| 64 MB size gate in the build | no | yes |
| Ramdisk checks in CI | no | yes |
| C installer with checks | no | yes |

## Kept from the base

EROFS support, modern decryption, Mint kernel compatibility and the AVB `vbmeta.img`.
These are not new.

## The installer

- Device check: a50 or a50dd, model SM-A505. It rejects a50s and A507.
- Per-file check: CRC32 and a SHA-256 value built into the installer.
- Dry run: a full decode with no write. Needs the `ANDROID!` header, 64 MiB or less, no extra data.
- Verified write: backup, write, read back, compare hash, flush.
- Restore: the backup is written back if the check fails.
- vbmeta: written only if the partition exists, then read back.
- Zip size: about 25 MB. The recovery is stored as `recovery.img.xz` (LZMA2, CRC32).
- The zip has no 7za and no busybox.

## Tested and not tested

Tested:
- Boots and works on a real A50.
- The installer passes 15 self tests on an x86 host.
- The CI build stays under 64 MB (63,494,144 bytes in the first run).

Not tested:
- Restore after a failed write on a real phone.
- The `/tmp` exec flag.
- The applet with every installer.
- Phones other than the author's A50.
