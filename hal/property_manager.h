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

#ifndef PROPERTY_MANAGER_H_
#define PROPERTY_MANAGER_H_

#include <sys/inotify.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#define UART_DEV "/dev/ttyS1"

int init_property(void);
void check_property_inotifies(void);
void update_status_properties(void);


// Example property: "tx_a/rf/dac/freq"
int get_property(const char* prop, char* data, size_t max_len);
int set_property(const char* prop, const char* data);
// API for the Crimson

#endif
