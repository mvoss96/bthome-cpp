#pragma once

#include <cstdint>
#include <type_traits>

#include "bthome_defs.h"

namespace BTHome
{
    namespace detail
    {

        struct SensorEncoding
        {
            float factor = 1.0f;
            std::uint8_t width = 0;
            bool is_signed = false;
        };

        /**
         * @brief Return encoding rules for a given sensor object ID.
         * @param id BTHome sensor object ID.
         * @return Scaling factor, payload width in bytes and signedness.
         */
        inline SensorEncoding sensor_encoding(SensorObjectId id)
        {
            switch (id)
            {
            case SensorObjectId::Temperature:
            case SensorObjectId::Dewpoint:
                return {0.01f, 2, true};
            case SensorObjectId::Humidity:
            case SensorObjectId::Moisture:
                return {0.01f, 2, false};
            case SensorObjectId::Pressure:
            case SensorObjectId::Illuminance:
            case SensorObjectId::Power:
                return {0.01f, 3, false};
            case SensorObjectId::Energy:
            case SensorObjectId::Gas:
            case SensorObjectId::Duration:
                return {0.001f, 3, false};
            case SensorObjectId::VolumeFlowRate:
            case SensorObjectId::Acceleration:
            case SensorObjectId::Gyroscope:
            case SensorObjectId::CurrentS16:
                return {0.001f, 2, false};
            case SensorObjectId::MassKg:
            case SensorObjectId::MassLb:
                return {0.01f, 2, false};
            case SensorObjectId::Voltage:
            case SensorObjectId::Current:
                return {0.001f, 2, false};
            case SensorObjectId::Pm2_5:
            case SensorObjectId::Pm10:
            case SensorObjectId::Co2:
            case SensorObjectId::Tvoc:
            case SensorObjectId::DistanceMm:
            case SensorObjectId::VolumeMl:
            case SensorObjectId::Conductivity:
            case SensorObjectId::RotationalSpeed:
                return {1.0f, 2, false};
            case SensorObjectId::Rotation:
            case SensorObjectId::TemperatureC1:
                return {0.1f, 2, true};
            case SensorObjectId::DistanceM:
            case SensorObjectId::Speed:
            case SensorObjectId::VolumeL:
            case SensorObjectId::VoltageCenti:
            case SensorObjectId::Precipitation:
                return {0.1f, 2, false};
            case SensorObjectId::UvIndex:
                return {0.1f, 1, false};
            case SensorObjectId::HumidityU8:
            case SensorObjectId::MoistureU8:
            case SensorObjectId::Channel:
            case SensorObjectId::LightLevel:
            case SensorObjectId::SettingsRevision:
                return {1.0f, 1, false};
            case SensorObjectId::GasU32:
            case SensorObjectId::EnergyU32:
            case SensorObjectId::VolumeU32:
            case SensorObjectId::Water:
            case SensorObjectId::VolumeStorage:
                return {0.001f, 4, false};
            case SensorObjectId::Direction:
                return {0.01f, 2, false};
            case SensorObjectId::TemperatureS8:
            case SensorObjectId::CountS8:
                return {1.0f, 1, true};
            case SensorObjectId::TemperatureS8_035:
                return {0.35f, 1, true};
            case SensorObjectId::CountS16:
                return {1.0f, 2, true};
            case SensorObjectId::CountS32:
                return {1.0f, 4, true};
            case SensorObjectId::PowerS32:
                return {0.01f, 4, true};
            case SensorObjectId::SpeedS32:
            case SensorObjectId::AccelerationS32:
                return {0.000001f, 4, true};
            default:
                return {};
            }
        }

        /**
         * @brief Encode an unsigned raw integer into a Measurement payload.
         * @param id Object ID byte.
         * @param raw Unsigned raw value.
         * @param width Number of payload bytes.
         * @return Encoded Measurement with little-endian value bytes.
         */
        inline Measurement make_u(std::uint8_t id, std::uint64_t raw,
                                  std::uint8_t width)
        {
            Measurement m;
            m.object_id = id;
            m.len = width;

            const std::uint64_t umax =
                (width >= 8) ? ~std::uint64_t{0}
                             : ((std::uint64_t{1} << (8 * width)) - 1);
            if (raw > umax)
            {
                raw = umax;
            }

            for (std::uint8_t i = 0; i < width; ++i)
            {
                m.data[i] = static_cast<std::uint8_t>(raw & 0xFF);
                raw >>= 8;
            }
            return m;
        }

        /**
         * @brief Encode a signed raw integer into a Measurement payload.
         * @param id Object ID byte.
         * @param raw Signed raw value.
         * @param width Number of payload bytes.
         * @return Encoded Measurement with little-endian value bytes.
         */
        inline Measurement make_s(std::uint8_t id, std::int64_t raw,
                                  std::uint8_t width)
        {
            Measurement m;
            m.object_id = id;
            m.len = width;

            const std::int64_t max_v = (std::int64_t{1} << (8 * width - 1)) - 1;
            const std::int64_t min_v = -(std::int64_t{1} << (8 * width - 1));
            if (raw > max_v)
            {
                raw = max_v;
            }
            if (raw < min_v)
            {
                raw = min_v;
            }

            std::uint64_t u = static_cast<std::uint64_t>(raw);
            for (std::uint8_t i = 0; i < width; ++i)
            {
                m.data[i] = static_cast<std::uint8_t>(u & 0xFF);
                u >>= 8;
            }
            return m;
        }

        /**
         * @brief Encode a sensor value using rules from sensor_encoding().
         * @param id BTHome sensor object ID.
         * @param value Physical value to encode.
         * @return Encoded Measurement.
         */
        inline Measurement make_sensor(SensorObjectId id, float value)
        {
            const SensorEncoding e = sensor_encoding(id);
            Measurement m;
            m.object_id = static_cast<std::uint8_t>(id);
            m.len = e.width;

            // Scale to raw integer units and round half away from zero.
            const float s = value / e.factor;
            std::int64_t raw = static_cast<std::int64_t>(s >= 0.0f ? s + 0.5f : s - 0.5f);

            // Clamp to the representable range for the selected width/sign.
            if (e.is_signed)
            {
                const std::int64_t max_v = (std::int64_t{1} << (8 * e.width - 1)) - 1;
                const std::int64_t min_v = -(std::int64_t{1} << (8 * e.width - 1));
                if (raw > max_v)
                {
                    raw = max_v;
                }
                if (raw < min_v)
                {
                    raw = min_v;
                }
            }
            else
            {
                if (raw < 0)
                {
                    raw = 0;
                }
                const std::uint64_t umax =
                    (e.width >= 8) ? ~std::uint64_t{0}
                                   : ((std::uint64_t{1} << (8 * e.width)) - 1);
                if (static_cast<std::uint64_t>(raw) > umax)
                {
                    raw = static_cast<std::int64_t>(umax);
                }
            }

            // Write payload in little-endian byte order.
            std::uint64_t u = static_cast<std::uint64_t>(raw);
            for (std::uint8_t i = 0; i < e.width; ++i)
            {
                m.data[i] = static_cast<std::uint8_t>(u & 0xFF);
                u >>= 8;
            }

            return m;
        }

        /**
         * @brief Encode a 1-byte unsigned value for an enum-based object ID.
         * @tparam ObjectId Enum type of the object ID.
         * @param id Object ID.
         * @param raw Unsigned raw value.
         * @return Encoded Measurement.
         */
        template <typename ObjectId>
        inline Measurement u8(ObjectId id, std::uint64_t raw)
        {
            static_assert(std::is_enum<ObjectId>::value, "u8 expects an enum object ID type.");
            return make_u(static_cast<std::uint8_t>(id), raw, 1);
        }

        /**
         * @brief Encode a 2-byte unsigned value for an enum-based object ID.
         * @tparam ObjectId Enum type of the object ID.
         * @param id Object ID.
         * @param raw Unsigned raw value.
         * @return Encoded Measurement.
         */
        template <typename ObjectId>
        inline Measurement u16(ObjectId id, std::uint64_t raw)
        {
            static_assert(std::is_enum<ObjectId>::value, "u16 expects an enum object ID type.");
            return make_u(static_cast<std::uint8_t>(id), raw, 2);
        }

        /**
         * @brief Encode a 4-byte unsigned value for an enum-based object ID.
         * @tparam ObjectId Enum type of the object ID.
         * @param id Object ID.
         * @param raw Unsigned raw value.
         * @return Encoded Measurement.
         */
        template <typename ObjectId>
        inline Measurement u32(ObjectId id, std::uint64_t raw)
        {
            static_assert(std::is_enum<ObjectId>::value, "u32 expects an enum object ID type.");
            return make_u(static_cast<std::uint8_t>(id), raw, 4);
        }

        /**
         * @brief Encode a 1-byte signed value for an enum-based object ID.
         * @tparam ObjectId Enum type of the object ID.
         * @param id Object ID.
         * @param raw Signed raw value.
         * @return Encoded Measurement.
         */
        template <typename ObjectId>
        inline Measurement i8(ObjectId id, std::int64_t raw)
        {
            static_assert(std::is_enum<ObjectId>::value, "i8 expects an enum object ID type.");
            return make_s(static_cast<std::uint8_t>(id), raw, 1);
        }

        /**
         * @brief Encode a 2-byte signed value for an enum-based object ID.
         * @tparam ObjectId Enum type of the object ID.
         * @param id Object ID.
         * @param raw Signed raw value.
         * @return Encoded Measurement.
         */
        template <typename ObjectId>
        inline Measurement i16(ObjectId id, std::int64_t raw)
        {
            static_assert(std::is_enum<ObjectId>::value, "i16 expects an enum object ID type.");
            return make_s(static_cast<std::uint8_t>(id), raw, 2);
        }

        /**
         * @brief Encode a 4-byte signed value for an enum-based object ID.
         * @tparam ObjectId Enum type of the object ID.
         * @param id Object ID.
         * @param raw Signed raw value.
         * @return Encoded Measurement.
         */
        template <typename ObjectId>
        inline Measurement i32(ObjectId id, std::int64_t raw)
        {
            static_assert(std::is_enum<ObjectId>::value, "i32 expects an enum object ID type.");
            return make_s(static_cast<std::uint8_t>(id), raw, 4);
        }

        /**
         * @brief Encode a boolean value as a 1-byte payload for an enum-based object ID.
         * @tparam ObjectId Enum type of the object ID.
         * @param id Object ID.
         * @param on Boolean payload value.
         * @return Encoded Measurement.
         */
        template <typename ObjectId>
        inline Measurement b(ObjectId id, bool on)
        {
            static_assert(std::is_enum<ObjectId>::value, "b expects an enum object ID type.");
            Measurement m;
            m.object_id = static_cast<std::uint8_t>(id);
            m.len = 1;
            m.data[0] = on ? 1 : 0;
            return m;
        }

    } // namespace detail
} // namespace BTHome
