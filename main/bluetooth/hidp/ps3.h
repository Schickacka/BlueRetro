/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HIDP_PS3_H_
#define _BT_HIDP_PS3_H_

#include "hidp.h"

#define BT_HIDP_PS3_BT_INIT 0xf4
struct bt_hidp_ps3_bt_init {
    uint8_t magic[4];
} __packed;

#define BT_HIDP_PS3_STATUS 0x01
struct bt_hidp_ps3_status {
    uint8_t data[48];
} __packed;

struct bt_hidp_ps3_led {
    uint8_t time_enabled;
    uint8_t duty_length;
    uint8_t enabled;
    uint8_t duty_off;
    uint8_t duty_on;
} __packed;

#define BT_HIDP_PS3_SET_CONF 0x01
struct bt_hidp_ps3_set_conf {
    uint8_t tbd0;
    uint8_t hf_motor_len;
    uint8_t hf_motor_pwr;
    uint8_t lf_motor_len;
    uint8_t lf_motor_pwr;
    uint8_t tbd1[4];
    uint8_t leds;
    struct bt_hidp_ps3_led led[4];
    uint8_t reserved[5];
} __packed;

_Static_assert(sizeof(struct bt_hidp_ps3_set_conf) == 35, "Invalid PS3 set_conf size");

void bt_hid_ps3_set_led_mode(struct bt_hidp_ps3_set_conf *set_conf, uint8_t is_analog);
void bt_hid_ps3_tick(void);
void bt_hid_ps3_reset(uint8_t dev_id);
void bt_hid_cmd_ps3_set_conf(struct bt_dev *device, void *report);
void bt_hid_ps3_init(struct bt_dev *device);
void bt_hid_ps3_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);

#endif /* _BT_HIDP_PS3_H_ */
