// Host-side correctness test. Build & run from project root:
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I .\src .\tests\test_bthome.cpp -o .\build\test_bthome.exe
//   .\build\test_bthome.exe
#include "bthome.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

static int g_failures = 0;

static void expect_true(const char *name, bool cond)
{
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond)
    {
        ++g_failures;
    }
}

static void expect_bytes(const char *name,
                         const std::uint8_t *got,
                         std::size_t got_len,
                         const std::uint8_t *want,
                         std::size_t want_len)
{
    const bool ok = (got_len == want_len) && (std::memcmp(got, want, want_len) == 0);
    std::printf("[%s] %s\n  got : ", ok ? "PASS" : "FAIL", name);
    for (std::size_t i = 0; i < got_len; ++i)
    {
        std::printf("%02X ", got[i]);
    }
    std::printf("\n  want: ");
    for (std::size_t i = 0; i < want_len; ++i)
    {
        std::printf("%02X ", want[i]);
    }
    std::printf("\n");

    if (!ok)
    {
        ++g_failures;
    }
}

static void test_header_and_basic_packet()
{
    // Tests: Empty packet without measurements.
    // Expects: Exact header AD block [04 16 D2 FC 40] and serviceDataSize = size-2.
    {
        BTHomePacket<31> p;
        const std::uint8_t want[] = {0x04, 0x16, 0xD2, 0xFC, 0x40};
        expect_bytes("header-only packet", p.data(), p.size(), want, sizeof(want));
        expect_true("serviceDataSize == size - 2", p.serviceDataSize() == (p.size() - 2));
    }

    // Tests: Mixed measurements inserted in intentionally unsorted order.
    // Expects: Serialization sorted by object_id and exact match with known byte vector.
    {
        BTHomePacket<31> p;
        const bool a = p.add(BTHome::humidity(54.2f));
        const bool b = p.add(BTHome::temperature(21.53f));
        const bool c = p.add(BTHome::battery(87));

        const std::uint8_t want_ad[] = {
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

        const std::uint8_t want_sd[] = {
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
    BTHomePacket<31> p;
    p.setTriggerBased(true);
    p.setEncrypted(true);
    expect_true("device-info encrypted+trigger bitmask", p.serviceData()[2] == 0x45);
}

static void test_ordering_and_ids()
{
    // Tests: Two measurements with identical object_id (temperature).
    // Expects: Stable ordering preserved in insertion order (18.2 before 21.5).
    {
        BTHomePacket<31> p;
        p.add(BTHome::temperature(18.2f));
        p.add(BTHome::temperature(21.5f));

        const std::uint8_t want_sd[] = {
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
        BTHomePacket<31> p;
        p.add(BTHome::temperature(20.0f));
        p.add(BTHome::packet_id(7));

        const std::uint8_t want_sd[] = {
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
        BTHomePacket<31> p;
        p.add(BTHome::motion(true));
        p.add(BTHome::window(false));
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x21, 0x01, 0x2D, 0x00};
        expect_bytes("binary ids motion/window", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    {
        // Tests: Mapping of binary IDs heat/lock according to the BTHome table.
        // Expects: heat=0x1D and lock=0x1F with correct bool payload bytes.
        BTHomePacket<31> p;
        p.add(BTHome::heat(true));
        p.add(BTHome::lock(false));
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x1D, 0x01, 0x1F, 0x00};
        expect_bytes("binary ids heat/lock", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    {
        // Tests: Boundary area of adjacent IDs 0x2D (window) and 0x2E (humidity_u8).
        // Expects: No ID mix-up, exactly [2D 00 2E 23] in the payload section.
        BTHomePacket<31> p;
        p.add(BTHome::window(false));
        p.add(BTHome::humidity_u8(35));
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x2D, 0x00, 0x2E, 0x23};
        expect_bytes("window (0x2D) vs humidity_u8 (0x2E)", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }
}

static void test_numeric_encoding_paths()
{
    // Tests: Signed temperature encoding with a negative value.
    // Expects: Two's-complement little-endian, -12.34 -> [2E FB].
    {
        BTHomePacket<31> p;
        p.add(BTHome::temperature(-12.34f));
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x02, 0x2E, 0xFB};
        expect_bytes("negative temperature encoding", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Unsigned 24-bit path for pressure.
    // Expects: 1013.25 hPa -> raw 101325 -> [CD 8B 01].
    {
        BTHomePacket<31> p;
        p.add(BTHome::pressure(1013.25f));
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x04, 0xCD, 0x8B, 0x01};
        expect_bytes("pressure uint24 encoding", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Unsigned 32-bit scaling path for energy_u32.
    // Expects: 12.345 -> raw 12345 -> [39 30 00 00].
    {
        BTHomePacket<31> p;
        p.add(BTHome::energy_u32(12.345f));
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x4D, 0x39, 0x30, 0x00, 0x00};
        expect_bytes("energy_u32 scaled", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Signed integer wrapper count_s16.
    // Expects: -22 encoded as int16 little-endian [EA FF].
    {
        BTHomePacket<31> p;
        p.add(BTHome::count_s16(-22));
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x5A, 0xEA, 0xFF};
        expect_bytes("count_s16 signed wrapper", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Raw u32 path for timestamp without scaling.
    // Expects: epoch 1684093277 -> [5D 39 61 64] little-endian.
    {
        BTHomePacket<31> p;
        p.add(BTHome::timestamp(1684093277u));
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x50, 0x5D, 0x39, 0x61, 0x64};
        expect_bytes("timestamp uint32", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }
}

static void test_device_object_paths()
{
    // Tests: Device type id object 0xF0 with uint16 payload.
    // Expects: F0 + little-endian 0x0001 -> [F0 01 00].
    {
        BTHomePacket<31> p;
        p.add(BTHome::device_type_id(1));
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0xF0, 0x01, 0x00};
        expect_bytes("device type id u16", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Firmware version object 0xF1 with uint32 payload.
    // Expects: 4.2.1.0 packed as 0x04020100 -> [F1 00 01 02 04].
    {
        BTHomePacket<31> p;
        p.add(BTHome::firmware_version_u32(0x04020100u));
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0xF1, 0x00, 0x01, 0x02, 0x04};
        expect_bytes("firmware version u32", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // Tests: Firmware version object 0xF2 with uint24 payload.
    // Expects: 6.1.0 packed as 0x060100 -> [F2 00 01 06].
    {
        BTHomePacket<31> p;
        p.add(BTHome::firmware_version_u24(0x060100u));
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0xF2, 0x00, 0x01, 0x06};
        expect_bytes("firmware version u24", p.serviceData(), p.serviceDataSize(), want_sd, sizeof(want_sd));
    }
}

static void test_rounding_and_clamping()
{
    // Tests: "Half away from zero" rounding for signed scaled values.
    // Expects: +1.235 -> +124 ([7C 00]) and -1.235 -> -124 ([84 FF]).
    {
        BTHomePacket<31> p;
        p.add(BTHome::temperature(1.235f)); // 123.5 -> 124 -> 0x007C
        p.add(BTHome::dewpoint(-1.235f));   // -123.5 -> -124 -> 0xFF84

        const std::uint8_t want_sd[] = {
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
        BTHomePacket<31> p;
        p.add(BTHome::temperature(500.0f));
        p.add(BTHome::dewpoint(-500.0f));

        const std::uint8_t want_sd[] = {
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
        BTHomePacket<31> p;
        p.add(BTHome::humidity(-3.0f));     // unsigned 2-byte scaled -> clamp to 0
        p.add(BTHome::pressure(200000.0f)); // unsigned 3-byte scaled -> clamp to 0xFFFFFF

        const std::uint8_t want_sd[] = {
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
        BTHomePacket<8> p;
        const bool first_ok = p.add(BTHome::battery(50));
        const bool second_ok = p.add(BTHome::humidity(40.0f));

        expect_true("tiny packet first add ok", first_ok);
        expect_true("tiny packet second add rejected", !second_ok);

        const std::uint8_t want_ad[] = {0x06, 0x16, 0xD2, 0xFC, 0x40, 0x01, 0x32};
        expect_bytes("tiny packet remains valid after rejection", p.data(), p.size(), want_ad, sizeof(want_ad));
    }
}

static void test_advertising_builder()
{
    // Tests: Basic advertising build without local name.
    // Expects: Flags AD + BTHome service-data AD in exactly this order.
    {
        BTHomePacket<31> p;
        p.add(BTHome::battery(87));

        std::uint8_t adv[31] = {};
        const int adv_size = build_bthome_advertising(p, adv, sizeof(adv));
        expect_true("adv basic build success", adv_size >= 0);

        const std::uint8_t want[] = {
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
        expect_bytes("adv basic payload", adv, static_cast<std::size_t>(adv_size), want, sizeof(want));
    }

    // Tests: Advertising with complete local name (AD type 0x09).
    // Expects: Correctly appends [len 0x09 'node'] after the BTHome block.
    {
        BTHomePacket<31> p;
        p.add(BTHome::battery(87));

        std::uint8_t adv[31] = {};
        const int adv_size = build_bthome_advertising(p, adv, sizeof(adv), "node", true);
        expect_true("adv with complete local name", adv_size >= 0);

        const std::uint8_t want[] = {
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
        expect_bytes("adv local name complete", adv, static_cast<std::size_t>(adv_size), want, sizeof(want));
    }

    // Tests: Advertising with shortened local name (AD type 0x08).
    // Expects: Correctly appends [len 0x08 'nd'] after the BTHome block.
    {
        BTHomePacket<31> p;
        p.add(BTHome::battery(87));

        std::uint8_t adv[31] = {};
        const int adv_size = build_bthome_advertising(p, adv, sizeof(adv), "nd", false);
        expect_true("adv with shortened local name", adv_size >= 0);

        const std::uint8_t want[] = {
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
        expect_bytes("adv local name shortened", adv, static_cast<std::size_t>(adv_size), want, sizeof(want));
    }

    // Tests: Error paths of the advertising builder.
    // Expects: Returns -1 for too-small output buffer, nullptr output, and too-long name (>254).
    {
        BTHomePacket<31> p;
        p.add(BTHome::battery(87));

        std::uint8_t adv_small[9] = {};
        expect_true("adv fails on too-small output buffer",
                    build_bthome_advertising(p, adv_small, sizeof(adv_small)) == -1);

        expect_true("adv fails on null output pointer",
                    build_bthome_advertising(p, nullptr, 31) == -1);

        char long_name[256] = {};
        for (std::size_t i = 0; i < 255; ++i)
        {
            long_name[i] = 'A';
        }
        long_name[255] = '\0';
        std::uint8_t adv[300] = {};
        expect_true("adv rejects local name > 254 bytes",
                    build_bthome_advertising(p, adv, sizeof(adv), long_name, true) == -1);
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

    std::printf("\n%s\n", g_failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return g_failures == 0 ? 0 : 1;
}
