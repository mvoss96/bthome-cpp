// AES-CCM encrypted BTHome on a classic AVR (Uno/Nano/Pro Mini, ATmega328P),
// for transports that are not BLE advertising.
//
// Two things make this example different from the ESP32 and Zephyr ones:
//
//   1. There is no BLE stack, so there is no advertisement to build. The bytes
//      go out over whatever transport the board has - a raw 2.4 GHz radio
//      (nRF24L01), a UART-attached module, LoRa - and the shape those want is
//      the service-data value, which build_encrypted_service_data() produces.
//
//   2. There is no platform crypto. ESP-IDF has mbedtls and Zephyr has PSA;
//      an ATmega has neither, and this library ships no cipher of its own by
//      design. So the backend comes from micro-AES, an external library - see
//      README.md, which also covers the two constants it needs set.
//
// Everything else is what encryption always demands: a nonce MAC both ends
// derive identically, a counter that survives reboots, and one build per
// update because each consumes a counter value.
#include <Arduino.h>
#include <EEPROM.h>

#include "bthome.h"

// The external cipher. Configure micro_aes.h for BTHome first:
//   CCM_NONCE_LEN = 13, CCM_TAG_LEN = 4
// Its defaults are 11 and 16, which produce a packet no BTHome receiver can
// read - and the failure is a MIC mismatch, indistinguishable from a wrong key.
extern "C" {
#include <micro_aes.h>
}

// 16-byte bindkey, shared with the receiver (Home Assistant asks for it as 32
// hex chars). The value from the BTHome specification's worked example.
static const uint8_t kKey[BTHome::Encryptor::kKeyBytes] = {
    0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1,
    0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32};

// The six MAC bytes of the CCM nonce. On BLE these are the advertiser address;
// off BLE there is none, so both ends must agree on what stands in for it.
// Whatever identifies a device on your transport is the right answer - an
// nRF24 receiver keyed by a 4-byte sender id, for instance, uses that id
// zero-extended to six. Nothing on the wire carries or checks this agreement,
// and getting it wrong looks exactly like a wrong key.
static const uint8_t kNonceMac[BTHome::Encryptor::kMacBytes] = {
    0xB7, 0x4F, 0xE7, 0x7F, 0x00, 0x00};

// On boot the counter resumes at last-stored + margin, so values consumed
// between EEPROM writes can never be reused after a power loss. The write then
// happens once per margin rather than once per packet: EEPROM endures about
// 100,000 cycles, and a packet-by-packet write would spend that in a month.
static const uint32_t kCounterMargin = 1024;
static const int kCounterAddr = 0;

static uint32_t counterStored = 0;

// The CCM backend: all this library wants from a platform. It owns the nonce,
// the layout and the counter; the cipher is yours to supply.
static bool ccmBackend(const uint8_t *key, const uint8_t *nonce,
                       const uint8_t *plaintext, size_t length,
                       uint8_t *ciphertext, uint8_t *mic)
{
    // micro-AES writes ciphertext and tag into one buffer, so the tag is split
    // off afterwards; BTHome transmits them in different places.
    uint8_t sealed[24 + BTHome::Encryptor::kMicBytes];
    if (length + BTHome::Encryptor::kMicBytes > sizeof(sealed))
    {
        return false;
    }
    AES_CCM_encrypt(key, nonce, NULL, 0, plaintext, length, sealed);
    memcpy(ciphertext, sealed, length);
    memcpy(mic, sealed + length, BTHome::Encryptor::kMicBytes);
    return true;
}

static BTHome::Encryptor encryptor(&ccmBackend);
static uint8_t packetId = 0;

static void storeCounter(uint32_t counter)
{
    EEPROM.put(kCounterAddr, counter);
    counterStored = counter;
}

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

    encryptor.setKey(kKey);
    encryptor.setMac(kNonceMac);

    uint32_t counter = 0;
    EEPROM.get(kCounterAddr, counter);
    // A fresh chip reads 0xFFFFFFFF; treating that as a counter would leave no
    // room to advance and every packet after it would be refused as a replay.
    if (counter == 0xFFFFFFFFul)
    {
        counter = 0;
    }
    counter += kCounterMargin;
    encryptor.setCounter(counter);
    storeCounter(counter);
}

void loop()
{
    // Capacity 28, so counter and MIC are reserved inside it: EncryptedPacket
    // reports size() including the 8 bytes of overhead, and add() refuses a
    // measurement that would no longer fit. Fill logic written against the
    // plaintext Packet therefore works unchanged and cannot overfill.
    BTHome::EncryptedPacket<28> packet;
    packet.add(BTHome::packet_id(packetId++));
    packet.add(BTHome::temperature(22.4f));
    packet.add(BTHome::battery(92));

    // [uuid lo][uuid hi][device-info][ciphertext][counter][MIC] - no Flags AD
    // and no AD length/type pair, because this is not an advertisement. It is
    // exactly what a receiver hands to Decryptor::decryptServiceData().
    uint8_t serviceData[28];
    const int n = BTHome::build_encrypted_service_data(packet, encryptor, serviceData,
                                                       sizeof(serviceData));
    if (n < 0)
    {
        // No counter was consumed, so retrying later costs nothing.
        Serial.println(F("encryption failed"));
        delay(5000);
        return;
    }

    dumpHex("encrypted service data:", serviceData, (size_t) n);
    // my_radio_send(serviceData, n);

    // Park the counter further ahead once the margin has been used up.
    if (encryptor.counter() - counterStored >= kCounterMargin)
    {
        storeCounter(encryptor.counter());
    }

    delay(5000);
}
