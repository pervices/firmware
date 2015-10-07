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

#ifndef PROPERTIES_H_
#define PROPERTIES_H_

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pwd.h>
#include <math.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "comm_manager.h"
#include "common.h"

typedef enum {RW, RO, WO} perm_t;
typedef enum {POLL, NO_POLL} poll_t;

typedef struct prop {
	char path[MAX_PROP_LEN];
	int (*handler)(const char* data, char* ret);
	perm_t permissions;
	char def_val[MAX_PROP_LEN];	// default value
	int wd;		// inotify watch descriptor
} prop_t;

// Inline functions
inline size_t get_num_prop(void);
inline prop_t* get_prop(size_t idx);
inline prop_t* get_prop_from_wd(int wd);
inline prop_t* get_prop_from_cmd(const char* cmd);
char* get_abs_path(prop_t* prop, char* path);
char* get_abs_dir(prop_t* prop, char* path);
char* get_root (prop_t* prop, char* root);
inline void pass_uart_synth_fd(int fd);
inline void pass_uart_tx_fd(int fd);
inline void pass_uart_rx_fd(int fd);
void pass_profile_pntr_prop(uint8_t* load, uint8_t* save, char* load_path, char* save_path);

#endif
