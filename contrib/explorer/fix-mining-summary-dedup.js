#!/usr/bin/env node
// Post-patch fixup for the BurritoCoin block explorer (btc-rpc-explorer).
//
// Fixes duplicate rows / inflated "Total" on the Mining Summary page. The
// vendor page (views/mining-summary.pug) starts a 125ms status-poll timer per
// load without clearing the previous one, and renders by APPENDING rows with
// no reset. So toggling the time-range buttons (or a bunched-up burst of
// completion polls) makes a single build render its miner rows two-or-more
// times, doubling the visible row list and inflating the Total.
//
// Two idempotency guards:
//   1) loadMiningData()     clears any running poll timer before starting a new one
//   2) displaySummaryData() clears existing rows before it re-appends them
//
// The donut's ">1% of revenue" slice threshold is deliberately left alone —
// that is vendor-intended behavior (small miners fold into "Other" in the
// chart but remain in the Data table), not a bug.
//
// Run from inside the explorer checkout AFTER applying burritocoin-explorer.patch:
//   cd /opt/btc-rpc-explorer
//   node /root/BurritoCoin/contrib/explorer/fix-mining-summary-dedup.js
// Optionally pass the path to mining-summary.pug as the first argument.
//
// Safe to re-run: it is a no-op if the fix is already present.

const fs = require("fs");
const file = process.argv[2] || "views/mining-summary.pug";

const orig = fs.readFileSync(file, "utf8");

if (orig.includes("BRTO fix: reset rows first")) {
    console.log("Already applied; nothing to do: " + file);
    process.exit(0);
}

const lines = orig.split("\n");

function pad(line) { return (line.match(/^[ \t]*/) || [""])[0]; }

function insertBefore(anchor, add) {
    const i = lines.findIndex(l => l.includes(anchor));
    if (i < 0) throw new Error("ANCHOR NOT FOUND: " + anchor);
    const p = pad(lines[i]);
    lines.splice(i, 0, ...add.map(a => (a === "" ? "" : p + a)));
}

function insertAfter(anchor, add) {
    const i = lines.findIndex(l => l.includes(anchor));
    if (i < 0) throw new Error("ANCHOR NOT FOUND: " + anchor);
    const p = pad(lines[i]);
    lines.splice(i + 1, 0, ...add.map(a => (a === "" ? "" : p + a)));
}

// 1) stop any previous poll timer so a stale build cannot re-render rows
insertBefore("statusId = Math.random().toString(36)", [
    "// BRTO fix: stop any previous poll timer so a stale build cannot re-render rows",
    "if (statusInterval) { clearInterval(statusInterval); }"
]);

// 2) reset rows first so a repeated render cannot duplicate them
insertAfter("function displaySummaryData(summary) {", [
    "// BRTO fix: reset rows first so a repeated render cannot duplicate them",
    "$(\".miner-summary-row\").remove();"
]);

const out = lines.join("\n");
if (out === orig) throw new Error("anchors matched but no change produced");

fs.writeFileSync(file + ".bak", orig);
fs.writeFileSync(file, out);
console.log("Applied Mining Summary dup-row fix to " + file + " (backup: " + file + ".bak)");
