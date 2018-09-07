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

#ifndef COMM_MANAGER_H_
#define COMM_MANAGER_H_

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>

#define UDP_PAYLOAD_LEN 1472
#define MAX_UART_RET_LEN 4096

int init_udp_comm(int *fd, const char *dev, in_port_t port, uint16_t options);
int close_udp_comm(int fd);
int recv_udp_comm(int fd, uint8_t *data, uint16_t *size, uint16_t max_size);
int send_udp_comm(int fd, uint8_t *data, uint16_t size);

int init_uart_comm(int *fd, const char *dev, uint16_t options);
int close_uart_comm(int fd);
int recv_uart_comm(int fd, uint8_t *data, uint16_t *size, uint16_t max_size);
int send_uart_comm(int fd, uint8_t *data, uint16_t size);
int flush_uart_comm(int fd); // flushes UART on MCU side

#endif
