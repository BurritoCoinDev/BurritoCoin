#!/usr/bin/env node
"use strict";

// BurritoCoin block explorer launcher.
//
// Usage:
//   node start.js            (reads .env automatically)
//   BTCEXP_PORT=3003 node start.js
//
// This script:
//   1. Loads environment variables from .env (via dotenv).
//   2. Ensures BTCEXP_COIN is set to BRTO.
//   3. Injects the BRTO coin definition into btc-rpc-explorer's coin registry
//      at runtime — so even if the postinstall step was skipped or the package
//      was upgraded, the explorer will always know about BRTO.
//   4. Starts the btc-rpc-explorer web server.

const path = require("path");

// ---------------------------------------------------------------------------
// 1. Load .env
// ---------------------------------------------------------------------------
require("dotenv").config({ path: path.join(__dirname, ".env") });

// Default to BRTO if the caller hasn't specified a coin.
if (!process.env.BTCEXP_COIN) {
	process.env.BTCEXP_COIN = "BRTO";
}

// ---------------------------------------------------------------------------
// 2. Locate btc-rpc-explorer
// ---------------------------------------------------------------------------
let btcExpRoot;
try {
	btcExpRoot = path.dirname(
		require.resolve("btc-rpc-explorer/package.json")
	);
} catch (e) {
	console.error(
		"[BurritoCoin Explorer] btc-rpc-explorer is not installed.\n" +
		"Run `npm install` in the explorer/ directory first."
	);
	process.exit(1);
}

// ---------------------------------------------------------------------------
// 3. Inject BRTO coin definition at runtime
//    Handles both the v3.x app/coins.js and the directory-based layout.
// ---------------------------------------------------------------------------
const brtoCoinDef = require("./coins/BRTO.js");

const candidateCoinsModules = [
	path.join(btcExpRoot, "app", "coins.js"),
	path.join(btcExpRoot, "app", "coins", "index.js"),
];

let coinsInjected = false;
for (const coinsPath of candidateCoinsModules) {
	try {
		const coinsRegistry = require(coinsPath);
		if (typeof coinsRegistry === "object" && coinsRegistry !== null) {
			coinsRegistry["BRTO"] = brtoCoinDef;
			coinsInjected = true;
			console.log(`[BurritoCoin Explorer] Registered BRTO in ${coinsPath}`);
			break;
		}
	} catch (_) {
		// Not found at this path — try the next candidate.
	}
}

if (!coinsInjected) {
	console.warn(
		"[BurritoCoin Explorer] Could not find btc-rpc-explorer's coin registry.\n" +
		"The explorer may not recognise BTCEXP_COIN=BRTO. " +
		"Try running `node scripts/postinstall.js` to patch the package."
	);
}

// ---------------------------------------------------------------------------
// 4. Start btc-rpc-explorer
// ---------------------------------------------------------------------------
const appCandidates = [
	path.join(btcExpRoot, "app", "app.js"),
	path.join(btcExpRoot, "app", "index.js"),
	path.join(btcExpRoot, "index.js"),
];

let appStarted = false;
for (const appPath of appCandidates) {
	try {
		require(appPath);
		appStarted = true;
		break;
	} catch (e) {
		if (e.code === "MODULE_NOT_FOUND" && !e.message.includes(appPath)) {
			// A dependency inside the app is missing — propagate the error.
			throw e;
		}
		// Module not found at this path — try the next candidate.
	}
}

if (!appStarted) {
	// Fall back to the btc-rpc-explorer CLI entry point.
	try {
		require(path.join(btcExpRoot, "bin", "cli.js"));
	} catch (e) {
		console.error(
			"[BurritoCoin Explorer] Failed to start btc-rpc-explorer.\n" +
			"Check that btc-rpc-explorer is installed correctly (`npm install`).\n" +
			e.message
		);
		process.exit(1);
	}
}
