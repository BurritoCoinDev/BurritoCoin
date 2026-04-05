#!/usr/bin/env node
"use strict";

// Copies the BRTO coin definition into btc-rpc-explorer's coins directory
// and registers it in the coins index so the explorer can use BTCEXP_COIN=BRTO.
//
// This runs automatically after `npm install` (via the "postinstall" script in
// package.json). Re-run it manually if you upgrade btc-rpc-explorer:
//   node scripts/postinstall.js

const fs   = require("fs");
const path = require("path");

// ---------------------------------------------------------------------------
// Locate btc-rpc-explorer inside node_modules
// ---------------------------------------------------------------------------
let btcExpRoot;
try {
	btcExpRoot = path.dirname(
		require.resolve("btc-rpc-explorer/package.json")
	);
} catch (e) {
	console.error(
		"[BRTO postinstall] btc-rpc-explorer not found in node_modules.\n" +
		"Run `npm install` first."
	);
	process.exit(1);
}

// ---------------------------------------------------------------------------
// 1. Copy BRTO.js into btc-rpc-explorer's coins directory
// ---------------------------------------------------------------------------
const srcCoinFile  = path.join(__dirname, "..", "coins", "BRTO.js");
const coinsDir     = path.join(btcExpRoot, "app", "coins");
const destCoinFile = path.join(coinsDir, "BRTO.js");

if (!fs.existsSync(coinsDir)) {
	console.error(
		`[BRTO postinstall] Expected coins directory not found: ${coinsDir}\n` +
		"btc-rpc-explorer package structure may have changed."
	);
	process.exit(1);
}

fs.copyFileSync(srcCoinFile, destCoinFile);
console.log(`[BRTO postinstall] Copied BRTO.js → ${destCoinFile}`);

// ---------------------------------------------------------------------------
// 2. Register BRTO in btc-rpc-explorer's coins index
// ---------------------------------------------------------------------------
// The index file is typically app/coins.js (v3.x) or app/coins/index.js.
// We look for whichever exists and patch it if BRTO isn't already listed.
const candidateIndexFiles = [
	path.join(btcExpRoot, "app", "coins.js"),
	path.join(btcExpRoot, "app", "coins", "index.js"),
];

let indexFile = candidateIndexFiles.find(f => fs.existsSync(f));

if (!indexFile) {
	console.warn(
		"[BRTO postinstall] Could not find btc-rpc-explorer coins index file.\n" +
		"BRTO.js was copied but may not be auto-loaded; start.js will patch at runtime instead."
	);
	process.exit(0);
}

let indexSource = fs.readFileSync(indexFile, "utf8");

// Check for the actual require entry, not just the string "BRTO" which could
// appear in comments, URLs, or other string values and cause a false positive.
if (/["']BRTO["']\s*:\s*require\s*\(/.test(indexSource)) {
	console.log("[BRTO postinstall] BRTO already registered in coins index — skipping patch.");
	process.exit(0);
}

// Append a require line before the module.exports or at the end of the object.
// Strategy: find the last entry in the coins object and insert after it.
// We use a simple text-insertion approach that works for the standard format.
const requireLine = '\t"BRTO": require("./BRTO.js"),';

// Insert our require line before the last closing brace in the file.
// Using lastIndexOf avoids the multiline-regex pitfall where the first '}'
// in the file (inside a nested object) would be matched instead of the final one.
const lastBraceIdx = indexSource.lastIndexOf("}");
if (lastBraceIdx !== -1) {
	indexSource =
		indexSource.slice(0, lastBraceIdx) +
		requireLine + "\n" +
		indexSource.slice(lastBraceIdx);
	fs.writeFileSync(indexFile, indexSource, "utf8");
	console.log(`[BRTO postinstall] Registered BRTO in ${indexFile}`);
} else {
	console.warn(
		"[BRTO postinstall] Could not auto-patch the coins index (unexpected format).\n" +
		"start.js will patch the coins registry at runtime instead."
	);
}
