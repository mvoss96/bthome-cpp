// Arduino (ESP32) + NimBLE-Arduino example.
//
// Library Manager dependency: "NimBLE-Arduino" by h2zero.
// Place bthome.h on the include path (PlatformIO: add this repo as a lib_dep;
// Arduino IDE: install bthome-cpp as a library, or drop bthome.h beside this
// sketch).
//
// The library only builds the BTHome bytes; NimBLE does the advertising.
#include <NimBLEDevice.h>
#include "bthome.h"

void setup() {
  Serial.begin(115200);
  NimBLEDevice::init("bthome");

  BTHome::Packet<31> packet;
  packet.add(BTHome::temperature(22.4f));
  packet.add(BTHome::humidity(54.3f));
  packet.add(BTHome::battery(92));

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData adData;

  // serviceData() returns [UUID lo][UUID hi][device-info][measurements...].
  // NimBLE wants the 16-bit UUID plus the value bytes that follow it.
  adData.setServiceData(
      NimBLEUUID(BTHome::kServiceUuid),
      std::string(reinterpret_cast<const char*>(packet.serviceData() + 2),
                  packet.serviceDataSize() - 2));

  adv->setAdvertisementData(adData);
  adv->start();
}

void loop() {
  delay(1000);
}
