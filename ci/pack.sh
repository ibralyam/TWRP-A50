#!/bin/bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
RECOVERY_IMG="${RECOVERY_IMG:?set RECOVERY_IMG}"
VBMETA_IMG="${VBMETA_IMG:-$HERE/zip/vbmeta.img}"
OUT_DIR="${OUT_DIR:-$PWD/dist}"
CC="${CC:-aarch64-linux-gnu-gcc}"
LZMA_INC="${LZMA_INC:-}"
LZMA_A="${LZMA_A:?set LZMA_A to the static liblzma.a for the target}"
ZIP_NAME="${ZIP_NAME:-TWRP_hybrid_A50.zip}"
MAX_RAW=67108864
MAX_ZIP=30000000
VBMETA_SIZE=9296

die() { echo "ERROR: $1" >&2; exit 1; }

RAW=$(stat -c %s "$RECOVERY_IMG")
echo "recovery.img: $RAW bytes (limit $MAX_RAW)"
[ "$RAW" -le "$MAX_RAW" ] || die "recovery.img is bigger than $MAX_RAW bytes"
[ "$(head -c 8 "$RECOVERY_IMG")" = "ANDROID!" ] || die "recovery.img is not a boot image"
[ "$(stat -c %s "$VBMETA_IMG")" = "$VBMETA_SIZE" ] || die "vbmeta.img must be $VBMETA_SIZE bytes"
[ "$(head -c 4 "$VBMETA_IMG")" = "AVB0" ] || die "vbmeta.img has no AVB0 header"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/stage/META-INF/com/google/android"

xz -9e --check=crc32 --lzma2=dict=32MiB -T1 -c "$RECOVERY_IMG" > "$OUT_DIR/recovery.img.xz"
xz -t "$OUT_DIR/recovery.img.xz"
xz -lv "$OUT_DIR/recovery.img.xz" | grep -q 'Check:.*CRC32' || die "xz check is not CRC32"
[ "$(xz -dc "$OUT_DIR/recovery.img.xz" | wc -c)" = "$RAW" ] || die "xz round trip size mismatch"
xz -dc "$OUT_DIR/recovery.img.xz" | cmp - "$RECOVERY_IMG" || die "xz round trip content mismatch"

H_XZ=$(sha256sum "$OUT_DIR/recovery.img.xz" | cut -d' ' -f1)
H_RAW=$(sha256sum "$RECOVERY_IMG" | cut -d' ' -f1)
H_VB=$(sha256sum "$VBMETA_IMG" | cut -d' ' -f1)
echo "sha256 recovery.img.xz: $H_XZ"
echo "sha256 recovery.img:    $H_RAW"
echo "sha256 vbmeta.img:      $H_VB"

INC=()
[ -n "$LZMA_INC" ] && INC=(-I"$LZMA_INC")
"$CC" -static -O2 -s -fvisibility=hidden -Wall -Wextra "${INC[@]}" \
  -DEXPECT_XZ_SHA256="\"$H_XZ\"" \
  -DEXPECT_RAW_SHA256="\"$H_RAW\"" \
  -DEXPECT_VBMETA_SHA256="\"$H_VB\"" \
  -o "$OUT_DIR/stage/META-INF/com/google/android/update-binary" \
  "$HERE/flasher/twrp_flasher.c" "$LZMA_A"
chmod 755 "$OUT_DIR/stage/META-INF/com/google/android/update-binary"
echo "# ibralyam hybrid TWRP. The installer is the static update-binary." > "$OUT_DIR/stage/META-INF/com/google/android/updater-script"
cp "$OUT_DIR/recovery.img.xz" "$OUT_DIR/stage/recovery.img.xz"
cp "$VBMETA_IMG" "$OUT_DIR/stage/vbmeta.img"

( cd "$OUT_DIR/stage" && zip -q -0 -X -r "../$ZIP_NAME" META-INF recovery.img.xz vbmeta.img )
ZS=$(stat -c %s "$OUT_DIR/$ZIP_NAME")
echo "$ZIP_NAME: $ZS bytes (limit $MAX_ZIP)"
[ "$ZS" -lt "$MAX_ZIP" ] || die "zip is $ZS bytes, limit is $MAX_ZIP"
unzip -l "$OUT_DIR/$ZIP_NAME" | grep -E '7za|busybox|frsh_toolkit' && die "legacy files are still in the zip"
unzip -tq "$OUT_DIR/$ZIP_NAME"
unzip -l "$OUT_DIR/$ZIP_NAME"
( cd "$OUT_DIR" && sha256sum "$ZIP_NAME" recovery.img.xz > SHA256SUMS.txt && cat SHA256SUMS.txt )
