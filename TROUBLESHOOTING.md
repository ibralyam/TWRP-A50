# Troubleshooting

## Invalid zip format

You flashed the outer zip from GitHub Actions. Unpack it. Flash `TWRP_hybrid_A50.zip`.

## Device check failed

The installer only accepts a50 or a50dd with model SM-A505. It rejects a50s and A507.
Send the recovery log if your phone is an A505 and it still fails.

## A file check failed

The zip is damaged or changed. Download it again. Compare the SHA-256 value with `SHA256SUMS.txt`.
Nothing was written.

## The write failed

The installer tries to restore your old recovery. Do not reboot yet.
Flash the zip again. If it fails again, send the recovery log.

## The version name is not shown

You are still in the old recovery. Flash the zip again, then reboot to recovery.
Check that the zip is from a build that has the version patch.

## The recovery does not boot

Flash your old recovery with Odin. Then try again with a fresh download.

## An old installer cannot find a tool

Boot the hybrid recovery. The tools are `xzcat`, `unxz` and `busybox`.
`busybox` here is a small applet, not full BusyBox. Other applets go to `toybox`.
Tell the installer author which applet is missing.

## How to get the recovery log

In TWRP, tap Advanced, then Copy Log. The file is saved as `recovery.log`.

## Help

Telegram: https://t.me/ibralyam
