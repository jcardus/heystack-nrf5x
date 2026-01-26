#ifndef CONFIG_SERVICE_H__
#define CONFIG_SERVICE_H__

#include <stdint.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf5x-compat.h"

// Custom UUID base: 0000xxxx-0000-1000-8000-00805F9B34FB (Bluetooth SIG base)
// Service UUID: 0xFF00
// Characteristic UUIDs: 0xFF01 (Key Write), 0xFF02 (Key Count)

#define CONFIG_SERVICE_UUID_BASE         {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, \
                                          0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

#define CONFIG_SERVICE_UUID_SERVICE      0xFF00
#define CONFIG_SERVICE_UUID_KEY_WRITE    0xFF01
#define CONFIG_SERVICE_UUID_KEY_COUNT    0xFF02

#define CONFIG_SERVICE_KEY_LENGTH        28

// Forward declaration
typedef struct config_service_s config_service_t;

// Callback type for key write events
typedef void (*config_service_key_write_handler_t)(config_service_t *p_service,
                                                   const uint8_t *key_data,
                                                   uint16_t length);

// Configuration service structure
struct config_service_s
{
    uint16_t                            service_handle;
    ble_gatts_char_handles_t            key_write_handles;
    ble_gatts_char_handles_t            key_count_handles;
    uint8_t                             uuid_type;
    uint16_t                            conn_handle;
    config_service_key_write_handler_t  key_write_handler;
    uint16_t                            key_count;
};

// Initialization structure
typedef struct
{
    config_service_key_write_handler_t  key_write_handler;
    uint16_t                            initial_key_count;
} config_service_init_t;

/**
 * @brief Initialize the Configuration Service.
 *
 * @param[out] p_service      Pointer to the service structure.
 * @param[in]  p_init         Initialization parameters.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
uint32_t config_service_init(config_service_t *p_service, const config_service_init_t *p_init);

/**
 * @brief Handle BLE events for the Configuration Service.
 *
 * @param[in] p_service       Pointer to the service structure.
 * @param[in] p_ble_evt       Pointer to the BLE event.
 */
void config_service_on_ble_evt(config_service_t *p_service, ble_evt_t *p_ble_evt);

/**
 * @brief Update the key count characteristic value.
 *
 * @param[in] p_service       Pointer to the service structure.
 * @param[in] key_count       New key count value.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
uint32_t config_service_update_key_count(config_service_t *p_service, uint16_t key_count);

#endif // CONFIG_SERVICE_H__
