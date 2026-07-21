// Host-side encryption test against the official BTHome v2 spec vector
// (https://bthome.io/encryption/). Requires mbedtls. Build & run from project root:
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I src tests/test_encryption.cpp -lmbedcrypto -o build/test_encryption
//   ./build/test_encryption
#include "bthome.h"
#include "bthome_crypto_mbedtls.h"

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

// Official spec vector from https://bthome.io/encryption/
static const std::uint8_t kSpecKey[BTHome::Encryptor::kKeyBytes] = {
    0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1,
    0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32};
static const std::uint8_t kSpecMac[BTHome::Encryptor::kMacBytes] = {
    0x54, 0x48, 0xE6, 0x8F, 0x80, 0xA5};
static constexpr std::uint32_t kSpecCounter = 0x00112233u;

static void test_spec_vector()
{
    // Tests: The complete worked example from the BTHome encryption spec page:
    // temperature 25.06 C + humidity 50.55 % with the documented key/MAC/counter.
    // Expects: Exact ciphertext, counter and MIC bytes as published.
    BTHome::EncryptedPacket<28> packet;
    expect_true("spec: add temperature", packet.add(BTHome::temperature(25.06f)));
    expect_true("spec: add humidity", packet.add(BTHome::humidity(50.55f)));

    // Inner plaintext AD element before encryption: header (0x41!) + 02 CA 09 03 BF 13.
    const std::uint8_t want_inner[] = {0x0A, 0x16, 0xD2, 0xFC, 0x41,
                                       0x02, 0xCA, 0x09, 0x03, 0xBF, 0x13};
    expect_bytes("spec: inner plaintext element", packet.inner().data(), packet.inner().size(),
                 want_inner, sizeof(want_inner));
    expect_true("spec: size() includes overhead",
                packet.size() == packet.inner().size() + BTHome::Encryptor::kOverheadBytes);

    BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
    encryptor.setKey(kSpecKey);
    encryptor.setMac(kSpecMac);
    encryptor.setCounter(kSpecCounter);

    std::uint8_t out[31] = {};
    const int size = BTHome::build_encrypted_advertising(packet, encryptor, out, sizeof(out));
    const std::uint8_t want_adv[] = {
        0x02, 0x01, 0x06,                                // Flags AD
        0x12, 0x16, 0xD2, 0xFC, 0x41,                    // Service data AD header
        0xE4, 0x45, 0xF3, 0xC9, 0x96, 0x2B,              // Ciphertext
        0x33, 0x22, 0x11, 0x00,                          // Counter (little-endian)
        0x6C, 0x7C, 0x45, 0x19};                         // MIC
    expect_true("spec: build succeeded", size == static_cast<int>(sizeof(want_adv)));
    expect_bytes("spec: full advertisement", out, static_cast<std::size_t>(size > 0 ? size : 0),
                 want_adv, sizeof(want_adv));
    expect_true("spec: counter consumed once", encryptor.counter() == kSpecCounter + 1);
}

static void test_counter_makes_ciphertext_unique()
{
    // Tests: Building the same packet twice with one Encryptor.
    // Expects: Different ciphertext/MIC bytes (fresh nonce per build) and counter advanced by 2.
    BTHome::EncryptedPacket<28> packet;
    packet.add(BTHome::temperature(25.06f));

    BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
    encryptor.setKey(kSpecKey);
    encryptor.setMac(kSpecMac);
    encryptor.setCounter(1000);

    std::uint8_t out_a[31] = {};
    std::uint8_t out_b[31] = {};
    const int size_a = BTHome::build_encrypted_advertising(packet, encryptor, out_a, sizeof(out_a));
    const int size_b = BTHome::build_encrypted_advertising(packet, encryptor, out_b, sizeof(out_b));
    expect_true("unique: both builds succeeded", size_a > 0 && size_a == size_b);
    expect_true("unique: ciphertext differs between builds",
                std::memcmp(out_a, out_b, static_cast<std::size_t>(size_a)) != 0);
    expect_true("unique: counter advanced twice", encryptor.counter() == 1002);
}

static void test_trigger_flag_in_nonce()
{
    // Tests: A trigger-based encrypted packet (device info 0x45) decrypted with a
    // manually built nonce containing 0x45.
    // Expects: mbedtls auth-decrypt succeeds and yields the original plaintext,
    // proving the nonce uses the device-info byte exactly as transmitted.
    BTHome::EncryptedPacket<28> packet;
    packet.setTriggerBased(true);
    packet.add(BTHome::motion(true));

    constexpr std::uint32_t kCounter = 42;
    BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
    encryptor.setKey(kSpecKey);
    encryptor.setMac(kSpecMac);
    encryptor.setCounter(kCounter);

    std::uint8_t out[31] = {};
    const int size = BTHome::build_encrypted_advertising(packet, encryptor, out, sizeof(out));
    expect_true("trigger: build succeeded", size > 0);
    expect_true("trigger: device info is 0x45", out[7] == 0x45);

    // Reassemble the nonce the way a receiver would.
    std::uint8_t nonce[BTHome::Encryptor::kNonceBytes];
    std::memcpy(nonce, kSpecMac, 6);
    nonce[6] = 0xD2;
    nonce[7] = 0xFC;
    nonce[8] = 0x45;
    nonce[9] = static_cast<std::uint8_t>(kCounter & 0xFFu);
    nonce[10] = static_cast<std::uint8_t>((kCounter >> 8) & 0xFFu);
    nonce[11] = static_cast<std::uint8_t>((kCounter >> 16) & 0xFFu);
    nonce[12] = static_cast<std::uint8_t>((kCounter >> 24) & 0xFFu);

    const std::size_t cipher_len = 2; // motion(true) = [0x21][0x01]
    const std::uint8_t *cipher = out + 8;
    const std::uint8_t *mic = out + 8 + cipher_len + 4;

    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    std::uint8_t plain[2] = {};
    const bool decrypted =
        mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES, kSpecKey, 128) == 0 &&
        mbedtls_ccm_auth_decrypt(&ccm, cipher_len, nonce, sizeof(nonce), nullptr, 0,
                                 cipher, plain, mic, 4) == 0;
    mbedtls_ccm_free(&ccm);

    const std::uint8_t want_plain[] = {0x21, 0x01};
    expect_true("trigger: receiver-side auth decrypt succeeded", decrypted);
    expect_bytes("trigger: decrypted plaintext", plain, sizeof(plain), want_plain, sizeof(want_plain));
}

static void test_capacity_accounting()
{
    // Tests: EncryptedPacket reserves the 8 overhead bytes inside AdvCapacity.
    // Expects: Fill logic driven by size() can never overflow a 28-byte AD element;
    // the built advertisement fits exactly into 31 bytes.
    BTHome::EncryptedPacket<28> packet;
    std::size_t added = 0;
    while (packet.add(BTHome::battery(static_cast<std::uint8_t>(added))))
    {
        ++added;
    }
    // Inner capacity 28 - 8 = 20: header (5) + 7 battery entries (2 bytes each) = 19; the 8th fails.
    expect_true("capacity: 7 battery entries fit", added == 7);
    expect_true("capacity: size() within AD budget", packet.size() <= 28);

    BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
    encryptor.setKey(kSpecKey);
    encryptor.setMac(kSpecMac);

    std::uint8_t out[31] = {};
    const int size = BTHome::build_encrypted_advertising(packet, encryptor, out, sizeof(out));
    expect_true("capacity: full packet builds into 31 bytes", size > 0 && size <= 31);
}

static bool failing_backend(const std::uint8_t *, const std::uint8_t *,
                            const std::uint8_t *, std::size_t, std::uint8_t *, std::uint8_t *)
{
    return false;
}

static void test_error_paths()
{
    // Tests: Error conditions of build_encrypted_advertising.
    // Expects: -1 without consuming a counter value in every case.
    BTHome::EncryptedPacket<28> packet;
    packet.add(BTHome::battery(50));

    BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
    encryptor.setKey(kSpecKey);
    encryptor.setMac(kSpecMac);
    encryptor.setCounter(7);

    std::uint8_t out[31] = {};
    expect_true("error: null output buffer",
                BTHome::build_encrypted_advertising(packet, encryptor, nullptr, 31) == -1);
    expect_true("error: output buffer too small",
                BTHome::build_encrypted_advertising(packet, encryptor, out, 10) == -1);
    expect_true("error: counter untouched after failures", encryptor.counter() == 7);

    BTHome::Encryptor no_backend(nullptr);
    no_backend.setCounter(7);
    expect_true("error: missing backend",
                BTHome::build_encrypted_advertising(packet, no_backend, out, sizeof(out)) == -1);
    expect_true("error: counter untouched without backend", no_backend.counter() == 7);

    BTHome::Encryptor failing(&failing_backend);
    failing.setCounter(7);
    expect_true("error: failing backend",
                BTHome::build_encrypted_advertising(packet, failing, out, sizeof(out)) == -1);
    expect_true("error: counter untouched after backend failure", failing.counter() == 7);
}

int main()
{
    test_spec_vector();
    test_counter_makes_ciphertext_unique();
    test_trigger_flag_in_nonce();
    test_capacity_accounting();
    test_error_paths();

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
