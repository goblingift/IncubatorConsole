#include "AesGcm.h"
#include <string.h>

AesGcm::AesGcm(const uint8_t key[KEY_SIZE]) : _counter(0) {
    mbedtls_gcm_init(&_ctx);
    mbedtls_gcm_setkey(&_ctx, MBEDTLS_CIPHER_ID_AES, key, KEY_SIZE * 8);
}

AesGcm::~AesGcm() {
    mbedtls_gcm_free(&_ctx);
}

bool AesGcm::encrypt(const uint8_t* plain, size_t plainLen,
                     uint8_t* out, size_t& outLen) {
    // Build 12-byte nonce from 4-byte counter, remaining bytes stay zero.
    uint8_t nonce[NONCE_SIZE] = {0};
    memcpy(nonce, &_counter, sizeof(_counter));
    _counter++;

    uint8_t* ciphertext = out + NONCE_SIZE;
    uint8_t* tag        = out + NONCE_SIZE + plainLen;

    int ret = mbedtls_gcm_crypt_and_tag(
        &_ctx,
        MBEDTLS_GCM_ENCRYPT,
        plainLen,
        nonce,   NONCE_SIZE,
        nullptr, 0,           // no additional authenticated data
        plain,
        ciphertext,
        TAG_SIZE,
        tag
    );

    if (ret != 0) return false;

    memcpy(out, nonce, NONCE_SIZE);
    outLen = NONCE_SIZE + plainLen + TAG_SIZE;
    return true;
}
