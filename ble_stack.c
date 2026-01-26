#include "ble_stack.h"

#define APP_BLE_CONN_CFG_TAG            1                                       /**< A tag identifying the SoftDevice BLE configuration. */
ble_gap_adv_params_t adv_params;
static ble_gap_scan_params_t m_scan_param;                 /**< Scan parameters requested for scanning and connection. */

#define SCAN_INTERVAL               0x00A0                              /**< Determines scan interval in units of 0.625 millisecond. */
#define SCAN_WINDOW                 0x00A0                              /**< Determines scan window in units of 0.625 millisecond. (100% duty cycle) */

// Config mode definitions
#define DEVICE_NAME                 "HeyStack-Config"
#define MIN_CONN_INTERVAL           MSEC_TO_UNITS(100, UNIT_1_25_MS)
#define MAX_CONN_INTERVAL           MSEC_TO_UNITS(200, UNIT_1_25_MS)
#define SLAVE_LATENCY               0
#define CONN_SUP_TIMEOUT            MSEC_TO_UNITS(4000, UNIT_10_MS)
#define CONFIG_ADV_INTERVAL         MSEC_TO_UNITS(100, UNIT_0_625_MS)

#if NRF_SDK_VERSION >= 15
uint8_t adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
#else
#endif

// Module variables for config mode
static ble_operating_mode_t m_current_mode = BLE_MODE_CONFIG;
static config_service_t     m_config_service;
static uint16_t             m_conn_handle = BLE_CONN_HANDLE_INVALID;

uint8_t status_flag = 0;
uint8_t bt_addr[6] = {0xFF, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
uint8_t offline_finding_adv[] = {
	0x1e,		/* Length (30) */
	0xff,		/* Manufacturer Specific Data (type 0xff) */
	0x4c, 0x00, /* Company ID (Apple) */
	0x12, 0x19, /* Offline Finding type and length */
	0x00,		/* State */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, /* First two bits */
	0x00, /* Hint (0x00) */
};
size_t offline_finding_adv_len = sizeof(offline_finding_adv);


// Set maximum transmit power for advertising or connection
void ble_set_max_tx_power(void)
{
    uint32_t err_code;
    // Set the transmit power to the maximum allowed by the hardware.
    static int8_t max_tx_power = -1;

    if (max_tx_power == -1) {
        int8_t powers[] = { 8, 7, 6, 5, 4 };  // List of possible power levels
        size_t num_powers = sizeof(powers) / sizeof(powers[0]);

        for (size_t i = 0; i < num_powers; i++) {
            int8_t tx_power = powers[i];
            #if NRF_SDK_VERSION >= 15
            err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, adv_handle, tx_power);
            #else
            err_code = sd_ble_gap_tx_power_set(tx_power);
            #endif

            if (err_code == NRF_SUCCESS) {
                max_tx_power = tx_power;
                COMPAT_NRF_LOG_INFO("ble_set_max_tx_power: %d dBm", max_tx_power);
                break;
            } else {
                COMPAT_NRF_LOG_INFO("ble_set_max_tx_power: %d dBm failed", tx_power);
            }
        }

        // If none of the power levels succeeded, handle the error
        if (max_tx_power == -1) {
            APP_ERROR_CHECK(err_code);  // Will trigger an error since err_code is not NRF_SUCCESS
        }
    } else {
        // max_tx_power has been determined previously, set it directly
        #if NRF_SDK_VERSION >= 15
        err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, adv_handle, max_tx_power);
        #else
        err_code = sd_ble_gap_tx_power_set(max_tx_power);
        #endif
        APP_ERROR_CHECK(err_code);
    }
}

/*
 * set_addr_from_key will set the bluetooth address from the first 6 bytes of the key used to be advertised
 */
static void set_addr_from_key(const char *key)
{
	/* copy first 6 bytes */
	bt_addr[5] = key[0] | 0b11000000;
	bt_addr[4] = key[1];
	bt_addr[3] = key[2];
	bt_addr[2] = key[3];
	bt_addr[1] = key[4];
	bt_addr[0] = key[5];
}

/*
 * fill_adv_template_from_key will set the advertising data based on the remaining bytes from the advertised key
 */
static void fill_adv_template_from_key(const char *key)
{
	memcpy(&offline_finding_adv[7], &key[6], 22);
	offline_finding_adv[29] = key[0] >> 6;
}

/**
 * Set the Bluetooth MAC address.
 */
static void ble_set_mac_address(uint8_t *addr)
{
    ble_gap_addr_t gap_addr;
    uint32_t err_code;

    // Copy the address to the gap_addr structure.
    memcpy(gap_addr.addr, addr, sizeof(gap_addr.addr));

    // Set the address type. This can be either public or random static.
    gap_addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;

    // Set the address using the SoftDevice API.
    #if NRF_SDK_VERSION >= 15
        err_code = sd_ble_gap_addr_set(&gap_addr);
        APP_ERROR_CHECK(err_code);
    #else  // For SDK 12 and earlier
        // In SDK 12 and earlier, the SoftDevice address is set in a similar way.
        err_code = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &gap_addr);
        APP_ERROR_CHECK(err_code);
    #endif

    COMPAT_NRF_LOG_INFO("ble_set_mac_address: %02x:%02x:%02x:%02x:%02x:%02x",
            addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    
    // Check for any errors.
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
void ble_advertising_init(void)
{
    memset(&adv_params, 0, sizeof(adv_params));

    #if NRF_SDK_VERSION >= 15
        // Set the advertising type to non-connectable.
        adv_params.properties.type = BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
        // Set advertising interval (in 0.625 ms units).
        adv_params.interval = MSEC_TO_UNITS(ADVERTISING_INTERVAL, UNIT_0_625_MS);
        // Set advertising timeout to zero (no timeout).
        adv_params.duration = 0;
        // Set the filter policy to allow all.
        adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
        // No specific peer address.
        adv_params.p_peer_addr = NULL;
        // Set primary PHY (1M) and secondary PHY (not used in this case).
        adv_params.primary_phy = BLE_GAP_PHY_1MBPS;

        // Call the API to configure and start advertising.
        int err_code = sd_ble_gap_adv_set_configure(&adv_handle, NULL, &adv_params);
        APP_ERROR_CHECK(err_code);
    #else
        // Set the advertising parameters.
        adv_params.type = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
        adv_params.p_peer_addr = NULL;
        adv_params.fp = BLE_GAP_ADV_FP_ANY;
        // Set the advertising interval (in units of 0.625 ms).
        adv_params.interval = MSEC_TO_UNITS(ADVERTISING_INTERVAL, UNIT_0_625_MS);
        adv_params.timeout = 0;
        sd_ble_gap_adv_start(&adv_params);
    #endif
}

/*
 * set_advertisement_key will setup the key to be advertised
 *
 * @param[in] key public key to be advertised
 *
 * @returns raw data size
 */
uint8_t ble_set_advertisement_key(const char *key)
{

    #if NRF_SDK_VERSION >= 15
        if (adv_handle != BLE_GAP_ADV_SET_HANDLE_NOT_SET) {
            int err_code = sd_ble_gap_adv_stop(adv_handle);
            if (err_code != NRF_ERROR_INVALID_STATE) // Invalid state is fine if no advertisement is running
            {
                APP_ERROR_CHECK(err_code);
            }
        }
    #endif

    set_addr_from_key(key);
   	fill_adv_template_from_key(key);

	ble_set_mac_address(bt_addr);

    #if NRF_SDK_VERSION >= 15
        // Set advertising data
	    ble_gap_adv_data_t adv_data;
        memset(&adv_data, 0, sizeof(adv_data));
        // Set advertising data
        adv_data.adv_data.p_data = offline_finding_adv;
        adv_data.adv_data.len = offline_finding_adv_len;
        // No scan response data in this case (NULL and length 0).
        adv_data.scan_rsp_data.p_data = NULL;
        adv_data.scan_rsp_data.len = 0;
        // Initialize advertising parameters as before (assumed to be done previously)
        uint32_t err_code = sd_ble_gap_adv_set_configure(&adv_handle, &adv_data, &adv_params);
        APP_ERROR_CHECK(err_code);

        // Start advertising (also assumed done previously)
        err_code = sd_ble_gap_adv_start(adv_handle, APP_BLE_CONN_CFG_TAG);
        APP_ERROR_CHECK(err_code);
    #else
        uint32_t err_code = sd_ble_gap_adv_data_set(offline_finding_adv, offline_finding_adv_len, NULL, 0);
	    APP_ERROR_CHECK(err_code);
    #endif

    // Set the maximum transmit power for advertising.
    ble_set_max_tx_power();

	return offline_finding_adv_len;
}

void _set_status(uint8_t status)
{
	offline_finding_adv[6] = status;
}

void set_battery(uint8_t battery_level)
{
    status_flag &= (~STATUS_FLAG_BATTERY_MASK);
    if(battery_level > 80){
        // do nothing
    }else if(battery_level > 50){
        status_flag |= STATUS_FLAG_MEDIUM_BATTERY;
    }else if(battery_level > 30){
        status_flag |= STATUS_FLAG_LOW_BATTERY;
    }else{
        status_flag |= STATUS_FLAG_CRITICALLY_LOW_BATTERY;
    }
    COMPAT_NRF_LOG_INFO("Battery level: %d, status: %d%d",
            battery_level, (status_flag >> 7) & 1, (status_flag >> 6) & 1);
	_set_status(status_flag);
}

void set_status(uint8_t status)
{
	status_flag &= (~STATUS_FLAG_COUNTER_MASK);
	status_flag |= status;

	_set_status(status_flag);
}

void set_raw_status(uint8_t raw_status)
{
	status_flag = raw_status;
	_set_status(status_flag);
}

void scan_start(void)
{
    ret_code_t ret;

    m_scan_param.active   = 0;
    m_scan_param.interval = SCAN_INTERVAL;
    m_scan_param.window   = SCAN_WINDOW;


    #if (NRF_SD_BLE_API_VERSION == 2)
        m_scan_param.selective   = 0;
        m_scan_param.p_whitelist = NULL;
    #endif
    #if (NRF_SD_BLE_API_VERSION == 3)
        m_scan_param.use_whitelist  = 0;
        m_scan_param.adv_dir_report = 0;
    #endif
    m_scan_param.timeout  = 0x0000; // No timeout.

    ret = sd_ble_gap_scan_start(&m_scan_param);
    COMPAT_NRF_LOG_INFO("scan_start ret: %d", ret);
    APP_ERROR_CHECK(ret);

}

static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    COMPAT_NRF_LOG_INFO("on_ble_evt: %d", p_ble_evt->header.evt_id);

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            COMPAT_NRF_LOG_INFO("Connected");
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            COMPAT_NRF_LOG_INFO("Disconnected");
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // No security required - reject pairing
            COMPAT_NRF_LOG_INFO("Rejecting pairing request");
            sd_ble_gap_sec_params_reply(p_ble_evt->evt.gap_evt.conn_handle,
                                        BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
                                        NULL, NULL);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes stored
            sd_ble_gatts_sys_attr_set(p_ble_evt->evt.gap_evt.conn_handle, NULL, 0, 0);
            break;

        case BLE_GAP_EVT_ADV_REPORT:
        {
            const ble_gap_evt_adv_report_t * p_adv = &p_ble_evt->evt.gap_evt.params.adv_report;

            COMPAT_NRF_LOG_INFO("ADV: %02x:%02x:%02x:%02x:%02x:%02x",
                p_adv->peer_addr.addr[5], p_adv->peer_addr.addr[4],
                p_adv->peer_addr.addr[3], p_adv->peer_addr.addr[2],
                p_adv->peer_addr.addr[1], p_adv->peer_addr.addr[0]);
            COMPAT_NRF_LOG_INFO("  RSSI:%d len:%d", p_adv->rssi, p_adv->dlen);
        } break;

        default:
            break;
    }
}

void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    COMPAT_NRF_LOG_INFO("BLE evt: %d", p_ble_evt->header.evt_id);
    on_ble_evt(p_ble_evt);

    // Forward events to config service when in config mode
    if (m_current_mode == BLE_MODE_CONFIG)
    {
        config_service_on_ble_evt(&m_config_service, p_ble_evt);
    }
}

/**
 * @brief Initialize GAP parameters for connectable advertising.
 */
void ble_gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);

    COMPAT_NRF_LOG_INFO("GAP params initialized");
}

/**
 * @brief Start connectable advertising for configuration mode.
 */
void ble_start_config_advertising(void)
{
    uint32_t err_code;

    m_current_mode = BLE_MODE_CONFIG;

    #if NRF_SDK_VERSION >= 15
        // Stop any existing advertising
        if (adv_handle != BLE_GAP_ADV_SET_HANDLE_NOT_SET)
        {
            err_code = sd_ble_gap_adv_stop(adv_handle);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
        }

        // Set up advertising data
        static uint8_t adv_data[] = {
            0x02, 0x01, 0x06,  // Flags: General Discoverable, BR/EDR not supported
            0x10, 0x09,        // Complete Local Name length and type
            'H', 'e', 'y', 'S', 't', 'a', 'c', 'k', '-', 'C', 'o', 'n', 'f', 'i', 'g'
        };

        ble_gap_adv_data_t gap_adv_data;
        memset(&gap_adv_data, 0, sizeof(gap_adv_data));
        gap_adv_data.adv_data.p_data = adv_data;
        gap_adv_data.adv_data.len = sizeof(adv_data);

        // Set up advertising parameters for connectable mode
        ble_gap_adv_params_t config_adv_params;
        memset(&config_adv_params, 0, sizeof(config_adv_params));
        config_adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
        config_adv_params.interval = CONFIG_ADV_INTERVAL;
        config_adv_params.duration = 0;  // No timeout
        config_adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
        config_adv_params.primary_phy = BLE_GAP_PHY_1MBPS;

        err_code = sd_ble_gap_adv_set_configure(&adv_handle, &gap_adv_data, &config_adv_params);
        APP_ERROR_CHECK(err_code);

        err_code = sd_ble_gap_adv_start(adv_handle, APP_BLE_CONN_CFG_TAG);
        APP_ERROR_CHECK(err_code);
    #else
        // SDK 12 advertising setup
        ble_advdata_t advdata;
        memset(&advdata, 0, sizeof(advdata));

        advdata.name_type = BLE_ADVDATA_FULL_NAME;
        advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

        err_code = ble_advdata_set(&advdata, NULL);
        APP_ERROR_CHECK(err_code);

        ble_gap_adv_params_t config_adv_params;
        memset(&config_adv_params, 0, sizeof(config_adv_params));
        config_adv_params.type = BLE_GAP_ADV_TYPE_ADV_IND;  // Connectable undirected
        config_adv_params.fp = BLE_GAP_ADV_FP_ANY;
        config_adv_params.interval = CONFIG_ADV_INTERVAL;
        config_adv_params.timeout = 0;

        err_code = sd_ble_gap_adv_start(&config_adv_params);
        APP_ERROR_CHECK(err_code);
    #endif

    COMPAT_NRF_LOG_INFO("Config advertising started");
}

/**
 * @brief Stop advertising.
 */
void ble_stop_advertising(void)
{
    uint32_t err_code;

    #if NRF_SDK_VERSION >= 15
        if (adv_handle != BLE_GAP_ADV_SET_HANDLE_NOT_SET)
        {
            err_code = sd_ble_gap_adv_stop(adv_handle);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
        }
    #else
        err_code = sd_ble_gap_adv_stop();
        if (err_code != NRF_ERROR_INVALID_STATE)
        {
            APP_ERROR_CHECK(err_code);
        }
    #endif

    COMPAT_NRF_LOG_INFO("Advertising stopped");
}

/**
 * @brief Switch to offline finding mode (non-connectable).
 */
void ble_switch_to_offline_mode(void)
{
    uint32_t err_code;

    // Disconnect any active connection
    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        if (err_code != NRF_ERROR_INVALID_STATE)
        {
            APP_ERROR_CHECK(err_code);
        }
        m_conn_handle = BLE_CONN_HANDLE_INVALID;
    }

    // Stop config advertising
    ble_stop_advertising();

    // Update mode
    m_current_mode = BLE_MODE_OFFLINE;

    // Initialize non-connectable advertising parameters
    ble_advertising_init();

    COMPAT_NRF_LOG_INFO("Switched to offline mode");
}

/**
 * @brief Get the current operating mode.
 */
ble_operating_mode_t ble_get_current_mode(void)
{
    return m_current_mode;
}

/**
 * @brief Get pointer to the config service instance.
 */
config_service_t* ble_get_config_service(void)
{
    return &m_config_service;
}