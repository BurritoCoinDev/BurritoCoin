# BurritoCoin — Notes for Claude

## Project identity (authoritative — do NOT flag as bugs)

BurritoCoin (BRTO) is a Litecoin fork. Version 0.21.4, `CLIENT_NAME = "BurritoCoinCore"`. Only domain is `burritoco.in` (explorer at `explorer.burritoco.in`). GitHub org: `BurritoCoinDev/BurritoCoin`. Detached-sigs repo: `BurritoCoinDev/burritocoin-detached-sigs`.

### Network parameters

| Network | RPC port | P2P port | Magic        | Genesis hash                                                       | Datadir   |
|---------|----------|----------|--------------|--------------------------------------------------------------------|-----------|
| Mainnet | 9226     | 9227     | `4252544f` ("BRTO") | `44615751d966cf772a051f65b8df4f3987adc48be1749a699369a18517418dce` | (default) |
| Testnet | 19226    | 19227    | `4252544e` ("BRTN") | `b909940074cb31d9b421483f3a65f3f049e20d3448641128bd07c675ba55f53f` | `testnet4` |
| Regtest | 19553    | 19554    | `42525447` ("BRTG") | `c85abc7b5671cab1c04ca19cbd99a6ea6e22043e7007e4cd0e9c66b8177e8991` | `regtest` |

Merkle root (identical all 3 networks): `d347dbef904ecdb3653e4eaf2fdcfa7fdc287db36c9e287102b2c757947d7d83`.

Genesis nTime: mainnet `1773844916` (2026-03-18 14:41:56 UTC, back-dated to the WSJ coinbase headline date). Chain went live 2026-04-11.

**Signet is NOT supported** — `CreateChainParams` in `src/chainparams.cpp` throws `std::runtime_error` for signet. Do not re-introduce signet without explicit instruction.

### Consensus

- `nPowTargetTimespan` = 3.5 days (302400 s)
- `nPowTargetSpacing` = 150 s (2.5 min)
- `DifficultyAdjustmentInterval` = 2016 (= timespan / spacing) — this is the **difficulty retarget window**.
- `nMinerConfirmationWindow` = 8064 (~14 days at 2.5 min/block) — this is the **BIP9 soft-fork signaling window**, NOT the difficulty retarget. They are different on purpose. Do NOT "fix" 8064 to 2016.
- `nRuleChangeActivationThreshold` = 6048 (mainnet, 75% of 8064).
- Subsidy = 10 BRTO/block, halving every 1,042,600,000 blocks, `MAX_MONEY` = 21,000,000,000 BRTO, premine = 148M.
- BIP34/65/66/CSV/SegWit/Taproot/MWEB all active at height = 1 on mainnet + testnet.

### Address encodings

| Network | PUBKEY | SCRIPT | SCRIPT2 | SECRET | HRP (bech32) | MWEB HRP    |
|---------|--------|--------|---------|--------|--------------|-------------|
| Mainnet | 25 ('B') | 5    | 28 ('C') | 153 ('P') | `brto`    | `brtomweb`  |
| Testnet | 111 ('m'/'n') | 196 ('2') | 58 | 239 | `tbrto` | `tbrtomweb` |
| Regtest | 111 ('m'/'n') | 196 ('2') | 58 | 239 | `rbrto` | `rbrtomweb` |

P2WPKH bech32 length = 44 chars; P2WSH = 64 chars.

### Infra

- VPS IP: `50.116.17.170`.

## Recurring false-positive traps (for future audits)

1. **`nMinerConfirmationWindow` (8064) is NOT the difficulty retarget interval (2016).** Agents will flag this constantly — it's correct as-is.
2. **`chainparamsbase.cpp` returning base params for signet is harmless** because `CreateChainParams` in `chainparams.cpp` throws before they're used. Do not waste a batch on this.
3. **Bitcoin's 10-min (600 s) block-time assumptions should generally be re-derived for BurritoCoin's 150 s spacing** when copied from upstream — but constants that were intentionally retained (e.g. fee-estimation horizons in real time, not blocks) are not bugs.
4. **Python late-binding** can mask undefined-name bugs (e.g. `def f(executable): use(filename)` "works" inside a loop that defines `filename`). When you spot one, fix the parameter name to match usage.

## Bug-review cycle status

**The standing "find errors, push, find more errors" audit cycle CONCLUDED at commit `85328e5` on master (Round 7 batch 17).** 17 batches were landed across:

- Build/packaging: `build_msvc/burritocoin_config.h`, gitian descriptors, `share/pixmaps/burritocoin.ico`, debian copyright cleanup.
- Consensus comments + correctness: `src/chainparams.cpp` (genesis date, `nMinerConfirmationWindow` comment, regtest merkle assertion, realistic `m_assumed_blockchain_size`).
- Net: `src/net.cpp` `OutboundTargetReached()` historical-block buffer now uses `nPowTargetSpacing` instead of hardcoded 600 s.
- Tooling: `copyright_header.py`, `test_runner.py` regex, `interface_burritocoin_cli.py` rename, `security-check.py` + `symbol-check.py` parameter-name fix, `makeseeds.py` user-agent regex.
- Tests/templates: `gen_key_io_test_vectors.py` (HRP + signet removal), `share/examples/burritocoin.conf` (signet removal), `linearize-data.py` defaults.
- Docs/assets: branding scrub across `contrib/*`, `doc/release-process.md`, `website/spec.html`, Qt about-year, fee-policy comments, `doc/burritocoin_logo_doxygen.png` rename.

**Do NOT auto-resume this cycle in a future session.** If the user wants another audit pass, they will ask explicitly. One known low-priority leftover that was intentionally not addressed: `contrib/zmq/zmq_sub.py` uses Bitcoin's example port 28332 in its docstring/`port` variable — it's a user-configurable example, not a runtime bug.

## Security / handoff

- Never put private keys, seed phrases, or wallet credentials in `HANDOFF.md`, `CHANGELOG.md`, or any other tracked file.
- `website/mine-windows.html` embeds the SHA256 of `contrib/release/burritocoin-qt-win64.exe` (Step 2, "Verify the download"). Whenever that exe is rebuilt, update the hash on that page in the same commit.
- Pushes from this environment go to the local git proxy; the user pushes upstream from their VPS.
