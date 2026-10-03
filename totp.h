#pragma once

#include <stdint.h>

// Generate a 6-digit TOTP string from a base32 encoded secret and a unix timestamp.
// `output_str` must be at least 7 bytes long (6 digits + null terminator).
void generate_totp(const char *base32_secret, uint32_t unix_time, char *output_str);

// Decrypt the secret using RC4 and the provided PIN, then generate the TOTP.
// The decrypted secret is wiped from RAM immediately.
void generate_encrypted_totp(const uint8_t *encrypted_secret, uint8_t secret_len, 
                             const char *pin, uint8_t pin_len, 
                             uint32_t unix_time, char *output_str);
