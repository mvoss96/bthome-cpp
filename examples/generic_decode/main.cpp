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
        switch (obj.kind) {
            case BTHome::ObjectKind::PacketId:
                // Not every object is a measurement: the packet id is an
                // exact integer and lives in raw, not in value.
                printf("  0x%02X  packet id %u\n", obj.object_id, static_cast<unsigned>(obj.raw));
                break;
            case BTHome::ObjectKind::Binary:
                printf("  0x%02X  %s\n", obj.object_id, obj.on ? "on" : "off");
                break;
            case BTHome::ObjectKind::ButtonEvent:
                printf("  0x%02X  button event %u\n", obj.object_id, obj.event);
                break;
            case BTHome::ObjectKind::DimmerEvent:
                printf("  0x%02X  dimmer event %u, %u steps\n", obj.object_id, obj.event, obj.steps);
                break;
            default:
                printf("  0x%02X  %.2f\n", obj.object_id, static_cast<double>(obj.value));
                break;
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
