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

#ifndef REGMAP_H_
#define REGMAP_H_

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "socal.h"
#include "hps.h"

#define ALT_LWFPGASLVS_SPAN (ALT_LWH2F_OFST - ALT_LWFPGASLVS_OFST - 1)

#if defined(TATE)
    #define HPS2FPGA_GPR_OFST (0x80000000)
#elif defined(TATE_4R4T)
    #define HPS2FPGA_GPR_OFST (0x80000000)
#elif defined(TATE_9R7T)
    #define HPS2FPGA_GPR_OFST (0x80000000)
#elif defined(TATE_4R4T_3G)
    #define HPS2FPGA_GPR_OFST (0x80000000)
#elif defined(TATE_8R)
    #define HPS2FPGA_GPR_OFST (0x80000000)
#elif defined(VAUNT)
    #define HPS2FPGA_GPR_OFST (0xFF200000)
#else
    #error "This file must be compiled with a valid PRODUCT (TATE, TATE_4R4T, TATE_9R7T, TATE_4R4T_3G, TATE_8R, VAUNT). Confirm spelling and spaces."
#endif

typedef struct bits {
    char **names;
    uint8_t *spans;
} bits_t;

typedef struct reg {
    uint32_t addr;
    const char *name;
    uint32_t def_val;
    const char *perm;
    bits_t field;
} reg_t;

size_t get_num_regs(void);
const reg_t *get_reg_from_name(const char *name);
const reg_t *get_reg_from_addr(uint32_t addr);
const reg_t *get_reg_from_index(uint32_t index);
void print_regs(int verbose);

#endif
