/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 * Modified by Schickacka (PlayStation 1 Custom Edition)
 */

#include <stdio.h>
#include <esp32/rom/crc.h>
#include <esp_timer.h>
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "wired/ps_spi.h"
#include "ps.h"

static void bt_hid_cmd_ps5_set_conf(struct bt_dev *device, void *report);

static void bt_hid_cmd_ps4_clone_set_conf(struct bt_dev *device, struct bt_hidp_ps4_set_conf *report) {
    bt_host_tx_lock();
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

    /* Send Report 0x05 on interrupt channel only (never on control channel to prevent flood/handshake stalls) */
    memcpy((void *)bt_hid_pkt_tmp.hidp_data, &clone_conf, sizeof(clone_conf));
    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_PS4_SET_CONF_USB, sizeof(clone_conf));
    bt_host_tx_unlock();
}

static void bt_hid_cmd_ps4_set_conf(struct bt_dev *device, void *report) {
    bt_host_tx_lock();
    struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_hid_pkt_tmp.hidp_data;

    bt_hid_pkt_tmp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_hid_pkt_tmp.hidp_hdr.protocol = BT_HIDP_PS4_SET_CONF;

    memcpy((void *)set_conf, report, sizeof(*set_conf));

    set_conf->crc = crc32_le((uint32_t)~0xFFFFFFFF, (void *)&bt_hid_pkt_tmp.hidp_hdr,
        sizeof(bt_hid_pkt_tmp.hidp_hdr) + sizeof(*set_conf) - sizeof(set_conf->crc));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_PS4_SET_CONF, sizeof(*set_conf));

    /* If this is a clone / third-party controller operating in HID status mode, also send Report 0x05 */
    if (device->ids.report_type == BT_HIDP_HID_STATUS) {
        bt_hid_cmd_ps4_clone_set_conf(device, (struct bt_hidp_ps4_set_conf *)report);
    }
    bt_host_tx_unlock();
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

    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
    struct bt_hidp_ps5_set_conf *base_conf = (struct bt_hidp_ps5_set_conf *)bt_data->base.output;
    struct bt_hidp_ps5_set_conf ps5_set_conf = *base_conf;

    ps5_set_conf.conf0 = 0x02;
    ps5_set_conf.cmd = 0x0c;
    ps5_set_conf.r2_trigger_motor_mode = perc_threshold_r > -1 ? 0x02 : 0x00;
    ps5_set_conf.r2_trigger_start_resistance = r2_trigger_start_resistance;
    ps5_set_conf.r2_trigger_effect_force = r2_trigger_effect_force;
    ps5_set_conf.r2_trigger_range_force = 0xff;
    ps5_set_conf.r2_trigger_near_release_str = 0x00;
    ps5_set_conf.r2_trigger_near_middle_str = 0x00;
    ps5_set_conf.r2_trigger_pressed_str = 0x00;
    ps5_set_conf.r2_trigger_actuation_freq = 0x00;
    ps5_set_conf.l2_trigger_motor_mode = perc_threshold_l > -1 ? 0x02 : 0x00;
    ps5_set_conf.l2_trigger_start_resistance = l2_trigger_start_resistance;
    ps5_set_conf.l2_trigger_effect_force = l2_trigger_effect_force;
    ps5_set_conf.l2_trigger_range_force = 0xff;
    ps5_set_conf.l2_trigger_near_release_str = 0x00;
    ps5_set_conf.l2_trigger_near_middle_str = 0x00;
    ps5_set_conf.l2_trigger_pressed_str = 0x00;
    ps5_set_conf.l2_trigger_actuation_freq = 0x00;

    bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_conf);
}

struct ps4_blink_ctx {
    bool active;
    struct bt_dev *device;
    uint32_t pending_led;
    uint8_t current_breath;
    uint8_t total_breaths;
    uint8_t step; /* 0 to 49 (50 steps * 40ms = 2000ms per breath) */
    uint8_t tick_cnt; /* 4 ticks * 10ms = 40ms per step */
};

struct ps_conn_rumble_ctx {
    bool active;
    struct bt_dev *device;
    uint8_t tick_cnt; /* 15 ticks * 10ms = 150ms */
    uint8_t saved_hf;
    uint8_t saved_lf;
};

static struct ps4_blink_ctx ps4_blink_ctxs[BT_MAX_DEV] = {0};
static struct ps_conn_rumble_ctx ps_conn_rumble_ctxs[BT_MAX_DEV] = {0};
static void ps_start_player_rumble(struct bt_dev *device);

#if defined(CONFIG_BR_PS1_BATTERY_IND) && CONFIG_BR_PS1_BATTERY_IND
#define PS1_BATT_POLL_TICKS  3000 /* 3000 * 10 ms = 30 s */
#define PS1_BATT_PULSE_TICKS 50   /* 50 * 10 ms = 500 ms */

struct ps_batt_ctx {
    bool valid;
    bool charging;
    uint8_t level;
    uint16_t tick_cnt;
    uint8_t pulse_tick;
    bool pulse;
};

static struct ps_batt_ctx ps_batt_ctxs[BT_MAX_DEV] = {0};

static uint8_t ps_is_analog_for_dev(struct bt_dev *device) {
    if (wired_adapter.system_id == PSX || wired_adapter.system_id == PS2) {
        return ps_spi_is_analog(device->ids.out_idx);
    }
    return 0;
}

static void ps_send_led_only(struct bt_dev *device, uint32_t color) {
    if (device->ids.subtype == BT_PS5_DS) {
        struct bt_hidp_ps5_set_conf set_conf = {0};

        set_conf.conf0 = 0x02;      /* overwritten with seq_tag by the wrapper */
        set_conf.cmd = 0x00;        /* no vibration flags: LED-only report */
        set_conf.conf1 = 0x14;      /* lightbar + player indicator enable */
        set_conf.valid_flag2 = 0x00;
        set_conf.lightbar_setup = 0x00;
        set_conf.led_brightness = ps1_led_brightness();
        set_conf.player_leds = bt_hidp_ps5_player_led(device->ids.out_idx);
        set_conf.leds = color;
        bt_hid_cmd_ps5_set_conf(device, (void *)&set_conf);
    }
    else {
        struct bt_hidp_ps4_set_conf set_conf = {0};

        set_conf.conf0 = 0xc4;
        set_conf.conf1 = 0x03;
        set_conf.leds = color;
        bt_hid_cmd_ps4_set_conf(device, (void *)&set_conf);
    }
}

void ps_battery_update(uint8_t dev_id, uint8_t level, bool charging) {
    if (dev_id >= BT_MAX_DEV) {
        return;
    }
    struct ps_batt_ctx *ctx = &ps_batt_ctxs[dev_id];

    ctx->valid = true;
    ctx->level = level;
    ctx->charging = charging;

    /* Unknown level (255) or healthy battery: cancel any pending pulse. */
    if (level == 255 || level > 20 || charging) {
        ctx->pulse = false;
        ctx->pulse_tick = 0;
        ctx->tick_cnt = 0;
    }
}

bool ps1_battery_pulse_active(uint8_t dev_id) {
    if (dev_id >= BT_MAX_DEV) {
        return false;
    }
    struct ps_batt_ctx *ctx = &ps_batt_ctxs[dev_id];
    return ctx->valid && ctx->pulse;
}

static void ps_battery_tick(struct bt_dev *device) {
    uint8_t dev_id = device->ids.id;
    if (dev_id >= BT_MAX_DEV) {
        return;
    }
    struct ps_batt_ctx *ctx = &ps_batt_ctxs[dev_id];
    if (!ctx->valid || ctx->level > 20 || ctx->charging) {
        return;
    }

    if (!ctx->pulse) {
        if (++ctx->tick_cnt >= PS1_BATT_POLL_TICKS) {
            ctx->tick_cnt = 0;
            ctx->pulse = true;
            ctx->pulse_tick = 0;
            ps_send_led_only(device, ps1_battery_color());
        }
    }
    else {
        if (++ctx->pulse_tick >= PS1_BATT_PULSE_TICKS) {
            ctx->pulse = false;
            ps_send_led_only(device, ps1_led_color(ps_is_analog_for_dev(device)));
        }
    }
}
#else
void ps_battery_update(uint8_t dev_id, uint8_t level, bool charging) {
    (void)dev_id;
    (void)level;
    (void)charging;
}

bool ps1_battery_pulse_active(uint8_t dev_id) {
    (void)dev_id;
    return false;
}
#endif

uint8_t ps4_is_breathing(uint8_t dev_id) {
    if (dev_id < BT_MAX_DEV) {
        return ps4_blink_ctxs[dev_id].active;
    }
    return 0;
}

void ps4_set_pending_led(uint8_t dev_id, uint32_t color) {
    if (dev_id < BT_MAX_DEV) {
        ps4_blink_ctxs[dev_id].pending_led = color;
    }
}

void ps4_blink_stop(uint8_t dev_id) {
    if (dev_id < BT_MAX_DEV) {
        ps4_blink_ctxs[dev_id].active = false;
        ps4_blink_ctxs[dev_id].device = NULL;
        ps4_blink_ctxs[dev_id].tick_cnt = 0;
        ps_conn_rumble_ctxs[dev_id].active = false;
        ps_conn_rumble_ctxs[dev_id].device = NULL;
        ps_conn_rumble_ctxs[dev_id].tick_cnt = 0;
#if defined(CONFIG_BR_PS1_BATTERY_IND) && CONFIG_BR_PS1_BATTERY_IND
        memset(&ps_batt_ctxs[dev_id], 0, sizeof(ps_batt_ctxs[dev_id]));
#endif
    }
}

static void ps4_start_player_blink(struct bt_dev *device) {
#if defined(CONFIG_BR_PS1_BLINK) && !CONFIG_BR_PS1_BLINK
    return;
#endif
    uint8_t dev_id = device->ids.id;
    if (dev_id >= BT_MAX_DEV) {
        return;
    }

    struct ps4_blink_ctx *ctx = &ps4_blink_ctxs[dev_id];
    if (ctx->active) {
        return;
    }

    ctx->device = device;
    uint8_t is_analog = 0;
    if (wired_adapter.system_id == PSX || wired_adapter.system_id == PS2) {
        is_analog = ps_spi_is_analog(device->ids.out_idx);
    }
    ctx->pending_led = ps1_led_color(is_analog);
    ctx->current_breath = 0;
    ctx->total_breaths = device->ids.out_idx + 1;
    ctx->step = 0;
    ctx->tick_cnt = 0;
    ctx->active = true;
}

static void bt_hid_ps5_init_callback(void *arg) {
    struct bt_dev *device = (struct bt_dev *)arg;
    bt_host_tx_lock();

    /* Always release the one-shot timer, even if the device changed type. */
    if (device && device->timer_hdl) {
        esp_timer_delete(device->timer_hdl);
        device->timer_hdl = NULL;
    }

    if (!device || device->ids.type != BT_PS) {
        bt_host_tx_unlock();
        return;
    }

    if (device->ids.subtype == BT_PS5_DS || device->ids.report_type == BT_HIDP_PS5_STATUS) {
        device->ids.subtype = BT_PS5_DS;
        struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
        struct bt_hidp_ps5_set_conf *set_conf = (struct bt_hidp_ps5_set_conf *)bt_data->base.output;

        uint8_t is_analog = 0;
        if (wired_adapter.system_id == PSX || wired_adapter.system_id == PS2) {
            is_analog = ps_spi_is_analog(device->ids.out_idx);
        }
        uint32_t init_led = ps1_led_color(is_analog);

        /* Init output data for Rumble/LED feedback */
        memset(set_conf, 0x00, sizeof(*set_conf));
        set_conf->conf0 = 0x02;
        set_conf->cmd = 0x03;
        set_conf->conf1 = 0x14;             /* lightbar 0x04 + player indicator 0x10 */
        set_conf->valid_flag2 = 0x00;
        set_conf->lightbar_setup = 0x00;
        set_conf->led_brightness = ps1_led_brightness();
        set_conf->player_leds = bt_hidp_ps5_player_led(device->ids.out_idx);
        set_conf->leds = init_led;

        /*
         * Linux kernel hid-playstation parity (dualsense_reset_leds):
         * On Bluetooth startup, the DualSense controller displays an internal
         * boot animation and holds solid blue. To release the hardware into host
         * control, send a reset report with LIGHTBAR_SETUP_CONTROL_ENABLE (0x02)
         * and LIGHT_OUT (0x02).
         */
        struct bt_hidp_ps5_set_conf ps5_reset = {
            .conf0 = 0x02,
            .valid_flag2 = BT_HIDP_PS5_VALID_FLAG2_LIGHTBAR_SETUP,
            .lightbar_setup = BT_HIDP_PS5_LIGHTBAR_SETUP_OUT,
        };
        struct bt_hidp_ps5_set_conf ps5_set_led = {
            .conf0 = 0x02,
            .cmd = 0x03,
            .conf1 = 0x14,
            .valid_flag2 = 0x00,
            .lightbar_setup = 0x00,
            .led_brightness = ps1_led_brightness(),
            .player_leds = bt_hidp_ps5_player_led(device->ids.out_idx),
            .leds = init_led,
        };
        printf("# %s: PS5 DualSense (Player %ld)\n", __FUNCTION__, device->ids.out_idx + 1);

        bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_reset);
        bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_led);

        /* Set trigger click haptic effect when rumble is on */
        if (config.out_cfg[device->ids.out_idx].acc_mode == ACC_RUMBLE
                || config.out_cfg[device->ids.out_idx].acc_mode == ACC_BOTH) {
            bt_hid_cmd_ps5_trigger_init(device);
            bt_hid_cmd_ps5_set_conf(device, (void *)set_conf);
        }

        atomic_set_bit(&device->flags, BT_DEV_HID_INIT_DONE);
        printf("# PS init done\n");

#if defined(CONFIG_BR_PS1_CONN_RUMBLE)
        ps_start_player_rumble(device);
#endif
    }
    else {
        /* Clone or official DS4: Start connection player blink and rumble */
        printf("# %s: DS4 (report_type: 0x%02lX, Player %ld)\n", __FUNCTION__, device->ids.report_type, device->ids.out_idx + 1);
        atomic_set_bit(&device->flags, BT_DEV_HID_INIT_DONE);
        printf("# PS init done\n");

#if defined(CONFIG_BR_PS1_BLINK)
        ps4_start_player_blink(device);
#endif
#if defined(CONFIG_BR_PS1_CONN_RUMBLE)
        ps_start_player_rumble(device);
#endif
    }
    bt_host_tx_unlock();
}

static void bt_hid_cmd_ps5_set_conf(struct bt_dev *device, void *report) {
    static uint8_t output_seq[BT_MAX_DEV] = {0};
    uint8_t seq_idx = (device->ids.id < BT_MAX_DEV) ? device->ids.id : 0;

    bt_host_tx_lock();
    struct bt_hidp_ps5_set_conf *set_conf = (struct bt_hidp_ps5_set_conf *)bt_hid_pkt_tmp.hidp_data;

    bt_hid_pkt_tmp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_hid_pkt_tmp.hidp_hdr.protocol = BT_HIDP_PS5_SET_CONF;

    memcpy((void *)set_conf, report, sizeof(*set_conf));

    /*
     * Bluetooth output report 0x31 (78 bytes incl. report ID):
     *   payload[0] = seq_tag: sequence in the high nibble, low nibble 0
     *   payload[1] = tag 0x10
     * USB report 0x02 does not carry these two bytes, but this firmware
     * only sends feedback over the Bluetooth interrupt channel.
     */
    set_conf->conf0 = (output_seq[seq_idx]++ & 0x0F) << 4;
    set_conf->tag = 0x10;

    set_conf->crc = crc32_le((uint32_t)~0xFFFFFFFF, (void *)&bt_hid_pkt_tmp.hidp_hdr,
        sizeof(bt_hid_pkt_tmp.hidp_hdr) + sizeof(*set_conf) - sizeof(set_conf->crc));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_PS5_SET_CONF, sizeof(*set_conf));
    bt_host_tx_unlock();
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

static void ps_start_player_rumble(struct bt_dev *device) {
#if defined(CONFIG_BR_PS1_CONN_RUMBLE) && !CONFIG_BR_PS1_CONN_RUMBLE
    return;
#endif
    uint8_t dev_id = device->ids.id;
    if (dev_id >= BT_MAX_DEV) {
        return;
    }

    struct ps_conn_rumble_ctx *ctx = &ps_conn_rumble_ctxs[dev_id];
    if (ctx->active) {
        return;
    }

    ctx->device = device;
    ctx->tick_cnt = 0;

    struct bt_data *bt_data = &bt_adapter.data[dev_id];
    if (device->ids.subtype == BT_PS5_DS) {
        struct bt_hidp_ps5_set_conf *set_conf = (struct bt_hidp_ps5_set_conf *)bt_data->base.output;
        if (set_conf->hf_motor_pwr == 0xFF && set_conf->lf_motor_pwr == 0x00) {
            ctx->saved_hf = 0;
            ctx->saved_lf = 0;
        } else {
            ctx->saved_hf = set_conf->hf_motor_pwr;
            ctx->saved_lf = set_conf->lf_motor_pwr;
        }
        set_conf->cmd = 0x03;
        set_conf->hf_motor_pwr = 0xFF;
        set_conf->lf_motor_pwr = 0x00;
        bt_hid_cmd_ps5_set_conf(device, (void *)set_conf);
    }
    else {
        struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_data->base.output;
        ctx->saved_hf = set_conf->hf_motor_pwr;
        ctx->saved_lf = set_conf->lf_motor_pwr;
        set_conf->hf_motor_pwr = 0xFF;
        set_conf->lf_motor_pwr = 0x00;
        bt_hid_cmd_ps4_set_conf(device, (void *)set_conf);
    }
    ctx->active = true;
}

void bt_hid_ps_tick(void) {
    for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
        /* 1. Blink state machine */
        struct ps4_blink_ctx *bctx = &ps4_blink_ctxs[i];
        if (bctx->active) {
            struct bt_dev *dev = bctx->device;
            if (!dev || dev->ids.type == BT_NONE || !atomic_test_bit(&dev->flags, BT_DEV_HID_INIT_DONE)) {
                bctx->active = false;
                bctx->device = NULL;
            }
            else {
                bctx->tick_cnt++;
                if (bctx->tick_cnt >= 4) { /* 4 * 10ms = 40ms */
                    bctx->tick_cnt = 0;
                    struct bt_data *bt_data = &bt_adapter.data[dev->ids.id];
                    struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_data->base.output;

                    uint8_t max_bright = ps1_blink_brightness_max();
                    uint8_t blue_val;
                    if (bctx->step < 25) {
                        blue_val = (uint8_t)((bctx->step * max_bright) / 24);
                    }
                    else {
                        blue_val = (uint8_t)(((49 - bctx->step) * max_bright) / 24);
                    }

                    set_conf->rgb[0] = 0;
                    set_conf->rgb[1] = 0;
                    set_conf->rgb[2] = blue_val;
                    bt_hid_cmd_ps4_set_conf(dev, (void *)set_conf);

                    bctx->step++;
                    if (bctx->step >= 50) {
                        bctx->step = 0;
                        bctx->current_breath++;
                        if (bctx->current_breath >= bctx->total_breaths) {
                            bctx->active = false;
                            set_conf->leds = bctx->pending_led;
                            bt_hid_cmd_ps4_set_conf(dev, (void *)set_conf);
                        }
                    }
                }
            }
        }

        /* 2. Connection rumble state machine */
        struct ps_conn_rumble_ctx *rctx = &ps_conn_rumble_ctxs[i];
        if (rctx->active) {
            struct bt_dev *dev = rctx->device;
            if (!dev || dev->ids.type == BT_NONE || !atomic_test_bit(&dev->flags, BT_DEV_HID_INIT_DONE)) {
                rctx->active = false;
                rctx->device = NULL;
            }
            else {
                rctx->tick_cnt++;
                if (rctx->tick_cnt >= 15) { /* 15 * 10ms = 150ms */
                    rctx->active = false;
                    struct bt_data *bt_data = &bt_adapter.data[dev->ids.id];
                    /*
                     * The pulse must never clobber real game feedback. Only
                     * restore the pre-pulse values if the output still holds
                     * the pulse itself (HF=0xFF, LF=0x00). If the game wrote
                     * new rumble during the 150 ms window, its values win.
                     */
                    if (dev->ids.subtype == BT_PS5_DS) {
                        struct bt_hidp_ps5_set_conf *set_conf = (struct bt_hidp_ps5_set_conf *)bt_data->base.output;
                        if (set_conf->hf_motor_pwr == 0xFF && set_conf->lf_motor_pwr == 0x00) {
                            set_conf->cmd = 0x03;
                            set_conf->hf_motor_pwr = rctx->saved_hf;
                            set_conf->lf_motor_pwr = rctx->saved_lf;
                            bt_hid_cmd_ps5_set_conf(dev, (void *)set_conf);
                        }
                    }
                    else {
                        struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_data->base.output;
                        if (set_conf->hf_motor_pwr == 0xFF && set_conf->lf_motor_pwr == 0x00) {
                            set_conf->hf_motor_pwr = rctx->saved_hf;
                            set_conf->lf_motor_pwr = rctx->saved_lf;
                            bt_hid_cmd_ps4_set_conf(dev, (void *)set_conf);
                        }
                    }
                }
            }
        }

#if defined(CONFIG_BR_PS1_BATTERY_IND) && CONFIG_BR_PS1_BATTERY_IND
        /* 3. Low-battery amber pulse (best effort, every 30 s) */
        struct bt_dev *batt_dev = NULL;
        if (bt_host_get_dev_from_id(i, &batt_dev) == 0 && batt_dev
                && atomic_test_bit(&batt_dev->flags, BT_DEV_HID_INIT_DONE)) {
            ps_battery_tick(batt_dev);
        }
#endif
    }
}

void bt_hid_ps_init(struct bt_dev *device) {
#ifndef CONFIG_BLUERETRO_TEST_FALLBACK_REPORT
    if (atomic_test_bit(&device->flags, BT_DEV_HID_INIT_DONE) || device->timer_hdl != NULL) {
        return;
    }

    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
    memset(bt_data->base.output, 0, sizeof(struct bt_hidp_ps5_set_conf));
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
                    rumble4.hf_motor_pwr = bt_hci_acl_pkt->hidp_data[10];
                    rumble4.lf_motor_pwr = bt_hci_acl_pkt->hidp_data[9];
                    bt_hid_cmd_ps4_set_conf(device, &rumble4);
#else
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
#endif
                    break;
                case BT_HIDP_PS5_STATUS:
                    if (device->ids.report_type != BT_HIDP_PS5_STATUS) {
                        bt_type_update(device->ids.id, BT_PS, BT_PS5_DS);
                        device->ids.subtype = BT_PS5_DS;
                        device->ids.report_type = BT_HIDP_PS5_STATUS;
                        memset(bt_adapter.data[device->ids.id].base.output, 0,
                               sizeof(struct bt_hidp_ps5_set_conf));
                    }
#ifdef CONFIG_BLUERETRO_ADAPTER_RUMBLE_TEST
                    struct bt_hidp_ps5_set_conf rumble = {
                        .conf0 = 0x02,
                        .cmd = 0x03,
                    };
                    rumble.hf_motor_pwr = bt_hci_acl_pkt->hidp_data[6];
                    rumble.lf_motor_pwr = bt_hci_acl_pkt->hidp_data[5];
                    bt_hid_cmd_ps5_set_conf(device, &rumble);
#else
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
#endif
                    break;
            }
            break;
    }
}
