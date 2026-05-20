// Host-side correctness test. Build & run:
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I ../src
//       test_bthome.cpp -o test && ./test
#include "bthome.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

static int g_failures = 0;

static void expect_bytes(const char* name, const std::uint8_t* got,
                         std::size_t got_len, const std::uint8_t* want,
                         std::size_t want_len) {
    bool ok = (got_len == want_len) &&
              (std::memcmp(got, want, want_len) == 0);
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

static void expect_true(const char* name, bool cond) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond)
    {
        ++g_failures;
    }
}

int main() {
    // 1. The README/success-metric example. Added out of canonical order to
    //    prove the library reorders to ascending object id.
    {
        BTHomePacket<31> p;
        bool a = p.add(BTHome::humidity(54.2f));
        bool b = p.add(BTHome::temperature(21.53f));
        bool c = p.add(BTHome::battery(87));
        // temp 21.53/0.01=2153=0x0869 -> 69 08 ; hum 54.2/0.01=5420=0x152C -> 2C 15
        // ordering: battery(01) temp(02) hum(03)
        const std::uint8_t want[] = {
            0x0C, 0x16, 0xD2, 0xFC, 0x40,
            0x01, 0x57,
            0x02, 0x69, 0x08,
            0x03, 0x2C, 0x15,
        };
        expect_bytes("full AD element, reordered", p.data(), p.size(),
                     want, sizeof(want));
        const std::uint8_t want_sd[] = {
            0xD2, 0xFC, 0x40,
            0x01, 0x57, 0x02, 0x69, 0x08, 0x03, 0x2C, 0x15,
        };
        expect_bytes("serviceData() value", p.serviceData(),
                     p.serviceDataSize(), want_sd, sizeof(want_sd));
        expect_true("all adds fit", a && b && c);
    }

    // 2. Empty packet is still a valid (header-only) BTHome element.
    {
        BTHomePacket<31> p;
        const std::uint8_t want[] = {0x04, 0x16, 0xD2, 0xFC, 0x40};
        expect_bytes("empty packet header", p.data(), p.size(), want,
                     sizeof(want));
    }

    // 3. Flags: trigger-based + encrypted set the device-info bits.
    {
        BTHomePacket<31> p;
        p.setTriggerBased(true);
        p.setEncrypted(true);
        expect_true("device-info 0x45", p.serviceData()[2] == 0x45);
    }

    // 4. Signed/negative temperature encodes two's complement little-endian.
    {
        BTHomePacket<31> p;
        p.add(BTHome::temperature(-12.34f));  // -1234 = 0xFB2E -> 2E FB
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x02, 0x2E, 0xFB};
        expect_bytes("negative temperature", p.serviceData(),
                     p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // 5. uint24 pressure.
    {
        BTHomePacket<31> p;
        p.add(BTHome::pressure(1013.25f));  // /0.01 = 101325 = 0x018BCD -> CD 8B 01
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x04, 0xCD, 0x8B, 0x01};
        expect_bytes("uint24 pressure", p.serviceData(), p.serviceDataSize(),
                     want_sd, sizeof(want_sd));
    }

    // 6. Multiple same-type entries keep insertion order for equal object-id.
    {
        BTHomePacket<31> p;
        p.add(BTHome::temperature(18.2f));
        p.add(BTHome::temperature(21.5f));
        // insertion order for equal object id is preserved (upper_bound)
        // 18.2 -> 1820 = 0x071C -> 1C 07 (first)
        // 21.5 -> 2150 = 0x0866 -> 66 08 (second)
        const std::uint8_t want_sd[] = {
            0xD2, 0xFC, 0x40,
            0x02, 0x1C, 0x07,
            0x02, 0x66, 0x08,
        };
        expect_bytes("two temperatures same object id", p.serviceData(),
                     p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // 7. Binary sensors encode a single 0/1 byte and must use spec IDs.
    {
        BTHomePacket<31> p;
        p.add(BTHome::motion(true));
        p.add(BTHome::window(false));
        // BTHome v2 spec: motion=0x21, window=0x2D
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x21, 0x01, 0x2D, 0x00};
        expect_bytes("binary sensors", p.serviceData(), p.serviceDataSize(),
                     want_sd, sizeof(want_sd));
    }

    // 8. Additional binary ID conformance checks against BTHome v2 table.
    {
        BTHomePacket<31> p;
        p.add(BTHome::heat(true));
        p.add(BTHome::lock(false));
        // BTHome v2 spec: heat=0x1D, lock=0x1F
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x1D, 0x01, 0x1F, 0x00};
        expect_bytes("binary heat/lock IDs", p.serviceData(), p.serviceDataSize(),
                     want_sd, sizeof(want_sd));
    }

    // 9. Distinct IDs near 0x2D/0x2E boundary (window vs humidity_u8).
    {
        BTHomePacket<31> p;
        p.add(BTHome::window(false));
        p.add(BTHome::humidity_u8(35));
        // BTHome v2 spec: window=0x2D, humidity_u8=0x2E
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x2D, 0x00, 0x2E, 0x23};
        expect_bytes("window vs humidity_u8 IDs", p.serviceData(),
                     p.serviceDataSize(), want_sd, sizeof(want_sd));
    }

    // 10. Overflow is reported, not crashed. Tiny capacity holds header + one
    //    2-byte measurement (battery) but not a second.
    {
        BTHomePacket<8> p;  // 5 header + room for one id+1 value
        bool a = p.add(BTHome::battery(50));
        bool b = p.add(BTHome::humidity(40.0f));  // would need 3 bytes -> drop
        expect_true("first add fit", a);
        expect_true("second add failed", !b);
    }

    // 11. New fixed-width uint32 scaled sensor (energy_u32).
    {
        BTHomePacket<31> p;
        p.add(BTHome::energy_u32(12.345f));  // /0.001 => 12345 => 0x00003039 => 39 30 00 00
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x4D, 0x39, 0x30, 0x00, 0x00};
        expect_bytes("energy_u32 scaled", p.serviceData(), p.serviceDataSize(),
                     want_sd, sizeof(want_sd));
    }

    // 12. New signed integer wrapper path (count_s16).
    {
        BTHomePacket<31> p;
        p.add(BTHome::count_s16(-22));  // -22 => 0xFFEA => EA FF
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x5A, 0xEA, 0xFF};
        expect_bytes("count_s16 signed", p.serviceData(), p.serviceDataSize(),
                     want_sd, sizeof(want_sd));
    }

    // 13. Timestamp uses raw uint32 epoch seconds.
    {
        BTHomePacket<31> p;
        p.add(BTHome::timestamp(1684093277u));  // 0x6461395D => 5D 39 61 64
        const std::uint8_t want_sd[] = {0xD2, 0xFC, 0x40, 0x50, 0x5D, 0x39, 0x61, 0x64};
        expect_bytes("timestamp uint32", p.serviceData(), p.serviceDataSize(),
                     want_sd, sizeof(want_sd));
    }

    // 14. BTHome-specific raw advertising payload:
    //     Flags + BTHome Service Data.
    {
        BTHomePacket<31> p;
        p.add(BTHome::battery(87));

        std::uint8_t adv[31] = {};
        int adv_size = build_bthome_advertising(p, adv, sizeof(adv));
        expect_true("adv build flags + bthome", adv_size >= 0);

        const std::uint8_t want[] = {
            0x02, 0x01, 0x06,
            0x06, 0x16, 0xD2, 0xFC, 0x40, 0x01, 0x57,
        };
        expect_bytes("bthome raw advertising payload",
                     adv, static_cast<std::size_t>(adv_size), want, sizeof(want));
    }

    std::printf("\n%s\n", g_failures == 0 ? "ALL TESTS PASSED"
                                          : "SOME TESTS FAILED");
    return g_failures == 0 ? 0 : 1;
}
