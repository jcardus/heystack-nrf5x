#ifndef BLE_STACK_H__
#define BLE_STACK_H__

#include <stdint.h>
#include <string.h>

#include "app_error.h"
#include "ble.h"
#include "ble_err.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_conn_params.h"

#include "nrf5x-compat.h"
#include "config_service.h"

#if NRF_SDK_VERSION < 15
#define APP_TIMER_PRESCALER 31
#define APP_TIMER_MAX_TIMERS 1
#define APP_TIMER_OP_QUEUE_SIZE 4
#include "softdevice_handler.h"
#endif

#if defined(NRF_SD_BLE_API_VERSION) && NRF_SD_BLE_API_VERSION > 3
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"
#include "ble_lbs.h"
#endif 

#include "boards.h"
#include "app_timer.h"
#include "app_button.h"

#define APP_BLE_CONN_CFG_TAG            1                                       /**< A tag identifying the SoftDevice BLE configuration. */

#define STATUS_FLAG_BATTERY_MASK           0b11000000
#define STATUS_FLAG_COUNTER_MASK           0b00111111
#define STATUS_FLAG_MEDIUM_BATTERY         0b01000000
#define STATUS_FLAG_LOW_BATTERY            0b10000000
#define STATUS_FLAG_CRITICALLY_LOW_BATTERY 0b11000000

#ifndef ADVERTISING_INTERVAL
#define ADVERTISING_INTERVAL 1000
#endif

// Operating mode enum
typedef enum {
    BLE_MODE_CONFIG,    // Connectable configuration mode (on boot)
    BLE_MODE_OFFLINE    // Non-connectable offline finding mode
} ble_operating_mode_t;

void ble_advertising_init(void);
void ble_set_max_tx_power(void);
void set_battery(uint8_t battery_level);
uint8_t ble_set_advertisement_key(const char *key);
void scan_start(void);
void ble_evt_dispatch(ble_evt_t * p_ble_evt);

// Config mode functions
void ble_gap_params_init(void);
void ble_start_config_advertising(void);
void ble_stop_advertising(void);
void ble_switch_to_offline_mode(void);
ble_operating_mode_t ble_get_current_mode(void);
config_service_t* ble_get_config_service(void);

#endif // BLE_STACK_H__