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
inline bool mbedtls_ccm_backend(const std::uint8_t *key,
                                const std::uint8_t *nonce,
                                const std::uint8_t *plaintext,
                                std::size_t length,
                                std::uint8_t *ciphertext,
                                std::uint8_t *mic)
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

} // namespace BTHome
