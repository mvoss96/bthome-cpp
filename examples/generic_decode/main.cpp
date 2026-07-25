// Generic decoding example: no BLE stack, no encoder - just bytes that
// arrived. These two are what examples/arduino_avr prints over serial; the
// same bytes reach a receiver over ESP-NOW, nRF24 or a BLE scan.
// From project root:
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I .\src .\examples\generic_decode\main.cpp -o .\build\demo_decode.exe
//   .\build\demo_decode.exe
#include "bthome.h"

#include <stdint.h>
#include <stdio.h>

// packet id 0, temperature 22.4, humidity 54.3, battery 92
static const uint8_t kSensorAd[] = {0x0E, 0x16, 0xD2, 0xFC, 0x40, 0x00, 0x00, 0x01,
                                    0x5C, 0x02, 0xC0, 0x08, 0x03, 0x36, 0x15};

// packet id 1, button press, dimmer rotate left 3 steps (trigger-based)
static const uint8_t kEventAd[] = {0x0B, 0x16, 0xD2, 0xFC, 0x44, 0x00,
                                   0x01, 0x3A, 0x01, 0x3C, 0x01, 0x03};

static bool decodeAdElement(const char* label, const uint8_t* ad, size_t len) {
    printf("%s\n", label);

    // An AD element starts with [length][type]; the service data the decoder
    // wants begins after those two bytes.
    BTHome::Decoder dec(ad + 2, len - 2);

    BTHome::Decoded obj;
    while (dec.next(obj)) {
        // is() compares against the object id enums - no hex literals, no
        // casts. Sensors carry value; events and exact integers carry raw,
        // which event() and steps() read back.
        if (obj.is(BTHome::ObjectId::PacketId)) {
            printf("  packet id    %u\n", static_cast<unsigned>(obj.raw));
        } else if (obj.is(BTHome::ObjectId::Battery)) {
            printf("  battery      %.0f %%\n", static_cast<double>(obj.value));
        } else if (obj.is(BTHome::ObjectId::Temperature)) {
            printf("  temperature  %.2f C\n", static_cast<double>(obj.value));
        } else if (obj.is(BTHome::ObjectId::Humidity)) {
            printf("  humidity     %.1f %%\n", static_cast<double>(obj.value));
        } else if (obj.is(BTHome::ObjectId::ButtonEvent)) {
            printf("  button       event %u\n", obj.event());
        } else if (obj.is(BTHome::ObjectId::DimmerEvent)) {
            printf("  dimmer       event %u, %u steps\n", obj.event(), obj.steps());
        }
    }

    // End is the only clean outcome. Truncated means the transport lost
    // bytes, Encrypted means decrypt first, and UnknownId means the sender
    // used an object this version does not know - obj.object_id names it.
    if (dec.status() != BTHome::DecodeStatus::End) {
        printf("  stopped early\n");
        return false;
    }
    return true;
}

int main() {
    bool ok = decodeAdElement("sensor packet", kSensorAd, sizeof(kSensorAd));
    ok = decodeAdElement("event packet", kEventAd, sizeof(kEventAd)) && ok;
    return ok ? 0 : 1;
}
