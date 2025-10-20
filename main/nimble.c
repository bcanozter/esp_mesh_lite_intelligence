#include <nimble.h>
#include "esp_mac.h"
#include "esp_log.h"
static const char *TAG = "nimble";

void blecent_scan(void)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params = {0};
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGI(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 1;

    /**
     * Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.
     */
    disc_params.passive = 0;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                      blecent_gap_event, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d\n",
                 rc);
    }
}

void print_manufacturer_data(const uint8_t *data, size_t data_len)
{
    if (data == NULL || data_len < 2)
    {
        ESP_LOGI(TAG, "No manufacturer data or too short");
        return;
    }

    printf("Manufacturer Data (Hex): ");
    for (size_t i = 0; i < data_len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    uint16_t company_id = data[0] | (data[1] << 8);
    ESP_LOGI(TAG, "Manufacturer Company ID: 0x%04X", company_id);
    const uint8_t *payload = &data[2];
    int payload_len = data_len - 2;

    switch (company_id)
    {
    case 0x004C: // Apple, Inc.
    {
        ESP_LOGI(TAG, "Vendor: Apple, Inc.");

        if (payload_len < 2)
        {
            ESP_LOGI(TAG, "Payload too short for Apple format");
            return;
        }

        uint8_t type = payload[0];
        uint8_t subtype = payload[1];
        ESP_LOGI(TAG, "Type: 0x%02X  SubType: 0x%02X", type, subtype);

        // Apple BLE Type/Subtype table
        if (type == 0x02 && subtype == 0x15)
        {
            ESP_LOGI(TAG, "Detected Apple iBeacon device");
        }
        else if (type == 0x10)
        {
            switch (subtype)
            {
            case 0x05:
                ESP_LOGI(TAG, "Detected Apple AirPods — pairing/presence broadcast");
                break;
            case 0x06:
                ESP_LOGI(TAG, "Detected Apple AirPods — in-case or charging state broadcast");
                break;
            case 0x07:
                ESP_LOGI(TAG, "Detected Apple AirPods — active/connected broadcast");
                break;
            default:
                ESP_LOGI(TAG, "Detected Apple AirPods (unknown subtype 0x%02X)", subtype);
                break;
            }
        }
        else if (type == 0x12)
        {
            switch (subtype)
            {
            case 0x02:
                ESP_LOGI(TAG, "Detected Apple Continuity device (iPhone/Watch/Mac) — proximity or presence broadcast");
                break;
            case 0x03:
                ESP_LOGI(TAG, "Detected Apple Continuity device variant");
                break;
            case 0x18:
                ESP_LOGI(TAG, "Detected Apple Watch Continuity broadcast");
                break;
            case 0x19:
                ESP_LOGI(TAG, "Detected Apple Find My / AirTag broadcast");
                break;
            default:
                ESP_LOGI(TAG, "Detected Apple Continuity BLE frame (unknown subtype 0x%02X)", subtype);
                break;
            }
        }
        else
        {
            ESP_LOGI(TAG, "Unknown Apple BLE type (Type=0x%02X, SubType=0x%02X)", type, subtype);
        }

        break;
    }

    case 0xFFFF:
        ESP_LOGI(TAG, "Vendor: Reserved/Test Manufacturer (0xFFFF)");
        ESP_LOG_BUFFER_HEX(TAG, payload, payload_len);
        break;

    default:
        ESP_LOGI(TAG, "Vendor: Unknown (0x%04X)", company_id);
        ESP_LOG_BUFFER_HEX(TAG, payload, payload_len);
        break;
    }
}

void print_uuid_debug(const ble_uuid_t *uuid)
{
    char buf[BLE_UUID_STR_LEN];

    ESP_LOGI(TAG, "%s", ble_uuid_to_str(uuid, buf));
}

void print_bytes_debug(const uint8_t *bytes, int len)
{
    int i;

    for (i = 0; i < len; i++)
    {
        ESP_LOGI(TAG, "%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}
void print_fields(const struct ble_hs_adv_fields *fields)
{
    char s[BLE_HS_ADV_MAX_SZ];
    // const uint8_t *u8p;
    // int i;

    // if (fields->flags != 0)
    // {
    //     ESP_LOGI(TAG, "    flags=0x%02x\n", fields->flags);
    // }

    // if (fields->uuids16 != NULL)
    // {
    //     ESP_LOGI(TAG, "    uuids16(%scomplete)=",
    //              fields->uuids16_is_complete ? "" : "in");
    //     for (i = 0; i < fields->num_uuids16; i++)
    //     {
    //         print_uuid_debug(&fields->uuids16[i].u);
    //         ESP_LOGI(TAG, " ");
    //     }
    //     ESP_LOGI(TAG, "\n");
    // }

    // if (fields->uuids32 != NULL)
    // {
    //     ESP_LOGI(TAG, "    uuids32(%scomplete)=",
    //              fields->uuids32_is_complete ? "" : "in");
    //     for (i = 0; i < fields->num_uuids32; i++)
    //     {
    //         print_uuid_debug(&fields->uuids32[i].u);
    //         ESP_LOGI(TAG, " ");
    //     }
    //     ESP_LOGI(TAG, "\n");
    // }

    // if (fields->uuids128 != NULL)
    // {
    //     ESP_LOGI(TAG, "    uuids128(%scomplete)=",
    //              fields->uuids128_is_complete ? "" : "in");
    //     for (i = 0; i < fields->num_uuids128; i++)
    //     {
    //         print_uuid_debug(&fields->uuids128[i].u);
    //         ESP_LOGI(TAG, " ");
    //     }
    //     ESP_LOGI(TAG, "\n");
    // }

    if (fields->name != NULL)
    {
        assert(fields->name_len < sizeof s - 1);
        memcpy(s, fields->name, fields->name_len);
        s[fields->name_len] = '\0';
        ESP_LOGI(TAG, "Name(%scomplete)=%s\n",
                 fields->name_is_complete ? "" : "in", s);
    }

    // if (fields->tx_pwr_lvl_is_present)
    // {
    //     ESP_LOGI(TAG, "    tx_pwr_lvl=%d\n", fields->tx_pwr_lvl);
    // }

    // if (fields->slave_itvl_range != NULL)
    // {
    //     ESP_LOGI(TAG, "    slave_itvl_range=");
    //     print_bytes_debug(fields->slave_itvl_range, BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
    //     ESP_LOGI(TAG, "\n");
    // }

    // if (fields->svc_data_uuid16 != NULL)
    // {
    //     ESP_LOGI(TAG, "    svc_data_uuid16=");
    //     print_bytes_debug(fields->svc_data_uuid16, fields->svc_data_uuid16_len);
    //     ESP_LOGI(TAG, "\n");
    // }

    // if (fields->public_tgt_addr != NULL)
    // {
    //     ESP_LOGI(TAG, "    public_tgt_addr=");
    //     u8p = fields->public_tgt_addr;
    //     for (i = 0; i < fields->num_public_tgt_addrs; i++)
    //     {
    //         ESP_LOGI(TAG, "public_tgt_addr=%s ", addr_str(u8p));
    //         u8p += BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN;
    //     }
    //     ESP_LOGI(TAG, "\n");
    // }

    // if (fields->appearance_is_present)
    // {
    //     ESP_LOGI(TAG, "    appearance=0x%04x\n", fields->appearance);
    // }

    // if (fields->adv_itvl_is_present)
    // {
    //     ESP_LOGI(TAG, "    adv_itvl=0x%04x\n", fields->adv_itvl);
    // }

    // if (fields->svc_data_uuid32 != NULL)
    // {
    //     ESP_LOGI(TAG, "    svc_data_uuid32=");
    //     print_bytes_debug(fields->svc_data_uuid32, fields->svc_data_uuid32_len);
    //     ESP_LOGI(TAG, "\n");
    // }

    // if (fields->svc_data_uuid128 != NULL)
    // {
    //     ESP_LOGI(TAG, "    svc_data_uuid128=");
    //     print_bytes_debug(fields->svc_data_uuid128, fields->svc_data_uuid128_len);
    //     ESP_LOGI(TAG, "\n");
    // }

    // if (fields->uri != NULL)
    // {
    //     ESP_LOGI(TAG, "    uri=");
    //     print_bytes_debug(fields->uri, fields->uri_len);
    //     ESP_LOGI(TAG, "\n");
    // }

    if (fields->mfg_data != NULL)
    {
        // print_bytes_debug(fields->mfg_data, fields->mfg_data_len);
        if (fields->mfg_data_len > 0)
        {
            print_manufacturer_data(fields->mfg_data, fields->mfg_data_len);
        }
        ESP_LOGI(TAG, "\n");
    }
}

int blecent_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;
    switch (event->type)
    {
    case BLE_GAP_EVENT_DISC:
        rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                     event->disc.length_data);
        if (rc != 0)
        {
            return 0;
        }
        const ble_addr_t *addr = &event->disc.addr;

        // printf("Device rssi %d\n",event->disc.rssi);
#ifdef CONFIG_DEBUG
        if (fields.mfg_data_len > 0 && event->disc.rssi > -60)
        {
            ESP_LOGI(TAG, "Device Address: " MACSTR "\n",
                     MAC2STR(addr->val));
            print_fields(&fields);
        }
#endif
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated. */
        ESP_LOGI(TAG, "disconnect; reason=%d ", event->disconnect.reason);
        print_conn_desc(&event->disconnect.conn);
        ESP_LOGI(TAG, "\n");

        /* Forget about peer. */
        peer_delete(event->disconnect.conn.conn_handle);

        /* Resume scanning. */
        blecent_scan();
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "discovery complete; reason=%d\n",
                 event->disc_complete.reason);
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        ESP_LOGI(TAG, "encryption change event; status=%d ",
                 event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        print_conn_desc(&desc);
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        /* Peer sent us a notification or indication. */
        ESP_LOGI(TAG, "received %s; conn_handle=%d attr_handle=%d "
                      "attr_len=%d\n",
                 event->notify_rx.indication ? "indication" : "notification",
                 event->notify_rx.conn_handle,
                 event->notify_rx.attr_handle,
                 OS_MBUF_PKTLEN(event->notify_rx.om));

        /* Attribute data is contained in event->notify_rx.attr_data. */
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                 event->mtu.conn_handle,
                 event->mtu.channel_id,
                 event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    default:
        return 0;
    }
}

static void blecent_on_reset(int reason)
{
    ESP_LOGI(TAG, "Resetting state; reason=%d\n", reason);
}

static void blecent_on_sync(void)
{
    int rc;

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    blecent_scan();
}

void blecent_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t init_nimble(void)
{
    esp_err_t ret = ESP_OK;
    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init nimble %d ", ret);
        return ret;
    }
    /* Configure the host. */
    ble_hs_cfg.reset_cb = blecent_on_reset;
    ble_hs_cfg.sync_cb = blecent_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

#if CONFIG_BT_NIMBLE_GAP_SERVICE
    int m;
    /* Set the default device name. */
    m = ble_svc_gap_device_name_set("nimble-blecent");
    assert(m == 0);
#endif

    ble_store_config_init();
    nimble_port_freertos_init(blecent_host_task);
    return ESP_OK;
}