// Generic decryption example: the counterpart of examples/generic_encrypted.
// Takes the service data that one prints, verifies and decrypts it, then
// decodes the result. Requires mbedtls. From project root:
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I .\src .\examples\generic_decrypt\main.cpp -lmbedcrypto -o .\build\demo_decrypt.exe
//   .\build\demo_decrypt.exe
#include "bthome.h"
#include "bthome_crypto_mbedtls.h"

#include <stdint.h>
#include <stdio.h>

// Bindkey and MAC of the sender, from https://bthome.io/encryption/
static const uint8_t kKey[BTHome::Encryptor::kKeyBytes] = {
    0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1,
    0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32};
static const uint8_t kMac[BTHome::Encryptor::kMacBytes] = {
    0x54, 0x48, 0xE6, 0x8F, 0x80, 0xA5};

// The service data examples/generic_encrypted emits:
// [uuid][info][ciphertext][counter][MIC] - temperature 25.06, humidity 50.55.
static const uint8_t kServiceData[] = {0xD2, 0xFC, 0x41, 0xE4, 0x45, 0xF3,
                                       0xC9, 0x96, 0x2B, 0x33, 0x22, 0x11,
                                       0x00, 0x6C, 0x7C, 0x45, 0x19};

int main() {
    BTHome::Decryptor decryptor(&BTHome::mbedtls_ccm_decrypt_backend);
    decryptor.setKey(kKey);
    decryptor.setMac(kMac);
    // Restore this from flash after a reboot; without it the first packet is
    // accepted whatever its counter, which reopens the replay window.
    // decryptor.setLastCounter(persisted);

    uint8_t plain[32] = {};
    size_t plain_len = 0;
    const BTHome::DecryptStatus status = decryptor.decryptServiceData(
        kServiceData, sizeof(kServiceData), plain, sizeof(plain), plain_len);

    if (status != BTHome::DecryptStatus::Ok) {
        // Replay is normal radio noise and can be dropped quietly; AuthFailed
        // means a wrong bindkey or a tampered packet and is worth logging.
        printf("rejected (status %u)\n", static_cast<unsigned>(status));
        return 1;
    }

    // The encrypted bit is cleared in the output, so this feeds the Decoder.
    BTHome::Decoder dec(plain, plain_len);
    BTHome::Decoded obj;
    while (dec.next(obj)) {
        if (obj.is(BTHome::ObjectId::Temperature)) {
            printf("temperature  %.2f C\n", static_cast<double>(obj.value));
        } else if (obj.is(BTHome::ObjectId::Humidity)) {
            printf("humidity     %.2f %%\n", static_cast<double>(obj.value));
        }
    }

    printf("counter seen: %u\n", static_cast<unsigned>(decryptor.lastCounter()));
    return dec.status() == BTHome::DecodeStatus::End ? 0 : 1;
}
