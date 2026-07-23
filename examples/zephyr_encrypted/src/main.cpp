// Zephyr example: advertise an AES-CCM encrypted BTHome payload.
//
// Uses the PSA Crypto backend (see prj.conf): on vanilla Zephyr PSA is backed
// by mbedtls in software, but the SAME code runs unchanged on the nRF Connect
// SDK, where PSA routes to the Oberon library or CryptoCell hardware instead
// (the legacy mbedtls_ccm_* API is deprecated there).
// Note the MAC byte order: bt_id_get() returns the address with the LEAST
// significant byte first (val[0] = last octet of the printed address), while
// the BTHome nonce wants display order - so the bytes must be reversed.
#include "bthome.h"
#include "bthome_crypto_psa.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>

// 16-byte key, shared with Home Assistant (enter it there as 32 hex chars).
static const uint8_t kKey[BTHome::Encryptor::kKeyBytes] = {
    0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1,
    0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32};

static BTHome::Encryptor encryptor(&BTHome::psa_ccm_backend);

// Built once at startup; the bytes stay valid because the buffer is static.
static uint8_t adv[31];

int main(void) {
    if (bt_enable(nullptr)) {
        return -1;
    }

    // Nonce MAC = the identity address this device advertises with,
    // reversed from Zephyr's little-endian storage into display order.
    bt_addr_le_t addr;
    size_t count = 1;
    bt_id_get(&addr, &count);
    uint8_t mac[BTHome::Encryptor::kMacBytes];
    for (size_t i = 0; i < BTHome::Encryptor::kMacBytes; i++) {
        mac[i] = addr.a.val[BTHome::Encryptor::kMacBytes - 1 - i];
    }

    encryptor.setKey(kKey);
    encryptor.setMac(mac);
    // The counter MUST increase across reboots. Persist encryptor.counter()
    // (plus a safety margin) with the settings/NVS subsystem in real firmware;
    // see examples/esp_idf_encrypted for the pattern.
    encryptor.setCounter(1);

    BTHome::EncryptedPacket<28> packet;
    packet.add(BTHome::temperature(22.4f));
    packet.add(BTHome::humidity(54.3f));
    packet.add(BTHome::battery(92));

    const int n = BTHome::build_encrypted_advertising(packet, encryptor, adv, sizeof(adv));
    if (n < 0) {
        return -1;
    }

    // BT_DATA_SVC_DATA16 wants [UUID lo][UUID hi][value...]. The built payload
    // is [Flags(3)][len][0x16][UUID(2)][value...], so service data starts at
    // offset 5 and runs to the end (no name AD was requested).
    const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
        BT_DATA(BT_DATA_SVC_DATA16, adv + 5, static_cast<uint8_t>(n - 5)),
    };

    bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), nullptr, 0);
    return 0;
}
