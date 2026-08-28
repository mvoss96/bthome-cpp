#pragma once

#include <stdint.h>

#include "bthome_defs.h"

namespace BTHome
{
    namespace detail
    {

        /**
         * @brief Serialization rules for one BTHome object id.
         *
         * `width` is the number of value bytes on the wire; `variable` marks
         * the objects whose length is not fixed (their rule follows from
         * `kind`). For `scaled` objects the physical value is
         * `raw * factor` and the public API is float-based; otherwise the raw
         * integer *is* the value and the public API is integer-based.
         */
        struct ObjectLayout
        {
            ObjectKind kind = ObjectKind::Unknown;
            uint8_t width = 0;      // fixed value bytes; 0 when variable or unknown
            float factor = 1.0f;    // physical value = raw * factor (scaled only)
            bool is_signed = false; // raw is two's complement
            bool scaled = false;    // float-valued (factor applies) vs. exact integer
            bool variable = false;  // length is not fixed - see kind
        };

        /** @brief Object id as its raw byte. Takes ObjectId and nothing else. */
        constexpr uint8_t oid(ObjectId e)
        {
            return static_cast<uint8_t>(e);
        }

        constexpr ObjectLayout scaled_sensor(float factor, uint8_t width, bool is_signed)
        {
            return ObjectLayout{ObjectKind::Sensor, width, factor, is_signed, true, false};
        }

        constexpr ObjectLayout int_sensor(uint8_t width, bool is_signed)
        {
            return ObjectLayout{ObjectKind::Sensor, width, 1.0f, is_signed, false, false};
        }

        constexpr ObjectLayout fixed_width(ObjectKind kind, uint8_t width)
        {
            return ObjectLayout{kind, width, 1.0f, false, false, false};
        }

        constexpr ObjectLayout variable_length(ObjectKind kind)
        {
            return ObjectLayout{kind, 0, 1.0f, false, false, true};
        }

        /**
         * @brief The single description of every BTHome v2 object id.
         *
         * Both directions read this table - the factories derive the payload
         * width from it and the decoder derives width, scaling and family from
         * it - so a width is stated exactly once and encode/decode cannot
         * drift apart.
         *
         * Deliberately a switch and not a lookup array. A table of ~90 rows is
         * roughly 1 KB of constant data, and avr-gcc copies const objects into
         * RAM at startup unless they carry PROGMEM - an ATmega328P has 2 KB in
         * total. As a switch the same information lives in flash as code, which
         * is where there is room. The decoder has to look ids up at runtime
         * (they arrive over the air), so the table cannot be compiled away
         * there; the encode side reaches it through the u/s/b/make_sensor
         * templates, which evaluate it at compile time.
         *
         * @param id BTHome object id byte.
         * @return Layout rules, or a default-constructed (Unknown, width 0)
         *         layout for ids this library version does not know.
         */
        constexpr ObjectLayout object_layout(uint8_t id)
        {
            switch (id)
            {
            // Misc data
            case oid(ObjectId::PacketId):            return fixed_width(ObjectKind::PacketId, 1);

            // Sensor data - in object-id order, one line per object, so the table
            // can be read straight against https://bthome.io/format/
            case oid(ObjectId::Battery):           return int_sensor(1, false);               // 0x01  uint8
            case oid(ObjectId::Temperature):       return scaled_sensor(0.01f, 2, true);      // 0x02  sint16   x0.01
            case oid(ObjectId::Humidity):          return scaled_sensor(0.01f, 2, false);     // 0x03  uint16   x0.01
            case oid(ObjectId::Pressure):          return scaled_sensor(0.01f, 3, false);     // 0x04  uint24   x0.01
            case oid(ObjectId::Illuminance):       return scaled_sensor(0.01f, 3, false);     // 0x05  uint24   x0.01
            case oid(ObjectId::MassKg):            return scaled_sensor(0.01f, 2, false);     // 0x06  uint16   x0.01
            case oid(ObjectId::MassLb):            return scaled_sensor(0.01f, 2, false);     // 0x07  uint16   x0.01
            case oid(ObjectId::Dewpoint):          return scaled_sensor(0.01f, 2, true);      // 0x08  sint16   x0.01
            case oid(ObjectId::Count):             return int_sensor(1, false);               // 0x09  uint8
            case oid(ObjectId::Energy):            return scaled_sensor(0.001f, 3, false);    // 0x0A  uint24   x0.001
            case oid(ObjectId::Power):             return scaled_sensor(0.01f, 3, false);     // 0x0B  uint24   x0.01
            case oid(ObjectId::Voltage):           return scaled_sensor(0.001f, 2, false);    // 0x0C  uint16   x0.001
            case oid(ObjectId::Pm2_5):             return scaled_sensor(1.0f, 2, false);      // 0x0D  uint16
            case oid(ObjectId::Pm10):              return scaled_sensor(1.0f, 2, false);      // 0x0E  uint16
            case oid(ObjectId::Co2):               return scaled_sensor(1.0f, 2, false);      // 0x12  uint16
            case oid(ObjectId::Tvoc):              return scaled_sensor(1.0f, 2, false);      // 0x13  uint16
            case oid(ObjectId::Moisture):          return scaled_sensor(0.01f, 2, false);     // 0x14  uint16   x0.01
            case oid(ObjectId::HumidityU8):        return int_sensor(1, false);               // 0x2E  uint8
            case oid(ObjectId::MoistureU8):        return int_sensor(1, false);               // 0x2F  uint8
            case oid(ObjectId::CountU16):          return int_sensor(2, false);               // 0x3D  uint16
            case oid(ObjectId::CountU32):          return int_sensor(4, false);               // 0x3E  uint32
            case oid(ObjectId::Rotation):          return scaled_sensor(0.1f, 2, true);       // 0x3F  sint16   x0.1
            case oid(ObjectId::DistanceMm):        return scaled_sensor(1.0f, 2, false);      // 0x40  uint16
            case oid(ObjectId::DistanceM):         return scaled_sensor(0.1f, 2, false);      // 0x41  uint16   x0.1
            case oid(ObjectId::Duration):          return scaled_sensor(0.001f, 3, false);    // 0x42  uint24   x0.001
            case oid(ObjectId::Current):           return scaled_sensor(0.001f, 2, false);    // 0x43  uint16   x0.001
            case oid(ObjectId::Speed):             return scaled_sensor(0.01f, 2, false);     // 0x44  uint16   x0.01
            case oid(ObjectId::TemperatureC1):     return scaled_sensor(0.1f, 2, true);       // 0x45  sint16   x0.1
            case oid(ObjectId::UvIndex):           return scaled_sensor(0.1f, 1, false);      // 0x46  uint8    x0.1
            case oid(ObjectId::VolumeL):           return scaled_sensor(0.1f, 2, false);      // 0x47  uint16   x0.1
            case oid(ObjectId::VolumeMl):          return scaled_sensor(1.0f, 2, false);      // 0x48  uint16
            case oid(ObjectId::VolumeFlowRate):    return scaled_sensor(0.001f, 2, false);    // 0x49  uint16   x0.001
            case oid(ObjectId::VoltageCenti):      return scaled_sensor(0.1f, 2, false);      // 0x4A  uint16   x0.1
            case oid(ObjectId::Gas):               return scaled_sensor(0.001f, 3, false);    // 0x4B  uint24   x0.001
            case oid(ObjectId::GasU32):            return scaled_sensor(0.001f, 4, false);    // 0x4C  uint32   x0.001
            case oid(ObjectId::EnergyU32):         return scaled_sensor(0.001f, 4, false);    // 0x4D  uint32   x0.001
            case oid(ObjectId::VolumeU32):         return scaled_sensor(0.001f, 4, false);    // 0x4E  uint32   x0.001
            case oid(ObjectId::Water):             return scaled_sensor(0.001f, 4, false);    // 0x4F  uint32   x0.001
            case oid(ObjectId::Timestamp):         return int_sensor(4, false);               // 0x50  uint32   epoch seconds
            case oid(ObjectId::Acceleration):      return scaled_sensor(0.001f, 2, false);    // 0x51  uint16   x0.001
            case oid(ObjectId::Gyroscope):         return scaled_sensor(0.001f, 2, false);    // 0x52  uint16   x0.001
            case oid(ObjectId::Text):              return variable_length(ObjectKind::Text);  // 0x53  [len][bytes...]
            case oid(ObjectId::Raw):               return variable_length(ObjectKind::Raw);   // 0x54  [len][bytes...]
            case oid(ObjectId::VolumeStorage):     return scaled_sensor(0.001f, 4, false);    // 0x55  uint32   x0.001
            case oid(ObjectId::Conductivity):      return scaled_sensor(1.0f, 2, false);      // 0x56  uint16
            case oid(ObjectId::TemperatureS8):     return int_sensor(1, true);                // 0x57  sint8
            case oid(ObjectId::TemperatureS8_035): return scaled_sensor(0.35f, 1, true);      // 0x58  sint8    x0.35
            case oid(ObjectId::CountS8):           return int_sensor(1, true);                // 0x59  sint8
            case oid(ObjectId::CountS16):          return int_sensor(2, true);                // 0x5A  sint16
            case oid(ObjectId::CountS32):          return int_sensor(4, true);                // 0x5B  sint32
            case oid(ObjectId::PowerS32):          return scaled_sensor(0.01f, 4, true);      // 0x5C  sint32   x0.01
            case oid(ObjectId::CurrentS16):        return scaled_sensor(0.001f, 2, true);     // 0x5D  sint16   x0.001
            case oid(ObjectId::Direction):         return scaled_sensor(0.01f, 2, false);     // 0x5E  uint16   x0.01
            case oid(ObjectId::Precipitation):     return scaled_sensor(0.1f, 2, false);      // 0x5F  uint16   x0.1
            case oid(ObjectId::Channel):           return int_sensor(1, false);               // 0x60  uint8
            case oid(ObjectId::RotationalSpeed):   return scaled_sensor(1.0f, 2, false);      // 0x61  uint16
            case oid(ObjectId::SpeedS32):          return scaled_sensor(0.000001f, 4, true);  // 0x62  sint32   x0.000001
            case oid(ObjectId::AccelerationS32):   return scaled_sensor(0.000001f, 4, true);  // 0x63  sint32   x0.000001
            case oid(ObjectId::LightLevel):        return int_sensor(1, false);               // 0x64  uint8
            case oid(ObjectId::SettingsRevision):  return int_sensor(1, false);               // 0x65  uint8

            // Binary sensor data - every one of them a single 0x00/0x01 byte.
            case oid(ObjectId::GenericBoolean):
            case oid(ObjectId::PowerState):
            case oid(ObjectId::Opening):
            case oid(ObjectId::BatteryLow):
            case oid(ObjectId::BatteryCharging):
            case oid(ObjectId::CarbonMonoxide):
            case oid(ObjectId::Cold):
            case oid(ObjectId::Connectivity):
            case oid(ObjectId::Door):
            case oid(ObjectId::GarageDoor):
            case oid(ObjectId::GasDetected):
            case oid(ObjectId::Heat):
            case oid(ObjectId::Light):
            case oid(ObjectId::Lock):
            case oid(ObjectId::MoistureDetected):
            case oid(ObjectId::Motion):
            case oid(ObjectId::Moving):
            case oid(ObjectId::Occupancy):
            case oid(ObjectId::Plug):
            case oid(ObjectId::Presence):
            case oid(ObjectId::Problem):
            case oid(ObjectId::Running):
            case oid(ObjectId::Safety):
            case oid(ObjectId::Smoke):
            case oid(ObjectId::Sound):
            case oid(ObjectId::Tamper):
            case oid(ObjectId::Vibration):
            case oid(ObjectId::Window):
                return fixed_width(ObjectKind::Binary, 1);

            // Events. The command event carries [argument count][opcode][args],
            // so its length depends on its own payload; the dimmer object is
            // fixed at [event][steps] (see dimmer_event()).
            case oid(ObjectId::ButtonEvent):        return fixed_width(ObjectKind::ButtonEvent, 1);  // 0x3A  uint8
            case oid(ObjectId::CommandEvent):       return variable_length(ObjectKind::CommandEvent);  // 0x3B  [argc][opcode][args...]
            case oid(ObjectId::DimmerEvent):        return fixed_width(ObjectKind::DimmerEvent, 2);  // 0x3C  [event][steps]

            // Device information
            case oid(ObjectId::DeviceTypeId):      return fixed_width(ObjectKind::DeviceTypeId, 2);  // 0xF0  uint16
            case oid(ObjectId::FirmwareVersionU32): return fixed_width(ObjectKind::FirmwareVersion, 4);  // 0xF1  uint32
            case oid(ObjectId::FirmwareVersionU24): return fixed_width(ObjectKind::FirmwareVersion, 3);  // 0xF2  uint24

            default:
                return ObjectLayout{};
            }
        }

        /**
         * @brief Compile-time sanity check over the whole id space.
         *
         * Catches the typos a hand-written table invites - a variable-length
         * entry that also states a width, a factor on an exact-integer object,
         * a width no Measurement can hold.
         */
        constexpr bool layout_table_is_consistent()
        {
            for (unsigned id = 0; id <= 0xFFu; ++id)
            {
                const ObjectLayout l = object_layout(static_cast<uint8_t>(id));
                if (l.kind == ObjectKind::Unknown)
                {
                    if (l.width != 0 || l.variable || l.scaled)
                    {
                        return false;
                    }
                    continue;
                }
                if (l.variable)
                {
                    if (l.width != 0 || l.scaled)
                    {
                        return false;
                    }
                }
                else if (l.width == 0 || l.width > 4)
                {
                    return false;
                }
                if (l.scaled && l.kind != ObjectKind::Sensor)
                {
                    return false;
                }
                if (!l.scaled && l.factor != 1.0f)
                {
                    return false;
                }
            }
            return true;
        }

        static_assert(layout_table_is_consistent(),
                      "object_layout() table is internally inconsistent");

    } // namespace detail

    /**
     * @brief A measurement that carries its object id in its type.
     *
     * Measurement::object_id is a runtime field, so a template parameter
     * cannot be deduced from it. The factories return this instead, which lets
     * StaticPacket work out its layout from the values alone:
     *
     *   BTHome::StaticPacket packet(BTHome::temperature(22.4f),
     *                               BTHome::humidity(54.3f));
     *
     * It carries only the bytes its object actually has, not the six a
     * Measurement reserves for the widest one. A StaticPacket constructor has
     * to materialize all of its measurements at once, and on AVR the
     * difference between four 8-byte objects and four 2-to-4-byte ones is
     * hundreds of bytes of flash.
     */
    template <ObjectId Id>
    struct Typed
    {
        static constexpr uint8_t kWidth = detail::object_layout(detail::oid(Id)).width;

        // len is kWidth normally and 0 when the factory could not encode the
        // value - make_sensor() drops NaN and the infinities that way, and
        // Packet::add() rejects a zero-length measurement.
        uint8_t len = kWidth;
        uint8_t data[kWidth ? kWidth : 1] = {};

        /** @brief Converts wherever a plain Measurement is expected. */
        operator Measurement() const
        {
            Measurement m;
            m.object_id = detail::oid(Id);
            m.len = len;
            for (uint8_t i = 0; i < kWidth; ++i)
            {
                m.data[i] = data[i];
            }
            return m;
        }
    };

} // namespace BTHome
