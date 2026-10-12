/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <esp32/rom/crc.h>
#include <esp_timer.h>
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "ps.h"

static void bt_hid_cmd_ps5_set_conf(struct bt_dev *device, void *report);

static void bt_hid_cmd_ps4_clone_set_conf(struct bt_dev *device, struct bt_hidp_ps4_set_conf *report) {
    struct bt_hidp_ps4_set_conf_usb clone_conf = {
        .conf0 = 0xFF,
        .conf1 = 0x04,
        .tbd = 0x00,
        .hf_motor_pwr = report->hf_motor_pwr,
        .lf_motor_pwr = report->lf_motor_pwr,
        .r = report->rgb[0],
        .g = report->rgb[1],
        .b = report->rgb[2],
        .led_on_delay = report->led_on_delay,
        .led_off_delay = report->led_off_delay,
    };

    /* Send Report 0x05 on interrupt channel */
    memcpy((void *)bt_hci_pkt_tmp.hidp_data, &clone_conf, sizeof(clone_conf));
    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_PS4_SET_CONF_USB, sizeof(clone_conf));

    /* Also send on control channel if available */
    if (device->ctrl_chan.dcid) {
        memcpy((void *)bt_hci_pkt_tmp.hidp_data, &clone_conf, sizeof(clone_conf));
        bt_hid_cmd(device->acl_handle, device->ctrl_chan.dcid, BT_HIDP_SET_OUT, BT_HIDP_PS4_SET_CONF_USB, sizeof(clone_conf));
    }
}

static void bt_hid_cmd_ps4_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_hci_pkt_tmp.hidp_data;

    bt_hci_pkt_tmp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_hci_pkt_tmp.hidp_hdr.protocol = BT_HIDP_PS4_SET_CONF;

    memcpy((void *)set_conf, report, sizeof(*set_conf));

    set_conf->crc = crc32_le((uint32_t)~0xFFFFFFFF, (void *)&bt_hci_pkt_tmp.hidp_hdr,
        sizeof(bt_hci_pkt_tmp.hidp_hdr) + sizeof(*set_conf) - sizeof(set_conf->crc));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_PS4_SET_CONF, sizeof(*set_conf));

    /* If this is a clone / third-party controller operating in HID status mode, also send Report 0x05 */
    if (device->ids.report_type == BT_HIDP_HID_STATUS) {
        bt_hid_cmd_ps4_clone_set_conf(device, (struct bt_hidp_ps4_set_conf *)report);
    }
}

static void bt_hid_cmd_ps5_trigger_init(struct bt_dev *device) {
    int32_t perc_threshold_l = -1;
    int32_t perc_threshold_r = -1;
    int32_t dev = device->ids.id;
    uint32_t map_cnt_l = 0;
    uint32_t map_cnt_r = 0;

    if (wired_adapter.system_id == WIRED_AUTO) {
        /* Can't configure feature if target system is unknown */
        return;
    }

    printf("# %s\n", __FUNCTION__);

    /* Make sure meta desc is init */
    adapter_meta_init();

    /* Go through the list of mappings, looking for PAD_RM and PAD_LM */
    for (uint32_t i = 0; i < config.in_cfg[dev].map_size; i++) {
        if (config.in_cfg[dev].map_cfg[i].dst_btn < BR_COMBO_BASE_1) {
            uint8_t is_axis = btn_is_axis(config.in_cfg[dev].map_cfg[i].dst_id, config.in_cfg[dev].map_cfg[i].dst_btn);
            if (config.in_cfg[dev].map_cfg[i].src_btn == PAD_RM) {
                map_cnt_r++;
                if (is_axis) {
                    continue;
                }
                if (config.in_cfg[dev].map_cfg[i].perc_threshold > perc_threshold_r) {
                    perc_threshold_r = config.in_cfg[dev].map_cfg[i].perc_threshold;
                }
            }
            else if (config.in_cfg[dev].map_cfg[i].src_btn == PAD_LM) {
                map_cnt_l++;
                if (is_axis) {
                    continue;
                }
                if (config.in_cfg[dev].map_cfg[i].perc_threshold > perc_threshold_l) {
                    perc_threshold_l = config.in_cfg[dev].map_cfg[i].perc_threshold;
                }
            }
        }
    }
    /* If only one mapping exist do not set resistance */
    if (map_cnt_r < 2) {
        perc_threshold_r = -1;
    }
    if (map_cnt_l < 2) {
        perc_threshold_l = -1;
    }

    uint8_t r2_start_resistance_value = (perc_threshold_r * 255) / 100;
    uint8_t l2_start_resistance_value = (perc_threshold_l * 255) / 100;

    uint8_t r2_trigger_start_resistance = (uint8_t)(0x94 * (r2_start_resistance_value / 255.0));
    uint8_t r2_trigger_effect_force =
        (uint8_t)((0xb4 - r2_trigger_start_resistance) * (r2_start_resistance_value / 255.0) + r2_trigger_start_resistance);

    uint8_t l2_trigger_start_resistance = (uint8_t)(0x94 * (l2_start_resistance_value / 255.0));
    uint8_t l2_trigger_effect_force =
        (uint8_t)((0xb4 - l2_trigger_start_resistance) * (l2_start_resistance_value / 255.0) + l2_trigger_start_resistance);

    struct bt_hidp_ps5_set_conf ps5_set_conf = {
        .conf0 = 0x02,
        .cmd = 0x0c,
        .r2_trigger_motor_mode = perc_threshold_r > -1 ? 0x02 : 0x00,
        .r2_trigger_start_resistance = r2_trigger_start_resistance,
        .r2_trigger_effect_force = r2_trigger_effect_force,
        .r2_trigger_range_force = 0xff,
        .r2_trigger_near_release_str = 0x00,
        .r2_trigger_near_middle_str = 0x00,
        .r2_trigger_pressed_str = 0x00,
        .r2_trigger_actuation_freq = 0x00,
        .l2_trigger_motor_mode = perc_threshold_l > -1 ? 0x02 : 0x00,
        .l2_trigger_start_resistance = l2_trigger_start_resistance,
        .l2_trigger_effect_force = l2_trigger_effect_force,
        .l2_trigger_range_force = 0xff,
        .l2_trigger_near_release_str = 0x00,
        .l2_trigger_near_middle_str = 0x00,
        .l2_trigger_pressed_str = 0x00,
        .l2_trigger_actuation_freq = 0x00,
    };

    bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_conf);
}

struct ps4_blink_ctx {
    esp_timer_handle_t timer;
    struct bt_dev *device;
    uint32_t pending_led;
    uint8_t current_breath;
    uint8_t total_breaths;
    uint8_t step; /* 0 to 49 (50 steps * 40ms = 2000ms per breath) */
};

static struct ps4_blink_ctx ps4_blink_ctxs[BT_MAX_DEV] = {0};

static void ps4_blink_timer_cb(void *arg) {
    struct ps4_blink_ctx *ctx = (struct ps4_blink_ctx *)arg;
    struct bt_dev *device = ctx->device;

    if (!device || device->ids.type == BT_NONE || !atomic_test_bit(&device->flags, BT_DEV_HID_INIT_DONE)) {
        if (ctx->timer) {
            esp_timer_stop(ctx->timer);
            esp_timer_delete(ctx->timer);
            ctx->timer = NULL;
        }
        return;
    }

    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
    struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_data->base.output;

    uint8_t blue_val;
    if (ctx->step < 25) {
        /* Fade in: 0 -> 255 in 1.0s */
        blue_val = (uint8_t)((ctx->step * 255) / 24);
    }
    else {
        /* Fade out: 255 -> 0 in 1.0s */
        blue_val = (uint8_t)(((49 - ctx->step) * 255) / 24);
    }

    /* Update buffer directly so background tasks never send white/grey */
    set_conf->rgb[0] = 0;
    set_conf->rgb[1] = 0;
    set_conf->rgb[2] = blue_val;
    bt_hid_cmd_ps4_set_conf(device, (void *)set_conf);

    ctx->step++;
    if (ctx->step >= 50) {
        ctx->step = 0;
        ctx->current_breath++;

        if (ctx->current_breath >= ctx->total_breaths) {
            /* Breathing complete! Stop timer */
            esp_timer_stop(ctx->timer);
            esp_timer_delete(ctx->timer);
            ctx->timer = NULL;

            /* Restore pending SCPH-1200 mode LED (Grey for Digital, Red for Analog) */
            set_conf->leds = ctx->pending_led;
            bt_hid_cmd_ps4_set_conf(device, (void *)set_conf);
        }
    }
}

uint8_t ps4_is_breathing(uint8_t dev_id) {
    if (dev_id < BT_MAX_DEV) {
        return (ps4_blink_ctxs[dev_id].timer != NULL);
    }
    return 0;
}

void ps4_set_pending_led(uint8_t dev_id, uint32_t color) {
    if (dev_id < BT_MAX_DEV) {
        ps4_blink_ctxs[dev_id].pending_led = color;
    }
}

static void ps4_start_player_blink(struct bt_dev *device) {
    uint8_t dev_id = device->ids.id;
    if (dev_id >= BT_MAX_DEV) {
        return;
    }

    struct ps4_blink_ctx *ctx = &ps4_blink_ctxs[dev_id];
    if (ctx->timer) {
        esp_timer_stop(ctx->timer);
        esp_timer_delete(ctx->timer);
        ctx->timer = NULL;
    }

    ctx->device = device;
    ctx->pending_led = 0x202020; /* Default: Digital mode Soft Grey / White */
    ctx->current_breath = 0;
    ctx->total_breaths = device->ids.out_idx + 1;
    ctx->step = 0;

    const esp_timer_create_args_t blink_timer_args = {
        .callback = &ps4_blink_timer_cb,
        .arg = (void *)ctx,
        .name = "ps4_breath_tmr"
    };

    if (esp_timer_create(&blink_timer_args, &ctx->timer) == ESP_OK) {
        /* 40ms interval * 50 steps = 2000ms (2.0s) per full breath */
        esp_timer_start_periodic(ctx->timer, 40000);
        ps4_blink_timer_cb((void *)ctx);
    }
}

static void bt_hid_ps5_init_callback(void *arg) {
    struct bt_dev *device = (struct bt_dev *)arg;

    esp_timer_delete(device->timer_hdl);
    device->timer_hdl = NULL;

    atomic_set_bit(&device->flags, BT_DEV_HID_INIT_DONE);
    printf("# PS init done\n");

    if (device->ids.subtype == BT_PS5_DS || device->ids.report_type == BT_HIDP_PS5_STATUS) {
        static const uint8_t ps5_player_led_mask[4] = {
            0x04, /* Player 1: . . * . . */
            0x0A, /* Player 2: . * . * . */
            0x15, /* Player 3: * . * . * */
            0x1B, /* Player 4: * * . * * */
        };

        struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
        struct bt_hidp_ps5_set_conf *set_conf = (struct bt_hidp_ps5_set_conf *)bt_data->base.output;

        /* Init output data for Rumble/LED feedback */
        memset(set_conf, 0x00, sizeof(*set_conf));
        set_conf->conf0 = 0x02;
        set_conf->cmd = 0x03;
        set_conf->conf1 = 0x04;
        set_conf->valid_flag2 = 0x14; /* Enable lightbar + player indicator */
        set_conf->lightbar_setup = 0x02;
        set_conf->player_leds = ps5_player_led_mask[device->ids.out_idx & 0x3];
        set_conf->leds = 0x202020; /* Boot: Soft Grey / White */

        struct bt_hidp_ps5_set_conf ps5_clear_led = {
            .conf0 = 0x02,
            .conf1 = 0x08,
        };
        struct bt_hidp_ps5_set_conf ps5_set_led = {
            .conf0 = 0x02,
            .conf1 = 0x04,
            .valid_flag2 = 0x14,
            .lightbar_setup = 0x02,
            .player_leds = ps5_player_led_mask[device->ids.out_idx & 0x3],
            .leds = 0x202020,
        };
        printf("# %s: PS5 DualSense (Player %ld)\n", __FUNCTION__, device->ids.out_idx + 1);

        bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_clear_led);
        bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_led);

        /* Set trigger "click" haptic effect when rumble is on */
        if (config.out_cfg[device->ids.out_idx].acc_mode == ACC_RUMBLE
                || config.out_cfg[device->ids.out_idx].acc_mode == ACC_BOTH) {
            bt_hid_cmd_ps5_trigger_init(device);
        }
    }
    else {
        /* Clone or official DS4: Start connection player blink */
        printf("# %s: DS4 (report_type: 0x%02lX, Player %ld)\n", __FUNCTION__, device->ids.report_type, device->ids.out_idx + 1);
        ps4_start_player_blink(device);
    }
}

static void bt_hid_cmd_ps5_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_ps5_set_conf *set_conf = (struct bt_hidp_ps5_set_conf *)bt_hci_pkt_tmp.hidp_data;

    bt_hci_pkt_tmp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_hci_pkt_tmp.hidp_hdr.protocol = BT_HIDP_PS5_SET_CONF;

    memcpy((void *)set_conf, report, sizeof(*set_conf));

    set_conf->crc = crc32_le((uint32_t)~0xFFFFFFFF, (void *)&bt_hci_pkt_tmp.hidp_hdr,
        sizeof(bt_hci_pkt_tmp.hidp_hdr) + sizeof(*set_conf) - sizeof(set_conf->crc));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_PS5_SET_CONF, sizeof(*set_conf));
}

void bt_hid_cmd_ps_set_conf(struct bt_dev *device, void *report) {
    switch (device->ids.subtype) {
        case BT_PS5_DS:
            bt_hid_cmd_ps5_set_conf(device, report);
            break;
        default:
            bt_hid_cmd_ps4_set_conf(device, report);
            break;
    }
}

void bt_hid_ps_init(struct bt_dev *device) {
#ifndef CONFIG_BLUERETRO_TEST_FALLBACK_REPORT
    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
    struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_data->base.output;

    /* Init output data for Rumble/LED feedback */
    set_conf->conf0 = 0xc4;
    set_conf->conf1 = 0x03;
    set_conf->leds = 0x000000;

    switch (device->ids.subtype) {
        case BT_PS5_DS:
            bt_hid_ps5_init_callback((void *)device);
            break;
        default:
        {
            const esp_timer_create_args_t ps5_timer_args = {
                .callback = &bt_hid_ps5_init_callback,
                .arg = (void *)device,
                .name = "ps5_init_timer"
            };
            struct bt_hidp_ps4_set_conf ps4_set_conf = {
                .conf0 = 0xc0,
                .conf1 = 0x07,
            };
            ps4_set_conf.leds = 0x000000;

            printf("# %s\n", __FUNCTION__);

            esp_timer_create(&ps5_timer_args, (esp_timer_handle_t *)&device->timer_hdl);
            esp_timer_start_once(device->timer_hdl, 1000000);
            bt_hid_cmd_ps4_set_conf(device, (void *)&ps4_set_conf);
            break;
        }
    }
#endif
}

void bt_hid_ps_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    uint32_t hidp_data_len = len - (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
                                    + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_hidp_hdr));

    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_HID_STATUS:
                    if (device->ids.report_type != BT_HIDP_HID_STATUS) {
                        bt_type_update(device->ids.id, BT_PS, BT_SUBTYPE_DEFAULT);
                        device->ids.report_type = BT_HIDP_HID_STATUS;
                        struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
                        struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_data->base.output;
                        bt_hid_cmd_ps4_clone_set_conf(device, set_conf);
                    }
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    break;
                case BT_HIDP_PS4_STATUS:
                    if (device->ids.report_type != BT_HIDP_PS4_STATUS) {
                        bt_type_update(device->ids.id, BT_PS, BT_SUBTYPE_DEFAULT);
                        device->ids.report_type = BT_HIDP_PS4_STATUS;
                    }
#ifdef CONFIG_BLUERETRO_ADAPTER_RUMBLE_TEST
                    struct bt_hidp_ps4_set_conf rumble4 = {
                        .conf0 = 0xc4,
                        .conf1 = 0x03,
                    };
                    rumble4.r_rumble = bt_hci_acl_pkt->hidp_data[10];
                    rumble4.l_rumble = bt_hci_acl_pkt->hidp_data[9];
                    bt_hid_cmd_ps4_set_conf(device, &rumble4);
#else
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
#endif
                    break;
                case BT_HIDP_PS5_STATUS:
                    if (device->ids.report_type != BT_HIDP_PS5_STATUS) {
                        bt_type_update(device->ids.id, BT_PS, BT_PS5_DS);
                        device->ids.report_type = BT_HIDP_PS5_STATUS;
                    }
#ifdef CONFIG_BLUERETRO_ADAPTER_RUMBLE_TEST
                    struct bt_hidp_ps5_set_conf rumble = {
                        .conf0 = 0x02,
                        .cmd = 0x03,
                    };
                    rumble.r_rumble = bt_hci_acl_pkt->hidp_data[6];
                    rumble.l_rumble = bt_hci_acl_pkt->hidp_data[5];
                    bt_hid_cmd_ps5_set_conf(device, &rumble);
#else
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
#endif
                    break;
            }
            break;
    }
}
