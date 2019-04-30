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

#ifndef PROPERTY_MANAGER_H_
#define PROPERTY_MANAGER_H_

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

/* TATE ports are expanded with xmacros
* in hal/{channels.h,property_manager.c,properties.c} */
#if defined(VAUNT)
    #define UART_TX "/dev/ttycrimson-tx"
    #define UART_RX "/dev/ttycrimson-rx"
    #define UART_SYNTH "/dev/ttycrimson-time"
#elif defined(TATE)
    #define UART_SYNTH "/dev/ttycyan-time"
    #define UART_CYAN_RFE "/dev/ttycyan-rfe-"
#else
    #error "This file must be compiled with either -DTATE or -DVAUNT. Confirm spelling and spaces."
#endif

int get_inotify_fd();

int init_property(uint8_t options);
void check_property_inotifies(void);
void update_status_properties(void);
int save_properties(const char *file);
int load_properties(const char *file);
void pass_profile_pntr_manager(uint8_t *load, uint8_t *save, char *load_path,
                               char *save_path);
// Example property: "tx_a/rf/dac/freq"
int get_property(const char *prop, char *data, size_t max_len);
int set_property(const char *prop, const char *data);

#endif
