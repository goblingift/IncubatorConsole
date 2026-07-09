#include "AesGcm.h"
#include <string.h>
#include "esp_system.h"

AesGcm::AesGcm(const uint8_t key[KEY_SIZE], uint8_t nonceDomain)
    : _counter(0), _nonceDomain(nonceDomain), _bootRandom(esp_random()) {
    mbedtls_gcm_init(&_ctx);
    mbedtls_gcm_setkey(&_ctx, MBEDTLS_CIPHER_ID_AES, key, KEY_SIZE * 8);
}

AesGcm::~AesGcm() {
    mbedtls_gcm_free(&_ctx);
}

bool AesGcm::encrypt(const uint8_t* plain, size_t plainLen,
                     uint8_t* out, size_t& outLen) {
    // Nonce: [ counter | domain | boot random | 0x00 x3 ] — see header.
    uint8_t nonce[NONCE_SIZE] = {0};
    memcpy(nonce, &_counter, sizeof(_counter));
    _counter++;
    nonce[4] = _nonceDomain;
    memcpy(nonce + 5, &_bootRandom, sizeof(_bootRandom));

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
