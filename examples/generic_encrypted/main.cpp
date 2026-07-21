// Generic encryption example: build an AES-CCM encrypted payload and print
// the bytes. Uses the official spec vector from https://bthome.io/encryption/,
// so the output is verifiable by eye. Needs mbedtls. From project root:
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I src examples/generic_encrypted/main.cpp -lmbedcrypto -o build/demo_encrypted
//   ./build/demo_encrypted
#include "bthome.h"
#include "bthome_crypto_mbedtls.h"

#include <cstdint>
#include <cstdio>

int main()
{
    // EncryptedPacket<28> reserves the 8-byte counter+MIC overhead internally,
    // so size() can never exceed 28 bytes (a full advertisement minus Flags AD).
    BTHome::EncryptedPacket<28> packet;
    bool ok = true;
    ok = packet.add(BTHome::temperature(25.06f)) && ok;
    ok = packet.add(BTHome::humidity(50.55f)) && ok;

    // Key, MAC and counter from the official BTHome encryption example.
    const std::uint8_t key[BTHome::Encryptor::kKeyBytes] = {
        0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1,
        0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32};
    const std::uint8_t mac[BTHome::Encryptor::kMacBytes] = {
        0x54, 0x48, 0xE6, 0x8F, 0x80, 0xA5};

    BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
    encryptor.setKey(key);
    encryptor.setMac(mac);
    encryptor.setCounter(0x00112233u);

    std::uint8_t adv[31] = {};
    const int n = BTHome::build_encrypted_advertising(packet, encryptor, adv, sizeof(adv));
    if (n < 0 || !ok)
    {
        std::printf("ERROR: build failed or payload item did not fit\n");
        return 1;
    }

    // Expected service data (after the 3 Flags bytes and AD header):
    //   D2 FC 41 E4 45 F3 C9 96 2B 33 22 11 00 6C 7C 45 19
    std::printf("Encrypted advertisement (%d bytes): ", n);
    for (int i = 0; i < n; ++i)
    {
        std::printf("%02X ", adv[i]);
    }
    std::printf("\nNext counter to persist: %u\n", static_cast<unsigned>(encryptor.counter()));
    return 0;
}
