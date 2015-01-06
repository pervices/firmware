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

#include "udp.h"
#include "uart.h"
#include "comm_manager.h"

#define MAX_DEVICES 16
#define USED_DEVICE 1
#define FREE_DEVICE 0

/**************************************
 * UDP Driver Manager
 **************************************/
// Array of UDP devices
static udp_dev_t* udp_devices[MAX_DEVICES];
static uint8_t used_udp_devices[MAX_DEVICES] = {FREE_DEVICE};

// Gets the next available file descriptor
static int get_next_udp_fd (int* fd) {
	int i;
	for (i = 0; i < MAX_DEVICES; i++) {
		if (used_udp_devices[i] == FREE_DEVICE) {
			*fd = i;
			return RETURN_SUCCESS;
		}
	}
	return RETURN_ERROR_INSUFFICIENT_RESOURCES;
}

int init_udp_comm(int* fd, const char* dev, uint32_t port, uint16_t options) {
	if (get_next_udp_fd(fd) < 0)
		return RETURN_ERROR_INSUFFICIENT_RESOURCES;

	int ret = RETURN_SUCCESS;

	#ifdef DEBUG
	printf("Found file descriptor %i\n", *fd);
	#endif

	used_udp_devices[*fd] = USED_DEVICE;

	// Allocate space for udp device
	udp_devices[*fd] = malloc (sizeof(*udp_devices[*fd]));
	udp_dev_t* mydev = udp_devices[*fd];

	mydev -> eth = malloc (sizeof(*mydev -> eth));

	strcpy(mydev -> eth -> iface, dev);
	mydev -> eth -> port = port;
	mydev -> opt = options;

	#ifdef DEBUG
	printf("Calling establish_udp_connection()\n");
	#endif

	ret = establish_udp_connection(mydev);
	if (ret < 0) {
		close_udp_comm(*fd);
	}
	return ret;
}

int close_udp_comm(int fd) {
	if (fd < 0 || !udp_devices[fd]) return RETURN_ERROR_PARAM;

	printf("Closing udp network %s()\n", __func__);

	udp_dev_t* mydev = udp_devices[fd];
	severe_udp_connection(mydev);

	free(mydev -> eth);
	free(mydev);
	used_udp_devices[fd] = FREE_DEVICE;

	return RETURN_SUCCESS;
}

int recv_udp_comm(int fd, uint8_t* data, uint16_t* size, uint16_t max_size) {
	if (fd < 0 || !udp_devices[fd]) return RETURN_ERROR_PARAM;

	udp_dev_t* mydev = udp_devices[fd];
	return recv_udp(mydev, data, size, max_size);
}

int send_udp_comm(int fd, uint8_t* data, uint16_t size) {
	if (fd < 0 || !udp_devices[fd]) return RETURN_ERROR_PARAM;

	udp_dev_t* mydev = udp_devices[fd];
	return send_udp(mydev, data, size);
}


/**************************************
 * UART Driver Manager
 **************************************/
// Array of UART devices, file descriptors
static int uart_devices[MAX_DEVICES];
static uint8_t used_uart_devices[MAX_DEVICES] = {FREE_DEVICE};

// Gets the next available file descriptor
static int get_next_uart_fd (int* fd) {
	int i;
	for (i = 0; i < MAX_DEVICES; i++) {
		if (used_uart_devices[i] == FREE_DEVICE) {
			*fd = i;
			return RETURN_SUCCESS;
		}
	}
	return RETURN_ERROR_INSUFFICIENT_RESOURCES;
}

int init_uart_comm(int* fd, const char* dev, uint16_t options) {
	if (get_next_uart_fd(fd) < 0)
		return RETURN_ERROR_INSUFFICIENT_RESOURCES;

	#ifdef DEBUG
	printf("Found file descriptor %i\n", *fd);
	#endif

	used_uart_devices[*fd] = USED_DEVICE;

	#ifdef DEBUG
	printf("Opening UART port: %s\n", dev);
	#endif

	// Allocate space for uart device
	uart_devices[*fd] = open(dev, O_RDWR | O_NOCTTY | O_SYNC);
	if (uart_devices[*fd] < 0)
	{
		fprintf(stderr, "%s(): ERROR, %s\n", __func__, strerror(errno));
		return RETURN_ERROR;
	}
	int mydev = uart_devices[*fd];

	#ifdef DEBUG
	printf("Configuring UART\n");
	#endif

	set_uart_interface_attribs (mydev, B115200, 0);  // set speed to 115,200 bps, 8n1 (no parity)
	set_uart_blocking (mydev, 0);               	 // set no blocking

	return RETURN_SUCCESS;
}

int close_uart_comm(int fd) {
	if (fd < 0) return RETURN_ERROR_PARAM;

	printf("Closing uart network %s()\n", __func__);

	int mydev = uart_devices[fd];

	close(mydev);
	used_uart_devices[fd] = FREE_DEVICE;

	return RETURN_SUCCESS;
}

int recv_uart_comm(int fd, uint8_t* data, uint16_t* size, uint16_t max_size) {
	if (fd < 0) return RETURN_ERROR_PARAM;
	int mydev = uart_devices[fd];
	return recv_uart(mydev, data, size, max_size);
}

int send_uart_comm(int fd, uint8_t* data, uint16_t size) {
	if (fd < 0) return RETURN_ERROR_PARAM;
	int mydev = uart_devices[fd];
	return send_uart(mydev, data, size);
}
