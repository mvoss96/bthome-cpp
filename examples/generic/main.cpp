// Generic example: build a payload and print the bytes. No BLE stack.
// From project root:
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I .\src .\examples\generic\main.cpp -o .\build\demo_generic.exe
//   .\build\demo_generic.exe
#include "bthome.h"

#include <cstdint>
#include <cstdio>

int main() {
    BTHome::Packet<31> packet;

    // Add in any order; the library emits canonical (ascending) order.
    bool ok = true;
    ok = packet.add(BTHome::temperature(22.4f)) && ok;
    ok = packet.add(BTHome::humidity(54.3f)) && ok;
    ok = packet.add(BTHome::battery(92)) && ok;
    ok = packet.add(BTHome::motion(true)) && ok;

    // packet.data()/size()        -> full AD element ([len][0x16]...)
    // packet.serviceData()/...    -> service-data value ([UUID lo][UUID hi]...)
    const std::uint8_t* p = packet.data();
    std::printf("BTHome AD element (%zu bytes): ", packet.size());
    for (std::size_t i = 0; i < packet.size(); ++i)
    {
        std::printf("%02X ", p[i]);
    }
    std::printf("\n");

    if (!ok)
    {
        std::printf("WARNING: payload item did not fit\n");
    }
    return 0;
}
