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

#define UART_SYNTH  "/dev/ttycrimson-time"
#define UART_TX	    "/dev/ttycrimson-tx"
#define UART_RX	    "/dev/ttycrimson-rx"

#define ARG_MCU_SILENT	"-s"
#define ARG_MCU_CONSOLE	"-c"
#define ARG_MCU_FWD	"-f"
#define ARG_MCU_TIMEOUT	"-t"

#define DEFAULT_TIMEOUT 100	// 100 milliseconds

static int uart_comm_fd;
static char buf[MAX_UART_LEN] = {};
static boolean silent = FALSE;
static boolean console = FALSE;
static boolean fwd = FALSE;
static uint32_t timeout = DEFAULT_TIMEOUT;
static char fwd_board = 0;

static int contains (const char* str, char letter, int size) {
	int i = 0, cnt = 0;
	for (i = 0; i < size; i++) {
		if (str[i] == letter) cnt++;
	}
	return cnt;
}

int main(int argc, char *argv[]) {
    int uart_synth_fd;
    int uart_tx_fd;
    int uart_rx_fd;

	// initialize the comm port
	if ( init_uart_comm(&uart_synth_fd, UART_SYNTH, 0) < 0 ) {
		printf("ERROR: %s, cannot initialize uart %s\n", __func__, UART_SYNTH);
		return RETURN_ERROR_COMM_INIT;
	}
	if ( init_uart_comm(&uart_tx_fd, UART_TX, 0) < 0 ) {
		printf("ERROR: %s, cannot initialize uart %s\n", __func__, UART_TX);
		return RETURN_ERROR_COMM_INIT;
	}
	if ( init_uart_comm(&uart_rx_fd, UART_RX, 0) < 0 ) {
		printf("ERROR: %s, cannot initialize uart %s\n", __func__, UART_RX);
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

		// if argument to specify this is a forward command
		} else if (strcmp(argv[i], ARG_MCU_FWD) == 0  && (i != argc - 1)) {
			fwd = TRUE;
         i++;
         if (argv[i][0] == 't') {
        	 uart_comm_fd = uart_tx_fd;
            fwd_board = '1';
         } else if (argv[i][0] == 'r') {
        	 uart_comm_fd = uart_rx_fd;
            fwd_board = '0';
         } else if (argv[i][0] == 's') {
        	 uart_comm_fd = uart_synth_fd;
            fwd_board = '2';
         } else {
			   printf("Usage: mcu [%s] [%s] [%s [t|r|s]] [%s milliseconds]\n",
				   ARG_MCU_SILENT, ARG_MCU_CONSOLE,
				   ARG_MCU_FWD, ARG_MCU_TIMEOUT);
            return 0;
         }

		// if argument to reconfigure the timeout
		} else if (strcmp(argv[i], ARG_MCU_TIMEOUT) == 0 && (i != argc - 1)) {
			i++;
			sscanf(argv[i], "%"SCNu32"", &timeout);

		// usage menu
		} else {
			printf("Usage: mcu [%s] [%s] [%s [t|r|s]] [%s milliseconds]\n",
				ARG_MCU_SILENT, ARG_MCU_CONSOLE,
				ARG_MCU_FWD, ARG_MCU_TIMEOUT);
			return 0;
		}
	}

	// initiate UART transaction
	do {
      // read in the input from stdin      
      if (fwd == TRUE) {
         //strcpy(buf, "fwd -b   -m '");
         //buf[7] = fwd_board;
		   fgets(buf, MAX_UART_LEN, stdin);
         strcat(buf, "'\r");
      } else {
		   fgets(buf, MAX_UART_LEN, stdin);
   		strcat(buf, "\r");
      }

		send_uart_comm(uart_comm_fd, (uint8_t*)buf, strlen(buf));

		// if not silent, read the output
		if (!silent) {
			memset(buf, 0, MAX_UART_LEN);
			uint16_t total_bytes = 0, cur_bytes = 0;

			while ( contains(buf, '>', total_bytes) < 1 ) {
			   if (recv_uart_comm(uart_comm_fd, ((uint8_t*)buf) + total_bytes,
					   &cur_bytes, MAX_UART_LEN - total_bytes)) {
				return 0;
			   }
			   total_bytes += cur_bytes;
			}

			// if fwd, remove everything prior to the second message
//			if (fwd == TRUE) {
//				uint16_t pos = 0, real_size = 0;
//				while (buf[pos] != '>') pos++;
//				pos++;
//				real_size = total_bytes - pos;
//				memcpy(buf, buf + pos, real_size);
//				memset(buf + real_size, 0, MAX_UART_LEN - real_size);
//			}

			printf("%s\n", buf);
		}
	} while (console);

	return 0;
}
