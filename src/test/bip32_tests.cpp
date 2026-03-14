// Copyright (c) 2013-2020 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <clientversion.h>
#include <key.h>
#include <key_io.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>

#include <string>
#include <vector>

struct TestDerivation {
    std::string pub;
    std::string prv;
    unsigned int nChild;
};

struct TestVector {
    std::string strHexMaster;
    std::vector<TestDerivation> vDerive;

    explicit TestVector(std::string strHexMasterIn) : strHexMaster(strHexMasterIn) {}

    TestVector& operator()(std::string pub, std::string prv, unsigned int nChild) {
        vDerive.push_back(TestDerivation());
        TestDerivation &der = vDerive.back();
        der.pub = pub;
        der.prv = prv;
        der.nChild = nChild;
        return *this;
    }
};

// Test vectors generated using BurritoCoin's "BurritoCoin seed" HMAC-SHA512 key
// (see key.cpp:SetSeed). The derivation paths are identical to the original
// BIP32 specification vectors but the master key IL/IR differ because the
// HMAC key string is "BurritoCoin seed" rather than "Bitcoin seed".
//
// To regenerate: implement BIP32 master-key derivation with the string
// "BurritoCoin seed" and re-run the three standard BIP32 derivation paths.

// Path: m / 0H / 1 / 2H / 2 / 1000000000 / 0
TestVector test1 =
  TestVector("000102030405060708090a0b0c0d0e0f")
    ("xpub661MyMwAqRbcGBqAuKgPCL8NSnZTfBzBL9m3GdCmkd6hLDcFb3cmPLqz6yFSAWJZvfVwKwumNaML9BdL3nnGKPDcDoQqp3Yqfdc22azBWq9",
     "xprv9s21ZrQH143K3hkhoJ9NqCBdtkiyFjGKxvqSUEoACHZiTRH73WJWqYXWFhQSD2iDgnQ3o44rM1sHzks9qAK285cBedeZKQf2w7qBtQbyC7o",
     0x80000000)
    ("xpub69MDUKUzXhwnMUvZGj4zstUJ4xKfYd7qd2nX3c99XbjdSsECVR1H7LfHZ5U1j4gKZGuncSJn8P56D8rp3Yo8FE6gvGWePVzuV8G7AZxASxx",
     "xprv9vMs4ox6hLPV8zr6AhXzWkXZWvVB9APzForvFDjXyGCea4u3wsh2ZYLohmiyKaQrbbeaZM74QGNS7evWcXLpRoBM2a52betfbiM3Vrv9DRt",
     1)
    ("xpub6AjPymAKZT4hjgi9q6wTUaMaAYvcNvbM8TPWt5XKknMWeChpQwry9pGR4VnZvmrSUoDqtPPiLNC9gtBUCvydYNGJFhHrXKFYXhjVRDLFJ7j",
     "xprv9wk3aFdRj5WQXCdgj5QT7SQqcX67yTsVmETv5h7iCSpXmQNfsQYic1wwDF5zMzEErR34eC3kMB7AwRzAsgdcfJBmA2bS2RJGCdTzdQGo5Zg",
     0x80000002)
    ("xpub6C5LgoyMeF4WPKXV1cN9U7dSKJPsRwjjroSdGF9hQLr2zUdyN8cgenFKnAGQMMbu8KRiXuhZ9Azz5znhUJgk2kH6Tek9XSv5QJT94HWHK9X",
     "xprv9y5zHJSTosWDAqT1uaq96yghmGZP2V1tVaX2Trk5r1K47gJppbJS6yvqvrfyha9hWb3owD5Ls7v5rC3dLnqtj71hEFFXFraENacabRVW4ar",
     2)
    ("xpub6EKtKan9Dx5EWHSqPXKUeYF2vzZj8bAbYyFa9V2W6SfxLexwvbCWX3y4tcVzQktXisTbzMatav8fbivzR7C1SpxywWZcsYcxbaB8Ejnwvrp",
     "xprvA1LXv5FFPaWwHoNNHVnUHQJJNxjEj8SkBkKyM6ctY78yTrdoP3tFyFeb3KjoDihFuRSNHRTVjfhPXfzJq7T7DhV4bxJmcR6kULTUfLKHojx",
     1000000000)
    ("xpub6G469YoqHm5nkREbT6hK4TL2MPa2DMGE2juKbzTrhXAqewjuKBN5hKDMGabgvHZx2XDqpxnNnZoKb5cCxaKKWBYM9mTzMHBCsm4bHRDMWcE",
     "xprvA34jk3GwTPXVXwA8M5AJhKPHoMjXotYNfWyioc4F9Bdrn9Qkme3q9WtsRJYqaye3oznQrgbu5jNbQMgbwMH6thzsGRg1vazqtpqgG71mTxk",
     0);

// Path: m / 0 / 2147483647H / 1 / 2147483646H / 2 / 0
TestVector test2 =
  TestVector("fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542")
    ("xpub661MyMwAqRbcGuE6i9PEmDD3RrqtDsFuBmLcEAfQj1rVogfHsXxCZ6ReWmUfivL1BFjf9WTK7eioqyo2rNafhFdVR8K1CnBAWUEi2h14rC9",
     "xprv9s21ZrQH143K4R9dc7rEQ5GJsq1PpQY3pYR1RnFoAgKWvtL9Kzdx1J7AfWBs8KHSxGrzBRESH9A9mx8SLv6EsPRKUowxKRQDvbFY5u4qYNU",
     0)
    ("xpub68SDVPopTdzGLj1imAvWAaLe117H4g6fZvwLufDjgExqPg9Ac3Te11r6wKBDp7fvRweVhe37AhcoKjAyDvsgt18PyKANULKUo4H6HzbnLsY",
     "xprv9uSs5tGvdGRy8EwFf9PVoSPuSyGnfDNpCi1k7Gp87uRrWsp24W9PTDXd655R2xLLhipQREzeqsv4YY6p23C46dHgvWUZvochuE4K1n27CKN",
     0xFFFFFFFF)
    ("xpub69zH8gTnfeQR8d62B5kK7N1GXWHTMzV5CDBp1o6485B5puoYf4mLhspJwArjhaNHYQwQ1GSaUshDTkT8Axrt4rrc9q8WEuyZCZ9CbcnrLuf",
     "xprv9vzvjAvtqGr7v91Z54DJkE4XyUSxxXmDpzGDDQgSZje6x7UQ7XT6A5Vq5t6qvhGkmWo7v4YeLjyDPAAGuJmPvZUwkw4cpaq459qHotNTAYB",
     1)
    ("xpub6BjADkDx91TbcPdgceR1wKo22rmRajyk6sF2gSpwjYFseKHS6P79F1hwjYqpAL9kSE9PeRbP85TtL7ThtvwbgqzMwRnU9Xys7Wo4QN6b6DB",
     "xprv9xjopEh4JduJPuZDWct1aBrHUpvwBHFtjeKRt4RLBCitmWxHYqnthDPTtEuvHLMH35zXYzUzJwvUS2ZXMgcyfMFBEsVLqjr6hURNhP3uc8n",
     0xFFFFFFFE)
    ("xpub6ExMgkBbr4QdW1znyJf8JBGY9JmZaTo91F9HP6MTVB7nhB1qRhDBmf852HZahwaFtJf1nQRF8ojXzvP5vaubWAiRFJ5gqMGQB9tqXxbUB2V",
     "xprvA1y1HEei1grLHXvKsH87w3KobGw5B15He2DgahwqvqaopNggt9twDrobB21qXHxktKtpe1EN2E1LGvcDo4JEjMp82C9MB8U5xdwkFdGQdyf",
     2)
    ("xpub6GWX3vwigDDMREPvL3STM8G2dSZdEHpUocFPWim7dzVVyPdcbFxzBH7oEcktkuouGfdB2Z51dYwyd5sVvWSnboBcevwEnaVwa2j4CsoukiW",
     "xprvA3XAeRQpqqf4CkKTE1uSyzKJ5Qj8pq6dSPKniLMW5exX6bJU3iejdUoKPJwdrnXWaDmikNwu16SWtQwdDA8Mopijd8PBWoxCtYD5sJpu31B",
     0);

// Path: m / 0H / 0
TestVector test3 =
  TestVector("4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be")
    ("xpub661MyMwAqRbcGfaJ8aL7eTEnLVjcZhrDGPu9TPRcbzFAGB61zNTrjtwzir1jRMpEwSA3fhX3Ldq9SrnJSq8MjePZMujZqZnoh9L6hNRUKfg",
     "xprv9s21ZrQH143K4BVq2Yo7HKJ3nTu8AF8MuAyYf1213eiBPNksSq9cC6dWsbPwuwmbc27uyeVkFfkoTJpjmUP5biRx13mNizFybFZ62Do6YyV",
      0x80000000)
    ("xpub68N9LVZqTYfFSVr7M5bN2jPBLwn2pQcJX2LRXCK2KS7oDCEzLg5Kg9rmzrJEjxyve8G57z9MMbevYxv1ZuNRJRXJf9ErHTdFb99kokLPy2F",
     "xprv9uNnvz2wdB6xE1meF44MfbSSnuwYQwtT9oQpiouQm6apLPuqo8m58MYJ9YyfF7DyeHEiKTZ5SFfKx7fnKzzFy35B5xxa79EyDKphkr46wgG",
      0);

static void RunTest(const TestVector &test) {
    std::vector<unsigned char> seed = ParseHex(test.strHexMaster);
    CExtKey key;
    CExtPubKey pubkey;
    key.SetSeed(seed.data(), seed.size());
    pubkey = key.Neuter();
    for (const TestDerivation &derive : test.vDerive) {
        unsigned char data[74];
        key.Encode(data);
        pubkey.Encode(data);

        // Test private key
        BOOST_CHECK(EncodeExtKey(key) == derive.prv);
        BOOST_CHECK(DecodeExtKey(derive.prv) == key); //ensure a base58 decoded key also matches

        // Test public key
        BOOST_CHECK(EncodeExtPubKey(pubkey) == derive.pub);
        BOOST_CHECK(DecodeExtPubKey(derive.pub) == pubkey); //ensure a base58 decoded pubkey also matches

        // Derive new keys
        CExtKey keyNew;
        BOOST_CHECK(key.Derive(keyNew, derive.nChild));
        CExtPubKey pubkeyNew = keyNew.Neuter();
        if (!(derive.nChild & 0x80000000)) {
            // Compare with public derivation
            CExtPubKey pubkeyNew2;
            BOOST_CHECK(pubkey.Derive(pubkeyNew2, derive.nChild));
            BOOST_CHECK(pubkeyNew == pubkeyNew2);
        }
        key = keyNew;
        pubkey = pubkeyNew;
    }
}

BOOST_FIXTURE_TEST_SUITE(bip32_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bip32_test1) {
    RunTest(test1);
}

BOOST_AUTO_TEST_CASE(bip32_test2) {
    RunTest(test2);
}

BOOST_AUTO_TEST_CASE(bip32_test3) {
    RunTest(test3);
}

BOOST_AUTO_TEST_SUITE_END()
