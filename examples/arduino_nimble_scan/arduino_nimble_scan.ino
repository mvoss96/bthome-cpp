// Arduino (ESP32) + NimBLE-Arduino example: receive what arduino_nimble.ino
// advertises. Flash that sketch on one board and this one on another.
//
// Library Manager dependency: "NimBLE-Arduino" by h2zero.
// Place bthome.h on the include path (PlatformIO: add this repo as a lib_dep;
// Arduino IDE: install bthome-cpp as a library, or drop bthome.h beside this
// sketch).
//
// NimBLE does the scanning; the library turns the service data back into values.
#include <NimBLEDevice.h>
#include "bthome.h"

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    // Asked for one UUID, so NimBLE returns only what follows it - the same
    // cut the sender makes with serviceData() + 2. Hence fromPayload(); the
    // Decoder constructor expects the two UUID bytes to still be there.
    const std::string sd = dev->getServiceData(NimBLEUUID(BTHome::kServiceUuid));
    if (sd.empty()) {
      return;  // not a BTHome advertisement
    }

    BTHome::Decoder dec =
        BTHome::Decoder::fromPayload((const uint8_t*)sd.data(), sd.size());

    Serial.printf("%s rssi %d\n", dev->getAddress().toString().c_str(), dev->getRSSI());

    BTHome::Decoded obj;
    while (dec.next(obj)) {
      Serial.printf("  0x%02X = %.2f\n", obj.object_id, obj.value);
    }

    // End is the only clean outcome; status() says which of the others it was.
    if (dec.status() != BTHome::DecodeStatus::End) {
      Serial.println("  incomplete: encrypted, truncated or an unknown object id");
    }
  }
};

static ScanCallbacks scanCallbacks;

void setup() {
  Serial.begin(115200);
  NimBLEDevice::init("");

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(false);                    // BTHome rides in the advertisement
  scan->setScanCallbacks(&scanCallbacks, true);  // true: keep repeats
  scan->start(0);                                // 0 = keep scanning
  Serial.println("scanning for BTHome");
}

void loop() {
  delay(1000);
}
