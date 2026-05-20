#pragma once

#include <cstddef>
#include <cstdint>

#include "bthome_defs.h"

template <std::size_t Capacity>
class BTHomePacket
{
private:
    static constexpr std::size_t kHeaderBytes = BTHome::ServiceDataHeader::kByteCount; // [len][type][uuid lo][uuid hi][device-info]
    static_assert(Capacity >= kHeaderBytes, "Capacity must hold the BTHome header");   // Capacity must fit fixed header bytes.
    static constexpr std::size_t kMaxItems = (Capacity - kHeaderBytes) / 2 + 1;        // Worst-case slots; exact fit is checked in insert().

    BTHome::ServiceDataHeader m_header = {};     // Header model incl. device-info flags.
    BTHome::Measurement m_items[kMaxItems] = {}; // Sorted measurements before serialization.
    std::size_t m_count = 0;                     // Number of valid entries in m_items.
    std::uint8_t m_buf[Capacity] = {};           // Final AD element bytes.
    std::size_t m_size = kHeaderBytes;           // Current used bytes in m_buf.

public:
    /**
     * @brief Constructs an empty BTHome packet and initializes the fixed header bytes.
     */
    BTHomePacket() { rebuild(); }

    /**
     * @brief Adds one measurement to the packet.
     * @param m Measurement to insert.
     * @return true if the measurement was inserted, false if capacity would be exceeded.
     */
    bool add(const BTHome::Measurement &m)
    {
        return insert(m);
    }

    /**
     * @brief Sets the trigger-based flag (device-info bit 2).
     * @param on true to set trigger-based behavior, false for regular updates.
     */
    void setTriggerBased(bool on)
    {
        m_header.m_device_info.m_trigger_based = on;
        rebuild();
    }

    /**
     * @brief Sets the encryption flag in the device-info byte (bit 0).
     * @param on true to mark payload as encrypted, false otherwise.
     * @note This only sets the flag bit; payload encryption itself is external.
     */
    void setEncrypted(bool on)
    {
        m_header.m_device_info.m_encrypted = on;
        rebuild();
    }

    /**
     * @brief Returns the full AD element bytes.
     * @return Pointer to bytes formatted as [len][0x16][uuid lo][uuid hi][device-info][measurements...].
     */
    const std::uint8_t *data() const
    {
        return m_buf;
    }

    /**
     * @brief Returns the current AD element size in bytes.
     * @return Number of bytes currently used in the packet buffer.
     */
    std::size_t size() const
    {
        return m_size;
    }

    /**
     * @brief Returns the service-data value bytes without AD length/type prefix.
     * @return Pointer to bytes formatted as [uuid lo][uuid hi][device-info][measurements...].
     */
    const std::uint8_t *serviceData() const
    {
        return m_buf + 2;
    }

    /**
     * @brief Returns the size of the service-data value (without AD length/type prefix).
     * @return Number of bytes in serviceData().
     */
    std::size_t serviceDataSize() const
    {
        return m_size - 2;
    }

private:
    /**
     * @brief Inserts one measurement in canonical order and rebuilds serialized bytes.
     * @param m Measurement to insert.
     * @return true if insertion succeeded, false on capacity overflow.
     */
    bool insert(const BTHome::Measurement &m)
    {
        if (m_count >= kMaxItems)
        {
            return false;
        }

        if (m_size + 1 + m.len > Capacity)
        {
            return false;
        }

        // Find first slot whose object_id exceeds m's (canonical ascending order).
        std::size_t pos = 0;
        while (pos < m_count && m_items[pos].object_id <= m.object_id)
        {
            ++pos;
        }

        // Shift the tail up by one, then drop m into place.
        for (std::size_t k = m_count; k > pos; --k)
        {
            m_items[k] = m_items[k - 1];
        }
        m_items[pos] = m;
        ++m_count;
        rebuild();
        return true;
    }

    /**
     * @brief Rebuilds the serialized AD element from header and stored measurements.
     */
    void rebuild()
    {
        m_header.writeTo(m_buf);
        std::size_t p = kHeaderBytes;

        for (std::size_t k = 0; k < m_count; ++k)
        {
            const BTHome::Measurement &m = m_items[k];
            m_buf[p++] = m.object_id;
            for (std::uint8_t b = 0; b < m.len; ++b)
            {
                m_buf[p++] = m.data[b];
            }
        }

        m_buf[0] = static_cast<std::uint8_t>(p - 1);
        m_size = p;
    }
};
