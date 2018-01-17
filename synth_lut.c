#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
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
#include "synth_lut.h"

#ifndef SYNTH_LUT_LEN
#define SYNTH_LUT_LEN ( 6000000000 / 25000000 )
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
 * id - a single char describing the channel (e.g. 'A')
 * fn - path to calibration data for the channel
 * fd - file descriptor for the file named in fn
 * fm - mmap(2) for the file named in fn
 * ms - array size of fm
 */
struct synth_lut_ctx {
	const bool tx;
	const char *id;
	const char fn[ PATH_MAX ];
	int fd;
	synth_rec_t *fm;
	size_t fs;
};

// the two tables below are sufficient for Crimson TNG

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

static void synth_lut_fini_one_board( struct synth_lut_ctx *ctx );

static int synth_lut_calibrate_one_board( struct synth_lut_ctx *ctx, const bool manual ) {

	if ( manual ) {

		PRINT( INFO, "Calibrating %s %c using data stored in %s\n", ctx->tx ? "TX" : "RX", ctx->id, ctx->fn );

	} else {

		PRINT( INFO, "Performing initial calibration of %s %c..\n", ctx->tx ? "TX" : "RX", ctx->id, ctx->fn );

	}

	return EXIT_SUCCESS;
}

static int synth_lut_init_one_board( struct synth_lut_ctx *ctx ) {

	static const bool manual = true;
	static const bool automatic = false;

	static const size_t n = SYNTH_LUT_LEN;

	int r;

	struct stat st;
	synth_rec_t *rec = NULL;

	r = stat( ctx->fn, & st );
	if ( EXIT_SUCCESS != r ) {
		if ( ENOENT != errno ) {
			r = errno;
			PRINT( ERROR, "Failed to stat %s (%d,%s)\n", ctx->fn, errno, strerror( errno ) );
			goto out;
		}

		// In this case, no calibration data exists on file, so we have to create a file of calibration data
		// We assume that after going through autocalibration once for each frequency, that the micro stores a copy of the calibration table

		PRINT( INFO, "Creating calibration data for %s %c\n", ctx->tx ? "TX" : "RX", ctx->id );

		rec = calloc( n, sizeof( *rec ) );
		if ( NULL == rec ) {
			r = errno; // assumes malloc sets errno on failure (some libc's do not)
			PRINT( ERROR, "Failed to allocate %u bytes for synth_rec storage (%d,%s)\n", n * sizeof( *rec ), errno, strerror( errno ) );
			goto out;
		}

		r = synth_lut_calibrate_one_board( ctx, automatic );
		if ( EXIT_SUCCESS != r ) {
			PRINT( ERROR, "Failed to generate calibration data (%d,%s)\n", errno, strerror( errno ) );
			goto out;
		}

		// when stat sets errno to ENOENT, it means the file does not exist
		// we must create the file, write calibration data to it, and then close the file

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
			goto out;
		}

		if ( n * sizeof( *rec ) != r ) {
			r = EINVAL;
			PRINT( ERROR, "Wrote wrong number of bytes to calibration data file. Expected: %u, Actual: %u\n", n * sizeof( *rec ), r );
			goto out;
		}

		r = close( ctx->fd );
		if ( -1 == r ) {
			PRINT( ERROR, "Warning: Failed to close calibration data file (%d,%s)\n", errno, strerror( errno ) );
		}
		ctx->fd = -1;

		PRINT( INFO, "Created calibration data for %s %c\n", ctx->tx ? "TX" : "RX", ctx->id );

		r = EXIT_SUCCESS;
		goto out;
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

	ctx->fm = mmap( NULL, ctx->fs, PROT_READ, MAP_SHARED, ctx->fd, 0 );
	if ( MAP_FAILED == ctx->fm ) {
		r = errno;
		PRINT( ERROR, "Failed to map calibration file %s (%d,%s)\n", ctx->fn, errno, strerror( errno ) );
		goto out;
	}
	ctx->fs = n * sizeof( *rec );

	r = synth_lut_calibrate_one_board( ctx, manual );
	if ( EXIT_SUCCESS != r ) {
		PRINT( ERROR, "Failed to calibrate %s %c (%d,%s)\n", ctx->tx ? "TX" : "RX", ctx->id, r, strerror( r ) );
		goto out;
	}

	synth_lut_fini_one_board( ctx );

out:
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
