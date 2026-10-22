# Prebuilt binaries

`burritocoin-qt-win64.exe` — BurritoCoin Core GUI wallet for 64-bit Windows,
cross-compiled from this source tree (Qt5 static build, mingw-w64).

- Unsigned: Windows SmartScreen will say "unknown publisher". Click
  **More info -> Run anyway**.
- It is a full wallet + node; first launch syncs the chain.
- Verify your download against the SHA256 published on
  <https://burritoco.in/mine-windows> (Step 2, `#verify`). That
  page is updated in the same commit as the binary, so the two never drift.

This binary is committed for convenience — the download links on
burritoco.in point at `raw/master/` here, so replacing this file republishes
the wallet with no other moving parts. The canonical way to distribute
release binaries is GitHub Releases; moving to that would stop each rebuild
adding another ~35 MB blob to git history.

The current binary self-reports `v0.21.4.0-f096336` in **Help -> About**:
it was cross-built from commit `f096336` by GitHub Actions run `39851655047`
(`.github/workflows/build-windows.yml`) on 2026-10-22. It is the first build
whose Mine tab mines on a quiet chain; earlier builds refused to start if
the newest block was more than a day old when the wallet was opened. The
SHA256 is still the check that proves which bytes you have.
