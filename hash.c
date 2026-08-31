#include "hash.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * Implements the xxh3 64 bit hash algorithm for small inputs
 * https://github.com/Cyan4973/xxHash/blob/dev/doc/xxhash_spec.md#xxh3-algorithm-overview
 */

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
// NON STANDARD C!
typedef __uint128_t u128;

// static const u64 PRIME32_1 = 0x9E3779B1U;  // 0b10011110001101110111100110110001
// static const u64 PRIME32_2 = 0x85EBCA77U;  // 0b10000101111010111100101001110111
// static const u64 PRIME32_3 = 0xC2B2AE3DU;  // 0b11000010101100101010111000111101
// static const u64 PRIME64_1 = 0x9E3779B185EBCA87ULL;  // 0b1001111000110111011110011011000110000101111010111100101010000111
static const u64 PRIME64_2 = 0xC2B2AE3D27D4EB4FULL;  // 0b1100001010110010101011100011110100100111110101001110101101001111
static const u64 PRIME64_3 = 0x165667B19E3779F9ULL;  // 0b0001011001010110011001111011000110011110001101110111100111111001
// static const u64 PRIME64_4 = 0x85EBCA77C2B2AE63ULL;  // 0b1000010111101011110010100111011111000010101100101010111001100011
// static const u64 PRIME64_5 = 0x27D4EB2F165667C5ULL;  // 0b0010011111010100111010110010111100010110010101100110011111000101
static const u64 PRIME_MX1 = 0x165667919E3779F9ULL;  // 0b0001011001010110011001111001000110011110001101110111100111111001
static const u64 PRIME_MX2 = 0x9FB21C651E98DF25ULL;  // 0b1001111110110010000111000110010100011110100110001101111100100101

static const u8 defaultSecret[192] = {
  0xb8, 0xfe, 0x6c, 0x39, 0x23, 0xa4, 0x4b, 0xbe, 0x7c, 0x01, 0x81, 0x2c, 0xf7, 0x21, 0xad, 0x1c,
  0xde, 0xd4, 0x6d, 0xe9, 0x83, 0x90, 0x97, 0xdb, 0x72, 0x40, 0xa4, 0xa4, 0xb7, 0xb3, 0x67, 0x1f,
  0xcb, 0x79, 0xe6, 0x4e, 0xcc, 0xc0, 0xe5, 0x78, 0x82, 0x5a, 0xd0, 0x7d, 0xcc, 0xff, 0x72, 0x21,
  0xb8, 0x08, 0x46, 0x74, 0xf7, 0x43, 0x24, 0x8e, 0xe0, 0x35, 0x90, 0xe6, 0x81, 0x3a, 0x26, 0x4c,
  0x3c, 0x28, 0x52, 0xbb, 0x91, 0xc3, 0x00, 0xcb, 0x88, 0xd0, 0x65, 0x8b, 0x1b, 0x53, 0x2e, 0xa3,
  0x71, 0x64, 0x48, 0x97, 0xa2, 0x0d, 0xf9, 0x4e, 0x38, 0x19, 0xef, 0x46, 0xa9, 0xde, 0xac, 0xd8,
  0xa8, 0xfa, 0x76, 0x3f, 0xe3, 0x9c, 0x34, 0x3f, 0xf9, 0xdc, 0xbb, 0xc7, 0xc7, 0x0b, 0x4f, 0x1d,
  0x8a, 0x51, 0xe0, 0x4b, 0xcd, 0xb4, 0x59, 0x31, 0xc8, 0x9f, 0x7e, 0xc9, 0xd9, 0x78, 0x73, 0x64,
  0xea, 0xc5, 0xac, 0x83, 0x34, 0xd3, 0xeb, 0xc3, 0xc5, 0x81, 0xa0, 0xff, 0xfa, 0x13, 0x63, 0xeb,
  0x17, 0x0d, 0xdd, 0x51, 0xb7, 0xf0, 0xda, 0x49, 0xd3, 0x16, 0x55, 0x26, 0x29, 0xd4, 0x68, 0x9e,
  0x2b, 0x16, 0xbe, 0x58, 0x7d, 0x47, 0xa1, 0xfc, 0x8f, 0xf8, 0xb8, 0xd1, 0x7a, 0xd0, 0x31, 0xce,
  0x45, 0xcb, 0x3a, 0x8f, 0x95, 0x16, 0x04, 0x28, 0xaf, 0xd7, 0xfb, 0xca, 0xbb, 0x4b, 0x40, 0x7e,
};

static u64 derivedSecret[24] = { 0 };

static u64 seed = 0;

void derive_secret(u64 s) {
    // u64 derivedSecret[24] = defaultSecret[0:192];
    for (int i = 0; i < 24; ++i) {
        for (int j = 0; j < 8; ++j) {
            derivedSecret[i] |= ((u64)defaultSecret[j + i*8] << (7-j));
        }
    }
    for (int i = 0; i < 12; i++) {
        derivedSecret[i*2] += s;
        derivedSecret[i*2+1] -= s;
    }
    seed = s;
}

static u64 avalanche(u64 x) {
  x = x ^ (x >> 37);
  x = x * PRIME_MX1;
  x = x ^ (x >> 32);
  return x;
}

static u64 avalanche_XXH64(u64 x) {
  x = x ^ (x >> 33);
  x = x * PRIME64_2;
  x = x ^ (x >> 29);
  x = x * PRIME64_3;
  x = x ^ (x >> 32);
  return x;
}

// empty input
static u64 XXH3_64_empty(void) {
    // u64 secretWords[2] = secret[56:72];
    u64 secretWords[2] = {
        derivedSecret[7],
        derivedSecret[8]
    };
    return avalanche_XXH64(seed ^ secretWords[0] ^ secretWords[1]); 
}

static u64 lower_half_64(u64 i) {
    return i & 0x00000000FFFFFFFF;
}
static u64 lower_half_128(u128 i) {
    return (u64)i;
}
static u64 higher_half_128(u128 i) {
    return (u64)(i >> 64);
}

static u32 bswap32(u32 i) {
    u32 j = 0;
    j |= (i & 0xFF000000) >> 24;
    j |= (i & 0x00FF0000) >> 8;
    j |= (i & 0x0000FF00) << 8;
    j |= (i & 0x000000FF) << 24;
    return j;
}

static u64 bswap64(u64 i) {
    u64 j = 0;
    j |= (i & 0xFF00000000000000) >> 56;
    j |= (i & 0x00FF000000000000) >> 40;
    j |= (i & 0x0000FF0000000000) >> 24;
    j |= (i & 0x000000FF00000000) >> 8;
    j |= (i & 0x00000000000000FF) << 56;
    j |= (i & 0x000000000000FF00) << 40;
    j |= (i & 0x0000000000FF0000) << 24;
    j |= (i & 0x00000000FF000000) << 8;
    return j;
}

static u64 rotate_left(u64 num, u64 rot) {
    u64 m = num << rot;
    m |= num >> (64-rot);
    return m;
}

// inputs of length 1 to 3
static u64 XXH3_64_1to3(char* input, size_t input_len) {
    assert(input_len <= 3);
    // LSB          8       16           24                    MSB
    //  | last byte | length | first byte | middle-or-last byte |
    u32 combined = (u32)input[input_len-1] | ((u32)input_len << 8) | ((u32)input[0] << 16) | ((u32)input[input_len>>1] << 24);
    // u32 secretWords[2] = secret[0:8];
    u32 secretWords[2] = {
        (u32)(derivedSecret[0] >> 32),
        (u32)lower_half_64(derivedSecret[0])
    };
    u64 value = ((u64)(secretWords[0] ^ secretWords[1]) + seed) ^ (u64)combined;
    return avalanche_XXH64(value);
}

// inputs of length 4-8
static u64 XXH3_64_4to8(char* input, size_t input_len) {
    // u32 inputFirst = input[0:4];
    u32 input_first = (u32)(input[0] << 24 | input[1] << 16 | input[2] << 8 | input[3]);
    // u32 inputLast = input[input_len-4:input_len];
    u32 input_last = (u32)(input[input_len-4] << 24 | input[input_len-3] << 16 | input[input_len-2] << 8 | input[input_len-1]);
    u64 modified_seed = seed ^ ((u64)bswap32((u32)lower_half_64(seed)) << 32);
    // u64 secretWords[2] = secret[8:24];
    u64 secret_words[2] = {
        derivedSecret[1],
        derivedSecret[2],
    };
    u64 combined = (u64)input_last | ((u64)input_first << 32);
    u64 value = ((secret_words[0] ^ secret_words[1]) - modified_seed) ^ combined;
    value = value ^ rotate_left(value, 49) ^ rotate_left(value, 24);
    value = value * PRIME_MX2;
    value = value ^ ((value >> 35) + input_len);
    value = value * PRIME_MX2;
    value = value ^ (value >> 28);
    return value;
}

static u64 XXH3_64_9to16(char* input, size_t input_len) {
    // u64 inputFirst = input[0:8];
    u64 input_first = (u64)input[0] << 56 | (u64)input[1] << 48 | (u64)input[2] << 40 | (u64)input[3] << 32
        | (u64)input[4] << 24 | (u64)input[5] << 16 | (u64)input[6] << 8 | (u64)input[7];
    // u64 inputLast = input[inputLength-8:inputLength];
    u64 input_last = (u64)input[input_len-8] << 56 | (u64)input[input_len-7] << 48 | (u64)input[input_len-6] << 40 | (u64)input[input_len-5] << 32
        | (u64)input[input_len-4] << 24 | (u64)input[input_len-3] << 16 | (u64)input[input_len-2] << 8 | (u64)input[input_len-1];
    // u64 secretWords[4] = secret[24:56];
    u64 secret_words[4] = {
        derivedSecret[3],
        derivedSecret[4],
        derivedSecret[5],
        derivedSecret[6]
    };
    u64 low = ((secret_words[0] ^ secret_words[1]) + seed) ^ input_first;
    u64 high = ((secret_words[2] ^ secret_words[3]) - seed) ^ input_last;
    u128 mulResult = (u128)low * (u128)high;
    u64 value = input_len + bswap64(low) + high + (u64)(lower_half_128(mulResult) ^ higher_half_128(mulResult));
    return avalanche(value);
}

uint64_t hash(char* str) {
    size_t len = strlen(str);
    if (len == 0) {
        return XXH3_64_empty();
    }
    if (len >= 1 && len <= 3) {
        return XXH3_64_1to3(str, len);
    }
    if (len >= 4 && len <= 8) {
        return XXH3_64_4to8(str, len);
    }
    if (len >= 9 && len <= 16) {
        return XXH3_64_9to16(str, len);
    }
    fprintf(stderr, "hash: string %s of length %zu not supported", str, len);
    assert(false);
}