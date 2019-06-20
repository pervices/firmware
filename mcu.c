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

#define ARG_MCU_SILENT "-s"
#define ARG_MCU_CONSOLE "-c"
#define ARG_MCU_FWD "-f"
#define ARG_MCU_TIMEOUT "-t"
#define ARG_MCU_UART_TX "-TX"
#define ARG_MCU_UART_SN "-SN"
#define ARG_MCU_UART_RX "-RX"

#define DEFAULT_TIMEOUT 100 // Milliseconds

//#define DEBUG_OUTPUTS

static int uart_comm_fd = 0;
static char buf[MAX_UART_LEN] = {};
static boolean silent = FALSE;
static boolean console = FALSE;
static boolean fwd = FALSE;
static uint32_t timeout = DEFAULT_TIMEOUT;
static char fwd_board = 0;

// Crimson files
static const char *UART_CRIMSON_SN = "/dev/ttycrimson-time";
static const char *UART_CRIMSON_TX = "/dev/ttycrimson-tx";
static const char *UART_CRIMSON_RX = "/dev/ttycrimson-rx";

// Cyan files
static const char *UART_CYAN_SN = "/dev/ttycyan-time";
static const char *UART_CYAN_RFE[16]  = {"/dev/ttycyan-rfe-0",
                                    "/dev/ttycyan-rfe-1",
                                    "/dev/ttycyan-rfe-2",
                                    "/dev/ttycyan-rfe-3",
                                    "/dev/ttycyan-rfe-4",
                                    "/dev/ttycyan-rfe-5",
                                    "/dev/ttycyan-rfe-6",
                                    "/dev/ttycyan-rfe-7",
                                    "/dev/ttycyan-rfe-8",
                                    "/dev/ttycyan-rfe-9",
                                    "/dev/ttycyan-rfe-10",
                                    "/dev/ttycyan-rfe-11",
                                    "/dev/ttycyan-rfe-12",
                                    "/dev/ttycyan-rfe-13",
                                    "/dev/ttycyan-rfe-14",
                                    "/dev/ttycyan-rfe-15"};

static int uart_crimson_synth_fd = 3;
static int uart_crimson_tx_fd = 4;
static int uart_crimson_rx_fd = 5;

static int uart_cyan_synth_fd = 3;
static int uart_cyan_rfe_fd[16] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

static int contains(const char *str, char letter, int size) {
    int cnt = 0;
    for (int i = 0; i < size; i++) {
        if (str[i] == letter)
            cnt++;
    }
    return cnt;
}

static void dump_args(void) {
    int i = 0;
#if defined(VAUNT)
    printf("%s\n", UART_CRIMSON_SN);
    printf("%s\n", UART_CRIMSON_TX);
    printf("%s\n", UART_CRIMSON_RX);
    printf("%d\n", uart_crimson_synth_fd);
    printf("%d\n", uart_crimson_tx_fd);
    printf("%d\n", uart_crimson_rx_fd);
#elif defined(TATE)
    printf("%s\n", UART_CYAN_SN);
    for (i = 0; i < 16; i++) {
        printf("%s\n", UART_CYAN_RFE[i]);
    }
    for (i = 0; i < 16; i++) {
        printf("%d\n", uart_cyan_synth_fd);
    }
    printf("%d\n", uart_cyan_rfe_fd[i]);
#endif
    printf("%d\n", fwd);
    printf("%d\n", uart_comm_fd);
}

static void help(void) {
    printf("Usage: mcu"
           "[%s /dev/tty] "
           "[%s /dev/tty] "
           "[%s /dev/tty] "
           "[%s] "
           "[%s] "
           "[%s [t|r|s]] "
           "[%s milliseconds]\n",
           ARG_MCU_UART_TX, ARG_MCU_UART_RX, ARG_MCU_UART_SN, ARG_MCU_SILENT,
           ARG_MCU_CONSOLE, ARG_MCU_FWD, ARG_MCU_TIMEOUT);
    exit(1);
}

static boolean streql(char *a, char *b) { return strcmp(a, b) == 0; }

static boolean last(const int arg, const int argc) { return arg == argc - 1; }

static void parse_args(int argc, char *argv[]) {
    // parse through the arguments
    for (int i = 1; i < argc; i++) {
        // if argument to silent the output
        if (streql(argv[i], ARG_MCU_SILENT)) {
            silent = TRUE;

            // if argument to open a console transaction
        } else if (streql(argv[i], ARG_MCU_CONSOLE)) {
            console = TRUE;

            // if argument to specify this is a forward command
        } else if (streql(argv[i], ARG_MCU_UART_SN) && !last(i, argc)) {

            UART_CRIMSON_SN = argv[i + 1];
            i++;
        } else if (streql(argv[i], ARG_MCU_UART_TX) && !last(i, argc)) {

            UART_CRIMSON_TX = argv[i + 1];
            i++;
        } else if (streql(argv[i], ARG_MCU_UART_RX) && !last(i, argc)) {

            UART_CRIMSON_RX = argv[i + 1];
            i++;
        } else if (streql(argv[i], ARG_MCU_FWD) && !last(i, argc)) {
            fwd = TRUE;
            i++;
            if (argv[i][0] == 't') {
                uart_comm_fd = uart_crimson_tx_fd;
                fwd_board = '1';
            } else if (argv[i][0] == 'r') {
                uart_comm_fd = uart_crimson_rx_fd;
                fwd_board = '0';
            } else if (argv[i][0] == 's') {
                uart_comm_fd = uart_crimson_synth_fd;
                fwd_board = '2';
            } else if (atoi(argv[i]) < 16 && atoi(argv[i]) >= 0) {
                uart_comm_fd = uart_cyan_rfe_fd[atoi(argv[i])];
            } else {
                help();
            }

            // if argument to reconfigure the timeout
        } else if (streql(argv[i], ARG_MCU_TIMEOUT) && !last(i, argc)) {
            i++;
            sscanf(argv[i], "%" SCNu32 "", &timeout);

        } else {
            // usage menu
            help();
        }
    }
}

int main(int argc, char *argv[]) {

    int i;

    parse_args(argc, argv);
    #ifdef DEBUG_OUTPUTS
        dump_args();
    #endif

#if defined(VAUNT)
    // initialize the comm port
    if (init_uart_comm(&uart_crimson_synth_fd, UART_CRIMSON_SN, 0) < 0) {
        printf("ERROR: %s, cannot initialize uart %s\n", __func__, UART_CRIMSON_SN);
        return RETURN_ERROR_COMM_INIT;
    }
    if (init_uart_comm(&uart_crimson_tx_fd, UART_CRIMSON_TX, 0) < 0) {
        printf("ERROR: %s, cannot initialize uart %s\n", __func__, UART_CRIMSON_TX);
        return RETURN_ERROR_COMM_INIT;
    }
    if (init_uart_comm(&uart_crimson_rx_fd, UART_CRIMSON_RX, 0) < 0) {
        printf("ERROR: %s, cannot initialize uart %s\n", __func__, UART_CRIMSON_RX);
        return RETURN_ERROR_COMM_INIT;
    }
#elif defined(TATE)
    if (init_uart_comm(&uart_cyan_synth_fd, UART_CYAN_SN, 0) < 0) {
        printf("ERROR: %s, cannot initialize uart %s\n", __func__, UART_CYAN_SN);
        return RETURN_ERROR_COMM_INIT;
    }
    for (i = 0; i < 16; i++) {
        if (init_uart_comm(&uart_cyan_rfe_fd[i], UART_CYAN_RFE[i], 0) < 0) {
            printf("ERROR: %s, cannot initialize uart %s\n", __func__, UART_CYAN_RFE[i]);
            return RETURN_ERROR_COMM_INIT;
        }
    }
#endif

    // initiate UART transaction
    do {
        // read in the input from stdin
        if (fwd == TRUE) {
            // strcpy(buf, "fwd -b   -m '");
            // buf[7] = fwd_board;
            fgets(buf, MAX_UART_LEN, stdin);
            strcat(buf, "'\r");
        } else {
            fgets(buf, MAX_UART_LEN, stdin);
            strcat(buf, "\r");
        }

        send_uart_comm(uart_comm_fd, (uint8_t *)buf, strlen(buf));

        // if not silent, read the output
        if (!silent) {
            memset(buf, 0, MAX_UART_LEN);
            uint16_t total_bytes = 0, cur_bytes = 0;

            while (contains(buf, '>', total_bytes) < 1) {
                if (recv_uart_comm(uart_comm_fd, ((uint8_t *)buf) + total_bytes,
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
            //				memset(buf + real_size, 0, MAX_UART_LEN
            //- real_size);
            //			}

            printf("%s\n", buf);
        }
    } while (console);
    return 0;
}
