/*
 * Copyright (c) 2021-2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_attr.h>
#include "sdkconfig.h"
#include "memory_card.h"

#if defined(CONFIG_BR_PS1_MEMORY_CARD) && (CONFIG_BR_PS1_MEMORY_CARD != 0)
#include <sys/stat.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <soc/soc_memory_layout.h>
#include "xtensa/core-macros.h"
#include "system/fs.h"
#include "config.h"

/* Related to bare metal hack, something goes writing there. */
/* Workaround: Remove region from heap pool until I figure it out */
SOC_RESERVE_MEMORY_REGION(0x3FFE7D98, 0x3FFE7E28, bad_region);

static uint8_t *mc_buffer[MC_BUFFER_BLOCK_CNT] = {0};
static esp_timer_handle_t mc_timer_hdl = NULL;
static int32_t mc_block_state = 0;
static bool mc_ready = false;
static uint8_t mc_zero_block[32] = {0};

static int32_t mc_restore(void);
static int32_t mc_store(void);
static inline void mc_store_cb(void *arg);

static inline bool mc_range_ok(uint32_t addr, uint32_t size) {
    return addr <= MC_BUFFER_SIZE && size <= (MC_BUFFER_SIZE - addr);
}

static void mc_start_update_timer(uint64_t timeout_us) {
    if (mc_timer_hdl) {
        if (esp_timer_is_active(mc_timer_hdl)) {
            esp_timer_stop(mc_timer_hdl);
        }
        esp_timer_start_once(mc_timer_hdl, timeout_us);
    }
}

static int32_t mc_restore(void) {
    struct stat st;
    int32_t ret = -1;

    if (stat(MEMORY_CARD_FILE, &st) != 0) {
        printf("# %s: No Memory Card on FS. Creating...\n", __FUNCTION__);
        ret = mc_store();
    }
    else {
        FILE *file = fopen(MEMORY_CARD_FILE, "rb");
        if (file == NULL) {
            printf("# %s: failed to open file for reading\n", __FUNCTION__);
        }
        else {
            uint32_t count = 0;
            for (uint32_t i = 0; i < MC_BUFFER_BLOCK_CNT; i++) {
                count += fread((void *)mc_buffer[i], MC_BUFFER_BLOCK_SIZE, 1, file);
            }
            fclose(file);

            if (count == MC_BUFFER_BLOCK_CNT) {
                ret = 0;
                printf("# %s: restore successful!\n", __FUNCTION__);
            }
            else {
                printf("# %s: restore failed! cnt: %ld File size:%ld\n", __FUNCTION__, count, st.st_size);
            }
        }
    }

    return ret;
}

static int32_t mc_store(void) {
    int32_t ret = -1;
    FILE *file = fopen(MEMORY_CARD_FILE, "wb");
    if (file == NULL) {
        printf("# %s: failed to open file for writing\n", __FUNCTION__);
    }
    else {
        uint32_t count = 0;
        for (uint32_t i = 0; i < MC_BUFFER_BLOCK_CNT; i++) {
            count += fwrite((void *)mc_buffer[i], MC_BUFFER_BLOCK_SIZE, 1, file);
        }
        fclose(file);

        if (count == MC_BUFFER_BLOCK_CNT) {
            ret = 0;
            mc_block_state = 0;
        }
        printf("# %s: file updated cnt: %ld\n", __FUNCTION__, count);
    }

    return ret;
}

static int32_t mc_store_spread(void) {
    int32_t ret = 0;
    int32_t block = __builtin_ffs(mc_block_state) - 1;

    if (block < 0) {
        return 0;
    }

    FILE *file = fopen(MEMORY_CARD_FILE, "r+b");
    if (file == NULL) {
        printf("# %s: failed to open file for writing\n", __FUNCTION__);
        ret = -1;
    }
    else {
        fseek(file, block * MC_BUFFER_BLOCK_SIZE, SEEK_SET);
        uint32_t count = fwrite((void *)mc_buffer[block], MC_BUFFER_BLOCK_SIZE, 1, file);
        fclose(file);
        if (count == 1) {
            atomic_clear_bit(&mc_block_state, block);
        }
        else {
            /* Keep the dirty bit: a later retry must not lose this block. */
            printf("# %s: failed to write block %ld\n", __FUNCTION__, block);
            ret = -1;
        }
    }

    return ret;
}

static inline void mc_store_cb(void *arg) {
    mc_store_spread();
    if (mc_block_state) {
        mc_start_update_timer(20000);
    }
}

int32_t mc_init_mem(void) {
    for (uint32_t i = 0; i < MC_BUFFER_BLOCK_CNT; i++) {
        mc_buffer[i] = heap_caps_malloc(MC_BUFFER_BLOCK_SIZE, MALLOC_CAP_DMA);
        if (mc_buffer[i] == NULL) {
            printf("# %s: failed to alloc block %ld\n", __FUNCTION__, i);
            for (uint32_t j = 0; j < i; j++) {
                heap_caps_free(mc_buffer[j]);
                mc_buffer[j] = NULL;
            }
            return -1;
        }
        /* A blank card is all ones. Do not persist uninitialized heap data. */
        memset(mc_buffer[i], 0xFF, MC_BUFFER_BLOCK_SIZE);
    }

    return 0;
}

int32_t mc_init(void) {
    int32_t ret = -1;

    if (!mc_buffer[0]) {
        printf("# %s: memory not allocated\n", __FUNCTION__);
        return -1;
    }

    /*
     * In debug/trace mode (banksel >= CONFIG_BANKSEL_MAX) the same 128 KB
     * buffer is used as a raw H4 trace sink. Do not restore a filesystem
     * image over it and do not advertise a memory card in that mode.
     */
    if (config.global_cfg.banksel < CONFIG_BANKSEL_MAX) {
        const esp_timer_create_args_t mc_timer_args = {
            .callback = &mc_store_cb,
            .arg = NULL,
            .name = "mc_timer"
        };

        if (esp_timer_create(&mc_timer_args, &mc_timer_hdl) != ESP_OK) {
            printf("# %s: failed to create timer\n", __FUNCTION__);
            return -1;
        }

        ret = mc_restore();
    }

    if (ret == 0) {
        mc_ready = true;
    }

    return ret;
}

void mc_storage_update(void) {
    mc_start_update_timer(1000000);
}

/* Bounds-checked read that also handles buffers crossing a 4 KB block. */
void IRAM_ATTR mc_read(uint32_t addr, uint8_t *data, uint32_t size) {
    if (!data || !size) {
        return;
    }
    if (!mc_range_ok(addr, size)) {
        memset(data, 0, size);
        return;
    }

    while (size) {
        uint32_t block = addr >> 12;
        uint32_t offset = addr & 0xFFF;
        uint32_t chunk = MC_BUFFER_BLOCK_SIZE - offset;

        if (chunk > size) {
            chunk = size;
        }
        memcpy(data, mc_buffer[block] + offset, chunk);
        data += chunk;
        addr += chunk;
        size -= chunk;
    }
}

/* Bounds-checked write that also handles buffers crossing a 4 KB block. */
void IRAM_ATTR mc_write(uint32_t addr, uint8_t *data, uint32_t size) {
    if (!data || !size || !mc_range_ok(addr, size)) {
        return;
    }

    uint32_t remaining = size;

    while (remaining) {
        uint32_t block = addr >> 12;
        uint32_t offset = addr & 0xFFF;
        uint32_t chunk = MC_BUFFER_BLOCK_SIZE - offset;

        if (chunk > remaining) {
            chunk = remaining;
        }
        memcpy(mc_buffer[block] + offset, data, chunk);
        data += chunk;
        addr += chunk;
        remaining -= chunk;

        if (config.global_cfg.banksel < CONFIG_BANKSEL_MAX) {
            atomic_set_bit(&mc_block_state, block);
        }
    }

    if (config.global_cfg.banksel < CONFIG_BANKSEL_MAX) {
        struct raw_fb fb_data = {0};

        fb_data.header.wired_id = 0;
        fb_data.header.type = FB_TYPE_MEM_WRITE;
        fb_data.header.data_len = 0;
        adapter_q_fb(&fb_data);
    }
}

uint8_t IRAM_ATTR *mc_get_ptr(uint32_t addr) {
    /*
     * Only nsi.c uses this pointer, for a 32-byte N64 Controller Pak page.
     * Reject out-of-range or block-crossing requests instead of returning a
     * pointer past a separately allocated 4 KB block.
     */
    if (!mc_range_ok(addr, 32) || ((addr & 0xFFF) + 32) > MC_BUFFER_BLOCK_SIZE) {
        return mc_zero_block;
    }
    return mc_buffer[addr >> 12] + (addr & 0xFFF);
}

uint32_t IRAM_ATTR mc_get_state(void) {
    return mc_block_state;
}

bool IRAM_ATTR mc_get_ready(void) {
    return mc_ready;
}
#else
int32_t mc_init_mem(void) {
    return 0;
}
int32_t mc_init(void) {
    return 0;
}
void mc_storage_update(void) {
}
void IRAM_ATTR mc_read(uint32_t addr, uint8_t *data, uint32_t size) {
    (void)addr;
    if (data && size) {
        memset(data, 0, size);
    }
}
void IRAM_ATTR mc_write(uint32_t addr, uint8_t *data, uint32_t size) {
    (void)addr;
    (void)data;
    (void)size;
}
uint8_t IRAM_ATTR *mc_get_ptr(uint32_t addr) {
    (void)addr;
    return NULL;
}
uint32_t IRAM_ATTR mc_get_state(void) {
    return 0;
}
bool IRAM_ATTR mc_get_ready(void) {
    return false;
}
#endif
