# Prebuilt binaries

`burritocoin-qt-win64.exe` — BurritoCoin Core GUI wallet for 64-bit Windows,
cross-compiled from this source tree (Qt5 static build).

- Unsigned: Windows SmartScreen will say "unknown publisher". Click
  **More info -> Run anyway**.
- It is a full wallet + node; first launch syncs the chain.
- Verify your download against the sha256 printed in the commit that added it.

This binary is committed for convenience. The canonical way to distribute
release binaries is GitHub Releases; this avoids bloating git history.
