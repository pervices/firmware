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

#ifndef UART_H_
#define UART_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include "common.h"

int set_uart_interface_attribs (int fd, int speed, int parity);
void set_uart_blocking (int fd, int should_block);
int recv_uart(int fd, uint8_t* data, uint16_t* size, uint16_t max_size);
int send_uart(int fd, uint8_t* data, uint16_t size);
int flush_uart(int fd);
void set_uart_debug_opt(uint8_t options);

#endif
