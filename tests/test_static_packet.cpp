// StaticPacket must emit exactly what Packet emits for the same objects -
// it only decides the layout earlier. Build & run from project root:
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -Werror -I src tests/test_static_packet.cpp -o build/test_static_packet
//   ./build/test_static_packet
#include "bthome.h"
#include "test_utils.h"

static void test_matches_packet()
{
    // Tests: The same four objects through both packet types.
    // Expects: Byte-identical AD elements. StaticPacket is a layout decision,
    //          not a different format.
    BTHome::Packet<31> dynamic;
    dynamic.add(BTHome::humidity(54.3f));
    dynamic.add(BTHome::temperature(22.4f));
    dynamic.add(BTHome::battery(92));
    dynamic.add(BTHome::packet_id(7));

    // No template arguments: the ids come from the values.
    BTHome::StaticPacket fixed(BTHome::packet_id(7), BTHome::temperature(22.4f),
                               BTHome::humidity(54.3f), BTHome::battery(92));

    expect_bytes("match: identical AD element", fixed.data(), fixed.size(),
                 dynamic.data(), dynamic.size());
}

static void test_argument_order_irrelevant()
{
    // Tests: The same objects passed in descending id order.
    // Expects: The same bytes - objects are placed by id, because BTHome
    //          serializes ascending regardless of how they were written.
    BTHome::StaticPacket reversed(BTHome::humidity(54.3f), BTHome::temperature(22.4f),
                                  BTHome::battery(92));

    BTHome::Packet<31> dynamic;
    dynamic.add(BTHome::battery(92));
    dynamic.add(BTHome::temperature(22.4f));
    dynamic.add(BTHome::humidity(54.3f));

    expect_bytes("order: descending arguments, ascending wire", reversed.data(), reversed.size(),
                 dynamic.data(), dynamic.size());
}

static void test_widths_and_flags()
{
    // Tests: Mixed widths (1, 2, 3, 4) and the trigger-based flag.
    // Expects: Same bytes as Packet, so the compile-time offsets agree with
    //          the table for every width.
    BTHome::StaticPacket fixed(BTHome::battery(87), BTHome::temperature(-12.34f),
                               BTHome::pressure(1013.25f), BTHome::timestamp(1700000000u));
    fixed.setTriggerBased(true);

    BTHome::Packet<31> dynamic;
    dynamic.setTriggerBased(true);
    dynamic.add(BTHome::battery(87));
    dynamic.add(BTHome::temperature(-12.34f));
    dynamic.add(BTHome::pressure(1013.25f));
    dynamic.add(BTHome::timestamp(1700000000u));

    expect_bytes("widths: 1/2/3/4 plus trigger flag", fixed.data(), fixed.size(),
                 dynamic.data(), dynamic.size());
}

static void test_advertising_and_decode()
{
    // Tests: The shared advertising builder and the decoder.
    // Expects: build_advertising() takes the type without an overload of its
    //          own, and the result decodes back to what went in.
    BTHome::StaticPacket fixed(BTHome::temperature(21.5f), BTHome::motion(true));

    uint8_t adv[31] = {};
    const int n = BTHome::build_advertising(fixed, adv, sizeof(adv), "Sens");
    const uint8_t want_adv[] = {0x02, 0x01, 0x06,
                                0x09, 0x16, 0xD2, 0xFC, 0x40, 0x02, 0x66, 0x08, 0x21, 0x01,
                                0x05, 0x09, 0x53, 0x65, 0x6E, 0x73};
    expect_bytes("advertising: built from a StaticPacket", adv,
                 static_cast<size_t>(n > 0 ? n : 0), want_adv, sizeof(want_adv));

    BTHome::Decoder dec(fixed.serviceData(), fixed.serviceDataSize());
    BTHome::Decoded obj;
    bool motion = false;
    float temperature = 0.0f;
    while (dec.next(obj))
    {
        if (obj.is(BTHome::ObjectId::Temperature)) { temperature = obj.value; }
        else if (obj.is(BTHome::ObjectId::Motion)) { motion = obj.on(); }
    }
    expect_true("decode: clean end", dec.status() == BTHome::DecodeStatus::End);
    expect_true("decode: values round-trip",
                motion && temperature > 21.49f && temperature < 21.51f);
}

static void test_typed_is_still_a_measurement()
{
    // Tests: The factories now return Typed<Id>, which Packet must still take.
    // Expects: Packet::add() works unchanged, and Typed is smaller than a
    //          Measurement - it carries only its own width, which is what
    //          keeps the StaticPacket constructor cheap.
    BTHome::Packet<31> p;
    expect_true("typed: Packet::add accepts it", p.add(BTHome::temperature(21.0f)));
    expect_true("typed: smaller than Measurement",
                sizeof(BTHome::Typed<BTHome::ObjectId::Temperature>) <
                    sizeof(BTHome::Measurement));
    expect_true("typed: exactly len plus its own bytes",
                sizeof(BTHome::Typed<BTHome::ObjectId::Temperature>) == 3 &&
                    sizeof(BTHome::Typed<BTHome::ObjectId::Battery>) == 2);

    // And it still converts to a plain Measurement where one is wanted.
    const BTHome::Measurement plain = BTHome::temperature(21.0f);
    expect_true("typed: slices to Measurement",
                plain.object_id == static_cast<uint8_t>(BTHome::ObjectId::Temperature));
}

static void test_size_is_exact()
{
    // Tests: The buffer is sized to the objects, not to a worst case.
    // Expects: header (5) + 1+2 + 1+1 = 10 bytes, and sizeof matches - this is
    //          the whole point on a 2 KB device.
    BTHome::StaticPacket fixed(BTHome::temperature(21.0f), BTHome::battery(90));
    expect_true("size: exact AD length", fixed.size() == 10);
    expect_true("size: no slack in the object", sizeof(fixed) == 10);
    expect_true("size: smaller than Packet<31>", sizeof(fixed) < sizeof(BTHome::Packet<31>));
}

int main()
{
    test_matches_packet();
    test_argument_order_irrelevant();
    test_widths_and_flags();
    test_advertising_and_decode();
    test_typed_is_still_a_measurement();
    test_size_is_exact();

    return test_summary();
}
