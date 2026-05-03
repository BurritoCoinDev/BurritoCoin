"use strict";

const Decimal = require("decimal.js");
const Decimal8 = Decimal.clone({ precision:8, rounding:8 });

const blockRewardEras = [ new Decimal8(10) ];
for (let i = 1; i < 34; i++) {
    let previous = blockRewardEras[i - 1];
    blockRewardEras.push(new Decimal8(previous).dividedBy(2));
}

const currencyUnits = [
    { type:"native", name:"BRTO", multiplier:1, default:true, values:["","brto","BRTO"], decimalPlaces:8 },
    { type:"native", name:"mBRTO", multiplier:1000, values:["mbrto"], decimalPlaces:5 },
    { type:"native", name:"sat", multiplier:100000000, values:["sat","satoshi"], decimalPlaces:0 }
];

module.exports = {
    name:"BurritoCoin",
    ticker:"BRTO",
    logoUrlsByNetwork:{ "main":"./img/network-mainnet/logo.svg", "test":"./img/network-testnet/logo.svg", "regtest":"./img/network-regtest/logo.svg" },
    coinIconUrlsByNetwork:{ "main":"./img/network-mainnet/coin-icon.svg", "test":"./img/network-testnet/coin-icon.svg", "regtest":"./img/network-regtest/coin-icon.svg" },
    coinColorsByNetwork:{ "main":"#FF8C00", "test":"#1daf00", "regtest":"#777" },
    siteTitlesByNetwork:{ "main":"BurritoCoin Explorer", "test":"BurritoCoin Testnet Explorer", "regtest":"BurritoCoin Regtest Explorer" },
    demoSiteUrlsByNetwork:{ "main":"http://50.116.17.170:3002" },
    knownTransactionsByNetwork:{ main:"d347dbef904ecdb3653e4eaf2fdcfa7fdc287db36c9e287102b2c757947d7d83" },
    miningPoolsConfigUrls:[],
    maxBlockWeight:4000000,
    maxBlockSize:1000000,
    minTxBytes:166,
    minTxWeight:664,
    difficultyAdjustmentBlockCount:2016,
    maxSupplyByNetwork:{ "main":new Decimal(21000000000), "test":new Decimal(21000000000), "regtest":new Decimal(21000000000) },
    targetBlockTimeSeconds:150,
    targetBlockTimeMinutes:2.5,
    currencyUnits:currencyUnits,
    currencyUnitsByName:{"BRTO":currencyUnits[0],"mBRTO":currencyUnits[1],"sat":currencyUnits[2]},
    baseCurrencyUnit:currencyUnits[2],
    defaultCurrencyUnit:currencyUnits[0],
    feeSatoshiPerByteBucketMaxima:[1,2,3,4,5,6,7,8,9,10,15,20,25,50,75,100,150],
    halvingBlockIntervalsByNetwork:{ "main":1042600000, "test":1042600000, "regtest":1042600000 },
    terminalHalvingCountByNetwork:{ "main":32, "test":32, "regtest":32 },
    coinSupplyCheckpointsByNetwork:{ "main":[0,new Decimal(148000000)], "test":[0,new Decimal(148000000)], "regtest":[0,new Decimal(148000000)] },
    genesisBlockHashesByNetwork:{
        "main":"44615751d966cf772a051f65b8df4f3987adc48be1749a699369a18517418dce",
        "test":"b909940074cb31d9b421483f3a65f3f049e20d3448641128bd07c675ba55f53f",
        "regtest":"c85abc7b5671cab1c04ca19cbd99a6ea6e22043e7007e4cd0e9c66b8177e8991"
    },
    genesisCoinbaseTransactionIdsByNetwork:{
        "main":"d347dbef904ecdb3653e4eaf2fdcfa7fdc287db36c9e287102b2c757947d7d83",
        "test":"d347dbef904ecdb3653e4eaf2fdcfa7fdc287db36c9e287102b2c757947d7d83",
        "regtest":"d347dbef904ecdb3653e4eaf2fdcfa7fdc287db36c9e287102b2c757947d7d83"
    },
    genesisBlockStatsByNetwork:{
        "main":{
            "avgfee":0,"avgfeerate":0,"avgtxsize":299,
            "blockhash":"44615751d966cf772a051f65b8df4f3987adc48be1749a699369a18517418dce",
            "feerate_percentiles":[0,0,0,0,0],
            "height":0,"ins":0,"maxfee":0,"maxfeerate":0,"maxtxsize":299,
            "medianfee":0,"mediantime":1773844916,"mediantxsize":299,
            "minfee":0,"minfeerate":0,"mintxsize":299,"outs":1,
            "subsidy":14800000000000000,
            "swtotal_size":0,"swtotal_weight":0,"swtxs":0,
            "time":1773844916,"total_out":0,"total_size":299,"total_weight":1196,
            "totalfee":0,"txs":1,"utxo_increase":1,"utxo_size_inc":117
        }
    },
    utxoSetCheckpointsByNetwork:{},
    genesisCoinbaseOutputAddressScripthash:"",
    historicalData:[],
    blockRewardFunction:function(blockHeight, chain) {
        let index = Math.floor(blockHeight / 1042600000);
        return blockRewardEras[index];
    }
};
