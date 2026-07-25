// Generic decoding example: the counterpart of examples/generic. No BLE
// stack - the same bytes reach a receiver over ESP-NOW, nRF24 or a serial
// link, and a BLE scan yields them just as well.
// From project root:
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I .\src .\examples\generic_decode\main.cpp -o .\build\demo_decode.exe
//   .\build\demo_decode.exe
#include "bthome.h"

#include <stdint.h>
#include <stdio.h>

int main() {
    // The packet examples/generic builds - stand in for received bytes.
    BTHome::Packet<31> packet;
    packet.add(BTHome::temperature(22.4f));
    packet.add(BTHome::humidity(54.3f));
    packet.add(BTHome::battery(92));
    packet.add(BTHome::motion(true));

    BTHome::Decoder dec(packet.serviceData(), packet.serviceDataSize());

    BTHome::Decoded obj;
    while (dec.next(obj))
    {
        if (obj.kind == BTHome::ObjectKind::Binary)
        {
            printf("0x%02X  %s\n", obj.object_id, obj.on ? "on" : "off");
        }
        else
        {
            printf("0x%02X  %.2f\n", obj.object_id, static_cast<double>(obj.value));
        }
    }

    // End is the only clean outcome. Truncated means the transport lost
    // bytes, Encrypted means decrypt first, and UnknownId means the sender
    // used an object this version does not know - obj.object_id names it.
    if (dec.status() != BTHome::DecodeStatus::End)
    {
        printf("stopped early\n");
        return 1;
    }
    return 0;
}
