// Arduino (ESP32) + NimBLE-Arduino example: receive what
// arduino_nimble_encrypted.ino advertises. Flash that sketch on one board and
// this one on another, with the same key.
//
// Library Manager dependency: "NimBLE-Arduino" by h2zero. mbedtls ships with
// the ESP32 Arduino core. Shows the two duties decryption adds on the receiver
// side, mirroring the sender's:
//   1. Use the SAME MAC in the nonce that the sender advertises with - it
//      comes from the scan result, not from the payload.
//   2. Persist the highest counter seen; after a reboot there is nothing to
//      compare against and the replay window is open until the next packet.
#include <NimBLEDevice.h>
#include <Preferences.h>

#include "bthome.h"
#include "bthome_crypto_mbedtls.h"

// The same 16-byte key the sender uses.
static const uint8_t kKey[BTHome::Encryptor::kKeyBytes] = {
    0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1,
    0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32};

// Key and replay state are per sender: listening to several devices needs one
// Decryptor each, keyed by address.
static BTHome::Decryptor decryptor(&BTHome::mbedtls_ccm_decrypt_backend);
static Preferences prefs;

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    const std::string sd = dev->getServiceData(NimBLEUUID(BTHome::kServiceUuid));
    if (sd.empty()) {
      return;  // not a BTHome advertisement
    }

    // 1. NimBLE stores the address little-endian, BTHome builds the nonce from
    //    it in display order. Skipping this reversal shows up only as
    //    DecryptStatus::AuthFailed, with nothing else to go on.
    uint8_t mac[BTHome::Encryptor::kMacBytes];
    const uint8_t* val = dev->getAddress().getVal();
    for (uint8_t i = 0; i < sizeof(mac); i++) {
      mac[i] = val[sizeof(mac) - 1 - i];
    }
    decryptor.setMac(mac);

    // getServiceData() strips the UUID, so this is the payload shape.
    uint8_t plain[32];
    size_t plain_len = 0;
    if (decryptor.decryptPayload((const uint8_t*)sd.data(), sd.size(), plain, sizeof(plain),
                                 plain_len) != BTHome::DecryptStatus::Ok) {
      // Replay is ordinary radio noise - senders repeat a packet for
      // reliability - so dropping quietly is right. AuthFailed would mean a
      // wrong key, a wrong MAC or a tampered packet.
      return;
    }

    // 2. A real receiver would persist on a timer rather than per packet.
    prefs.putUInt("counter", decryptor.lastCounter());

    Serial.printf("%s rssi %d\n", dev->getAddress().toString().c_str(), dev->getRSSI());

    BTHome::Decoder dec = BTHome::Decoder::fromPayload(plain, plain_len);
    BTHome::Decoded obj;
    while (dec.next(obj)) {
      if (obj.is(BTHome::ObjectId::Temperature)) {
        Serial.printf("  temperature %.2f C\n", obj.value);
      } else if (obj.is(BTHome::ObjectId::Humidity)) {
        Serial.printf("  humidity    %.1f %%\n", obj.value);
      } else if (obj.is(BTHome::ObjectId::Battery)) {
        Serial.printf("  battery     %.0f %%\n", obj.value);
      }
    }
  }
};

static ScanCallbacks scanCallbacks;

void setup() {
  Serial.begin(115200);
  NimBLEDevice::init("");

  decryptor.setKey(kKey);
  prefs.begin("bthome-rx");
  const uint32_t seen = prefs.getUInt("counter", 0);
  if (seen != 0) {
    decryptor.setLastCounter(seen);
  }

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(false);                    // BTHome rides in the advertisement
  scan->setScanCallbacks(&scanCallbacks, true);  // true: keep repeats
  scan->start(0);                                // 0 = keep scanning
  Serial.println("scanning for encrypted BTHome");
}

void loop() {
  delay(1000);
}
