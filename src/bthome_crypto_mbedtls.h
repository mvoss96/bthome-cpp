#pragma once

// Optional mbedtls backend for BTHome::Encryptor. Only include this header on
// platforms that provide mbedtls (ESP-IDF does by default; on desktop link
// against mbedcrypto). The core library stays dependency-free.

#include <mbedtls/ccm.h>

#include "bthome_encryption.h"

namespace BTHome
{

/**
 * @brief CcmEncryptFn adapter backed by mbedtls AES-128-CCM.
 *
 * Usage: BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
 *
 * @return true on success, false on any mbedtls error.
 */
inline bool mbedtls_ccm_backend(const uint8_t *key,
                                const uint8_t *nonce,
                                const uint8_t *plaintext,
                                size_t length,
                                uint8_t *ciphertext,
                                uint8_t *mic)
{
    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    const bool ok =
        mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES, key, 8 * Encryptor::kKeyBytes) == 0 &&
        mbedtls_ccm_encrypt_and_tag(&ccm, length, nonce, Encryptor::kNonceBytes, nullptr, 0,
                                    plaintext, ciphertext, mic, Encryptor::kMicBytes) == 0;
    mbedtls_ccm_free(&ccm);
    return ok;
}

/**
 * @brief CcmDecryptFn adapter backed by mbedtls AES-128-CCM.
 *
 * Usage: BTHome::Decryptor decryptor(&BTHome::mbedtls_ccm_decrypt_backend);
 *
 * @return true only when the MIC verified; mbedtls_ccm_auth_decrypt() reports
 *         a mismatch as an error, which is exactly the check the Decryptor
 *         relies on to reject wrong-key and tampered packets.
 */
inline bool mbedtls_ccm_decrypt_backend(const uint8_t *key,
                                        const uint8_t *nonce,
                                        const uint8_t *ciphertext,
                                        size_t length,
                                        const uint8_t *mic,
                                        uint8_t *plaintext)
{
    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    const bool ok =
        mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES, key, 8 * Encryptor::kKeyBytes) == 0 &&
        mbedtls_ccm_auth_decrypt(&ccm, length, nonce, Encryptor::kNonceBytes, nullptr, 0,
                                 ciphertext, plaintext, mic, Encryptor::kMicBytes) == 0;
    mbedtls_ccm_free(&ccm);
    return ok;
}

} // namespace BTHome
