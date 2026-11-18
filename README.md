# Hybrid TWRP for Galaxy A50 (a50dd)

Layout of this folder (put it in your repo as twrp-hybrid/):

    twrp-hybrid/
      .github/workflows/build-twrp.yml   copy this file to the repo root .github/workflows/
      flasher/twrp_flasher.c             update-binary and xzcat, unxz, busybox applets
      patches/0001-hybrid-twrp-a50.patch patch for PeterKnecht93/android_device_samsung_a50 (twrp-12.1)
      tree-patched/                      the four patched files, already merged
      ci/pack.sh                         compress, bake hashes, build flasher, make zip
      tests/run_tests.sh                 flasher self tests (x86 host)
      zip/vbmeta.img                     9296 bytes, from the 3.7.1 zip

Final zip layout:

    TWRP_hybrid_A50.zip
      META-INF/com/google/android/update-binary   static arm64 ELF (twrp_flasher)
      META-INF/com/google/android/updater-script  one comment line
      recovery.img.xz                             LZMA2, CRC32 check
      vbmeta.img                                  9296 bytes

Static compile command (arm64):

    aarch64-linux-gnu-gcc -static -O2 -s -fvisibility=hidden -Wall -Wextra \
      -I$XZ/include \
      -DEXPECT_XZ_SHA256='"<sha256 of recovery.img.xz>"' \
      -DEXPECT_RAW_SHA256='"<sha256 of recovery.img>"' \
      -DEXPECT_VBMETA_SHA256='"<sha256 of vbmeta.img>"' \
      -o update-binary flasher/twrp_flasher.c $XZ/lib/liblzma.a

Without the three -D values the same source builds only the xzcat, unxz and busybox applets.
