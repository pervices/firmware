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

#include <sys/select.h>

#include "uart.h"

#include "../../../common/timespec_ops.h"

#define TIMEOUT_TS { .tv_sec = 0, .tv_nsec = 500000, }

// Options passed from server.c
static uint8_t _options = 0;

void set_uart_debug_opt(uint8_t options) {
	_options = options;
}

int set_uart_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
		PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
                return RETURN_ERROR;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 1;            // return when there is at least 1 char
        tty.c_cc[VTIME] = 1;            // return when 0.1s elapses without receiving a char

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
		PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
                return RETURN_ERROR;
        }
        return RETURN_SUCCESS;
}

void set_uart_blocking (int fd, int should_block)
{
	int r;
	int flags;
	int oldflags;

	r = fcntl( fd, F_GETFL, 0 );
	if ( -1 == r ) {
		PRINT(ERROR, "%s(), fcntl( F_GETFL ): %s\n", __func__, strerror(errno));
		return;
	}
	oldflags = r;
	flags = oldflags;

	if ( should_block ) {
		flags |= O_NONBLOCK;
	} else {
		flags &= ~O_NONBLOCK;
	}

	if ( flags ^ oldflags ) {
		r = fcntl( fd, F_SETFL, flags );
		if ( -1 == r ) {
			PRINT(ERROR, "%s(), fcntl( F_SETFL ): %s\n", __func__, strerror(errno));
			return;
		}
	}
}

int recv_uart(int fd, uint8_t* data, uint16_t* size, uint16_t max_size) {

	int r;

	fd_set rfds;

	struct timespec now, then, dt;
	struct timespec timeout = TIMEOUT_TS;
	struct timespec req, rem;

	//PRINT( INFO, "%s() called...\n", __func__ );

	for(
		clock_gettime( CLOCK_MONOTONIC, & now ),
			timesadd( & now, & timeout, & then ),
			*size = 0
			;
		*size < max_size && timescmp( & now, & then, < )
			;
		clock_gettime( CLOCK_MONOTONIC, & now )
	) {

		timessub( & then, & now, & dt );

		FD_ZERO( & rfds );
		FD_SET( fd, & rfds );

		r = pselect( fd + 1, & rfds, NULL, NULL, & dt, NULL );
		switch( r ) {
		case -1:
			PRINT(ERROR, "%s(), pselect: %s\n", __func__, strerror( errno ) );
			return RETURN_ERROR;
			break;

		case 0:
			//PRINT(ERROR, "%s(), pselect(2) timedout\n", __func__);
			break;

		default:

			r = read( fd, data, max_size );
			if ( -1 == r ) {
				PRINT(ERROR, "%s(), read: %s\n", __func__, strerror( errno ) );
			} else {

//				PRINT( INFO, "%s(), read %u bytes\n", __func__, r );

				*size += r;
				data += r;
			}

			break;

		}
	}

	return RETURN_SUCCESS;
}

int send_uart(int fd, uint8_t* data, uint16_t size) {

	int r;

	struct timespec now, then, dt;
	struct timespec timeout = TIMEOUT_TS;

	//PRINT( INFO, "%s() called...\n", __func__ );

	for(
		clock_gettime( CLOCK_MONOTONIC, & now ),
			timesadd( & now, & timeout, & then )
			;
		size > 0 && timescmp( & now, & then, < )
			;
		clock_gettime( CLOCK_MONOTONIC, & now )
	) {

		timessub( & then, & now, & dt );

		r = write( fd, data, size );
		if ( -1 == r ) {
			PRINT(ERROR, "%s(), write: %s\n", __func__, strerror( errno ) );
		} else {

			//PRINT( INFO, "%s(), wrote %u bytes\n", __func__, r );

			size -= r;
			data += r;
		}
	}

	return RETURN_SUCCESS;
}

int flush_uart(int fd) {
	// flushes UART on HPS side
	if( tcflush(fd, TCIOFLUSH) == 0)
		return RETURN_SUCCESS;
	else
		return RETURN_ERROR_UART_FLUSH;
}
