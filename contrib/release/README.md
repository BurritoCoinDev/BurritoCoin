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

The current binary self-reports `v0.21.4.0-57f5cf3` in **Help -> About**.
That was the source commit it was cross-built from, but it is a *pre-rewrite*
SHA: on 2026-08-28 this repository's history was rewritten to drop 44
superseded copies of this executable, which renumbered every commit. The
string is baked into the compiled binary and cannot be corrected, so it no
longer resolves with `git show`. Verify by SHA256 instead - that is the
check that actually proves which bytes you have.
