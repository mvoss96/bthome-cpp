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

        /** @brief Object id of any BTHome id enum as a raw byte. */
        template <typename ObjectIdEnum>
        constexpr uint8_t oid(ObjectIdEnum e)
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

        constexpr ObjectLayout fixed(ObjectKind kind, uint8_t width)
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
         * Deliberately a switch and not a lookup array: an ObjectLayout[256]
         * would be roughly 3 KB, and avr-gcc places const arrays in RAM unless
         * they carry PROGMEM - an ATmega328P has 2 KB in total. As a switch the
         * compiler emits a jump table in flash and folds constant lookups away.
         *
         * @param id BTHome object id byte.
         * @return Layout rules, or a default-constructed (Unknown, width 0)
         *         layout for ids this library version does not know.
         */
        constexpr ObjectLayout object_layout(uint8_t id)
        {
            switch (id)
            {
            // Misc
            case oid(MiscObjectId::PacketId):
                return fixed(ObjectKind::PacketId, 1);

            // Scaled sensors - float API, physical value = raw * factor.
            case oid(SensorObjectId::Temperature):
            case oid(SensorObjectId::Dewpoint):
                return scaled_sensor(0.01f, 2, true);
            case oid(SensorObjectId::Humidity):
            case oid(SensorObjectId::Moisture):
                return scaled_sensor(0.01f, 2, false);
            case oid(SensorObjectId::Pressure):
            case oid(SensorObjectId::Illuminance):
            case oid(SensorObjectId::Power):
                return scaled_sensor(0.01f, 3, false);
            case oid(SensorObjectId::Energy):
            case oid(SensorObjectId::Gas):
            case oid(SensorObjectId::Duration):
                return scaled_sensor(0.001f, 3, false);
            case oid(SensorObjectId::VolumeFlowRate):
            case oid(SensorObjectId::Acceleration):
            case oid(SensorObjectId::Gyroscope):
                return scaled_sensor(0.001f, 2, false);
            case oid(SensorObjectId::CurrentS16):
                return scaled_sensor(0.001f, 2, true);
            case oid(SensorObjectId::MassKg):
            case oid(SensorObjectId::MassLb):
            case oid(SensorObjectId::Speed):
                return scaled_sensor(0.01f, 2, false);
            case oid(SensorObjectId::Voltage):
            case oid(SensorObjectId::Current):
                return scaled_sensor(0.001f, 2, false);
            case oid(SensorObjectId::Pm2_5):
            case oid(SensorObjectId::Pm10):
            case oid(SensorObjectId::Co2):
            case oid(SensorObjectId::Tvoc):
            case oid(SensorObjectId::DistanceMm):
            case oid(SensorObjectId::VolumeMl):
            case oid(SensorObjectId::Conductivity):
            case oid(SensorObjectId::RotationalSpeed):
                return scaled_sensor(1.0f, 2, false);
            case oid(SensorObjectId::Rotation):
            case oid(SensorObjectId::TemperatureC1):
                return scaled_sensor(0.1f, 2, true);
            case oid(SensorObjectId::DistanceM):
            case oid(SensorObjectId::VolumeL):
            case oid(SensorObjectId::VoltageCenti):
            case oid(SensorObjectId::Precipitation):
                return scaled_sensor(0.1f, 2, false);
            case oid(SensorObjectId::UvIndex):
                return scaled_sensor(0.1f, 1, false);
            case oid(SensorObjectId::GasU32):
            case oid(SensorObjectId::EnergyU32):
            case oid(SensorObjectId::VolumeU32):
            case oid(SensorObjectId::Water):
            case oid(SensorObjectId::VolumeStorage):
                return scaled_sensor(0.001f, 4, false);
            case oid(SensorObjectId::Direction):
                return scaled_sensor(0.01f, 2, false);
            case oid(SensorObjectId::TemperatureS8_035):
                return scaled_sensor(0.35f, 1, true);
            case oid(SensorObjectId::PowerS32):
                return scaled_sensor(0.01f, 4, true);
            case oid(SensorObjectId::SpeedS32):
            case oid(SensorObjectId::AccelerationS32):
                return scaled_sensor(0.000001f, 4, true);

            // Integer sensors - the raw value is the value. These deliberately
            // skip the float path: a float cannot hold a full uint32 exactly,
            // which matters for Timestamp and the 32-bit counters.
            case oid(SensorObjectId::Battery):
            case oid(SensorObjectId::Count):
            case oid(SensorObjectId::HumidityU8):
            case oid(SensorObjectId::MoistureU8):
            case oid(SensorObjectId::Channel):
            case oid(SensorObjectId::LightLevel):
            case oid(SensorObjectId::SettingsRevision):
                return int_sensor(1, false);
            case oid(SensorObjectId::CountU16):
                return int_sensor(2, false);
            case oid(SensorObjectId::CountU32):
            case oid(SensorObjectId::Timestamp):
                return int_sensor(4, false);
            case oid(SensorObjectId::TemperatureS8):
            case oid(SensorObjectId::CountS8):
                return int_sensor(1, true);
            case oid(SensorObjectId::CountS16):
                return int_sensor(2, true);
            case oid(SensorObjectId::CountS32):
                return int_sensor(4, true);

            // Variable-length sensors: [id][len][bytes...]
            case oid(SensorObjectId::Text):
                return variable_length(ObjectKind::Text);
            case oid(SensorObjectId::Raw):
                return variable_length(ObjectKind::Raw);

            // Binary sensors - one byte each.
            case oid(BinaryObjectId::GenericBoolean):
            case oid(BinaryObjectId::PowerState):
            case oid(BinaryObjectId::Opening):
            case oid(BinaryObjectId::BatteryLow):
            case oid(BinaryObjectId::BatteryCharging):
            case oid(BinaryObjectId::CarbonMonoxide):
            case oid(BinaryObjectId::Cold):
            case oid(BinaryObjectId::Connectivity):
            case oid(BinaryObjectId::Door):
            case oid(BinaryObjectId::GarageDoor):
            case oid(BinaryObjectId::GasDetected):
            case oid(BinaryObjectId::Heat):
            case oid(BinaryObjectId::Light):
            case oid(BinaryObjectId::Lock):
            case oid(BinaryObjectId::MoistureDetected):
            case oid(BinaryObjectId::Motion):
            case oid(BinaryObjectId::Moving):
            case oid(BinaryObjectId::Occupancy):
            case oid(BinaryObjectId::Plug):
            case oid(BinaryObjectId::Presence):
            case oid(BinaryObjectId::Problem):
            case oid(BinaryObjectId::Running):
            case oid(BinaryObjectId::Safety):
            case oid(BinaryObjectId::Smoke):
            case oid(BinaryObjectId::Sound):
            case oid(BinaryObjectId::Tamper):
            case oid(BinaryObjectId::Vibration):
            case oid(BinaryObjectId::Window):
                return fixed(ObjectKind::Binary, 1);

            // Events. The command event carries [argument count][opcode][args],
            // so its length depends on its own payload; the dimmer object is
            // fixed at [event][steps] (see dimmer_event()).
            case oid(EventObjectId::ButtonEvent):
                return fixed(ObjectKind::ButtonEvent, 1);
            case oid(EventObjectId::CommandEvent):
                return variable_length(ObjectKind::CommandEvent);
            case oid(EventObjectId::DimmerEvent):
                return fixed(ObjectKind::DimmerEvent, 2);

            // Device objects
            case oid(DeviceObjectId::DeviceTypeId):
                return fixed(ObjectKind::DeviceTypeId, 2);
            case oid(DeviceObjectId::FirmwareVersionU32):
                return fixed(ObjectKind::FirmwareVersion, 4);
            case oid(DeviceObjectId::FirmwareVersionU24):
                return fixed(ObjectKind::FirmwareVersion, 3);

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
} // namespace BTHome
