#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <linux/limits.h>
#include <math.h>
#include <pthread.h>
#include <regex.h>
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

// I couldn't actually find this hard-coded anywhere.
#ifndef VCS_PATH
#define VCS_PATH "/var/crimson/state"
#endif

extern int get_uart_rx_fd();
extern int get_uart_tx_fd();

extern void server_init_led();
extern void server_ready_led();

// this should really be a conditional defined in configure.ac based on the hardware revision we're targetting, but ATM this is all I care about
#ifndef hw_rev_defined_
#define hw_rev_defined_

#define CRIMSON_TNG

#endif

enum {
	RX,
	TX
};

extern int set_freq_internal( const bool tx, const unsigned channel, const double freq );

struct synth_lut_ctx {
	const bool tx;
	const char *id;
	// "/var/crimson/calibration-data/TXA-XXXXXXXXXXXXXXXXXXXXXX.bin", where "XXXX.." are lowercase hex digits
	char fn[ PATH_MAX ];
	int fd;
	synth_rec_t *fm;
	size_t fs;
	bool enabled;
	pthread_mutex_t lock;

	size_t (*channel)( struct synth_lut_ctx *ctx );
	void (*disable)( struct synth_lut_ctx *ctx );
	int (*enable)( struct synth_lut_ctx *ctx );
	void (*erase)( struct synth_lut_ctx *ctx );
	int (*get)( struct synth_lut_ctx *ctx, const double freq, synth_rec_t *rec );
	int (*init)( struct synth_lut_ctx *ctx );
};

static size_t _synth_lut_channel( struct synth_lut_ctx *ctx );
static void _synth_lut_disable( struct synth_lut_ctx *ctx );
static int _synth_lut_enable( struct synth_lut_ctx *ctx );
static void _synth_lut_erase( struct synth_lut_ctx *ctx );
static int _synth_lut_get( struct synth_lut_ctx *ctx, const double freq, synth_rec_t *rec );
static int synth_lut_init( struct synth_lut_ctx *ctx );

#define DEF_RTX_CTX( rtx, rtxbool, ch ) \
	{ \
		.tx = rtxbool, \
		.id = #ch, \
		.fn = "", \
		.fd = -1, \
		.fm = MAP_FAILED, \
		.fs = 0, \
		.enabled = false, \
		.lock = PTHREAD_MUTEX_INITIALIZER, \
		.channel = _synth_lut_channel, \
		.disable = _synth_lut_disable, \
		.enable = _synth_lut_enable, \
		.erase = _synth_lut_erase, \
		.get = _synth_lut_get, \
		.init = synth_lut_init, \
	}
#define DEF_RX_CTX( ch ) DEF_RTX_CTX( rx, RX, ch )
#define DEF_TX_CTX( ch ) DEF_RTX_CTX( tx, TX, ch )


// Crimson TNG specific defines
#define FREQ_TOP    PLL1_RFOUT_MAX_HZ
//#define FREQ_BOTTOM PLL1_RFOUT_MIN_HZ
#define FREQ_BOTTOM 75000000

#define LO_STEP_SIZE PLL_CORE_REF_FREQ_HZ
static struct synth_lut_ctx synth_lut_rx_ctx[] = {
	DEF_RX_CTX( A ),
	DEF_RX_CTX( B ),
	DEF_RX_CTX( C ),
	DEF_RX_CTX( D ),
};

static struct synth_lut_ctx synth_lut_tx_ctx[] = {
	DEF_TX_CTX( A ),
	DEF_TX_CTX( B ),
	DEF_TX_CTX( C ),
	DEF_TX_CTX( D ),
};

#define SYNTH_LUT_LEN ( ( ( FREQ_TOP - FREQ_BOTTOM ) / LO_STEP_SIZE ) + 1 )

static int synth_lut_uart_cmd( const int fd, char *query, char *resp, const size_t resp_max_sz ) {
	int r;

	uint16_t resp_sz = 0;

	memset( resp, '\0', resp_max_sz );

	//PRINT( INFO, "writing '%s' to fd %d\n", query, fd );

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
		usleep( 100000 );
	}

	r = EXIT_SUCCESS;

out:
	return r;
}

static int synth_lut_calibrate_for_freq( const bool tx, const size_t chan_i, const double freq, synth_rec_t *rec ) {
	int r;

	char cmd_buf[ PATH_MAX ];
	char resp_buf[ PATH_MAX ];

	int core, band, bias;
	int uart_fd;

	uart_fd = tx ? get_uart_tx_fd() : get_uart_rx_fd();

	// tell the mcu to use autocal
	snprintf( cmd_buf, sizeof( cmd_buf ), "rf -c %c -A 1", 'a' + chan_i );
	r = synth_lut_uart_cmd( uart_fd, cmd_buf, resp_buf, sizeof( resp_buf ) );
	if ( EXIT_SUCCESS != r ) {
		PRINT( ERROR, "synth_lut_uart_cmd() failed (%d,%s)\n", r, strerror( r ) );
		goto out;
	}

	r = set_freq_internal( tx, chan_i, freq );
	if ( EXIT_SUCCESS != r ) {
		PRINT( ERROR, "failed to set %s %c @ %u MHz (%d,%s)\n", tx ? "TX" : "RX", 'A' + chan_i, freq / 1000000, r, strerror( r ) );
		goto out;
	}

	// read the value back
	snprintf( cmd_buf, sizeof(cmd_buf), "rf -c %c -p", 'a' + chan_i );
	r = synth_lut_uart_cmd( uart_fd, cmd_buf, resp_buf, sizeof( resp_buf ) );
	if ( EXIT_SUCCESS != r ) {
		PRINT( ERROR, "synth_lut_uart_cmd() failed (%d,%s)\n", r, strerror( r ) );
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

	snprintf( cmd_buf, sizeof(cmd_buf), "rf -c %c -A 0", 'a' + chan_i );
	r = synth_lut_uart_cmd( uart_fd, cmd_buf, resp_buf, sizeof( resp_buf ) );
	if ( EXIT_SUCCESS != r ) {
		goto out;
	}

	r = EXIT_SUCCESS;

out:
	return r;
}

static struct synth_lut_ctx *synth_lut_find( const bool tx, const size_t channel ) {

	struct synth_lut_ctx *it = NULL;

	//PRINT( INFO, "Looking for %s %c\n", tx ? "TX" : "RX", 'A' + channel );

	if ( tx ) {
		FOR_EACH( it, synth_lut_tx_ctx ) {
			//PRINT( INFO, "Considering TX %c @ %p\n", 'A' + i, & synth_lut_tx_ctx[ i ] );
			if ( channel == it->channel( it ) ) {
				//PRINT( INFO, "Found TX %c\n", 'A' + i );
				break;
			}
		}
	} else {
		FOR_EACH( it, synth_lut_rx_ctx ) {
			//PRINT( INFO, "Considering RX %c @ %p\n", 'A' + i, & synth_lut_rx_ctx[ i ] );
			if ( channel == it->channel( it ) ) {
				//PRINT( INFO, "Found RX %c\n", 'A' + i );
				break;
			}
		}
	}

	//PRINT( INFO, "Returning %s %c @ %p\n", tx ? "TX" : "RX", 'A' + channel, it );

	return it;
}

static int synth_lut_calibrate( struct synth_lut_ctx *ctx ) {

	int r;

	size_t i;
	double freq;
	size_t chan_i;

	synth_rec_t *rec;

	for( i = 0; i < SYNTH_LUT_LEN; i++ ) {

		freq = (double)FREQ_BOTTOM + i * (double)LO_STEP_SIZE;
		chan_i = ctx->channel( ctx );
		rec = & ctx->fm[ i ];

		r = synth_lut_calibrate_for_freq( ctx->tx, chan_i, freq, rec );
		if ( EXIT_SUCCESS != r ) {
			PRINT( ERROR, "%s %c @ %u MHz failed (%d,%s)\n",
				ctx->tx ? "TX" : "RX",
				'A' + chan_i,
				(unsigned)( freq / 1000000 ),
				r,
				strerror( r )
			);
			return r;
		}

		PRINT( INFO, "%s %c @ %u MHz: { %u, %u, %u }\n",
			ctx->tx ? "TX" : "RX",
			'A' + chan_i,
			(unsigned)( freq / 1000000 ),
			rec->core,
			rec->band,
			rec->bias
		);
	}

	return EXIT_SUCCESS;
}

static size_t _synth_lut_channel( struct synth_lut_ctx *ctx ) {

	size_t r;

	r = ctx->tx ? ARRAY_OFFSET( ctx, synth_lut_tx_ctx ) : ARRAY_OFFSET( ctx, synth_lut_rx_ctx );

	// PRINT( INFO, "synth_lut_rx_ctx: %p, synth_lut_tx_ctx: %p, ctx: %p, r: %u, sizeof( synth_lut_ctx ): %u\n", synth_lut_rx_ctx, synth_lut_tx_ctx, ctx, r, sizeof( *ctx ) );

	return r;
}

static void _synth_lut_disable( struct synth_lut_ctx *ctx ) {
	int r;

	pthread_mutex_lock( & ctx->lock );

	if ( ! ctx->enabled ) {
		goto out;
	}
	ctx->enabled = false;

	PRINT( INFO, "Disabling %s %c\n", ctx->tx ? "TX" : "RX", 'A' + ctx->channel( ctx ) );

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
	}
	ctx->fm = MAP_FAILED;
	ctx->fs = 0;

	if ( -1 != ctx->fd ) {
		PRINT( INFO, "Closing %s\n", ctx->fn );
		r = close( ctx->fd );
		if ( EXIT_SUCCESS != r ) {
			PRINT( ERROR, "Warning: Failed to close %s (%d,%s)\n", ctx->fn, r, strerror( errno ) );
		}
	}
	ctx->fd = -1;

out:
	pthread_mutex_unlock( & ctx->lock );
}

void synth_lut_disable( const bool tx, const size_t channel ) {

	struct synth_lut_ctx *it = synth_lut_find( tx, channel );
	if ( NULL == it ) {
		PRINT( ERROR, "unable to find %s %c\n", tx ? "TX" : "RX", 'A' + channel );
		goto out;
	}

	it->disable( it );

out:
	return;
}


static void _synth_lut_erase( struct synth_lut_ctx *ctx ) {

	pthread_mutex_lock( & ctx->lock );

	if ( -1 == remove( ctx->fn ) ) {
		PRINT( ERROR, "unable to remove %s (%d,%s)\n", ctx->fn, errno, strerror( errno ) );
	}

	pthread_mutex_unlock( & ctx->lock );
}

void synth_lut_erase( const bool tx, const size_t channel ) {

	struct synth_lut_ctx *it = synth_lut_find( tx, channel );
	if ( NULL == it ) {
		PRINT( ERROR, "unable to find %s %c\n", tx ? "TX" : "RX", 'A' + channel );
		goto out;
	}

	it->erase( it );

out:
	return;
}


static int _synth_lut_enable( struct synth_lut_ctx *ctx ) {

	static const size_t n = SYNTH_LUT_LEN;

	int r;

	struct stat st;
	synth_rec_t *rec = NULL;
	char *cmdbuf = NULL;
	size_t cmdbuf_sz;

	pthread_mutex_lock( & ctx->lock );

	if ( ctx->enabled ) {
		r = EXIT_SUCCESS;
		goto out;
	}

	server_init_led();

	PRINT( INFO, "Enabling sythesizer calibration tables on %s %c..\n", ctx->tx ? "TX" : "RX", 'A' + ctx->channel( ctx ) );

	r = ctx->init( ctx );
	if ( EXIT_SUCCESS != r ) {
		PRINT( INFO, "Failed to initialize %s %c (%d,%s)\n", ctx->tx ? "TX" : "RX", 'A' + ctx->channel( ctx ), r, strerror( r ) );
		goto out;
	}

	r = stat( ctx->fn, & st );
	if ( EXIT_SUCCESS != r ) {
		if ( ENOENT != errno ) {
			r = errno;
			PRINT( ERROR, "Failed to stat %s (%d,%s)\n", ctx->fn, errno, strerror( errno ) );
			goto out;
		}

		PRINT( INFO, "Did not find file '%s'\n", ctx->fn );

		// In this case, no calibration data exists on file, so we have to create a file of calibration data
		// We assume that after going through autocalibration once for each frequency, that the micro stores a copy of the calibrated values

		PRINT( INFO, "Creating calibration data for %s %s\n", ctx->tx ? "TX" : "RX", ctx->id );

		rec = calloc( n, sizeof( *rec ) );
		if ( NULL == rec ) {
			r = errno; // assumes malloc sets errno on failure (some libc's do not)
			PRINT( ERROR, "Failed to allocate %u bytes for synth_rec storage (%d,%s)\n", n * sizeof( *rec ), errno, strerror( errno ) );
			goto out;
		}

		//PRINT( INFO, "Allocated %u bytes @ %p for %u LUT records for %s %s\n", n * sizeof( *rec ), rec, n, ctx->tx ? "TX" : "RX", ctx->id );

		// we temporarily put our buffer inside the ctx
		ctx->fm = rec;

		r = synth_lut_calibrate( ctx );

		// remove reference to our buffer
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

		//PRINT( INFO, "Wrote %u bytes to '%s'\n", r, ctx->fn );

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

	ctx->fm = mmap( NULL, n * sizeof( *rec ), PROT_READ, MAP_SHARED, ctx->fd, 0 );
	if ( MAP_FAILED == ctx->fm ) {
		r = errno;
		PRINT( ERROR, "Failed to map calibration file %s (%d,%s)\n", ctx->fn, errno, strerror( errno ) );
		goto remove_file;
	}
	ctx->fs = n * sizeof( *rec );

	PRINT( INFO, "Mapped %u records of calibration data for %s %s at %p\n", n, ctx->tx ? "TX" : "RX", ctx->id, ctx->fm );

	r = EXIT_SUCCESS;
	ctx->enabled = true;

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
		//PRINT( INFO, "Freed memory @ %p\n", rec );
		rec = NULL;
	}

	server_ready_led();

	pthread_mutex_unlock( & ctx->lock );

	return r;
}

int synth_lut_enable( const bool tx, const size_t channel ) {
	int r;

	struct synth_lut_ctx *it = synth_lut_find( tx, channel );
	if ( NULL == it ) {
		PRINT( ERROR, "unable to find %s %c\n", tx ? "TX" : "RX", 'A' + channel );
		r = ENOENT;
		goto out;
	}

	r = it->enable( it );

out:
	return r;
}

static int _synth_lut_get( struct synth_lut_ctx *ctx, const double freq, synth_rec_t *rec ) {
	int r;

	double integral;
	double fractional;
	size_t k;
	size_t channel;

	pthread_mutex_lock( & ctx->lock );

	if ( ! ctx->enabled ) {
		PRINT( ERROR, "synth lut is not enabled" );
		r = ENOENT;
		goto out;
	}

	channel = ctx->channel( ctx );

	if (
		false
		|| freq < FREQ_BOTTOM
		|| 0 != ( fractional = modf( ( freq - FREQ_BOTTOM ) / (double)LO_STEP_SIZE, & integral ) )
		|| integral >= ctx->fs / sizeof( *rec )
	) {
		PRINT( ERROR, "unable to find calibration data for %s %c at frequency %f\n", ctx->tx ? "TX" : "RX", 'A' + channel, freq );
		r = EINVAL;
		goto out;
	}

	k = integral;

	*rec = ctx->fm[ k ];

	r = EXIT_SUCCESS;

out:

	pthread_mutex_unlock( & ctx->lock );

	return r;
}

int synth_lut_get( const bool tx, const uint8_t channel, const double freq, synth_rec_t *rec ) {
	int r;

	struct synth_lut_ctx *it = synth_lut_find( tx, channel );
	if ( NULL == it ) {
		PRINT( ERROR, "unable to find %s %c\n", tx ? "TX" : "RX", 'A' + channel );
		r = ENOENT;
		goto out;
	}

	r = it->get( it, freq, rec );

out:
	return r;
}

static int synth_lut_init( struct synth_lut_ctx *ctx ) {

	char req[] = "status -s";

	int r;

	// this array is used for both the command response and regular expression errors
	char buf[ 256 ];
	int uart_fd;

	regex_t preg;
	const char *regex = "^[0-9a-f]*";
	regmatch_t pmatch;

	if ( 0 != strlen( ctx->fn ) ) {
		// ctx->fn has already been initialized
		r = EXIT_SUCCESS;
		goto out;
	}

	r = regcomp( & preg, regex, 0 );
	if ( EXIT_SUCCESS != r ) {
		regerror( r, & preg, buf, sizeof( buf ) );
		PRINT( ERROR, "Failed to compile regular expression '%s' (%d,%s)\n", regex, r, buf );
		r = EINVAL;
		goto out;
	}

	uart_fd = ctx->tx ? get_uart_tx_fd() : get_uart_rx_fd();

	r = synth_lut_uart_cmd( uart_fd, (char *)req, buf, sizeof( buf ) );
	if ( EXIT_SUCCESS != r ) {
		PRINT( ERROR, "Failed to issue command '%s' to %s %c (%d,%s)\n", req, ctx->tx ? "TX" : "RX", 'A' + ctx->channel( ctx ), r, strerror( r ) );
		goto free_re;
	}

	r = regexec( & preg, buf, 1, & pmatch, 0 );
	if ( EXIT_SUCCESS != r ) {
		PRINT( ERROR, "Failed to match '%s' to regular expression '%s' for %s %c\n", buf, regex, ctx->tx ? "TX" : "RX", 'A' + ctx->channel( ctx ) );
		regerror( r, & preg, buf, sizeof( buf ) );
		PRINT( ERROR, "(%d,%s)\n", r, buf );
		r = EINVAL;
		goto free_re;
	}

	// truncate the string to the exact size of the matched regular expression
	buf[ pmatch.rm_eo ] = '\0';

	snprintf( ctx->fn, sizeof( ctx->fn ), "/var/crimson/calibration-data/%s%c-%s.bin", ctx->tx ? "TX" : "RX", 'A' + ctx->channel( ctx ), buf );
	r = EXIT_SUCCESS;

free_re:
	regfree( & preg );

out:
	return r;
}

int synth_lut_enable_all() {
	int r;

	struct synth_lut_ctx *it;

	FOR_EACH( it, synth_lut_rx_ctx ) {
		r = it->enable( it );
		if ( EXIT_SUCCESS != r ) {
			goto out;
		}
	}

	FOR_EACH( it, synth_lut_tx_ctx ) {
		r = it->enable( it );
		if ( EXIT_SUCCESS != r ) {
			goto out;
		}
	}

out:
	return r;
}

void synth_lut_disable_all() {
	struct synth_lut_ctx *it;

	FOR_EACH( it, synth_lut_rx_ctx ) {
		it->disable( it );
	}

	FOR_EACH( it, synth_lut_tx_ctx ) {
		it->disable( it );
	}
}

bool synth_lut_is_enabled( const bool tx, const size_t channel ) {
	bool r;

	struct synth_lut_ctx *it = synth_lut_find( tx, channel );
	if ( NULL == it ) {
		PRINT( ERROR, "unable to find %s %c\n", tx ? "TX" : "RX", 'A' + channel );
		r = false;
		goto out;
	}

	r = it->enabled;

out:
	return r;
}
