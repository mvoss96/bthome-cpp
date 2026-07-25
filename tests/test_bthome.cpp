// Host-side correctness test. Build & run from project root:
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I .\src .\tests\test_bthome.cpp -o .\build\test_bthome.exe
//   .\build\test_bthome.exe
#include "bthome.h"
#include "test_utils.h"

#include <math.h>

static void test_header_and_basic_packet()
{
    // Tests: Empty packet without measurements.
    // Expects: Exact header AD block [04 16 D2 FC 40] and serviceDataSize = size-2.
    {
        BTHome::Packet<31> p;
        const uint8_t want[] = {0x04, 0x16, 0xD2, 0xFC, 0x40};
        expect_bytes("header-only packet", p.data(), p.size(), want, sizeof(want));
        expect_true("serviceDataSize == size - 2", p.serviceDataSize() == (p.size() - 2));
    }

    // Tests: Mixed measurements inserted in intentionally unsorted order.
    // Expects: Serialization sorted by object_id and exact match with known byte vector.
    {
        BTHome::Packet<31> p;
        const bool a = p.add(BTHome::humidity(54.2f));
        const bool b = p.add(BTHome::temperature(21.53f));
        const bool c = p.add(BTHome::battery(87));

        const uint8_t want_ad[] = {
            0x0C,
            0x16,
            0xD2,
            0xFC,
            0x40,
            0x01,
            0x57,
            0x02,
            0x69,
            0x08,
            0x03,
            0x2C,
            0x15,
        };
        expect_bytes("mixed measurements sorted by object-id", p.data(), p.size(), want_ad, sizeof(want_ad));

        const uint8_t want_sd[] = {
            0xD2,
            0xFC,
            0x40,
            0x01,
            0x57,
            0x02,
            0x69,
            0x08,
            0x03,
            0x2C,
            0x15,
        };
        expect_bytes("serviceData bytes", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
        expect_true("all mixed adds succeeded", a && b && c);
    }
}

static void test_device_info_flags()
{
    // Tests: Setting device-info bits for trigger-based and encrypted.
    // Expects: Device-info byte is 0x45 (version 2 + trigger bit + encrypted bit).
    BTHome::Packet<31> p;
    p.setTriggerBased(true);
    p.setEncrypted(true);
    expect_true("device-info encrypted+trigger bitmask", p.serviceData()[2] == 0x45);
}

static void test_ordering_and_ids()
{
    // Tests: Two measurements with identical object_id (temperature).
    // Expects: Stable ordering preserved in insertion order (18.2 before 21.5).
    {
        BTHome::Packet<31> p;
        p.add(BTHome::temperature(18.2f));
        p.add(BTHome::temperature(21.5f));

        const uint8_t want_sd[] = {
            0xD2,
            0xFC,
            0x40,
            0x02,
            0x1C,
            0x07,
            0x02,
            0x66,
            0x08,
        };
        expect_bytes("stable order for equal object-id", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: packet_id (0x00) together with a sensor value.
    // Expects: packet_id is serialized before Temperature.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::temperature(20.0f));
        p.add(BTHome::packet_id(7));

        const uint8_t want_sd[] = {
            0xD2,
            0xFC,
            0x40,
            0x00,
            0x07,
            0x02,
            0xD0,
            0x07,
        };
        expect_bytes("packet_id sorts first", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Mapping of binary IDs motion/window according to the BTHome table.
    // Expects: motion=0x21 and window=0x2D with bool payload 0x01/0x00.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::motion(true));
        p.add(BTHome::window(false));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x21, 0x01, 0x2D, 0x00};
        expect_bytes("binary ids motion/window", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    {
        // Tests: Mapping of binary IDs heat/lock according to the BTHome table.
        // Expects: heat=0x1D and lock=0x1F with correct bool payload bytes.
        BTHome::Packet<31> p;
        p.add(BTHome::heat(true));
        p.add(BTHome::lock(false));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x1D, 0x01, 0x1F, 0x00};
        expect_bytes("binary ids heat/lock", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    {
        // Tests: Boundary area of adjacent IDs 0x2D (window) and 0x2E (humidity_u8).
        // Expects: No ID mix-up, exactly [2D 00 2E 23] in the payload section.
        BTHome::Packet<31> p;
        p.add(BTHome::window(false));
        p.add(BTHome::humidity_u8(35));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x2D, 0x00, 0x2E, 0x23};
        expect_bytes("window (0x2D) vs humidity_u8 (0x2E)", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }
}

static void test_numeric_encoding_paths()
{
    // Tests: Signed temperature encoding with a negative value.
    // Expects: Two's-complement little-endian, -12.34 -> [2E FB].
    {
        BTHome::Packet<31> p;
        p.add(BTHome::temperature(-12.34f));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x02, 0x2E, 0xFB};
        expect_bytes("negative temperature encoding", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Unsigned 24-bit path for pressure.
    // Expects: 1013.25 hPa -> raw 101325 -> [CD 8B 01].
    {
        BTHome::Packet<31> p;
        p.add(BTHome::pressure(1013.25f));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x04, 0xCD, 0x8B, 0x01};
        expect_bytes("pressure uint24 encoding", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Unsigned 32-bit scaling path for energy_u32.
    // Expects: 12.345 -> raw 12345 -> [39 30 00 00].
    {
        BTHome::Packet<31> p;
        p.add(BTHome::energy_u32(12.345f));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x4D, 0x39, 0x30, 0x00, 0x00};
        expect_bytes("energy_u32 scaled", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Signed integer wrapper count_s16.
    // Expects: -22 encoded as int16 little-endian [EA FF].
    {
        BTHome::Packet<31> p;
        p.add(BTHome::count_s16(-22));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x5A, 0xEA, 0xFF};
        expect_bytes("count_s16 signed wrapper", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Raw u32 path for timestamp without scaling.
    // Expects: epoch 1684093277 -> [5D 39 61 64] little-endian.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::timestamp(1684093277u));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x50, 0x5D, 0x39, 0x61, 0x64};
        expect_bytes("timestamp uint32", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }
}

static void test_device_object_paths()
{
    // Tests: Device type id object 0xF0 with uint16 payload.
    // Expects: F0 + little-endian 0x0001 -> [F0 01 00].
    {
        BTHome::Packet<31> p;
        p.add(BTHome::device_type_id(1));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0xF0, 0x01, 0x00};
        expect_bytes("device type id u16", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Firmware version object 0xF1 with uint32 payload.
    // Expects: 4.2.1.0 packed as 0x04020100 -> [F1 00 01 02 04].
    {
        BTHome::Packet<31> p;
        p.add(BTHome::firmware_version_u32(0x04020100u));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0xF1, 0x00, 0x01, 0x02, 0x04};
        expect_bytes("firmware version u32", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Firmware version object 0xF2 with uint24 payload.
    // Expects: 6.1.0 packed as 0x060100 -> [F2 00 01 06].
    {
        BTHome::Packet<31> p;
        p.add(BTHome::firmware_version_u24(0x060100u));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0xF2, 0x00, 0x01, 0x06};
        expect_bytes("firmware version u24", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }
}

static void test_rounding_and_clamping()
{
    // Tests: "Half away from zero" rounding for signed scaled values.
    // Expects: +1.235 -> +124 ([7C 00]) and -1.235 -> -124 ([84 FF]).
    {
        BTHome::Packet<31> p;
        p.add(BTHome::temperature(1.235f)); // 123.5 -> 124 -> 0x007C
        p.add(BTHome::dewpoint(-1.235f));   // -123.5 -> -124 -> 0xFF84

        const uint8_t want_sd[] = {
            0xD2,
            0xFC,
            0x40,
            0x02,
            0x7C,
            0x00,
            0x08,
            0x84,
            0xFF,
        };
        expect_bytes("rounding half away from zero", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Signed clamping for 2-byte temperature outside representable range.
    // Expects: +500.0 -> max int16 ([FF 7F]) and -500.0 -> min int16 ([00 80]).
    {
        BTHome::Packet<31> p;
        p.add(BTHome::temperature(500.0f));
        p.add(BTHome::dewpoint(-500.0f));

        const uint8_t want_sd[] = {
            0xD2,
            0xFC,
            0x40,
            0x02,
            0xFF,
            0x7F,
            0x08,
            0x00,
            0x80,
        };
        expect_bytes("signed clamp to int16 range", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Unsigned clamping for underflow and overflow.
    // Expects: humidity(-3.0) -> [00 00] and pressure(200000.0) -> [FF FF FF].
    {
        BTHome::Packet<31> p;
        p.add(BTHome::humidity(-3.0f));     // unsigned 2-byte scaled -> clamp to 0
        p.add(BTHome::pressure(200000.0f)); // unsigned 3-byte scaled -> clamp to 0xFFFFFF

        const uint8_t want_sd[] = {
            0xD2,
            0xFC,
            0x40,
            0x03,
            0x00,
            0x00,
            0x04,
            0xFF,
            0xFF,
            0xFF,
        };
        expect_bytes("unsigned clamp floor/ceiling", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }
}

static void test_capacity_and_overflow_behavior()
{
    // Tests: Capacity limit of a very small packet with a follow-up add.
    // Expects: First add succeeds, second add is rejected, packet stays valid and unchanged.
    {
        BTHome::Packet<8> p;
        const bool first_ok = p.add(BTHome::battery(50));
        const bool second_ok = p.add(BTHome::humidity(40.0f));

        expect_true("tiny packet first add ok", first_ok);
        expect_true("tiny packet second add rejected", !second_ok);

        const uint8_t want_ad[] = {0x06, 0x16, 0xD2, 0xFC, 0x40, 0x01, 0x32};
        expect_bytes("tiny packet remains valid after rejection", p.data(), p.size(), want_ad, sizeof(want_ad));
    }
}

static void test_advertising_builder()
{
    // Tests: Basic advertising build without local name.
    // Expects: Flags AD + BTHome service-data AD in exactly this order.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::battery(87));

        uint8_t adv[31] = {};
        const int adv_size = BTHome::build_advertising(p, adv, sizeof(adv));
        expect_true("adv basic build success", adv_size >= 0);

        const uint8_t want[] = {
            0x02,
            0x01,
            0x06,
            0x06,
            0x16,
            0xD2,
            0xFC,
            0x40,
            0x01,
            0x57,
        };
        expect_bytes("adv basic payload", adv, static_cast<size_t>(adv_size), want, sizeof(want));
    }

    // Tests: Advertising with complete local name (AD type 0x09).
    // Expects: Correctly appends [len 0x09 'node'] after the BTHome block.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::battery(87));

        uint8_t adv[31] = {};
        const int adv_size = BTHome::build_advertising(p, adv, sizeof(adv), "node", true);
        expect_true("adv with complete local name", adv_size >= 0);

        const uint8_t want[] = {
            0x02,
            0x01,
            0x06,
            0x06,
            0x16,
            0xD2,
            0xFC,
            0x40,
            0x01,
            0x57,
            0x05,
            0x09,
            'n',
            'o',
            'd',
            'e',
        };
        expect_bytes("adv local name complete", adv, static_cast<size_t>(adv_size), want, sizeof(want));
    }

    // Tests: Advertising with shortened local name (AD type 0x08).
    // Expects: Correctly appends [len 0x08 'nd'] after the BTHome block.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::battery(87));

        uint8_t adv[31] = {};
        const int adv_size = BTHome::build_advertising(p, adv, sizeof(adv), "nd", false);
        expect_true("adv with shortened local name", adv_size >= 0);

        const uint8_t want[] = {
            0x02,
            0x01,
            0x06,
            0x06,
            0x16,
            0xD2,
            0xFC,
            0x40,
            0x01,
            0x57,
            0x03,
            0x08,
            'n',
            'd',
        };
        expect_bytes("adv local name shortened", adv, static_cast<size_t>(adv_size), want, sizeof(want));
    }

    // Tests: Error paths of the advertising builder.
    // Expects: Returns -1 for too-small output buffer, nullptr output, and too-long name (>254).
    {
        BTHome::Packet<31> p;
        p.add(BTHome::battery(87));

        uint8_t adv_small[9] = {};
        expect_true("adv fails on too-small output buffer",
                    BTHome::build_advertising(p, adv_small, sizeof(adv_small)) == -1);

        expect_true("adv fails on null output pointer",
                    BTHome::build_advertising(p, nullptr, 31) == -1);

        char long_name[256] = {};
        for (size_t i = 0; i < 255; ++i)
        {
            long_name[i] = 'A';
        }
        long_name[255] = '\0';
        uint8_t adv[300] = {};
        expect_true("adv rejects local name > 254 bytes",
                    BTHome::build_advertising(p, adv, sizeof(adv), long_name, true) == -1);
    }
}

static void test_insert_positions()
{
    // Tests: Insertions that land at the front, middle and end of the sorted
    // payload, in an order that exercises each position.
    // Expects: Final serialization strictly sorted regardless of add order.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::timestamp(0x64613955u)); // 0x50 - becomes the tail
        p.add(BTHome::battery(87));            // 0x01 - insert at front
        p.add(BTHome::motion(true));           // 0x21 - insert in the middle
        p.add(BTHome::packet_id(1));           // 0x00 - new front
        p.add(BTHome::conductivity(3120.0f));  // 0x56 - new tail

        const uint8_t want_sd[] = {
            0xD2, 0xFC, 0x40,
            0x00, 0x01,                   // packet_id
            0x01, 0x57,                   // battery
            0x21, 0x01,                   // motion
            0x50, 0x55, 0x39, 0x61, 0x64, // timestamp
            0x56, 0x30, 0x0C,             // conductivity
        };
        expect_bytes("insert front/middle/end", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Three measurements with the same object_id.
    // Expects: Insertion order preserved among all three (stable sort).
    {
        BTHome::Packet<31> p;
        p.add(BTHome::temperature(1.0f)); // raw 100
        p.add(BTHome::temperature(2.0f)); // raw 200
        p.add(BTHome::temperature(3.0f)); // raw 300

        const uint8_t want_sd[] = {
            0xD2, 0xFC, 0x40,
            0x02, 0x64, 0x00,
            0x02, 0xC8, 0x00,
            0x02, 0x2C, 0x01,
        };
        expect_bytes("stable order for three equal ids", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }
}

static void test_exact_capacity_boundaries()
{
    // Tests: Adds that fill the packet to exactly its capacity.
    // Expects: The exactly-fitting add succeeds; one more byte is rejected and
    // the packet bytes stay untouched.
    {
        BTHome::Packet<10> p;                            // header 5 + up to 5 payload bytes
        expect_true("exact-fit first add", p.add(BTHome::temperature(20.0f))); // 3 bytes -> size 8
        expect_true("exact-fit second add", p.add(BTHome::motion(true)));      // 2 bytes -> size 10 == capacity
        expect_true("packet is exactly full", p.size() == 10);

        const bool overflow = p.add(BTHome::battery(50)); // 2 bytes -> would be 12
        expect_true("one-past-full add rejected", !overflow);

        const uint8_t want_ad[] = {
            0x09, 0x16, 0xD2, 0xFC, 0x40,
            0x02, 0xD0, 0x07,
            0x21, 0x01,
        };
        expect_bytes("full packet unchanged after rejected add", p.data(), p.size(), want_ad, sizeof(want_ad));
    }

    // Tests: Repeatedly adding until the packet reports overflow.
    // Expects: add() eventually returns false, the AD length byte always
    // matches the serialized size, and size() never exceeds capacity.
    {
        BTHome::Packet<31> p;
        int added = 0;
        while (p.add(BTHome::battery(static_cast<uint8_t>(added))) && added < 100)
        {
            ++added;
        }
        expect_true("fill loop terminates before 100 adds", added < 100);
        expect_true("filled size within capacity", p.size() <= 31);
        expect_true("filled AD length byte consistent", p.data()[0] == p.size() - 1);
        expect_true("fill count matches capacity math", added == (31 - 5) / 2); // 13 battery measurements
    }
}

static void test_flags_after_adds()
{
    // Tests: Changing device-info flags after measurements were added.
    // Expects: Only the device-info byte changes; measurement bytes intact.
    BTHome::Packet<31> p;
    p.add(BTHome::temperature(20.0f));
    p.add(BTHome::motion(true));

    p.setTriggerBased(true);
    const uint8_t want_trigger[] = {
        0xD2, 0xFC, 0x44,
        0x02, 0xD0, 0x07,
        0x21, 0x01,
    };
    expect_bytes("trigger flag set after adds", p.serviceData(), p.serviceDataSize(), want_trigger, sizeof(want_trigger));

    p.setEncrypted(true);
    const uint8_t want_both[] = {
        0xD2, 0xFC, 0x45,
        0x02, 0xD0, 0x07,
        0x21, 0x01,
    };
    expect_bytes("encrypted flag set after adds", p.serviceData(), p.serviceDataSize(), want_both, sizeof(want_both));

    p.setTriggerBased(false);
    p.setEncrypted(false);
    const uint8_t want_cleared[] = {
        0xD2, 0xFC, 0x40,
        0x02, 0xD0, 0x07,
        0x21, 0x01,
    };
    expect_bytes("flags cleared after adds", p.serviceData(), p.serviceDataSize(), want_cleared, sizeof(want_cleared));
}

static void test_accessor_idempotence()
{
    // Tests: Repeated accessor calls and a rejected add in between.
    // Expects: data()/size()/serviceData() are pure reads - identical results
    // on every call, also after a failed add.
    BTHome::Packet<8> p;
    p.add(BTHome::battery(50));

    uint8_t snapshot[8] = {};
    const size_t size_before = p.size();
    memcpy(snapshot, p.data(), size_before);

    (void) p.add(BTHome::humidity(40.0f)); // rejected (capacity)
    (void) p.data();
    (void) p.serviceData();

    expect_true("size unchanged by reads and failed add", p.size() == size_before);
    expect_bytes("bytes unchanged by reads and failed add", p.data(), p.size(), snapshot, size_before);
    expect_true("serviceData stays in sync", p.serviceData() == p.data() + 2 && p.serviceDataSize() == p.size() - 2);
}

static void test_text_and_raw()
{
    // Tests: Text (0x53) serialization with the bthome.io spec example.
    // Expects: [53][len][bytes] with the extra length byte, "Hello World!" = 12 chars.
    {
        BTHome::Packet<31> p;
        expect_true("text add ok", p.add(BTHome::text("Hello World!")));
        const uint8_t want_sd[] = {
            0xD2, 0xFC, 0x40,
            0x53, 0x0C, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64, 0x21,
        };
        expect_bytes("text spec example", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Raw (0x54) serialization.
    // Expects: [54][len][bytes] with the extra length byte.
    {
        BTHome::Packet<31> p;
        const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
        expect_true("raw add ok", p.add(BTHome::raw(payload, sizeof(payload))));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x54, 0x04, 0xDE, 0xAD, 0xBE, 0xEF};
        expect_bytes("raw entry", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Text sorts among regular measurements by object id.
    // Expects: battery (0x01) < temperature (0x02) < text (0x53) < conductivity (0x56).
    {
        BTHome::Packet<31> p;
        p.add(BTHome::text("hi"));
        p.add(BTHome::conductivity(3120.0f));
        p.add(BTHome::temperature(21.53f));
        p.add(BTHome::battery(87));
        const uint8_t want_sd[] = {
            0xD2, 0xFC, 0x40,
            0x01, 0x57,
            0x02, 0x69, 0x08,
            0x53, 0x02, 'h', 'i',
            0x56, 0x30, 0x0C,
        };
        expect_bytes("text sorted among measurements", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Factory truncation of over-long input.
    // Expects: Text clamped to VarMeasurement::kMaxBytes (24) characters.
    {
        const BTHome::VarMeasurement m =
            BTHome::text("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"); // 30 chars
        expect_true("text truncated to kMaxBytes", m.len == BTHome::VarMeasurement::kMaxBytes);
        expect_true("empty text has len 0", BTHome::text("").len == 0);
        expect_true("null text has len 0", BTHome::text(nullptr).len == 0);
    }

    // Tests: Capacity overflow with a text entry.
    // Expects: Too-long text rejected without modifying the packet; a shorter
    // one fits afterwards.
    {
        BTHome::Packet<12> p; // header 5 + 7 free bytes
        expect_true("oversized text rejected", !p.add(BTHome::text("toolong"))); // needs 2+7=9
        expect_true("packet untouched after rejection", p.size() == 5);

        expect_true("fitting text accepted", p.add(BTHome::text("abc"))); // needs 2+3=5
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x53, 0x03, 'a', 'b', 'c'};
        expect_bytes("small packet with text", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Text packet through the advertising builder.
    // Expects: Flags AD + service data with text, unchanged by the builder.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::battery(87));
        p.add(BTHome::text("ok"));

        uint8_t adv[31] = {};
        const int adv_size = BTHome::build_advertising(p, adv, sizeof(adv));
        const uint8_t want[] = {
            0x02, 0x01, 0x06,
            0x0A, 0x16, 0xD2, 0xFC, 0x40,
            0x01, 0x57,
            0x53, 0x02, 'o', 'k',
        };
        expect_true("adv with text build success", adv_size >= 0);
        expect_bytes("adv with text payload", adv, static_cast<size_t>(adv_size), want, sizeof(want));
    }
}

static void test_events()
{
    // Tests: The spec's multi-button example - None for button 1, press for
    // button 2, encoded as sequential 0x3A entries in insertion order.
    // Expects: Service data payload [3A 00 3A 01].
    {
        BTHome::Packet<31> p;
        p.add(BTHome::button_event(BTHome::ButtonEventType::None));
        p.add(BTHome::button_event(BTHome::ButtonEventType::Press));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x3A, 0x00, 0x3A, 0x01};
        expect_bytes("multi-button spec example 3A003A01", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: All three event kinds in one packet, added in reverse id order.
    // Expects: Canonical order button (0x3A) < command (0x3B) < dimmer (0x3C);
    // rotate and step events carry their argument bytes.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::dimmer_event(BTHome::DimmerEventType::RotateLeft, 3));
        p.add(BTHome::command_event(BTHome::CommandEventType::StepUp, 5));
        p.add(BTHome::button_event(BTHome::ButtonEventType::LongPress));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x3A, 0x04,
                                        0x3B, 0x01, 0x03, 0x05, 0x3C, 0x01, 0x03};
        expect_bytes("button + command + dimmer", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Multi-dimmer padding - None for dimmer 1, rotate for dimmer 2.
    // Expects: None carries its steps byte (3C 00 00), so receivers stay in
    // sync and can attribute the rotate event to instance 2.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::dimmer_event(BTHome::DimmerEventType::None));
        p.add(BTHome::dimmer_event(BTHome::DimmerEventType::RotateRight, 1));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x3C, 0x00, 0x00, 0x3C, 0x02, 0x01};
        expect_bytes("multi-dimmer None padding 3C0000", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: An event mixed with measurements and packet_id.
    // Expects: Canonical object-id order: packet_id (0x00) < temperature (0x02) < button (0x3A).
    {
        BTHome::Packet<31> p;
        p.add(BTHome::button_event(BTHome::ButtonEventType::Press));
        p.add(BTHome::temperature(20.0f));
        p.add(BTHome::packet_id(5));
        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x00, 0x05, 0x02, 0xD0, 0x07, 0x3A, 0x01};
        expect_bytes("event sorts after measurements", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }
}

static void test_non_finite_values()
{
    // Tests: NaN and the infinities are dropped, not encoded.
    // Expects: len 0 - BTHome has no representation for "no value", and a
    //          clamped extreme would look like a real reading to a receiver.
    {
        expect_true("NaN dropped (signed)", BTHome::temperature(NAN).len == 0);
        expect_true("+inf dropped", BTHome::temperature(INFINITY).len == 0);
        expect_true("-inf dropped", BTHome::temperature(-INFINITY).len == 0);
        expect_true("NaN dropped (unsigned)", BTHome::humidity(NAN).len == 0);
    }

    // Tests: finite values far outside the object range still clamp.
    // Expects: the documented extreme, and not the opposite one - converting an
    //          out-of-range float to int64_t is undefined, which used to turn
    //          huge positives into the minimum.
    {
        const BTHome::Measurement hi = BTHome::energy(1e30f);        // uint24 x0.001
        const BTHome::Measurement pos = BTHome::temperature(1e30f);  // sint16 x0.01
        const BTHome::Measurement neg = BTHome::temperature(-1e30f);
        expect_true("huge unsigned clamps to max",
                    hi.len == 3 && hi.data[0] == 0xFF && hi.data[1] == 0xFF && hi.data[2] == 0xFF);
        expect_true("huge positive clamps to max",
                    pos.len == 2 && pos.data[0] == 0xFF && pos.data[1] == 0x7F);
        expect_true("huge negative clamps to min",
                    neg.len == 2 && neg.data[0] == 0x00 && neg.data[1] == 0x80);
    }

    // Tests: Packet::add() refuses a zero-length Measurement.
    // Expects: false, and the packet unchanged - serializing a bare object id
    //          would make every receiver read the next object's bytes as this
    //          object's value and discard the whole advertisement.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::packet_id(1));
        const size_t before = p.serviceDataSize();

        BTHome::Measurement empty;
        empty.object_id = static_cast<uint8_t>(BTHome::ObjectId::Temperature);
        empty.len = 0;
        expect_true("zero-length measurement rejected", !p.add(empty));
        expect_true("zero-length measurement leaves packet untouched",
                    p.serviceDataSize() == before);
    }

    // Tests: a dropped NaN reading does not corrupt the objects around it.
    // Expects: exactly packet id + humidity, with no stray 0x02 byte.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::packet_id(1));
        expect_true("NaN reading not added", !p.add(BTHome::temperature(NAN)));
        p.add(BTHome::humidity(50.0f));

        const uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x00, 0x01, 0x03, 0x88, 0x13};
        expect_bytes("NaN reading leaves a clean packet",
                     p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }
}

int main()
{
    test_header_and_basic_packet();
    test_device_info_flags();
    test_ordering_and_ids();
    test_numeric_encoding_paths();
    test_device_object_paths();
    test_rounding_and_clamping();
    test_capacity_and_overflow_behavior();
    test_advertising_builder();
    test_insert_positions();
    test_exact_capacity_boundaries();
    test_flags_after_adds();
    test_accessor_idempotence();
    test_text_and_raw();
    test_events();
    test_non_finite_values();

    return test_summary();
}
