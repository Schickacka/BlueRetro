/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HIDP_PS_H_
#define _BT_HIDP_PS_H_

#include <stdint.h>
#include <stdbool.h>
#include "hidp.h"

#define BT_HIDP_HID_STATUS 0x01
#define BT_HIDP_PS4_STATUS 0x11
#define BT_HIDP_PS5_STATUS 0x31
struct bt_hidp_ps4_status {
    uint8_t data[77];
} __packed;

#define BT_HIDP_PS4_SET_CONF 0x11
struct bt_hidp_ps4_set_conf {
    uint8_t conf0;
    uint8_t tbd0;
    uint8_t conf1;
    uint8_t tbd1[2];
    uint8_t hf_motor_pwr;
    uint8_t lf_motor_pwr;
    union {
        struct {
            uint8_t rgb[3];
            uint8_t led_on_delay;
        };
        uint32_t leds;
    };
    uint8_t led_off_delay;
    uint8_t tbd2[61];
    uint32_t crc;
} __packed;

#define BT_HIDP_PS4_SET_CONF_USB 0x05
struct bt_hidp_ps4_set_conf_usb {
    uint8_t conf0;
    uint8_t conf1;
    uint8_t tbd;
    uint8_t hf_motor_pwr;
    uint8_t lf_motor_pwr;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t led_on_delay;
    uint8_t led_off_delay;
} __packed;

#define BT_HIDP_PS5_SET_CONF 0x31

#define BT_HIDP_PS5_LIGHTBAR_SETUP_ON   0x02
#define BT_HIDP_PS5_LIGHTBAR_SETUP_OUT  0x01

#define BT_HIDP_PS5_VALID_FLAG2_BRIGHTNESS      0x01
#define BT_HIDP_PS5_VALID_FLAG2_LIGHTBAR_SETUP  0x02
struct bt_hidp_ps5_set_conf {
    uint8_t conf0;      /* Bluetooth seq_tag: high nibble = sequence, low nibble = 0 */
    uint8_t tag;        /* Bluetooth output report tag, must be 0x10 */
    uint8_t cmd;        /* valid_flag0 */
    uint8_t conf1;
    uint8_t hf_motor_pwr;
    uint8_t lf_motor_pwr;
    uint8_t tbd0[4];
    uint8_t mic_led;
    uint8_t tbd1; // Mic/audio mute
    uint8_t r2_trigger_motor_mode;
    uint8_t r2_trigger_start_resistance;
    uint8_t r2_trigger_effect_force;
    uint8_t r2_trigger_range_force;
    uint8_t r2_trigger_near_release_str;
    uint8_t r2_trigger_near_middle_str;
    uint8_t r2_trigger_pressed_str;
    uint8_t tbd2[2];
    uint8_t r2_trigger_actuation_freq;
    uint8_t tbd3;
    uint8_t l2_trigger_motor_mode;
    uint8_t l2_trigger_start_resistance;
    uint8_t l2_trigger_effect_force;
    uint8_t l2_trigger_range_force;
    uint8_t l2_trigger_near_release_str;
    uint8_t l2_trigger_near_middle_str;
    uint8_t l2_trigger_pressed_str;
    uint8_t tbd4[2];
    uint8_t l2_trigger_actuation_freq;
    uint8_t tbd5[5];
    uint8_t l2_haptic_power_level;
    uint8_t tbd6; // Internal speaker volume
    uint8_t valid_flag2;
    uint8_t reserved3[2];
    uint8_t lightbar_setup;
    uint8_t led_brightness;
    uint8_t player_leds;
    union {
        struct {
            uint8_t rgb[3];
            uint8_t led_delay;
        };
        uint32_t leds;
    };
    /*
     * 23 bytes, not 24: the 4th byte of the RGB union (led_delay) already
     * occupies the first byte of the 24-byte reserved area of the official
     * 78-byte Bluetooth report (77-byte HIDP payload + report ID).
     */
    uint8_t tbd8[23];
    uint32_t crc;
} __packed;

#ifdef CONFIG_BR_PS1_LED_ANALOG_COLOR
#define BR_PS1_COLOR_ANALOG CONFIG_BR_PS1_LED_ANALOG_COLOR
#else
#define BR_PS1_COLOR_ANALOG 0x00006D
#endif

#ifdef CONFIG_BR_PS1_LED_DIGITAL_COLOR
#define BR_PS1_COLOR_DIGITAL CONFIG_BR_PS1_LED_DIGITAL_COLOR
#else
#define BR_PS1_COLOR_DIGITAL 0x101010
#endif

static inline uint32_t ps1_led_color(uint8_t is_analog) {
#ifdef CONFIG_BR_PS1_NIGHT_MODE
    uint32_t c = is_analog ? BR_PS1_COLOR_ANALOG : BR_PS1_COLOR_DIGITAL;
    /*
     * The report union is little-endian: low byte = red, middle = green,
     * high byte = blue. Halve the bytes in place so arbitrary user colors
     * keep their hue in night mode.
     */
    uint8_t r = (c & 0xFF) / 2;
    uint8_t g = ((c >> 8) & 0xFF) / 2;
    uint8_t b = ((c >> 16) & 0xFF) / 2;
    return (b << 16) | (g << 8) | r;
#else
    return is_analog ? BR_PS1_COLOR_ANALOG : BR_PS1_COLOR_DIGITAL;
#endif
}

static inline uint8_t ps1_led_brightness(void) {
#ifdef CONFIG_BR_PS1_NIGHT_MODE
    return 0x01;
#else
    return 0x02;
#endif
}

static inline uint8_t ps1_blink_brightness_max(void) {
#ifdef CONFIG_BR_PS1_NIGHT_MODE
    return 64;
#else
    return 128;
#endif
}

/*
 * Lightbar values are stored in the little-endian DS4/DS5 report union:
 * byte 0 = red, byte 1 = green, byte 2 = blue. Therefore the numeric
 * constant is 0x00BBGGRR, not 0x00RRGGBB. Amber = R=0xFF, G=0x80, B=0x00.
 */
#define PS1_BATTERY_AMBER 0x000080FF

static inline uint32_t ps1_battery_color(void) {
#ifdef CONFIG_BR_PS1_NIGHT_MODE
    return 0x0000407F; /* ~50% amber for night mode */
#else
    return PS1_BATTERY_AMBER;
#endif
}

static inline uint8_t bt_hidp_ps5_player_led(uint8_t out_idx) {
    static const uint8_t ps5_player_masks[] = { 0x04, 0x0A, 0x15, 0x1B, 0x1F };
    if (out_idx < sizeof(ps5_player_masks)) {
        return ps5_player_masks[out_idx];
    }
    return 0x1F;
}

void bt_hid_cmd_ps_set_conf(struct bt_dev *device, void *report);
void bt_hid_ps_init(struct bt_dev *device);
void ps_battery_update(uint8_t dev_id, uint8_t level, bool charging);
bool ps1_battery_pulse_active(uint8_t dev_id);
void bt_hid_ps_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);
uint8_t ps4_is_breathing(uint8_t dev_id);
void ps4_set_pending_led(uint8_t dev_id, uint32_t color);
void ps4_blink_stop(uint8_t dev_id);
void bt_hid_ps_tick(void);

#endif /* _BT_HIDP_PS_H_ */
