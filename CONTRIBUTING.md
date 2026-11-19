# Contributing

## Report a bug

Send these:
- What you did.
- What you expected.
- What happened.
- `recovery.log` (TWRP: Advanced, Copy Log).
- The zip name and its SHA-256 value.

## Send a change

1. Fork the repo.
2. Make one change per pull request.
3. Run `bash tests/run_tests.sh`. All 15 tests must pass.
4. Run the CI build. Say if it passed.
5. Say which phone you tested on.

## Rules

- The recovery image must stay at 67108864 bytes or less.
- The zip must stay under 30 MB.
- Do not add 7za or busybox to the zip.
- Do not claim a feature works if you did not test it.

## Contact

Telegram: https://t.me/ibralyam
