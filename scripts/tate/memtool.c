#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>

void print_help() {
    printf ("memtool: Utility to access registers inside FPGA\n");
    printf ("------------------------------------------------\n");
    printf ("   memtool <Read Address>\n");
    printf ("   memtool <Write Address> <Write Data>\n");
}

// Get device resource from the given environment variable
char* get_resource(char* env_name) {
    char * dev_resource = NULL;
    dev_resource = getenv(env_name);
    return dev_resource;
}

int main(int argc, char** argv)
{
    // Acquire the MEM_RESOURCE environment variable which points
    // to the memory resource that we will mmap.
    // char * dev_resource = get_resource("MEM_RESOURCE");
    char * dev_resource = "/dev/mem";

    if (dev_resource == NULL) {
        printf("Invalid MEM_RESOURCE. Please check your environment variable\n");
        exit(1);
    }

    enum operation {
        READ = 0,
        WRITE = 1
    } operation;
    uint32_t address;
    uint32_t write_data;

    if (argc == 2) {
        address = strtoul(argv[1], NULL, 0);
        operation = READ;
        
    } else if (argc == 3) {
        address = strtoul(argv[1], NULL, 0);
        write_data = strtoul(argv[2], NULL, 0);
        operation = WRITE;
    } else {
        printf ("Invalid Arguments\n");
        print_help();
        return -1;
    }

    // Make sure memory base address is pageSize aligned
    const uint32_t base_address = address & ~(getpagesize()-1);
    // Calculate the offset from memory base address
    // Assuming 32-bit addressing
    const uint32_t offset = (address - base_address)/4;

    const int fd = open(dev_resource, O_RDWR|O_SYNC);
    uint32_t* memory = (uint32_t*) mmap(0, getpagesize(),
                       PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_address);

    if (operation == READ) {
        // Read Operation
        printf("Read 0x%08x => 0x%08x\n", address, *(memory+offset));
    } else if ( operation == WRITE ) {
        // Write Operation
        *(memory+offset) = write_data;
        printf("Write 0x%08x <= 0x%08x\n", address, write_data);
    }
    return 0;
}
