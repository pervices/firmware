//
// Copyright 2014 Per Vices Corporation
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

#include "regmap.h"

#define HPS2FPGA_GPR_OFST	(0xff200000)
#define HPS2FPGA_10G_OFST	(0xff300000)

static const reg_t reg_table[] = {
	{0x000 + HPS2FPGA_GPR_OFST, "sys0", 0x00000000, "RW"},
	{0x010 + HPS2FPGA_GPR_OFST, "sys1", 0xffff003f, "RO"},
	{0x020 + HPS2FPGA_GPR_OFST, "sys2", 0x00000000, "RW"},

	{0x100 + HPS2FPGA_GPR_OFST, "ept0", 0x000001ff, "RW"},
	{0x110 + HPS2FPGA_GPR_OFST, "ept1", 0x00000000, "RW"},
	{0x120 + HPS2FPGA_GPR_OFST, "ept2", 0x00000000, "RO"},

	{0x200 + HPS2FPGA_GPR_OFST, "net0", 0x00000000, "RW"},
	{0x210 + HPS2FPGA_GPR_OFST, "net1", 0x00000000, "RW"},
	{0x220 + HPS2FPGA_GPR_OFST, "net2", 0x00000000, "RW"},
	{0x230 + HPS2FPGA_GPR_OFST, "net3", 0x00000000, "RW"},
	{0x240 + HPS2FPGA_GPR_OFST, "net4", 0x00000000, "RW"},
	{0x250 + HPS2FPGA_GPR_OFST, "net5", 0x00000000, "RW"},
	{0x260 + HPS2FPGA_GPR_OFST, "net6", 0x00000000, "RW"},
	{0x270 + HPS2FPGA_GPR_OFST, "net7", 0x00000000, "RW"},
	{0x280 + HPS2FPGA_GPR_OFST, "net8", 0x00000000, "RW"},
	{0x290 + HPS2FPGA_GPR_OFST, "net9", 0x00000000, "RW"},
	{0x2a0 + HPS2FPGA_GPR_OFST, "net10",0x00000000, "RW"},
	{0x2b0 + HPS2FPGA_GPR_OFST, "net11",0x00000000, "RW"},
	{0x2c0 + HPS2FPGA_GPR_OFST, "net12",0x00000000, "RW"},
	{0x2d0 + HPS2FPGA_GPR_OFST, "net13",0x00000000, "RW"},
	{0x2e0 + HPS2FPGA_GPR_OFST, "net14",0x00000000, "RW"},
	{0x2f0 + HPS2FPGA_GPR_OFST, "net15",0x00000000, "RW"},
	{0x300 + HPS2FPGA_GPR_OFST, "net16",0x00000000, "RW"},
	{0x310 + HPS2FPGA_GPR_OFST, "net17",0x00000000, "RW"},
	{0x320 + HPS2FPGA_GPR_OFST, "net18",0x00000000, "RW"},
	{0x330 + HPS2FPGA_GPR_OFST, "net19",0x00000000, "RW"},
	{0x340 + HPS2FPGA_GPR_OFST, "net20",0x00000000, "RW"},
	{0x350 + HPS2FPGA_GPR_OFST, "net21",0x00000000, "RW"},
	{0x320 + HPS2FPGA_GPR_OFST, "net22",0x00000000, "RW"},
	{0x370 + HPS2FPGA_GPR_OFST, "net23",0x00000000, "RW"},
	{0x380 + HPS2FPGA_GPR_OFST, "net24",0x00000000, "RW"},
	{0x390 + HPS2FPGA_GPR_OFST, "net25",0x00000000, "RW"},
	{0x3a0 + HPS2FPGA_GPR_OFST, "net26",0x00000000, "RW"},
	{0x3b0 + HPS2FPGA_GPR_OFST, "net27",0x00000000, "RW"},
	{0x3c0 + HPS2FPGA_GPR_OFST, "net28",0x00000000, "RW"},
	{0x3d0 + HPS2FPGA_GPR_OFST, "net29",0x00000000, "RW"},

	{0x400 + HPS2FPGA_GPR_OFST, "rxa0", 0x00000000, "RW"},
	{0x410 + HPS2FPGA_GPR_OFST, "rxa1", 0x000000ff, "RW"},
	{0x420 + HPS2FPGA_GPR_OFST, "rxa2", 0x00000000, "RW"},
	{0x430 + HPS2FPGA_GPR_OFST, "rxa3", 0x00000000, "RW"},
	{0x440 + HPS2FPGA_GPR_OFST, "rxa4", 0x00000000, "RW"},
	{0x450 + HPS2FPGA_GPR_OFST, "rxa5", 0x00000000, "RW"},
	{0x500 + HPS2FPGA_GPR_OFST, "rxb0", 0x00000000, "RW"},
	{0x510 + HPS2FPGA_GPR_OFST, "rxb1", 0x000000ff, "RW"},
	{0x520 + HPS2FPGA_GPR_OFST, "rxb2", 0x00000000, "RW"},
	{0x530 + HPS2FPGA_GPR_OFST, "rxb3", 0x00000000, "RW"},
	{0x540 + HPS2FPGA_GPR_OFST, "rxb4", 0x00000000, "RW"},
	{0x550 + HPS2FPGA_GPR_OFST, "rxb5", 0x00000000, "RW"},
	{0x600 + HPS2FPGA_GPR_OFST, "rxc0", 0x00000000, "RW"},
	{0x610 + HPS2FPGA_GPR_OFST, "rxc1", 0x000000ff, "RW"},
	{0x620 + HPS2FPGA_GPR_OFST, "rxc2", 0x00000000, "RW"},
	{0x630 + HPS2FPGA_GPR_OFST, "rxc3", 0x00000000, "RW"},
	{0x640 + HPS2FPGA_GPR_OFST, "rxc4", 0x00000000, "RW"},
	{0x650 + HPS2FPGA_GPR_OFST, "rxc5", 0x00000000, "RW"},
	{0x700 + HPS2FPGA_GPR_OFST, "rxd0", 0x00000000, "RW"},
	{0x710 + HPS2FPGA_GPR_OFST, "rxd1", 0x000000ff, "RW"},
	{0x720 + HPS2FPGA_GPR_OFST, "rxd2", 0x00000000, "RW"},
	{0x730 + HPS2FPGA_GPR_OFST, "rxd3", 0x00000000, "RW"},
	{0x740 + HPS2FPGA_GPR_OFST, "rxd4", 0x00000000, "RW"},
	{0x750 + HPS2FPGA_GPR_OFST, "rxd5", 0x00000000, "RW"},

	{0x800 + HPS2FPGA_GPR_OFST, "txa0", 0x00000000, "RW"},
	{0x810 + HPS2FPGA_GPR_OFST, "txa1", 0x000000ff, "RW"},
	{0x820 + HPS2FPGA_GPR_OFST, "txa2", 0x00000000, "RW"},
	{0x830 + HPS2FPGA_GPR_OFST, "txa3", 0x00000000, "RW"},
	{0x840 + HPS2FPGA_GPR_OFST, "txa4", 0x00000000, "RW"},
	{0x850 + HPS2FPGA_GPR_OFST, "txa5", 0x00000000, "RW"},
	{0x900 + HPS2FPGA_GPR_OFST, "txb0", 0x00000000, "RW"},
	{0x910 + HPS2FPGA_GPR_OFST, "txb1", 0x000000ff, "RW"},
	{0x920 + HPS2FPGA_GPR_OFST, "txb2", 0x00000000, "RW"},
	{0x930 + HPS2FPGA_GPR_OFST, "txb3", 0x00000000, "RW"},
	{0x940 + HPS2FPGA_GPR_OFST, "txb4", 0x00000000, "RW"},
	{0x950 + HPS2FPGA_GPR_OFST, "txb5", 0x00000000, "RW"},
	{0xa00 + HPS2FPGA_GPR_OFST, "txc0", 0x00000000, "RW"},
	{0xa10 + HPS2FPGA_GPR_OFST, "txc1", 0x000000ff, "RW"},
	{0xa20 + HPS2FPGA_GPR_OFST, "txc2", 0x00000000, "RW"},
	{0xa30 + HPS2FPGA_GPR_OFST, "txc3", 0x00000000, "RW"},
	{0xa40 + HPS2FPGA_GPR_OFST, "txc4", 0x00000000, "RW"},
	{0xa50 + HPS2FPGA_GPR_OFST, "txc5", 0x00000000, "RW"},
	{0xb00 + HPS2FPGA_GPR_OFST, "txd0", 0x00000000, "RW"},
	{0xb10 + HPS2FPGA_GPR_OFST, "txd1", 0x000000ff, "RW"},
	{0xb20 + HPS2FPGA_GPR_OFST, "txd2", 0x00000000, "RW"},
	{0xb30 + HPS2FPGA_GPR_OFST, "txd3", 0x00000000, "RW"},
	{0xb40 + HPS2FPGA_GPR_OFST, "txd4", 0x00000000, "RW"},
	{0xb50 + HPS2FPGA_GPR_OFST, "txd5", 0x00000000, "RW"}
};
static size_t num_regs = sizeof(reg_table)/sizeof(reg_table[0]);

// Gets the number of registers
inline size_t get_num_regs(void) {
	return num_regs;
}

// returns the register of the matching register in the reg_table based on name
// returns NULL if no match or error
const reg_t* get_reg_from_name(const char* name) {
	size_t i;
	for (i = 0; i < num_regs; i++) {
		if (strcmp(reg_table[i].name, name) == 0)
			return (reg_table + i);
	}
	return NULL;
}

// returns the register of the matching register in the reg_table based on offset
// returns NULL if no match or error
const reg_t* get_reg_from_addr(uint32_t addr) {
	size_t i;
	for (i = 0; i < num_regs; i++) {
		if (reg_table[i].addr == addr)
			return (reg_table + i);
	}
	return NULL;
}

// prints out all of the register names and addresses, if verbose, print out bitfields
void print_regs(int verbose) {
	size_t i;
	for (i = 0; i < num_regs; i++) {
		printf("reg = %s\taddress = 0x%04x\tdefault = 0x%08x\tperm = %s\n",
			reg_table[i].name, reg_table[i].addr,	reg_table[i].def_val, reg_table[i].perm);
		if (verbose) {

		}
	}
}
