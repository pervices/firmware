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

/* clang-format off */

#include "property_manager.h"

#if 1
    #include "comm_manager.h"
    #include "common.h"
    #include "mmap.h" // shouldn't need to include this, this is here for errata fixing
    #include "properties.h"
    #include "array-utils.h"
    #include "uart.h"

    #include <stdbool.h>
    #include <unistd.h>
#endif

#include "channels.h"
#include "time_it.h"

/* clang-format on */

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

//#define PROPERTY_MANAGER_DEBUG

#if !(defined(VAUNT) || defined(TATE_NRNT) || defined(LILY))
    #error "You must specify either ( VAUNT | TATE_NRNT | LILY ) when compiling this project."
#endif

static int uart_synth_comm_fd = 0;
#ifdef VAUNT
#ifdef RX_40GHZ_FE
    // In RX_40GHZ_FE mode tx uart buffer are used to communicate with the 40GHz equipment
    static int uart_tx_comm_fd[NUM_RX_CHANNELS];
#else
    static int uart_tx_comm_fd[NUM_TX_CHANNELS];
#endif
static int uart_rx_comm_fd[NUM_RX_CHANNELS];
#elif defined(TATE_NRNT) || defined(LILY)
// TODO: figure out why there should be 32 elements, and soft code this. It should probably be either number of channels present in the configuration, or theoretical maximum number of channles
int uart_tx_comm_fd[32];
int uart_rx_comm_fd[32];
#else
    #error "You must specify either ( VAUNT | TATE_NRNT | LILY ) when compiling this project."
#endif

// Inotify's file descriptor
static int inotify_fd;

// Options passed from server.c
static uint8_t _options = 0;

// Helper function to write to property
static void write_to_file(const char *path, const char *data) {
    FILE *fd;
    if (!(fd = fopen(path, "w"))) {
        PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
        return;
    }
    fprintf(fd, "%s", data);
    fclose(fd);

    // PRINT(VERBOSE, "wrote to file: %s (%s)\n", path, data);
}

// Helper function to read to property
// It is important that this makes sure the buffer null terminates, otherwise data from previously read properties will be used
static void read_from_file(const char *path, char *data, size_t max_len) {
    FILE *fd;

    // Open file
    if (!(fd = fopen(path, "r"))) {
        PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
        return;
    }

    // Read content
    data[0] = '\0';
    int data_read = 0;
    while ( fgets(data + data_read, max_len - data_read, fd) ) {
        data_read = strnlen(data, max_len);
        // Stop reading if the buffer is full (<=1 because the last byte must be null)
        if(max_len - data_read <= 1) {
            break;
        }
    }
    fclose(fd);

    // How big is the file?
    size_t pos = data_read;

    // Ignore any new lines found at the end of the file
    while ( pos > 0 && data[pos - 1] == '\n'){
        pos--;
    }
    data[pos] = '\0';

    // PRINT(VERBOSE, "read from file: %s (%s)\n", path, data);
}

static void change_group_permissions_for_all(void) {

    system("chgrp dev-grp0 -R " BASE_DIR);

}

// Helper function to make properties
static void make_prop(prop_t *prop) {
    const int CMD_LENGTH = 2*MAX_PATH_LEN + 100;
    char cmd[CMD_LENGTH];
    char path[MAX_PATH_LEN];

    switch (prop->type) {

    case PROP_TYPE_FILE:

        // TODO: @CF: The preferred way to build a directory tree relative to
        // some path would be to use mkdirat(2), openat(2), etc. Here, we don't
        // even check return values, which can be dangerous.

        // TODO: @CF: use mkdir(2)
        // mkdir -p /home/root/state/*
        snprintf(cmd, CMD_LENGTH, "mkdir -p %s", get_abs_dir(prop, path, MAX_PATH_LEN));
        system(cmd);
        // Enables execute and read for the directory containing the property
        // execute allow you to enter the directory
        // read allows you to list the contents
        // Not having write enabled prevents creating new files but does not affect writing to existing files
        snprintf(cmd, CMD_LENGTH, "chmod 0555 %s", get_abs_dir(prop, path, MAX_PATH_LEN));
        system(cmd);
        // PRINT( VERBOSE,"executing: %s\n", cmd);

        // TODO: replace with openat(2)
        // touch /home/root/state/*
        snprintf(cmd, CMD_LENGTH, "touch %s", get_abs_path(prop, path, MAX_PATH_LEN));
        system(cmd);
        // PRINT( VERBOSE,"executing: %s\n", cmd);

        // TODO: @CF: use fchmodat(2)
        // if read only property, change permissions
        if (prop->permissions == RO) {
            // chmod a-w
            snprintf(cmd, CMD_LENGTH, "chmod 0444 %s", get_abs_path(prop, path, MAX_PATH_LEN));
            system(cmd);
        } else if (prop->permissions == WO) {
            // TODO: @CF: use fchmodat(2)
            // chmod a-r
            snprintf(cmd, CMD_LENGTH, "chmod 0222 %s", get_abs_path(prop, path, MAX_PATH_LEN));
            system(cmd);
        } else if (prop->permissions == RW) {
            // TODO: @CF: use fchmodat(2)
            // chmod a-r
            snprintf(cmd, CMD_LENGTH, "chmod 0666 %s", get_abs_path(prop, path, MAX_PATH_LEN));
            system(cmd);
        }

        break;

    case PROP_TYPE_SYMLINK:

        // TODO: @CF: The preferred way to build a directory tree relative to
        // some path would be to use mkdirat(2), openat(2), etc. Here, we don't
        // even check return values, which can be dangerous.

        // TODO: @CF: use mkdir(2)
        // mkdir -p /home/root/state/*
        snprintf(cmd, CMD_LENGTH, "mkdir -p %s", get_abs_dir(prop, path, MAX_PATH_LEN));
        system(cmd);
        // PRINT( VERBOSE,"executing: %s\n", cmd);

        snprintf(cmd, sizeof(cmd), "rm -Rf " STATE_DIR "/%s", prop->path);
        system(cmd);

        // TODO: replace with symlinkat(2)
        snprintf(cmd, sizeof(cmd), "cd " STATE_DIR "; ln -sf " STATE_DIR "/%s %s",
                 prop->symlink_target, prop->path);
        system(cmd);
        // PRINT( VERBOSE,"executing: %s\n", cmd);

        break;
    }
}

// Helper function to add the property to inotify
// crimson continues if this fails, but will give an error
static void add_prop_to_inotify(prop_t *prop) {
    char path[MAX_PATH_LEN];

    // check if RO property
    if (prop->permissions != RO) {
        prop->wd = inotify_add_watch(inotify_fd, get_abs_path(prop, path, MAX_PATH_LEN),
                                     IN_CLOSE_WRITE);
    }

    if (prop->wd < 0)
        PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
}

// Helper function to call power-on reset values
static void init_prop_val(prop_t *prop) {
    char path[MAX_PATH_LEN];

    // exceptions for values that must persist through hard resets
    if (strcmp(prop->path, "fpga/link/net/hostname") == 0 ||
        strcmp(prop->path, "fpga/link/net/ip_addr") == 0 ||
        strcmp(prop->path, "save_config") == 0 ||
        strcmp(prop->path, "load_config") == 0) {
        return;
    }

    // if not WO property
    if (prop->permissions != WO) {
        write_to_file(get_abs_path(prop, path, MAX_PATH_LEN), prop->def_val);
    }
}

// TODO: @CF: modify function to first open base directory and then to use
// relative ops (openat, mkdirat, fchownat, fchmodat, etc) Helper function for
// building a tree in the home directory
static void build_tree(void) {

    // Sets up IPs and Ports.
    // This was introduced when the XMACRO
    // in channels.h was developed to help aid the port from VAUNT to TATE.
    patch_tree();

    //dump_tree();

    PRINT(INFO, "\tXXX: Building tree, %i properties found\n", get_num_prop());
    prop_t *prop;

    size_t i;
    for (i = 0; i < get_num_prop(); i++) {
        prop = get_prop(i);
        PRINT(VERBOSE, "\tXXX: %d: Making prop: %s wd: %i\n", i, prop->path, prop->wd);
        make_prop(prop);
        if (PROP_TYPE_SYMLINK != prop->type) {
            add_prop_to_inotify(prop);
            init_prop_val(prop);
        }
    }

    PRINT(INFO, "\tXXX: Changing permissions for all\n");
    change_group_permissions_for_all();

    // force property initofy check (writing of defaults) after init
    PRINT(INFO, "\tXXX: Checking proprety inotifies\n");
    check_property_inotifies();

    PRINT(INFO, "\tXXX: Last wd val: %i\n", get_prop(i - 1)->wd);
    PRINT(INFO, "\tXXX: Done building tree\n");
}

int get_inotify_fd() { return inotify_fd; }

// Initialize handler functions
int init_property(uint8_t options) {

    // uart transactions
    PRINT(VERBOSE, "Initializing UART\n");

    // Save the options
    _options = options;
    set_uart_debug_opt(options);
    set_mem_debug_opt(options);

    // XXX 
    uart_synth_comm_fd = -1;

    for(int i = 0; i < ARRAY_SIZE(uart_tx_comm_fd); i++)
        uart_tx_comm_fd[i] = -1;

    for(int i = 0; i < ARRAY_SIZE(uart_rx_comm_fd); i++)
        uart_rx_comm_fd[i] = -1;

    /* Setup all UART devices XXX: SHOULD RETURN -1 */
    init_uart_comm(&uart_synth_comm_fd, UART_SYNTH, 0);

#if defined(TATE_NRNT) || defined(LILY)
    static char name[512];
    #define X(ch)\
        const int chan_rx_##ch = INT_RX(ch);                                          \
        snprintf(name, 521, UART_RFE "%d", chan_rx_##ch);                              \
        init_uart_comm(&uart_rx_comm_fd[chan_rx_##ch], name, 0);
    RX_CHANNELS
    #undef X
    #define X(ch)\
        const int chan_tx_##ch = INT_TX(ch);                                          \
        snprintf(name, 521, UART_RFE "%d", chan_tx_##ch);                              \
        init_uart_comm(&uart_tx_comm_fd[chan_tx_##ch], name, 0);
    TX_CHANNELS
    #undef X
#elif defined(VAUNT)
    init_uart_comm(&uart_tx_comm_fd[0], UART_TX, 0);
    init_uart_comm(&uart_rx_comm_fd[0], UART_RX, 0);
    for (int i = 1; i < ARRAY_SIZE(uart_tx_comm_fd); i++)
        uart_tx_comm_fd[i] = uart_tx_comm_fd[0];
    for (int i = 1; i < ARRAY_SIZE(uart_rx_comm_fd); i++)
        uart_rx_comm_fd[i] = uart_rx_comm_fd[0];
#else
    #error "You must specify either ( VAUNT | TATE_NRNT | LILY ) when compiling this project."
#endif

    PRINT(INFO, "Configuring Time Board. Using UART: %s\n", UART_SYNTH);
    PRINT(INFO, "%d\n", uart_synth_comm_fd);
    PRINT(INFO, "TX FDS\n");
    for (int i = 0; i < ARRAY_SIZE(uart_tx_comm_fd); i++)
        PRINT(INFO, "%d\n", uart_tx_comm_fd[i]);
    PRINT(INFO, "RX FDS\n");
    for (int i = 0; i < ARRAY_SIZE(uart_rx_comm_fd); i++)
        PRINT(INFO, "%d\n", uart_rx_comm_fd[i]);

    PRINT(VERBOSE, "init_uart_comm(): UART connections up\n");
    PRINT(VERBOSE, "Initializing Inotify\n");

    // inotify
    if ((inotify_fd = inotify_init()) < 0) {
        PRINT(ERROR, "%s, cannot initialize inotify\n", __func__);
        return RETURN_ERROR_INOTIFY;
    }

    // pass the uart handler to the property handlers
    pass_uart_synth_fd(uart_synth_comm_fd);
    pass_uart_tx_fd(uart_tx_comm_fd);
    pass_uart_rx_fd(uart_rx_comm_fd);

    PRINT(INFO, "Building the property tree\n");

    build_tree();

    return RETURN_SUCCESS;
}

// non-standard set property (file modification)
void check_property_inotifies(void) {
    uint8_t buf[EVENT_BUF_LEN];
    char prop_data[MAX_PROP_LEN];
    char prop_ret[MAX_PROP_LEN];
    char path[MAX_PATH_LEN];
    int n;


    // returns if inotify_fd has no bytes to read to prevent server from hanging
    ioctl(inotify_fd, FIONREAD, &n);
    if (n == 0) {
        PRINT(INFO, "No bytes available for read on inotify_fd\n");
        return;
    }

    ssize_t len = read(inotify_fd, buf, EVENT_BUF_LEN);

    ssize_t i = 0;
    while (i < len) {
        // gets the event structure
        struct inotify_event *event = (struct inotify_event *)&buf[i];
        prop_t *prop = get_prop_from_wd(event->wd);

        // check if prop exists, prop will not exist if concurrent modifications
        // were made to the file while in this loop
        if ((event->mask & IN_CLOSE_WRITE) && prop) {
            // PRINT( VERBOSE,"Property located at %s has been modified,
            // executing handler\n", prop -> path);

            // read the change from the file
            read_from_file(get_abs_path(prop, path, MAX_PATH_LEN), prop_data, MAX_PROP_LEN);
            snprintf(prop_ret, MAX_PROP_LEN, prop_data);

            PRINT(INFO, "%s(): set_property( %s, %s )\n", __func__,
                  prop->path, prop_data);

            const int t0 = time_it();
            prop->handler(prop_data, prop_ret);
            const int t1 = time_it();

            PRINT(INFO, "%s :: %s -> %s :: %d\n", path, prop_data, prop_ret, t1 - t0);

            if (prop->permissions == RO) {
                snprintf(prop_ret, MAX_PROP_LEN, "%s", prop->def_val);
            }

            // if the return value didn't change, don't write to file again
            if (strcmp(prop_ret, prop_data) != 0) {
                // temperarily remove property from inotify so the file update
                // won't trigger another inotify event
                if (inotify_rm_watch(inotify_fd, prop->wd) < 0) {
                    PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
                }
                //PRINT(VERBOSE, "Removed inotify, wd: %i\n", prop->wd);

                // write output of handler to property
                write_to_file(get_abs_path(prop, path, MAX_PATH_LEN), prop_ret);

                // re-add property to inotify
                prop->wd = inotify_add_watch(
                    inotify_fd, get_abs_path(prop, path, MAX_PATH_LEN), IN_CLOSE_WRITE);
                if (prop->wd < 0) {
                    PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
                }
                //PRINT(VERBOSE, "Re-added to inotify, wd: %i\n", prop->wd);
            }
        }

        i += sizeof(struct inotify_event) + event->len;
    }
}

// Save properties to file
int save_properties(const char *file) {
    // open the file, if file exists, overwrite the file
    FILE *fout = fopen(file, "w");
    if (!fout)
        return RETURN_ERROR;

    // temp buffer to store the property values
    char prop_val[MAX_PROP_LEN];

    // loop through all properties and write them to file
    size_t i;
    for (i = 0; i < get_num_prop(); i++) {
        prop_t *cur_prop = get_prop(i);
        if (cur_prop && cur_prop->permissions != WO) {
            get_property(cur_prop->path, prop_val, MAX_PROP_LEN);
            fprintf(fout, "%s,%s\n", cur_prop->path, prop_val);
        }
    }

    // close the file
    fclose(fout);

    return RETURN_SUCCESS;
}

// Load properties from file
int load_properties(const char *file) {
    PRINT(VERBOSE, "loading properties from %s!\n", file);

    // open the file for reading
    FILE *fin = fopen(file, "r");
    if (!fin)
        return RETURN_ERROR;

    // temp buffer to store the property values
    char prop[MAX_PROP_LEN] = {0};

    // loop through all properties, if there are incompatibilities, error out
    size_t i;
    for (i = 0; i < get_num_prop(); i++) {

        prop_t *cur_prop = get_prop(i);
        if (!cur_prop) {
            PRINT(ERROR, "invalid property %s\n", prop);
            continue;
        }

        if (PROP_TYPE_SYMLINK == cur_prop->type) {
            continue;
        }

        // read from file and update the property table
        if (fscanf(fin, "%s", prop) == EOF) {
            break;
        }

        // if property exceeded the MAX_PROP_LEN
        prop[MAX_PROP_LEN - 1] = 0;

        // divide up the property and value
        char *prop_val = strchr(prop, ',');
        *prop_val = 0;
        prop_val++;

        // get pointer to current property
        strcpy(cur_prop->def_val, prop_val);

        if (PROP_TYPE_FILE == cur_prop->type) {
            // write the current property to device
            init_prop_val(cur_prop);
        }
    }

    // close the file
    fclose(fin);

    return RETURN_SUCCESS;
}

// Standard get property
int get_property(const char *prop, char *data, size_t max_len) {
    char path[MAX_PATH_LEN];
    prop_t *temp = get_prop_from_cmd(prop);

    // check if valid property
    if (!temp) {
        PRINT(ERROR, "Property: %s does not exist\n", prop);
        return RETURN_ERROR_SET_PROP;
    }

    // check if WO property
    if (temp->permissions == WO) {
        return RETURN_ERROR_GET_PROP;
    }

    read_from_file(get_abs_path(temp, path, MAX_PATH_LEN), data, max_len);

    return RETURN_SUCCESS;
}

int get_channel_for_path(const char *path) {

    // PRINT( VERBOSE,"%s(): %s\n", __func__, NULL == path ? "(null)" : path );

    if (NULL == path) {
        PRINT(ERROR, "No channel corresponding to path: %s\n", path);
        return -1;
    }
    if (strlen(path) < 4) {
        PRINT(ERROR, "No channel corresponding to path: %s\n", path);
        return -1;
    }
    if (1 && 0 != strncmp("rx", path, 2) && 0 != strncmp("tx", path, 2)) {
        PRINT(ERROR, "No channel corresponding to path: %s\n", path);
        // note: this is not necessarily an error (some paths do not begin with
        // rx or tx)
        return -1;
    }

    // PRINT( VERBOSE,"%s(): %s => %d\n", __func__, NULL == path ? "(null)" :
    // path, path[ 3 ] - 'a' );

    return path[3] - 'a';
}

void power_on_channel(bool is_tx, char channel) {
    char buf[MAX_PATH_LEN];
    prop_t *prop;

    snprintf(buf, sizeof(buf), "%s/%c/pwr", is_tx ? "tx" : "rx", channel);
    prop = get_prop_from_cmd(buf);
    if (NULL == prop) {
        PRINT(ERROR, "Cannot find prop for command '%s', channel: %i\n", buf, channel);
        return;
    }
    write_to_file(get_abs_path(prop, buf, MAX_PATH_LEN), "1");
}

// standard set property
int set_property(const char *prop, const char *data) {
    char path[MAX_PATH_LEN];
    prop_t *temp = get_prop_from_cmd(prop);

    // check if valid property
    if (!temp) {
        PRINT(ERROR, "Property: %s does not exist\n", prop);
        return RETURN_ERROR_SET_PROP;
    }

    // check if RO property
    if (temp->permissions == RO) {
        PRINT(ERROR, "Cannot invoke a set on this property\n");
        return RETURN_ERROR_SET_PROP;
    }

#ifdef PROPERTY_MANAGER_DEBUG
    PRINT(INFO, "pwr_en: %i\n", temp->pwr_en);
    PRINT(INFO, "Setting: %s\n", temp->path);
#endif
    if(temp->pwr_en == UP) {
        // enable (turns on and initializes) channel if it has not been enabled yet
        // (enabling the channel later will erase the current channels, so enable
        // now)
        //This is what causes the power to turn on every time anthing with tx or rx in its path is called
        //This is the old method
        PRINT(ERROR, "Old method of choosing if a board needs to be powered on removed\n");
    //powers of channels if they are not already on
    } else if(temp->pwr_en == RP) {
        power_on_channel(0, *(temp->ch));
    } else if(temp->pwr_en == TP) {
        power_on_channel(1, *(temp->ch));
    }

    write_to_file(get_abs_path(temp, path, MAX_PATH_LEN), data);

    check_property_inotifies();

    return RETURN_SUCCESS;
}

// Pass the pointers for load/saving profiles flags
void pass_profile_pntr_manager(uint8_t *load, uint8_t *save, char *load_path,
                               char *save_path) {
    pass_profile_pntr_prop(load, save, load_path, save_path);
}

int property_good(char *path) {
    // returns 1 if property is good else returns 0
    char property_read[5];
    char fullpath[200] = STATE_DIR "/";
    
    snprintf(&fullpath[0], 200, path);
    // need to write to the property to force an update
    set_property(&fullpath[0],"0");
    // then read from the property
    get_property(&fullpath[0],property_read,5);

    if(strstr(&property_read[0],"good") == NULL){
        return 0;
    }
    return 1;
}

int clr_property_bit(const char *prop, int bit) {
    int err;
    uint32_t reg;
    char buf[MAX_PATH_LEN];
    err = get_property(prop, buf, MAX_PROP_LEN);
    if (err != RETURN_SUCCESS) {
        return err;
    }
    if (sscanf(buf, "0x%x", &reg) != 1) {
        return RETURN_ERROR;
    }
    reg &= ~(1 << bit);
    snprintf(buf,MAX_PROP_LEN, "0x%x",reg);
    return set_property(prop, buf);
}

int set_property_bit(const char *prop, int bit) {
    int err;
    uint32_t reg;
    char buf[MAX_PATH_LEN];
    err = get_property(prop, buf, MAX_PROP_LEN);
    if (err != RETURN_SUCCESS) {
        return err;
    }
    if (sscanf(buf, "0x%x", &reg) != 1) {
        return RETURN_ERROR;
    }
    reg |= 1 << bit;
    snprintf(buf,MAX_PROP_LEN, "0x%x",reg);
    return set_property(prop, buf);
}
