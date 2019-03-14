//
// Copyright 2019 Per Vices Corporation
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

#include "comm_manager.h"
#include "uart.h"
#include "udp.h"

#define MAX_DEVICES 32
#define USED_DEVICE 1
#define FREE_DEVICE 0

/**************************************
 * UDP Driver Manager
 **************************************/

int init_udp_comm(int *fd, const char *dev, in_port_t port, uint16_t options) {
    int ret = RETURN_SUCCESS;

    // Allocate space for udp device
    udp_dev_t mydev = {
        .sockfd = -1,
    };

    PRINT(VERBOSE, "Calling establish_udp_connection()\n");

    ret = establish_udp_connection(&mydev, port);
    if (ret < 0) {
        close_udp_comm(*fd);
    }

    *fd = mydev.sockfd;

    return ret;
}

int close_udp_comm(int fd) {

    close(fd);

    return RETURN_SUCCESS;
}

/**************************************
 * UART Driver Manager
 **************************************/
// Array of UART devices, file descriptors
static int uart_devices[MAX_DEVICES];

static uint8_t used_uart_devices[MAX_DEVICES] = {
    USED_DEVICE, // stdin
    USED_DEVICE, // stdout
    USED_DEVICE, // stderr
};

// Gets the next available file descriptor
static int get_next_uart_fd(int *fd) {

    //
    // ASSUMING stdin / stdout / stderr occupy the file descriptors below 3,
    // fd UART devies
    //
    int i;
    for (i = 0; i < MAX_DEVICES; i++) { // XXX
        if (used_uart_devices[i] == FREE_DEVICE) {
            *fd = i;
            return RETURN_SUCCESS;
        }
    }
    return RETURN_ERROR_INSUFFICIENT_RESOURCES;
}

int init_uart_comm(int *fd, const char *dev, uint16_t options) {

    if (get_next_uart_fd(fd) < 0)
        return RETURN_ERROR_INSUFFICIENT_RESOURCES;

    PRINT(DEBUG, "Found file descriptor %i\n", *fd);
    used_uart_devices[*fd] = USED_DEVICE;
    PRINT(DEBUG, "Opening UART port: %s\n", dev);

    // Allocate space for uart device
    PRINT(DEBUG, "Opening %s as %d\n", dev, *fd);

    uart_devices[*fd] = open(dev, O_RDWR | O_NOCTTY | O_SYNC);
    if (uart_devices[*fd] < 0) {
        PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
        return RETURN_ERROR;
    }
    int mydev = uart_devices[*fd];

    PRINT(DEBUG, "Configuring UART\n");

    set_uart_interface_attribs(mydev, B115200,
                               0); // set speed to 115,200 bps, 8n1 (no parity)
    set_uart_blocking(mydev, 0);   // set no blocking

    return RETURN_SUCCESS;
}

int close_uart_comm(int fd) {
    if (fd < 0)
        return RETURN_ERROR_PARAM;

    PRINT(VERBOSE, "Closing uart network %s()\n", __func__);

    int mydev = uart_devices[fd];

    close(mydev);
    used_uart_devices[fd] = FREE_DEVICE;

    return RETURN_SUCCESS;
}

int recv_uart_comm(int fd, uint8_t *data, uint16_t *size, uint16_t max_size) {
    if (fd < 0)
        return RETURN_ERROR_PARAM;
    PRINT(DEBUG ,"recv %d\n", fd);
    int mydev = uart_devices[fd];
    return recv_uart(mydev, data, size, max_size);
}

int send_uart_comm(int fd, uint8_t *data, uint16_t size) {
    if (fd < 0)
        return RETURN_ERROR_PARAM;
    PRINT(DEBUG ,"send %d\n", fd);
    int mydev = uart_devices[fd];
    return send_uart(mydev, data, size);
}

int flush_uart_comm(int fd) {
    if (fd < 0)
        return RETURN_ERROR_PARAM;
    int mydev = uart_devices[fd];
    return flush_uart(mydev);
}
