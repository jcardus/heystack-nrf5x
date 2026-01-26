#include "config_service.h"
#include <string.h>
#include "app_error.h"

/**
 * @brief Handle write events to the Key Write characteristic.
 */
static void on_write(config_service_t *p_service, ble_evt_t *p_ble_evt)
{
    ble_gatts_evt_write_t *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    // Check if this write is to the Key Write characteristic
    if (p_evt_write->handle == p_service->key_write_handles.value_handle)
    {
        // Validate length
        if (p_evt_write->len == CONFIG_SERVICE_KEY_LENGTH)
        {
            // Call the application callback
            if (p_service->key_write_handler != NULL)
            {
                p_service->key_write_handler(p_service, p_evt_write->data, p_evt_write->len);
            }
        }
        else
        {
            COMPAT_NRF_LOG_INFO("Config: Invalid key length %d (expected %d)",
                               p_evt_write->len, CONFIG_SERVICE_KEY_LENGTH);
        }
    }
}

/**
 * @brief Add the Key Write characteristic to the service.
 */
static uint32_t key_write_char_add(config_service_t *p_service)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));
    memset(&attr_md, 0, sizeof(attr_md));
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    // Characteristic metadata: write-only, no read
    char_md.char_props.write         = 1;
    char_md.char_props.write_wo_resp = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    // UUID
    ble_uuid.type = p_service->uuid_type;
    ble_uuid.uuid = CONFIG_SERVICE_UUID_KEY_WRITE;

    // Attribute metadata: open permissions (no security)
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 0;

    // Attribute value
    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = CONFIG_SERVICE_KEY_LENGTH;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = CONFIG_SERVICE_KEY_LENGTH;
    attr_char_value.p_value   = NULL;

    return sd_ble_gatts_characteristic_add(p_service->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_service->key_write_handles);
}

/**
 * @brief Add the Key Count characteristic to the service.
 */
static uint32_t key_count_char_add(config_service_t *p_service, uint16_t initial_count)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));
    memset(&attr_md, 0, sizeof(attr_md));
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    // Characteristic metadata: read-only
    char_md.char_props.read          = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    // UUID
    ble_uuid.type = p_service->uuid_type;
    ble_uuid.uuid = CONFIG_SERVICE_UUID_KEY_COUNT;

    // Attribute metadata: open read permission
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 0;

    // Attribute value (2 bytes for key count)
    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(uint16_t);
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = sizeof(uint16_t);
    attr_char_value.p_value   = (uint8_t *)&initial_count;

    return sd_ble_gatts_characteristic_add(p_service->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_service->key_count_handles);
}

uint32_t config_service_init(config_service_t *p_service, const config_service_init_t *p_init)
{
    uint32_t   err_code;
    ble_uuid_t ble_uuid;

    if (p_service == NULL || p_init == NULL)
    {
        return NRF_ERROR_NULL;
    }

    // Initialize service structure
    p_service->conn_handle       = BLE_CONN_HANDLE_INVALID;
    p_service->key_write_handler = p_init->key_write_handler;
    p_service->key_count         = p_init->initial_key_count;

    // Add custom UUID base
    ble_uuid128_t base_uuid = {CONFIG_SERVICE_UUID_BASE};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_service->uuid_type);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Set up service UUID
    ble_uuid.type = p_service->uuid_type;
    ble_uuid.uuid = CONFIG_SERVICE_UUID_SERVICE;

    // Add service to GATT server
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_service->service_handle);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add Key Write characteristic
    err_code = key_write_char_add(p_service);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add Key Count characteristic
    err_code = key_count_char_add(p_service, p_init->initial_key_count);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    COMPAT_NRF_LOG_INFO("Config Service initialized");

    return NRF_SUCCESS;
}

void config_service_on_ble_evt(config_service_t *p_service, ble_evt_t *p_ble_evt)
{
    if (p_service == NULL || p_ble_evt == NULL)
    {
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            p_service->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            COMPAT_NRF_LOG_INFO("Config: Connected");
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            p_service->conn_handle = BLE_CONN_HANDLE_INVALID;
            COMPAT_NRF_LOG_INFO("Config: Disconnected");
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_service, p_ble_evt);
            break;

        default:
            break;
    }
}

uint32_t config_service_update_key_count(config_service_t *p_service, uint16_t key_count)
{
    ble_gatts_value_t gatts_value;

    if (p_service == NULL)
    {
        return NRF_ERROR_NULL;
    }

    p_service->key_count = key_count;

    memset(&gatts_value, 0, sizeof(gatts_value));
    gatts_value.len     = sizeof(uint16_t);
    gatts_value.offset  = 0;
    gatts_value.p_value = (uint8_t *)&key_count;

    return sd_ble_gatts_value_set(p_service->conn_handle,
                                  p_service->key_count_handles.value_handle,
                                  &gatts_value);
}
