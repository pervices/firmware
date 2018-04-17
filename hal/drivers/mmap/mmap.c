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
#include "mmap.h"

// Standard for linux
#define MEM_DEV 	"/dev/mem"

// Options passed from server.c
static uint8_t _options = 0;

void set_mem_debug_opt(uint8_t options) {
	_options = options;
}

static int reg_read(uint32_t addr, uint32_t* data, uint32_t bytes_to_read) {
	int fd, i;
	void* virtual_base;

	// round up the nearest PAGE_SIZE
	uint32_t span = bytes_to_read / PAGE_SIZE;
	if (span < bytes_to_read) span += PAGE_SIZE;

	if( ( fd = open( MEM_DEV, O_RDONLY | O_SYNC ) ) < 0 ) {
		PRINT(ERROR, "%s(), opening, %s\n", __func__, strerror(errno));
		return RETURN_ERROR_COMM_MMAP;
	}

	virtual_base = mmap( NULL, span, PROT_READ, MAP_SHARED, fd, addr & ~(span-1) );

	if( virtual_base == MAP_FAILED ) {
		PRINT(ERROR, "%s(), mmap, %s\n", __func__, strerror(errno));
		close( fd );
		return RETURN_ERROR_COMM_MMAP;
	}

	// Does the actual reading
	for (i = 0; i < bytes_to_read; i++) {
		data[i] = alt_read_word(virtual_base + ((addr + i) & (span - 1)));
	}

	if( munmap( virtual_base, span ) != 0 ) {
		PRINT(ERROR, "%s(), close, %s\n", __func__, strerror(errno));
		close(fd);
		return RETURN_ERROR_COMM_MMAP;
	}

	close(fd);
	return RETURN_SUCCESS;
}

static int reg_write(uint32_t addr, uint32_t* data, uint32_t bytes_to_write) {
	if (addr < ALT_LWFPGASLVS_OFST || addr >= ALT_LWFPGASLVS_OFST + ALT_LWFPGASLVS_SPAN)
		return RETURN_ERROR_ADDR_OUT_OF_RANGE;

	const reg_t *rg = get_reg_from_addr( addr );

	//PRINT(VERBOSE, "%s(): reg: %s data: 0x%08x\n", __func__, rg->name , *data);

	int fd, i;
	void* virtual_base;

	// round up the nearest PAGE_SIZE
	uint32_t span = bytes_to_write / PAGE_SIZE;
	if (span < bytes_to_write) span += PAGE_SIZE;

	if( ( fd = open( MEM_DEV, ( O_RDWR | O_SYNC ) ) ) < 0 ) {
		PRINT(ERROR, "%s(), opening, %s\n", __func__, strerror(errno));
		return RETURN_ERROR_COMM_MMAP;
	}

	virtual_base = mmap( NULL, span, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, addr & ~(span-1) );

	if( virtual_base == MAP_FAILED ) {
		PRINT(ERROR, "%s(), mmap, %s\n", __func__, strerror(errno));
		close( fd );
		return RETURN_ERROR_COMM_MMAP;
	}

	// Does the actual writing
	for (i = 0; i < bytes_to_write; i++) {
		alt_write_word(virtual_base + ((addr + i) & (span -1)), data[i]);
	}

	if( munmap( virtual_base, span ) != 0 ) {
		PRINT(ERROR, "%s(), close, %s\n", __func__, strerror(errno));
		close(fd);
		return RETURN_ERROR_COMM_MMAP;
	}

	close(fd);

	return RETURN_SUCCESS;
}

int burst_read_hps_addr(uint32_t addr, uint32_t* data, size_t bytes_to_read) {
	if (!data) return RETURN_ERROR_PARAM;
	return reg_read( addr, data, bytes_to_read );
}

int busrt_write_hps_addr(uint32_t addr, uint32_t* data, size_t bytes_to_write) {
	if (!data) return RETURN_ERROR_PARAM;
	return reg_write( addr, data, bytes_to_write );
}

int read_hps_addr(uint32_t addr, uint32_t* data) {
	if (!data) return RETURN_ERROR_PARAM;
	return reg_read( addr, data, 1 );
}

int write_hps_addr(uint32_t addr, uint32_t data) {
	return reg_write( addr, &data, 1 );
}

int write_hps_addr_mask(uint32_t addr, uint32_t data, uint32_t mask) {
	uint32_t tmp;
	if (read_hps_addr(addr, &tmp)) return RETURN_ERROR_INVALID_REGISTER;
	return write_hps_addr(addr, (tmp & ~mask) | (data & mask));
}

int read_hps_reg(const char* reg, uint32_t* data) {
	if (!reg || !data) return RETURN_ERROR_PARAM;

	const reg_t* temp = get_reg_from_name(reg);
	if (temp)	return reg_read( temp -> addr, data, 1 );
	else		return RETURN_ERROR_INVALID_REGISTER;
}

int write_hps_reg(const char* reg, uint32_t data) {
	if (!reg) return RETURN_ERROR_PARAM;

	//if (_options & SERVER_DEBUG_OPT)
	PRINT(DEBUG, "%s(): %s: 0x%08x\n", __func__, reg, data);

	const reg_t* temp = get_reg_from_name(reg);
	if (temp)	return reg_write( temp -> addr, &data, 1 );
	else		return RETURN_ERROR_INVALID_REGISTER;
}

int write_hps_reg_mask(const char* reg, uint32_t data, uint32_t mask) {
	uint32_t tmp;
	if (read_hps_reg(reg, &tmp)) return RETURN_ERROR_INVALID_REGISTER;
	return write_hps_reg(reg, (tmp & ~mask) | (data & mask));
}

void list_hps_reg(uint8_t verbosity) {
	print_regs(verbosity);
}

int dump_hps_reg(void) {
	int ret;
	uint32_t data, index;
	for (index = 0; index < get_num_regs(); index++) {
		const reg_t* temp = get_reg_from_index(index);
		ret = reg_read( temp -> addr, &data, 1);
		if (ret < 0) return ret;

		printf("reg = %s\taddress = 0x%08x\tvalue = 0x%08x\n",
			temp -> name, temp -> addr, data);
	}
	return RETURN_SUCCESS;
}
