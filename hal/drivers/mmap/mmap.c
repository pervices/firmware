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

#ifndef HPS2FPGA_GPR_OFST
#define HPS2FPGA_GPR_OFST (0xff200000)
#endif

static int mmap_fd = -1;
static void* mmap_base = MAP_FAILED;
static size_t mmap_len = 0;

// Standard for linux
#define MEM_DEV "/dev/mem"

// Options passed from server.c
static uint8_t _options = 0;

void set_mem_debug_opt(uint8_t options) { _options = options; }

static int reg_read(uint32_t addr, uint32_t* data)
{
    if (MAP_FAILED == mmap_base || -1 == mmap_fd || 0 == mmap_len) {
        return RETURN_ERROR_INSUFFICIENT_RESOURCES;
    }

    volatile uint32_t* mmap_addr
        = (uint32_t*)((uint8_t*)mmap_base - HPS2FPGA_GPR_OFST + addr);
    *data = *mmap_addr;

    return RETURN_SUCCESS;
}

static int reg_write(uint32_t addr, uint32_t* data)
{
    if (MAP_FAILED == mmap_base || -1 == mmap_fd || 0 == mmap_len) {
        return RETURN_ERROR_INSUFFICIENT_RESOURCES;
    }

    volatile uint32_t* mmap_addr
        = (uint32_t*)((uint8_t*)mmap_base - HPS2FPGA_GPR_OFST + addr);
    *mmap_addr = *data;
    msync(mmap_base, mmap_len, MS_SYNC | MS_INVALIDATE);

    return RETURN_SUCCESS;
}

int read_hps_addr(uint32_t addr, uint32_t* data)
{
    if (!data)
        return RETURN_ERROR_PARAM;
    return reg_read(addr, data);
}

int write_hps_addr(uint32_t addr, uint32_t data)
{
    return reg_write(addr, &data);
}

int write_hps_addr_mask(uint32_t addr, uint32_t data, uint32_t mask)
{
    uint32_t tmp;
    if (read_hps_addr(addr, &tmp))
        return RETURN_ERROR_INVALID_REGISTER;
    return write_hps_addr(addr, (tmp & ~mask) | (data & mask));
}

int read_hps_reg(const char* reg, uint32_t* data)
{
    if (!reg || !data)
        return RETURN_ERROR_PARAM;

    const reg_t* temp = get_reg_from_name(reg);
    if (temp)
        return reg_read(temp->addr, data);
    else
        return RETURN_ERROR_INVALID_REGISTER;
}

int write_hps_reg(const char* reg, uint32_t data)
{
    if (!reg)
        return RETURN_ERROR_PARAM;

    // if (_options & SERVER_DEBUG_OPT)
    PRINT(DEBUG, "%s(): %s: 0x%08x\n", __func__, reg, data);

    const reg_t* temp = get_reg_from_name(reg);
    if (temp)
        return reg_write(temp->addr, &data);
    else
        return RETURN_ERROR_INVALID_REGISTER;
}

int write_hps_reg_mask(const char* reg, uint32_t data, uint32_t mask)
{
    uint32_t tmp;
    if (read_hps_reg(reg, &tmp))
        return RETURN_ERROR_INVALID_REGISTER;
    return write_hps_reg(reg, (tmp & ~mask) | (data & mask));
}

void list_hps_reg(uint8_t verbosity) { print_regs(verbosity); }

int dump_hps_reg(void)
{
    int ret;
    uint32_t data, index;
    for (index = 0; index < get_num_regs(); index++) {
        const reg_t* temp = get_reg_from_index(index);
        ret = reg_read(temp->addr, &data);
        if (ret < 0)
            return ret;

        printf("reg = %s\taddress = 0x%08x\tvalue = 0x%08x\n", temp->name,
            temp->addr, data);
    }
    return RETURN_SUCCESS;
}

int mmap_init()
{
    int r;
    void* rr;

    r = open(MEM_DEV, O_RDWR | O_SYNC);
    if (-1 == r) {
        PRINT(ERROR, "mmap( /dev/mem ) failed: %s (%d)\n", strerror(errno),
            errno);
        r = errno;
        goto out;
    }
    mmap_fd = r;

    rr = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd,
        HPS2FPGA_GPR_OFST);
    if (MAP_FAILED == rr) {
        PRINT(ERROR, "mmap( /dev/mem ) failed: %s (%d)\n", strerror(errno),
            errno);
        r = errno;
        goto closefd;
    }
    mmap_base = rr;
    mmap_len = 0x1000;

    r = EXIT_SUCCESS;
    goto out;

closefd:
    close(mmap_fd);
    mmap_fd = -1;

out:
    return r;
}

void mmap_fini()
{
    if (MAP_FAILED != mmap_base) {
        munmap(mmap_base, mmap_len);
        mmap_base = MAP_FAILED;
    }
    if (-1 != mmap_fd) {
        close(mmap_fd);
        mmap_fd = -1;
    }
}
