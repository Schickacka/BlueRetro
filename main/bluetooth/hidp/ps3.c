#include "hidp.h"
/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <esp_timer.h>
#include "adapter/adapter.h"
#include "bluetooth/host.h"
#include "wired/ps_spi.h"
#include "ps3.h"

#define PS3_TX_MIN_INTERVAL_US   35000  /* 35 ms min spacing (safely above DS3 15-30 ms drop zone) */
#define PS3_TX_HANDSHAKE_TMO_US 100000  /* 100 ms watchdog if HIDP_HANDSHAKE is missed */
#define PS3_STOP_HOLD_US         45000  /* 45 ms hold-off on motor stop to bridge 1-2 frame burst gaps */
#define PS3_STOP_CONFIRM_US     150000  /* 150 ms single confirmation STOP so motors never stick */

struct ps3_tx_ctx {
    bool valid_sent;
    bool in_flight;
    bool dirty;
    bool stop_pending;
    bool stop_confirm_pending;
    int64_t last_tx_us;
    int64_t stop_req_us;
    int64_t stop_confirm_us;
    struct bt_hidp_ps3_set_conf last_sent;
};

static struct ps3_tx_ctx ps3_tx_ctxs[BT_MAX_DEV] = {0};

static const uint8_t bt_init_magic[] = {
    0x42, 0x03, 0x00, 0x00
};

static const uint8_t ps3_config[] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0xff, 0x27, 0x10, 0x00, 0x32, 0xff,
    0x27, 0x10, 0x00, 0x32, 0xff, 0x27, 0x10, 0x00,
    0x32, 0xff, 0x27, 0x10, 0x00, 0x32, 0x00, 0x00,
    0x00, 0x00, 0x00
};

void bt_hid_ps3_set_led_mode(struct bt_hidp_ps3_set_conf *set_conf, uint8_t is_analog) {
    /*
     * DualShock 3 hardware LED timing (matches Linux hid-sony.c):
     * - Analog ON:  solid player LED       (duty_off=0x00, duty_on=0x32)
     * - Digital:    lit ~2 s, brief 200 ms off-blink (duty_off=0x14, duty_on=0xC8)
     * Applied across all 4 LED slots so any active player LED (P1..P4+) uses
     * the hardware blink timer with zero extra Bluetooth traffic.
     */
    for (uint32_t i = 0; i < 4; i++) {
        set_conf->led[i].time_enabled = 0xFF;
        set_conf->led[i].duty_length = 0x27;
        set_conf->led[i].enabled = 0x10;
        if (is_analog) {
            set_conf->led[i].duty_off = 0x00;
            set_conf->led[i].duty_on = 0x32;
        }
        else {
            set_conf->led[i].duty_off = 0x14;
            set_conf->led[i].duty_on = 0xC8;
        }
    }
}

static uint8_t ps3_is_analog_for_dev(struct bt_dev *device) {
    if (wired_adapter.system_id == PSX || wired_adapter.system_id == PS2) {
        return ps_spi_is_analog(device->ids.out_idx);
    }
    return 0;
}

static void bt_hid_cmd_ps3_bt_init(struct bt_dev *device) {
    bt_host_tx_lock();
    struct bt_hidp_ps3_bt_init *bt_init = (struct bt_hidp_ps3_bt_init *)bt_hid_pkt_tmp.hidp_data;

    memcpy((void *)bt_init, bt_init_magic, sizeof(*bt_init));

    bt_hid_cmd(device->acl_handle, device->ctrl_chan.dcid, BT_HIDP_SET_FE, BT_HIDP_PS3_BT_INIT, sizeof(*bt_init));
    bt_host_tx_unlock();
}

static void bt_hid_ps3_init_callback(void *arg) {
    struct bt_dev *device = (struct bt_dev *)arg;
    bt_host_tx_lock();
    /* Always release the one-shot timer, even if the device changed type. */
    if (device && device->timer_hdl) {
        esp_timer_delete(device->timer_hdl);
        device->timer_hdl = NULL;
    }
    if (!device || device->ids.type != BT_PS3) {
        bt_host_tx_unlock();
        return;
    }
    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
    struct bt_hidp_ps3_set_conf *set_conf = (struct bt_hidp_ps3_set_conf *)bt_data->base.output;
    uint8_t is_analog = ps3_is_analog_for_dev(device);

    set_conf->leds = (bt_hid_led_dev_id_map[device->ids.out_idx] << 1);
    bt_hid_ps3_set_led_mode(set_conf, is_analog);
    printf("# %s (Player %ld, analog=%u)\n", __FUNCTION__, device->ids.out_idx + 1, is_analog);
    bt_hid_cmd_ps3_set_conf(device, bt_data->base.output);
    atomic_set_bit(&device->flags, BT_DEV_HID_INIT_DONE);
    bt_host_tx_unlock();
}

static bool ps3_conf_motors_on(const struct bt_hidp_ps3_set_conf *conf) {
    return (conf->hf_motor_pwr != 0) || (conf->lf_motor_pwr != 0);
}

static bool ps3_conf_leds_changed(const struct bt_hidp_ps3_set_conf *a, const struct bt_hidp_ps3_set_conf *b) {
    return (a->leds != b->leds) || (memcmp(a->led, b->led, sizeof(a->led)) != 0);
}

static void bt_hid_cmd_ps3_send_now(struct bt_dev *device, void *report, bool is_confirm) {
    struct bt_hidp_ps3_set_conf *set_conf = (struct bt_hidp_ps3_set_conf *)bt_hid_pkt_tmp.hidp_data;
    memcpy((void *)set_conf, report, sizeof(*set_conf));
    bt_hid_cmd(device->acl_handle, device->ctrl_chan.dcid, BT_HIDP_SET_OUT, BT_HIDP_PS3_SET_CONF, sizeof(*set_conf));

    if (device->ids.id < BT_MAX_DEV) {
        struct ps3_tx_ctx *ctx = &ps3_tx_ctxs[device->ids.id];
        bool was_on = ctx->valid_sent && ps3_conf_motors_on(&ctx->last_sent);
        bool now_on = ps3_conf_motors_on((const struct bt_hidp_ps3_set_conf *)report);
        int64_t now = esp_timer_get_time();

        memcpy(&ctx->last_sent, report, sizeof(ctx->last_sent));
        ctx->valid_sent = true;
        ctx->in_flight = true;
        ctx->dirty = false;
        ctx->stop_pending = false;
        ctx->last_tx_us = now;

        if (was_on && !now_on && !is_confirm) {
            ctx->stop_confirm_pending = true;
            ctx->stop_confirm_us = now;
        }
        else {
            ctx->stop_confirm_pending = false;
        }
    }
}

void bt_hid_ps3_reset(uint8_t dev_id) {
    if (dev_id < BT_MAX_DEV) {
        memset(&ps3_tx_ctxs[dev_id], 0, sizeof(ps3_tx_ctxs[dev_id]));
    }
}

void bt_hid_ps3_tick(void) {
    int64_t now = esp_timer_get_time();

    for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
        struct ps3_tx_ctx *ctx = &ps3_tx_ctxs[i];
        if (!ctx->dirty && !ctx->in_flight && !ctx->stop_pending && !ctx->stop_confirm_pending) {
            continue;
        }

        struct bt_dev *device = NULL;
        if (bt_host_get_dev_from_id(i, &device) < 0 || !device ||
                device->ids.type != BT_PS3 ||
                !atomic_test_bit(&device->flags, BT_DEV_HID_INIT_DONE)) {
            bt_hid_ps3_reset(i);
            continue;
        }

        if (ctx->in_flight && (now - ctx->last_tx_us) >= PS3_TX_HANDSHAKE_TMO_US) {
            ctx->in_flight = false;
        }

        if (ctx->stop_pending && (now - ctx->stop_req_us) >= PS3_STOP_HOLD_US) {
            ctx->stop_pending = false;
            ctx->dirty = true;
        }

        if (!ctx->in_flight && ctx->dirty && (now - ctx->last_tx_us) >= PS3_TX_MIN_INTERVAL_US) {
            struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
            if (!ctx->valid_sent || memcmp(bt_data->base.output, &ctx->last_sent, sizeof(ctx->last_sent)) != 0) {
                bt_hid_cmd_ps3_send_now(device, bt_data->base.output, false);
            }
            else {
                ctx->dirty = false;
            }
            continue;
        }

        if (!ctx->in_flight && !ctx->dirty && !ctx->stop_pending && ctx->stop_confirm_pending &&
                (now - ctx->stop_confirm_us) >= PS3_STOP_CONFIRM_US) {
            struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
            const struct bt_hidp_ps3_set_conf *cur = (const struct bt_hidp_ps3_set_conf *)bt_data->base.output;
            if (!ps3_conf_motors_on(cur)) {
                bt_hid_cmd_ps3_send_now(device, bt_data->base.output, true);
            }
            else {
                ctx->stop_confirm_pending = false;
            }
        }
    }
}

void bt_hid_cmd_ps3_set_conf(struct bt_dev *device, void *report) {
    bt_host_tx_lock();
    if (device->ids.id < BT_MAX_DEV) {
        struct ps3_tx_ctx *ctx = &ps3_tx_ctxs[device->ids.id];
        const struct bt_hidp_ps3_set_conf *new_conf = (const struct bt_hidp_ps3_set_conf *)report;
        int64_t now = esp_timer_get_time();

        if (ctx->in_flight && (now - ctx->last_tx_us) >= PS3_TX_HANDSHAKE_TMO_US) {
            ctx->in_flight = false;
        }

        if (ps3_conf_motors_on(new_conf)) {
            ctx->stop_confirm_pending = false;
        }

        /*
         * If the desired state matches what is already active on the DS3
         * (e.g. a brief 1-2 frame off-gap during a rapid-fire burst just
         * ended and resumed the same rumble), cancel any pending stop and
         * send nothing so the motors never spin down.
         */
        if (ctx->valid_sent && memcmp(new_conf, &ctx->last_sent, sizeof(ctx->last_sent)) == 0) {
            ctx->stop_pending = false;
            ctx->dirty = false;
            bt_host_tx_unlock();
            return;
        }

        /*
         * Hold off pure motor-stop transitions briefly (45 ms) so rapid-fire
         * PS1 games that pulse rumble every 1-2 frames (like RE3 assault rifle)
         * keep both motors spinning smoothly instead of stopping and restarting
         * between shots. Never delay LED updates or motor-ON transitions.
         */
        if (ctx->valid_sent &&
                ps3_conf_motors_on(&ctx->last_sent) &&
                !ps3_conf_motors_on(new_conf) &&
                !ps3_conf_leds_changed(&ctx->last_sent, new_conf)) {
            if (!ctx->stop_pending) {
                ctx->stop_pending = true;
                ctx->stop_req_us = now;
            }
            ctx->dirty = false;
            bt_host_tx_unlock();
            return;
        }

        ctx->stop_pending = false;

        if (ctx->in_flight || (ctx->last_tx_us != 0 && (now - ctx->last_tx_us) < PS3_TX_MIN_INTERVAL_US)) {
            /*
             * DS3 drops SET_REPORT (0x52) sent while the previous control
             * transaction is still in flight or within ~15-30 ms. Latch dirty
             * so bt_hid_ps3_tick() / HIDP_HANDSHAKE flushes the latest report.
             */
            ctx->dirty = true;
            bt_host_tx_unlock();
            return;
        }
    }
    bt_hid_cmd_ps3_send_now(device, report, false);
    bt_host_tx_unlock();
}

void bt_hid_ps3_init(struct bt_dev *device) {
    if (device->timer_hdl) {
        return;
    }
    bt_hid_ps3_reset(device->ids.id);
    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
    struct bt_hidp_ps3_set_conf *set_conf = (struct bt_hidp_ps3_set_conf *)bt_data->base.output;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)set_conf, ps3_config, sizeof(ps3_config));
    set_conf->leds = (bt_hid_led_dev_id_map[device->ids.out_idx] << 1);
    bt_hid_ps3_set_led_mode(set_conf, ps3_is_analog_for_dev(device));

    /* PS3 ctrl not yet ready to RX config, delay 20ms */
    const esp_timer_create_args_t ps3_timer_args = {
        .callback = &bt_hid_ps3_init_callback,
        .arg = (void *)device,
        .name = "ps3_init_timer"
    };
    esp_timer_create(&ps3_timer_args, (esp_timer_handle_t *)&device->timer_hdl);
    esp_timer_start_once(device->timer_hdl, 1000000);

    bt_hid_cmd_ps3_bt_init(device);
}

void bt_hid_ps3_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    const uint32_t min_hdr_len = BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
                                 + sizeof(struct bt_l2cap_hdr) + 1;
    if (len < min_hdr_len) {
        return;
    }

    /* HIDP HANDSHAKE (high nibble 0x0) acknowledges SET_REPORT on ctrl_chan */
    if ((bt_hci_acl_pkt->sig_hdr.code & 0xF0) == 0x00) {
        if (device->ids.id < BT_MAX_DEV) {
            struct ps3_tx_ctx *ctx = &ps3_tx_ctxs[device->ids.id];
            ctx->in_flight = false;
            if (ctx->dirty && atomic_test_bit(&device->flags, BT_DEV_HID_INIT_DONE)) {
                int64_t now = esp_timer_get_time();
                if ((now - ctx->last_tx_us) >= PS3_TX_MIN_INTERVAL_US) {
                    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
                    if (!ctx->valid_sent || memcmp(bt_data->base.output, &ctx->last_sent, sizeof(ctx->last_sent)) != 0) {
                        bt_hid_cmd_ps3_send_now(device, bt_data->base.output, false);
                    }
                    else {
                        ctx->dirty = false;
                    }
                }
            }
        }
        return;
    }

    const uint32_t full_hdr_len = BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
                                  + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_hidp_hdr);
    if (len < full_hdr_len) {
        return;
    }
    uint32_t hidp_data_len = len - full_hdr_len;
#ifdef CONFIG_BLUERETRO_ADAPTER_RUMBLE_TEST
    (void)hidp_data_len; /* only the #else path forwards the raw report */
#endif

    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_PS3_STATUS:
#ifdef CONFIG_BLUERETRO_ADAPTER_RUMBLE_TEST
                    struct bt_hidp_ps3_set_conf rumble;
                    memcpy(&rumble, ps3_config, sizeof(rumble));
                    if (bt_hci_acl_pkt->hidp_data[17]) {
                        rumble.lf_motor_pwr = bt_hci_acl_pkt->hidp_data[17];
                        rumble.lf_motor_len = 0xFF;
                    }
                    if (bt_hci_acl_pkt->hidp_data[18]) {
                        rumble.hf_motor_pwr = 0x01;
                        rumble.hf_motor_len = 0xFF;
                    }
                    bt_hid_cmd_ps3_set_conf(device, &rumble);
#else
                    if (bt_hci_acl_pkt->hidp_data[0] != 0xFF) {
                        bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    }
#endif
                    break;
            }
            break;
    }
}
