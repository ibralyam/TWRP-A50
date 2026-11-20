# Changelog

## Unreleased

- Version name `3.7.1_12-0-UNOFFICIAL-@ibralyam` (needs a CI check).
- CI fails if the version string is missing from the recovery binary.
- Fixed the patch file. It held the workflow text by mistake.
- Fixed the ramdisk check. It now reads `system/etc/recovery.fstab`.

## First build

- TWRP 3.7.1_12 base from Peter's `twrp-12.1` tree.
- Added `/system_root` for ext4 and EROFS.
- Added `xzcat`, `unxz` and `busybox`.
- SELinux permissive, `/tmp` remount and the hybrid marker prop.
- 64 MB size gate. First image: 63,494,144 bytes.
- C installer with device check, hash check, dry run, verified write and restore.
- Zip size about 25 MB.
