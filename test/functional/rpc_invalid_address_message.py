#!/usr/bin/env python3
# Copyright (c) 2020 The BurritoCoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test error messages for 'getaddressinfo' and 'validateaddress' RPC commands."""

from test_framework.test_framework import BurritoCoinTestFramework

from test_framework.util import assert_raises_rpc_error

BECH32_VALID = 'rbrto1qhku5rq7jz8ulufe2y6fkcpnlvpsta7rq88nf4w'
BECH32_INVALID_BECH32 = 'rbrto1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqryjgs9'
BECH32_INVALID_BECH32M = 'rbrto1qw508d6qejxt0g49s79gr565qdrqggdj47668mt'
BECH32_INVALID_VERSION = 'rbrto13w508d6qejxt0g49s79gr565qdrqggdj4pguce5'
BECH32_INVALID_SIZE = 'rbrto1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqlfgghs'
BECH32_INVALID_V0_SIZE = 'rbrto1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq5f5zft'
BECH32_INVALID_PREFIX = 'bc1pw508d6qejxtdg4y5r3zarvary0c5xw7kw508d6qejxtdg4y5r3zarvary0c5xw7k7grplx'

BASE58_VALID = 'mipcBbFg9gMiCh81Kj8tqqdgoZub1ZJRfn'
BASE58_INVALID_PREFIX = '17VZNX1SN5NtKa8UQFxwQbFeFc3iqRYhem'

INVALID_ADDRESS = 'asfah14i8fajz0123f'

class InvalidAddressErrorMessageTest(BurritoCoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def test_validateaddress(self):
        node = self.nodes[0]

        # Bech32
        info = node.validateaddress(BECH32_INVALID_SIZE)
        assert not info['isvalid']

        info = node.validateaddress(BECH32_INVALID_PREFIX)
        assert not info['isvalid']

        info = node.validateaddress(BECH32_INVALID_BECH32)
        assert not info['isvalid']

        info = node.validateaddress(BECH32_INVALID_BECH32M)
        assert not info['isvalid']

        info = node.validateaddress(BECH32_INVALID_V0_SIZE)
        assert not info['isvalid']

        info = node.validateaddress(BECH32_VALID)
        assert info['isvalid']
        assert 'error' not in info

        # Base58
        info = node.validateaddress(BASE58_INVALID_PREFIX)
        assert not info['isvalid']

        info = node.validateaddress(BASE58_VALID)
        assert info['isvalid']
        assert 'error' not in info

        # Invalid address format
        info = node.validateaddress(INVALID_ADDRESS)
        assert not info['isvalid']

    def test_getaddressinfo(self):
        node = self.nodes[0]

        assert_raises_rpc_error(-5, "Invalid address", node.getaddressinfo, BECH32_INVALID_SIZE)

        assert_raises_rpc_error(-5, "Invalid address", node.getaddressinfo, BECH32_INVALID_PREFIX)

        assert_raises_rpc_error(-5, "Invalid address", node.getaddressinfo, BASE58_INVALID_PREFIX)

        assert_raises_rpc_error(-5, "Invalid address", node.getaddressinfo, INVALID_ADDRESS)

    def run_test(self):
        self.test_validateaddress()
        self.test_getaddressinfo()


if __name__ == '__main__':
    InvalidAddressErrorMessageTest().main()
