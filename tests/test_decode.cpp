// Round-trip tests for the Decoder: everything is encoded with the public
// factories/Packet API and must decode back to the original values.
#include "bthome.h"
#include "test_utils.h"

#include <math.h>

static bool near(float a, float b, float eps)
{
    return fabsf(a - b) <= eps;
}

// Collect all decoded objects of a packet into a fixed array.
template <size_t Cap>
static size_t decode_all(const BTHome::Packet<31> &p, BTHome::Decoded (&out)[Cap],
                         BTHome::Decoder *dec_out = nullptr)
{
    BTHome::Decoder dec(p.serviceData(), p.serviceDataSize());
    size_t n = 0;
    BTHome::Decoded d;
    while (n < Cap && dec.next(d))
    {
        out[n++] = d;
    }
    if (dec_out != nullptr)
    {
        *dec_out = dec;
    }
    return n;
}

static void test_header()
{
    BTHome::Packet<31> p;
    p.setTriggerBased(true);
    p.add(BTHome::packet_id(42));

    BTHome::Decoder dec(p.serviceData(), p.serviceDataSize());
    expect_true("header: starts ready", dec.status() == BTHome::DecodeStatus::Ok);
    expect_true("header: version 2", dec.version() == 2);
    expect_true("header: trigger-based", dec.triggerBased());
    expect_true("header: not encrypted", !dec.encrypted());

    const uint8_t bogus[3] = {0xAA, 0xBB, 0x40};
    BTHome::Decoder bad(bogus, sizeof(bogus));
    expect_true("header: wrong uuid rejected",
                bad.status() == BTHome::DecodeStatus::BadHeader);

    BTHome::Decoder tooShort(p.serviceData(), 2);
    expect_true("header: too short rejected",
                tooShort.status() == BTHome::DecodeStatus::BadHeader);

    // A valid but empty payload is a clean pass, not an error.
    const uint8_t empty[3] = {0xD2, 0xFC, 0x40};
    BTHome::Decoder none(empty, sizeof(empty));
    BTHome::Decoded discard;
    expect_true("header: empty payload yields nothing", !none.next(discard));
    expect_true("header: empty payload ends cleanly",
                none.status() == BTHome::DecodeStatus::End);
}

static void test_scaled_sensors()
{
    BTHome::Packet<31> p;
    p.add(BTHome::temperature(-12.34f)); // signed, 0.01
    p.add(BTHome::humidity(54.32f));     // unsigned, 0.01
    p.add(BTHome::voltage(2.432f));      // unsigned, 0.001
    p.add(BTHome::battery(87));          // u8

    BTHome::Decoded d[8];
    BTHome::Decoder dec(nullptr, 0);
    const size_t n = decode_all(p, d, &dec);
    expect_true("sensors: count", n == 4);
    expect_true("sensors: clean end", dec.status() == BTHome::DecodeStatus::End);

    // Canonical order: battery 0x01, temperature 0x02, humidity 0x03, voltage 0x0C
    expect_true("sensors: battery", d[0].kind == BTHome::ObjectKind::Sensor && near(d[0].value, 87.0f, 0.001f));
    expect_true("sensors: temperature", near(d[1].value, -12.34f, 0.005f));
    expect_true("sensors: humidity", near(d[2].value, 54.32f, 0.005f));
    expect_true("sensors: voltage", near(d[3].value, 2.432f, 0.0005f));
}

static void test_integer_sensors()
{
    BTHome::Packet<31> p;
    p.add(BTHome::count(200));
    p.add(BTHome::count_u16(60000));
    p.add(BTHome::count_u32(70000));
    p.add(BTHome::count_s16(-1234));
    p.add(BTHome::timestamp(1700000000u));

    BTHome::Decoded d[8];
    BTHome::Decoder dec(nullptr, 0);
    const size_t n = decode_all(p, d, &dec);
    expect_true("ints: count", n == 5);
    expect_true("ints: clean end", dec.status() == BTHome::DecodeStatus::End);
    // Order: 0x09 count, 0x3D u16, 0x3E u32, 0x50 timestamp, 0x5A s16
    expect_true("ints: count u8", d[0].raw == 200);
    expect_true("ints: count u16", d[1].raw == 60000);
    expect_true("ints: count u32", d[2].raw == 70000);
    expect_true("ints: timestamp", d[3].raw == 1700000000u);
    expect_true("ints: count s16", near(d[4].value, -1234.0f, 0.5f));
}

static void test_binary_and_events()
{
    BTHome::Packet<31> p;
    p.add(BTHome::packet_id(7));
    p.add(BTHome::motion(true));
    p.add(BTHome::button_event(BTHome::ButtonEventType::None)); // button 1 padding
    p.add(BTHome::button_event(BTHome::ButtonEventType::DoublePress)); // button 2
    p.add(BTHome::command_event(BTHome::CommandEventType::StepUp, 5));
    p.add(BTHome::dimmer_event(BTHome::DimmerEventType::RotateLeft, 3));

    BTHome::Decoded d[8];
    BTHome::Decoder dec(nullptr, 0);
    const size_t n = decode_all(p, d, &dec);
    expect_true("events: count", n == 6);
    expect_true("events: clean end", dec.status() == BTHome::DecodeStatus::End);

    // Order: 0x00 pid, 0x21 motion, 0x3A, 0x3A, 0x3B command, 0x3C dimmer
    expect_true("events: packet id", d[0].kind == BTHome::ObjectKind::PacketId && d[0].raw == 7);
    expect_true("events: motion", d[1].kind == BTHome::ObjectKind::Binary && d[1].on);
    expect_true("events: button 1 none",
                d[2].kind == BTHome::ObjectKind::ButtonEvent &&
                    d[2].event == static_cast<uint8_t>(BTHome::ButtonEventType::None));
    expect_true("events: button 2 double",
                d[3].event == static_cast<uint8_t>(BTHome::ButtonEventType::DoublePress));
    expect_true("events: command step up 5",
                d[4].kind == BTHome::ObjectKind::CommandEvent &&
                    d[4].event == static_cast<uint8_t>(BTHome::CommandEventType::StepUp) &&
                    d[4].steps == 5);
    expect_true("events: dimmer left 3",
                d[5].kind == BTHome::ObjectKind::DimmerEvent &&
                    d[5].event == static_cast<uint8_t>(BTHome::DimmerEventType::RotateLeft) &&
                    d[5].steps == 3);
}

static void test_device_objects_and_text()
{
    BTHome::Packet<31> p;
    p.add(BTHome::device_type_id(1));
    p.add(BTHome::firmware_version_u24(0x010203u)); // 1.2.3
    p.add(BTHome::text("RotRemote"));

    BTHome::Decoded d[8];
    BTHome::Decoder dec(nullptr, 0);
    const size_t n = decode_all(p, d, &dec);
    expect_true("device: count", n == 3);
    expect_true("device: clean end", dec.status() == BTHome::DecodeStatus::End);

    // Order: 0x53 text, 0xF0 type, 0xF2 fw
    expect_true("device: text kind", d[0].kind == BTHome::ObjectKind::Text && d[0].length == 9);
    expect_true("device: text bytes",
                d[0].bytes != nullptr && memcmp(d[0].bytes, "RotRemote", 9) == 0);
    expect_true("device: type id", d[1].kind == BTHome::ObjectKind::DeviceTypeId && d[1].raw == 1);
    expect_true("device: fw 1.2.3",
                d[2].kind == BTHome::ObjectKind::FirmwareVersion && d[2].raw == 0x010203u);
}

// Objects sort by id, so look them up by id rather than by position.
static const BTHome::Decoded *find(const BTHome::Decoded *d, size_t n, uint8_t id)
{
    for (size_t i = 0; i < n; ++i)
    {
        if (d[i].object_id == id)
        {
            return &d[i];
        }
    }
    return nullptr;
}

static void test_signed_widths()
{
    // Signed objects at every width the table uses, scaled and exact. These
    // are the cases the old sign extension got wrong on 16-bit-int targets:
    // it shifted `~0u`, which is 16 bits wide on avr-gcc, so it could not
    // reach bits 16..31 and negative values came out as large positives.
    BTHome::Packet<31> p;
    p.add(BTHome::temperature_s8(-16));       // 0x57 exact   signed w1
    p.add(BTHome::temperature(-12.34f));      // 0x02 scaled  signed w2 x0.01
    p.add(BTHome::count_s32(-70000));         // 0x5B exact   signed w4
    p.add(BTHome::acceleration_s32(-0.5f));   // 0x63 scaled  signed w4 x1e-6

    BTHome::Decoded d[8];
    BTHome::Decoder dec(nullptr, 0);
    const size_t n = decode_all(p, d, &dec);
    expect_true("signed: count", n == 4);
    expect_true("signed: clean end", dec.status() == BTHome::DecodeStatus::End);

    const BTHome::Decoded *s8 = find(d, n, 0x57);
    const BTHome::Decoded *s16 = find(d, n, 0x02);
    const BTHome::Decoded *s32 = find(d, n, 0x5B);
    const BTHome::Decoded *sf32 = find(d, n, 0x63);
    expect_true("signed: width 1 exact (-16)", s8 != nullptr && near(s8->value, -16.0f, 0.001f));
    expect_true("signed: width 2 scaled (-12.34)", s16 != nullptr && near(s16->value, -12.34f, 0.005f));
    expect_true("signed: width 4 exact (-70000)", s32 != nullptr && near(s32->value, -70000.0f, 0.5f));
    expect_true("signed: width 4 scaled (-0.5)", sf32 != nullptr && near(sf32->value, -0.5f, 0.001f));
}

static void test_wide_and_remaining_kinds()
{
    // uint24 (width 3) is a distinct assembly path and was untested, as were
    // Raw, the dimmer None padding and the 32-bit firmware version.
    const uint8_t payload[3] = {0xDE, 0xAD, 0xBE};
    BTHome::Packet<31> p;
    p.add(BTHome::pressure(1013.25f));                             // 0x04 w3 x0.01
    p.add(BTHome::energy(12.345f));                                // 0x0A w3 x0.001
    p.add(BTHome::dimmer_event(BTHome::DimmerEventType::None));    // 0x3C, steps byte present
    p.add(BTHome::raw(payload, sizeof(payload)));                  // 0x54
    p.add(BTHome::firmware_version_u32(0x01020304u));              // 0xF1

    BTHome::Decoded d[8];
    BTHome::Decoder dec(nullptr, 0);
    const size_t n = decode_all(p, d, &dec);
    expect_true("wide: count", n == 5);
    expect_true("wide: clean end", dec.status() == BTHome::DecodeStatus::End);

    const BTHome::Decoded *pres = find(d, n, 0x04);
    const BTHome::Decoded *ener = find(d, n, 0x0A);
    const BTHome::Decoded *dim = find(d, n, 0x3C);
    const BTHome::Decoded *rawo = find(d, n, 0x54);
    const BTHome::Decoded *fw = find(d, n, 0xF1);

    expect_true("wide: uint24 pressure", pres != nullptr && near(pres->value, 1013.25f, 0.01f) && pres->raw == 101325u);
    expect_true("wide: uint24 energy", ener != nullptr && near(ener->value, 12.345f, 0.001f));
    expect_true("wide: dimmer none carries a zero steps byte",
                dim != nullptr && dim->kind == BTHome::ObjectKind::DimmerEvent &&
                    dim->event == static_cast<uint8_t>(BTHome::DimmerEventType::None) && dim->steps == 0);
    expect_true("wide: raw kind and bytes",
                rawo != nullptr && rawo->kind == BTHome::ObjectKind::Raw && rawo->length == 3 &&
                    rawo->bytes != nullptr && memcmp(rawo->bytes, payload, 3) == 0);
    expect_true("wide: firmware u32",
                fw != nullptr && fw->kind == BTHome::ObjectKind::FirmwareVersion && fw->raw == 0x01020304u);
}

static void test_payload_entry_point()
{
    BTHome::Packet<31> p;
    p.add(BTHome::packet_id(3));
    p.add(BTHome::temperature(19.5f));

    // What a BLE stack that matched the UUID itself hands over: the same
    // bytes, minus the two UUID bytes (NimBLE's getServiceData()).
    const uint8_t *payload = p.serviceData() + 2;
    const size_t payload_len = p.serviceDataSize() - 2;

    BTHome::Decoder dec = BTHome::Decoder::fromPayload(payload, payload_len);
    expect_true("payload: header still readable", dec.version() == 2 && !dec.encrypted());

    BTHome::Decoded got[4];
    BTHome::Decoded one;
    size_t n = 0;
    while (n < 4 && dec.next(one))
    {
        got[n++] = one;
    }
    expect_true("payload: same objects as the full buffer",
                n == 2 && got[0].raw == 3 && near(got[1].value, 19.5f, 0.005f));
    expect_true("payload: clean end", dec.status() == BTHome::DecodeStatus::End);

    BTHome::Decoder tooShort = BTHome::Decoder::fromPayload(payload, 0);
    expect_true("payload: empty buffer rejected",
                tooShort.status() == BTHome::DecodeStatus::BadHeader);

    const uint8_t enc[] = {0x41, 0x00, 0x05};
    BTHome::Decoder encrypted = BTHome::Decoder::fromPayload(enc, sizeof(enc));
    expect_true("payload: encrypted flag honoured",
                encrypted.encrypted() && encrypted.status() == BTHome::DecodeStatus::Encrypted);

    // Feeding a UUID-less payload to the service-data constructor is the
    // mistake this entry point exists to prevent - it must not parse.
    BTHome::Decoder mismatched(payload, payload_len);
    expect_true("payload: uuid-less buffer rejected by the other entry point",
                mismatched.status() == BTHome::DecodeStatus::BadHeader);
}

static void test_malformed()
{
    BTHome::Packet<31> p;
    p.add(BTHome::packet_id(1));
    p.add(BTHome::temperature(21.0f));

    // Truncate mid-object: temperature announces 2 value bytes, cut one off.
    {
        BTHome::Decoder dec(p.serviceData(), p.serviceDataSize() - 1);
        BTHome::Decoded d;
        expect_true("truncated: first object still parses", dec.next(d));
        expect_true("truncated: stops", !dec.next(d));
        expect_true("truncated: reported as Truncated",
                    dec.status() == BTHome::DecodeStatus::Truncated);
    }

    // Unknown object id: length unknowable, parser must stop and name the id.
    {
        const uint8_t unknown[] = {0xD2, 0xFC, 0x40, /*pid*/ 0x00, 0x05, /*unknown*/ 0xE7, 0x01};
        BTHome::Decoder dec(unknown, sizeof(unknown));
        BTHome::Decoded d;
        expect_true("unknown id: pid parses", dec.next(d) && d.raw == 5);
        expect_true("unknown id: stops", !dec.next(d));
        expect_true("unknown id: reported as UnknownId",
                    dec.status() == BTHome::DecodeStatus::UnknownId);
        expect_true("unknown id: names the offending id", d.object_id == 0xE7);

        // Terminal status is sticky - another call changes nothing.
        expect_true("unknown id: sticky", !dec.next(d) &&
                                              dec.status() == BTHome::DecodeStatus::UnknownId);
    }

    // Encrypted flag: objects are ciphertext, iteration must refuse - and it
    // must not look like a clean empty packet, which is what ok() used to say.
    {
        const uint8_t enc[] = {0xD2, 0xFC, 0x41, 0x00, 0x05};
        BTHome::Decoder dec(enc, sizeof(enc));
        BTHome::Decoded d;
        expect_true("encrypted: flagged", dec.encrypted());
        expect_true("encrypted: header still readable", dec.version() == 2);
        expect_true("encrypted: no iteration", !dec.next(d));
        expect_true("encrypted: distinguishable from a clean end",
                    dec.status() == BTHome::DecodeStatus::Encrypted);
    }
}

int main()
{
    test_header();
    test_scaled_sensors();
    test_integer_sensors();
    test_binary_and_events();
    test_device_objects_and_text();
    test_signed_widths();
    test_wide_and_remaining_kinds();
    test_payload_entry_point();
    test_malformed();
    return test_summary();
}
