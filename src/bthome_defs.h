#pragma once

#include <stddef.h>
#include <stdint.h>

namespace BTHome
{

    inline constexpr uint16_t kServiceUuid = 0xFCD2;

    // A single encoded measurement: object id plus its little-endian value bytes.
    // Produced by the BTHome::* factories.
    struct Measurement
    {
        uint8_t object_id = 0;
        uint8_t len = 0;      // number of value bytes (0..6)
        uint8_t data[6] = {}; // little-endian value, `len` bytes used
    };

    // A variable-length measurement for Text (0x53) and Raw (0x54) objects,
    // which are serialized with an extra length byte: [id][len][bytes...].
    // Produced by BTHome::text() / BTHome::raw(); owns its bytes by value.
    struct VarMeasurement
    {
        // Largest value that can ever fit one AD element: 31 bytes capacity
        // minus service-data header (5) minus object id and length byte (2).
        static constexpr size_t kMaxBytes = 24;

        uint8_t object_id = 0;
        uint8_t len = 0;              // number of value bytes (0..kMaxBytes)
        uint8_t data[kMaxBytes] = {}; // value bytes, `len` bytes used
    };

    // BTHome device-information byte (first byte after UUID in service data).
    struct DeviceInfo
    {
        static constexpr uint8_t kEncryptedBit = 0x01u;    // bit 0
        static constexpr uint8_t kTriggerBasedBit = 0x04u; // bit 2

        bool m_encrypted = false;     // bit 0
        bool m_trigger_based = false; // bit 2
        uint8_t m_version = 2;   // bits 5..7

        constexpr uint8_t toByte() const
        {
            const uint8_t version_bits = static_cast<uint8_t>((m_version & 0x07u) << 5);
            const uint8_t encrypted_bit = m_encrypted ? kEncryptedBit : 0u;
            const uint8_t trigger_bit = m_trigger_based ? kTriggerBasedBit : 0u;
            return static_cast<uint8_t>(version_bits | encrypted_bit | trigger_bit);
        }
    };

    // Fixed BTHome service-data AD-element header bytes:
    // [len][0x16][UUID lo][UUID hi][device-info]
    struct ServiceDataHeader
    {
        static constexpr size_t kByteCount = 5;
        static constexpr uint8_t kAdTypeServiceData16 = 0x16;

        DeviceInfo m_device_info = {};

        constexpr void writeTo(uint8_t *out) const
        {
            out[0] = 0; // length is finalized after payload is written
            out[1] = kAdTypeServiceData16;
            out[2] = static_cast<uint8_t>(kServiceUuid & 0xFFu);
            out[3] = static_cast<uint8_t>(kServiceUuid >> 8);
            out[4] = m_device_info.toByte();
        }
    };

    // Object family, independent of the ~90 raw object ids. Assigned to every
    // id by detail::object_layout() (bthome_objects.h) and used to pick the
    // right serialization rule on both the encode and the decode side.
    enum class ObjectKind : uint8_t
    {
        Unknown,         // id not known to this library version
        PacketId,        // 0x00
        Sensor,          // measurement, scaled or exact integer
        Binary,          // binary sensor
        ButtonEvent,     // 0x3A
        DimmerEvent,     // 0x3C
        CommandEvent,    // 0x3B
        Text,            // 0x53
        Raw,             // 0x54
        DeviceTypeId,    // 0xF0
        FirmwareVersion, // 0xF1 / 0xF2
    };

    // Misc data object IDs.
    enum class MiscObjectId : uint8_t
    {
        PacketId = 0x00,
    };

    // Sensor data object IDs.
    enum class SensorObjectId : uint8_t
    {
        Battery = 0x01,
        Temperature = 0x02,
        Humidity = 0x03,
        Pressure = 0x04,
        Illuminance = 0x05,
        MassKg = 0x06,
        MassLb = 0x07,
        Dewpoint = 0x08,
        Count = 0x09,
        Energy = 0x0A,
        Power = 0x0B,
        Voltage = 0x0C,
        Pm2_5 = 0x0D,
        Pm10 = 0x0E,

        Co2 = 0x12,
        Tvoc = 0x13,
        Moisture = 0x14,

        HumidityU8 = 0x2E,
        MoistureU8 = 0x2F,

        CountU16 = 0x3D,
        CountU32 = 0x3E,
        Rotation = 0x3F,
        DistanceMm = 0x40,
        DistanceM = 0x41,
        Duration = 0x42,
        Current = 0x43,
        Speed = 0x44,
        TemperatureC1 = 0x45,
        UvIndex = 0x46,
        VolumeL = 0x47,
        VolumeMl = 0x48,
        VolumeFlowRate = 0x49,
        VoltageCenti = 0x4A,
        Gas = 0x4B,
        GasU32 = 0x4C,
        EnergyU32 = 0x4D,
        VolumeU32 = 0x4E,
        Water = 0x4F,
        Timestamp = 0x50,
        Acceleration = 0x51,
        Gyroscope = 0x52,
        Text = 0x53,
        Raw = 0x54,
        VolumeStorage = 0x55,
        Conductivity = 0x56,
        TemperatureS8 = 0x57,
        TemperatureS8_035 = 0x58,
        CountS8 = 0x59,
        CountS16 = 0x5A,
        CountS32 = 0x5B,
        PowerS32 = 0x5C,
        CurrentS16 = 0x5D,
        Direction = 0x5E,
        Precipitation = 0x5F,
        Channel = 0x60,
        RotationalSpeed = 0x61,
        SpeedS32 = 0x62,
        AccelerationS32 = 0x63,
        LightLevel = 0x64,
        SettingsRevision = 0x65,
    };

    // Binary sensor data object IDs.
    enum class BinaryObjectId : uint8_t
    {
        GenericBoolean = 0x0F,
        PowerState = 0x10,
        Opening = 0x11,

        BatteryLow = 0x15,
        BatteryCharging = 0x16,
        CarbonMonoxide = 0x17,
        Cold = 0x18,
        Connectivity = 0x19,
        Door = 0x1A,
        GarageDoor = 0x1B,
        GasDetected = 0x1C,
        Heat = 0x1D,
        Light = 0x1E,
        Lock = 0x1F,
        MoistureDetected = 0x20,
        Motion = 0x21,
        Moving = 0x22,
        Occupancy = 0x23,
        Plug = 0x24,
        Presence = 0x25,
        Problem = 0x26,
        Running = 0x27,
        Safety = 0x28,
        Smoke = 0x29,
        Sound = 0x2A,
        Tamper = 0x2B,
        Vibration = 0x2C,
        Window = 0x2D,
    };

    // Event object IDs.
    enum class EventObjectId : uint8_t
    {
        ButtonEvent = 0x3A,
        CommandEvent = 0x3B,
        DimmerEvent = 0x3C,
    };

    // Button event codes (value byte of EventObjectId::ButtonEvent).
    enum class ButtonEventType : uint8_t
    {
        // Placeholder for "no event": with several buttons, the k-th 0x3A
        // entry in a packet addresses button k, so earlier buttons are padded
        // with None (spec example: 3A 00 3A 01 = press on button 2).
        None = 0x00,
        Press = 0x01,
        DoublePress = 0x02,
        TriplePress = 0x03,
        LongPress = 0x04,
        LongDoublePress = 0x05,
        LongTriplePress = 0x06,
        HoldPress = 0x80,
    };

    // Dimmer event codes (first value byte of EventObjectId::DimmerEvent).
    enum class DimmerEventType : uint8_t
    {
        None = 0x00,
        RotateLeft = 0x01,
        RotateRight = 0x02,
    };

    // Command event opcodes (EventObjectId::CommandEvent). Serialized as
    // [argument count][opcode][arguments...]; StepUp/StepDown carry one
    // step-count argument. The spec advises sending commands only in
    // encrypted advertisements, since anyone can observe or spoof them.
    enum class CommandEventType : uint8_t
    {
        Off = 0x00,
        On = 0x01,
        Toggle = 0x02,
        StepUp = 0x03,
        StepDown = 0x04,
    };

    // Device object IDs.
    enum class DeviceObjectId : uint8_t
    {
        DeviceTypeId = 0xF0,
        FirmwareVersionU32 = 0xF1,
        FirmwareVersionU24 = 0xF2,
    };

} // namespace BTHome
