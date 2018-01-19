#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <linux/limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "array-utils.h"
#include "common.h"
#include "comm_manager.h"
#include "synth_lut.h"

#include "pllcalc.h"

extern int get_uart_rx_fd();
extern int get_uart_tx_fd();

// this should really be a conditional defined in configure.ac based on the hardware revision we're targetting, but ATM this is all I care about
#ifndef hw_rev_defined_
#define hw_rev_defined_

#define CRIMSON_TNG

#endif

#define SYNTH_LUT_PATH( trx, ch ) \
	"/var/crimson/calibration-data/" #trx "/" #ch ".bin"

enum {
	RX,
	TX
};

#define DEF_RTX_CTX( rtx, rtxbool, ch ) \
	{ .tx = rtxbool, .id = #ch, .fn = SYNTH_LUT_PATH( rtx, ch ), .fd = -1, .fm = MAP_FAILED, .fs = 0, }
#define DEF_RX_CTX( ch ) DEF_RTX_CTX( rx, RX, ch )
#define DEF_TX_CTX( ch ) DEF_RTX_CTX( tx, TX, ch )


/**
 * tx - true if the lut is for a TX channel
 * id - a string describing the channel (e.g. "A")
 * fn - file name, including full path, to calibration data for the channel
 * fd - file descriptor for the file named in fn
 * fm - file map via mmap(2) for the file named in fn
 * fs - file size of file named in fn
 */
struct synth_lut_ctx {
	const bool tx;
	const char *id;
	const char fn[ PATH_MAX ];
	int fd;
	synth_rec_t *fm;
	size_t fs;
};

// Crimson TNG specific defines
#define FREQ_TOP    PLL1_RFOUT_MAX_HZ
#define FREQ_BOTTOM PLL1_RFOUT_MIN_HZ

#define LO_STEP_SIZE PLL_CORE_REF_FREQ_HZ
static struct synth_lut_ctx rx_ctx[] = {
	DEF_RX_CTX( A ),
	DEF_RX_CTX( B ),
	DEF_RX_CTX( C ),
	DEF_RX_CTX( D ),
};
static struct synth_lut_ctx tx_ctx[] = {
	DEF_TX_CTX( A ),
	DEF_TX_CTX( B ),
	DEF_TX_CTX( C ),
	DEF_TX_CTX( D ),
};

#define SYNTH_LUT_LEN ( ( FREQ_TOP - FREQ_BOTTOM ) / LO_STEP_SIZE )

/**
 * Returns the channel number (e.g. 0 for 'A')
 * @return the channel number
 */
static size_t synth_lut_get_channel( const struct synth_lut_ctx * const ctx ) {
	return ctx->id[ 0 ] - 'A';
}

static int synth_lut_uart_cmd( const int fd, char *query, char *resp, const size_t resp_max_sz ) {
	int r;

	uint16_t resp_sz = 0;

	memset( resp, '\0', resp_max_sz );

	PRINT( INFO, "writing '%s' to fd %d\n", query, fd );

	query[ strlen( query ) ] = '\r';

	r = write( fd, query, strlen( query ) );
	if ( strlen( query ) != r ) {
		PRINT( ERROR, "failed to send query '%s' on uart_fd %d\n", query, fd, errno, strerror( errno ) );
		r = errno;
		goto out;
	}

	int tries = 10;
	for( resp_sz = 0, tries = 10; NULL == strstr( resp, ">" ) && tries; tries-- ) {
		// read_uart_comm is super annoying and does not seem to work for this, only returning partial strings
		int rr = read( fd, & resp[ resp_sz ], resp_max_sz - resp_sz );

		//PRINT( INFO, "read() returned %d: resp: '%s'\n", rr, resp );

		switch( rr ) {

		case 0:
			// end of file
			break;

		case -1:
			// error
			if ( ETIMEDOUT == errno || EAGAIN == errno ) {
				break;
			}
			PRINT( ERROR, "failed to receive response to command '%s' on uart_fd %d\n", query, fd );
			r = errno;
			goto out;

		default:

			resp_sz += rr;
			break;
		}
		usleep( 10000 );
	}

	r = EXIT_SUCCESS;

out:
	return r;
}

extern int set_pll_frequency2( int actual_uart_fd, uint64_t reference, pllparam_t* pll );

static int synth_lut_get_calibration_for_freq( const bool tx, const size_t chan_i, const size_t freq, synth_rec_t *rec ) {
	int r;

	char cmd_buf[ PATH_MAX ];
	char resp_buf[ PATH_MAX ];

	int core, band, bias;
	int uart_fd;

	uart_fd = tx ? get_uart_tx_fd() : get_uart_rx_fd();

	// tell the mcu what channel to select
	snprintf( cmd_buf, sizeof( cmd_buf ), "rf -c %c", 'a' + chan_i );
	r = synth_lut_uart_cmd( uart_fd, cmd_buf, resp_buf, sizeof( resp_buf ) );
	if ( EXIT_SUCCESS != r ) {
		PRINT( ERROR, "synth_lut_uart_cmd() failed (%d,%s)\n", r, strerror( r ) );
		goto out;
	}

	// tell the mcu to use autocal
	snprintf( cmd_buf, sizeof( cmd_buf ), "rf -A 1" );
	r = synth_lut_uart_cmd( uart_fd, cmd_buf, resp_buf, sizeof( resp_buf ) );
	if ( EXIT_SUCCESS != r ) {
		PRINT( ERROR, "synth_lut_uart_cmd() failed (%d,%s)\n", r, strerror( r ) );
		goto out;
	}

	// run the pll calc algorithm
	pllparam_t pll;
	uint64_t in_freq = freq;
	double out_freq = setFreq( &in_freq, &pll );

	PRINT( INFO, "in_freq: %u, out_freq: %u\n", freq, out_freq );

	// Send Parameters over to the MCU
	r = set_pll_frequency2( uart_fd, LO_STEP_SIZE, & pll );
	if ( EXIT_SUCCESS != r ) {
		PRINT( ERROR, "set_pll_frequency2() failed (%d,%s)\n", r, strerror( r ) );
		goto out;
	}

	// read the value back
	snprintf( cmd_buf, sizeof(cmd_buf), "rf -p" );
	r = synth_lut_uart_cmd( uart_fd, cmd_buf, resp_buf, sizeof( resp_buf ) );
	if ( EXIT_SUCCESS != r ) {
		goto out;
	}

	if ( 3 != sscanf( resp_buf, "%u,%u,%u", & core, & band, & bias ) ) {
		PRINT( ERROR, "unable to parse response '%s', expecting number,number,number\n", resp_buf );
		r = EINVAL;
		goto out;
	}

	rec->core = core;
	rec->band = band;
	rec->bias = bias;

	snprintf( cmd_buf, sizeof(cmd_buf), "rf -A 0" );
	r = synth_lut_uart_cmd( uart_fd, cmd_buf, resp_buf, sizeof( resp_buf ) );
	if ( EXIT_SUCCESS != r ) {
		goto out;
	}

	r = EXIT_SUCCESS;

out:
	return r;
}

static int synth_lut_calibrate_one_board( struct synth_lut_ctx *ctx ) {

	int r;

	size_t i;
	size_t freq;
	size_t chan_i;

	synth_rec_t *rec;

	PRINT( INFO, "Performing initial calibration of %s %s..\n", ctx->tx ? "TX" : "RX", ctx->id, ctx->fn );

	for( i = 0; i < SYNTH_LUT_LEN; i++ ) {

		freq = FREQ_BOTTOM + i * LO_STEP_SIZE;
		chan_i = synth_lut_get_channel( ctx );
		rec = & ctx->fm[ i ];

		r = synth_lut_get_calibration_for_freq( ctx->tx, chan_i, freq, rec );
		if ( EXIT_SUCCESS != r ) {
			PRINT( ERROR, "%s %s @ %u MHz failed (%d,%s)\n",
				ctx->tx ? "TX" : "RX",
				ctx->id,
				freq / 1000000,
				r,
				strerror( r )
			);
			return r;
		}

		PRINT( INFO, "%s %s @ %u MHz: { %u, %u, %u }\n",
			ctx->tx ? "TX" : "RX",
			ctx->id,
			freq / 1000000,
			rec->core,
			rec->band,
			rec->bias
		);
	}

	return EXIT_SUCCESS;
}

static int synth_lut_init_one_board( struct synth_lut_ctx *ctx ) {

	static const size_t n = SYNTH_LUT_LEN;

	int r;

	struct stat st;
	synth_rec_t *rec = NULL;
	char *cmdbuf = NULL;
	size_t cmdbuf_sz;

	r = stat( ctx->fn, & st );
	if ( EXIT_SUCCESS != r ) {
		if ( ENOENT != errno ) {
			r = errno;
			PRINT( ERROR, "Failed to stat %s (%d,%s)\n", ctx->fn, errno, strerror( errno ) );
			goto out;
		}

		// In this case, no calibration data exists on file, so we have to create a file of calibration data
		// We assume that after going through autocalibration once for each frequency, that the micro stores a copy of the calibrated values

		PRINT( INFO, "Creating calibration data for %s %s\n", ctx->tx ? "TX" : "RX", ctx->id );

		rec = calloc( n, sizeof( *rec ) );
		if ( NULL == rec ) {
			r = errno; // assumes malloc sets errno on failure (some libc's do not)
			PRINT( ERROR, "Failed to allocate %u bytes for synth_rec storage (%d,%s)\n", n * sizeof( *rec ), errno, strerror( errno ) );
			goto out;
		}

		// we temporarily put our buffer inside the ctx
		ctx->fm = rec;

		r = synth_lut_calibrate_one_board( ctx );

		// we temporarily put our buffer inside the ctx
		ctx->fm = MAP_FAILED;

		if ( EXIT_SUCCESS != r ) {
			PRINT( ERROR, "Failed to generate calibration data (%d,%s)\n", r, strerror( r ) );
			goto out;
		}

		cmdbuf_sz = strlen( "'mkdir -p $(dirname " ) + strlen( ctx->fn ) + strlen( ")'" ) + sizeof( '\0' );
		cmdbuf = malloc( cmdbuf_sz );
		if ( NULL == cmdbuf ) {
			PRINT( ERROR, "Failed to allocate memory for mkdir -p <> (%d,%s)\n", errno, strerror( errno ) );
			goto out;
		}
		snprintf( cmdbuf, cmdbuf_sz, "mkdir -p $(dirname %s)", ctx->fn );
		PRINT( INFO, "Running cmd '%s'\n", cmdbuf );
		r = system( cmdbuf );
		if ( EXIT_SUCCESS != r ) {
			errno = EIO;
			PRINT( ERROR, "Failed to run command %s\n", cmdbuf, errno, strerror( errno ) );
			goto out;
		}

		// when stat sets errno to ENOENT, it means the file does not exist
		// we must create the file, write calibration data to it, close, and then reopen the file, read-only

		r = open( ctx->fn, O_RDWR | O_SYNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP |  S_IWGRP );
		if ( -1 == r ) {
			r = errno;
			PRINT( ERROR, "Failed to create / open calibration data file %s (%d,%s)\n", ctx->fn, errno, strerror( errno ) );
			goto out;
		}
		ctx->fd = r;

		r = write( ctx->fd, rec, n * sizeof( *rec ) );
		if ( -1 == r ) {
			r = errno;
			PRINT( ERROR, "Failed to write calibration data (%d,%s)\n", errno, strerror( errno ) );
			goto remove_file;
		}

		if ( n * sizeof( *rec ) != r ) {
			r = EINVAL;
			PRINT( ERROR, "Wrote wrong number of bytes to calibration data file. Expected: %u, Actual: %u\n", n * sizeof( *rec ), r );
			goto remove_file;
		}

		r = close( ctx->fd );
		if ( -1 == r ) {
			PRINT( ERROR, "Warning: Failed to close calibration data file (%d,%s)\n", errno, strerror( errno ) );
		}
		ctx->fd = -1;

		PRINT( INFO, "Created calibration data for %s %s\n", ctx->tx ? "TX" : "RX", ctx->id );
	}

	// we have an existing calibration table on file
	// we need to communicate that calibration data to the micro

	r = open( ctx->fn, O_RDONLY, 0);
	if ( -1 == r ) {
		r = errno;
		PRINT( ERROR, "Failed to open calibration data file %s (%d,%s)\n", ctx->fn, errno, strerror( errno ) );
		goto out;
	}
	ctx->fd = r;

	PRINT( INFO, "Mapping %u records of calibration data for %s %s\n", n, ctx->tx ? "TX" : "RX", ctx->id );

	ctx->fm = mmap( NULL, n * sizeof( *rec ), PROT_READ, MAP_SHARED, ctx->fd, 0 );
	if ( MAP_FAILED == ctx->fm ) {
		r = errno;
		PRINT( ERROR, "Failed to map calibration file %s (%d,%s)\n", ctx->fn, errno, strerror( errno ) );
		goto remove_file;
	}
	ctx->fs = n * sizeof( *rec );

	PRINT( INFO, "Mapped %u records of calibration data for %s %s at %p\n", n, ctx->tx ? "TX" : "RX", ctx->id, ctx->fm );

	r = EXIT_SUCCESS;

	goto out;

remove_file:
	// XXX: fail-safe to ensure we do not have a corrupt calibration file
	close( ctx->fd );
	ctx->fd = -1;
	remove( ctx->fn );
	sync();

out:
	if ( NULL != cmdbuf ) {
		free( cmdbuf );
		cmdbuf = NULL;
	}
	if ( NULL != rec ) {
		free( rec );
		rec = NULL;
	}
	return r;
}

static void synth_lut_fini_one_board( struct synth_lut_ctx *ctx ) {
	int r;
	if (
		true
		&& MAP_FAILED != ctx->fm
		&& -1 != ctx->fd
	) {
		PRINT( INFO, "Unmapping %s\n", ctx->fn );
		r = munmap( ctx->fm, ctx->fs );
		if ( EXIT_SUCCESS != r ) {
			PRINT( ERROR, "Warning: Failed to unmap %s (%d,%s)\n", ctx->fn, r, strerror( errno ) );
		}
		ctx->fm = MAP_FAILED;
		ctx->fs = 0;
	}
	if ( -1 != ctx->fd ) {
		PRINT( INFO, "Closing %s\n", ctx->fn );
		r = close( ctx->fd );
		if ( EXIT_SUCCESS != r ) {
			PRINT( ERROR, "Warning: Failed to close %s (%d,%s)\n", ctx->fn, r, strerror( errno ) );
		}
		ctx->fd = -1;
	}
}

int synth_lut_init() {

	int r;

	struct synth_lut_ctx *it;

	FOR_EACH( it, rx_ctx ) {
		r = synth_lut_init_one_board( it );
		if ( EXIT_SUCCESS != r ) {
			PRINT( ERROR, "failed to initialize RX %c (%d,%s)\n", it->id, r, strerror( r ) );
			goto out;
		}
	}

	FOR_EACH( it, tx_ctx ) {
		r = synth_lut_init_one_board( it );
		if ( EXIT_SUCCESS != r ) {
			PRINT( ERROR, "failed to initialize TX %c (%d,%s)\n", it->id, r, strerror( r ) );
			goto out;
		}
	}

out:
	return r;
}

void synth_lut_fini() {
	struct synth_lut_ctx *it;

	FOR_EACH( it, rx_ctx ) {
		synth_lut_fini_one_board( it );
	}

	FOR_EACH( it, rx_ctx ) {
		synth_lut_fini_one_board( it );
	}
}

int synth_lut_get_rec( const bool tx, const uint8_t channel, const double freq, synth_rec_t *rec ) {
	int r;

	struct synth_lut_ctx *it;
	double integral;
	double fractional;
	size_t k;

	if ( tx ) {
		FOR_EACH( it, tx_ctx ) {
			if ( channel == ARRAY_OFFSET( it, tx_ctx ) ) {
				break;
			}
		}
	} else {
		FOR_EACH( it, rx_ctx ) {
			if ( channel == ARRAY_OFFSET( it, rx_ctx ) ) {
				break;
			}
		}
	}
	if ( NULL == it ) {
		PRINT( ERROR, "unable to find calibration data for %s %c\n", tx ? "TX" : "RX", 'A' + channel );
		r = ENOENT;
		goto out;
	}

	if (
		false
		|| (double)LO_STEP_SIZE > freq
		|| 0 != ( fractional = modf( freq / (double)LO_STEP_SIZE, & integral ) )
		|| integral >= it->fs / sizeof( *rec )
	) {
		PRINT( ERROR, "unable to find calibration data for %s %c at frequency %f\n", tx ? "TX" : "RX", 'A' + channel, freq );
		r = EINVAL;
		goto out;
	}

	k = integral;

	*rec = it->fm[ k ];

	r = EXIT_SUCCESS;

out:
	return r;
}
