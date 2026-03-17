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
// Extended keys are encoded with BurritoCoin-specific version bytes:
//   EXT_PUBLIC_KEY  = 0x0188D9CE  ("Ktub..." prefix)
//   EXT_SECRET_KEY  = 0x0188D26A  ("Ktpv..." prefix)
//
// To regenerate: implement BIP32 master-key derivation with the string
// "BurritoCoin seed", run the three standard BIP32 derivation paths, then
// re-encode the resulting keys with the BRTO version bytes above.

// Path: m / 0H / 1 / 2H / 2 / 1000000000 / 0
TestVector test1 =
  TestVector("000102030405060708090a0b0c0d0e0f")
    ("Ktub27cD8LUHaBN8fFuzfStkr796McjaUMWpYKSoKK3zk7ZX1VRpzPwwavgHSdosq13u7EniAbhkkweAMzNtEzThN5KRebbstpVPtQJiYboL5ni",
     "Ktpv6gRsXZ3kL2qB3PDAQqQNmXJsv43PQ6xAToLmeLHhovmvR8EkfUuWaKa2CFys5qhVCFrX867p3SDQ6yNfWasyxxHvjruK3StaTnPSZMg7xPw",
     0x80000000)
    ("Ktub2Ax4dJ27GTiJkZ1P2rHNXfV1ynVnMneUqCUH6HzNX6CT893mtmLTJvVatk2TPZRejrCZT66mWkMvRwcNEkUZHvCWM4hgUGwThtxogaqUgCz",
     "Ktpv6jmj2Wba2KBM8gJYnEnzT5eoYDobHY5pkgNFRKE5auQrXmrhZrJ2JKPKeLJQCPQ87573tPA26giYDsS2HwunGfs67oKnKh8D8NuJAgqLBmq",
     1)
    ("Ktub2CLF8jhSJCqE8knybE9q8MNJ5P6jC67zLd5GvmNYkGpLKUXPpJC9MQ6iQAM1bGbmfNWcj3BhijUyugw2Q8f4b4N7gVUtc6C6kUSBw7n2Wnf",
     "Ktpv6m9uXxGu44JGWt69LcfT3mY5dpQY7qZLG6yFFncFp62jj7LKVP9iLnzT9ofREoDWMtVXyE6i3bTH3eVgZ7CaWAsWFFrBkTXojJ2FJNWtd4b",
     0x80000002)
    ("Ktub2DgBqnWUNzq2nPcJmjaX7teAE8ZzF7GP4y8PJvzvPqJrfkTYmUwrrN5d7ppr1rMEJtiVNZVYXYHpJoYFfWNB5SNutSwBcDrdd59qaEGn6ph",
     "Ktpv6nVrF15w8rJ5AWuUX8693JownZsoArhizT2MdxEdTeXG5PGUSZuRqkyMsRFQaP8y24WHGF8JZYGBxQZ92DQrZyhSKUWGytomuFAqGEoigY8",
     2)
    ("Ktub2FvjUZKFxhqkuMXf9eXrJKFkqpjqwkhEm8wLCAsj5w8n1vnXKwXgidoNEH4S5FdruSkNq1NsyHRVpXgYcJsSVX4oNJkexKZWpLspkjqEQod",
     "Ktpv6pkPsmtiiZJoHUppu33UDjRYQG3esW8agcqJXC7S9kMBRZbT12VFi2h6ytKE6XgXQttqcTWTS63VdtVpWY254aAohBZXLTLJ111jLJ5JzVH",
     1000000000)
    ("Ktub2HewJXLx2WrK9VKRDDugiELkGDk92WnsEub5egK5h1dfLDZUiXhFtu3ecFA8anKHD6WcfcaNAw69otMm9mzkYseAaZf2S47m6XmHoLFjEne",
     "Ktpv6rUbhjvQnNKMXccaxcRJdeWXpf3wxGEDAPV3yhYnkpr4jrNQPceptHwPMs8GTndKKUEtBiern9ihWaC7cmr4jagcMevmedEPRVPvvtju3V6",
     0);

// Path: m / 0 / 2147483647H / 1 / 2147483646H / 2 / 0
TestVector test2 =
  TestVector("fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542")
    ("Ktub27cD8LUHaBN8fyJvUGbcQzDmLh2132nYPw2NGrWdiWKKUxUsGtHNkgFwrS37PR5LMq2RzAFJW21e4nYb3aG6jwjJqvW3HZ7ijEwQYjWTXzf",
     "Ktpv6gRsXZ3kL2qB46c6Df7ELQPYu8KoxnDtKQvLbskLnKXitbHnwyEwk59gc4mJ18GiTkKTWTHPyZWFtAdx2LfCiG74a3Ci3TdmTFonkmJhdLD",
     0)
    ("Ktub2A34eNLwCPknjo6YXJ8spMMMuqHPsqdJn6d6xM4xfjRf4wxk1PnpCbgQGyjfUcRFcWwGYHq6Z4udYXvXR8Z7vhEDQ7MQZ7G31pynotBs7Bu",
     "Ktpv6irj3avPxFDq7vPiGgeVjmX9UGbCob4ehaX5HNJfjYe4UamfgUkPBza92dequmKcDCGskH3cYJGAekcKhTm1wVyS1jjKeqrFRtcZgeWZgpC",
     0xFFFFFFFF)
    ("Ktub2Bb8HezuQQAwXhAqwCxgm91zSLTaBA1iQNsa4UwH7ZduWBd84R6WuTecGqRBN57cizEAqvEZsEz3gZCgNAYK7YxRadKYKgv7RKqu7ZsxuEd",
     "Ktpv6kQngsaNAFdyupU1gbUJgZBmzmmP6uT4KrmYPWAzBNrJupS3jW45trYM2SgGoWG2GzFbF6bc3AKKVNfnajLMmSAgrAKNYd4bbpPYUnrQf3A",
     1)
    ("Ktub2DL1Nim4smE81TiWNmdPb6ojwgwYPuWPK2vnj8gAj2ihKb71VjSKSbYF5DQFppu5coSAV5PNWSkiYvDG68d2jY6BNDyWEJvRLHVkvFSB8jL",
     "Ktpv6n9fmwLXdchAPb1g8A91WWyXW8FMKewjEWpm49usnqw6jDuwApPtRzRypoVMA9LYYZSzt2Xx1NGaYF5337BwWDvvL6k6Zn5eE8ydNGhzwiN",
     0xFFFFFFFE)
    ("Ktub2GZCqiiiapB9u65cjRsVwxHG48wgPdKnDQq3RnCgUfacNSqQq3YMyExNMx82NSKb4swnd4DEXB2NDj8e7nb2YrpEg6Giv8CxPvbY3xyTsWX",
     "Ktpv6qNsEwJBLfeCHDNnUpP7sNT3caFVKNm88tj1koSPYUo1n5eLW8Vvxdr77abGQ6x2PoMHy3HKieMSP97jUUsCaEVs7RQ6uAhdVJVzvTRyPUt",
     2)
    ("Ktub2J7NCuUqQxyspJUk6AepzuGkYGjk3TM81mw9ZQcLdUxKefTBzcJANrx6aHKLRQZETEuwsCs11vEoqtd47i8DeVHS5j8GsMSVnoRkisg1t2s",
     "Ktpv6rw2c84JApSvCRmuqZASvKSY6i3YyCnTwFq7tRr3hJAj4JG7fhFjNFqqKsX4jbWn5hEC5QzrhWnczdT8tahKehQUiMdwErBkRCmLYCBrJic",
     0);

// Path: m / 0H / 0
TestVector test3 =
  TestVector("4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be")
    ("Ktub27cD8LUHaBN8fjf7thYVJEFWFKujNsNrUZauW5GqbUhywSubPio2wUnJ4WaB5rZa81SpWMK2j17yffXre2onnLVNnhvbvLjMuv2oDHzxZHj",
     "Ktpv6gRsXZ3kL2qB3rxHe647DeRHomDYJcpCQ3Usq6WYfHvPM5iX4okbvsg2p9yNnkks7VaPJgYhx66uZXLFStx3Sb7h6H28T2VX7v7Lh22k7S1",
      0x80000000)
    ("Ktub29xzVU6xCJRmqZvw7CojgWPuFmx9da8wjC2BZtAFJvactU4Zk2QVsjh5LWrgQTjFphYqxdwLjxwkmmfZm73rM7d85wRtNEZoourTKiySMPe",
     "Ktpv6inetggQx9tpDhE6rbKMbvZgpDFxZKaHefv9tuPxNjo2J6sVR7N4s8ap67Z67vDF9khBeVc38g1S4LBJ1RZDoukvBCDKqBUWjzNxRgbbdjp",
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
