#pragma once

#include <stddef.h>
#include <stdint.h>
// C headers (<stdint.h>/<stddef.h>/<string.h>) instead of the C++ wrappers:
// avr-gcc ships no libstdc++ wrapper headers at all, and Zephyr's minimal C++
// library lacks <cstring>. The C headers exist on every supported toolchain,
// so types and calls stay unqualified (uint8_t, memcpy - no std::).
#include <string.h>

#include "bthome_defs.h"
#include "bthome_packet.h"

namespace BTHome
{

/**
 * @brief AES-128-CCM backend callback used by Encryptor.
 *
 * The library owns the BTHome-specific parts (nonce construction, payload
 * layout, counter handling) but contains no cipher implementation itself.
 * Implementations must perform AES-128-CCM with a 13-byte nonce and a 4-byte
 * tag, no additional authenticated data (see bthome_crypto_mbedtls.h for a
 * ready-made adapter). Backends needing state can keep it internally.
 *
 * @param key        16-byte AES key.
 * @param nonce      13-byte CCM nonce.
 * @param plaintext  Input bytes to encrypt.
 * @param length     Number of plaintext/ciphertext bytes.
 * @param ciphertext Output buffer for @p length encrypted bytes.
 * @param mic        Output buffer for the 4-byte message integrity check.
 * @return true on success, false on any backend failure.
 */
using CcmEncryptFn = bool (*)(const uint8_t *key,
                              const uint8_t *nonce,
                              const uint8_t *plaintext,
                              size_t length,
                              uint8_t *ciphertext,
                              uint8_t *mic);

/**
 * @brief AES-128-CCM decrypt-and-verify callback used by Decryptor.
 *
 * The mirror of CcmEncryptFn. Implementations must verify @p mic and return
 * false when it does not match - the library relies on that to reject a
 * tampered or wrong-key packet, and never looks at @p plaintext otherwise.
 *
 * @param key        16-byte AES key.
 * @param nonce      13-byte CCM nonce.
 * @param ciphertext Input bytes to decrypt.
 * @param length     Number of ciphertext/plaintext bytes.
 * @param mic        4-byte message integrity check to verify against.
 * @param plaintext  Output buffer for @p length decrypted bytes.
 * @return true when the MIC verified and @p plaintext is valid.
 */
using CcmDecryptFn = bool (*)(const uint8_t *key,
                              const uint8_t *nonce,
                              const uint8_t *ciphertext,
                              size_t length,
                              const uint8_t *mic,
                              uint8_t *plaintext);

/**
 * @brief Why a decryption attempt ended.
 *
 * Told apart because they call for different reactions: Replay is normal
 * radio noise and should be dropped quietly, AuthFailed means a wrong bindkey
 * or a tampered packet and is worth logging, NotEncrypted means the sender is
 * not configured the way this receiver expects.
 */
enum class DecryptStatus : uint8_t
{
    Ok,           // plaintext written, counter accepted
    NoBackend,    // no cipher backend was set on the Decryptor
    BadBuffer,    // input too short to hold counter and MIC, or output too small
    NotEncrypted, // the device-info encrypted bit is not set
    Replay,       // counter did not advance - a captured packet sent again
    AuthFailed,   // MIC mismatch: wrong key, or the packet was tampered with
    BadUuid,      // service UUID is not BTHome's 0xFCD2 - not BTHome data
};

class Encryptor;

template <size_t AdvCapacity>
class EncryptedPacket;

namespace detail
{

/**
 * @brief Builds the BTHome v2 CCM nonce, for both directions.
 *
 * MAC(6) + UUID little-endian(2) + device-info(1) + counter little-endian(4).
 * Sender and receiver must agree on every byte, so this exists once.
 *
 * @param mac       Device MAC in display order.
 * @param mac_bytes Number of MAC bytes (6).
 * @param device_info Device-information byte exactly as transmitted.
 * @param counter   Counter value belonging to this packet.
 * @param out       Receives 13 nonce bytes.
 */
inline void build_nonce(const uint8_t *mac, size_t mac_bytes,
                        uint8_t device_info, uint32_t counter,
                        uint8_t *out)
{
    memcpy(out, mac, mac_bytes);
    size_t p = mac_bytes;
    out[p++] = static_cast<uint8_t>(kServiceUuid & 0xFFu);
    out[p++] = static_cast<uint8_t>(kServiceUuid >> 8);
    out[p++] = device_info;
    out[p++] = static_cast<uint8_t>(counter & 0xFFu);
    out[p++] = static_cast<uint8_t>((counter >> 8) & 0xFFu);
    out[p++] = static_cast<uint8_t>((counter >> 16) & 0xFFu);
    out[p++] = static_cast<uint8_t>((counter >> 24) & 0xFFu);
}

} // namespace detail

/**
 * @brief Build an encrypted raw BLE advertising payload for BTHome.
 *
 * Output layout: [Flags AD][len][0x16][UUID][device-info][ciphertext][counter][MIC][optional Name AD].
 * On success the Encryptor's counter has been consumed and incremented.
 *
 * @tparam AdvCapacity Capacity parameter of the input EncryptedPacket.
 * @param packet Source encrypted BTHome packet (plaintext measurements inside).
 * @param encryptor Encryption material; counter is consumed on success.
 * @param out Destination byte buffer for the final raw advertising payload.
 * @param out_capacity Capacity of @p out in bytes.
 * @param local_name Optional local name; pass nullptr to omit a name AD element.
 * @param complete_local_name true for AD type 0x09 (Complete Local Name), false for 0x08 (Shortened Local Name).
 * @return Payload size in bytes on success, or -1 on error (no counter is consumed on error).
 */
template <size_t AdvCapacity>
int build_encrypted_advertising(const EncryptedPacket<AdvCapacity> &packet,
                                Encryptor &encryptor,
                                uint8_t *out,
                                size_t out_capacity,
                                const char *local_name = nullptr,
                                bool complete_local_name = true);

/**
 * @brief Build an encrypted BTHome service-data value, without the AD wrapper.
 *
 * The encrypting counterpart of Packet::serviceData(), and the exact input
 * Decryptor::decryptServiceData() consumes:
 * [uuid lo][uuid hi][device-info][ciphertext][counter][MIC].
 *
 * Use this on any transport that carries BTHome service data without being BLE
 * advertising - a raw radio link, a serial framing, an MQTT payload. Those
 * transports have no Flags AD and no AD length/type pair, and building a whole
 * advertisement only to skip its first five bytes is both wasteful and easy to
 * get wrong by one.
 *
 * On success the Encryptor's counter has been consumed and incremented.
 *
 * @tparam AdvCapacity Capacity parameter of the input EncryptedPacket.
 * @param packet Source encrypted BTHome packet (plaintext measurements inside).
 * @param encryptor Encryption material; counter is consumed on success.
 * @param out Destination buffer for the service-data value.
 * @param out_capacity Capacity of @p out in bytes.
 * @return Service-data size in bytes on success, or -1 on error (no counter is
 *         consumed on error).
 */
template <size_t AdvCapacity>
int build_encrypted_service_data(const EncryptedPacket<AdvCapacity> &packet,
                                 Encryptor &encryptor,
                                 uint8_t *out,
                                 size_t out_capacity);

/**
 * @brief Holds the encryption material and produces BTHome v2 ciphertexts.
 *
 * The counter is owned and auto-incremented by this class: every successful
 * build consumes exactly one counter value, and a build at the end of the
 * 32-bit counter space fails rather than wrap. Together that makes CCM nonce
 * reuse - the one catastrophic failure mode - impossible as long as one
 * Encryptor instance is used per key. Restore the counter after reboot via
 * setCounter() (persist counter() with a safety margin, added saturating);
 * receivers reject non-increasing counters as replays.
 *
 * The last usable counter value is 0xFFFFFFFE. Once the counter reads
 * 0xFFFFFFFF the key is used up: every build fails until a new key is
 * provisioned and the counter reset. At one advertisement per second that
 * point lies ~136 years out, but a persistence bug that restores a corrupt
 * counter can reach it early - failing hard beats reusing a nonce.
 */
class Encryptor
{
public:
    static constexpr size_t kKeyBytes = 16;     // AES-128 key length; entered in Home Assistant as 32 hex chars.
    static constexpr size_t kMacBytes = 6;      // Bluetooth MAC address length (part of the nonce).
    static constexpr size_t kNonceBytes = 13;   // CCM nonce: MAC(6) + UUID(2) + device-info(1) + counter(4).
    static constexpr size_t kCounterBytes = 4;  // Replay-protection counter, sent little-endian after the ciphertext.
    static constexpr size_t kMicBytes = 4;      // Message integrity check (auth tag), sent after the counter.
    static constexpr size_t kOverheadBytes = kCounterBytes + kMicBytes; // Extra payload bytes an encrypted packet needs.

    /**
     * @brief Constructs an Encryptor with a CCM backend.
     * @param fn Backend callback performing AES-128-CCM.
     */
    explicit Encryptor(CcmEncryptFn fn) : m_fn(fn) {}

    /**
     * @brief Sets the 16-byte AES key.
     * @param key Key bytes; copied into the Encryptor.
     */
    void setKey(const uint8_t (&key)[kKeyBytes])
    {
        memcpy(m_key, key, kKeyBytes);
    }

    /**
     * @brief Sets the device MAC address used in the nonce.
     * @param mac MAC bytes in display order (e.g. 54:48:E6:8F:80:A5 -> {0x54, 0x48, ...}).
     */
    void setMac(const uint8_t (&mac)[kMacBytes])
    {
        memcpy(m_mac, mac, kMacBytes);
    }

    /**
     * @brief Sets the counter used for the next encryption.
     * @param counter Next counter value; must exceed every previously broadcast
     *        value. 0xFFFFFFFF marks the counter space as exhausted and makes
     *        every build fail - saturate to it rather than wrap when adding a
     *        restore margin.
     */
    void setCounter(uint32_t counter)
    {
        m_counter = counter;
    }

    /**
     * @brief Returns the counter that the next encryption will use.
     * @return Current counter value (persist this, plus a margin, across reboots).
     */
    uint32_t counter() const
    {
        return m_counter;
    }

private:
    // Only build_encrypted_service_data drives the encryption; keeping this
    // private keeps the multi-parameter call out of the public API.
    // build_encrypted_advertising needs no access of its own - it wraps that
    // function's output, so there is exactly one place where a nonce is built
    // and a counter consumed.
    template <size_t AdvCapacity>
    friend int build_encrypted_service_data(const EncryptedPacket<AdvCapacity> &,
                                            Encryptor &, uint8_t *, size_t);

    /**
     * @brief Encrypts one BTHome measurement section and emits counter + MIC.
     *
     * On success the internal counter is incremented, so the next call uses a
     * fresh nonce automatically.
     *
     * @param device_info Device-information byte exactly as transmitted (encrypted bit set).
     * @param plaintext   Measurement bytes to encrypt.
     * @param length      Number of plaintext bytes.
     * @param ciphertext  Output buffer for @p length encrypted bytes.
     * @param counter_out Output buffer for the 4-byte little-endian counter.
     * @param mic_out     Output buffer for the 4-byte MIC.
     * @return true on success, false if no backend is set, the backend fails,
     *         or the counter space is exhausted.
     */
    bool encrypt(uint8_t device_info,
                 const uint8_t *plaintext,
                 size_t length,
                 uint8_t *ciphertext,
                 uint8_t *counter_out,
                 uint8_t *mic_out)
    {
        if (m_fn == nullptr)
        {
            return false;
        }

        // A wrapped counter would repeat nonces already broadcast under this
        // key - the one failure CCM cannot survive. 0xFFFFFFFF is therefore
        // never encrypted with: reaching it means the key is used up.
        if (m_counter == 0xFFFFFFFFu)
        {
            return false;
        }

        uint8_t nonce[kNonceBytes];
        detail::build_nonce(m_mac, kMacBytes, device_info, m_counter, nonce);

        if (!m_fn(m_key, nonce, plaintext, length, ciphertext, mic_out))
        {
            return false;
        }

        // Counter travels in clear text right after the ciphertext; copied from
        // the nonce so both are guaranteed to hold identical bytes.
        memcpy(counter_out, nonce + kNonceBytes - kCounterBytes, kCounterBytes);

        ++m_counter;
        return true;
    }

    CcmEncryptFn m_fn = nullptr;
    uint8_t m_key[kKeyBytes] = {};
    uint8_t m_mac[kMacBytes] = {};
    uint32_t m_counter = 0;
};

/**
 * @brief A BTHome packet whose serialized form is encrypted.
 *
 * Wraps a plain Packet but reserves Encryptor::kOverheadBytes (counter + MIC)
 * inside @p AdvCapacity, and reports size() including that overhead. Fill
 * logic written against Packet's size()/add() therefore works unchanged and
 * can never overfill an advertisement. Only build_encrypted_advertising()
 * accepts this type, so plaintext serialization of an encrypted packet is a
 * compile error.
 */
template <size_t AdvCapacity>
class EncryptedPacket
{
public:
    static constexpr size_t kOverheadBytes = Encryptor::kOverheadBytes;
    static_assert(AdvCapacity > ServiceDataHeader::kByteCount + kOverheadBytes,
                  "AdvCapacity must hold the BTHome header plus counter and MIC");

    using Inner = Packet<AdvCapacity - kOverheadBytes>;

    /**
     * @brief Constructs an empty encrypted packet (device-info encrypted bit set).
     */
    EncryptedPacket()
    {
        m_packet.setEncrypted(true);
    }

    /**
     * @brief Adds one measurement to the packet.
     * @param m Measurement to insert.
     * @return true if the measurement was inserted, false if capacity would be exceeded.
     */
    bool add(const BTHome::Measurement &m)
    {
        return m_packet.add(m);
    }

    /**
     * @brief Adds one variable-length measurement (Text/Raw) to the packet.
     * @param m Variable-length measurement to insert.
     * @return true if the measurement was inserted, false if capacity would be exceeded.
     */
    bool add(const BTHome::VarMeasurement &m)
    {
        return m_packet.add(m);
    }

    /**
     * @brief Sets the trigger-based flag (device-info bit 2).
     * @param on true to set trigger-based behavior, false for regular updates.
     */
    void setTriggerBased(bool on)
    {
        m_packet.setTriggerBased(on);
    }

    /**
     * @brief Returns the serialized size INCLUDING encryption overhead.
     * @return Size in bytes of the final encrypted AD element.
     */
    size_t size() const
    {
        return m_packet.size() + kOverheadBytes;
    }

    /**
     * @brief Returns the wrapped plaintext packet.
     * @return Inner Packet holding the unencrypted AD element.
     */
    const Inner &inner() const
    {
        return m_packet;
    }

private:
    Inner m_packet;
};

template <size_t AdvCapacity>
int build_encrypted_service_data(const EncryptedPacket<AdvCapacity> &packet,
                                 Encryptor &encryptor,
                                 uint8_t *out,
                                 size_t out_capacity)
{
    // [uuid lo][uuid hi][device-info] - the AD element's header minus the
    // [len][type] pair that belongs to advertising rather than to BTHome.
    constexpr size_t kSdHeaderBytes = ServiceDataHeader::kByteCount - 2;
    if (out == nullptr)
    {
        return -1;
    }

    const auto &inner = packet.inner();
    const size_t sd_size = inner.serviceDataSize() + Encryptor::kOverheadBytes;
    if (sd_size > out_capacity)
    {
        return -1;
    }

    const uint8_t *sd = inner.serviceData();
    memcpy(out, sd, kSdHeaderBytes);

    // The device-info byte goes into the nonce exactly as transmitted, encrypted
    // bit included - EncryptedPacket has already set it.
    const uint8_t device_info = sd[kSdHeaderBytes - 1];
    const uint8_t *plaintext = sd + kSdHeaderBytes;
    const size_t plain_len = inner.serviceDataSize() - kSdHeaderBytes;

    uint8_t *ciphertext = out + kSdHeaderBytes;
    uint8_t *counter_out = ciphertext + plain_len;
    uint8_t *mic_out = counter_out + Encryptor::kCounterBytes;
    if (!encryptor.encrypt(device_info, plaintext, plain_len, ciphertext, counter_out, mic_out))
    {
        return -1;
    }

    return static_cast<int>(sd_size);
}

template <size_t AdvCapacity>
int build_encrypted_advertising(const EncryptedPacket<AdvCapacity> &packet,
                                Encryptor &encryptor,
                                uint8_t *out,
                                size_t out_capacity,
                                const char *local_name,
                                bool complete_local_name)
{
    constexpr size_t kFlagsBytes = 3; // 02 01 06
    if (out == nullptr)
    {
        return -1;
    }

    const size_t local_name_len = (local_name != nullptr) ? strlen(local_name) : 0;
    if (local_name_len > 0xFE)
    {
        return -1;
    }

    const size_t ad_size = packet.size(); // Inner size + counter + MIC.
    // The AD length byte counts type + data, so one element caps out at 256
    // bytes. A larger EncryptedPacket is legal for service data on non-BLE
    // transports (build_encrypted_service_data()), but cannot be encoded
    // here - truncating the length byte would corrupt the advertisement.
    if (ad_size > 256)
    {
        return -1;
    }
    const size_t total = kFlagsBytes + ad_size + ((local_name_len > 0) ? (2 + local_name_len) : 0);
    if (total > out_capacity)
    {
        return -1;
    }

    out[0] = 0x02; // length of Flags AD
    out[1] = 0x01; // AD type: Flags
    out[2] = 0x06; // Flags value: LE General Discoverable Mode, BR/EDR Not Supported

    // The AD length/type pair, then the service data itself. Only the length
    // byte differs from the inner packet's: it grows by the encryption overhead.
    out[kFlagsBytes] = static_cast<uint8_t>(ad_size - 1);
    out[kFlagsBytes + 1] = ServiceDataHeader::kAdTypeServiceData16;

    // Checked above against `total`, so the capacity passed on cannot be the
    // reason this fails; a failure here is the cipher backend's.
    if (build_encrypted_service_data(packet, encryptor, out + kFlagsBytes + 2,
                                     out_capacity - kFlagsBytes - 2) < 0)
    {
        return -1;
    }

    size_t p = kFlagsBytes + ad_size;
    if (local_name_len > 0)
    {
        out[p++] = static_cast<uint8_t>(1 + local_name_len); // length of Local Name AD
        out[p++] = complete_local_name ? 0x09 : 0x08;             // AD type: Complete or Shortened Local Name
        memcpy(out + p, local_name, local_name_len);         // Copy local name
        p += local_name_len;
    }

    return static_cast<int>(p);
}

/**
 * @brief Holds the decryption material and verifies BTHome v2 ciphertexts.
 *
 * The mirror of Encryptor, and one instance per sender: bindkey, MAC and
 * replay state are all per-device.
 *
 * Replay protection is not optional here. Every accepted packet's counter is
 * remembered, and a packet whose counter does not exceed it is rejected -
 * without that, a captured advertisement stays valid forever and can be
 * replayed to fake a reading or a button press. The stored counter only moves
 * after the MIC has verified, so a forged packet claiming a huge counter
 * cannot lock out the real sender.
 *
 * Persist lastCounter() across reboots the way a sender persists its counter;
 * without it the first packet after a restart is accepted whatever its
 * counter, which reopens the replay window until the next packet arrives.
 */
class Decryptor
{
public:
    /**
     * @brief Constructs a Decryptor with a CCM backend.
     * @param fn Backend callback performing AES-128-CCM decrypt-and-verify.
     */
    explicit Decryptor(CcmDecryptFn fn) : m_fn(fn) {}

    /**
     * @brief Sets the 16-byte AES key (the sender's bindkey).
     * @param key Key bytes; copied into the Decryptor.
     */
    void setKey(const uint8_t (&key)[Encryptor::kKeyBytes])
    {
        memcpy(m_key, key, Encryptor::kKeyBytes);
    }

    /**
     * @brief Sets the sender's MAC address used in the nonce.
     * @param mac MAC bytes in display order, as the sender transmits them.
     *        On BLE this is the advertiser address from the scan result.
     */
    void setMac(const uint8_t (&mac)[Encryptor::kMacBytes])
    {
        memcpy(m_mac, mac, Encryptor::kMacBytes);
    }

    /**
     * @brief Returns the counter of the last accepted packet.
     * @return Counter value; meaningless while haveCounter() is false.
     */
    uint32_t lastCounter() const { return m_last_counter; }

    /** @brief Whether any packet has been accepted yet. */
    bool haveCounter() const { return m_have_counter; }

    /**
     * @brief Restores the replay state, e.g. after a reboot.
     * @param counter Highest counter already seen from this sender.
     */
    void setLastCounter(uint32_t counter)
    {
        m_last_counter = counter;
        m_have_counter = true;
    }

    /**
     * @brief Decrypts received BTHome service data.
     *
     * @param service_data [uuid lo][uuid hi][device info][ciphertext][counter][MIC].
     *        The service UUID is verified; a mismatch is BadUuid.
     * @param len Number of bytes in @p service_data.
     * @param out Receives [uuid lo][uuid hi][device info][objects...] - the
     *        encrypted bit is cleared, so the result feeds BTHome::Decoder
     *        directly.
     * @param out_capacity Capacity of @p out.
     * @param out_len Set to the number of bytes written on success.
     * @return DecryptStatus::Ok on success. On AuthFailed the object bytes of
     *         @p out have already been overwritten by the cipher backend -
     *         decryption runs before the MIC can be checked, and there is no
     *         way around that - so @p out holds no usable data unless Ok is
     *         returned. Every other status returns before the cipher and
     *         leaves @p out alone.
     */
    DecryptStatus decryptServiceData(const uint8_t *service_data, size_t len,
                                     uint8_t *out, size_t out_capacity,
                                     size_t &out_len)
    {
        return decryptInto(service_data, len, kUuidBytes, out, out_capacity, out_len);
    }

    /**
     * @brief Decrypts a payload whose UUID the BLE stack already stripped.
     *
     * @param payload [device info][ciphertext][counter][MIC] - what NimBLE's
     *        getServiceData() hands over.
     * @param len Number of bytes in @p payload.
     * @param out Receives [device info][objects...], encrypted bit cleared,
     *        ready for BTHome::Decoder::fromPayload().
     * @param out_capacity Capacity of @p out.
     * @param out_len Set to the number of bytes written on success.
     * @return DecryptStatus::Ok on success; @p out holds no usable data
     *         otherwise, and is clobbered on AuthFailed (see
     *         decryptServiceData()). Do not keep last-good plaintext in the
     *         buffer you decrypt into.
     */
    DecryptStatus decryptPayload(const uint8_t *payload, size_t len,
                                 uint8_t *out, size_t out_capacity,
                                 size_t &out_len)
    {
        return decryptInto(payload, len, 0, out, out_capacity, out_len);
    }

private:
    static constexpr size_t kUuidBytes = 2;

    // Shared by both entry points; uuid_bytes is 2 when the buffer still
    // carries the service UUID and 0 when the BLE stack already removed it.
    DecryptStatus decryptInto(const uint8_t *data, size_t len, size_t uuid_bytes,
                              uint8_t *out, size_t out_capacity, size_t &out_len)
    {
        if (m_fn == nullptr)
        {
            return DecryptStatus::NoBackend;
        }

        const size_t header = uuid_bytes + 1; // device-info byte follows the UUID
        if (data == nullptr || out == nullptr || len < header)
        {
            return DecryptStatus::BadBuffer;
        }

        // Whatever this buffer is, it is not BTHome service data - rejecting
        // it here keeps the decryptor as strict as the Decoder, which refuses
        // the same bytes in their plaintext form.
        if (uuid_bytes != 0 &&
            (data[0] != static_cast<uint8_t>(kServiceUuid & 0xFFu) ||
             data[1] != static_cast<uint8_t>(kServiceUuid >> 8)))
        {
            return DecryptStatus::BadUuid;
        }

        // Checked before the length: a plaintext packet is shorter than the
        // encryption overhead, and reporting it as a bad buffer would send a
        // receiver looking for a radio fault instead of a misconfigured sender.
        const uint8_t device_info = data[uuid_bytes];
        if ((device_info & DeviceInfo::kEncryptedBit) == 0)
        {
            return DecryptStatus::NotEncrypted;
        }

        if (len < header + Encryptor::kOverheadBytes)
        {
            return DecryptStatus::BadBuffer;
        }

        const size_t cipher_len = len - header - Encryptor::kOverheadBytes;
        if (out_capacity < header + cipher_len)
        {
            return DecryptStatus::BadBuffer;
        }

        const uint8_t *ciphertext = data + header;
        const uint8_t *counter_bytes = ciphertext + cipher_len;
        const uint8_t *mic = counter_bytes + Encryptor::kCounterBytes;

        const uint32_t counter = static_cast<uint32_t>(counter_bytes[0]) |
                                 (static_cast<uint32_t>(counter_bytes[1]) << 8) |
                                 (static_cast<uint32_t>(counter_bytes[2]) << 16) |
                                 (static_cast<uint32_t>(counter_bytes[3]) << 24);

        // Cheap check first: a replay never reaches the cipher.
        if (m_have_counter && counter <= m_last_counter)
        {
            return DecryptStatus::Replay;
        }

        // The nonce uses the device-info byte exactly as transmitted, so it is
        // built before the encrypted bit is cleared for the output.
        uint8_t nonce[Encryptor::kNonceBytes];
        detail::build_nonce(m_mac, Encryptor::kMacBytes, device_info, counter, nonce);

        // Decrypting straight into the caller's buffer: CCM cannot verify the
        // MIC before it has decrypted, so a scratch buffer would only move the
        // clobbering, at the cost of another packet's worth of RAM on a part
        // that may have 2 KB. Hence the documented rule that out means nothing
        // unless Ok comes back.
        if (!m_fn(m_key, nonce, ciphertext, cipher_len, mic, out + header))
        {
            return DecryptStatus::AuthFailed;
        }

        // Only now is the counter trusted: moving it on an unauthenticated
        // packet would let anyone lock this receiver out of its sender.
        m_last_counter = counter;
        m_have_counter = true;

        if (uuid_bytes != 0)
        {
            out[0] = static_cast<uint8_t>(kServiceUuid & 0xFFu);
            out[1] = static_cast<uint8_t>(kServiceUuid >> 8);
        }
        // The objects that follow are plaintext now, so the flag has to go -
        // otherwise Decoder would refuse the very buffer it just produced.
        out[uuid_bytes] = static_cast<uint8_t>(device_info & ~DeviceInfo::kEncryptedBit);

        out_len = header + cipher_len;
        return DecryptStatus::Ok;
    }

    CcmDecryptFn m_fn = nullptr;
    uint8_t m_key[Encryptor::kKeyBytes] = {};
    uint8_t m_mac[Encryptor::kMacBytes] = {};
    uint32_t m_last_counter = 0;
    bool m_have_counter = false;
};

} // namespace BTHome
