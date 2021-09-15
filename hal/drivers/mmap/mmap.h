//
// Copyright 2014 - 2019 Per Vices Corporation
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef MMAP_H_
#define MMAP_H_

#define PAGE_SIZE (getpagesize())
#define PAGE_MASK (getpagesize() - 1)

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include "common.h"

int burst_read_hps_addr(uint32_t addr, uint32_t *data, size_t bytes_to_read);
int busrt_write_hps_addr(uint32_t addr, uint32_t *data, size_t bytes_to_write);
int read_hps_addr(uint32_t addr, uint32_t *data);
int write_hps_addr(uint32_t addr, uint32_t data);
int read_hps_reg(const char *reg, uint32_t *data);
int write_hps_reg(const char *reg, uint32_t data);
int write_hps_addr_mask(uint32_t addr, uint32_t data, uint32_t mask);
int write_hps_reg_mask(const char *reg, uint32_t data, uint32_t mask);

void list_hps_reg(uint8_t verbosity);
int dump_hps_reg(void);
void set_mem_debug_opt(uint8_t options);

int mmap_init();
void mmap_fini();
uint32_t read_jesd_reg(uint8_t jesd_num, uint32_t address, uint32_t *data);

#endif
