#pragma once

#include <cstddef>
#include <cstdint>
// <string.h> instead of <cstring>: Zephyr's minimal C++ library ships no
// C++ wrapper headers (only cstddef/cstdint/new), but the C header exists on
// every supported libc. Calls are therefore unqualified (memcpy, not std::memcpy).
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
using CcmEncryptFn = bool (*)(const std::uint8_t *key,
                              const std::uint8_t *nonce,
                              const std::uint8_t *plaintext,
                              std::size_t length,
                              std::uint8_t *ciphertext,
                              std::uint8_t *mic);

class Encryptor;

template <std::size_t AdvCapacity>
class EncryptedPacket;

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
template <std::size_t AdvCapacity>
int build_encrypted_advertising(const EncryptedPacket<AdvCapacity> &packet,
                                Encryptor &encryptor,
                                std::uint8_t *out,
                                std::size_t out_capacity,
                                const char *local_name = nullptr,
                                bool complete_local_name = true);

/**
 * @brief Holds the encryption material and produces BTHome v2 ciphertexts.
 *
 * The counter is owned and auto-incremented by this class: every successful
 * build consumes exactly one counter value. This makes CCM nonce reuse -
 * the one catastrophic failure mode - structurally impossible as long as one
 * Encryptor instance is used per key. Restore the counter after reboot via
 * setCounter() (persist counter() with a safety margin); receivers reject
 * non-increasing counters as replays.
 */
class Encryptor
{
public:
    static constexpr std::size_t kKeyBytes = 16;     // AES-128 key length; entered in Home Assistant as 32 hex chars.
    static constexpr std::size_t kMacBytes = 6;      // Bluetooth MAC address length (part of the nonce).
    static constexpr std::size_t kNonceBytes = 13;   // CCM nonce: MAC(6) + UUID(2) + device-info(1) + counter(4).
    static constexpr std::size_t kCounterBytes = 4;  // Replay-protection counter, sent little-endian after the ciphertext.
    static constexpr std::size_t kMicBytes = 4;      // Message integrity check (auth tag), sent after the counter.
    static constexpr std::size_t kOverheadBytes = kCounterBytes + kMicBytes; // Extra payload bytes an encrypted packet needs.

    /**
     * @brief Constructs an Encryptor with a CCM backend.
     * @param fn Backend callback performing AES-128-CCM.
     */
    explicit Encryptor(CcmEncryptFn fn) : m_fn(fn) {}

    /**
     * @brief Sets the 16-byte AES key.
     * @param key Key bytes; copied into the Encryptor.
     */
    void setKey(const std::uint8_t (&key)[kKeyBytes])
    {
        memcpy(m_key, key, kKeyBytes);
    }

    /**
     * @brief Sets the device MAC address used in the nonce.
     * @param mac MAC bytes in display order (e.g. 54:48:E6:8F:80:A5 -> {0x54, 0x48, ...}).
     */
    void setMac(const std::uint8_t (&mac)[kMacBytes])
    {
        memcpy(m_mac, mac, kMacBytes);
    }

    /**
     * @brief Sets the counter used for the next encryption.
     * @param counter Next counter value; must exceed every previously broadcast value.
     */
    void setCounter(std::uint32_t counter)
    {
        m_counter = counter;
    }

    /**
     * @brief Returns the counter that the next encryption will use.
     * @return Current counter value (persist this, plus a margin, across reboots).
     */
    std::uint32_t counter() const
    {
        return m_counter;
    }

private:
    // Only build_encrypted_advertising drives the encryption; keeping this
    // private keeps the multi-parameter call out of the public API.
    template <std::size_t AdvCapacity>
    friend int build_encrypted_advertising(const EncryptedPacket<AdvCapacity> &,
                                           Encryptor &, std::uint8_t *, std::size_t,
                                           const char *, bool);

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
     * @return true on success, false if no backend is set or the backend fails.
     */
    bool encrypt(std::uint8_t device_info,
                 const std::uint8_t *plaintext,
                 std::size_t length,
                 std::uint8_t *ciphertext,
                 std::uint8_t *counter_out,
                 std::uint8_t *mic_out)
    {
        if (m_fn == nullptr)
        {
            return false;
        }

        // Nonce: MAC(6) + UUID little-endian(2) + device-info(1) + counter little-endian(4).
        std::uint8_t nonce[kNonceBytes];
        memcpy(nonce, m_mac, kMacBytes);
        std::size_t p = kMacBytes;
        nonce[p++] = static_cast<std::uint8_t>(kServiceUuid & 0xFFu);
        nonce[p++] = static_cast<std::uint8_t>(kServiceUuid >> 8);
        nonce[p++] = device_info;
        nonce[p++] = static_cast<std::uint8_t>(m_counter & 0xFFu);
        nonce[p++] = static_cast<std::uint8_t>((m_counter >> 8) & 0xFFu);
        nonce[p++] = static_cast<std::uint8_t>((m_counter >> 16) & 0xFFu);
        nonce[p++] = static_cast<std::uint8_t>((m_counter >> 24) & 0xFFu);

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
    std::uint8_t m_key[kKeyBytes] = {};
    std::uint8_t m_mac[kMacBytes] = {};
    std::uint32_t m_counter = 0;
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
template <std::size_t AdvCapacity>
class EncryptedPacket
{
public:
    static constexpr std::size_t kOverheadBytes = Encryptor::kOverheadBytes;
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
    std::size_t size() const
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

template <std::size_t AdvCapacity>
int build_encrypted_advertising(const EncryptedPacket<AdvCapacity> &packet,
                                Encryptor &encryptor,
                                std::uint8_t *out,
                                std::size_t out_capacity,
                                const char *local_name,
                                bool complete_local_name)
{
    constexpr std::size_t kFlagsBytes = 3; // 02 01 06
    constexpr std::size_t kHeaderBytes = ServiceDataHeader::kByteCount;
    if (out == nullptr)
    {
        return -1;
    }

    const std::size_t local_name_len = (local_name != nullptr) ? strlen(local_name) : 0;
    if (local_name_len > 0xFE)
    {
        return -1;
    }

    const std::size_t ad_size = packet.size(); // Inner size + counter + MIC.
    const std::size_t total = kFlagsBytes + ad_size + ((local_name_len > 0) ? (2 + local_name_len) : 0);
    if (total > out_capacity)
    {
        return -1;
    }

    out[0] = 0x02; // length of Flags AD
    out[1] = 0x01; // AD type: Flags
    out[2] = 0x06; // Flags value: LE General Discoverable Mode, BR/EDR Not Supported

    // Header is copied from the inner packet; only the AD length byte grows
    // by the encryption overhead.
    const auto &inner = packet.inner();
    memcpy(out + kFlagsBytes, inner.data(), kHeaderBytes);
    out[kFlagsBytes] = static_cast<std::uint8_t>(ad_size - 1);

    const std::uint8_t device_info = inner.data()[kHeaderBytes - 1];
    const std::uint8_t *plaintext = inner.data() + kHeaderBytes;
    const std::size_t plain_len = inner.size() - kHeaderBytes;

    std::uint8_t *ciphertext = out + kFlagsBytes + kHeaderBytes;
    std::uint8_t *counter_out = ciphertext + plain_len;
    std::uint8_t *mic_out = counter_out + Encryptor::kCounterBytes;
    if (!encryptor.encrypt(device_info, plaintext, plain_len, ciphertext, counter_out, mic_out))
    {
        return -1;
    }

    std::size_t p = kFlagsBytes + ad_size;
    if (local_name_len > 0)
    {
        out[p++] = static_cast<std::uint8_t>(1 + local_name_len); // length of Local Name AD
        out[p++] = complete_local_name ? 0x09 : 0x08;             // AD type: Complete or Shortened Local Name
        memcpy(out + p, local_name, local_name_len);         // Copy local name
        p += local_name_len;
    }

    return static_cast<int>(p);
}

} // namespace BTHome
