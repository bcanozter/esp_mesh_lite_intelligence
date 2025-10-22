#ifndef __NIMBLE_H__
#define __NIMBLE_H__

/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"

#if MYNEWT_VAL(BLE_GATT_CACHING)
#include "host/ble_esp_gattc_cache.h"
#endif

#include "modlog/modlog.h"
#include "esp_central.h"
#ifdef __cplusplus
extern "C"
{
#endif

    struct ble_hs_adv_fields;
    struct ble_gap_conn_desc;
    struct ble_hs_cfg;
    union ble_store_value;
    union ble_store_key;

    int blecent_gap_event(struct ble_gap_event *event, void *arg);
    void blecent_scan(void);
    void ble_store_config_init(void);
    esp_err_t init_nimble(void);
#define BLECENT_SVC_ALERT_UUID 0x1811
#define BLECENT_CHR_SUP_NEW_ALERT_CAT_UUID 0x2A47
#define BLECENT_CHR_NEW_ALERT 0x2A46
#define BLECENT_CHR_SUP_UNR_ALERT_CAT_UUID 0x2A48
#define BLECENT_CHR_UNR_ALERT_STAT_UUID 0x2A45
#define BLECENT_CHR_ALERT_NOT_CTRL_PT 0x2A44

#ifdef __cplusplus
}
#endif

#endif