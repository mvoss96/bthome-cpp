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

    BTHome::StaticPacket<BTHome::ObjectId::PacketId,
                         BTHome::ObjectId::Battery,
                         BTHome::ObjectId::Temperature,
                         BTHome::ObjectId::Humidity>
        fixed;
    expect_true("match: put packet id", fixed.put<BTHome::ObjectId::PacketId>(BTHome::packet_id(7)));
    expect_true("match: put temperature",
                fixed.put<BTHome::ObjectId::Temperature>(BTHome::temperature(22.4f)));
    expect_true("match: put humidity",
                fixed.put<BTHome::ObjectId::Humidity>(BTHome::humidity(54.3f)));
    expect_true("match: put battery", fixed.put<BTHome::ObjectId::Battery>(BTHome::battery(92)));

    expect_bytes("match: identical AD element", fixed.data(), fixed.size(),
                 dynamic.data(), dynamic.size());
}

static void test_declaration_order_irrelevant()
{
    // Tests: The same objects listed in descending id order.
    // Expects: The same bytes - offsets follow the id order, not the order the
    //          ids were written in, because BTHome serializes ascending.
    BTHome::StaticPacket<BTHome::ObjectId::Humidity,
                         BTHome::ObjectId::Temperature,
                         BTHome::ObjectId::Battery>
        reversed;
    reversed.put<BTHome::ObjectId::Battery>(BTHome::battery(92));
    reversed.put<BTHome::ObjectId::Temperature>(BTHome::temperature(22.4f));
    reversed.put<BTHome::ObjectId::Humidity>(BTHome::humidity(54.3f));

    BTHome::Packet<31> dynamic;
    dynamic.add(BTHome::battery(92));
    dynamic.add(BTHome::temperature(22.4f));
    dynamic.add(BTHome::humidity(54.3f));

    expect_bytes("order: descending declaration, ascending wire", reversed.data(), reversed.size(),
                 dynamic.data(), dynamic.size());
}

static void test_widths_and_flags()
{
    // Tests: Mixed widths (1, 2, 3, 4) and the trigger-based flag.
    // Expects: Same bytes as Packet, so the compile-time offsets agree with
    //          the table for every width.
    BTHome::StaticPacket<BTHome::ObjectId::Battery,      // 1
                         BTHome::ObjectId::Temperature,  // 2
                         BTHome::ObjectId::Pressure,     // 3
                         BTHome::ObjectId::Timestamp>    // 4
        fixed;
    fixed.setTriggerBased(true);
    fixed.put<BTHome::ObjectId::Battery>(BTHome::battery(87));
    fixed.put<BTHome::ObjectId::Temperature>(BTHome::temperature(-12.34f));
    fixed.put<BTHome::ObjectId::Pressure>(BTHome::pressure(1013.25f));
    fixed.put<BTHome::ObjectId::Timestamp>(BTHome::timestamp(1700000000u));

    BTHome::Packet<31> dynamic;
    dynamic.setTriggerBased(true);
    dynamic.add(BTHome::battery(87));
    dynamic.add(BTHome::temperature(-12.34f));
    dynamic.add(BTHome::pressure(1013.25f));
    dynamic.add(BTHome::timestamp(1700000000u));

    expect_bytes("widths: 1/2/3/4 plus trigger flag", fixed.data(), fixed.size(),
                 dynamic.data(), dynamic.size());
}

static void test_mismatched_put_is_rejected()
{
    // Tests: A measurement handed to the wrong slot.
    // Expects: false and an untouched buffer. Without the check, humidity
    //          bytes would land in the temperature slot and the receiver would
    //          believe them.
    BTHome::StaticPacket<BTHome::ObjectId::Temperature, BTHome::ObjectId::Humidity> fixed;
    fixed.put<BTHome::ObjectId::Temperature>(BTHome::temperature(22.4f));

    uint8_t before[16] = {};
    memcpy(before, fixed.data(), fixed.size());

    expect_true("mismatch: rejected",
                !fixed.put<BTHome::ObjectId::Temperature>(BTHome::humidity(54.3f)));
    expect_true("mismatch: buffer untouched",
                memcmp(before, fixed.data(), fixed.size()) == 0);
}

static void test_unwritten_objects_and_advertising()
{
    // Tests: An object that never gets a put(), and the shared advertising
    // builder.
    // Expects: The id is present with zero value bytes - a StaticPacket always
    //          transmits its full set - and build_advertising() accepts the
    //          type without an overload of its own.
    BTHome::StaticPacket<BTHome::ObjectId::Temperature, BTHome::ObjectId::Humidity> fixed;
    fixed.put<BTHome::ObjectId::Temperature>(BTHome::temperature(22.4f));

    const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x02, 0xC0, 0x08, 0x03, 0x00, 0x00};
    expect_bytes("unwritten: humidity transmits as zero", fixed.serviceData(),
                 fixed.serviceDataSize(), want_sd, sizeof(want_sd));

    uint8_t adv[31] = {};
    const int n = BTHome::build_advertising(fixed, adv, sizeof(adv), "Sens");
    const uint8_t want_adv[] = {0x02, 0x01, 0x06,
                                0x0A, 0x16, 0xD2, 0xFC, 0x40, 0x02, 0xC0, 0x08, 0x03, 0x00, 0x00,
                                0x05, 0x09, 0x53, 0x65, 0x6E, 0x73};
    expect_bytes("unwritten: advertising built", adv, static_cast<size_t>(n > 0 ? n : 0),
                 want_adv, sizeof(want_adv));
}

static void test_decodes_back()
{
    // Tests: A StaticPacket through the Decoder.
    // Expects: The values that went in - the two halves of the library agree.
    BTHome::StaticPacket<BTHome::ObjectId::Temperature, BTHome::ObjectId::Motion> fixed;
    fixed.put<BTHome::ObjectId::Temperature>(BTHome::temperature(21.5f));
    fixed.put<BTHome::ObjectId::Motion>(BTHome::motion(true));

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

static void test_size_is_exact()
{
    // Tests: The buffer is sized to the objects, not to a worst case.
    // Expects: header (5) + 1+2 + 1+1 = 10 bytes, and sizeof matches - this is
    //          the whole point on a 2 KB device.
    using Two = BTHome::StaticPacket<BTHome::ObjectId::Temperature, BTHome::ObjectId::Battery>;
    Two fixed;
    expect_true("size: exact AD length", fixed.size() == 10);
    expect_true("size: no slack in the object", sizeof(Two) == 10);
    expect_true("size: smaller than Packet<31>", sizeof(Two) < sizeof(BTHome::Packet<31>));
}

int main()
{
    test_matches_packet();
    test_declaration_order_irrelevant();
    test_widths_and_flags();
    test_mismatched_put_is_rejected();
    test_unwritten_objects_and_advertising();
    test_decodes_back();
    test_size_is_exact();

    return test_summary();
}
