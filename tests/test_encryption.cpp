// Host-side encryption test against the official BTHome v2 spec vector
// (https://bthome.io/encryption/). Requires mbedtls. Build & run from project root:
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I src tests/test_encryption.cpp -lmbedcrypto -o build/test_encryption
//   ./build/test_encryption
#include "bthome.h"
#include "bthome_crypto_mbedtls.h"
#include "bthome_crypto_psa.h"
#include "test_utils.h"

// Official spec vector from https://bthome.io/encryption/
static const uint8_t kSpecKey[BTHome::Encryptor::kKeyBytes] = {
    0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1,
    0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32};
static const uint8_t kSpecMac[BTHome::Encryptor::kMacBytes] = {
    0x54, 0x48, 0xE6, 0x8F, 0x80, 0xA5};
static constexpr uint32_t kSpecCounter = 0x00112233u;

static void test_spec_vector()
{
    // Tests: The complete worked example from the BTHome encryption spec page:
    // temperature 25.06 C + humidity 50.55 % with the documented key/MAC/counter.
    // Expects: Exact ciphertext, counter and MIC bytes as published.
    BTHome::EncryptedPacket<28> packet;
    expect_true("spec: add temperature", packet.add(BTHome::temperature(25.06f)));
    expect_true("spec: add humidity", packet.add(BTHome::humidity(50.55f)));

    // Inner plaintext AD element before encryption: header (0x41!) + 02 CA 09 03 BF 13.
    const uint8_t want_inner[] = {0x0A, 0x16, 0xD2, 0xFC, 0x41,
                                       0x02, 0xCA, 0x09, 0x03, 0xBF, 0x13};
    expect_bytes("spec: inner plaintext element", packet.inner().data(), packet.inner().size(),
                 want_inner, sizeof(want_inner));
    expect_true("spec: size() includes overhead",
                packet.size() == packet.inner().size() + BTHome::Encryptor::kOverheadBytes);

    BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
    encryptor.setKey(kSpecKey);
    encryptor.setMac(kSpecMac);
    encryptor.setCounter(kSpecCounter);

    uint8_t out[31] = {};
    const int size = BTHome::build_encrypted_advertising(packet, encryptor, out, sizeof(out));
    const uint8_t want_adv[] = {
        0x02, 0x01, 0x06,                                // Flags AD
        0x12, 0x16, 0xD2, 0xFC, 0x41,                    // Service data AD header
        0xE4, 0x45, 0xF3, 0xC9, 0x96, 0x2B,              // Ciphertext
        0x33, 0x22, 0x11, 0x00,                          // Counter (little-endian)
        0x6C, 0x7C, 0x45, 0x19};                         // MIC
    expect_true("spec: build succeeded", size == static_cast<int>(sizeof(want_adv)));
    expect_bytes("spec: full advertisement", out, static_cast<size_t>(size > 0 ? size : 0),
                 want_adv, sizeof(want_adv));
    expect_true("spec: counter consumed once", encryptor.counter() == kSpecCounter + 1);
}

static void test_spec_vector_psa_backend()
{
    // Tests: The identical spec vector through the PSA Crypto adapter.
    // Expects: Byte-identical output to the mbedtls adapter - both backends
    // must be interchangeable.
    BTHome::EncryptedPacket<28> packet;
    packet.add(BTHome::temperature(25.06f));
    packet.add(BTHome::humidity(50.55f));

    BTHome::Encryptor encryptor(&BTHome::psa_ccm_backend);
    encryptor.setKey(kSpecKey);
    encryptor.setMac(kSpecMac);
    encryptor.setCounter(kSpecCounter);

    uint8_t out[31] = {};
    const int size = BTHome::build_encrypted_advertising(packet, encryptor, out, sizeof(out));
    const uint8_t want_adv[] = {
        0x02, 0x01, 0x06,
        0x12, 0x16, 0xD2, 0xFC, 0x41,
        0xE4, 0x45, 0xF3, 0xC9, 0x96, 0x2B,
        0x33, 0x22, 0x11, 0x00,
        0x6C, 0x7C, 0x45, 0x19};
    expect_true("psa: build succeeded", size == static_cast<int>(sizeof(want_adv)));
    expect_bytes("psa: full advertisement", out, static_cast<size_t>(size > 0 ? size : 0),
                 want_adv, sizeof(want_adv));
    expect_true("psa: counter consumed once", encryptor.counter() == kSpecCounter + 1);
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

    uint8_t out_a[31] = {};
    uint8_t out_b[31] = {};
    const int size_a = BTHome::build_encrypted_advertising(packet, encryptor, out_a, sizeof(out_a));
    const int size_b = BTHome::build_encrypted_advertising(packet, encryptor, out_b, sizeof(out_b));
    expect_true("unique: both builds succeeded", size_a > 0 && size_a == size_b);
    expect_true("unique: ciphertext differs between builds",
                memcmp(out_a, out_b, static_cast<size_t>(size_a)) != 0);
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

    constexpr uint32_t kCounter = 42;
    BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
    encryptor.setKey(kSpecKey);
    encryptor.setMac(kSpecMac);
    encryptor.setCounter(kCounter);

    uint8_t out[31] = {};
    const int size = BTHome::build_encrypted_advertising(packet, encryptor, out, sizeof(out));
    expect_true("trigger: build succeeded", size > 0);
    expect_true("trigger: device info is 0x45", out[7] == 0x45);

    // Reassemble the nonce the way a receiver would.
    uint8_t nonce[BTHome::Encryptor::kNonceBytes];
    memcpy(nonce, kSpecMac, 6);
    nonce[6] = 0xD2;
    nonce[7] = 0xFC;
    nonce[8] = 0x45;
    nonce[9] = static_cast<uint8_t>(kCounter & 0xFFu);
    nonce[10] = static_cast<uint8_t>((kCounter >> 8) & 0xFFu);
    nonce[11] = static_cast<uint8_t>((kCounter >> 16) & 0xFFu);
    nonce[12] = static_cast<uint8_t>((kCounter >> 24) & 0xFFu);

    const size_t cipher_len = 2; // motion(true) = [0x21][0x01]
    const uint8_t *cipher = out + 8;
    const uint8_t *mic = out + 8 + cipher_len + 4;

    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    uint8_t plain[2] = {};
    const bool decrypted =
        mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES, kSpecKey, 128) == 0 &&
        mbedtls_ccm_auth_decrypt(&ccm, cipher_len, nonce, sizeof(nonce), nullptr, 0,
                                 cipher, plain, mic, 4) == 0;
    mbedtls_ccm_free(&ccm);

    const uint8_t want_plain[] = {0x21, 0x01};
    expect_true("trigger: receiver-side auth decrypt succeeded", decrypted);
    expect_bytes("trigger: decrypted plaintext", plain, sizeof(plain), want_plain, sizeof(want_plain));
}

static void test_capacity_accounting()
{
    // Tests: EncryptedPacket reserves the 8 overhead bytes inside AdvCapacity.
    // Expects: Fill logic driven by size() can never overflow a 28-byte AD element;
    // the built advertisement fits exactly into 31 bytes.
    BTHome::EncryptedPacket<28> packet;
    size_t added = 0;
    while (packet.add(BTHome::battery(static_cast<uint8_t>(added))))
    {
        ++added;
    }
    // Inner capacity 28 - 8 = 20: header (5) + 7 battery entries (2 bytes each) = 19; the 8th fails.
    expect_true("capacity: 7 battery entries fit", added == 7);
    expect_true("capacity: size() within AD budget", packet.size() <= 28);

    BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
    encryptor.setKey(kSpecKey);
    encryptor.setMac(kSpecMac);

    uint8_t out[31] = {};
    const int size = BTHome::build_encrypted_advertising(packet, encryptor, out, sizeof(out));
    expect_true("capacity: full packet builds into 31 bytes", size > 0 && size <= 31);
}

static bool failing_backend(const uint8_t *, const uint8_t *,
                            const uint8_t *, size_t, uint8_t *, uint8_t *)
{
    return false;
}

static void test_service_data_spec_vector()
{
    // Tests: build_encrypted_service_data against the spec vector - the same
    //        bytes as the advertisement, without the AD wrapper.
    // Expects: [uuid][device-info][ciphertext][counter][MIC] and nothing else,
    //          which is exactly what Decryptor::decryptServiceData() consumes.
    BTHome::EncryptedPacket<28> packet;
    packet.add(BTHome::temperature(25.06f));
    packet.add(BTHome::humidity(50.55f));

    BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
    encryptor.setKey(kSpecKey);
    encryptor.setMac(kSpecMac);
    encryptor.setCounter(kSpecCounter);

    uint8_t out[26] = {};
    const int size = BTHome::build_encrypted_service_data(packet, encryptor, out, sizeof(out));
    const uint8_t want[] = {
        0xD2, 0xFC, 0x41,                    // UUID + device-info, encrypted bit set
        0xE4, 0x45, 0xF3, 0xC9, 0x96, 0x2B,  // Ciphertext
        0x33, 0x22, 0x11, 0x00,              // Counter (little-endian)
        0x6C, 0x7C, 0x45, 0x19};             // MIC
    expect_true("sd: build succeeded", size == static_cast<int>(sizeof(want)));
    expect_bytes("sd: service data", out, static_cast<size_t>(size > 0 ? size : 0),
                 want, sizeof(want));
    expect_true("sd: counter consumed once", encryptor.counter() == kSpecCounter + 1);
}

static void test_service_data_is_the_advertising_tail()
{
    // Tests: The two builders agree - an advertisement is the Flags AD and the
    //        AD length/type pair, then precisely this service data.
    // Expects: Byte equality, so neither can drift from the other unnoticed.
    //          build_encrypted_advertising delegates, which is what makes the
    //          nonce and the counter come from one place only.
    BTHome::EncryptedPacket<28> packet;
    packet.add(BTHome::temperature(25.06f));
    packet.add(BTHome::humidity(50.55f));

    BTHome::Encryptor for_adv(&BTHome::mbedtls_ccm_backend);
    for_adv.setKey(kSpecKey);
    for_adv.setMac(kSpecMac);
    for_adv.setCounter(kSpecCounter);
    BTHome::Encryptor for_sd(&BTHome::mbedtls_ccm_backend);
    for_sd.setKey(kSpecKey);
    for_sd.setMac(kSpecMac);
    for_sd.setCounter(kSpecCounter);

    uint8_t adv[31] = {};
    uint8_t sd[26] = {};
    const int adv_size = BTHome::build_encrypted_advertising(packet, for_adv, adv, sizeof(adv));
    const int sd_size = BTHome::build_encrypted_service_data(packet, for_sd, sd, sizeof(sd));

    expect_true("sd: both built", adv_size > 5 && sd_size > 0);
    expect_true("sd: advertisement is five bytes longer", adv_size == sd_size + 5);
    expect_bytes("sd: advertisement tail equals service data",
                 adv + 5, static_cast<size_t>(adv_size > 5 ? adv_size - 5 : 0),
                 sd, static_cast<size_t>(sd_size > 0 ? sd_size : 0));
}

static void test_service_data_error_paths()
{
    // Tests: Error conditions of build_encrypted_service_data.
    // Expects: -1 without consuming a counter value in every case - a consumed
    //          counter that produced no packet is a gap a receiver reads as loss.
    BTHome::EncryptedPacket<28> packet;
    packet.add(BTHome::battery(50));

    BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
    encryptor.setKey(kSpecKey);
    encryptor.setMac(kSpecMac);
    encryptor.setCounter(7);

    uint8_t out[26] = {};
    expect_true("sd error: null output buffer",
                BTHome::build_encrypted_service_data(packet, encryptor, nullptr, 26) == -1);
    // This packet's plaintext service data is 5 bytes - [uuid][uuid][info] plus
    // a 2-byte battery object - so with counter and MIC it needs 13.
    expect_true("sd error: output buffer one byte too small",
                BTHome::build_encrypted_service_data(packet, encryptor, out, 12) == -1);
    expect_true("sd error: counter untouched after a short buffer", encryptor.counter() == 7);
    expect_true("sd error: exactly enough is enough",
                BTHome::build_encrypted_service_data(packet, encryptor, out, 13) == 13);
    encryptor.setCounter(7);

    BTHome::Encryptor no_backend(nullptr);
    no_backend.setCounter(7);
    expect_true("sd error: missing backend",
                BTHome::build_encrypted_service_data(packet, no_backend, out, sizeof(out)) == -1);
    expect_true("sd error: counter untouched without backend", no_backend.counter() == 7);

    BTHome::Encryptor failing(&failing_backend);
    failing.setCounter(7);
    expect_true("sd error: failing backend",
                BTHome::build_encrypted_service_data(packet, failing, out, sizeof(out)) == -1);
    expect_true("sd error: counter untouched after backend failure", failing.counter() == 7);
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

    uint8_t out[31] = {};
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

// Builds the spec vector's service data, which is what a receiver gets:
// [uuid][info][ciphertext][counter][MIC].
//
// This used to build a whole advertisement and copy out everything past its
// first five bytes. That workaround was the argument for the function it calls
// now: every transport carrying BTHome without being BLE advertising had to
// reinvent the same off-by-five.
static size_t buildEncryptedServiceData(uint32_t counter, uint8_t *out, size_t out_capacity)
{
    BTHome::EncryptedPacket<28> packet;
    packet.add(BTHome::temperature(25.06f));
    packet.add(BTHome::humidity(50.55f));

    BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
    encryptor.setKey(kSpecKey);
    encryptor.setMac(kSpecMac);
    encryptor.setCounter(counter);

    const int size = BTHome::build_encrypted_service_data(packet, encryptor, out, out_capacity);
    return size < 0 ? 0 : static_cast<size_t>(size);
}

static BTHome::Decryptor makeDecryptor()
{
    BTHome::Decryptor d(&BTHome::mbedtls_ccm_decrypt_backend);
    d.setKey(kSpecKey);
    d.setMac(kSpecMac);
    return d;
}

static void test_decrypt_round_trip()
{
    // Tests: An advertisement built by the library, decrypted and decoded back.
    // Expects: The original measurements, and a buffer the Decoder accepts -
    //          which means the encrypted bit must be cleared in the output.
    uint8_t sd[32] = {};
    const size_t sd_len = buildEncryptedServiceData(kSpecCounter, sd, sizeof(sd));
    expect_true("decrypt: encrypted service data built", sd_len > 0);

    BTHome::Decryptor decryptor = makeDecryptor();
    uint8_t plain[32] = {};
    size_t plain_len = 0;
    const BTHome::DecryptStatus status =
        decryptor.decryptServiceData(sd, sd_len, plain, sizeof(plain), plain_len);

    expect_true("decrypt: status Ok", status == BTHome::DecryptStatus::Ok);
    expect_true("decrypt: counter recorded",
                decryptor.haveCounter() && decryptor.lastCounter() == kSpecCounter);

    const uint8_t want_plain[] = {0xD2, 0xFC, 0x40, 0x02, 0xCA, 0x09, 0x03, 0xBF, 0x13};
    expect_bytes("decrypt: plaintext service data", plain, plain_len,
                 want_plain, sizeof(want_plain));

    // The decisive part: the result goes straight into the Decoder.
    BTHome::Decoder dec(plain, plain_len);
    BTHome::Decoded obj;
    float temperature = 0.0f;
    float humidity = 0.0f;
    while (dec.next(obj))
    {
        if (obj.is(BTHome::ObjectId::Temperature)) { temperature = obj.value; }
        else if (obj.is(BTHome::ObjectId::Humidity)) { humidity = obj.value; }
    }
    expect_true("decrypt: decodes cleanly", dec.status() == BTHome::DecodeStatus::End);
    expect_true("decrypt: temperature 25.06", temperature > 25.05f && temperature < 25.07f);
    expect_true("decrypt: humidity 50.55", humidity > 50.54f && humidity < 50.56f);
}

static void test_decrypt_payload_entry_point()
{
    // Tests: The UUID-less shape a BLE stack such as NimBLE hands over.
    // Expects: Same plaintext, minus the two UUID bytes, usable with
    //          Decoder::fromPayload().
    uint8_t sd[32] = {};
    const size_t sd_len = buildEncryptedServiceData(kSpecCounter, sd, sizeof(sd));

    BTHome::Decryptor decryptor = makeDecryptor();
    uint8_t plain[32] = {};
    size_t plain_len = 0;
    const BTHome::DecryptStatus status =
        decryptor.decryptPayload(sd + 2, sd_len - 2, plain, sizeof(plain), plain_len);

    expect_true("payload: status Ok", status == BTHome::DecryptStatus::Ok);
    const uint8_t want_plain[] = {0x40, 0x02, 0xCA, 0x09, 0x03, 0xBF, 0x13};
    expect_bytes("payload: plaintext without uuid", plain, plain_len,
                 want_plain, sizeof(want_plain));

    BTHome::Decoder dec = BTHome::Decoder::fromPayload(plain, plain_len);
    BTHome::Decoded obj;
    size_t count = 0;
    while (dec.next(obj)) { ++count; }
    expect_true("payload: two objects, clean end",
                count == 2 && dec.status() == BTHome::DecodeStatus::End);
}

static void test_decrypt_psa_backend()
{
    // Tests: The identical round trip through the PSA Crypto adapter.
    // Expects: Same plaintext as the mbedtls adapter, and a rejected MIC on a
    //          tampered packet - the two backends must be interchangeable in
    //          both directions, not just for encryption.
    uint8_t sd[32] = {};
    const size_t sd_len = buildEncryptedServiceData(kSpecCounter, sd, sizeof(sd));

    BTHome::Decryptor decryptor(&BTHome::psa_ccm_decrypt_backend);
    decryptor.setKey(kSpecKey);
    decryptor.setMac(kSpecMac);

    uint8_t plain[32] = {};
    size_t plain_len = 0;
    expect_true("psa decrypt: status Ok",
                decryptor.decryptServiceData(sd, sd_len, plain, sizeof(plain), plain_len) ==
                    BTHome::DecryptStatus::Ok);

    const uint8_t want_plain[] = {0xD2, 0xFC, 0x40, 0x02, 0xCA, 0x09, 0x03, 0xBF, 0x13};
    expect_bytes("psa decrypt: plaintext matches mbedtls", plain, plain_len,
                 want_plain, sizeof(want_plain));

    // The rejoin of ciphertext and MIC is PSA-specific; a tampered byte has to
    // fail there too, otherwise the adapter would be accepting anything.
    uint8_t tampered[32] = {};
    memcpy(tampered, sd, sd_len);
    tampered[sd_len - 1] ^= 0x01; // last MIC byte
    BTHome::Decryptor fresh(&BTHome::psa_ccm_decrypt_backend);
    fresh.setKey(kSpecKey);
    fresh.setMac(kSpecMac);
    expect_true("psa decrypt: tampered MIC rejected",
                fresh.decryptServiceData(tampered, sd_len, plain, sizeof(plain), plain_len) ==
                    BTHome::DecryptStatus::AuthFailed);
}

static void test_decrypt_rejections()
{
    uint8_t sd[32] = {};
    const size_t sd_len = buildEncryptedServiceData(kSpecCounter, sd, sizeof(sd));
    uint8_t plain[32] = {};
    size_t plain_len = 0;

    // Tests: A replayed packet - the same bytes a second time.
    // Expects: Replay, because the counter did not advance. This is the
    //          property that makes a captured advertisement worthless.
    {
        BTHome::Decryptor decryptor = makeDecryptor();
        expect_true("replay: first packet accepted",
                    decryptor.decryptServiceData(sd, sd_len, plain, sizeof(plain), plain_len) ==
                        BTHome::DecryptStatus::Ok);
        expect_true("replay: same packet again rejected",
                    decryptor.decryptServiceData(sd, sd_len, plain, sizeof(plain), plain_len) ==
                        BTHome::DecryptStatus::Replay);
    }

    // Tests: A wrong bindkey.
    // Expects: AuthFailed, and the replay state left untouched - otherwise an
    //          attacker could push the counter forward and lock out the sender.
    {
        BTHome::Decryptor decryptor(&BTHome::mbedtls_ccm_decrypt_backend);
        uint8_t wrong_key[BTHome::Encryptor::kKeyBytes] = {};
        memcpy(wrong_key, kSpecKey, sizeof(wrong_key));
        wrong_key[0] ^= 0xFF;
        decryptor.setKey(wrong_key);
        decryptor.setMac(kSpecMac);

        expect_true("wrong key: rejected",
                    decryptor.decryptServiceData(sd, sd_len, plain, sizeof(plain), plain_len) ==
                        BTHome::DecryptStatus::AuthFailed);
        expect_true("wrong key: replay state untouched", !decryptor.haveCounter());
    }

    // Tests: A tampered ciphertext byte.
    // Expects: AuthFailed - the MIC covers the ciphertext.
    {
        uint8_t tampered[32] = {};
        memcpy(tampered, sd, sd_len);
        tampered[3] ^= 0x01; // first ciphertext byte
        BTHome::Decryptor decryptor = makeDecryptor();
        expect_true("tampered: rejected",
                    decryptor.decryptServiceData(tampered, sd_len, plain, sizeof(plain), plain_len) ==
                        BTHome::DecryptStatus::AuthFailed);
    }

    // Tests: A plaintext packet, a truncated buffer, an output buffer too
    // small, and a Decryptor without a backend.
    // Expects: One distinct status each, so a receiver can tell them apart.
    {
        BTHome::Decryptor decryptor = makeDecryptor();
        BTHome::Packet<31> plainPacket;
        plainPacket.add(BTHome::temperature(21.0f));
        expect_true("plaintext input: NotEncrypted",
                    decryptor.decryptServiceData(plainPacket.serviceData(),
                                                 plainPacket.serviceDataSize(), plain,
                                                 sizeof(plain), plain_len) ==
                        BTHome::DecryptStatus::NotEncrypted);
        expect_true("short input: BadBuffer",
                    decryptor.decryptServiceData(sd, 5, plain, sizeof(plain), plain_len) ==
                        BTHome::DecryptStatus::BadBuffer);
        expect_true("small output: BadBuffer",
                    decryptor.decryptServiceData(sd, sd_len, plain, 4, plain_len) ==
                        BTHome::DecryptStatus::BadBuffer);

        BTHome::Decryptor headless(nullptr);
        expect_true("no backend: NoBackend",
                    headless.decryptServiceData(sd, sd_len, plain, sizeof(plain), plain_len) ==
                        BTHome::DecryptStatus::NoBackend);
    }

    // Tests: A later counter after a restored replay state.
    // Expects: Accepted, and the stored counter moves forward.
    {
        uint8_t later[32] = {};
        const size_t later_len = buildEncryptedServiceData(kSpecCounter + 5, later, sizeof(later));
        BTHome::Decryptor decryptor = makeDecryptor();
        decryptor.setLastCounter(kSpecCounter);
        expect_true("advanced counter: accepted",
                    decryptor.decryptServiceData(later, later_len, plain, sizeof(plain), plain_len) ==
                        BTHome::DecryptStatus::Ok);
        expect_true("advanced counter: state moved",
                    decryptor.lastCounter() == kSpecCounter + 5);
    }
}

int main()
{
    test_spec_vector();
    test_spec_vector_psa_backend();
    test_counter_makes_ciphertext_unique();
    test_trigger_flag_in_nonce();
    test_capacity_accounting();
    test_service_data_spec_vector();
    test_service_data_is_the_advertising_tail();
    test_service_data_error_paths();
    test_error_paths();
    test_decrypt_round_trip();
    test_decrypt_payload_entry_point();
    test_decrypt_psa_backend();
    test_decrypt_rejections();

    return test_summary();
}
