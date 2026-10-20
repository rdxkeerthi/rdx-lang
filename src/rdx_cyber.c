/*
 * ============================================================================
 *  rdx_cyber.c  —  RDX Cyber Forensics & Incident Response Engine Implementation
 * ============================================================================
 */

#include "rdx_cyber.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ============================================================================
 *  1. IPv4 and CIDR Subnet Forensics
 * ============================================================================ */

static bool parse_ipv4_u32(const char *ip, uint32_t *out_val, int octets[4]) {
    if (!ip) return false;
    int o[4] = {0};
    int count = 0;
    const char *p = ip;

    while (*p && count < 4) {
        if (!isdigit((unsigned char)*p)) return false;
        long val = 0;
        while (isdigit((unsigned char)*p)) {
            val = val * 10 + (*p - '0');
            if (val > 255) return false;
            p++;
        }
        o[count++] = (int)val;
        if (count < 4) {
            if (*p != '.') return false;
            p++;
        }
    }

    if (count != 4 || *p != '\0') return false;

    if (octets) {
        for (int i = 0; i < 4; i++) octets[i] = o[i];
    }
    if (out_val) {
        *out_val = ((uint32_t)o[0] << 24) |
                   ((uint32_t)o[1] << 16) |
                   ((uint32_t)o[2] << 8)  |
                   ((uint32_t)o[3]);
    }
    return true;
}

bool rdx_ip_is_valid(const char *ip) {
    return parse_ipv4_u32(ip, NULL, NULL);
}

bool rdx_ip_octets(const char *ip, int octets[4]) {
    return parse_ipv4_u32(ip, NULL, octets);
}

bool rdx_ip_is_loopback(const char *ip) {
    int o[4];
    if (!parse_ipv4_u32(ip, NULL, o)) return false;
    return (o[0] == 127);
}

bool rdx_ip_is_private(const char *ip) {
    int o[4];
    if (!parse_ipv4_u32(ip, NULL, o)) return false;

    /* 10.0.0.0/8 */
    if (o[0] == 10) return true;
    /* 172.16.0.0/12 (172.16.x.x - 172.31.x.x) */
    if (o[0] == 172 && (o[1] >= 16 && o[1] <= 31)) return true;
    /* 192.168.0.0/16 */
    if (o[0] == 192 && o[1] == 168) return true;
    /* 127.0.0.0/8 Loopback */
    if (o[0] == 127) return true;
    /* 169.254.0.0/16 Link-local APIPA */
    if (o[0] == 169 && o[1] == 254) return true;

    return false;
}

bool rdx_ip_is_public(const char *ip) {
    if (!rdx_ip_is_valid(ip)) return false;
    return !rdx_ip_is_private(ip);
}

bool rdx_ip_in_subnet(const char *ip, const char *cidr) {
    if (!ip || !cidr) return false;
    uint32_t ip_num = 0;
    if (!parse_ipv4_u32(ip, &ip_num, NULL)) return false;

    char subnet_ip[64];
    int prefix_len = 32;
    const char *slash = strchr(cidr, '/');
    if (slash) {
        size_t ip_part_len = (size_t)(slash - cidr);
        if (ip_part_len >= sizeof(subnet_ip)) return false;
        memcpy(subnet_ip, cidr, ip_part_len);
        subnet_ip[ip_part_len] = '\0';
        prefix_len = atoi(slash + 1);
        if (prefix_len < 0 || prefix_len > 32) return false;
    } else {
        strncpy(subnet_ip, cidr, sizeof(subnet_ip) - 1);
        subnet_ip[sizeof(subnet_ip) - 1] = '\0';
    }

    uint32_t base_ip = 0;
    if (!parse_ipv4_u32(subnet_ip, &base_ip, NULL)) return false;

    uint32_t mask = (prefix_len == 0) ? 0 : (~0U << (32 - prefix_len));
    return (ip_num & mask) == (base_ip & mask);
}

/* ============================================================================
 *  2. Pure C Cryptographic Hashing: MD5, SHA-1, SHA-256
 * ============================================================================ */

/* --- MD5 Implementation (RFC 1321) --- */
typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t  buffer[64];
} RDX_MD5_CTX;

#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (uint32_t)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (uint32_t)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (uint32_t)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (uint32_t)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }

static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    for (int i = 0, j = 0; j < 64; i++, j += 4)
        x[i] = ((uint32_t)block[j]) | (((uint32_t)block[j+1]) << 8) |
               (((uint32_t)block[j+2]) << 16) | (((uint32_t)block[j+3]) << 24);

    FF (a, b, c, d, x[ 0], 7, 0xd76aa478);
    FF (d, a, b, c, x[ 1], 12, 0xe8c7b756);
    FF (c, d, a, b, x[ 2], 17, 0x242070db);
    FF (b, c, d, a, x[ 3], 22, 0xc1bdceee);
    FF (a, b, c, d, x[ 4], 7, 0xf57c0faf);
    FF (d, a, b, c, x[ 5], 12, 0x4787c62a);
    FF (c, d, a, b, x[ 6], 17, 0xa8304613);
    FF (b, c, d, a, x[ 7], 22, 0xfd469501);
    FF (a, b, c, d, x[ 8], 7, 0x698098d8);
    FF (d, a, b, c, x[ 9], 12, 0x8b44f7af);
    FF (c, d, a, b, x[10], 17, 0xffff5bb1);
    FF (b, c, d, a, x[11], 22, 0x895cd7be);
    FF (a, b, c, d, x[12], 7, 0x6b901122);
    FF (d, a, b, c, x[13], 12, 0xfd987193);
    FF (c, d, a, b, x[14], 17, 0xa679438e);
    FF (b, c, d, a, x[15], 22, 0x49b40821);

    GG (a, b, c, d, x[ 1], 5, 0xf61e2562);
    GG (d, a, b, c, x[ 6], 9, 0xc040b340);
    GG (c, d, a, b, x[11], 14, 0x265e5a51);
    GG (b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
    GG (a, b, c, d, x[ 5], 5, 0xd62f105d);
    GG (d, a, b, c, x[10], 9,  0x2441453);
    GG (c, d, a, b, x[15], 14, 0xd8a1e681);
    GG (b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
    GG (a, b, c, d, x[ 9], 5, 0x21e1cde6);
    GG (d, a, b, c, x[14], 9, 0xc33707d6);
    GG (c, d, a, b, x[ 3], 14, 0xf4d50d87);
    GG (b, c, d, a, x[ 8], 20, 0x455a14ed);
    GG (a, b, c, d, x[13], 5, 0xa9e3e905);
    GG (d, a, b, c, x[ 2], 9, 0xfcefa3f8);
    GG (c, d, a, b, x[ 7], 14, 0x676f02d9);
    GG (b, c, d, a, x[12], 20, 0x8d2a4c8a);

    HH (a, b, c, d, x[ 5], 4, 0xfffa3942);
    HH (d, a, b, c, x[ 8], 11, 0x8771f681);
    HH (c, d, a, b, x[11], 16, 0x6d9d6122);
    HH (b, c, d, a, x[14], 23, 0xfde5380c);
    HH (a, b, c, d, x[ 1], 4, 0xa4beea44);
    HH (d, a, b, c, x[ 4], 11, 0x4bdecfa9);
    HH (c, d, a, b, x[ 7], 16, 0xf6bb4b60);
    HH (b, c, d, a, x[10], 23, 0xbebfbc70);
    HH (a, b, c, d, x[13], 4, 0x289b7ec6);
    HH (d, a, b, c, x[ 0], 11, 0xeaa127fa);
    HH (c, d, a, b, x[ 3], 16, 0xd4ef3085);
    HH (b, c, d, a, x[ 6], 23,  0x4881d05);
    HH (a, b, c, d, x[ 9], 4, 0xd9d4d039);
    HH (d, a, b, c, x[12], 11, 0xe6db99e5);
    HH (c, d, a, b, x[15], 16, 0x1fa27cf8);
    HH (b, c, d, a, x[ 2], 23, 0xc4ac5665);

    II (a, b, c, d, x[ 0], 6, 0xf4292244);
    II (d, a, b, c, x[ 7], 10, 0x432aff97);
    II (c, d, a, b, x[14], 15, 0xab9423a7);
    II (b, c, d, a, x[ 5], 21, 0xfc93a039);
    II (a, b, c, d, x[12], 6, 0x655b59c3);
    II (d, a, b, c, x[ 3], 10, 0x8f0ccc92);
    II (c, d, a, b, x[10], 15, 0xffeff47d);
    II (b, c, d, a, x[ 1], 21, 0x85845dd1);
    II (a, b, c, d, x[ 8], 6, 0x6fa87e4f);
    II (d, a, b, c, x[15], 10, 0xfe2ce6e0);
    II (c, d, a, b, x[ 6], 15, 0xa3014314);
    II (b, c, d, a, x[13], 21, 0x4e0811a1);
    II (a, b, c, d, x[ 4], 6, 0xf7537e82);
    II (d, a, b, c, x[11], 10, 0xbd3af235);
    II (c, d, a, b, x[ 2], 15, 0x2ad7d2bb);
    II (b, c, d, a, x[ 9], 21, 0xeb86d391);

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

static void md5_init(RDX_MD5_CTX *ctx) {
    ctx->count[0] = ctx->count[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

static void md5_update(RDX_MD5_CTX *ctx, const uint8_t *input, size_t inputLen) {
    uint32_t i, index, partLen;
    index = (uint32_t)((ctx->count[0] >> 3) & 0x3F);
    if ((ctx->count[0] += ((uint32_t)inputLen << 3)) < ((uint32_t)inputLen << 3))
        ctx->count[1]++;
    ctx->count[1] += ((uint32_t)inputLen >> 29);
    partLen = 64 - index;

    if (inputLen >= partLen) {
        memcpy(&ctx->buffer[index], input, partLen);
        md5_transform(ctx->state, ctx->buffer);
        for (i = partLen; i + 63 < inputLen; i += 64)
            md5_transform(ctx->state, &input[i]);
        index = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[index], &input[i], inputLen - i);
}

static void md5_final(uint8_t digest[16], RDX_MD5_CTX *ctx) {
    uint8_t bits[8];
    for (int i = 0; i < 4; i++) {
        bits[i] = (uint8_t)((ctx->count[0] >> (i * 8)) & 0xff);
        bits[i + 4] = (uint8_t)((ctx->count[1] >> (i * 8)) & 0xff);
    }
    uint32_t index = (uint32_t)((ctx->count[0] >> 3) & 0x3f);
    uint32_t padLen = (index < 56) ? (56 - index) : (120 - index);
    static const uint8_t PADDING[64] = { 0x80 };
    md5_update(ctx, PADDING, padLen);
    md5_update(ctx, bits, 8);
    for (int i = 0; i < 4; i++) {
        digest[i*4]   = (uint8_t)(ctx->state[i] & 0xff);
        digest[i*4+1] = (uint8_t)((ctx->state[i] >> 8) & 0xff);
        digest[i*4+2] = (uint8_t)((ctx->state[i] >> 16) & 0xff);
        digest[i*4+3] = (uint8_t)((ctx->state[i] >> 24) & 0xff);
    }
}

void rdx_hash_md5(const char *input, size_t len, char out_hex[33]) {
    RDX_MD5_CTX ctx;
    uint8_t digest[16];
    md5_init(&ctx);
    md5_update(&ctx, (const uint8_t*)input, len);
    md5_final(digest, &ctx);
    for (int i = 0; i < 16; i++) {
        snprintf(&out_hex[i * 2], 3, "%02x", digest[i]);
    }
    out_hex[32] = '\0';
}

/* --- SHA-1 Implementation (RFC 3174) --- */
typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
} RDX_SHA1_CTX;

static void sha1_transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)buffer[i * 4] << 24) |
               ((uint32_t)buffer[i * 4 + 1] << 16) |
               ((uint32_t)buffer[i * 4 + 2] << 8) |
               ((uint32_t)buffer[i * 4 + 3]);
    }
    for (int i = 16; i < 80; i++) {
        w[i] = ROTATE_LEFT(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t temp = ROTATE_LEFT(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = ROTATE_LEFT(b, 30);
        b = a;
        a = temp;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1_init(RDX_SHA1_CTX *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count[0] = ctx->count[1] = 0;
}

static void sha1_update(RDX_SHA1_CTX *ctx, const uint8_t *data, size_t len) {
    uint32_t i, j = (ctx->count[0] >> 3) & 63;
    if ((ctx->count[0] += ((uint32_t)len << 3)) < ((uint32_t)len << 3)) ctx->count[1]++;
    ctx->count[1] += ((uint32_t)len >> 29);
    if ((j + len) > 63) {
        memcpy(&ctx->buffer[j], data, (i = 64 - j));
        sha1_transform(ctx->state, ctx->buffer);
        for (; i + 63 < len; i += 64) sha1_transform(ctx->state, &data[i]);
        j = 0;
    } else i = 0;
    memcpy(&ctx->buffer[j], &data[i], len - i);
}

static void sha1_final(uint8_t digest[20], RDX_SHA1_CTX *ctx) {
    uint8_t finalcount[8];
    for (int i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t)((ctx->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);
    }
    sha1_update(ctx, (const uint8_t*)"\200", 1);
    while ((ctx->count[0] & 504) != 448) sha1_update(ctx, (const uint8_t*)"\0", 1);
    sha1_update(ctx, finalcount, 8);
    for (int i = 0; i < 20; i++) {
        digest[i] = (uint8_t)((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
}

void rdx_hash_sha1(const char *input, size_t len, char out_hex[41]) {
    RDX_SHA1_CTX ctx;
    uint8_t digest[20];
    sha1_init(&ctx);
    sha1_update(&ctx, (const uint8_t*)input, len);
    sha1_final(digest, &ctx);
    for (int i = 0; i < 20; i++) {
        snprintf(&out_hex[i * 2], 3, "%02x", digest[i]);
    }
    out_hex[40] = '\0';
}

/* --- SHA-256 Implementation (FIPS 180-2) --- */
typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  data[64];
    uint32_t datalen;
} RDX_SHA256_CTX;

#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTATE_LEFT(x, 30) ^ ROTATE_LEFT(x, 19) ^ ROTATE_LEFT(x, 10))
#define EP1(x) (ROTATE_LEFT(x, 26) ^ ROTATE_LEFT(x, 21) ^ ROTATE_LEFT(x, 7))
#define SIG0(x) (ROTATE_LEFT(x, 25) ^ ROTATE_LEFT(x, 14) ^ ((x) >> 3))
#define SIG1(x) (ROTATE_LEFT(x, 15) ^ ROTATE_LEFT(x, 13) ^ ((x) >> 10))

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void sha256_transform(RDX_SHA256_CTX *ctx, const uint8_t data[]) {
    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) |
               ((uint32_t)data[j + 2] << 8) | ((uint32_t)data[j + 3]);
    for (; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + sha256_k[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(RDX_SHA256_CTX *ctx) {
    ctx->datalen = 0;
    ctx->bitlen  = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(RDX_SHA256_CTX *ctx, const uint8_t data[], size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(RDX_SHA256_CTX *ctx, uint8_t hash[]) {
    uint32_t i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += (uint64_t)ctx->datalen * 8;
    ctx->data[63] = (uint8_t)(ctx->bitlen);
    ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
    ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
    ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
    ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
    ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
    ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
    ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
    sha256_transform(ctx, ctx->data);

    for (i = 0; i < 4; ++i) {
        hash[i]      = (uint8_t)((ctx->state[0] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 4]  = (uint8_t)((ctx->state[1] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 8]  = (uint8_t)((ctx->state[2] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 12] = (uint8_t)((ctx->state[3] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 16] = (uint8_t)((ctx->state[4] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 20] = (uint8_t)((ctx->state[5] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 24] = (uint8_t)((ctx->state[6] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 28] = (uint8_t)((ctx->state[7] >> (24 - i * 8)) & 0x000000ff);
    }
}

void rdx_hash_sha256(const char *input, size_t len, char out_hex[65]) {
    RDX_SHA256_CTX ctx;
    uint8_t digest[32];
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)input, len);
    sha256_final(&ctx, digest);
    for (int i = 0; i < 32; i++) {
        snprintf(&out_hex[i * 2], 3, "%02x", digest[i]);
    }
    out_hex[64] = '\0';
}

bool rdx_hash_file_md5(const char *filepath, char out_hex[33]) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return false;
    RDX_MD5_CTX ctx;
    md5_init(&ctx);
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        md5_update(&ctx, buf, n);
    }
    fclose(f);
    uint8_t digest[16];
    md5_final(digest, &ctx);
    for (int i = 0; i < 16; i++) {
        snprintf(&out_hex[i * 2], 3, "%02x", digest[i]);
    }
    out_hex[32] = '\0';
    return true;
}

bool rdx_hash_file_sha256(const char *filepath, char out_hex[65]) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return false;
    RDX_SHA256_CTX ctx;
    sha256_init(&ctx);
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha256_update(&ctx, buf, n);
    }
    fclose(f);
    uint8_t digest[32];
    sha256_final(&ctx, digest);
    for (int i = 0; i < 32; i++) {
        snprintf(&out_hex[i * 2], 3, "%02x", digest[i]);
    }
    out_hex[64] = '\0';
    return true;
}

bool rdx_hash_match(const char *h1, const char *h2) {
    if (!h1 || !h2) return false;
    size_t l1 = strlen(h1);
    size_t l2 = strlen(h2);
    if (l1 != l2) return false;
    for (size_t i = 0; i < l1; i++) {
        if (tolower((unsigned char)h1[i]) != tolower((unsigned char)h2[i])) return false;
    }
    return true;
}

/* ============================================================================
 *  3. Network Port & Vulnerability Surface
 * ============================================================================ */

typedef struct {
    int port;
    const char *service;
    const char *risk;
} PortInfo;

static const PortInfo PORT_DB[] = {
    { 21,   "FTP",      "HIGH (Plaintext Credentials)" },
    { 22,   "SSH",      "MEDIUM (Brute Force Target)" },
    { 23,   "Telnet",   "CRITICAL (Unencrypted Legacy Protocol)" },
    { 25,   "SMTP",     "MEDIUM (Mail Relay / Spoofing)" },
    { 53,   "DNS",      "MEDIUM (Amplification / Cache Poisoning)" },
    { 80,   "HTTP",     "LOW (Web Service)" },
    { 110,  "POP3",     "HIGH (Cleartext Email)" },
    { 135,  "RPC",      "CRITICAL (Windows Remote RPC Exploit Surface)" },
    { 137,  "NetBIOS",  "HIGH (Internal Name Poisoning / LLMNR)" },
    { 139,  "NetBIOS",  "HIGH (SMB over NetBIOS)" },
    { 143,  "IMAP",     "MEDIUM (Email Access)" },
    { 443,  "HTTPS",    "LOW (Encrypted Web Traffic)" },
    { 445,  "SMB",      "CRITICAL (Lateral Movement / EternalBlue Surface)" },
    { 1433, "MSSQL",    "CRITICAL (Database Port Exposed)" },
    { 1521, "Oracle",   "CRITICAL (Database Exposed)" },
    { 3306, "MySQL",    "CRITICAL (Database Port Exposed)" },
    { 3389, "RDP",      "CRITICAL (Remote Desktop / Ransomware Entry)" },
    { 5432, "PostgreSQL","HIGH (Database Port Exposed)" },
    { 5900, "VNC",      "CRITICAL (Remote Framebuffer Unauthenticated)" },
    { 6379, "Redis",    "CRITICAL (Unauthenticated Redis In-Memory DB)" },
    { 8080, "HTTP-Proxy","MEDIUM (Alternative Web Port)" },
    { 8443, "HTTPS-Alt","MEDIUM (Alternative Admin Portal)" },
    { 9200, "Elastic",  "CRITICAL (Elasticsearch Unauthorized Access)" },
    { 27017,"MongoDB",  "CRITICAL (Unprotected NoSQL Exposure)" },
    { 0,    NULL,       NULL }
};

const char *rdx_net_port_name(int port) {
    for (int i = 0; PORT_DB[i].service != NULL; i++) {
        if (PORT_DB[i].port == port) return PORT_DB[i].service;
    }
    return "UNKNOWN_PORT";
}

const char *rdx_net_port_risk(int port) {
    for (int i = 0; PORT_DB[i].service != NULL; i++) {
        if (PORT_DB[i].port == port) return PORT_DB[i].risk;
    }
    return (port < 1024) ? "MEDIUM (Privileged Port)" : "INFO (High/Dynamic Port)";
}

/* ============================================================================
 *  4. Forensic Log & Attack Inspection
 * ============================================================================ */

bool rdx_log_extract_ip(const char *log, char out_ip[64]) {
    if (!log) return false;
    const char *p = log;
    while (*p) {
        if (isdigit((unsigned char)*p)) {
            const char *cand = p;
            int octets = 0;
            const char *curr = p;
            while (*curr && (isdigit((unsigned char)*curr) || *curr == '.')) {
                if (*curr == '.') octets++;
                curr++;
            }
            if (octets == 3) {
                size_t len = (size_t)(curr - cand);
                if (len < 64) {
                    char temp[64];
                    memcpy(temp, cand, len);
                    temp[len] = '\0';
                    if (rdx_ip_is_valid(temp)) {
                        strcpy(out_ip, temp);
                        return true;
                    }
                }
            }
            p = curr;
        } else {
            p++;
        }
    }
    return false;
}

bool rdx_log_extract_url(const char *log, char out_url[256]) {
    if (!log) return false;
    const char *pos = strstr(log, "http://");
    if (!pos) pos = strstr(log, "https://");
    if (!pos) return false;

    const char *end = pos;
    while (*end && !isspace((unsigned char)*end) && *end != '"' && *end != '\'' && *end != '>') {
        end++;
    }
    size_t len = (size_t)(end - pos);
    if (len >= 256) len = 255;
    memcpy(out_url, pos, len);
    out_url[len] = '\0';
    return true;
}

bool rdx_log_extract_cve(const char *log, char out_cve[64]) {
    if (!log) return false;
    const char *pos = strstr(log, "CVE-");
    if (!pos) pos = strstr(log, "cve-");
    if (!pos) return false;

    const char *curr = pos + 4;
    int year_digits = 0;
    while (isdigit((unsigned char)*curr)) { year_digits++; curr++; }
    if (year_digits != 4 || *curr != '-') return false;
    curr++;
    int id_digits = 0;
    while (isdigit((unsigned char)*curr)) { id_digits++; curr++; }
    if (id_digits < 4 || id_digits > 7) return false;

    size_t total_len = (size_t)(curr - pos);
    if (total_len >= 64) total_len = 63;
    memcpy(out_cve, pos, total_len);
    out_cve[total_len] = '\0';
    return true;
}

int rdx_log_extract_status(const char *log) {
    if (!log) return 0;
    const char *http = strstr(log, "HTTP/1.");
    if (!http) http = strstr(log, "HTTP/2");
    if (http) {
        const char *p = http + 6;
        while (*p && !isdigit((unsigned char)*p)) p++;
        if (isdigit((unsigned char)*p)) {
            int code = atoi(p);
            if (code >= 100 && code <= 599) return code;
        }
    }
    return 0;
}

static bool str_contains_nocase(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    size_t nlen = strlen(needle);
    size_t hlen = strlen(haystack);
    if (nlen > hlen) return false;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

const char *rdx_log_detect_attack(const char *log) {
    if (!log) return NULL;

    /* SQL Injection Signatures */
    if (str_contains_nocase(log, "' or '1'='1") ||
        str_contains_nocase(log, "' union select") ||
        str_contains_nocase(log, "union all select") ||
        str_contains_nocase(log, "waitfor delay") ||
        str_contains_nocase(log, "xp_cmdshell")) {
        return "ATTACK: SQL Injection (SQLi) Detected";
    }

    /* Cross-Site Scripting (XSS) */
    if (str_contains_nocase(log, "<script") ||
        str_contains_nocase(log, "javascript:") ||
        str_contains_nocase(log, "onerror=") ||
        str_contains_nocase(log, "onload=") ||
        str_contains_nocase(log, "<img src=x")) {
        return "ATTACK: Cross-Site Scripting (XSS) Vector Detected";
    }

    /* Path Traversal / LFI */
    if (strstr(log, "../..") || strstr(log, "..\\..") ||
        str_contains_nocase(log, "/etc/passwd") ||
        str_contains_nocase(log, "c:\\windows\\win.ini") ||
        str_contains_nocase(log, "/proc/self/environ")) {
        return "ATTACK: Directory Traversal / LFI Attempt Detected";
    }

    /* Remote Command Execution (RCE) */
    if (str_contains_nocase(log, "/bin/sh") ||
        str_contains_nocase(log, "/bin/bash") ||
        str_contains_nocase(log, "cmd.exe /c") ||
        str_contains_nocase(log, "powershell -enc") ||
        str_contains_nocase(log, "| wget ") ||
        str_contains_nocase(log, "| curl ")) {
        return "ATTACK: Remote Code Execution (RCE) Payload Detected";
    }

    /* Auth Brute Force Keyword */
    if (str_contains_nocase(log, "failed password") ||
        str_contains_nocase(log, "authentication failure") ||
        str_contains_nocase(log, "invalid user")) {
        return "SUSPICIOUS: Authentication / Brute-Force Activity";
    }

    return NULL;
}

/* ============================================================================
 *  5. Incident Alerts & Timeline
 * ============================================================================ */

void rdx_cyber_alert(const char *level, const char *msg, const char *detail) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    const char *tag = "[ALERT]";
    if (str_contains_nocase(level, "crit")) tag = "[CRITICAL]";
    else if (str_contains_nocase(level, "warn")) tag = "[WARNING]";
    else if (str_contains_nocase(level, "threat")) tag = "[THREAT DETECTED]";
    else if (str_contains_nocase(level, "info")) tag = "[INFO]";

    printf("\n\033[1;31m================== %s ==================\033[0m\n", tag);
    printf("  TIMESTAMP: %s\n", time_str);
    printf("  SEVERITY:  %s\n", level ? level : "DEFAULT");
    printf("  SUMMARY:   %s\n", msg ? msg : "");
    if (detail && detail[0]) {
        printf("  DETAILS:   %s\n", detail);
    }
    printf("\033[1;31m====================================================\033[0m\n\n");
}

void rdx_cyber_timeline(const char *event_name, const char *detail) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", t);

    printf("  [%s] -> %-24s | %s\n", time_str, event_name ? event_name : "EVENT", detail ? detail : "");
}
