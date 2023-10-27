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

#include "uart.h"

// Maximum before a UART command is considered a fail
// Longest command: board -r
#define TIMEOUT 15000000UL // us, 15 seconds

// Minimum time between UART commands
#define TIME_INTERVAL 50000 // us, 0.05 seconds

static struct timeval tprev;  // time since previous UART command
static struct timeval tstart; // time since the beginning of a UART send attempt
static struct timeval tend;

// Options passed from server.c
static uint8_t _options = 0;

void set_uart_debug_opt(uint8_t options) { _options = options; }

// return 1 if timeout, 0 if not
static uint8_t timeout(struct timeval *t, long long int time) {
    gettimeofday(&tend, NULL);

    // overflow issue when computing all within the same statement
    long long int cur = (t->tv_usec + 1000000UL * t->tv_sec);
    long long int pre = (tend.tv_usec + 1000000UL * tend.tv_sec);
    long long int elapsed = pre - cur;

    if (elapsed > time || elapsed < 0) {
        return 1;
    } else {
        return 0;
    }
}

int set_uart_interface_attribs(int fd, int speed, int parity) {
    gettimeofday(&tprev, NULL); // on config, reset the prev timer

    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
        return RETURN_ERROR;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK; // disable break processing
    tty.c_lflag = 0;        // no signaling chars, no echo,
                            // no canonical processing
    tty.c_oflag = 0;        // no remapping, no delays
    tty.c_cc[VMIN] = 0;     // read doesn't block
    tty.c_cc[VTIME] = 5;    // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls,
                                       // enable reading
    tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
        return RETURN_ERROR;
    }
    return RETURN_SUCCESS;
}

void set_uart_blocking(int fd, int should_block) {
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
        return;
    }

    tty.c_cc[VMIN] = should_block ? 1 : 0;
    tty.c_cc[VTIME] = 5; // 0.5 seconds read timeout

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
        PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));

    return;
}

int recv_uart(int fd, uint8_t *data, uint32_t *size, int32_t max_size) {

    if(fd == 0 || fd == 1 || fd == 2)
    {
        puts("cannot write to standard devices");
        exit(1);
    }

    if(max_size < 0) {
        *size = 0;
        PRINT(ERROR, "must request a positive number of bytes to read\n");
        return RETURN_ERROR_PARAM;
    }

    if(fd == -1)
        return RETURN_ERROR_UART_INVALID_FD;

    gettimeofday(&tstart, NULL);

    int rd_len = 0;
    while (!rd_len && !timeout(&tstart, TIMEOUT)) {
        int read_return = read(fd, data+rd_len, (max_size - 1));
        if(read_return < 0) {
            data[read_return] = 0;
            PRINT(ERROR, "UART read failed with error code %s\n", strerror(errno));
            return RETURN_ERROR_UART_IO;
        }
        rd_len += read_return;
    }

    // Adds null terminator to the value read + sets rd_len to 0 in case of error
    if (rd_len < 0) {
        rd_len = 0;
        *data = 0;
    } else if(rd_len == 0) {
        *data = 0;
    } else {
        //remember to have read use max_size - 1 to leave space for the null terminator
        *(data+rd_len) = 0;
    }

    if (timeout(&tstart, TIMEOUT)) {
        PRINT(ERROR, "%s(), timedout\n", __func__);
        *size = rd_len;
        return RETURN_ERROR_UART_TIMEOUT;
    }

    *size = rd_len;

    return RETURN_SUCCESS;
}

int send_uart(int fd, uint8_t *data, uint16_t size) {

    if(fd == 0 || fd == 1 || fd == 2)
    {
        puts("cannot write to standard devices");
        exit(1);
    }

    //
    // NOTE:
    // All UART FDS are set to -1 at startup.
    // Writing with a -1 FD will do nothing.
    // This was done to preserve legacy Crimson UART READ / WRITES while porting to TATE.
    //

    if(fd == -1)
        return RETURN_SUCCESS;

    flush_uart(fd);

    gettimeofday(&tstart, NULL);

    int wr_len = 0;
    while (wr_len != size && !timeout(&tstart, TIMEOUT)) {
        int write_return = write(fd, data + wr_len, size - wr_len);
        if(write_return < 0) {
            PRINT(ERROR, "UART write failed with error code %s\n", strerror(errno));
            return RETURN_ERROR_UART_IO;
        }
        wr_len+=write_return;
    }

    gettimeofday(&tprev, NULL);

    if (timeout(&tstart, TIMEOUT)) {
        PRINT(ERROR, "timedout\n");
        return RETURN_ERROR_UART_TIMEOUT;
    }

    return RETURN_SUCCESS;
}

// Flushes UART on HPS side
int flush_uart(int fd) {
    if(fd == -1)
        return RETURN_SUCCESS;
    if (tcflush(fd, TCIOFLUSH) == 0)
        return RETURN_SUCCESS;
    else
        PRINT(ERROR, "UART flush error: %s\n", strerror(errno));
        return RETURN_ERROR_UART_FLUSH;
}
