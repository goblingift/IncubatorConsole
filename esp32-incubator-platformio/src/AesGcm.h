#pragma once
#include <stdint.h>
#include <stddef.h>
#include "mbedtls/gcm.h"

// AES-128-GCM authenticated encryption/decryption.
//
// Overhead per call (RFC 5116 §5.1):
//   +12 bytes  nonce  (N_MIN = N_MAX = 12 octets)
//   +16 bytes  auth tag ("ciphertext is exactly 16 octets longer than plaintext")
//   ─────────────────
//   +28 bytes  total overhead
//
// Output layout:  [ 12-byte nonce | ciphertext | 16-byte tag ]
//
// Nonce layout: [ 4-byte counter | 1-byte domain | 4-byte boot random | 3x 0x00 ].
// The domain byte separates the encrypting parties (incubator 0x00, gateway
// 0x01) so both can transmit under one pre-shared key without ever reusing a
// nonce; the boot random keeps nonces unique across reboots even though the
// counter restarts at zero. Decryption reads the nonce from the packet, so it
// needs no domain knowledge.
class AesGcm {
public:
    static constexpr size_t KEY_SIZE   = 16;  // AES-128
    static constexpr size_t NONCE_SIZE = 12;  // 96-bit, NIST SP 800-38D §8.2
    static constexpr size_t TAG_SIZE   = 16;  // 128-bit, RFC 5116 §5.1
    static constexpr size_t OVERHEAD   = NONCE_SIZE + TAG_SIZE;  // 28 bytes

    explicit AesGcm(const uint8_t key[KEY_SIZE], uint8_t nonceDomain = 0x00);
    ~AesGcm();

    // Encrypts plain[plainLen] bytes and writes result to out.
    // out must be at least plainLen + OVERHEAD bytes large.
    // Sets outLen to the number of bytes written on success.
    // Returns true on success, false on mbedTLS error.
    bool encrypt(const uint8_t* plain, size_t plainLen,
                 uint8_t* out, size_t& outLen);

    // Decrypts a buffer produced by encrypt() (any domain, same key).
    // On success, writes plaintext to out and sets outLen.
    // out must be at least (encLen - OVERHEAD) bytes large.
    bool decrypt(const uint8_t* enc, size_t encLen,
                 uint8_t* out, size_t& outLen);

private:
    mbedtls_gcm_context _ctx;
    uint32_t            _counter;
    uint8_t             _nonceDomain;
    uint32_t            _bootRandom;
};
