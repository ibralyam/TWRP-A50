#!/bin/bash
set -u
SRC="$(cd "$(dirname "$0")/.." && pwd)/flasher/twrp_flasher.c"
LZMA_A="${LZMA_A:-/usr/lib/x86_64-linux-gnu/liblzma.a}"
CC="${CC:-gcc}"
T="$(mktemp -d)"
PASS=0
FAIL=0
ok() { echo "PASS $1"; PASS=$((PASS + 1)); }
bad() { echo "FAIL $1"; FAIL=$((FAIL + 1)); }
sha() { sha256sum "$1" | cut -d' ' -f1; }

mkimg() {
  local out="$1" size="$2" magic="$3"
  { printf '%s' "$magic"; head -c 2040 /dev/zero; head -c "$size" /dev/urandom; } > "$out"
}
build() {
  local name="$1" xz="$2" raw="$3" vb="$4"
  $CC -static -O2 -s -fvisibility=hidden -Wall -Wextra \
    -DEXPECT_XZ_SHA256="\"$(sha "$xz")\"" \
    -DEXPECT_RAW_SHA256="\"$(sha "$raw")\"" \
    -DEXPECT_VBMETA_SHA256="\"$(sha "$vb")\"" \
    -o "$T/$name" "$SRC" "$LZMA_A" || { echo "BUILD FAILED $name"; exit 1; }
}
mkzip() {
  local out="$1" xz="$2" vb="$3" d="$T/z.$RANDOM"
  mkdir -p "$d/META-INF/com/google/android"
  cp "$xz" "$d/recovery.img.xz"
  cp "$vb" "$d/vbmeta.img"
  echo "# ibralyam" > "$d/META-INF/com/google/android/updater-script"
  (cd "$d" && zip -q -0 -X -r "$out" . )
}
parts() {
  rm -rf "$T/by-name"; mkdir -p "$T/by-name"
  head -c "$1" /dev/zero | tr '\0' '\252' > "$T/by-name/recovery"
  head -c 65536 /dev/zero | tr '\0' '\252' > "$T/by-name/vbmeta"
}
props() { printf 'ro.product.device=%s\nro.build.product=%s\nro.boot.product.model=%s\n' "$1" "$1" "$2" > "$T/props"; }
run() {
  A50_BYNAME="$T/by-name" A50_PROP_FILE="$T/props" "$T/$1" 3 1 "$2" > "$T/out.txt" 2> "$T/err.txt" 1>&2
  echo $? > "$T/rc"
  cat "$T/err.txt" > "$T/out.txt"
}
rcv() { cat "$T/rc"; }
untouched() { [ "$(tr -d '\252' < "$T/by-name/recovery" | wc -c)" = "0" ] && [ "$(tr -d '\252' < "$T/by-name/vbmeta" | wc -c)" = "0" ]; }

mkimg "$T/rec.img" 3000000 'ANDROID!'
xz -9e --check=crc32 -T1 -c "$T/rec.img" > "$T/rec.img.xz"
head -c 9296 /dev/urandom > "$T/vb.tmp"; { printf 'AVB0'; tail -c +5 "$T/vb.tmp"; } > "$T/vbmeta.img"
build good "$T/rec.img.xz" "$T/rec.img" "$T/vbmeta.img"
mkzip "$T/good.zip" "$T/rec.img.xz" "$T/vbmeta.img"

parts 4194304; props a50dd SM-A505F; run good "$T/good.zip"
if [ "$(rcv)" = 0 ] && cmp -n "$(stat -c %s "$T/rec.img")" "$T/rec.img" "$T/by-name/recovery" && cmp -n 9296 "$T/vbmeta.img" "$T/by-name/vbmeta"; then ok "good flash writes recovery and vbmeta"; else bad "good flash"; cat "$T/out.txt"; fi
[ "$(stat -c %s "$T/by-name/recovery")" = 4194304 ] && ok "partition size unchanged" || bad "partition size"

parts 4194304; props a50s SM-A507FN; run good "$T/good.zip"
if [ "$(rcv)" != 0 ] && untouched && grep -q "A50s" "$T/out.txt"; then ok "a50s rejected, nothing written"; else bad "a50s"; fi

parts 4194304; props a10 SM-A105F; run good "$T/good.zip"
if [ "$(rcv)" != 0 ] && untouched; then ok "other device rejected"; else bad "other device"; fi

parts 4194304; props a50 SM-A505F; run good "$T/good.zip"
[ "$(rcv)" = 0 ] && ok "plain a50 accepted" || bad "plain a50"

head -c 1500000 /dev/urandom > "$T/other.bin"; { printf 'ANDROID!'; head -c 2040 /dev/zero; cat "$T/other.bin"; } > "$T/other.img"
xz -9e --check=crc32 -T1 -c "$T/other.img" > "$T/other.xz"
mkzip "$T/swap.zip" "$T/other.xz" "$T/vbmeta.img"
parts 4194304; props a50dd SM-A505F; run good "$T/swap.zip"
if [ "$(rcv)" != 0 ] && untouched && grep -q "was changed" "$T/out.txt"; then ok "swapped payload rejected (anti-repack)"; else bad "swapped payload"; fi

cp "$T/rec.img.xz" "$T/bad.xz"; printf '\377' | dd of="$T/bad.xz" bs=1 seek=1500000 conv=notrunc 2>/dev/null
build corrupt "$T/bad.xz" "$T/rec.img" "$T/vbmeta.img"
mkzip "$T/corrupt.zip" "$T/bad.xz" "$T/vbmeta.img"
parts 4194304; run corrupt "$T/corrupt.zip"
if [ "$(rcv)" != 0 ] && untouched && grep -q "corrupt" "$T/out.txt"; then ok "corrupt xz stream aborts before any write"; else bad "corrupt stream"; cat "$T/out.txt"; fi

head -c 200000 "$T/rec.img.xz" > "$T/trunc.xz"
build trunc "$T/trunc.xz" "$T/rec.img" "$T/vbmeta.img"
mkzip "$T/trunc.zip" "$T/trunc.xz" "$T/vbmeta.img"
parts 4194304; run trunc "$T/trunc.zip"
if [ "$(rcv)" != 0 ] && untouched; then ok "truncated xz aborts before any write"; else bad "truncated"; fi

{ cat "$T/rec.img.xz"; printf 'garbage'; } > "$T/tail.xz"
build tail "$T/tail.xz" "$T/rec.img" "$T/vbmeta.img"
mkzip "$T/tail.zip" "$T/tail.xz" "$T/vbmeta.img"
parts 4194304; run tail "$T/tail.zip"
if [ "$(rcv)" != 0 ] && untouched; then ok "trailing garbage aborts before any write"; else bad "trailing garbage"; fi

{ head -c 70000000 /dev/zero; } > "$T/big.body"; { printf 'ANDROID!'; head -c 2040 /dev/zero; cat "$T/big.body"; } > "$T/big.img"
xz -9e --check=crc32 -T1 -c "$T/big.img" > "$T/big.xz"
build big "$T/big.xz" "$T/big.img" "$T/vbmeta.img"
mkzip "$T/big.zip" "$T/big.xz" "$T/vbmeta.img"
parts 4194304; run big "$T/big.zip"
if [ "$(rcv)" != 0 ] && untouched && grep -q "bigger than 64 MB" "$T/out.txt"; then ok "image over 64 MB rejected"; else bad "oversize"; cat "$T/out.txt"; fi

parts 2000000; run good "$T/good.zip"
if [ "$(rcv)" != 0 ] && grep -q "does not fit" "$T/out.txt"; then ok "image larger than partition rejected"; else bad "partition too small"; fi

head -c 3000000 /dev/urandom > "$T/nb.img"
xz -9e --check=crc32 -T1 -c "$T/nb.img" > "$T/nb.xz"
build notboot "$T/nb.xz" "$T/nb.img" "$T/vbmeta.img"
mkzip "$T/nb.zip" "$T/nb.xz" "$T/vbmeta.img"
parts 4194304; run notboot "$T/nb.zip"
if [ "$(rcv)" != 0 ] && untouched && grep -q "not a boot image" "$T/out.txt"; then ok "non boot image rejected"; else bad "non boot image"; fi

xz -6 --check=none -T1 -c "$T/rec.img" > "$T/nocheck.xz"
build nocheck "$T/nocheck.xz" "$T/rec.img" "$T/vbmeta.img"
mkzip "$T/nocheck.zip" "$T/nocheck.xz" "$T/vbmeta.img"
parts 4194304; run nocheck "$T/nocheck.zip"
if [ "$(rcv)" != 0 ] && untouched; then ok "xz without integrity check rejected"; else bad "no check"; fi

parts 4194304; rm -f "$T/by-name/vbmeta"; run good "$T/good.zip"
if [ "$(rcv)" = 0 ] && grep -q "No vbmeta partition" "$T/out.txt" && cmp -n "$(stat -c %s "$T/rec.img")" "$T/rec.img" "$T/by-name/recovery"; then ok "missing vbmeta partition is skipped"; else bad "no vbmeta partition"; fi

rm -rf "$T/z.rm"; mkdir "$T/z.rm"; cp "$T/good.zip" "$T/stor.zip"
mkdir -p "$T/dz/META-INF/com/google/android"; cp "$T/rec.img.xz" "$T/dz/recovery.img.xz"; cp "$T/vbmeta.img" "$T/dz/"
(cd "$T/dz" && zip -q -9 -X -r "$T/defl.zip" . )
parts 4194304; run good "$T/defl.zip"
if [ "$(rcv)" != 0 ] && untouched; then ok "deflated entries rejected (zip -0 required)"; else bad "deflated"; fi

echo "SUMMARY pass=$PASS fail=$FAIL"
rm -rf "$T"
[ "$FAIL" = 0 ]
