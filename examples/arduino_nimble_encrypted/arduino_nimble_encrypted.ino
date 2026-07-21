// Arduino (ESP32) + NimBLE-Arduino example with AES-CCM encryption.
//
// Library Manager dependency: "NimBLE-Arduino" by h2zero. mbedtls ships with
// the ESP32 Arduino core, so the bthome-cpp mbedtls backend works out of the
// box. Shows the three duties encryption adds on the device side:
//   1. Use the SAME MAC in the nonce that the BLE stack advertises with.
//   2. Persist the counter across reboots (Preferences/NVS) - receivers
//      reject non-increasing counters as replays.
//   3. Rebuild (re-encrypt) for every content update; each build consumes
//      one counter value.
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_mac.h>

#include "bthome.h"
#include "bthome_crypto_mbedtls.h"

// 16-byte key, shared with Home Assistant (enter it there as 32 hex chars).
static const uint8_t kKey[BTHome::Encryptor::kKeyBytes] = {
    0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1,
    0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32};

// On boot the counter resumes at last-persisted + margin, so values consumed
// between NVS writes can never be reused after a crash or power loss.
static constexpr uint32_t kCounterMargin = 1024;

static BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
static Preferences prefs;

void setup() {
  Serial.begin(115200);
  NimBLEDevice::init("bthome");

  // 1. Nonce MAC = the Bluetooth MAC this device advertises with.
  uint8_t mac[BTHome::Encryptor::kMacBytes] = {};
  esp_read_mac(mac, ESP_MAC_BT);
  encryptor.setKey(kKey);
  encryptor.setMac(mac);

  // 2. Restore the counter with a safety margin and persist the new base.
  prefs.begin("bthome");
  const uint32_t counter = prefs.getUInt("counter", 0) + kCounterMargin;
  encryptor.setCounter(counter);
  prefs.putUInt("counter", counter);

  // 3. Build and encrypt the payload (consumes one counter value).
  BTHome::EncryptedPacket<28> packet;
  packet.add(BTHome::temperature(22.4f));
  packet.add(BTHome::humidity(54.3f));
  packet.add(BTHome::battery(92));

  static uint8_t adv[31];
  const int n = BTHome::build_encrypted_advertising(packet, encryptor, adv, sizeof(adv));
  if (n < 0) {
    Serial.println("build_encrypted_advertising failed");
    return;
  }

  // NimBLE wants the 16-bit UUID plus the service-data value bytes.
  // The built payload is [Flags(3)][len][0x16][UUID(2)][value...], so the
  // value starts at offset 7: [device-info][ciphertext][counter][MIC].
  NimBLEAdvertising* bleAdv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData adData;
  adData.setServiceData(
      NimBLEUUID(BTHome::kServiceUuid),
      std::string(reinterpret_cast<const char*>(adv + 7), n - 7));

  bleAdv->setAdvertisementData(adData);
  bleAdv->start();
}

void loop() {
  delay(1000);
}
