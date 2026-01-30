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

// Used for mutexs for avoid contention with other processes
#include <pthread.h>
#include <sys/file.h>
#include <fcntl.h>

static int mmap_fd = -1;
static int mmap_mutx_fd = -1;
static pthread_mutex_t* mutex = NULL;
static void *mmap_base = MAP_FAILED;
static size_t mmap_len = 0;

// Standard for linux
#define MEM_DEV "/dev/mem"

// Path to shared memory to store mutex for accessing FPGA regs
#define MEM_MUTEX "/mmap_mutex"

#define INIT_LOCK "/var/lock/mmap_init"

// Options passed from server.c
static uint8_t _options = 0;

void set_mem_debug_opt(uint8_t options) { _options = options; }

// Gets a lock on the mutex that controls access to FPGA regs
// Must release it later with pthread_mutex_unlock
// A lock is required since having 2 processes attempt to read from registers at the same time freezes Linux
// The freeze occurs even if accessing different registers
static inline void get_lock() {
    time_t deadline = time(NULL) + 30;

    // Flag used to only print timeout message once
    int msg_printed = 0;

    while(pthread_mutex_trylock(mutex)) {
        if(errno == 0) {
            usleep(1);
        }
        if(time(NULL) >= deadline && !msg_printed) {
            PRINT(ERROR, "pthread_mutex_trylock has taken more than 30 seconds to get the lock. Is likely to hang forever: %s (%d)\n", strerror(errno), errno);
            msg_printed = 1;
            break;
        }
    }
}

static int reg_read(uint32_t addr, uint32_t *data) {
    if (MAP_FAILED == mmap_base || -1 == mmap_fd || 0 == mmap_len) {
        return RETURN_ERROR_INSUFFICIENT_RESOURCES;
    }

    volatile uint32_t *mmap_addr =
        (uint32_t *)((uint8_t *)mmap_base + addr - HPS2FPGA_GPR_OFST);

    get_lock();

    *data = *mmap_addr;

    // Release lock
    if(pthread_mutex_unlock(mutex)) {
        PRINT(ERROR, "pthread_mutex_unlock failed: %s (%d)\n", strerror(errno), errno);
    }

    return RETURN_SUCCESS;
}

static int reg_write(uint32_t addr, uint32_t *data) {
    // Rewrite to all reg first
    rewrite_all_reg();
    if (MAP_FAILED == mmap_base || -1 == mmap_fd || 0 == mmap_len) {
        return RETURN_ERROR_INSUFFICIENT_RESOURCES;
    }

    volatile uint32_t *mmap_addr =
        (uint32_t *)((uint8_t *)mmap_base + addr - HPS2FPGA_GPR_OFST);

    get_lock();

    *mmap_addr = *data;

    // Release lock
    pthread_mutex_unlock(mutex);
    if(pthread_mutex_unlock(mutex)) {
        PRINT(ERROR, "pthread_mutex_unlock failed: %s (%d)\n", strerror(errno), errno);
    }

    // FIXME: This command always returns with an error, it may be the reason why so many regwrites that shouldn't need delays require them
    msync(mmap_base, mmap_len, MS_SYNC | MS_INVALIDATE);

    return RETURN_SUCCESS;
}

int read_hps_addr(uint32_t addr, uint32_t *data) {
    if (!data)
        return RETURN_ERROR_PARAM;
    return reg_read(addr, data);
}

int write_hps_addr(uint32_t addr, uint32_t data) {
    return reg_write(addr, &data);
}

int write_hps_addr_mask(uint32_t addr, uint32_t data, uint32_t mask) {
    uint32_t tmp;
    if (read_hps_addr(addr, &tmp))
        return RETURN_ERROR_INVALID_REGISTER;
    return write_hps_addr(addr, (tmp & ~mask) | (data & mask));
}

int read_hps_reg(const char *reg, uint32_t *data) {
    if (!reg || !data)
        return RETURN_ERROR_PARAM;

    const reg_t *temp = get_reg_from_name(reg);
    if (temp) {
        return reg_read(temp->addr, data);
    }
    else {
        *data = 0;
        return RETURN_ERROR_INVALID_REGISTER;
    }
}

int write_hps_reg(const char *reg, uint32_t data) {
    if (!reg)
        return RETURN_ERROR_PARAM;

    //PRINT(VERBOSE, "%s(): %s: 0x%08x\n", __func__, reg, data);

    const reg_t *temp = get_reg_from_name(reg);
    if (temp) {
        return reg_write(temp->addr, &data);
    }
    else {
        return RETURN_ERROR_INVALID_REGISTER;
    }
}

int write_hps_reg_mask(const char *reg, uint32_t data, uint32_t mask) {
    uint32_t tmp;
    if (read_hps_reg(reg, &tmp))
        return RETURN_ERROR_INVALID_REGISTER;
    return write_hps_reg(reg, (tmp & ~mask) | (data & mask));
}

void list_hps_reg(uint8_t verbosity) { print_regs(verbosity); }

int dump_hps_reg(void) {
    int ret;
    uint32_t data, index;
    for (index = 0; index < get_num_regs(); index++) {
        const reg_t *temp = get_reg_from_index(index);
        ret = reg_read(temp->addr, &data);
        if (ret < 0)
            return ret;

        printf("reg = %s\taddress = 0x%08x\tvalue = 0x%08x\n", temp->name,
               temp->addr, data);
    }
    return RETURN_SUCCESS;
}

// Excempt register from check
#define CHECK_EXCEMPT 1
// CHECK_SPECIAL_* indicates the the register is a special case
// sys0 can cause a system reboot when bit 16 is low for 7 seconds
#define CHECK_SPECIAL_SYS0 2
#define CHECK_SPECIAL_RST_REQ0 3
//compares the before and after values of all registers when writting to a register
//writes the current values of each register to itself
int check_hps_reg(void) {
    printf("Begining register check\n");
    int ret;
    //check index is the register being checked for side effects, index is ther register being checked for change
    uint32_t check_index, index, new_val;
    uint32_t dummy_data = 0;
    uint8_t exempt_regs[get_num_regs()];
    uint32_t old_val[get_num_regs()];
    //generates the list of registers to exempt from the test
    for(index = 0; index < get_num_regs(); index++) {
        // System reset
        if(strstr(get_reg_from_index(index)->name, "sys0") != 0) {
            exempt_regs[index] = CHECK_SPECIAL_SYS0;
        // FPGA reset
        } else if(strstr(get_reg_from_index(index)->name, "rst_req0") != 0) {
            exempt_regs[index] = CHECK_SPECIAL_RST_REQ0;
        } else if(strstr(get_reg_from_index(index)->perm, "RO") != 0) {
            exempt_regs[index] = CHECK_EXCEMPT;
        } else {
            exempt_regs[index] = 0;
        }
    }
    for(check_index = 0; check_index < get_num_regs(); check_index++) {
        if(exempt_regs[check_index] == CHECK_EXCEMPT) {
            continue;
        }

        // Gets the current value of every relevant register
        for (index = 0; index < get_num_regs(); index++) {
            if(exempt_regs[index] == CHECK_EXCEMPT) {
                continue;
            }
            const reg_t *temp = get_reg_from_index(index);
            ret = reg_read(temp->addr, &old_val[index]);
            //returns on error
            if (ret < 0)
                return ret;
        }

        if(exempt_regs[check_index] == CHECK_SPECIAL_RST_REQ0) {
            // Sets rst_req0 to a value that won't trigger a reset
            dummy_data = 0xffffffff;
        } else {
            dummy_data = 0;
            //searches for a value to write to the checked register that is not currently in use anywhere
            for(int n = 0; n < get_num_regs(); n++) {
                if(dummy_data == old_val[n]) {
                    dummy_data++;
                    n = 0;
                }
            }
        }

        // Writes the register to be checked to the value not used anywhere
        ret = reg_write(get_reg_from_index(check_index)->addr, &dummy_data);
        old_val[check_index] = dummy_data;
        if(ret <0) return ret;

        // Verifies no other registers have unexpected changes
        for (index = 0; index < get_num_regs(); index++) {
            if(exempt_regs[index] == CHECK_EXCEMPT) {
                continue;
            }
            const reg_t *temp = get_reg_from_index(index);
            ret = reg_read(temp->addr, &new_val);
            //returns on error
            if (ret < 0)
                return ret;
            if(new_val!=old_val[index]) {
                printf("reg = %s caused a change in reg = %s\n", get_reg_from_index(check_index)->name, get_reg_from_index(index)->name);
            }
        }

        // Makes sure sys0 bit 16 is high to prevent a reboot
        if(exempt_regs[check_index] == CHECK_SPECIAL_SYS0) {
            uint32_t good_sys0_val = 0x00010000;
            reg_write(get_reg_from_index(check_index)->addr, &good_sys0_val);
        }
    }
    printf("Register check complete\n");
    return RETURN_SUCCESS;
}

// Rewrite current values of each register that is not exempted
int rewrite_all_reg(void) {
    int ret;
    //check index is the register being checked for side effects, index is ther register being checked for change
    uint32_t check_index, index, new_val;
    uint8_t exempt_regs[get_num_regs()];
    uint32_t old_val[get_num_regs()];
     printf("Begining register rewrite...\n");
    //generates the list of registers to exempt from the test
    for(index = 0; index < get_num_regs(); index++) {
        // Just exempting the special cases instead of handling them like check_hps_reg does
        // System reset
        if(strstr(get_reg_from_index(index)->name, "sys0") != 0) {
            exempt_regs[index] = CHECK_EXCEMPT;
        // FPGA reset
        } else if(strstr(get_reg_from_index(index)->name, "rst_req0") != 0) {
            exempt_regs[index] = CHECK_EXCEMPT;
        } else if(strstr(get_reg_from_index(index)->perm, "RO") != 0) {
            exempt_regs[index] = CHECK_EXCEMPT;
        } else {
            exempt_regs[index] = 0;
        }
    }

    // Get values of each register
    for (index=0; index < get_num_regs(); index++) {
        // Skip exempted registers
        if(exempt_regs[index] == CHECK_EXCEMPT) {
            continue;
        }
        const reg_t *temp = get_reg_from_index(index);
        ret = reg_read(temp->addr, &old_val[index]);
        if (ret < 0)
            return ret;
    }

    // Rewrites values to all registers
    for (index = 0; index < get_num_regs(); index++) {
        // Skip exempted regs
        if(exempt_regs[index] == CHECK_EXCEMPT) {
            continue;
        }
        // Rewrite existing val to reg
        ret = reg_write(get_reg_from_index(index)->addr, &old_val[index]);
        if (ret < 0)
            return ret;
    
        // Verifies no other registers have unexpected changes
        for (check_index = 0; check_index < get_num_regs(); check_index++) {
            if(exempt_regs[check_index] == CHECK_EXCEMPT) {
                continue;
            }
            const reg_t *temp = get_reg_from_index(check_index);
            ret = reg_read(temp->addr, &new_val);
            //returns on error
            if (ret < 0)
                return ret;
            if(new_val!=old_val[check_index]) {
                printf("reg = %s caused a change in reg = %s\n", get_reg_from_index(index)->name, get_reg_from_index(check_index)->name);
            }
        }
    }

    printf("Register rewrite complete\n");
    return RETURN_SUCCESS;
}

int mmap_init() {
    int lock_fd = -1;
    int r;
    void *rr;

    r = open(MEM_DEV, O_RDWR | O_SYNC, 640);
    if (-1 == r) {
        PRINT(ERROR, "mmap( /dev/mem ) failed: %s (%d)\n", strerror(errno),
              errno);
        r = errno;
        goto out;
    }
    mmap_fd = r;

#if defined(TATE_NRNT) || defined(LILY)
    mmap_len = 0x4000;
#elif defined(VAUNT) || defined(AVERY)
    mmap_len = 0x1000;
#else
    #error "You must specify either ( VAUNT | AVERY | TATE_NRNT | LILY ) when compiling this project."
#endif

    rr = mmap(NULL, mmap_len, PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd,
              HPS2FPGA_GPR_OFST);
    if (MAP_FAILED == rr) {
        PRINT(ERROR, "mmap( /dev/mem ) failed: %s (%d)\n", strerror(errno),
              errno);
        r = errno;
        goto closefd;
    }
    mmap_base = rr;

    lock_fd = open(INIT_LOCK, O_CREAT | O_RDONLY,1);
    if(lock_fd == -1) {
        PRINT(ERROR, "open ( " INIT_LOCK " ) failed: %s (%d)\n", strerror(errno), errno);
        r = lock_fd;
        goto closefd;
    }

    // Lock to prevent race conditions when initializing mutex
    r = flock(lock_fd, LOCK_EX);
    if(r == -1) {
        PRINT(ERROR, "flock ( " INIT_LOCK " ) failed: %s (%d)\n", strerror(errno), errno);
        goto closefd;
    }

    // Attempt to open shared memory to store mutex
    // Using O_CREAT and O_EXCL is expected to fail if another process has already created the shared memory
    mmap_mutx_fd = shm_open(MEM_MUTEX, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

    int mutex_init_required;
    if(mmap_mutx_fd == -1) {
        // Using O_CREAT and O_EXCL failed, meaning that another thread is handling initializing the mutex
        mutex_init_required = 0;
        mmap_mutx_fd = shm_open(MEM_MUTEX, O_RDWR, S_IRUSR | S_IWUSR);

        if(mmap_mutx_fd == -1) {
            PRINT(ERROR, "shm_open( " MEM_MUTEX " ) failed: %s (%d)\n", strerror(errno), errno);
            r = errno;
            goto closefd;
        }
    } else {
        // Set the size of shared memory
        if (ftruncate(mmap_mutx_fd, sizeof(pthread_mutex_t)) == -1) {
            PRINT(ERROR, "ftruncate( " MEM_MUTEX " ) failed: %s (%d)\n", strerror(errno), errno);
            r = errno;
            goto closefd;
        }
        // Using O_CREAT and O_EXCL succeeded, meaning that the shared memory has not been initialized yet
        mutex_init_required = 1;
    }

    mutex = (pthread_mutex_t*) mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED, mmap_mutx_fd, 0);
    if(mutex == MAP_FAILED) {
        r = errno;
        PRINT(ERROR, "mutex mmap failed: %s (%d)\n", strerror(errno), errno);
        goto closefd;
    }

    // Create the mutex to control access to the FPGA regs
    if(mutex_init_required) {
        r = pthread_mutex_init(mutex, NULL);
        if(r == -1) {
            PRINT(ERROR, "pthread_mutex_init failed: %s (%d)\n", strerror(errno), errno);
            goto closefd;
        }
    }

    // Unlock since mutex has been initialized
    r = flock(lock_fd, LOCK_UN);
    if(r == -1) {
        PRINT(ERROR, "flock unlock failed: %s (%d)\n", strerror(errno), errno);
        goto closefd;
    }
    close(lock_fd);

    r = EXIT_SUCCESS;
    goto out;

closefd:
    flock(lock_fd, LOCK_UN);
    close(lock_fd);
    close(mmap_mutx_fd);
    close(mmap_fd);
    mmap_fd = -1;

out:
    return r;
}

void mmap_init_regs(uint8_t verbosity) {
    size_t num_regs = get_num_regs();
    const reg_t *curr_reg;
    for (size_t i = 0; i < num_regs; i++) {
        curr_reg = get_reg_from_index(i);
        if(strcmp(curr_reg->perm,"RO")){ // for each non-read-only register
            if (verbosity) {
                printf("WRITE\treg = %s\taddress = 0x%04x\tdefault = 0x%08x\tperm = %s\n",
                   curr_reg->name, curr_reg->addr, curr_reg->def_val, curr_reg->perm);
            }
            write_hps_addr(curr_reg->addr, curr_reg->def_val);
        } else if (verbosity) {
            printf("SKIP\treg = %s\taddress = 0x%04x\tdefault = 0x%08x\tperm = %s\n",
                   curr_reg->name, curr_reg->addr, curr_reg->def_val, curr_reg->perm);
        }
    }
}

void mmap_fini() {
    if (MAP_FAILED != mmap_base) {
        munmap(mmap_base, mmap_len);
        mmap_base = MAP_FAILED;
    }
    if (-1 != mmap_fd) {
        close(mmap_fd);
        mmap_fd = -1;
    }
}

#if defined(TATE_NRNT) || defined(LILY)
//the jesd_shift = JESD_SHIFT_RX or JESD_SHIFT_TX + INT(ch)
int read_jesd_reg(uint8_t jesd_shift, uint32_t address, uint32_t *data) {
    uint32_t jesd_id = 1 << jesd_shift;
    int error_code = write_hps_reg("net6", jesd_id);
    error_code |= write_hps_reg("net7", address/4);
    error_code |= write_hps_reg("net9", 0x1);
    error_code |= write_hps_reg("net9", 0x0);
    error_code |= read_hps_reg("res_ro30", data);
    return error_code;
}

//the jesd_shift = JESD_SHIFT_RX or JESD_SHIFT_TX + INT(ch)
int write_jesd_reg(uint8_t jesd_shift, uint32_t address, uint32_t data) {
    uint32_t jesd_id = 1 << jesd_shift;
    int error_code = write_hps_reg("net6", jesd_id);
    error_code |= write_hps_reg("net7", address/4);
    error_code |= write_hps_reg("net8", data);
    error_code |= write_hps_reg("net9", 0x2);
    error_code |= write_hps_reg("net9", 0x0);
    return error_code;
}

int write_jesd_reg_mask(uint8_t jesd_shift, uint32_t address, uint32_t data, uint32_t mask) {
    uint32_t tmp = 0;
    if (read_jesd_reg(jesd_shift, address, &tmp)) {
        return RETURN_ERROR_INVALID_REGISTER;
    }
    return write_jesd_reg(jesd_shift, address, (tmp & ~mask) | (data & mask));
}
#elif defined(VAUNT) || defined(AVERY)
// NO-OP
#else
    #error "You must specify either ( VAUNT | AVERY | TATE_NRNT | LILY ) when compiling this project."
#endif
