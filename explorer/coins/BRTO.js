"use strict";

// BurritoCoin (BRTO) coin definition for btc-rpc-explorer.
// Based on Litecoin's config; adapted for BurritoCoin's unique chain parameters.

const Decimal = require("decimal.js");
const Decimal8 = Decimal.clone({ precision: 8, rounding: 8 });

const SUBSIDY_HALVING_INTERVAL = 1042600000;
const INITIAL_SUBSIDY = new Decimal8("10"); // 10 BRTO per block

// Matches chainparams.cpp genesis block comment.
const GENESIS_BLOCK_HASH =
	"00000f4b714b973787f41b7bf17002a796a3975b2556a6717f8ab7065c0da822";

// The genesis block has exactly one transaction; its hash equals the merkle root.
const GENESIS_COINBASE_TX_ID =
	"5370f1ef9a7a1861b679e158c76bd848d5a6431f9d2d1c805fb2060ff67a4c5a";

// Premine carried in the genesis coinbase output (148,000,000 BRTO × 1e8 satoshis).
const GENESIS_BLOCK_REWARD = new Decimal8("148000000");

module.exports = {
	name: "BurritoCoin",
	ticker: "BRTO",
	logoUrl: "img/logo/brto.svg", // place your logo at explorer/public/img/logo/brto.svg
	siteTitle: "BurritoCoin Explorer",
	nodeTitle: "BurritoCoin Core",
	nodeUrl: "https://burritocoin.org",

	// -----------------------------------------------------------------------
	// RPC defaults — overridden at runtime by .env / environment variables.
	// -----------------------------------------------------------------------
	rpcPort: 9226, // mainnet JSON-RPC port (P2P is 9227)

	// -----------------------------------------------------------------------
	// Genesis block
	// -----------------------------------------------------------------------
	genesisBlockHash: GENESIS_BLOCK_HASH,
	genesisCoinbaseTransactionId: GENESIS_COINBASE_TX_ID,
	genesisBlockReward: GENESIS_BLOCK_REWARD,

	// Text shown in the genesis coinbase scriptSig (the "Satoshi message").
	genesisCoinbaseTransactionDescription:
		"WSJ 18/Mar/2026 Finance Bros to Tech Bros: Don't Mess With My Bloomberg Terminal",

	// -----------------------------------------------------------------------
	// Mining / supply
	// -----------------------------------------------------------------------
	targetBlockTimeSeconds: 150, // 2.5 minutes

	// Returns the block subsidy (in whole BRTO) for a given height.
	// Does NOT include the genesis premine — that is a special one-time output.
	blockRewardFunction: function (blockHeight) {
		// Height 0 is the genesis premine block; treat it separately.
		if (blockHeight === 0) {
			return GENESIS_BLOCK_REWARD;
		}

		let halvings = Math.floor(blockHeight / SUBSIDY_HALVING_INTERVAL);

		// After 64 halvings the subsidy rounds to zero in integer arithmetic.
		if (halvings >= 64) {
			return new Decimal8("0");
		}

		let subsidy = INITIAL_SUBSIDY;
		for (let i = 0; i < halvings; i++) {
			subsidy = subsidy.dividedBy(2);
		}
		return subsidy;
	},

	// -----------------------------------------------------------------------
	// Address formats
	// -----------------------------------------------------------------------
	// Base58 version bytes (decimal) — from chainparams.cpp base58Prefixes[].
	//   PUBKEY_ADDRESS  = 25  → P2PKH addresses start with 'B'
	//   SCRIPT_ADDRESS  =  5  → P2SH  addresses start with '3'
	//   SCRIPT_ADDRESS2 = 28  → P2SH2 addresses start with 'C'
	p2pkhAddressPrefix: "19", // 25 decimal = 0x19 hex
	p2shAddressPrefix: "05",  //  5 decimal = 0x05 hex
	p2sh2AddressPrefix: "1c", // 28 decimal = 0x1C hex

	// SegWit / bech32 — from chainparams.cpp bech32_hrp / mweb_hrp.
	bech32AddressPrefix: "brto",
	mwebAddressPrefix: "brtomweb",

	// -----------------------------------------------------------------------
	// Network ports
	// -----------------------------------------------------------------------
	defaultPort: 9227, // P2P mainnet

	// -----------------------------------------------------------------------
	// Display / UI hints
	// -----------------------------------------------------------------------
	// BurritoCoin is Litecoin-derived; re-use Litecoin's known mining pools
	// config URL so the explorer can label miner coinbases once the network
	// is live. Replace with a BRTO-specific URL when one exists.
	miningPoolsConfigUrls: [
		"https://raw.githubusercontent.com/btccom/Blockchain-Known-Pools/master/pools.json",
	],

	// Displayed in the "summary" block on the home page.
	maxSupplyDescription: "~21,000,000,000 BRTO (148M genesis premine + ~20,852M mined)",

	// -----------------------------------------------------------------------
	// Donation address (optional — shown in the explorer footer if set).
	// -----------------------------------------------------------------------
	// donationAddress: "Bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
};
