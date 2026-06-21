#pragma once
#include <stdint.h>
#include <stddef.h>
#include "mbedtls/gcm.h"

class AesGcm {
public:
    static constexpr size_t KEY_SIZE   = 16;
    static constexpr size_t NONCE_SIZE = 12;
    static constexpr size_t TAG_SIZE   = 16;
    static constexpr size_t OVERHEAD   = NONCE_SIZE + TAG_SIZE;  // 28 bytes

    explicit AesGcm(const uint8_t key[KEY_SIZE]);
    ~AesGcm();

    // Decrypts an encrypted buffer produced by the sender.
    // Input layout: [ 12-byte nonce | ciphertext | 16-byte tag ]
    // On success, writes plaintext to out and sets outLen.
    // out must be at least (encLen - OVERHEAD) bytes large.
    bool decrypt(const uint8_t* enc, size_t encLen,
                 uint8_t* out, size_t& outLen);

private:
    mbedtls_gcm_context _ctx;
};
