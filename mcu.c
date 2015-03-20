#include <stdio.h>
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

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include "common.h"
#include "comm_manager.h"

#define UART_DEV "/dev/ttyS1"

#define ARG_MCU_SILENT	"-s"
#define ARG_MCU_CONSOLE	"-c"
#define ARG_MCU_FLUSH	"-f"
#define ARG_MCU_TIMEOUT	"-t"

#define DEFAULT_TIMEOUT 100	// 100 milliseconds

static int uart_comm_fd;
static char buf[MAX_PROP_LEN] = {};
static boolean silent = FALSE;
static boolean console = FALSE;
static boolean flush = FALSE;
static uint32_t timeout = DEFAULT_TIMEOUT;
static uint16_t buf_size;

int main(int argc, char *argv[]) {
	// initialize the comm port
	if ( init_uart_comm(&uart_comm_fd, UART_DEV, 0) < 0 ) {
		printf("ERROR: %s, cannot initialize uart %s\n", __func__, UART_DEV);
		return RETURN_ERROR_COMM_INIT;
	}

	int i;

	// parse through the arguments
	for (i = 1; i < argc; i++) {
		// if argument to silent the output
		if (strcmp(argv[i], ARG_MCU_SILENT) == 0) {
			silent = TRUE;

		// if argument to open a console transaction
		} else if (strcmp(argv[i], ARG_MCU_CONSOLE) == 0) {
			console = TRUE;

		// if argument to flush prior to executing the transaction
		} else if (strcmp(argv[i], ARG_MCU_FLUSH) == 0) {
			flush = TRUE;

		// if argument to reconfigure the timeout
		} else if (strcmp(argv[i], ARG_MCU_TIMEOUT) == 0 && (i != argc - 1)) {
			i++;
			sscanf(argv[i], "%"SCNu32"", &timeout);

		// usage menu
		} else {
			printf("Usage: mcu [%s] [%s] [%s] [%s milliseconds]\n",
				ARG_MCU_SILENT, ARG_MCU_CONSOLE,
				ARG_MCU_FLUSH, ARG_MCU_TIMEOUT);
			return 0;
		}
	}

	// if flush, clear out the serial port
	if (flush) flush_uart_comm(uart_comm_fd);

	do {
		// read in the input from stdin
		fgets(buf, MAX_PROP_LEN, stdin);
		strcat(buf, "\r");
		send_uart_comm(uart_comm_fd, (uint8_t*)buf, strlen(buf));

		// if not silent, read the output
		if (!silent) {
			memset(buf, 0, MAX_PROP_LEN);
			recv_uart_comm(uart_comm_fd, (uint8_t*)buf, &buf_size, MAX_PROP_LEN);
			printf("%s\n", buf);
		}
	} while (console);

	return 0;
}
