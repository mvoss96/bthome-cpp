// The arduino_avr example with StaticPacket instead of Packet.
//
// Same two packets, same bytes on the wire - but the object set is fixed at
// compile time, so there is no insertion sort, no memmove and no offset table,
// and each buffer is exactly as large as its objects. On an ATmega328P that is
// about 800 bytes of flash and 37 bytes of stack per packet.
//
// Use Packet instead when the object set varies between packets, for
// Text/Raw/command events, for repeated ids such as button padding, or where a
// sensor can be unavailable - a StaticPacket always sends its whole set.
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
    // The object ids come from the values - no template arguments to write,
    // and a value cannot end up under the wrong id.
    BTHome::StaticPacket sensor(BTHome::packet_id(packetId++),
                                BTHome::temperature(22.4f),
                                BTHome::humidity(54.3f),
                                BTHome::battery(92));
    dumpHex("sensor AD element: ", sensor.data(), sensor.size());

    BTHome::StaticPacket event(BTHome::packet_id(packetId++),
                               BTHome::button_event(BTHome::ButtonEventType::Press),
                               BTHome::dimmer_event(BTHome::DimmerEventType::RotateLeft, 3));
    event.setTriggerBased(true);
    dumpHex("event AD element:  ", event.data(), event.size());

    delay(5000);
}
