/*
 * Copyright (c) 2021-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include "soc/gpio_struct.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "ps.h"
#include "wired.h"

#ifdef BR_PS1_ONLY
#define WIRED_OTHER(func) NULL
#else
#include "npiso.h"
#include "cdi.h"
#include "genesis.h"
#include "pce.h"
#include "real.h"
#include "jag.h"
#include "pcfx.h"
#include "saturn.h"
#include "jvs.h"
#include "n64.h"
#include "dc.h"
#include "gc.h"
#include "parallel_1p.h"
#include "parallel_2p.h"
#include "sea.h"
#include "wii.h"
#define WIRED_OTHER(func) func
#endif

static from_generic_t from_generic_func[WIRED_MAX] = {
    NULL, /* WIRED_AUTO */
    WIRED_OTHER(para_1p_from_generic), /* PARALLEL_1P */
    WIRED_OTHER(para_2p_from_generic), /* PARALLEL_2P */
    WIRED_OTHER(npiso_from_generic), /* NES */
    WIRED_OTHER(pce_from_generic), /* PCE */
    WIRED_OTHER(genesis_from_generic), /* GENESIS */
    WIRED_OTHER(npiso_from_generic), /* SNES */
    WIRED_OTHER(cdi_from_generic), /* CDI */
    NULL, /* CD32 */
    WIRED_OTHER(real_from_generic), /* REAL_3DO */
    WIRED_OTHER(jag_from_generic), /* JAGUAR */
    ps_from_generic, /* PSX */
    WIRED_OTHER(saturn_from_generic), /* SATURN */
    WIRED_OTHER(pcfx_from_generic), /* PCFX */
    WIRED_OTHER(jvs_from_generic), /* JVS */
    WIRED_OTHER(n64_from_generic), /* N64 */
    WIRED_OTHER(dc_from_generic), /* DC */
    ps_from_generic, /* PS2 */
    WIRED_OTHER(gc_from_generic), /* GC */
    WIRED_OTHER(wii_from_generic), /* WII_EXT */
    WIRED_OTHER(npiso_from_generic), /* VB */
    WIRED_OTHER(para_1p_from_generic), /* PARALLEL_1P_OD */
    WIRED_OTHER(para_2p_from_generic), /* PARALLEL_2P_OD */
    WIRED_OTHER(sea_from_generic), /* SEA_BOARD */
};

static fb_to_generic_t fb_to_generic_func[WIRED_MAX] = {
    NULL, /* WIRED_AUTO */
    NULL, /* PARALLEL_1P */
    NULL, /* PARALLEL_2P */
    NULL, /* NES */
    NULL, /* PCE */
    NULL, /* GENESIS */
    WIRED_OTHER(npiso_fb_to_generic), /* SNES */
    NULL, /* CDI */
    NULL, /* CD32 */
    NULL, /* REAL_3DO */
    NULL, /* JAGUAR */
    ps_fb_to_generic, /* PSX */
    NULL, /* SATURN */
    NULL, /* PCFX */
    NULL, /* JVS */
    WIRED_OTHER(n64_fb_to_generic), /* N64 */
    WIRED_OTHER(dc_fb_to_generic), /* DC */
    ps_fb_to_generic, /* PS2 */
    WIRED_OTHER(gc_fb_to_generic), /* GC */
    NULL, /* WII_EXT */
    NULL, /* VB */
    NULL, /* PARALLEL_1P_OD */
    NULL, /* PARALLEL_2P_OD */
    NULL, /* SEA_BOARD */
};

static meta_init_t meta_init_func[WIRED_MAX] = {
    NULL, /* WIRED_AUTO */
    WIRED_OTHER(para_1p_meta_init), /* PARALLEL_1P */
    WIRED_OTHER(para_2p_meta_init), /* PARALLEL_2P */
    WIRED_OTHER(npiso_meta_init), /* NES */
    WIRED_OTHER(pce_meta_init), /* PCE */
    WIRED_OTHER(genesis_meta_init), /* GENESIS */
    WIRED_OTHER(npiso_meta_init), /* SNES */
    WIRED_OTHER(cdi_meta_init), /* CDI */
    NULL, /* CD32 */
    WIRED_OTHER(real_meta_init), /* REAL_3DO */
    WIRED_OTHER(jag_meta_init), /* JAGUAR */
    ps_meta_init, /* PSX */
    WIRED_OTHER(saturn_meta_init), /* SATURN */
    WIRED_OTHER(pcfx_meta_init), /* PCFX */
    WIRED_OTHER(jvs_meta_init), /* JVS */
    WIRED_OTHER(n64_meta_init), /* N64 */
    WIRED_OTHER(dc_meta_init), /* DC */
    ps_meta_init, /* PS2 */
    WIRED_OTHER(gc_meta_init), /* GC */
    WIRED_OTHER(wii_meta_init), /* WII_EXT */
    WIRED_OTHER(npiso_meta_init), /* VB */
    WIRED_OTHER(para_1p_meta_init), /* PARALLEL_1P_OD */
    WIRED_OTHER(para_2p_meta_init), /* PARALLEL_2P_OD */
    WIRED_OTHER(sea_meta_init), /* SEA_BOARD */
};

static DRAM_ATTR buffer_init_t buffer_init_func[WIRED_MAX] = {
    NULL, /* WIRED_AUTO */
    WIRED_OTHER(para_1p_init_buffer), /* PARALLEL_1P */
    WIRED_OTHER(para_2p_init_buffer), /* PARALLEL_2P */
    WIRED_OTHER(npiso_init_buffer), /* NES */
    WIRED_OTHER(pce_init_buffer), /* PCE */
    WIRED_OTHER(genesis_init_buffer), /* GENESIS */
    WIRED_OTHER(npiso_init_buffer), /* SNES */
    WIRED_OTHER(cdi_init_buffer), /* CDI */
    NULL, /* CD32 */
    WIRED_OTHER(real_init_buffer), /* REAL_3DO */
    WIRED_OTHER(jag_init_buffer), /* JAGUAR */
    ps_init_buffer, /* PSX */
    WIRED_OTHER(saturn_init_buffer), /* SATURN */
    WIRED_OTHER(pcfx_init_buffer), /* PCFX */
    WIRED_OTHER(jvs_init_buffer), /* JVS */
    WIRED_OTHER(n64_init_buffer), /* N64 */
    WIRED_OTHER(dc_init_buffer), /* DC */
    ps_init_buffer, /* PS2 */
    WIRED_OTHER(gc_init_buffer), /* GC */
    WIRED_OTHER(wii_init_buffer), /* WII_EXT */
    WIRED_OTHER(npiso_init_buffer), /* VB */
    WIRED_OTHER(para_1p_init_buffer), /* PARALLEL_1P_OD */
    WIRED_OTHER(para_2p_init_buffer), /* PARALLEL_2P_OD */
    WIRED_OTHER(sea_init_buffer), /* SEA_BOARD */
};

int32_t wired_meta_init(struct wired_ctrl *ctrl_data) {
    if (meta_init_func[wired_adapter.system_id]) {
        meta_init_func[wired_adapter.system_id](ctrl_data);
        return 0;
    }
    return -1;
}

void IRAM_ATTR wired_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    if (buffer_init_func[wired_adapter.system_id]) {
        buffer_init_func[wired_adapter.system_id](dev_mode, wired_data);
    }
}

void wired_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    if (from_generic_func[wired_adapter.system_id]) {
        from_generic_func[wired_adapter.system_id](dev_mode, ctrl_data, wired_data);
    }
}

void wired_fb_to_generic(int32_t dev_mode, struct raw_fb *raw_fb_data, struct generic_fb *fb_data) {
    if (fb_to_generic_func[wired_adapter.system_id]) {
        fb_to_generic_func[wired_adapter.system_id](dev_mode, raw_fb_data, fb_data);
    }
}

void wired_para_turbo_mask_hdlr(void) {
#ifndef BR_PS1_ONLY
    if (wired_adapter.system_id == PARALLEL_1P || wired_adapter.system_id == PARALLEL_1P_OD) {
        struct para_1p_map *map = (struct para_1p_map *)wired_adapter.data[0].output;
        struct para_1p_map *turbo_map_mask = (struct para_1p_map *)wired_adapter.data[0].output_mask;

        ++wired_adapter.data[0].frame_cnt;
        para_1p_gen_turbo_mask(&wired_adapter.data[0]);

        GPIO.out = map->buttons | turbo_map_mask->buttons;
        GPIO.out1.val = map->buttons_high | turbo_map_mask->buttons_high;
    }
    else if (wired_adapter.system_id == PARALLEL_2P || wired_adapter.system_id == PARALLEL_2P_OD) {
        struct para_2p_map *map1 = (struct para_2p_map *)wired_adapter.data[0].output;
        struct para_2p_map *map2 = (struct para_2p_map *)wired_adapter.data[1].output;
        struct para_2p_map *map1_mask = (struct para_2p_map *)wired_adapter.data[0].output_mask;
        struct para_2p_map *map2_mask = (struct para_2p_map *)wired_adapter.data[1].output_mask;

        ++wired_adapter.data[0].frame_cnt;
        para_2p_gen_turbo_mask(0, &wired_adapter.data[0]);
        ++wired_adapter.data[1].frame_cnt;
        para_2p_gen_turbo_mask(1, &wired_adapter.data[1]);

        GPIO.out = (map1->buttons | map1_mask->buttons) & (map2->buttons | map2_mask->buttons);
        GPIO.out1.val = (map1->buttons_high | map1_mask->buttons_high) & (map2->buttons_high | map2_mask->buttons_high);
    }
    else if (wired_adapter.system_id == SEA_BOARD) {
        struct sea_map *map = (struct sea_map *)wired_adapter.data[0].output;
        struct sea_map *turbo_map_mask = (struct sea_map *)wired_adapter.data[0].output_mask;

        ++wired_adapter.data[0].frame_cnt;
        sea_gen_turbo_mask(&wired_adapter.data[0]);

        if (!(map->gbahd_state & BIT(GBAHD_STATE_OSD))) {
            GPIO.out = map->buttons | turbo_map_mask->buttons;
            GPIO.out1.val = map->buttons_high | turbo_map_mask->buttons_high;
        }
    }
#endif
}

void IRAM_ATTR wired_gen_turbo_mask_btns16_pos(struct wired_data *wired_data, uint16_t *buttons, const uint32_t btns_mask[32]) {
    for (uint32_t i = 0; i < 32; i++) {
        uint8_t mask = wired_data->cnt_mask[i] >> 1;

        if (btns_mask[i] && mask) {
            if (wired_data->cnt_mask[i] & 1) {
                if (!(mask & wired_data->frame_cnt)) {
                    *buttons &= ~btns_mask[i];
                }
            }
            else {
                if (!((mask & wired_data->frame_cnt) == mask)) {
                    *buttons &= ~btns_mask[i];
                }
            }
        }
    }
}

void IRAM_ATTR wired_gen_turbo_mask_btns16_neg(struct wired_data *wired_data, uint16_t *buttons, const uint32_t btns_mask[32]) {
    for (uint32_t i = 0; i < 32; i++) {
        uint8_t mask = wired_data->cnt_mask[i] >> 1;

        if (btns_mask[i] && mask) {
            if (wired_data->cnt_mask[i] & 1) {
                if (!(mask & wired_data->frame_cnt)) {
                    *buttons |= btns_mask[i];
                }
            }
            else {
                if (!((mask & wired_data->frame_cnt) == mask)) {
                    *buttons |= btns_mask[i];
                }
            }
        }
    }
}

void IRAM_ATTR wired_gen_turbo_mask_btns32(struct wired_data *wired_data, uint32_t *buttons, const uint32_t (*btns_mask)[32],
                                            uint32_t bank_cnt) {
    for (uint32_t i = 0; i < 32; i++) {
        uint8_t mask = wired_data->cnt_mask[i] >> 1;

        if (mask) {
            for (uint32_t j = 0; j < bank_cnt; j++) {
                if (btns_mask[j][i]) {
                    if (wired_data->cnt_mask[i] & 1) {
                        if (!(mask & wired_data->frame_cnt)) {
                            buttons[j] |= btns_mask[j][i];
                        }
                    }
                    else {
                        if (!((mask & wired_data->frame_cnt) == mask)) {
                            buttons[j] |= btns_mask[j][i];
                        }
                    }
                }
            }
        }
    }
}

void IRAM_ATTR wired_gen_turbo_mask_axes8(struct wired_data *wired_data, uint8_t *axes, uint32_t axes_cnt,
                                            const uint8_t axes_idx[6], const struct ctrl_meta *axes_meta) {
    for (uint32_t i = 0; i < axes_cnt; i++) {
        uint8_t btn_id = axis_to_btn_id(i);
        uint8_t mask = wired_data->cnt_mask[btn_id] >> 1;
        if (mask) {
            if (wired_data->cnt_mask[btn_id] & 1) {
                if (!(mask & wired_data->frame_cnt)) {
                    axes[axes_idx[i]] = axes_meta[i].neutral;
                }
            }
            else {
                if (!((mask & wired_data->frame_cnt) == mask)) {
                    axes[axes_idx[i]] = axes_meta[i].neutral;
                }
            }
        }
    }
}

