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
#include <termios.h>
#include <unistd.h>

#include "array-utils.h"
#include "common.h"
#include "comm_manager.h"
#include "synth_lut.h"

#include "pllcalc.h"

// I couldn't actually find this hard-coded anywhere.
#ifndef VCS_PATH
    #if defined(TATE)
        #define VCS_PATH "/var/cyan/state"
    #elif defined(TATE_4R4T)
        #define VCS_PATH "/var/cyan/state"
    #elif defined(TATE_8R)
        #define VCS_PATH "/var/cyan/state"
    #elif defined(VAUNT)
        #define VCS_PATH "/var/crimson/state"
    #else
        #error "This file must be compiled with a valid PRODUCT (TATE, TATE_4R4T, TATE_8R, VAUNT). Confirm spelling and spaces."
    #endif
#endif
extern void server_init_led();
extern void server_ready_led();

enum { RX, TX };

extern int set_freq_internal(const bool tx, const unsigned channel,
                             const double freq);

struct synth_lut_ctx {
    const bool tx;
    const char *id;
    // "/var/crimson/calibration-data/TXA-XXXXXXXXXXXXXXXXXXXXXX.bin", where
    // "XXXX.." are lowercase hex digits
    char fn[PATH_MAX];
    int fd;
    synth_rec_t *fm;
    size_t fs;
    bool enabled;
    pthread_mutex_t lock;

    uint32_t (*channel)(struct synth_lut_ctx *ctx);
    void (*disable)(struct synth_lut_ctx *ctx);
    int (*enable)(struct synth_lut_ctx *ctx);
    void (*erase)(struct synth_lut_ctx *ctx);
    int (*is_calibrated)(struct synth_lut_ctx *ctx, bool *is);
    int (*get)(struct synth_lut_ctx *ctx, const double freq, synth_rec_t *rec);
    int (*init)(struct synth_lut_ctx *ctx);
    int (*set_freq)(struct synth_lut_ctx *ctx, const double freq);
    int (*autocal_enable)(struct synth_lut_ctx *ctx, const bool en);
    int (*autocal_values)(struct synth_lut_ctx *ctx, synth_rec_t *rec);
};

static uint32_t _synth_lut_channel(struct synth_lut_ctx *ctx);
static void _synth_lut_disable(struct synth_lut_ctx *ctx);
static int _synth_lut_enable(struct synth_lut_ctx *ctx);
static void _synth_lut_erase(struct synth_lut_ctx *ctx);
static int _synth_lut_is_calibrated(struct synth_lut_ctx *ctx, bool *is);
static int _synth_lut_get(struct synth_lut_ctx *ctx, const double freq,
                          synth_rec_t *rec);
static int _synth_lut_init(struct synth_lut_ctx *ctx);
static int _synth_lut_set_freq(struct synth_lut_ctx *ctx, const double freq);
static int _synth_lut_autocal_enable(struct synth_lut_ctx *ctx, const bool en);
static int _synth_lut_autocal_values(struct synth_lut_ctx *ctx,
                                     synth_rec_t *rec);

#define DEF_RTX_CTX(rtx, rtxbool, ch)                                          \
    {                                                                          \
        .tx = rtxbool, .id = #ch, .fn = "", .fd = -1, .fm = MAP_FAILED,        \
        .fs = 0, .enabled = false, .lock = PTHREAD_MUTEX_INITIALIZER,          \
        .channel = _synth_lut_channel, .disable = _synth_lut_disable,          \
        .enable = _synth_lut_enable, .erase = _synth_lut_erase,                \
        .is_calibrated = _synth_lut_is_calibrated, .get = _synth_lut_get,      \
        .init = _synth_lut_init, .set_freq = _synth_lut_set_freq,              \
        .autocal_enable = _synth_lut_autocal_enable,                           \
        .autocal_values = _synth_lut_autocal_values,                           \
    }
#define DEF_RX_CTX(ch) DEF_RTX_CTX(rx, RX, ch)
#define DEF_TX_CTX(ch) DEF_RTX_CTX(tx, TX, ch)

// Crimson TNG specific defines
#define FREQ_TOP PLL1_RFOUT_MAX_HZ
//#define FREQ_BOTTOM PLL1_RFOUT_MIN_HZ
#define FREQ_BOTTOM 125000000

#define LO_STEP_SIZE PLL_CORE_REF_FREQ_HZ_ADF5355
static struct synth_lut_ctx synth_lut_rx_ctx[] = {
    DEF_RX_CTX(A),
    DEF_RX_CTX(B),
    DEF_RX_CTX(C),
    DEF_RX_CTX(D),
    DEF_RX_CTX(E),
    DEF_RX_CTX(F),
    DEF_RX_CTX(G),
    DEF_RX_CTX(H),
    DEF_RX_CTX(I),
    DEF_RX_CTX(J),
    DEF_RX_CTX(K),
    DEF_RX_CTX(L),
    DEF_RX_CTX(M),
    DEF_RX_CTX(N),
    DEF_RX_CTX(O),
    DEF_RX_CTX(P),
};

static struct synth_lut_ctx synth_lut_tx_ctx[] = {
    DEF_TX_CTX(A),
    DEF_TX_CTX(B),
    DEF_TX_CTX(C),
    DEF_TX_CTX(D),
    DEF_TX_CTX(E),
    DEF_TX_CTX(F),
    DEF_TX_CTX(G),
    DEF_TX_CTX(H),
    DEF_TX_CTX(I),
    DEF_TX_CTX(J),
    DEF_TX_CTX(K),
    DEF_TX_CTX(L),
    DEF_TX_CTX(M),
    DEF_TX_CTX(N),
    DEF_TX_CTX(O),
    DEF_TX_CTX(P),
};

#define SYNTH_LUT_LEN ((size_t)(((FREQ_TOP - FREQ_BOTTOM) / LO_STEP_SIZE) + 1))

static int synth_lut_uart_cmd(const int fd, char *query, char *resp,
                              const size_t resp_max_sz) {
    int r;

    uint16_t resp_sz = 0;

    memset(resp, '\0', resp_max_sz);

    // PRINT( INFO, "writing '%s' to fd %d\n", query, fd );

    query[strlen(query)] = '\r';

    if (!isatty(fd)) {
        r = errno;
        PRINT(ERROR, "fd %d is not a tty (%d,%s)\n", fd, errno,
              strerror(errno));
        goto out;
    }

    r = tcflush(fd, TCIOFLUSH);
    if (EXIT_SUCCESS != r) {
        PRINT(ERROR, "tcflush on fd %d failed (%d,%s)\n", fd, errno,
              strerror(errno));
    }

    r = write(fd, query, strlen(query));
    if (strlen(query) != r) {
        PRINT(ERROR, "failed to send query '%s' on uart_fd %d\n", query, fd,
              errno, strerror(errno));
        r = errno;
        goto out;
    }

    int tries = 10;
    for (resp_sz = 0, tries = 10; NULL == strstr(resp, ">") && tries; tries--) {
        // read_uart_comm is super annoying and does not seem to work for this,
        // only returning partial strings
        int rr = read(fd, &resp[resp_sz], resp_max_sz - resp_sz);

        // PRINT( INFO, "read() returned %d: resp: '%s'\n", rr, resp );

        switch (rr) {

        case 0:
            // end of file
            break;

        case -1:
            // error
            if (ETIMEDOUT == errno || EAGAIN == errno) {
                break;
            }
            PRINT(ERROR,
                  "failed to receive response to command '%s' on uart_fd %d\n",
                  query, fd);
            r = errno;
            goto out;

        default:

            resp_sz += rr;
            break;
        }
        usleep(100000);
    }

    r = EXIT_SUCCESS;

out:
    return r;
}

#if 0
static int synth_lut_calibrate_one_for_freq(struct synth_lut_ctx *ctx,
                                            const double freq,
                                            synth_rec_t *rec) {
    int r;

    bool tx = ctx->tx;
    size_t chan_i = ctx->channel(ctx);

    r = ctx->autocal_enable(ctx, true);
    if (EXIT_SUCCESS != r) {
        PRINT(ERROR, "autocal_enable() failed (%d,%s)\n", r, strerror(r));
        goto out;
    }

    r = ctx->set_freq(ctx, freq);
    if (EXIT_SUCCESS != r) {
        PRINT(ERROR, "failed to set %s %c @ %u MHz (%d,%s)\n", tx ? "TX" : "RX",
              'A' + chan_i, freq / 1000000, r, strerror(r));
        goto out;
    }

    r = ctx->autocal_values(ctx, rec);
    if (EXIT_SUCCESS != r) {
        PRINT(ERROR,
              "failed to read back autocal values %s %c @ %u MHz (%d,%s)\n",
              tx ? "TX" : "RX", 'A' + chan_i, freq / 1000000, r, strerror(r));
        goto out;
    }

    r = ctx->autocal_enable(ctx, false);
    if (EXIT_SUCCESS != r) {
        PRINT(ERROR, "autocal_enable() failed (%d,%s)\n", r, strerror(r));
        goto out;
    }

    r = EXIT_SUCCESS;

out:
    return r;
}
#endif

static int synth_lut_calibrate_n_for_freq(const double freq, const size_t n,
                                          struct synth_lut_ctx *ctx_[],
                                          synth_rec_t *rec_[]) {
    int r;

    size_t i;
    bool tx;
    size_t chan_i;
    struct synth_lut_ctx *ctx;
    synth_rec_t *rec;

    // disable autocal for all channels
    for (i = 0; i < n; i++) {
        ctx = ctx_[i];
        r = ctx->autocal_enable(ctx, true);
        if (EXIT_SUCCESS != r) {
            PRINT(ERROR, "autocal_enable() failed (%d,%s)\n", r, strerror(r));
            goto out;
        }
    }

    // tune for all channels
    for (i = 0; i < n; i++) {
        ctx = ctx_[i];
        tx = ctx->tx;
        chan_i = ctx->channel(ctx);
        r = ctx->set_freq(ctx, freq);
        if (EXIT_SUCCESS != r) {
            PRINT(ERROR, "failed to set %s %c @ %u MHz (%d,%s)\n",
                  tx ? "TX" : "RX", 'A' + chan_i, freq / 1000000, r,
                  strerror(r));
            goto out;
        }
    }

    // read back calibration data for all channels
    for (i = 0; i < n; i++) {
        ctx = ctx_[i];
        rec = rec_[i];
        tx = ctx->tx;
        chan_i = ctx->channel(ctx);
        r = ctx->autocal_values(ctx, rec);
        if (EXIT_SUCCESS != r) {
            PRINT(ERROR,
                  "failed to read back autocal values %s %c @ %u MHz (%d,%s)\n",
                  tx ? "TX" : "RX", 'A' + chan_i, freq / 1000000, r,
                  strerror(r));
            goto out;
        }

        PRINT(INFO, "%s %c @ %u MHz: { %u, %u, %u }\n", ctx->tx ? "TX" : "RX",
              'A' + chan_i, (unsigned)(freq / 1000000), rec->core, rec->band,
              rec->bias);
    }

    // disable autocal for all channels
    for (i = 0; i < n; i++) {
        ctx = ctx_[i];
        r = ctx->autocal_enable(ctx, false);
        if (EXIT_SUCCESS != r) {
            PRINT(ERROR, "autocal_enable() failed (%d,%s)\n", r, strerror(r));
            goto out;
        }
    }

    r = EXIT_SUCCESS;

out:
    return r;
}

static struct synth_lut_ctx *synth_lut_find(const bool tx,
                                            const uint32_t channel) {

    struct synth_lut_ctx *it = NULL;

    // PRINT( INFO, "Looking for %s %c\n", tx ? "TX" : "RX", 'A' + channel );

    if (tx) {
        int i = 0;
        FOR_EACH(it, synth_lut_tx_ctx) {
            PRINT( INFO, "Considering TX %c @ %p\n", 'A' + i, &synth_lut_tx_ctx[ i ] );
            if (channel == it->channel(it)) {
                PRINT( INFO, "Found TX %c\n", 'A' + i );
                break;
            }
            i++;
        }
    } else {
        int i = 0;
        FOR_EACH(it, synth_lut_rx_ctx) {
            PRINT( INFO, "Considering RX %c @ %p\n", 'A' + i, &synth_lut_rx_ctx[ i ] );
            if (channel == it->channel(it)) {
                PRINT( INFO, "Found RX %c\n", 'A' + i );
                break;
            }
            i++;
        }
    }

    PRINT( INFO, "Returning %s %c @ %p\n", tx ? "TX" : "RX", 'A' + channel, it );

    return it;
}

#if 0
static int synth_lut_calibrate_one(struct synth_lut_ctx *ctx) {

    int r;

    size_t i;
    double freq;
    size_t chan_i;

    synth_rec_t *rec;

    for (i = 0; i < SYNTH_LUT_LEN; i++) {

        freq = (double)FREQ_BOTTOM + i * (double)LO_STEP_SIZE;
        chan_i = ctx->channel(ctx);
        rec = &ctx->fm[i];

        r = synth_lut_calibrate_one_for_freq(ctx, freq, rec);
        if (EXIT_SUCCESS != r) {
            PRINT(ERROR, "%s %c @ %u MHz failed (%d,%s)\n",
                  ctx->tx ? "TX" : "RX", 'A' + chan_i,
                  (unsigned)(freq / 1000000), r, strerror(r));
            return r;
        }

        PRINT(INFO, "%s %c @ %u MHz: { %u, %u, %u }\n", ctx->tx ? "TX" : "RX",
              'A' + chan_i, (unsigned)(freq / 1000000), rec->core, rec->band,
              rec->bias);
    }

    return EXIT_SUCCESS;
}
#endif

static int synth_lut_recalibrate_all() {

    int r;

    size_t i;
    size_t j;
    double freq;
    size_t n_channels;
    size_t cmdbuf_sz;
    char *cmdbuf = NULL;

    synth_rec_t **rec = NULL;
    struct synth_lut_ctx **ctx = NULL;

    synth_rec_t *rect = NULL;

    synth_lut_erase_all();

    n_channels = ARRAY_SIZE(synth_lut_rx_ctx) + ARRAY_SIZE(synth_lut_tx_ctx);

    // create an array of rec to pass to synth_lut_calibrate_n_for_freq
    rec = calloc(n_channels, sizeof(*rec));
    if (NULL == rec) {
        r = errno;
        PRINT(ERROR, "calloc failed (%d, %s)\n", r, strerror(r));
        goto out;
    }

    // create an array of ctx to pass to synth_lut_calibrate_n_for_freq
    ctx = calloc(n_channels, sizeof(*ctx));
    if (NULL == ctx) {
        r = errno;
        PRINT(ERROR, "calloc failed (%d, %s)\n", r, strerror(r));
        goto out;
    }

    // populate our array of ctx to pass into synth_lut_calibrate_n_for_freq
    for (j = 0; j < ARRAY_SIZE(synth_lut_rx_ctx); j++) {
        ctx[j] = &synth_lut_rx_ctx[j];
    }
    for (j = 0; j < ARRAY_SIZE(synth_lut_tx_ctx); j++) {
        ctx[ARRAY_SIZE(synth_lut_rx_ctx) + j] = &synth_lut_tx_ctx[j];
    }

    // just to reduce the number of allocations to free, we just allocate space
    // for all calibration tables contiguously
    // PRINT( INFO, "allocating %u * %u * %u = %u bytes for %u synt_rec_t\n", n,
    // SYNTH_LUT_LEN, sizeof( synth_rec_t ), n * SYNTH_LUT_LEN * sizeof(
    // synth_rec_t ), n * SYNTH_LUT_LEN );
    rect = calloc(n_channels * SYNTH_LUT_LEN, sizeof(*rect));
    if (NULL == rect) {
        r = errno;
        PRINT(ERROR, "calloc failed (%d, %s)\n", r, strerror(r));
        goto out;
    }
    for (j = 0; j < n_channels; j++) {
        ctx[j]->fm = &rect[j * SYNTH_LUT_LEN];
    }

    // for each frequency, calibrate all channels
    for (i = 0; i < SYNTH_LUT_LEN; i++) {

        freq = (double)FREQ_BOTTOM + i * (double)LO_STEP_SIZE;

        // populate our array of rec to pass to synth_lut_calibrate_n_for_freq
        for (j = 0; j < n_channels; j++) {
            rec[j] = &ctx[j]->fm[i];
        }

        PRINT(INFO, "calling synth_lut_calibrate_n_for_freq()..\n");

        r = synth_lut_calibrate_n_for_freq(freq, n_channels, ctx, rec);
        if (EXIT_SUCCESS != r) {
            PRINT(ERROR, "%u MHz failed (%d,%s)\n", (unsigned)(freq / 1000000),
                  r, strerror(r));
            goto out;
        }
    }

    for (j = 0; j < n_channels; j++) {

        cmdbuf_sz = strlen("'mkdir -p $(dirname ") + strlen(ctx[j]->fn) +
                    strlen(")'") + sizeof('\0');
        cmdbuf = malloc(cmdbuf_sz);
        if (NULL == cmdbuf) {
            PRINT(ERROR, "Failed to allocate memory for mkdir -p (%d,%s)\n",
                  errno, strerror(errno));
            goto out;
        }
        snprintf(cmdbuf, cmdbuf_sz, "mkdir -p $(dirname %s)", ctx[j]->fn);
        PRINT(INFO, "Running cmd '%s'\n", cmdbuf);
        r = system(cmdbuf);
        if (EXIT_SUCCESS != r) {
            errno = EIO;
            PRINT(ERROR, "Failed to run command %s\n", cmdbuf, errno,
                  strerror(errno));
            goto out;
        }

        // when stat sets errno to ENOENT, it means the file does not exist
        // we must create the file, write calibration data to it, close, and
        // then reopen the file, read-only

        r = open(ctx[j]->fn, O_RDWR | O_SYNC | O_CREAT,
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (-1 == r) {
            r = errno;
            PRINT(ERROR,
                  "Failed to create / open calibration data file %s (%d,%s)\n",
                  ctx[j]->fn, errno, strerror(errno));
            goto out;
        }
        ctx[j]->fd = r;

        r = write(ctx[j]->fd, ctx[j]->fm, SYNTH_LUT_LEN * sizeof(synth_rec_t));
        if (-1 == r) {
            r = errno;
            PRINT(ERROR, "Failed to write calibration data (%d,%s)\n", errno,
                  strerror(errno));
            remove(ctx[j]->fn);
            goto out;
        }

        // PRINT( INFO, "Wrote %u bytes to '%s'\n", r, ctx->fn );

        if (SYNTH_LUT_LEN * sizeof(synth_rec_t) != r) {
            r = EINVAL;
            PRINT(ERROR,
                  "Wrote wrong number of bytes to calibration data file. "
                  "Expected: %u, Actual: %u\n",
                  SYNTH_LUT_LEN * sizeof(synth_rec_t), r);
            remove(ctx[j]->fn);
            goto out;
        }

        r = close(ctx[j]->fd);
        if (-1 == r) {
            PRINT(ERROR,
                  "Warning: Failed to close calibration data file (%d,%s)\n",
                  errno, strerror(errno));
        }
        ctx[j]->fd = -1;
    }

out:
    if (NULL != cmdbuf) {
        free(cmdbuf);
        cmdbuf = NULL;
    }
    if (NULL != rect) {
        free(rect);
        rect = NULL;
    }
    if (NULL != rec) {
        free(rec);
        rec = NULL;
    }
    if (NULL != ctx) {

        for (j = 0; j < n_channels; j++) {
            // there was no memory map performed, just a malloc, pointed to by
            // "rect", which is freed above
            ctx[j]->fm = MAP_FAILED;
        }

        for (j = 0; j < n_channels; j++) {
            if (-1 != ctx[j]->fd) {
                close(ctx[j]->fd);
                ctx[j]->fd = -1;
            }
        }

        free(ctx);
        ctx = NULL;
    }
    return r;
}

static uint32_t _synth_lut_channel(struct synth_lut_ctx *ctx) {

    size_t r;

    r = ctx->tx ? ARRAY_OFFSET(ctx, synth_lut_tx_ctx)
                : ARRAY_OFFSET(ctx, synth_lut_rx_ctx);

    // PRINT( INFO, "synth_lut_rx_ctx: %p, synth_lut_tx_ctx: %p, ctx: %p, r: %u,
    // sizeof( synth_lut_ctx ): %u\n", synth_lut_rx_ctx, synth_lut_tx_ctx, ctx,
    // r, sizeof( *ctx ) );

    return r;
}

static void _synth_lut_disable(struct synth_lut_ctx *ctx) {
    int r;

    char cmdbuf[PATH_MAX];

    pthread_mutex_lock(&ctx->lock);

    if (!ctx->enabled) {
        goto out;
    }
    ctx->enabled = false;

    PRINT(INFO, "Disabling %s %c\n", ctx->tx ? "TX" : "RX",
          'A' + ctx->channel(ctx));

    if (true && MAP_FAILED != ctx->fm && -1 != ctx->fd) {
        PRINT(INFO, "Unmapping %s\n", ctx->fn);
        r = munmap(ctx->fm, ctx->fs);
        if (EXIT_SUCCESS != r) {
            PRINT(ERROR, "Warning: Failed to unmap %s (%d,%s)\n", ctx->fn, r,
                  strerror(errno));
        }
    }
    ctx->fm = MAP_FAILED;
    ctx->fs = 0;

    if (-1 != ctx->fd) {
        PRINT(INFO, "Closing %s\n", ctx->fn);
        r = close(ctx->fd);
        if (EXIT_SUCCESS != r) {
            PRINT(ERROR, "Warning: Failed to close %s (%d,%s)\n", ctx->fn, r,
                  strerror(errno));
        }
    }
    ctx->fd = -1;

#if defined(TATE)
    snprintf(cmdbuf, sizeof(cmdbuf),
             "echo 0 > /var/cyan/state/%cx/%c/rf/freq/lut_en",
             ctx->tx ? 't' : 'r', 'a' + ctx->channel(ctx));
#elif defined(TATE_4R4T)
    snprintf(cmdbuf, sizeof(cmdbuf),
             "echo 0 > /var/cyan/state/%cx/%c/rf/freq/lut_en",
             ctx->tx ? 't' : 'r', 'a' + ctx->channel(ctx));
#elif defined(TATE_8R)
    snprintf(cmdbuf, sizeof(cmdbuf),
             "echo 0 > /var/cyan/state/%cx/%lc/rf/freq/lut_en",
             ctx->tx ? 't' : 'r', 'a' + (uint32_t)ctx->channel(ctx));
#elif defined(VAUNT)
    snprintf(cmdbuf, sizeof(cmdbuf),
             "echo 0 > /var/crimson/state/%cx/%c/rf/freq/lut_en",
             ctx->tx ? 't' : 'r', 'a' + ctx->channel(ctx));
#else
    #error "This file must be compiled with a valid PRODUCT (TATE, TATE_4R4T, TATE_8R, VAUNT). Confirm spelling and spaces."
#endif
    system(cmdbuf);

out:
    pthread_mutex_unlock(&ctx->lock);
}

void synth_lut_disable(const bool tx, const uint32_t channel) {

    struct synth_lut_ctx *it = synth_lut_find(tx, channel);

    printf("disable %p\n", it);

    if (NULL == it) {
        PRINT(ERROR, "unable to find %s %c\n", tx ? "TX" : "RX", 'A' + channel);
        goto out;
    }

    it->disable(it);

out:
    return;
}

static void _synth_lut_erase(struct synth_lut_ctx *ctx) {

    PRINT(INFO, "Erasing %s\n", ctx->fn);

    pthread_mutex_lock(&ctx->lock);

    ctx->disable(ctx);

    if (-1 == remove(ctx->fn)) {
        if (ENOENT != errno) {
            PRINT(ERROR, "unable to remove %s (%d,%s)\n", ctx->fn, errno,
                  strerror(errno));
        }
    }

    pthread_mutex_unlock(&ctx->lock);
}

void synth_lut_erase(const bool tx, const uint32_t channel) {

    struct synth_lut_ctx *it = synth_lut_find(tx, channel);
    if (NULL == it) {
        PRINT(ERROR, "unable to find %s %c\n", tx ? "TX" : "RX", 'A' + channel);
        goto out;
    }

    it->erase(it);

out:
    return;
}

void synth_lut_erase_all() {

    struct synth_lut_ctx *it;

    FOR_EACH(it, synth_lut_rx_ctx) { it->erase(it); }

    FOR_EACH(it, synth_lut_tx_ctx) { it->erase(it); }
}

static int _synth_lut_enable(struct synth_lut_ctx *ctx) {

    static const size_t n = SYNTH_LUT_LEN;

    int r;

    synth_rec_t *rec = NULL;
    char cmdbuf[PATH_MAX];

    pthread_mutex_lock(&ctx->lock);

    if (ctx->enabled) {
        r = EXIT_SUCCESS;
        goto out;
    }

    server_init_led();

    PRINT(INFO, "Enabling sythesizer calibration tables on %s %c..\n",
          ctx->tx ? "TX" : "RX", 'A' + ctx->channel(ctx));

    r = ctx->init(ctx);
    if (EXIT_SUCCESS != r) {
        PRINT(ERROR, "Failed to initialize %s %c (%d,%s)\n",
              ctx->tx ? "TX" : "RX", 'A' + ctx->channel(ctx), r, strerror(r));
        goto out;
    }

    bool calib;
    r = ctx->is_calibrated(ctx, &calib);
    if (EXIT_SUCCESS != r) {
        PRINT(ERROR, "Failed to check calibration status %s %c (%d,%s)\n",
              ctx->tx ? "TX" : "RX", 'A' + ctx->channel(ctx), r, strerror(r));
        goto out;
    }

    if (!calib) {
        r = synth_lut_recalibrate_all();
        if (EXIT_SUCCESS != r) {
            PRINT(ERROR, "Failed to recalibrate all (%d,%s)\n", errno,
                  strerror(errno));
        }
        goto out;
    }

    PRINT(INFO, "Opening calibration data file %s\n", ctx->fn);

    // we have an existing calibration table on file
    // we need to communicate that calibration data to the micro

    r = open(ctx->fn, O_RDONLY, 0);
    if (-1 == r) {
        r = errno;
        PRINT(ERROR, "Failed to open calibration data file %s (%d,%s)\n",
              ctx->fn, errno, strerror(errno));
        goto out;
    }
    ctx->fd = r;

    ctx->fm = mmap(NULL, n * sizeof(*rec), PROT_READ, MAP_SHARED, ctx->fd, 0);
    if (MAP_FAILED == ctx->fm) {
        r = errno;
        PRINT(ERROR, "Failed to map calibration file %s (%d,%s)\n", ctx->fn,
              errno, strerror(errno));
        goto out;
    }
    ctx->fs = n * sizeof(*rec);

    PRINT(INFO, "Mapped %u records of calibration data for %s %s at %p\n", n,
          ctx->tx ? "TX" : "RX", ctx->id, ctx->fm);

    r = EXIT_SUCCESS;
    ctx->enabled = true;

#if defined(TATE)
    snprintf(cmdbuf, sizeof(cmdbuf),
             "echo 1 > /var/cyan/state/%cx/%lc/rf/freq/lut_en",
             ctx->tx ? 't' : 'r', 'a' + ctx->channel(ctx));
#elif defined(TATE_4R4T)
    snprintf(cmdbuf, sizeof(cmdbuf),
             "echo 1 > /var/cyan/state/%cx/%lc/rf/freq/lut_en",
             ctx->tx ? 't' : 'r', 'a' + ctx->channel(ctx));
#elif defined(TATE_8R)
    snprintf(cmdbuf, sizeof(cmdbuf),
             "echo 1 > /var/cyan/state/%cx/%lc/rf/freq/lut_en",
             ctx->tx ? 't' : 'r', 'a' + ctx->channel(ctx));
#elif defined(VAUNT)
    snprintf(cmdbuf, sizeof(cmdbuf),
             "echo 1 > /var/crimson/state/%cx/%lc/rf/freq/lut_en",
             ctx->tx ? 't' : 'r', 'a' + ctx->channel(ctx));
#else
    #error "This file must be compiled with a valid PRODUCT (TATE, TATE_4R4T, TATE_8R, VAUNT). Confirm spelling and spaces."
#endif
    system(cmdbuf);

out:
    if (NULL != rec) {
        free(rec);
        // PRINT( INFO, "Freed memory @ %p\n", rec );
        rec = NULL;
    }

    server_ready_led();

    pthread_mutex_unlock(&ctx->lock);

    return r;
}

int synth_lut_enable(const bool tx, const uint32_t channel) {
    int r;

    struct synth_lut_ctx *it = synth_lut_find(tx, channel);

    printf("%p\n", it);


    if (NULL == it) {
        PRINT(ERROR, "unable to find %s %c\n", tx ? "TX" : "RX", 'A' + channel);
        r = ENOENT;
        goto out;
    }

    r = it->enable(it);

out:
    return r;
}

static int _synth_lut_get(struct synth_lut_ctx *ctx, const double freq,
                          synth_rec_t *rec) {
    int r;

    double integral;
    double fractional;
    size_t k;
    uint32_t channel;

    pthread_mutex_lock(&ctx->lock);

    if (!ctx->enabled) {
        PRINT(ERROR, "synth lut is not enabled");
        r = ENOENT;
        goto out;
    }

    channel = ctx->channel(ctx);

    if (false || freq < FREQ_BOTTOM ||
        0 != (fractional = modf((freq - FREQ_BOTTOM) / (double)LO_STEP_SIZE,
                                &integral)) ||
        integral >= ctx->fs / sizeof(*rec)) {
        PRINT(ERROR,
              "unable to find calibration data for %s %c at frequency %f\n",
              ctx->tx ? "TX" : "RX", 'A' + channel, freq);
        r = EINVAL;
        goto out;
    }

    k = integral;

    *rec = ctx->fm[k];

    r = EXIT_SUCCESS;

out:

    pthread_mutex_unlock(&ctx->lock);

    return r;
}

int synth_lut_get(const bool tx, const uint8_t channel, const double freq,
                  synth_rec_t *rec) {
    int r;

    struct synth_lut_ctx *it = synth_lut_find(tx, channel);
    if (NULL == it) {
        PRINT(ERROR, "unable to find %s %c\n", tx ? "TX" : "RX", 'A' + channel);
        r = ENOENT;
        goto out;
    }

    r = it->get(it, freq, rec);

out:
    return r;
}

static int _synth_lut_init(struct synth_lut_ctx *ctx) {

    char req[] = "status -s";

    int r;

    // this array is used for both the command response and regular expression
    // errors
    char buf[256];
    int uart_fd;

    regex_t preg;
    const char *regex = "\\([0-9a-f]\\)\\+[\n>]*";
    regmatch_t pmatch[2];

    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&ctx->lock, &attr);

    if (0 != strlen(ctx->fn)) {
        // ctx->fn has already been initialized
        r = EXIT_SUCCESS;
        goto out;
    }

    r = regcomp(&preg, regex, 0);
    if (EXIT_SUCCESS != r) {
        regerror(r, &preg, buf, sizeof(buf));
        PRINT(ERROR, "Failed to compile regular expression '%s' (%d,%s)\n",
              regex, r, buf);
        r = EINVAL;
        goto out;
    }

    extern int* uart_tx_comm_fd;
    extern int* uart_rx_comm_fd;

    uart_fd = ctx->tx ? uart_tx_comm_fd[ctx->channel(ctx)] : uart_rx_comm_fd[ctx->channel(ctx)];

    memset(buf, '\0', sizeof(buf));
    r = synth_lut_uart_cmd(uart_fd, (char *)req, buf, sizeof(buf));
    if (EXIT_SUCCESS != r) {
        PRINT(ERROR, "Failed to issue command '%s' to %s %c (%d,%s)\n", req,
              ctx->tx ? "TX" : "RX", 'A' + ctx->channel(ctx), r, strerror(r));
        goto free_re;
    }

    r = regexec(&preg, buf, 2, pmatch, 0);
    if (EXIT_SUCCESS != r) {
        PRINT(ERROR,
              "Failed to match '%s' to regular expression '%s' for %s %c\n",
              buf, regex, ctx->tx ? "TX" : "RX", 'A' + ctx->channel(ctx));
        regerror(r, &preg, buf, sizeof(buf));
        PRINT(ERROR, "(%d,%s)\n", r, buf);
        r = EINVAL;
        goto free_re;
    }

    // truncate the string to the exact size of the matched regular expression
    buf[pmatch[1].rm_eo] = '\0';

#if defined(TATE)
    snprintf(ctx->fn, sizeof(ctx->fn),
             "/var/cyan/calibration-data/%s%c-%s.bin", ctx->tx ? "TX" : "RX",
             'A' + ctx->channel(ctx), buf);
#elif defined(TATE_4R4T)
    snprintf(ctx->fn, sizeof(ctx->fn),
             "/var/cyan/calibration-data/%s%c-%s.bin", ctx->tx ? "TX" : "RX",
             'A' + ctx->channel(ctx), buf);
#elif defined(TATE_8R)
    snprintf(ctx->fn, sizeof(ctx->fn),
             "/var/cyan/calibration-data/%s%c-%s.bin", ctx->tx ? "TX" : "RX",
             'A' + ctx->channel(ctx), buf);
#elif defined(VAUNT)
    snprintf(ctx->fn, sizeof(ctx->fn),
             "/var/crimson/calibration-data/%s%c-%s.bin", ctx->tx ? "TX" : "RX",
             'A' + ctx->channel(ctx), buf);
#else
    #error "This file must be compiled with a valid PRODUCT (TATE, TATE_4R4T, TATE_8R, VAUNT). Confirm spelling and spaces."
#endif
    r = EXIT_SUCCESS;

free_re:
    regfree(&preg);

out:
    return r;
}

int synth_lut_enable_all() {
    int r;

    struct synth_lut_ctx *it;

    FOR_EACH(it, synth_lut_rx_ctx) {
        r = it->enable(it);
        if (EXIT_SUCCESS != r) {
            goto out;
        }
    }

    FOR_EACH(it, synth_lut_tx_ctx) {
        r = it->enable(it);
        if (EXIT_SUCCESS != r) {
            goto out;
        }
    }

out:
    return r;
}

void synth_lut_disable_all() {
    struct synth_lut_ctx *it;

    FOR_EACH(it, synth_lut_rx_ctx) { it->disable(it); }

    FOR_EACH(it, synth_lut_tx_ctx) { it->disable(it); }
}

bool synth_lut_is_enabled(const bool tx, const uint32_t channel) {
    bool r;

    struct synth_lut_ctx *it = synth_lut_find(tx, channel);
    if (NULL == it) {
        PRINT(ERROR, "unable to find %s %c\n", tx ? "TX" : "RX", 'A' + channel);
        r = false;
        goto out;
    }

    r = it->enabled;

out:
    return r;
}

static int _synth_lut_is_calibrated(struct synth_lut_ctx *ctx, bool *is) {

    int r;

    struct stat st;

    pthread_mutex_lock(&ctx->lock);

    r = ctx->init(ctx);
    if (EXIT_SUCCESS != r) {
        PRINT(INFO, "Failed to initialize %s %c (%d,%s)\n",
              ctx->tx ? "TX" : "RX", 'A' + ctx->channel(ctx), r, strerror(r));
        goto out;
    }

    r = stat(ctx->fn, &st);
    if (EXIT_SUCCESS != r) {
        if (ENOENT == errno) {
            r = EXIT_SUCCESS;
            *is = false;
            goto out;
        }
        r = errno;
        PRINT(ERROR, "Failed to stat %s (%d,%s)\n", ctx->fn, errno,
              strerror(errno));
        goto out;
    }

    *is = true;

out:
    pthread_mutex_unlock(&ctx->lock);

    return r;
}

bool synth_lut_is_calibrated(const bool tx, const uint32_t channel) {
    bool r;
    int rr;
    struct synth_lut_ctx *it = synth_lut_find(tx, channel);
    if (NULL == it) {
        PRINT(ERROR, "unable to find %s %c\n", tx ? "TX" : "RX", 'A' + channel);
        r = false;
        goto out;
    }
    rr = it->is_calibrated(it, &r);
    if (EXIT_SUCCESS != rr) {
        PRINT(ERROR, "unable to check calibration for %s %c\n",
              tx ? "TX" : "RX", 'A' + channel);
        r = false;
        goto out;
    }

out:
    return r;
}

int synth_lut_enable_all_if_calibrated() {
    int r;

    struct synth_lut_ctx *it;
    bool is_calibrated;

    FOR_EACH(it, synth_lut_rx_ctx) {
        if (true &&
            EXIT_SUCCESS == (r = it->is_calibrated(it, &is_calibrated)) &&
            is_calibrated && EXIT_SUCCESS == (r = it->enable(it))) {
            PRINT(INFO, "Enabled synth lut for %s %c\n", it->tx ? "TX" : "RX",
                  it->id);
        }
        if (EXIT_SUCCESS != r) {
            PRINT(ERROR,
                  "Warning: Failed to check calibration or enable synth lut "
                  "for %s %c\n",
                  it->tx ? "TX" : "RX", it->id);
        }
    }

    FOR_EACH(it, synth_lut_tx_ctx) {
        if (true &&
            EXIT_SUCCESS == (r = it->is_calibrated(it, &is_calibrated)) &&
            is_calibrated && EXIT_SUCCESS == (r = it->enable(it))) {
            PRINT(INFO, "Enabled synth lut for %s %c\n", it->tx ? "TX" : "RX",
                  it->id);
        }
        if (EXIT_SUCCESS != r) {
            PRINT(ERROR,
                  "Warning: Failed to check calibration or enable synth lut "
                  "for %s %c\n",
                  it->tx ? "TX" : "RX", it->id);
        }
    }

    r = EXIT_SUCCESS;

    return r;
}

static int _synth_lut_set_freq(struct synth_lut_ctx *ctx, const double freq) {
    int r;

    pthread_mutex_lock(&ctx->lock);

    r = set_freq_internal(ctx->tx, ctx->channel(ctx), freq);

    pthread_mutex_unlock(&ctx->lock);

    return r;
}

static int _synth_lut_autocal_enable(struct synth_lut_ctx *ctx, const bool en) {
    int r;

    pthread_mutex_lock(&ctx->lock);

    char cmd_buf[PATH_MAX];
    char resp_buf[PATH_MAX];

    int uart_fd;

    uint32_t chan_i;

    extern int* uart_tx_comm_fd;
    extern int* uart_rx_comm_fd;

    chan_i = ctx->channel(ctx);
    uart_fd = ctx->tx ? uart_tx_comm_fd[chan_i] : uart_rx_comm_fd[chan_i];

    // tell the mcu to use autocal
    snprintf(cmd_buf, sizeof(cmd_buf), "rf -c %lc -A %c", 'a' + chan_i,
             en ? '1' : '0');
    r = synth_lut_uart_cmd(uart_fd, cmd_buf, resp_buf, sizeof(resp_buf));
    if (EXIT_SUCCESS != r) {
        PRINT(ERROR, "synth_lut_uart_cmd() failed (%d,%s)\n", r, strerror(r));
        goto out;
    }

out:

    pthread_mutex_unlock(&ctx->lock);

    return r;
}

static int _synth_lut_autocal_values(struct synth_lut_ctx *ctx,
                                     synth_rec_t *rec) {

    int r;

    char cmd_buf[PATH_MAX];
    char resp_buf[PATH_MAX];

    int core, band, bias;
    int uart_fd;

    bool tx = ctx->tx;
    size_t chan_i = ctx->channel(ctx);

    extern int* uart_tx_comm_fd;
    extern int* uart_rx_comm_fd;

    uart_fd = tx ? uart_tx_comm_fd[chan_i] : uart_rx_comm_fd[chan_i];

    // read the value back
    snprintf(cmd_buf, sizeof(cmd_buf), "rf -c %c -p", 'a' + chan_i);
    r = synth_lut_uart_cmd(uart_fd, cmd_buf, resp_buf, sizeof(resp_buf));
    if (EXIT_SUCCESS != r) {
        PRINT(ERROR, "synth_lut_uart_cmd() failed (%d,%s)\n", r, strerror(r));
        goto out;
    }

    if (3 != sscanf(resp_buf, "%u,%u,%u", &core, &band, &bias)) {
        PRINT(ERROR,
              "unable to parse response '%s', expecting number,number,number\n",
              resp_buf);
        r = EINVAL;
        goto out;
    }

    rec->core = core;
    rec->band = band;
    rec->bias = bias;

out:

    return r;
}
