/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 * Modified by Schickacka (PlayStation 1 Custom Edition)
 */

#ifndef _PS_SPI_H_
#define _PS_SPI_H_

void ps_spi_init(uint32_t package);
void ps_spi_port_cfg(uint16_t mask);
void ps_spi_poll(void);
uint8_t ps_spi_is_analog(uint8_t wired_id);

#endif /* _PS_SPI_H_ */
