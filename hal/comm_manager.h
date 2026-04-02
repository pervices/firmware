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

/**
 * Initializes a UDP server socket
 * @param fd Where to store the file descriptor for the new TCP socket
 * @param port The port for the new socket to use
 * @return Return 0 on success. Return negative value on failure. TODO:return -errno of the last operation
 */
int init_udp_comm(int *fd, in_port_t port);

/**
 * Initializes a TCP server socket
 * @param fd Where to store the file descriptor for the new TCP socket
 * @param port The port for the new socket to use
 * @param flags Flags to be passed to socket(2)
 * @return Return 0 on success. On failure return -errno of the last operation
 */
int init_tcp_comm(int *fd, in_port_t port, int flags);

int init_uart_comm(int *fd, const char *dev, uint16_t options);
int close_uart_comm(int fd);
int recv_uart_comm(int fd, uint8_t *data, uint32_t *size, int32_t max_size);
int recv_uart_comm_timeout(int fd, uint8_t *data, uint32_t *size, int32_t max_size, int64_t timeout_us);
int send_uart_comm(int fd, uint8_t *data, uint16_t size);
int flush_uart_comm(int fd); // flushes UART on MCU side

#endif
