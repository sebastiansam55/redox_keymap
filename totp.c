#include "totp.h"
#include <string.h>

#define SHA1_BLOCK_SIZE 64
#define SHA1_DIGEST_SIZE 20

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} sha1_ctx;

static const uint32_t K[4] = {
    0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xca62c1d6
};

static uint32_t rotl32(uint32_t value, unsigned int count) {
    return (value << count) | (value >> (32 - count));
}

static void sha1_transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a, b, c, d, e, temp, w[80];
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = (buffer[i*4] << 24) | (buffer[i*4+1] << 16) |
               (buffer[i*4+2] << 8) | (buffer[i*4+3]);
    }
    for (i = 16; i < 80; i++) {
        w[i] = rotl32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    }

    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];

    for (i = 0; i < 80; i++) {
        if (i < 20) {
            temp = rotl32(a, 5) + ((b & c) | ((~b) & d)) + e + w[i] + K[0];
        } else if (i < 40) {
            temp = rotl32(a, 5) + (b ^ c ^ d) + e + w[i] + K[1];
        } else if (i < 60) {
            temp = rotl32(a, 5) + ((b & c) | (b & d) | (c & d)) + e + w[i] + K[2];
        } else {
            temp = rotl32(a, 5) + (b ^ c ^ d) + e + w[i] + K[3];
        }
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1_init(sha1_ctx *context) {
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
    context->state[4] = 0xc3d2e1f0;
    context->count[0] = context->count[1] = 0;
}

static void sha1_update(sha1_ctx *context, const uint8_t *data, uint32_t len) {
    uint32_t i, j;
    j = (context->count[0] >> 3) & 63;
    if ((context->count[0] += len << 3) < (len << 3)) context->count[1]++;
    context->count[1] += (len >> 29);
    for (i = 0; len + j >= 64; i += 64 - j, len -= 64 - j, j = 0) {
        memcpy(&context->buffer[j], &data[i], 64 - j);
        sha1_transform(context->state, context->buffer);
    }
    memcpy(&context->buffer[j], &data[i], len);
}

static void sha1_final(uint8_t digest[20], sha1_ctx *context) {
    uint32_t i;
    uint8_t finalcount[8];
    uint8_t c;

    for (i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t)((context->count[(i >= 4 ? 0 : 1)]
         >> ((3-(i & 3)) * 8) ) & 255);
    }
    c = 0200;
    sha1_update(context, &c, 1);
    while ((context->count[0] & 504) != 448) {
        c = 0000;
        sha1_update(context, &c, 1);
    }
    sha1_update(context, finalcount, 8);
    for (i = 0; i < 20; i++) {
        digest[i] = (uint8_t)
         ((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
    }
    memset(context, 0, sizeof(*context));
}

static void hmac_sha1(const uint8_t *key, int key_len,
                      const uint8_t *text, int text_len, uint8_t *digest) {
    sha1_ctx context;
    uint8_t k_ipad[65];
    uint8_t k_opad[65];
    uint8_t tk[20];
    int i;

    if (key_len > 64) {
        sha1_init(&context);
        sha1_update(&context, key, key_len);
        sha1_final(tk, &context);
        key = tk;
        key_len = 20;
    }

    memset(k_ipad, 0, sizeof(k_ipad));
    memset(k_opad, 0, sizeof(k_opad));
    memcpy(k_ipad, key, key_len);
    memcpy(k_opad, key, key_len);

    for (i = 0; i < 64; i++) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5c;
    }

    sha1_init(&context);
    sha1_update(&context, k_ipad, 64);
    sha1_update(&context, text, text_len);
    sha1_final(digest, &context);

    sha1_init(&context);
    sha1_update(&context, k_opad, 64);
    sha1_update(&context, digest, 20);
    sha1_final(digest, &context);
}

static int base32_decode(const char *encoded, uint8_t *result) {
    int buffer = 0;
    int bitsLeft = 0;
    int count = 0;
    for (const char *ptr = encoded; *ptr; ++ptr) {
        uint8_t ch = *ptr;
        if (ch == ' ' || ch == '-') continue;
        if (ch >= 'A' && ch <= 'Z') ch -= 'A';
        else if (ch >= 'a' && ch <= 'z') ch -= 'a';
        else if (ch >= '2' && ch <= '7') ch -= '2' - 26;
        else continue;

        buffer <<= 5;
        buffer |= ch;
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            result[count++] = (uint8_t)(buffer >> (bitsLeft - 8));
            bitsLeft -= 8;
        }
    }
    return count;
}

void generate_totp(const char *base32_secret, uint32_t unix_time, char *output_str) {
    uint8_t key[64];
    int key_len = base32_decode(base32_secret, key);
    
    uint64_t time_step = unix_time / 30;
    uint8_t time_bytes[8];
    for (int i = 7; i >= 0; i--) {
        time_bytes[i] = (uint8_t)(time_step & 0xFF);
        time_step >>= 8;
    }
    
    uint8_t hash[20];
    hmac_sha1(key, key_len, time_bytes, 8, hash);
    
    int offset = hash[19] & 0x0f;
    uint32_t binary =
        ((hash[offset] & 0x7f) << 24) |
        ((hash[offset + 1] & 0xff) << 16) |
        ((hash[offset + 2] & 0xff) << 8) |
        (hash[offset + 3] & 0xff);
        
    uint32_t otp = binary % 1000000;
    
    output_str[0] = '0' + (otp / 100000) % 10;
    output_str[1] = '0' + (otp / 10000) % 10;
    output_str[2] = '0' + (otp / 1000) % 10;
    output_str[3] = '0' + (otp / 100) % 10;
    output_str[4] = '0' + (otp / 10) % 10;
    output_str[5] = '0' + (otp % 10);
    output_str[6] = '\0';
}

static void rc4(const uint8_t *key, uint8_t key_len, const uint8_t *data, uint8_t data_len, uint8_t *out) {
    uint8_t S[256];
    int i, j = 0;
    for (i = 0; i < 256; i++) S[i] = i;
    for (i = 0; i < 256; i++) {
        j = (j + S[i] + key[i % key_len]) % 256;
        uint8_t temp = S[i];
        S[i] = S[j];
        S[j] = temp;
    }
    i = j = 0;
    for (int k = 0; k < data_len; k++) {
        i = (i + 1) % 256;
        j = (j + S[i]) % 256;
        uint8_t temp = S[i];
        S[i] = S[j];
        S[j] = temp;
        out[k] = data[k] ^ S[(S[i] + S[j]) % 256];
    }
}

void generate_encrypted_totp(const uint8_t *encrypted_secret, uint8_t secret_len, 
                             const char *pin, uint8_t pin_len, 
                             uint32_t unix_time, char *output_str) {
    char decrypted_secret[64];
    if (secret_len >= sizeof(decrypted_secret)) return; // Too long

    rc4((const uint8_t*)pin, pin_len, encrypted_secret, secret_len, (uint8_t*)decrypted_secret);
    decrypted_secret[secret_len] = '\0';

    generate_totp(decrypted_secret, unix_time, output_str);

    // Wipe the decrypted secret from RAM
    memset(decrypted_secret, 0, sizeof(decrypted_secret));
}
