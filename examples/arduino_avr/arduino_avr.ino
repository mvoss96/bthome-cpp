// BTHome payload building on a classic AVR (Uno/Nano/Pro Mini, ATmega328P).
//
// AVRs have no BLE radio, but the library only builds bytes — hand them to
// whatever transport you have: a UART-attached BLE module that supports
// custom advertising data, a raw 2.4 GHz radio (e.g. nRF24L01), or plain
// serial for inspection, as done here.
//
// Also serves as the CI proof that the library compiles with avr-gcc,
// which ships no libstdc++ wrapper headers (<cstdint> etc.).
#include <Arduino.h>
#include "bthome.h"

static uint8_t packetId = 0;

static void dumpHex(const char *label, const uint8_t *data, size_t len)
{
    Serial.print(label);
    char buf[4];
    for (size_t i = 0; i < len; i++)
    {
        snprintf(buf, sizeof(buf), " %02X", data[i]);
        Serial.print(buf);
    }
    Serial.println();
}

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    // A sensor-style packet ...
    BTHome::Packet<31> sensor;
    sensor.add(BTHome::packet_id(packetId++));
    sensor.add(BTHome::temperature(22.4f));
    sensor.add(BTHome::humidity(54.3f));
    sensor.add(BTHome::battery(92));
    dumpHex("sensor AD element: ", sensor.data(), sensor.size());

    // ... and an event-style packet (e.g. for a battery powered remote)
    BTHome::Packet<31> event;
    event.setTriggerBased(true);
    event.add(BTHome::packet_id(packetId++));
    event.add(BTHome::button_event(BTHome::ButtonEventType::Press));
    event.add(BTHome::dimmer_event(BTHome::DimmerEventType::RotateLeft, 3));
    dumpHex("event AD element:  ", event.data(), event.size());

    delay(5000);
}
