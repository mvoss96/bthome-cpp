// ESP-IDF (Bluedroid) example: advertise an AES-CCM encrypted BTHome payload.
//
// Shows the three device-side duties that encryption adds:
//   1. Use the SAME MAC in the nonce that the BLE stack advertises with.
//   2. Persist the counter across reboots (NVS) - receivers reject
//      non-increasing counters as replays.
//   3. Rebuild (re-encrypt) the payload for every content update; each build
//      consumes one counter value.
#include "bthome.h"
#include "bthome_crypto_mbedtls.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_mac.h"
#include "nvs.h"
#include "nvs_flash.h"

// 16-byte key, shared with Home Assistant (enter it there as 32 hex chars).
static const uint8_t kKey[BTHome::Encryptor::kKeyBytes] = {
    0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1,
    0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32};

// On boot the counter resumes at last-persisted + margin, so values consumed
// between NVS writes can never be reused after a crash or power loss.
static constexpr uint32_t kCounterMargin = 1024;

static BTHome::Encryptor s_encryptor(&BTHome::mbedtls_ccm_backend);
static nvs_handle_t s_nvs;

static void gap_cb(esp_gap_ble_cb_event_t event,
                   esp_ble_gap_cb_param_t* param) {
    if (event == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT) {
        esp_ble_adv_params_t adv_params = {};
        adv_params.adv_int_min = 0x100;  // 160 ms
        adv_params.adv_int_max = 0x100;
        adv_params.adv_type = ADV_TYPE_NONCONN_IND;  // BTHome is connectionless
        adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
        adv_params.channel_map = ADV_CHNL_ALL;
        adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
        esp_ble_gap_start_advertising(&adv_params);
    }
}

extern "C" void app_main(void) {
    nvs_flash_init();
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_ble_gap_register_callback(gap_cb);

    // 1. Nonce MAC = the Bluetooth MAC this device advertises with
    //    (BLE_ADDR_TYPE_PUBLIC above).
    uint8_t mac[BTHome::Encryptor::kMacBytes] = {};
    esp_read_mac(mac, ESP_MAC_BT);
    s_encryptor.setKey(kKey);
    s_encryptor.setMac(mac);

    // 2. Restore the counter with a safety margin and persist the new base.
    nvs_open("bthome", NVS_READWRITE, &s_nvs);
    uint32_t counter = 0;
    nvs_get_u32(s_nvs, "counter", &counter);  // stays 0 on first boot
    counter += kCounterMargin;
    s_encryptor.setCounter(counter);
    nvs_set_u32(s_nvs, "counter", counter);
    nvs_commit(s_nvs);

    // 3. Build the encrypted advertisement (consumes one counter value).
    //    Re-run this block - plus a periodic counter save - on every update.
    BTHome::EncryptedPacket<28> packet;
    packet.add(BTHome::temperature(22.4f));
    packet.add(BTHome::humidity(54.3f));
    packet.add(BTHome::battery(92));

    static uint8_t raw_adv[31];
    const int n = BTHome::build_encrypted_advertising(packet, s_encryptor, raw_adv, sizeof(raw_adv));
    if (n > 0) {
        esp_ble_gap_config_adv_data_raw(raw_adv, static_cast<uint32_t>(n));
    }
}
