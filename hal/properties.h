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

#ifndef PROPERTIES_H_
#define PROPERTIES_H_

#include "comm_manager.h"
#include "common.h"
#include "decim_gain_lut.h"
#include "interp_gain_lut.h"
#include "pllcalc.h"
#include <arpa/inet.h>
#include <math.h>
#include <net/if.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef RTM3
    #define RTM_VER 3
#elif RTM4
    #define RTM_VER 4
#else
    #error "This file must be compiled with a valid hardware revision (RTM3, RTM4)"
#endif

#define INTERBOOT_DATA "/var/lib/cyan-server/"

//directory of the state tree
#define BASE_DIR "/var/cyan"
//state tree
#define STATE_DIR "/var/cyan/state"

typedef enum { RW, RO, WO } perm_t;
typedef enum { POLL, NO_POLL } poll_t;

typedef enum {
    PROP_TYPE_FILE,
    PROP_TYPE_SYMLINK,
} prop_type_t;

//records stores wether or not to turn on the tx or rx board before performing the task
//PROP_UNSPECIFIED_PWR means to use the old very clunky way
//It is being kept so that older stuff doesn't need to be tested with the new methd and should be phased out
typedef enum {
    TP = 3, //indicates to set a tx board on
    RP = 2, //indicates to set an rx board on
    SP = 1, //indicates not to seta board to on
    UP = 0, //indicates that is is unspecified whether or not to set a channel power (uses old method)
} prop_pwr_t;

//constant used in DEFINE_FILE_PROP_P, when a channel number is not applicable
#define NAC "-1"

typedef struct prop {
    prop_type_t type;
    char path[MAX_PATH_LEN];
    char symlink_target[MAX_PATH_LEN];
    int (*handler)(const char *data, char *ret);
    perm_t permissions;
    char def_val[MAX_DEFAULT_PROP_LEN]; // default value
    int wd;                     // inotify watch descriptor
    prop_pwr_t pwr_en; //stores wether or not to make sure a board is on before setting the property
    char *ch;//channel number, only relevant for rx and tx (it is a string because of the way macros work, in practice it should only be used as a char)

} prop_t;

// Externed functions
size_t get_num_prop(void);
prop_t *get_prop(size_t idx);
prop_t *get_prop_from_wd(int wd);
prop_t *get_prop_from_hdlr(int (*hdlr)(const char *, char *));
prop_t *get_prop_from_cmd(const char *cmd);
char *get_abs_path(prop_t *prop, char *path);
char *get_abs_dir(prop_t *prop, char *path);
char *get_root(prop_t *prop, char *root);

void pass_uart_synth_fd(int fd);
void pass_uart_tx_fd(int *fd);
void pass_uart_rx_fd(int *fd);

void pass_profile_pntr_prop(uint8_t *load, uint8_t *save, char *load_path,
                            char *save_path);
void sync_channels(uint8_t chan_mask);
void jesd_reset_all();
void set_pll_frequency(int uart_fd, uint64_t reference, pllparam_t *pll,
                       bool tx, uint32_t channel);

void dump_tree(void);

/* Expanding the server from 4 channels (Vaunt) to 16 channels (Tate)
 * broke some static element settings in the property table. Running
 * this tree patch will fix it */

void patch_tree(void);

#endif
