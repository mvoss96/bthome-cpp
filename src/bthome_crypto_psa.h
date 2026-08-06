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

// PSA's one-shot AEAD calls emit and expect ciphertext and tag contiguously,
// while BTHome carries the counter between them, so both adapters go through
// a fixed stack scratch buffer. Its size caps the plaintext per call at
// kPsaScratchBytes - kMicBytes = 60 bytes. That fits every legacy 31-byte
// advertisement several times over; only near-maximal extended-advertising
// or non-BLE service-data payloads exceed it and make the adapters return
// false. Use the mbedtls adapter (no such limit) for those, or enlarge this
// buffer at the cost of stack.
inline constexpr size_t kPsaScratchBytes = 64;

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
 * @return true on success, false on any PSA error or when the plaintext
 *         exceeds the 60-byte scratch limit (see kPsaScratchBytes).
 */
inline bool psa_ccm_backend(const uint8_t *key,
                            const uint8_t *nonce,
                            const uint8_t *plaintext,
                            size_t length,
                            uint8_t *ciphertext,
                            uint8_t *mic)
{
    // Encrypt into the scratch buffer and split afterwards (see
    // kPsaScratchBytes for the resulting plaintext limit).
    uint8_t buffer[kPsaScratchBytes];
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

/**
 * @brief CcmDecryptFn adapter backed by the PSA Crypto API.
 *
 * Usage: BTHome::Decryptor decryptor(&BTHome::psa_ccm_decrypt_backend);
 *
 * @return true only when the tag verified - psa_aead_decrypt() fails on a
 *         mismatch, which is the check the Decryptor relies on. Also false
 *         when the ciphertext exceeds the 60-byte scratch limit (see
 *         kPsaScratchBytes).
 */
inline bool psa_ccm_decrypt_backend(const uint8_t *key,
                                    const uint8_t *nonce,
                                    const uint8_t *ciphertext,
                                    size_t length,
                                    const uint8_t *mic,
                                    uint8_t *plaintext)
{
    // Rejoin ciphertext and tag in the scratch buffer first (see
    // kPsaScratchBytes for the resulting ciphertext limit).
    uint8_t buffer[kPsaScratchBytes];
    if (length + Encryptor::kMicBytes > sizeof(buffer))
    {
        return false;
    }
    if (psa_crypto_init() != PSA_SUCCESS)
    {
        return false;
    }
    memcpy(buffer, ciphertext, length);
    memcpy(buffer + length, mic, Encryptor::kMicBytes);

    const psa_algorithm_t alg =
        PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM, Encryptor::kMicBytes);

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
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
        psa_aead_decrypt(key_id, alg, nonce, Encryptor::kNonceBytes, nullptr, 0,
                         buffer, length + Encryptor::kMicBytes,
                         plaintext, length, &out_len) == PSA_SUCCESS &&
        out_len == length;
    psa_destroy_key(key_id);
    return ok;
}

} // namespace BTHome
