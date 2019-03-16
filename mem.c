#include <stdio.h>
#include "common.h"
#include "mmap.h"

#define ARG_MEM_READ "mr"
#define ARG_MEM_WRITE "mw"
#define ARG_MEM_DUMP "md"
#define ARG_REG_READ "rr"
#define ARG_REG_WRITE "rw"
#define ARG_REG_DUMP "rd"
#define ARG_REG_LIST "rl"

int main(int argc, char *argv[]) {
    char *reg;            // register
    uint32_t addr, value; // r/w address, and value if applicable
    int verbose = 0;      // printing verbosity, default, not verbose
    int length = 4;
    uint32_t rval;              // value that is read
    uint32_t mask = 0xffffffff; // default mask if not specified

    int r = mmap_init();
    if (EXIT_SUCCESS != r) {
        PRINT(ERROR, "mmap_init failed\n");
        return r;
    }
    atexit(mmap_fini);

    /* Parse arguments */
    // if command is memory write
    if ((argc == 5 || argc == 4) && strcmp(argv[1], ARG_MEM_WRITE) == 0) {
        sscanf(argv[2], "%x", &addr);
        sscanf(argv[3], "%x", &value);

        if (argc == 5) {
            sscanf(argv[4], "%x", &mask);
            write_hps_addr_mask(addr, value, mask);
        } else {
            write_hps_addr(addr, value);
        }

        // if command is memory read
    } else if (argc == 3 && strcmp(argv[1], ARG_MEM_READ) == 0) {
        sscanf(argv[2], "%x", &addr);
        read_hps_addr(addr, &rval);
        printf("0x%08x\n", rval);

        // if command is memory dump
    } else if (argc == 4 && strcmp(argv[1], ARG_MEM_DUMP) == 0) {
        sscanf(argv[2], "%x", &addr);
        sscanf(argv[3], "%i", &length);
        if (length % 4 != 0)
            printf("Not 32-bit alligned! Rounded down to nearest 32-bit "
                   "register!\n");

        uint32_t index;
        for (index = 0; index < length; index += 4) {
            read_hps_addr(addr + index, &rval);
            if (index % 16 == 0) {
                if (index != 0)
                    printf("\n");
                printf("0x%08x: ", addr + index);
            }
            printf("0x%08x ", rval);
        }
        printf("\n");

        // if command is register write
    } else if ((argc == 5 || argc == 4) &&
               strcmp(argv[1], ARG_REG_WRITE) == 0) {
        reg = argv[2];
        sscanf(argv[3], "%x", &value);

        if (argc == 5) {
            sscanf(argv[4], "%x", &mask);
            write_hps_reg_mask(reg, value, mask);
        } else {
            write_hps_reg(reg, value);
        }

        // if command is register read
    } else if (argc == 3 && strcmp(argv[1], ARG_REG_READ) == 0) {
        reg = argv[2];
        read_hps_reg(reg, &rval);
        printf("0x%08x\n", rval);

        // if command is register dump
    } else if ((argc == 2) && strcmp(argv[1], ARG_REG_DUMP) == 0) {
        dump_hps_reg();

        // if command is register list (execution ends here)
    } else if ((argc == 2 || argc == 3) && strcmp(argv[1], ARG_REG_LIST) == 0) {
        if (argc == 3)
            sscanf(argv[3], "%x", &verbose);
        list_hps_reg(verbose);
        return 0;

        // usage menu
    } else {
        printf("Usage: mem [%s|%s|%s|%s|%s|%s|%s] [address|reg_name|verbosity] "
               "[value|length|mask] [mask]\n",
               ARG_MEM_READ, ARG_MEM_WRITE, ARG_MEM_DUMP, ARG_REG_READ,
               ARG_REG_WRITE, ARG_REG_DUMP, ARG_REG_LIST);
        return 0;
    }

    return 0;
}
