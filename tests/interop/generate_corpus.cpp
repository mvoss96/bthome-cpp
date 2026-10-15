// Interop corpus generator: builds BTHome advertisements through the public
// library API and emits them as JSON Lines on stdout. Each line is one test
// case for tests/interop/check_with_bthome_ble.py, which feeds the payload
// into bthome-ble - the parser Home Assistant uses - and asserts that the
// decoded values match the inputs encoded here.
//
// Build & run from project root (needs mbedtls for the encrypted cases):
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I src tests/interop/generate_corpus.cpp -lmbedcrypto -o build/generate_corpus
//   ./build/generate_corpus | python3 tests/interop/check_with_bthome_ble.py
#include "bthome.h"
#include "bthome_crypto_mbedtls.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace
{

    // Any address works for plaintext; encrypted cases use the MAC baked into
    // the nonce (the spec vector MAC), because the parser rebuilds it from the
    // advertiser address.
    const char *const kPlainMac = "A4:C1:38:8D:18:B2";
    const char *const kSpecMacStr = "54:48:E6:8F:80:A5";
    const uint8_t kSpecMac[BTHome::Encryptor::kMacBytes] = {0x54, 0x48, 0xE6, 0x8F, 0x80, 0xA5};
    const char *const kSpecKeyHex = "231d39c1d7cc1ab1aee224cd096db932";
    const uint8_t kSpecKey[BTHome::Encryptor::kKeyBytes] = {
        0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1,
        0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32};

    void print_hex(const uint8_t *data, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
        {
            printf("%02X", data[i]);
        }
    }

    // Emits one case. `payload` is the service data starting at the
    // device-info byte (what bthome-ble expects as service_data value).
    // `expected_json` is the comma-separated content of the "expected" array.
    void emit(const char *name, const char *mac,
              const uint8_t *payload, size_t len,
              const char *expected_json,
              const char *bindkey = nullptr, const char *session = nullptr)
    {
        printf("{\"name\":\"%s\",\"mac\":\"%s\",\"payload_hex\":\"", name, mac);
        print_hex(payload, len);
        printf("\"");
        if (bindkey != nullptr)
        {
            printf(",\"bindkey\":\"%s\"", bindkey);
        }
        if (session != nullptr)
        {
            printf(",\"session\":\"%s\"", session);
        }
        printf(",\"expected\":[%s]}\n", expected_json);
    }

    void emit_packet(const char *name, const BTHome::Packet<31> &p, const char *expected_json)
    {
        emit(name, kPlainMac, p.serviceData() + 2, p.serviceDataSize() - 2, expected_json);
    }

    // One packet with a single measurement. `key` is the entity key bthome-ble
    // assigns to this object id (its spec-table name), so a wrong object id
    // fails even when another sensor type shares the same scaling.
    void sensor_case(const char *name, const BTHome::Measurement &m, const char *key,
                     double value, double tolerance)
    {
        BTHome::Packet<31> p;
        p.add(m);
        char expected[160];
        snprintf(expected, sizeof(expected),
                      "{\"kind\":\"sensor\",\"key\":\"%s\",\"value\":%.10g,\"tolerance\":%.10g}",
                      key, value, tolerance);
        emit_packet(name, p, expected);
    }

    void binary_case(const char *name, const BTHome::Measurement &m, bool value)
    {
        BTHome::Packet<31> p;
        p.add(m);
        char expected[64];
        snprintf(expected, sizeof(expected),
                      "{\"kind\":\"binary\",\"value\":%s}", value ? "true" : "false");
        emit_packet(name, p, expected);
    }

    // Encrypted packet: emits the payload of build_encrypted_advertising after
    // the AD headers (device-info byte onward), plus the bindkey.
    void emit_encrypted(const char *name, BTHome::EncryptedPacket<28> &p,
                        BTHome::Encryptor &encryptor, const char *expected_json,
                        const char *session = nullptr)
    {
        uint8_t adv[31] = {};
        const int n = BTHome::build_encrypted_advertising(p, encryptor, adv, sizeof(adv));
        if (n < 8)
        {
            fprintf(stderr, "encrypted build failed for %s\n", name);
            return;
        }
        // adv = [02 01 06][len][0x16][D2][FC][device-info]... -> payload at 7.
        emit(name, kSpecMacStr, adv + 7, static_cast<size_t>(n) - 7,
             expected_json, kSpecKeyHex, session);
    }

} // namespace

// Erases the per-id return type of a binary factory.
template <auto Factory>
static BTHome::Measurement as_measurement(bool on)
{
    return Factory(on);
}

int main()
{
    // --- Scalar sensors: same representative values as tests/test_factories.cpp,
    // tolerance = half the encoding factor.
    sensor_case("battery", BTHome::battery(87), "battery", 87, 0.5);
    sensor_case("temperature", BTHome::temperature(21.53f), "temperature", 21.53, 0.005);
    sensor_case("humidity", BTHome::humidity(54.2f), "humidity", 54.2, 0.005);
    sensor_case("pressure", BTHome::pressure(1013.25f), "pressure", 1013.25, 0.005);
    sensor_case("illuminance", BTHome::illuminance(13460.67f), "illuminance", 13460.67, 0.005);
    sensor_case("mass_kg", BTHome::mass_kg(80.3f), "mass", 80.3, 0.005);
    sensor_case("mass_lb", BTHome::mass_lb(71.42f), "mass", 71.42, 0.005);
    sensor_case("dewpoint", BTHome::dewpoint(17.38f), "dew_point", 17.38, 0.005);
    sensor_case("count", BTHome::count(96), "count", 96, 0.5);
    sensor_case("energy", BTHome::energy(12.345f), "energy", 12.345, 0.0005);
    sensor_case("power", BTHome::power(69.14f), "power", 69.14, 0.005);
    sensor_case("voltage", BTHome::voltage(3.074f), "voltage", 3.074, 0.0005);
    sensor_case("pm2_5", BTHome::pm2_5(561.0f), "pm25", 561, 0.5);
    sensor_case("pm10", BTHome::pm10(1188.0f), "pm10", 1188, 0.5);
    sensor_case("co2", BTHome::co2(1250.0f), "carbon_dioxide", 1250, 0.5);
    sensor_case("tvoc", BTHome::tvoc(307.0f), "volatile_organic_compounds", 307, 0.5);
    sensor_case("moisture", BTHome::moisture(30.74f), "moisture", 30.74, 0.005);
    sensor_case("humidity_u8", BTHome::humidity_u8(35), "humidity", 35, 0.5);
    sensor_case("moisture_u8", BTHome::moisture_u8(30), "moisture", 30, 0.5);
    sensor_case("count_u16", BTHome::count_u16(4884), "count", 4884, 0.5);
    sensor_case("count_u32", BTHome::count_u32(300000), "count", 300000, 0.5);
    sensor_case("rotation", BTHome::rotation(307.4f), "rotation", 307.4, 0.05);
    sensor_case("distance_mm", BTHome::distance_mm(12.0f), "distance", 12, 0.5);
    sensor_case("distance_m", BTHome::distance_m(7.8f), "distance", 7.8, 0.05);
    sensor_case("duration", BTHome::duration(13.39f), "duration", 13.39, 0.0005);
    sensor_case("current", BTHome::current(13.39f), "current", 13.39, 0.0005);
    sensor_case("speed", BTHome::speed(133.9f), "speed", 133.9, 0.005);
    sensor_case("temperature_c1", BTHome::temperature_c1(25.1f), "temperature", 25.1, 0.05);
    sensor_case("uv_index", BTHome::uv_index(5.0f), "uv_index", 5.0, 0.05);
    sensor_case("volume_l", BTHome::volume_l(2215.1f), "volume", 2215.1, 0.05);
    sensor_case("volume_ml", BTHome::volume_ml(34780.0f), "volume", 34780, 0.5);
    sensor_case("volume_flow_rate", BTHome::volume_flow_rate(2.022f), "volume_flow_rate", 2.022, 0.0005);
    sensor_case("voltage_c1", BTHome::voltage_c1(23.1f), "voltage", 23.1, 0.05);
    sensor_case("gas", BTHome::gas(12.345f), "gas", 12.345, 0.0005);
    sensor_case("gas_u32", BTHome::gas_u32(12.345f), "gas", 12.345, 0.0005);
    sensor_case("energy_u32", BTHome::energy_u32(12.345f), "energy", 12.345, 0.0005);
    sensor_case("volume_u32", BTHome::volume_u32(12.345f), "volume", 12.345, 0.0005);
    sensor_case("water", BTHome::water(12.345f), "water", 12.345, 0.0005);
    sensor_case("timestamp", BTHome::timestamp(1684093277u), "timestamp", 1684093277, 0.5);
    sensor_case("acceleration", BTHome::acceleration(22.151f), "acceleration", 22.151, 0.0005);
    sensor_case("gyroscope", BTHome::gyroscope(22.151f), "gyroscope", 22.151, 0.0005);
    sensor_case("volume_storage", BTHome::volume_storage(12.345f), "volume_storage", 12.345, 0.0005);
    sensor_case("conductivity", BTHome::conductivity(3120.0f), "conductivity", 3120, 0.5);
    sensor_case("temperature_s8", BTHome::temperature_s8(25), "temperature", 25, 0.5);
    sensor_case("temperature_s8_035", BTHome::temperature_s8_035(8.05f), "temperature", 8.05, 0.175);
    sensor_case("count_s8", BTHome::count_s8(-25), "count", -25, 0.5);
    sensor_case("count_s16", BTHome::count_s16(-22), "count", -22, 0.5);
    sensor_case("count_s32", BTHome::count_s32(-105), "count", -105, 0.5);
    sensor_case("power_s32", BTHome::power_s32(-69.14f), "power", -69.14, 0.005);
    sensor_case("current_s16", BTHome::current_s16(-1.234f), "current", -1.234, 0.0005);
    sensor_case("direction", BTHome::direction(90.0f), "direction", 90.0, 0.005);
    sensor_case("precipitation", BTHome::precipitation(12.5f), "precipitation", 12.5, 0.05);
    sensor_case("channel", BTHome::channel(5), "channel", 5, 0.5);
    sensor_case("rotational_speed", BTHome::rotational_speed(1500.0f), "rotational_speed", 1500, 0.5);
    sensor_case("speed_s32", BTHome::speed_s32(1.0f), "speed", 1.0, 0.0000005);
    sensor_case("acceleration_s32", BTHome::acceleration_s32(-1.0f), "acceleration", -1.0, 0.0000005);
    sensor_case("light_level", BTHome::light_level(200), "light_level", 200, 0.5);
    sensor_case("settings_revision", BTHome::settings_revision(3), "settings_revision", 3, 0.5);

    // --- Binary sensors, both states each.
    // Each factory has its own return type (Typed<Id> carries the object id),
    // so they cannot share a function-pointer type; this adapter gives them
    // one.
    struct BinaryEntry
    {
        const char *name;
        BTHome::Measurement (*factory)(bool);
    };
    const BinaryEntry binaries[] = {
        {"generic_boolean", &as_measurement<BTHome::generic_boolean>},
        {"power_state", &as_measurement<BTHome::power_state>},
        {"opening", &as_measurement<BTHome::opening>},
        {"battery_low", &as_measurement<BTHome::battery_low>},
        {"battery_charging", &as_measurement<BTHome::battery_charging>},
        {"carbon_monoxide", &as_measurement<BTHome::carbon_monoxide>},
        {"cold", &as_measurement<BTHome::cold>},
        {"connectivity", &as_measurement<BTHome::connectivity>},
        {"door", &as_measurement<BTHome::door>},
        {"garage_door", &as_measurement<BTHome::garage_door>},
        {"gas_detected", &as_measurement<BTHome::gas_detected>},
        {"heat", &as_measurement<BTHome::heat>},
        {"light", &as_measurement<BTHome::light>},
        {"lock", &as_measurement<BTHome::lock>},
        {"moisture_detected", &as_measurement<BTHome::moisture_detected>},
        {"motion", &as_measurement<BTHome::motion>},
        {"moving", &as_measurement<BTHome::moving>},
        {"occupancy", &as_measurement<BTHome::occupancy>},
        {"plug", &as_measurement<BTHome::plug>},
        {"presence", &as_measurement<BTHome::presence>},
        {"problem", &as_measurement<BTHome::problem>},
        {"running", &as_measurement<BTHome::running>},
        {"safety", &as_measurement<BTHome::safety>},
        {"smoke", &as_measurement<BTHome::smoke>},
        {"sound", &as_measurement<BTHome::sound>},
        {"tamper", &as_measurement<BTHome::tamper>},
        {"vibration", &as_measurement<BTHome::vibration>},
        {"window", &as_measurement<BTHome::window>},
    };
    for (const BinaryEntry &b : binaries)
    {
        char name[64];
        snprintf(name, sizeof(name), "%s_true", b.name);
        binary_case(name, b.factory(true), true);
        snprintf(name, sizeof(name), "%s_false", b.name);
        binary_case(name, b.factory(false), false);
    }

    // --- Events.
    struct ButtonEntry
    {
        const char *event_type; // bthome-ble's event_type string
        BTHome::ButtonEventType code;
    };
    const ButtonEntry buttons[] = {
        {"press", BTHome::ButtonEventType::Press},
        {"double_press", BTHome::ButtonEventType::DoublePress},
        {"triple_press", BTHome::ButtonEventType::TriplePress},
        {"long_press", BTHome::ButtonEventType::LongPress},
        {"long_double_press", BTHome::ButtonEventType::LongDoublePress},
        {"long_triple_press", BTHome::ButtonEventType::LongTriplePress},
        {"hold_press", BTHome::ButtonEventType::HoldPress},
    };
    for (const ButtonEntry &b : buttons)
    {
        BTHome::Packet<31> p;
        p.add(BTHome::button_event(b.code));
        char name[64];
        char expected[128];
        snprintf(name, sizeof(name), "button_%s", b.event_type);
        snprintf(expected, sizeof(expected),
                      "{\"kind\":\"event\",\"key\":\"button\",\"value\":\"%s\"}", b.event_type);
        emit_packet(name, p, expected);
    }

    {
        // Spec example 3A 00 3A 01: no event for button 1, press for button 2.
        BTHome::Packet<31> p;
        p.add(BTHome::button_event(BTHome::ButtonEventType::None));
        p.add(BTHome::button_event(BTHome::ButtonEventType::Press));
        emit_packet("button2_press", p,
                    "{\"kind\":\"event\",\"key\":\"button_2\",\"value\":\"press\"}");
    }

    {
        BTHome::Packet<31> p;
        p.add(BTHome::dimmer_event(BTHome::DimmerEventType::RotateLeft, 3));
        emit_packet("dimmer_rotate_left_3", p,
                    "{\"kind\":\"event\",\"key\":\"dimmer\",\"value\":\"rotate_left\",\"steps\":3}");
    }
    {
        BTHome::Packet<31> p;
        p.add(BTHome::dimmer_event(BTHome::DimmerEventType::RotateRight, 10));
        emit_packet("dimmer_rotate_right_10", p,
                    "{\"kind\":\"event\",\"key\":\"dimmer\",\"value\":\"rotate_right\",\"steps\":10}");
    }
    {
        // Multi-dimmer padding: None for dimmer 1 (must be 3C 00 00 - a
        // 1-byte None desynchronises the parser), rotate for dimmer 2.
        BTHome::Packet<31> p;
        p.add(BTHome::dimmer_event(BTHome::DimmerEventType::None));
        p.add(BTHome::dimmer_event(BTHome::DimmerEventType::RotateRight, 1));
        emit_packet("dimmer2_rotate_right", p,
                    "{\"kind\":\"event\",\"key\":\"dimmer_2\",\"value\":\"rotate_right\",\"steps\":1}");
    }
    {
        // Regression for the 1-byte None bug: sensors sharing a packet with a
        // None dimmer event must still be parsed.
        BTHome::Packet<31> p;
        p.add(BTHome::packet_id(123));
        p.add(BTHome::battery(52));
        p.add(BTHome::dimmer_event(BTHome::DimmerEventType::None));
        emit_packet("sensors_with_dimmer_none", p,
                    "{\"kind\":\"sensor\",\"key\":\"battery\",\"value\":52,\"tolerance\":0.5}");
    }

    {
        BTHome::Packet<31> p;
        p.add(BTHome::command_event(BTHome::CommandEventType::Toggle));
        emit_packet("command_toggle", p,
                    "{\"kind\":\"event\",\"key\":\"command\",\"value\":\"toggle\"}");
    }
    {
        BTHome::Packet<31> p;
        p.add(BTHome::command_event(BTHome::CommandEventType::Off));
        emit_packet("command_off", p,
                    "{\"kind\":\"event\",\"key\":\"command\",\"value\":\"off\"}");
    }
    {
        BTHome::Packet<31> p;
        p.add(BTHome::command_event(BTHome::CommandEventType::On));
        emit_packet("command_on", p,
                    "{\"kind\":\"event\",\"key\":\"command\",\"value\":\"on\"}");
    }
    {
        BTHome::Packet<31> p;
        p.add(BTHome::command_event(BTHome::CommandEventType::StepUp, 5));
        emit_packet("command_step_up_5", p,
                    "{\"kind\":\"event\",\"key\":\"command\",\"value\":\"step_up\",\"args\":\"05\"}");
    }
    {
        BTHome::Packet<31> p;
        p.add(BTHome::command_event(BTHome::CommandEventType::StepDown, 5));
        emit_packet("command_step_down_5", p,
                    "{\"kind\":\"event\",\"key\":\"command\",\"value\":\"step_down\",\"args\":\"05\"}");
    }

    {
        // Trigger-based device flag must not confuse the parser.
        BTHome::Packet<31> p;
        p.setTriggerBased(true);
        p.add(BTHome::button_event(BTHome::ButtonEventType::Press));
        emit_packet("trigger_based_button", p,
                    "{\"kind\":\"event\",\"key\":\"button\",\"value\":\"press\"}");
    }

    // --- Variable-length objects.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::text("Hello W\xC3\xB6rld"));
        emit_packet("text_utf8", p,
                    "{\"kind\":\"text\",\"value\":\"Hello W\\u00f6rld\"}");
    }
    {
        const uint8_t bytes[] = {0x01, 0x02, 0x03, 0x04};
        BTHome::Packet<31> p;
        p.add(BTHome::raw(bytes, sizeof(bytes)));
        emit_packet("raw_bytes", p, "{\"kind\":\"raw\",\"value\":\"01020304\"}");
    }

    // --- Device information objects.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::firmware_version_u32(0x04020100u));
        emit_packet("firmware_version_u32", p,
                    "{\"kind\":\"sw_version\",\"value\":\"4.2.1.0\"}");
    }
    {
        BTHome::Packet<31> p;
        p.add(BTHome::firmware_version_u24(0x060100u));
        emit_packet("firmware_version_u24", p,
                    "{\"kind\":\"sw_version\",\"value\":\"6.1.0\"}");
    }
    {
        // The parser accepts but does not surface the device type id.
        BTHome::Packet<31> p;
        p.add(BTHome::device_type_id(1));
        emit_packet("device_type_id", p, "{\"kind\":\"no_crash\"}");
    }

    // --- Combined packet: packet id + measurements, checks canonical ordering
    // and multi-value parsing in one advertisement.
    {
        BTHome::Packet<31> p;
        p.add(BTHome::humidity(54.2f));
        p.add(BTHome::packet_id(7));
        p.add(BTHome::battery(87));
        p.add(BTHome::temperature(21.53f));
        emit_packet("combined_packet", p,
                    "{\"kind\":\"sensor\",\"key\":\"temperature\",\"value\":21.53,\"tolerance\":0.005},"
                    "{\"kind\":\"sensor\",\"key\":\"humidity\",\"value\":54.2,\"tolerance\":0.005},"
                    "{\"kind\":\"sensor\",\"key\":\"battery\",\"value\":87,\"tolerance\":0.5}");
    }

    // --- Encrypted cases (AES-CCM, official spec key and MAC).
    {
        // The exact spec vector: counter 0x00112233, temperature + humidity.
        BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
        encryptor.setKey(kSpecKey);
        encryptor.setMac(kSpecMac);
        encryptor.setCounter(0x00112233u);
        BTHome::EncryptedPacket<28> p;
        p.add(BTHome::temperature(25.06f));
        p.add(BTHome::humidity(50.55f));
        emit_encrypted("encrypted_spec_vector", p, encryptor,
                       "{\"kind\":\"sensor\",\"key\":\"temperature\",\"value\":25.06,\"tolerance\":0.005},"
                       "{\"kind\":\"sensor\",\"key\":\"humidity\",\"value\":50.55,\"tolerance\":0.005}");
    }
    {
        // Two packets through ONE Encryptor and ONE parser session: the parser
        // must accept the increasing counter (replay protection).
        BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
        encryptor.setKey(kSpecKey);
        encryptor.setMac(kSpecMac);
        encryptor.setCounter(1000);

        BTHome::EncryptedPacket<28> p1;
        p1.add(BTHome::packet_id(1));
        p1.add(BTHome::temperature(20.0f));
        emit_encrypted("encrypted_seq_1", p1, encryptor,
                       "{\"kind\":\"sensor\",\"key\":\"temperature\",\"value\":20.0,\"tolerance\":0.005}",
                       "enc-seq");

        BTHome::EncryptedPacket<28> p2;
        p2.add(BTHome::packet_id(2));
        p2.add(BTHome::motion(true));
        emit_encrypted("encrypted_seq_2", p2, encryptor,
                       "{\"kind\":\"binary\",\"value\":true}",
                       "enc-seq");
    }
    {
        // Encrypted trigger-based event (device info 0x45): the device-info
        // byte is part of the nonce, so this exercises that path end to end.
        BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
        encryptor.setKey(kSpecKey);
        encryptor.setMac(kSpecMac);
        encryptor.setCounter(5000);
        BTHome::EncryptedPacket<28> p;
        p.setTriggerBased(true);
        p.add(BTHome::button_event(BTHome::ButtonEventType::Press));
        emit_encrypted("encrypted_trigger_button", p, encryptor,
                       "{\"kind\":\"event\",\"key\":\"button\",\"value\":\"press\"}");
    }

    return 0;
}
