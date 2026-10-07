/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Public synthetic envelopes projected from original Zclassic 14a83d510ffd109d3fa09bf74ebf8c28854a263f. */
#ifndef SHIELDED_ADDRESS_VECTORS_H
#define SHIELDED_ADDRESS_VECTORS_H
static const struct {
    zcl_shielded_address_kind kind;
    zcl_network network;
    unsigned pattern;
    bool valid;
    const char *text;
} shielded_vectors[] = {
    {ZCL_SPROUT_ADDRESS, ZCL_MAINNET, 0, true,
     "zc8E5gYid86n4bo2Usdq1cpr7PpfoJGzttwBHEEgGhGkLUg7SPPVFNB2AkRFXZ7usfphup5426dt1buMmY3fkYeRrQGLa8y"},
    {ZCL_SPROUT_ADDRESS, ZCL_TESTNET, 0, true,
     "ztJ1EWLKcGwF2S4NA17pAJVdco8Sdkz4AQPxt1cLTEfNuyNswJJc2BbBqYrsRZsp31xbVZwhF7c7a2L9jsF3p3ZwRWpqqyS"},
    {ZCL_SAPLING_ADDRESS, ZCL_MAINNET, 0, true,
     "zs1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqpq6d8g"},
    {ZCL_SAPLING_ADDRESS, ZCL_TESTNET, 0, true,
     "ztestsapling1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqfhgwqu"},
    {ZCL_SAPLING_ADDRESS, ZCL_TESTNET, 0, false,
     "zregtestsapling1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqknpr3m"},
    {ZCL_SAPLING_ADDRESS, ZCL_MAINNET, 0, false,
     "zs1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqpukwc66"},
    {ZCL_SAPLING_ADDRESS, ZCL_TESTNET, 0, false,
     "ztestsapling1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqp5pumaw"},
    {ZCL_SPROUT_ADDRESS, ZCL_MAINNET, 1, true,
     "zchid4y8fAVDtAb9Q4G3QiPJbNaocboTwXCdmQ9EhvpdMg4HK8hxRFafoMDjjmaLXHkqy68w4dk2nG4XhLJaKpBLh3RCRgC"},
    {ZCL_SPROUT_ADDRESS, ZCL_TESTNET, 1, true,
     "ztsVmtkjeKKgqzrV5Bk2ZQ466mtaT4WXD2fRNBWttUDFwAm3p3d5C4zqU9fMdnLEgdtjYr1aHeiGLgVKffVxPK6rGE8Z4rh"},
    {ZCL_SAPLING_ADDRESS, ZCL_MAINNET, 1, true,
     "zs1llllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllll74ty9uk"},
    {ZCL_SAPLING_ADDRESS, ZCL_TESTNET, 1, true,
     "ztestsapling1llllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllll7aukxmz"},
    {ZCL_SAPLING_ADDRESS, ZCL_TESTNET, 1, false,
     "zregtestsapling1llllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllll7zclt29"},
    {ZCL_SAPLING_ADDRESS, ZCL_MAINNET, 1, false,
     "zs1lllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllgasspy"},
    {ZCL_SAPLING_ADDRESS, ZCL_TESTNET, 1, false,
     "ztestsapling1lllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllq2znxs"},
    {ZCL_SPROUT_ADDRESS, ZCL_MAINNET, 2, true,
     "zc8E7R3StiJq1T1UaCdygazuEVBe9xddGdYBLMe8WNgnBTVRGiGwY9MEeVKqhWNtmbPmwi4S1uJtPobqCq4azuLJrKCFjcj"},
    {ZCL_SPROUT_ADDRESS, ZCL_TESTNET, 2, true,
     "ztJ1GEq3ss9HyHGpFL7xqGfgjtVQzRLgY8zxw91ngv5QkxCBmdC4JxmQKHmTbX8nvwXfXTw5EvH7xE2dBAFy4QFpRU7fB5b"},
    {ZCL_SAPLING_ADDRESS, ZCL_MAINNET, 2, true,
     "zs1qqqsyqcyq5rqwzqfpg9scrgwpugpzysnzs23v9ccrydpk8qarc0jqgfzyvjz2f389q5j5ctfvp5"},
    {ZCL_SAPLING_ADDRESS, ZCL_TESTNET, 2, true,
     "ztestsapling1qqqsyqcyq5rqwzqfpg9scrgwpugpzysnzs23v9ccrydpk8qarc0jqgfzyvjz2f389q5j5sum0xq"},
    {ZCL_SAPLING_ADDRESS, ZCL_TESTNET, 2, false,
     "zregtestsapling1qqqsyqcyq5rqwzqfpg9scrgwpugpzysnzs23v9ccrydpk8qarc0jqgfzyvjz2f389q5j50cjzh8"},
    {ZCL_SAPLING_ADDRESS, ZCL_MAINNET, 2, false,
     "zs1qqqsyqcyq5rqwzqfpg9scrgwpugpzysnzs23v9ccrydpk8qarc0jqgfzyvjz2f389q5j49aaeux"},
    {ZCL_SAPLING_ADDRESS, ZCL_TESTNET, 2, false,
     "ztestsapling1qqqsyqcyq5rqwzqfpg9scrgwpugpzysnzs23v9ccrydpk8qarc0jqgfzyvjz2f389q5j4d206mj"},
};
#endif
