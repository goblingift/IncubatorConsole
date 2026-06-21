#include "AesGcm.h"
#include <string.h>

AesGcm::AesGcm(const uint8_t key[KEY_SIZE]) {
    mbedtls_gcm_init(&_ctx);
    mbedtls_gcm_setkey(&_ctx, MBEDTLS_CIPHER_ID_AES, key, KEY_SIZE * 8);
}

AesGcm::~AesGcm() {
    mbedtls_gcm_free(&_ctx);
}

bool AesGcm::decrypt(const uint8_t* enc, size_t encLen,
                     uint8_t* out, size_t& outLen) {
    if (encLen < OVERHEAD) return false;

    const uint8_t* nonce      = enc;
    const uint8_t* ciphertext = enc + NONCE_SIZE;
    size_t         ctLen      = encLen - OVERHEAD;
    const uint8_t* tag        = enc + NONCE_SIZE + ctLen;

    int ret = mbedtls_gcm_auth_decrypt(
        &_ctx,
        ctLen,
        nonce,      NONCE_SIZE,
        nullptr,    0,
        tag,        TAG_SIZE,
        ciphertext,
        out
    );

    if (ret != 0) return false;

    outLen = ctLen;
    return true;
}
