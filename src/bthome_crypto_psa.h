#pragma once

// Optional PSA Crypto backend for BTHome::Encryptor. PSA is the Arm-standard
// crypto API: available in vanilla Zephyr (backed by mbedtls software), in the
// nRF Connect SDK (backed by Oberon or CryptoCell hardware - the legacy
// mbedtls_ccm_* API is deprecated there), and with TF-M secure processing.
// The same application code runs on all of them; the platform picks the best
// implementation underneath.

#include <psa/crypto.h>

#include "bthome_encryption.h"

namespace BTHome
{

/**
 * @brief CcmEncryptFn adapter backed by the PSA Crypto API.
 *
 * Usage: BTHome::Encryptor encryptor(&BTHome::psa_ccm_backend);
 *
 * The key is imported as a volatile PSA key per call and destroyed afterwards,
 * which keeps the adapter stateless. Production firmware handling many builds
 * per second could hold a persistent key id instead; for BLE advertising
 * cadence (seconds) the import cost is irrelevant.
 *
 * @return true on success, false on any PSA error.
 */
inline bool psa_ccm_backend(const uint8_t *key,
                            const uint8_t *nonce,
                            const uint8_t *plaintext,
                            size_t length,
                            uint8_t *ciphertext,
                            uint8_t *mic)
{
    // PSA emits ciphertext and tag contiguously; BTHome places the counter
    // between them, so encrypt into a stack buffer and split afterwards.
    uint8_t buffer[64];
    if (length + Encryptor::kMicBytes > sizeof(buffer))
    {
        return false;
    }
    if (psa_crypto_init() != PSA_SUCCESS)
    {
        return false;
    }

    const psa_algorithm_t alg =
        PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM, Encryptor::kMicBytes);

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 8 * Encryptor::kKeyBytes);

    psa_key_id_t key_id;
    if (psa_import_key(&attributes, key, Encryptor::kKeyBytes, &key_id) != PSA_SUCCESS)
    {
        return false;
    }

    size_t out_len = 0;
    const bool ok =
        psa_aead_encrypt(key_id, alg, nonce, Encryptor::kNonceBytes, nullptr, 0,
                         plaintext, length, buffer, sizeof(buffer), &out_len) == PSA_SUCCESS &&
        out_len == length + Encryptor::kMicBytes;
    psa_destroy_key(key_id);

    if (!ok)
    {
        return false;
    }
    memcpy(ciphertext, buffer, length);
    memcpy(mic, buffer + length, Encryptor::kMicBytes);
    return true;
}

} // namespace BTHome
