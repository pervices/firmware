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

// XXX: @CF: 20170711: we should really just write new code in C++, update build system, etc

#include <arpa/inet.h>   // htonl(3), htons(3)
#include <errno.h>       // errno
#include <getopt.h>      // getopt(3)
#include <inttypes.h>    // PRIu32, PRIx64..
#include <libgen.h>      // basename(3)
#include <limits.h>      // PATH_MAX
#include <netinet/in.h>  // in_port_t
#include <signal.h>      // signal(2)
#include <stdbool.h>     // true, false..
#include <stdio.h>       // fpritnf(3), snprintf(3)
#include <stdint.h>      // uint32_t..
#include <stdlib.h>      // EXIT_SUCCESS
#include <string.h>      // strerror(3)
#include <syslog.h>      // openlog(3), setlogmask(3)
#include <sys/select.h>  // select(2)
#include <sys/socket.h>  // recvfrom(2), socket(2), sendto(2)
#include <sys/types.h>   // for portability (see socket(2))
#include <unistd.h>      // close(2), pipe(2), write(2)

#include "common/array-utils.h" // ARRAY_SIZE
#include "common/common.h"      // UDP_FLOW_CNTRL_PORT
#define HAVE_SYSLOG
#include "common/ied.h"         // I(), E(), D()
#include "common/minmax.h"      // min(), max()
// XXX: @CF: 20170711: This is ugly. Use structs to access memory-mapped registers. Use init / fini.
#include "hal/drivers/mmap/mmap.h"

/**
 * Program Context
 *
 * r              return value of the program. EXIT_SUCCESS for success or an errno value
 * argc           the number of command-line arguments passed to the program
 * argV           the command-line arguments passed to the program
 * ident          the service name (defaults to argv[ 0 ])
 * should_exit    the service name (defaults to argv[ 0 ])
 *
 * signal_caught  true when a signal has been caught by the program
 * signal_fd      unnamed pipe for interrupting select(2)
 *
 * server_socket  the bound server socket file descriptor
 * server_port    the port the server socket is bound to
 *
 * highest_fd     the highest numbered file descriptor select(2) should listen to
 */
struct flow_context {

	int r;
	int argc;
	char **argv;
	char *ident;
	bool should_exit;

	bool signal_caught;
	int  signal_fd[ 2 ]; // 0 := listener, 1 := interruptor

	int server_socket;
	in_port_t server_port;

	int highest_fd;
};
#define FC_DEFAULT \
	{ \
		.r = EXIT_SUCCESS, \
		.argc = argc, \
		.argv = argv, \
		.ident = NULL, \
		.should_exit = false, \
		.signal_caught = false, \
		.signal_fd = { -1, -1, }, \
		.server_socket = -1, \
		.server_port = UDP_FLOW_CNTRL_PORT, \
		.highest_fd = -1, \
	}

typedef void (*sighandler_t)( int );

/**
 * Compare and Swap
 *
 * @param a  A pointer to the current highest value
 * @param b  A possible successor to the current highest value
 */
static inline void cas( int *a, int b ) {
	if ( b >= *a ) {
		*a = b;
	}
}

/**
 * Signal Handlers
 */
static const int signals_to_catch[] = {
	SIGTERM,
	SIGINT,
};
static struct flow_context *_fc;
static uint8_t flow_buffer[ 1 << 9 ]; // 512 bytes should be plenty
static char flow_progname[ PATH_MAX ];

static void sighandler( int signum ) {

	int r;
	struct flow_context *fc = _fc;

	D( "received signal %d", signum );

	if ( fc->signal_caught ) {

		D( "signal already caught" );

	} else {

		D( "exiting.." );

		fc->signal_caught = true;
		fc->should_exit = true;
		fc->r = EXIT_SUCCESS;

		r = write( _fc->signal_fd[ 1 ], "aieeeee!@!", strlen( "aieeeee!@!" ) );
		if ( -1 == r ) {
			E( "write(2) failed to signal via pipe" );
		}
	}
}

/**
 * Initialize flow-control context and process command-line arguments
 *
 * Sets fc->r to zero upon success, a non-zero errno code, otherwise.
 *
 * @param fc    The flow control context
 * @param argc  The number of command-line arguments
 * @param argv  The command-line arguments
 * @return      Upon success, zero. A non-zero errno code, otherwise.
 */
static void flow_init( struct flow_context *fc ) {

	int r;
	int i;

	struct sockaddr_in sa;
	socklen_t sa_len;
	sighandler_t old_sh;

	// determine the service name
	if ( NULL == fc->ident ) {
		memcpy(
			flow_progname,
			fc->argv[ 0 ],
			MIN(
				strlen( fc->argv[ 0 ] ),
				sizeof( flow_progname - 1 )
			)
		);
		fc->ident = basename( flow_progname );
	}

	// use getopt(3), create a usage(), etc
	// options could include verbosity

#ifdef HAVE_SYSLOG
	// enable logging
	openlog( fc->ident, 0, LOG_DAEMON );
	setlogmask( LOG_UPTO( LOG_INFO ) );
	// could be conditional, e.g. if command-line argument exists to enable / disable debug
	setlogmask( setlogmask( 0 ) | LOG_MASK( LOG_DEBUG ) );
#endif

	// set the global variable for signal handlers
	_fc = fc;

	I( "installing signal handlers.." );

	for( i = 0; i < ARRAY_SIZE( signals_to_catch ); i++ ) {
		old_sh = signal( signals_to_catch[ i ], sighandler );
		if ( SIG_ERR == old_sh ) {
			r = errno;
			E( "signal(2) failed" );
			goto out;
		}
	}

	I( "opening signal pipe.." );
	r = pipe( fc->signal_fd );
	if ( EXIT_SUCCESS != r ) {
		r = errno;
		E( "pipe(2) failed" );
		goto out;
	}
	D( "opened signal pipe as fd's %d, %d", fc->signal_fd[ 0 ], fc->signal_fd[ 1 ] );
	cas( & fc->highest_fd, fc->signal_fd[ 0 ] );

	I( "opening server socket.." );
	r = socket( AF_INET, SOCK_DGRAM, 0 );
	if ( -1 == r ) {
		r = errno;
		E( "socket(2) failed" );
		goto out;
	}
	fc->server_socket = r;
	D( "opened server socket as fd %d", fc->server_socket );

	I( "binding server socket.." );

	sa.sin_family = AF_INET;
	sa.sin_port = htons( fc->server_port );
	sa.sin_addr.s_addr = htonl( INADDR_ANY );
	sa_len = sizeof( sa );

	r = bind( fc->server_socket, (struct sockaddr *) & sa, sa_len );
	if ( -1 == r ) {
		r = errno;
		E( "bind(2) failed" );
		goto out;
	}
	D( "bound server socket to port %d", fc->server_port );
	cas( & fc->highest_fd, fc->server_socket );

out:
	fc->r = r;
}

/**
 * Finalize the flow-control context, cleaning up and freeing any resources.
 *
 * @param fc    The flow control context
 */
static void flow_fini( struct flow_context *fc ) {

	int r;
	int i;

	I( "cleaning up.." );

	if ( -1 != fc->server_socket ) {

		D( "closing server socket.." );

		r = close( fc->server_socket );
		if ( EXIT_SUCCESS != r ) {
			E( "close(2) failed for server socket" );
		}
		fc->server_socket = -1;
	}

	for( i = ARRAY_SIZE( fc->signal_fd ) - 1; i >= 0; i-- ) {

		if ( -1 != fc->signal_fd[ i ] ) {

			D( "closing signal pipe %d..", i );

			r = close( fc->signal_fd[ i ] );
			if ( EXIT_SUCCESS != r ) {
				E( "close(2) failed for signal_fd %d", i );
			}
			fc->signal_fd[ i ] = -1;
		}
	}

	D( "uninstalling signal handlers.." );
	// do nothing..
}

/**
 * Process flow control information request
 *
 * Sets fc->r to zero upon success, a non-zero errno code, otherwise.
 *
 * @param fc    The flow control context
 */
static void flow_request_process( struct flow_context *fc ) {

	struct xg_cmd_flc_time_diff {
		uint64_t cmd;     // FLOW_CONTROL_TIME_DIFF := 1
		int64_t tv_sec;   // see <time.h>
		int64_t tv_tick;  // same order of magnitude as tv_nsec
	}__attribute__(( packed ));

	ssize_t r;

	struct sockaddr_storage sa;
	socklen_t sa_len = sizeof( sa );

	struct xg_cmd_flc_time_diff flc_time_diff;
	uint32_t fifo_lvl[4];

	r = recvfrom( fc->server_socket, flow_buffer, sizeof( flow_buffer ), 0, (struct sockaddr *) & sa, & sa_len );
	if ( -1 == r ) {
		r = errno;
		E( "recvfrom(2) failed" );
		goto out;
	}

	D( "received %d bytes from %s", r, inet_ntoa( ( (struct sockaddr_in *) & sa )->sin_addr ) );

	// XXX: @CF: 20170711: This is ugly. Use struct pointers to access memory-mapped registers.

	// read flow control time diff
	read_hps_reg( "flc1", &( (uint32_t *) & flc_time_diff.tv_sec )[ 0 ] );
	read_hps_reg( "flc2", &( (uint32_t *) & flc_time_diff.tv_sec )[ 1 ] );
	read_hps_reg( "flc3", &( (uint32_t *) & flc_time_diff.tv_tick )[ 0 ] );
	read_hps_reg( "flc4", &( (uint32_t *) & flc_time_diff.tv_tick )[ 1 ] );

	// read fifo levels
	read_hps_reg("res_ro0", & fifo_lvl[ 0 ] );
	read_hps_reg("res_ro1", & fifo_lvl[ 1 ] );
	read_hps_reg("res_ro2", & fifo_lvl[ 2 ] );
	read_hps_reg("res_ro3", & fifo_lvl[ 3 ] );
	fifo_lvl[0] = (fifo_lvl[0] & 0xffff) >> 0;
	fifo_lvl[1] = (fifo_lvl[1] & 0xffff) >> 0;
	fifo_lvl[2] = (fifo_lvl[2] & 0xffff) >> 0;
	fifo_lvl[3] = (fifo_lvl[3] & 0xffff) >> 0;

	r = snprintf(
		(char *)flow_buffer, sizeof( flow_buffer ),
		"flow,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32",%" PRIx64",%" PRIx64 "\n",
		fifo_lvl[0], fifo_lvl[1], fifo_lvl[2], fifo_lvl[3],
		flc_time_diff.tv_sec, flc_time_diff.tv_tick
	);
	if ( r >= sizeof( flow_buffer ) ) {
		errno = ENOMEM;
		r = errno;
		E( "sprintf(3) requires a buffer of %d bytes, but only %d are available", r, sizeof( flow_buffer ) );
		goto out;
	}

	r = sendto( fc->server_socket, flow_buffer, strlen( (char *)flow_buffer ), 0, (struct sockaddr *) & sa, sa_len );
	if ( -1 == r ) {
		r = errno;
		E( "sendto(2) failed" );
		goto out;
	}

	D( "sent %d bytes to %s", r, inet_ntoa( ( (struct sockaddr_in *) & sa )->sin_addr ) );

	r = EXIT_SUCCESS;

out:
	fc->r = r;
}

/**
 * Main loop for flow control service;
 *
 * Sets fc->r to zero upon success, a non-zero errno code, otherwise.
 *
 * @param fc    The flow control context
 */
static void flow( struct flow_context *fc ) {

	int r;

	fd_set rfds;

	for( ; ! fc->should_exit ; ) {

		FD_ZERO( & rfds );
		FD_SET( fc->signal_fd[ 0 ], & rfds );
		FD_SET( fc->server_socket, & rfds );

		r = select( fc->highest_fd + 1, & rfds, NULL, NULL, NULL );
		switch( r ) {

		case -1:
		case 0:

			if ( -1 == r ) {

				if ( EINTR == errno ) {
					D( "select(2) interrupted" );
				} else {
					fc->r = errno;
					E( "select(2) failed" );
				}
				fc->should_exit = true;

			} else {

				D( "select(2) timed-out even though no timeout was specified" );
			}

			break;

		default:

			if ( FD_ISSET( fc->server_socket, & rfds ) ) {
				flow_request_process( fc );
				if ( EXIT_SUCCESS != fc->r ) {
					fc->should_exit = true;
				}
				r--;
			}

			if ( FD_ISSET( fc->signal_fd[ 0 ], & rfds ) ) {
				D( "received signal" );
				r--;
			}

			if ( 0 != r ) {
				D( "sanity-check: one or more file descriptors was not serviced" );
			}

			break;

		}

	}
}

int main( int argc, char *argv[] ) {
	int i;

	struct flow_context fc = FC_DEFAULT;

	void (*fp[])( struct flow_context *) = {
		flow_init,
		flow,
		flow_fini,
	};

	for( i = 0, fc.r = EXIT_SUCCESS; i < ARRAY_SIZE( fp ); i++ ) {
		if (
			false
			|| EXIT_SUCCESS == fc.r        // only run stage if previous stages were successful
			|| ARRAY_SIZE( fp ) - 1 == i ) // always run flow_fini() at the end
		{
			fp[ i ]( & fc );
		}
	}

	return fc.r;
}
