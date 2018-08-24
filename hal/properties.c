//
// Copyright 2014 - 2018 Per Vices Corporation
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

#if 0 /* Removes headers for quick gcc -E diagnostics for XMACRO stuffs */
    #include "properties.h"

    #include "array-utils.h"
    #include "mmap.h"
    #include "property_manager.h"
    #include "synth_lut.h"

    #include <ctype.h>
    #include <stdbool.h>
    #include <stdio.h>
    #include <string.h>
#endif

/* clang-format off */

/* -------------------------------------------------------------------------- */
/* -------------------------- SOME CPP DEFINES ------------------------------ */
/* -------------------------------------------------------------------------- */

// Sample rates are in samples per second (SPS).
#define BASE_SAMPLE_RATE   325000000.0
#define RESAMP_SAMPLE_RATE 260000000.0

#define IPVER_IPV4 0
#define IPVER_IPV6 1

// (2 ^ 32) / (1 * 322265625)
#define DSP_NCO_CONST \
    ((double)13.215283987692307692307692307692307692307692307692307690000)

// (2 ^ 48) / (4 * 322265625)
#define DAC_NCO_CONST \
    ((double)216519.21285435076923076923076923076923076923076923076919296)

#define PWR_ON  1
#define PWR_OFF 0

#define STREAM_ON  1
#define STREAM_OFF 0

#define LEN(a) ((int) sizeof(a) / sizeof(*a))

/* -------------------------------------------------------------------------- */
/* -------------------------- SOME XMACRO TALK ------------------------------ */
/* -------------------------------------------------------------------------- */

// Property functions are writen once and expanded N times for however many
// channels specified. Channels are specified here. Channel operations will be
// done in the order of this specification.

#define VAUNT
#if defined(VAUNT)
    #define CHANNELS  \
        X(a) /*  0 */ \
        X(b) /*  1 */ \
        X(c) /*  2 */ \
        X(e) /*  3 */
#elif defined(TATE)
    #define CHANNELS  \
        X(a) /*  0 */ \
        X(b) /*  1 */ \
        X(c) /*  2 */ \
        X(d) /*  3 */ \
        X(e) /*  4 */ \
        X(f) /*  5 */ \
        X(g) /*  6 */ \
        X(h) /*  7 */ \
        X(i) /*  8 */ \
        X(j) /*  9 */ \
        X(k) /* 10 */ \
        X(l) /* 11 */ \
        X(m) /* 12 */ \
        X(n) /* 13 */ \
        X(o) /* 14 */ \
        X(p) /* 15 */
#else
    #error "Project name (VAUNT | TATE) not specified or not recognized."
#endif

// This channel specification is also known as an XMACRO:
// https://en.wikipedia.org/wiki/X_Macro.
// An XMACRO can expand a preprocessor defintion and do some very valuable
// work using the following three tools (STR, CHR, INT).
#define STR(ch) #ch
#define CHR(ch) #ch[0]
#define INT(ch) ((int)(CHR(ch) - 'a'))

// STR converts a channel letter to a compile time string
//    -> (assuming strings are "a", "b", "c" ... "z").
// CHR converts the string into a runtime char.
// INT converts the char into a runtime integer.
//
// Let us try building an array of channel names using the XMACRO.
static const char* const names[] = {
#define X(ch) STR(ch)
    CHANNELS
#undef X
};

// This expands nicely into an array of strings that looks something like
// this: { "a", "b", "c" ... }.
// The number of channels is simply the length of the this array.
#define NUM_CHANNELS LEN(names)

// And that's all there is to the XMACRO.
// The XMACRO will be used heavily later on to expand
// channel functions into N times over for as many channels as there are.
// The CHANNEL preprocessor list can be reconfigured for however many
// channels the server needs.

/* -------------------------------------------------------------------------- */
/* -------------------------- GLOBAL VARIABLES ------------------------------ */
/* -------------------------------------------------------------------------- */

// These file descriptors point to MCU devices.
// For VAUNT, the TX and RX will use only one file descriptor as four TX
// channels share one MCU and four RX channels share another MCU.
// For TATE, there is one TX file descriptor per channel, and one RX file
// descriptor per channel, as each channel uses its own MCU.
static int *uart_tx_fd = NULL;
static int *uart_rx_fd = NULL;

// A typical VAUNT file descriptor layout may look something like this:
// RX = { 0, 0, 0, 0 }
// TX = { 1, 1, 1, 1 }
// For TATE, as there sixteen channels, with each file descriptor pointing to a
// unique MCU:
// RX = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }
// TX = {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 }
// Note that RX is -1 here as TATE is very configurable with its backplane
// configuration setup. One client may want 16 TX and 0 RX lines, while
// another may want half and half:
// RX = { -1, -1, -1, -1, -1, -1, -1, -1,  8,  9, 10, 11, 12, 13, 14, 15 }
// TX = {  0,  1,  2,  3,  4,  5,  6,  7, -1, -1, -1, -1, -1, -1, -1, -1 }

// For VAUNT and TATE there is always one MCU for time control, and thus one
// file descriptor.
static int uart_synth_fd = 0;

static uint8_t uart_ret_buf[MAX_UART_RET_LEN] = { 0x00 };
static char buf[MAX_PROP_LEN] = { '\0' };

// Here we XMACRO expand again, but just with PWR_OFF so that the array
// looks like { PWR_OFF, PWR_OFF, PWR_OFF ... } for as ever many
// channels as there are.
static uint8_t rx_power[] = {
#define X(ch) PWR_OFF,
    CHANNELS
#undef X
};

static uint8_t tx_power[] = {
#define X(ch) PWR_OFF,
    CHANNELS
#undef X
};

static uint8_t rx_stream[] = {
#define X(ch) STREAM_OFF,
    CHANNELS
#undef X
};

// Here is another XMACRO expand, but this one looks like:
// {
//   "rxa4", "rxb4", "rxc4", "rxd4" ...
//   "txa4", "txb4", "txc4", "txd4" ...
// }
// for however many channels there are.
static const char *reg4[] = {
#define X(ch) "rx"STR(ch)"4",
    CHANNELS
#undef X
#define X(ch) "tx"STR(ch)"4",
    CHANNELS
#undef X
};

// More XMACRO fun (you're starting to get the idea now) but this array
// is just an array of 17's: { 17, 17, 17 ... 17 } for however many
// channels there are.
static int i_bias[] = {
#define X(ch) 17,
    CHANNELS
#undef X
};

static int q_bias[] = {
#define X(ch) 17,
    CHANNELS
#undef X
};

// The server can be saved and loaded from disk. These pointers will
// help with that.
uint8_t *_save_profile;
uint8_t *_load_profile;
char *_save_profile_path;
char *_load_profile_path;

static const uint8_t ipver[] = {
    IPVER_IPV4,
    IPVER_IPV4,
};

/* clang-format on */

// Also known as strchr (maybe we should replace this someday).
static int contains(const char *str, char letter, int size) {
    int i = 0, cnt = 0;
    for (i = 0; i < size; i++) {
        if (str[i] == letter)
            cnt++;
    }
    return cnt;
}

static int read_uart(int uartfd) {
    char buf[MAX_UART_LEN] = {};
    memset(buf, 0, MAX_UART_LEN);

    uint16_t total_bytes = 0, cur_bytes = 0;

    while (contains(buf, '>', total_bytes) < 1) {
        if (recv_uart_comm(uartfd, ((uint8_t *)buf) + total_bytes, &cur_bytes,
                           MAX_UART_LEN - total_bytes)) {
            return 0;
        }
        total_bytes += cur_bytes;
    }

    printf("%s\n", buf);
    strncpy((char *)uart_ret_buf, buf, MAX_UART_RET_LEN - 1);
    return RETURN_SUCCESS;
}

// Finds the optimal value for the sample rate blocks
static uint16_t get_optimal_sr_factor(double rate, double base_rate,
                                      double *err) {
    double max_factor = 65536; // 2^16
    double min_factor = 1;
    double lower_factor_violation = 0;
    double upper_factor_violation = 0;

    double lower_factor = (double)floor(base_rate / rate);
    double upper_factor = lower_factor + 1;

    // Error bounds check
    if (lower_factor > max_factor || lower_factor < min_factor)
        lower_factor_violation = 1;
    if (upper_factor > max_factor || upper_factor < min_factor)
        upper_factor_violation = 1;

    double lower_factor_error = (base_rate / (double)lower_factor) - rate;
    double upper_factor_error = rate - (base_rate / (double)upper_factor);

    if (lower_factor_violation && upper_factor_violation) {
        return 0xffff;
    } else if (lower_factor_violation) {
        if (err)
            *err = upper_factor_error;
        return (uint16_t)(upper_factor - 1);
    } else if (upper_factor_violation) {
        if (err)
            *err = lower_factor_error;
        return (uint16_t)(lower_factor - 1);
    } else { // Nothing is wrong, then
        if (lower_factor_error < upper_factor_error) {
            if (err)
                *err = lower_factor_error;
            return (uint16_t)(lower_factor - 1);
        } else {
            if (err)
                *err = upper_factor_error;
            return (uint16_t)(upper_factor - 1);
        }
    }
}

// XXX
// Statement Expressions are bad... but this code will be replaced soon anyway.
#define set_reg_bits(name, shift, mask, val)                                   \
    ({                                                                         \
        int _r;                                                                \
        uint32_t _t;                                                           \
        _r = read_hps_reg(name, &_t);                                          \
        if (RETURN_SUCCESS != _r) {                                            \
            PRINT(ERROR, "read_hps_reg( '%s' ) failed: %d\n", name, _r);       \
        } else {                                                               \
            _t &= ~(mask << shift);                                            \
            _t |= val << shift;                                                \
            _r = write_hps_reg(name, _t);                                      \
            if (RETURN_SUCCESS != _r) {                                        \
                PRINT(ERROR, "write_hps_reg( '%s' ) failed: %d\n", name, _r);  \
            }                                                                  \
        }                                                                      \
        _r;                                                                    \
    })

/* -------------------------------------------------------------------------- */
/* -------------------------------- MISC ------------------------------------ */
/* -------------------------------------------------------------------------- */

static int hdlr_invalid(const char *data, char *ret) {
    PRINT(ERROR, "Cannot invoke a set on this property\n");
    return RETURN_ERROR_SET_PROP;
}

static int hdlr_rx_sync(const char *data, char *ret) {
    uint32_t old_val;

    // toggle the bit sys0[5]
    read_hps_reg("sys0", &old_val);
    write_hps_reg("sys0", old_val | 0x20);
    write_hps_reg("sys0", old_val & (~0x20));

    return RETURN_SUCCESS;
}

static int hdlr_tx_sync(const char *data, char *ret) {
    uint32_t old_val;

    // toggle the bit sys0[6]
    read_hps_reg("sys0", &old_val);
    write_hps_reg("sys0", old_val | 0x40);
    write_hps_reg("sys0", old_val & (~0x40));

    return RETURN_SUCCESS;
}

static int hdlr_save_config(const char *data, char *ret) {
    *_save_profile = 1;
    strcpy(_save_profile_path, data);
    return RETURN_SUCCESS;
}

static int hdlr_load_config(const char *data, char *ret) {
    *_load_profile = 1;
    strcpy(_load_profile_path, data);
    return RETURN_SUCCESS;
}

static int set_sma_dir(bool in) {
    return set_reg_bits("sys2", 4, 1, in);
}

static int set_sma_pol(bool positive) {
    return set_reg_bits("sys2", 6, 1, positive);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------- LUTS ------------------------------------ */
/* -------------------------------------------------------------------------- */

static int hdlr_XX_X_rf_freq_lut_en(const char *data, char *ret, const bool tx,
                                    const size_t channel) {
    int r = RETURN_SUCCESS;

    bool en = '0' != data[0];

    if (en) {
        r = synth_lut_enable(tx, channel);
        if (EXIT_SUCCESS != r) {
            sprintf(ret, "%c", '0');
        }
    } else {
        synth_lut_disable(tx, channel);
    }

    return r;
}

// And here we start with the REAL Xmacro work.
// As you can see, each function name has a channel character placed
// where ##ch## is found. Within the function, something else changes too,
// like here, where the function argument for 'channel' is specified with the
// INT preprocessor helper.
#define X(ch)                                                                  \
    static int hdlr_rx_##ch##_rf_freq_lut_en(const char *data, char *ret) {    \
        return hdlr_XX_X_rf_freq_lut_en(data, ret, false, INT(ch));            \
    }                                                                          \
    static int hdlr_tx_##ch##_rf_freq_lut_en(const char *data, char *ret) {    \
        return hdlr_XX_X_rf_freq_lut_en(data, ret, true, INT(ch));             \
    }
CHANNELS
#undef X
// This will be the theme for the rest of this property file.

/* -------------------------------------------------------------------------- */
/* -------------------------------- GATE ------------------------------------ */
/* -------------------------------------------------------------------------- */

static int set_gating_mode(const char *chan, bool dsp) {
    char reg_name[8];
    snprintf(reg_name, sizeof(reg_name), "tx%s6", chan);
    return set_reg_bits(reg_name, 12, 1, dsp);
}

static int valid_trigger_mode(const char *data, bool *edge) {

    if (false) {
    } else if (0 == strncmp("edge", data, strlen("edge"))) {
        *edge = true;
    } else if (0 == strncmp("level", data, strlen("level"))) {
        *edge = false;
    } else {
        return RETURN_ERROR_PARAM;
    }

    return RETURN_SUCCESS;
}

static int valid_trigger_pol(const char *data, bool *positive) {

    if (false) {
    } else if (0 == strncmp("positive", data, strlen("positive"))) {
        *positive = true;
    } else if (0 == strncmp("negative", data, strlen("negative"))) {
        *positive = false;
    } else {
        PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)");
        return RETURN_ERROR_PARAM;
    }
    return RETURN_SUCCESS;
}

static int valid_trigger_sel(const char *data, uint32_t *sel) {
    int r;

    r = sscanf(data, "%" PRIu32, sel);
    if (1 != r || *sel >= 4) {
        PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)");
        return RETURN_ERROR_PARAM;
    }

    return RETURN_SUCCESS;
}

static int valid_trigger_dir(const char *data, bool *in) {
    if (false) {
    } else if (0 == strncmp("in", data, strlen("in"))) {
        *in = true;
    } else if (0 == strncmp("out", data, strlen("out"))) {
        *in = false;
    } else {
        PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)");
        return RETURN_ERROR_PARAM;
    }
    return RETURN_SUCCESS;
}

static int set_edge_backoff(bool tx, const char *chan, uint32_t backoff) {
    char regname[8];
    snprintf(regname, sizeof(regname), "%s%s%u", tx ? "tx" : "rx", chan,
             tx ? 9 : 12);
    return set_reg_bits(regname, 0, -1, backoff);
}

static int set_edge_sample_num(bool tx, const char *chan, uint64_t num) {
    int r;
    uint32_t val_msw;
    uint32_t val_lsw;

    char regname_msw[8];
    char regname_lsw[8];

    snprintf(regname_msw, sizeof(regname_msw), "%s%s%u", tx ? "tx" : "rx", chan,
             tx ? 7 : 10);
    snprintf(regname_lsw, sizeof(regname_lsw), "%s%s%u", tx ? "tx" : "rx", chan,
             tx ? 8 : 11);

    val_msw = num >> 32;
    val_lsw = num & 0xffffffff;

    return set_reg_bits(regname_msw, 0, -1, val_msw) ||
           set_reg_bits(regname_lsw, 0, -1, val_lsw);
}

static int set_trigger_ufl_dir(bool tx, const char *chan, bool in) {
    char reg_name[8];
    snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan,
             tx ? 6 : 9);
    return set_reg_bits(reg_name, 9, 1, in);
}

static int set_trigger_sel(bool tx, const char *chan, uint32_t sel) {
    char reg_name[8];
    snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan,
             tx ? 6 : 9);
    return set_reg_bits(reg_name, 10, 0b11, sel);
}

static int set_trigger_mode(bool sma, bool tx, const char *chan, bool edge) {
    unsigned shift;
    char reg_name[8];
    snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan,
             tx ? 6 : 9);
    shift = sma ? 0 : 4;
    return set_reg_bits(reg_name, shift, 1, edge);
}

static int set_trigger_ufl_pol(bool tx, const char *chan, bool positive) {
    char reg_name[8];
    snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan,
             tx ? 6 : 9);
    return set_reg_bits(reg_name, 8, 1, positive);
}

static int valid_edge_backoff(const char *data, uint32_t *val) {
    if (1 == sscanf(data, "%" PRIu32, val)) {
        return RETURN_SUCCESS;
    } else {
        PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)");
        return RETURN_ERROR_PARAM;
    }
}

static int valid_edge_sample_num(const char *data, uint64_t *val) {
    if (1 == sscanf(data, "%" PRIu64, val)) {
        return RETURN_SUCCESS;
    } else {
        PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)");
        return RETURN_ERROR_PARAM;
    }
}

static int valid_gating_mode(const char *data, bool *dsp) {
    if (false) {
    } else if (0 == strncmp("dsp", data, strlen("dsp"))) {
        *dsp = true;
    } else if (0 == strncmp("output", data, strlen("output"))) {
        *dsp = false;
    } else {
        return RETURN_ERROR_PARAM;
    }
    return RETURN_SUCCESS;
}

#define X(ch)                                                                  \
    static int hdlr_tx_##ch##_trigger_sma_mode(const char *data, char *ret) {  \
        int r;                                                                 \
        bool val;                                                              \
        r = valid_trigger_mode(data, &val) ||                                  \
            set_trigger_mode(true, true, #ch, val);                            \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_edge_backoff(const char *data,           \
                                                   char *ret) {                \
        uint32_t val;                                                          \
        int r;                                                                 \
        r = valid_edge_backoff(data, &val) ||                                  \
            set_edge_backoff(true, #ch, val);                                  \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_edge_sample_num(const char *data,        \
                                                      char *ret) {             \
        uint64_t val;                                                          \
        int r;                                                                 \
        r = valid_edge_sample_num(data, &val) ||                               \
            set_edge_sample_num(true, #ch, val);                               \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_trig_sel(const char *data, char *ret) {  \
        uint32_t val;                                                          \
        int r;                                                                 \
        r = valid_trigger_sel(data, &val) || set_trigger_sel(true, #ch, val);  \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_ufl_dir(const char *data, char *ret) {   \
        int r;                                                                 \
        bool val;                                                              \
        r = valid_trigger_dir(data, &val) ||                                   \
            set_trigger_ufl_dir(true, #ch, val);                               \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_ufl_mode(const char *data, char *ret) {  \
        int r;                                                                 \
        bool val;                                                              \
        r = valid_trigger_mode(data, &val) ||                                  \
            set_trigger_mode(false, true, #ch, val);                           \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_ufl_pol(const char *data, char *ret) {   \
        int r;                                                                 \
        bool val;                                                              \
        r = valid_trigger_pol(data, &val) ||                                   \
            set_trigger_ufl_pol(true, #ch, val);                               \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_sma_mode(const char *data, char *ret) {  \
        int r;                                                                 \
        bool val;                                                              \
        r = valid_trigger_mode(data, &val) ||                                  \
            set_trigger_mode(true, false, #ch, val);                           \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_edge_backoff(const char *data,           \
                                                   char *ret) {                \
        uint32_t val;                                                          \
        int r;                                                                 \
        r = valid_edge_backoff(data, &val) ||                                  \
            set_edge_backoff(false, #ch, val);                                 \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_edge_sample_num(const char *data,        \
                                                      char *ret) {             \
        uint64_t val;                                                          \
        int r;                                                                 \
        r = valid_edge_sample_num(data, &val) ||                               \
            set_edge_sample_num(false, #ch, val);                              \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_trig_sel(const char *data, char *ret) {  \
        uint32_t val;                                                          \
        int r;                                                                 \
        r = valid_trigger_sel(data, &val) || set_trigger_sel(false, #ch, val); \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_ufl_dir(const char *data, char *ret) {   \
        int r;                                                                 \
        bool val;                                                              \
        r = valid_trigger_dir(data, &val) ||                                   \
            set_trigger_ufl_dir(false, #ch, val);                              \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_ufl_mode(const char *data, char *ret) {  \
        int r;                                                                 \
        bool val;                                                              \
        r = valid_trigger_mode(data, &val) ||                                  \
            set_trigger_mode(false, false, #ch, val);                          \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_ufl_pol(const char *data, char *ret) {   \
        int r;                                                                 \
        bool val;                                                              \
        r = valid_trigger_pol(data, &val) ||                                   \
            set_trigger_ufl_pol(false, #ch, val);                              \
        return r;                                                              \
    }
CHANNELS
#undef X

/* -------------------------------------------------------------------------- */
/* --------------------------------- TX ------------------------------------- */
/* -------------------------------------------------------------------------- */

#define X(ch)                                                                  \
    static int hdlr_tx_##ch##_rf_dac_dither_en(const char *data, char *ret) {  \
        int r;                                                                 \
        int en;                                                                \
                                                                               \
        r = sscanf(data, "%d", &en);                                           \
        if (1 != r) {                                                          \
            return RETURN_ERROR;                                               \
        }                                                                      \
        if (en < 0 || en > 1) {                                                \
            return RETURN_ERROR_PARAM;                                         \
        }                                                                      \
        snprintf(buf, sizeof(buf), "dac -c " STR(ch) " -l %u\r", en);          \
        sprintf(ret, "%u", en);                                                \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_dac_dither_mixer_en(const char *data,         \
                                                     char *ret) {              \
        int r;                                                                 \
        int en;                                                                \
                                                                               \
        r = sscanf(data, "%d", &en);                                           \
        if (1 != r) {                                                          \
            return RETURN_ERROR;                                               \
        }                                                                      \
        if (en < 0 || en > 1) {                                                \
            return RETURN_ERROR_PARAM;                                         \
        }                                                                      \
        snprintf(buf, sizeof(buf), "dac -c " STR(ch) " -3 %u\r", en);          \
        sprintf(ret, "%u", en);                                                \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_dac_dither_sra_sel(const char *data,          \
                                                    char *ret) {               \
        int r;                                                                 \
        int db;                                                                \
        int sel;                                                               \
                                                                               \
        r = sscanf(data, "%d", &db);                                           \
        if (1 != r) {                                                          \
            return RETURN_ERROR;                                               \
        }                                                                      \
        if (db < 6 || db > 96 || 0 != db % 6) {                                \
            return RETURN_ERROR_PARAM;                                         \
        }                                                                      \
                                                                               \
        sprintf(ret, "%u", db);                                                \
                                                                               \
        /* 96 dB (max) := 0x0, 90 dB := 0x1, ... 6 dB (min) := 0xf */          \
        sel = 16 - (db / 6);                                                   \
                                                                               \
        snprintf(buf, sizeof(buf), "dac -c " STR(ch) " -b %u\r", sel);         \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_dac_nco(const char *data, char *ret) {        \
        double freq;                                                           \
        sscanf(data, "%lf", &freq);                                            \
        uint64_t nco_steps = (uint64_t)round(freq * DAC_NCO_CONST);            \
        sprintf(ret, "%lf", (double)nco_steps / DAC_NCO_CONST);                \
                                                                               \
        strcpy(buf, "dac -c " STR(ch) " -e 0 -n ");                            \
        sprintf(buf + strlen(buf), "%" PRIu32 "",                              \
                (uint32_t)(nco_steps >> 32));                                  \
        strcat(buf, "\r");                                                     \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
                                                                               \
        strcpy(buf, "dac -o ");                                                \
        sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);        \
        strcat(buf, "\r");                                                     \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_dac_temp(const char *data, char *ret) {       \
        strcpy(buf, "board -c " STR(ch) " -t\r");                              \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_tx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_freq_val(const char *data, char *ret) {       \
        uint64_t freq = 0;                                                     \
        sscanf(data, "%" SCNd64 "", &freq);                                    \
                                                                               \
        /* if freq = 0, mute PLL */                                            \
        if (freq == 0) {                                                       \
            strcpy(buf, "rf -c " STR(ch) " -z\r");                             \
            send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));  \
                                                                               \
            return RETURN_SUCCESS;                                             \
        }                                                                      \
                                                                               \
        /* if freq out of bounds, kill channel*/                               \
        if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {        \
            strcpy(buf, "board -c " STR(ch) " -k\r");                          \
            send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));  \
                                                                               \
            /* Turn OFF TX on HPS */                                           \
            uint32_t old_val;                                                  \
                                                                               \
            /* disable DSP cores */                                            \
            read_hps_reg("tx" STR(ch) "4", &old_val);                          \
            PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__,                   \
                  toupper(CHR(ch)));                                           \
            write_hps_reg("tx" STR(ch) "4", old_val | 0x2);                    \
                                                                               \
            /* disable channel */                                              \
            read_hps_reg("tx" STR(ch) "4", &old_val);                          \
            write_hps_reg("tx" STR(ch) "4", old_val &(~0x100));                \
                                                                               \
            tx_power[INT(ch)] = PWR_OFF;                                       \
                                                                               \
            PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "       \
                         "Shutting Down TX" STR(ch) ".\n");                    \
                                                                               \
            return RETURN_ERROR;                                               \
        }                                                                      \
                                                                               \
        /* run the pll calc algorithm */                                       \
        pllparam_t pll;                                                        \
        long double outfreq = 0;                                               \
        outfreq = setFreq(&freq, &pll);                                        \
                                                                               \
        strcpy(buf, "rf -c " STR(ch) " \r");                                   \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
                                                                               \
        /* TODO: pll1.power setting TBD (need to modify pllparam_t) */         \
                                                                               \
        /* Send Parameters over to the MCU */                                  \
        set_pll_frequency(uart_tx_fd[INT(ch)], (uint64_t)PLL_CORE_REF_FREQ_HZ, \
                          &pll, true, INT(ch));                                \
                                                                               \
        sprintf(ret, "%lf", outfreq);                                          \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_freq_band(const char *data, char *ret) {      \
        strcpy(buf, "rf -c " STR(ch) " -b ");                                  \
        strcat(buf, data);                                                     \
        strcat(buf, "\r");                                                     \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_freq_i_bias(const char *data, char *ret) {    \
        sscanf(data, "%i", &(i_bias[INT(ch)]));                                \
        strcpy(buf, "rf -c " STR(ch) " -i ");                                  \
        sprintf(buf + strlen(buf), "%i", i_bias[INT(ch)]);                     \
        strcat(buf, " -q ");                                                   \
        sprintf(buf + strlen(buf), "%i", q_bias[INT(ch)]);                     \
        strcat(buf, " -m\r");                                                  \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_freq_q_bias(const char *data, char *ret) {    \
        sscanf(data, "%i", &(q_bias[INT(ch)]));                                \
        strcpy(buf, "rf -c " STR(ch) " -i ");                                  \
        sprintf(buf + strlen(buf), "%i", i_bias[INT(ch)]);                     \
        strcat(buf, " -q ");                                                   \
        sprintf(buf + strlen(buf), "%i", q_bias[INT(ch)]);                     \
        strcat(buf, " -m\r");                                                  \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_gain_val(const char *data, char *ret) {       \
        int gain;                                                              \
        sscanf(data, "%i", &gain);                                             \
                                                                               \
        /*   0 -> 126 attenuation only */                                      \
        /* 127    0dB */                                                       \
                                                                               \
        if (gain > 127)                                                        \
            gain = 127;                                                        \
        else if (gain < 0)                                                     \
            gain = 0;                                                          \
                                                                               \
        strcpy(buf, "rf -c " STR(ch) " -a ");                                  \
        sprintf(buf + strlen(buf), "%i", 127 - gain);                          \
        strcat(buf, "\r");                                                     \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_board_dump(const char *data, char *ret) {     \
        /* send the uart commands and read back the output and write to file   \
         */                                                                    \
                                                                               \
        /* DAC */                                                              \
        strcpy(buf, "dump -c " STR(ch) " -d\r");                               \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_tx_fd[INT(ch)]);                                        \
        PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);             \
                                                                               \
        /* GPIOX */                                                            \
        strcpy(buf, "dump -c " STR(ch) " -g\r");                               \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_tx_fd[INT(ch)]);                                        \
        PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);           \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_board_test(const char *data, char *ret) {     \
        /* TODO: MCU code cleanup */                                           \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_board_temp(const char *data, char *ret) {     \
        strcpy(buf, "board -c " STR(ch) " -t\r");                              \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_tx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_status_rfld(const char *data, char *ret) {       \
        strcpy(buf, "status -c " STR(ch) " -l\r");                             \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_tx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_status_dacld(const char *data, char *ret) {      \
        strcpy(buf, "status -c " STR(ch) " -p\r");                             \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_tx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_status_dacctr(const char *data, char *ret) {     \
        strcpy(buf, "status -c " STR(ch) " -e\r");                             \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_tx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_board_led(const char *data, char *ret) {      \
        strcpy(buf, "board -l\r");                                             \
        strcat(buf, data);                                                     \
        strcat(buf, "\r");                                                     \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_gain(const char *data, char *ret) {          \
        /* TODO: FW code */                                                    \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_rate(const char *data, char *ret) {          \
        uint32_t old_val;                                                      \
        uint16_t base_factor, resamp_factor;                                   \
        double base_err, resamp_err;                                           \
        double rate;                                                           \
        sscanf(data, "%lf", &rate);                                            \
                                                                               \
        /* get the error for base rate */                                      \
        base_factor =                                                          \
            get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);          \
        resamp_factor =                                                        \
            get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);      \
                                                                               \
        /* set the appropriate sample rate */                                  \
        memset(ret, 0, MAX_PROP_LEN);                                          \
                                                                               \
        if (resamp_err < base_err) {                                           \
            write_hps_reg("tx" STR(ch) "1", resamp_factor);                    \
            read_hps_reg("tx" STR(ch) "4", &old_val);                          \
            write_hps_reg("tx" STR(ch) "4", old_val | (1 << 15));              \
            sprintf(ret, "%lf",                                                \
                    RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));         \
            /* Set gain adjustment */                                          \
            read_hps_reg("txg" STR(ch), &old_val);                             \
            write_hps_reg("txg" STR(ch),                                       \
                          (old_val & ~(0xff << 0)) |                           \
                              (interp_gain_lut[(resamp_factor)] << 0));        \
        } else {                                                               \
            write_hps_reg("tx" STR(ch) "1", base_factor);                      \
            read_hps_reg("tx" STR(ch) "4", &old_val);                          \
            write_hps_reg("tx" STR(ch) "4", old_val & ~(1 << 15));             \
            sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1)); \
            /* Set gain adjustment */                                          \
            read_hps_reg("txg" STR(ch), &old_val);                             \
            write_hps_reg("txg" STR(ch),                                       \
                          (old_val & ~(0xff << 0)) |                           \
                              (interp_gain_lut[(base_factor)] << 0));          \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_nco_adj(const char *data, char *ret) {       \
        double freq;                                                           \
        uint32_t old_val;                                                      \
        uint8_t direction;                                                     \
                                                                               \
        /* check for a minus or plus sign at the front */                      \
        if (data[0] == '-') {                                                  \
            sscanf(data + 1, "%lf", &freq);                                    \
            direction = 1;                                                     \
        } else if (data[0] == '+') {                                           \
            sscanf(data + 1, "%lf", &freq);                                    \
            direction = 0;                                                     \
        } else {                                                               \
            sscanf(data, "%lf", &freq);                                        \
            direction = 0;                                                     \
        }                                                                      \
                                                                               \
        /* write NCO adj */                                                    \
        uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);            \
        write_hps_reg("tx" STR(ch) "0", nco_steps);                            \
        if (direction > 0) {                                                   \
            sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);           \
        } else {                                                               \
            sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);            \
        }                                                                      \
                                                                               \
        /* write direction */                                                  \
        read_hps_reg("tx" STR(ch) "4", &old_val);                              \
        write_hps_reg("tx" STR(ch) "4",                                        \
                      (old_val & ~(0x1 << 13)) | (direction << 13));           \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_rstreq(const char *data, char *ret) {        \
        uint32_t old_val;                                                      \
        read_hps_reg("tx" STR(ch) "4", &old_val);                              \
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper(CHR(ch)));    \
        write_hps_reg("tx" STR(ch) "4", old_val | 0x2);                        \
        write_hps_reg("tx" STR(ch) "4", old_val & ~0x2);                       \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_id(const char *data, char *ret) {          \
        /* don't need to do anything, save the ID in the file system */        \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_vita_en(const char *data, char *ret) {      \
        uint32_t old_val;                                                      \
        read_hps_reg("tx" STR(ch) "4", &old_val);                              \
        if (strcmp(data, "1") == 0)                                            \
            write_hps_reg("tx" STR(ch) "4", old_val | (1 << 14));              \
        else                                                                   \
            write_hps_reg("tx" STR(ch) "4", old_val & ~(1 << 14));             \
                                                                               \
        /* sync_channels( 15 ); */                                             \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_iface(const char *data, char *ret) {        \
        /* TODO: FW support for streaming to management port required */       \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_port(const char *data, char *ret) {         \
        uint32_t port;                                                         \
        sscanf(data, "%" SCNd32 "", &port);                                    \
        write_hps_reg("tx" STR(ch) "5", port);                                 \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * r04 uses different offsets for channels starting at index 4? */         \
    static int hdlr_tx_##ch##_qa_fifo_lvl(const char *data, char *ret) {       \
        uint32_t lvl;                                                          \
        read_hps_reg("res_ro4", &lvl);                                         \
        lvl &= 0xffff;                                                         \
        sprintf(ret, "%u", lvl);                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
       DOES NOT PORT WELL.                                                     \
       flc14 uses different offsets for chanenls starting at index 14? */      \
    static int hdlr_tx_##ch##_qa_oflow(const char *data, char *ret) {          \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        read_hps_reg("flc14", &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * flc6 uses different offsets for channels starting at index 6? */        \
    static int hdlr_tx_##ch##_qa_uflow(const char *data, char *ret) {          \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        read_hps_reg("flc6", &count);                                          \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_pwr(const char *data, char *ret) {               \
        uint32_t old_val;                                                      \
        uint8_t power;                                                         \
        uint8_t i;                                                             \
        sscanf(data, "%" SCNd8 "", &power);                                    \
                                                                               \
        /* check if power is already enabled */                                \
        if (power >= PWR_ON && tx_power[INT(ch)] == PWR_ON)                    \
            return RETURN_SUCCESS;                                             \
                                                                               \
        /* power on */                                                         \
        if (power >= PWR_ON) {                                                 \
            tx_power[INT(ch)] = PWR_ON;                                        \
                                                                               \
            /* board commands */                                               \
            strcpy(buf, "board -c " STR(ch) " -d\r");                          \
            send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));  \
            usleep(200000);                                                    \
                                                                               \
            /* disable dsp channels */                                         \
            for (i = 0; i < (NUM_CHANNELS * 2); i++) {                         \
                read_hps_reg(reg4[i], &old_val);                               \
                write_hps_reg(reg4[i], old_val & ~0x100);                      \
            }                                                                  \
                                                                               \
            /* send sync pulse */                                              \
            sync_channels(15);                                                 \
                                                                               \
            /* enable active dsp channels, and reset the DSP */                \
            for (i = 0; i < NUM_CHANNELS; i++) {                               \
                if (tx_power[i] == PWR_ON) {                                   \
                    read_hps_reg(reg4[i + 4], &old_val);                       \
                    write_hps_reg(reg4[i + 4], old_val | 0x100);               \
                    read_hps_reg(reg4[i + 4], &old_val);                       \
                    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__,           \
                          toupper(CHR(ch)));                                   \
                    write_hps_reg(reg4[i + 4], old_val | 0x2);                 \
                    write_hps_reg(reg4[i + 4], old_val &(~0x2));               \
                }                                                              \
                if (rx_power[i] == PWR_ON) {                                   \
                    read_hps_reg(reg4[i], &old_val);                           \
                    write_hps_reg(reg4[i], old_val | 0x100);                   \
                    read_hps_reg(reg4[i], &old_val);                           \
                    write_hps_reg(reg4[i], old_val | 0x2);                     \
                    write_hps_reg(reg4[i], old_val &(~0x2));                   \
                }                                                              \
            }                                                                  \
                                                                               \
            /* power off */                                                    \
        } else {                                                               \
            /* kill the channel */                                             \
            strcpy(buf, "board -c " STR(ch) " -k\r");                          \
            send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));  \
                                                                               \
            /* disable DSP cores */                                            \
            read_hps_reg("tx" STR(ch) "4", &old_val);                          \
            PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__,                   \
                  toupper(CHR(ch)));                                           \
            write_hps_reg("tx" STR(ch) "4", old_val | 0x2);                    \
                                                                               \
            /* disable channel */                                              \
            read_hps_reg("tx" STR(ch) "4", &old_val);                          \
            write_hps_reg("tx" STR(ch) "4", old_val &(~0x100));                \
                                                                               \
            tx_power[INT(ch)] = PWR_OFF;                                       \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_serial(const char *data, char *ret) {      \
        strcpy(buf, "status -s\r");                                            \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_tx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_mcudevid(const char *data, char *ret) {    \
        strcpy(buf, "status -d\r");                                            \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_tx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_mcurev(const char *data, char *ret) {      \
        strcpy(buf, "status -v\r");                                            \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_tx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_mcufuses(const char *data, char *ret) {    \
        strcpy(buf, "status -f\r");                                            \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_tx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_fw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -v\r");                                             \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_tx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }
CHANNELS
#undef X

/* -------------------------------------------------------------------------- */
/* --------------------------------- RX ------------------------------------- */
/* -------------------------------------------------------------------------- */

#define X(ch)                                                                  \
    static int hdlr_rx_##ch##_rf_freq_val(const char *data, char *ret) {       \
        uint64_t freq = 0;                                                     \
        sscanf(data, "%" SCNd64 "", &freq);                                    \
                                                                               \
        /* if freq = 0, mute PLL */                                            \
        if (freq == 0) {                                                       \
            strcpy(buf, "rf -c " STR(ch) " -z\r");                             \
            send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));  \
                                                                               \
            return RETURN_SUCCESS;                                             \
        }                                                                      \
                                                                               \
        /* if freq out of bounds, kill channel */                              \
        if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {        \
            strcpy(buf, "board -c " STR(ch) " -k\r");                          \
            send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));  \
                                                                               \
            /* Turn OFF RX on HPS */                                           \
            uint32_t old_val;                                                  \
                                                                               \
            /* disable DSP core */                                             \
            read_hps_reg("rx" STR(ch) "4", &old_val);                          \
            write_hps_reg("rx" STR(ch) "4", old_val | 0x2);                    \
                                                                               \
            /* disable channel */                                              \
            read_hps_reg("rx" STR(ch) "4", &old_val);                          \
            write_hps_reg("rx" STR(ch) "4", old_val &(~0x100));                \
                                                                               \
            rx_power[INT(ch)] = PWR_OFF;                                       \
            rx_stream[INT(ch)] = STREAM_OFF;                                   \
                                                                               \
            PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "       \
                         "Shutting Down RX" STR(ch) ".\n");                    \
                                                                               \
            return RETURN_ERROR;                                               \
        }                                                                      \
                                                                               \
        /* run the pll calc algorithm */                                       \
        pllparam_t pll;                                                        \
        long double outfreq = 0;                                               \
        outfreq = setFreq(&freq, &pll);                                        \
                                                                               \
        strcpy(buf, "rf -c " STR(ch) " \r");                                   \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
                                                                               \
        /* TODO: pll1.power setting TBD (need to modify pllparam_t) */         \
                                                                               \
        /* Send Parameters over to the MCU */                                  \
        set_pll_frequency(uart_rx_fd[INT(ch)], (uint64_t)PLL_CORE_REF_FREQ_HZ, \
                          &pll, false, INT(ch));                               \
                                                                               \
        sprintf(ret, "%lf", outfreq);                                          \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_freq_lna(const char *data, char *ret) {       \
        strcpy(buf, "rf -c " STR(ch) " -l ");                                  \
        strcat(buf, data);                                                     \
        strcat(buf, "\r");                                                     \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_freq_band(const char *data, char *ret) {      \
        strcpy(buf, "rf -c " STR(ch) " -b ");                                  \
        strcat(buf, data);                                                     \
        strcat(buf, "\r");                                                     \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_gain_val(const char *data, char *ret) {       \
        int gain;                                                              \
        sscanf(data, "%i", &gain);                                             \
                                                                               \
        if (gain > 126)                                                        \
            gain = 126;                                                        \
        else if (gain < 0)                                                     \
            gain = 0;                                                          \
                                                                               \
        if (gain % 2)                                                          \
            gain++; /* Odd Number */                                           \
                                                                               \
        /* 0 -> 126 gain */                                                    \
        strcpy(buf, "vga -c " STR(ch) " -g ");                                 \
        sprintf(buf + strlen(buf), "%i", gain >> 1);                           \
        strcat(buf, "\r");                                                     \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_atten_val(const char *data, char *ret) {      \
        int atten;                                                             \
        sscanf(data, "%i", &atten);                                            \
                                                                               \
        if (atten > 127)                                                       \
            atten = 127;                                                       \
        else if (atten < 0)                                                    \
            atten = 0;                                                         \
                                                                               \
        strcpy(buf, "rf -c " STR(ch) " -a ");                                  \
        sprintf(buf + strlen(buf), "%i", atten);                               \
        strcat(buf, "\r");                                                     \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_board_dump(const char *data, char *ret) {     \
        /* send the uart commands and read back the output and write to file   \
         */                                                                    \
                                                                               \
        /* ADC */                                                              \
        strcpy(buf, "dump -c " STR(ch) " -a\r");                               \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_rx_fd[INT(ch)]);                                        \
        PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);             \
                                                                               \
        /* GPIOX */                                                            \
        strcpy(buf, "dump -c " STR(ch) " -g\r");                               \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_rx_fd[INT(ch)]);                                        \
        PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);           \
                                                                               \
        /* ADC Driver */                                                       \
        strcpy(buf, "dump -c " STR(ch) " -v\r");                               \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_rx_fd[INT(ch)]);                                        \
        PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_board_test(const char *data, char *ret) {     \
        /* TODO: MCU code cleanup */                                           \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_board_temp(const char *data, char *ret) {     \
        strcpy(buf, "board -c " STR(ch) " -t\r");                              \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_rx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_status_rfld(const char *data, char *ret) {       \
        strcpy(buf, "status -c " STR(ch) " -l\r");                             \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_rx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_status_adcalarm(const char *data, char *ret) {   \
        strcpy(buf, "status -c " STR(ch) " -a\r");                             \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_rx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_board_led(const char *data, char *ret) {      \
        strcpy(buf, "board -l\r");                                             \
        strcat(buf, data);                                                     \
        strcat(buf, "\r");                                                     \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_signed(const char *data, char *ret) {        \
        uint32_t old_val, sign;                                                \
        sscanf(data, "%u", &sign);                                             \
        sign = sign ? 0 : 1;                                                   \
                                                                               \
        read_hps_reg("rx" STR(ch) "4", &old_val);                              \
        old_val &= ~(1 << 4);                                                  \
        write_hps_reg("rx" STR(ch) "4", old_val | (sign << 4));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_gain(const char *data, char *ret) {          \
        /* TODO: FW code */                                                    \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_rate(const char *data, char *ret) {          \
        uint32_t old_val;                                                      \
        uint16_t base_factor, resamp_factor;                                   \
        double base_err, resamp_err;                                           \
        double rate;                                                           \
        sscanf(data, "%lf", &rate);                                            \
                                                                               \
        /* get the error for base rate */                                      \
        base_factor =                                                          \
            get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);          \
        resamp_factor =                                                        \
            get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);      \
                                                                               \
        /* set the appropriate sample rate */                                  \
        memset(ret, 0, MAX_PROP_LEN);                                          \
        int gain_factor;                                                       \
                                                                               \
        if (resamp_err < base_err) {                                           \
            write_hps_reg("rx" STR(ch) "1", resamp_factor);                    \
            read_hps_reg("rx" STR(ch) "4", &old_val);                          \
            write_hps_reg("rx" STR(ch) "4", old_val | (1 << 15));              \
            sprintf(ret, "%lf",                                                \
                    RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));         \
            /*Set gain adjustment */                                           \
            gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;       \
            read_hps_reg("rxg" STR(ch), &old_val);                             \
            write_hps_reg("rxg" STR(ch), (old_val & ~(0xff << 0)) |            \
                                             (((uint16_t)gain_factor) << 0));  \
        } else {                                                               \
            write_hps_reg("rx" STR(ch) "1", base_factor);                      \
            read_hps_reg("rx" STR(ch) "4", &old_val);                          \
            write_hps_reg("rx" STR(ch) "4", old_val & ~(1 << 15));             \
            sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1)); \
            /*Set gain adjustment*/                                            \
            gain_factor = decim_gain_lut[(base_factor)];                       \
            read_hps_reg("rxg" STR(ch), &old_val);                             \
            write_hps_reg("rxg" STR(ch), (old_val & ~(0xff << 0)) |            \
                                             (((uint16_t)gain_factor) << 0));  \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_nco_adj(const char *data, char *ret) {       \
        double freq;                                                           \
        uint32_t old_val;                                                      \
        uint8_t direction;                                                     \
                                                                               \
        /* check for a minus or plus sign at the front */                      \
        if (data[0] == '-') {                                                  \
            sscanf(data + 1, "%lf", &freq);                                    \
            direction = 1;                                                     \
        } else if (data[0] == '+') {                                           \
            sscanf(data + 1, "%lf", &freq);                                    \
            direction = 0;                                                     \
        } else {                                                               \
            sscanf(data, "%lf", &freq);                                        \
            direction = 0;                                                     \
        }                                                                      \
                                                                               \
        /* write NCO adj */                                                    \
        uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);            \
        write_hps_reg("rx" STR(ch) "0", nco_steps);                            \
        if (direction > 0) {                                                   \
            sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);           \
        } else {                                                               \
            sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);            \
        }                                                                      \
                                                                               \
        /* write direction */                                                  \
        read_hps_reg("rx" STR(ch) "4", &old_val);                              \
        write_hps_reg("rx" STR(ch) "4",                                        \
                      (old_val & ~(0x1 << 13)) | (direction << 13));           \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_rstreq(const char *data, char *ret) {        \
        uint32_t old_val;                                                      \
        read_hps_reg("rx" STR(ch) "4", &old_val);                              \
        write_hps_reg("rx" STR(ch) "4", old_val | 0x2);                        \
        write_hps_reg("rx" STR(ch) "4", old_val & ~0x2);                       \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_loopback(const char *data, char *ret) {      \
        uint32_t old_val;                                                      \
        read_hps_reg("rx" STR(ch) "4", &old_val);                              \
        if (strcmp(data, "1") == 0)                                            \
            write_hps_reg("rx" STR(ch) "4", (old_val & ~0x1e00) | 0x400);      \
        else                                                                   \
            write_hps_reg("rx" STR(ch) "4", (old_val & ~0x1e00) | 0x000);      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_id(const char *data, char *ret) {          \
        /* don't need to do anything, save the ID in the file system */        \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_link_vita_en(const char *data, char *ret) {      \
        uint32_t old_val;                                                      \
        read_hps_reg("rx" STR(ch) "4", &old_val);                              \
        if (strcmp(data, "1") == 0)                                            \
            write_hps_reg("rx" STR(ch) "4", old_val | (1 << 14));              \
        else                                                                   \
            write_hps_reg("rx" STR(ch) "4", old_val & ~(1 << 14));             \
                                                                               \
        /*sync_channels( 15 ); */                                              \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_link_iface(const char *data, char *ret) {        \
        /* TODO: FW support for streaming to management port required */       \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_link_port(const char *data, char *ret) {         \
        uint32_t port;                                                         \
        sscanf(data, "%" SCNd32 "", &port);                                    \
        write_hps_reg("rx" STR(ch) "8", port);                                 \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_link_ip_dest(const char *data, char *ret) {      \
        uint8_t ip[4];                                                         \
        sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip,        \
               ip + 1, ip + 2, ip + 3);                                        \
        write_hps_reg("rx" STR(ch) "5",                                        \
                      (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3])); \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_link_mac_dest(const char *data, char *ret) {     \
        uint8_t mac[6];                                                        \
        sscanf(data,                                                           \
               "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8           \
               ":%" SCNx8 "",                                                  \
               mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);              \
        write_hps_reg("rx" STR(ch) "6", (mac[0] << 8) | (mac[1]));             \
        write_hps_reg("rx" STR(ch) "7", (mac[2] << 24) | (mac[3] << 16) |      \
                                            (mac[4] << 8) | mac[5]);           \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_stream(const char *data, char *ret) {            \
        uint32_t old_val;                                                      \
        uint8_t stream;                                                        \
        sscanf(data, "%" SCNd8 "", &stream);                                   \
                                                                               \
        /* if stream > 1, check the status of the stream */                    \
        if (stream > 1) {                                                      \
            sprintf(ret, "%u", rx_stream[INT(ch)]); /* Alert File Tree */      \
            return RETURN_SUCCESS;                                             \
        }                                                                      \
                                                                               \
        /* Stream is already ON or OFF then return */                          \
        if (stream == rx_stream[INT(ch)])                                      \
            return RETURN_SUCCESS;                                             \
                                                                               \
        /* Otherwise make the change accordingly */                            \
        if (stream > 0) { /* TURN THE STREAM ON */                             \
            if (rx_power[INT(ch)] == PWR_ON) {                                 \
                read_hps_reg(reg4[INT(ch)], &old_val);                         \
                write_hps_reg(reg4[INT(ch)], old_val | 0x100);                 \
                                                                               \
                read_hps_reg(reg4[INT(ch)], &old_val);                         \
                write_hps_reg(reg4[INT(ch)], old_val | 0x2);                   \
                write_hps_reg(reg4[INT(ch)], old_val &(~0x2));                 \
                                                                               \
                rx_stream[INT(ch)] = STREAM_ON;                                \
            } else {                                                           \
                /* Do not turn ON stream if channel is OFF */                  \
                sprintf(ret, "%u", 0); /* Alert File Tree */                   \
            }                                                                  \
        } else { /* TURN THE STREAM OFF */                                     \
            /* disable DSP core */                                             \
            read_hps_reg("rx" STR(ch) "4", &old_val);                          \
            write_hps_reg("rx" STR(ch) "4", old_val | 0x2);                    \
                                                                               \
            /* disable channel */                                              \
            read_hps_reg("rx" STR(ch) "4", &old_val);                          \
            write_hps_reg("rx" STR(ch) "4", old_val &(~0x100));                \
                                                                               \
            rx_stream[INT(ch)] = STREAM_OFF;                                   \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_pwr(const char *data, char *ret) {               \
        uint32_t old_val;                                                      \
        uint8_t power;                                                         \
        uint8_t i;                                                             \
        sscanf(data, "%" SCNd8 "", &power);                                    \
                                                                               \
        /* check if power is already enabled */                                \
        if (power >= PWR_ON && rx_power[INT(ch)] == PWR_ON)                    \
            return RETURN_SUCCESS;                                             \
                                                                               \
        /* power on */                                                         \
        if (power >= PWR_ON) {                                                 \
            rx_power[INT(ch)] = PWR_ON;                                        \
                                                                               \
            /* board command */                                                \
            strcpy(buf, "board -c " STR(ch) " -d\r");                          \
            send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));  \
            usleep(200000);                                                    \
                                                                               \
            /* disable dsp channels */                                         \
            for (i = 0; i < (NUM_CHANNELS * 2); i++) {                         \
                read_hps_reg(reg4[i], &old_val);                               \
                write_hps_reg(reg4[i], old_val & ~0x100);                      \
            }                                                                  \
                                                                               \
            /* send sync pulse */                                              \
            sync_channels(15);                                                 \
                                                                               \
            /* Enable active dsp channels, and reset DSP */                    \
            for (i = 0; i < NUM_CHANNELS; i++) {                               \
                if (tx_power[i] == PWR_ON) {                                   \
                    read_hps_reg(reg4[i + 4], &old_val);                       \
                    write_hps_reg(reg4[i + 4], old_val | 0x100);               \
                    read_hps_reg(reg4[i + 4], &old_val);                       \
                    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__,           \
                          toupper(CHR(ch)));                                   \
                    write_hps_reg(reg4[i + 4], old_val | 0x2);                 \
                    write_hps_reg(reg4[i + 4], old_val &(~0x2));               \
                }                                                              \
                if (rx_stream[i] == STREAM_ON) {                               \
                    read_hps_reg(reg4[i], &old_val);                           \
                    write_hps_reg(reg4[i], old_val | 0x100);                   \
                    read_hps_reg(reg4[i], &old_val);                           \
                    write_hps_reg(reg4[i], old_val | 0x2);                     \
                    write_hps_reg(reg4[i], old_val &(~0x2));                   \
                }                                                              \
            }                                                                  \
                                                                               \
            /* power off & stream off */                                       \
        } else {                                                               \
            rx_power[INT(ch)] = PWR_OFF;                                       \
            rx_stream[INT(ch)] = STREAM_OFF;                                   \
                                                                               \
            /* kill the channel */                                             \
            strcpy(buf, "board -c " STR(ch) " -k\r");                          \
            send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));  \
                                                                               \
            /* disable DSP core */                                             \
            read_hps_reg("rx" STR(ch) "4", &old_val);                          \
            write_hps_reg("rx" STR(ch) "4", old_val | 0x2);                    \
                                                                               \
            /* disable channel */                                              \
            read_hps_reg("rx" STR(ch) "4", &old_val);                          \
            write_hps_reg("rx" STR(ch) "4", old_val &(~0x100));                \
        }                                                                      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_serial(const char *data, char *ret) {      \
        strcpy(buf, "status -s\r");                                            \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_rx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_mcudevid(const char *data, char *ret) {    \
        strcpy(buf, "status -d\r");                                            \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_rx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_mcurev(const char *data, char *ret) {      \
        strcpy(buf, "status -v\r");                                            \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_rx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_mcufuses(const char *data, char *ret) {    \
        strcpy(buf, "status -f\r");                                            \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_rx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_fw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -v\r");                                             \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        read_uart(uart_rx_fd[INT(ch)]);                                        \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }
CHANNELS
#undef X

#define X(ch)                                                                  \
    static int hdlr_tx_##ch##_trigger_gating(const char *data, char *ret) {    \
        int r;                                                                 \
        bool val;                                                              \
        r = valid_gating_mode(data, &val) || set_gating_mode(#ch, val);        \
        return r;                                                              \
    }
CHANNELS
#undef X

/* -------------------------------------------------------------------------- */
/* --------------------------------- CM ------------------------------------- */
/* -------------------------------------------------------------------------- */

static uint16_t cm_chanmask_get(const char *path) {
    uint32_t r;

    FILE *fp;

    r = 0;

    if (!(fp = fopen(path, "r"))) {
        PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
        return r;
    }
    fscanf(fp, "%x", &r);
    fclose(fp);

    return r;
}

static int hdlr_cm_chanmask_rx(const char *data, char *ret) {
    uint32_t mask;

    if (1 != sscanf(data, "%x", &mask)) {
        return RETURN_ERROR_PARAM;
    }

    mask &= 0xffff;
    sprintf(ret, "%x", mask);

    return RETURN_SUCCESS;
}

static int hdlr_cm_chanmask_tx(const char *data, char *ret) {
    uint32_t mask;

    if (1 != sscanf(data, "%x", &mask)) {
        return RETURN_ERROR_PARAM;
    }

    mask &= 0xffff;
    sprintf(ret, "%x", mask);

    return RETURN_SUCCESS;
}

static int hdlr_cm_rx_atten_val(const char *data, char *ret) {
    int r;

    char inbuf[256];
    char outbuf[256];

    uint32_t mask_rx;

    int wd_backup;
    prop_t *prop;
    int (*hdlr)(const char *, char *) = NULL;
    int i;

    int atten = 0;

    mask_rx = cm_chanmask_get("/var/crimson/state/cm/chanmask-rx");

    sscanf(data, "%lf", &atten);

    sprintf(inbuf, "%lf", atten);

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }
#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_rx_##ch##_rf_atten_val;
        CHANNELS
#undef X

        // call the handler directly
        r = hdlr(inbuf, outbuf);
        if (RETURN_SUCCESS != r) {
            return r;
        }

        // disable inotify, write the value back to the file, re-enable inotify
        prop = get_prop_from_hdlr(hdlr);
        wd_backup = prop->wd;
        prop->wd = -1;
        set_property(prop->path, inbuf);
        prop->wd = wd_backup;
    }

    return RETURN_SUCCESS;
}

static int hdlr_cm_rx_gain_val(const char *data, char *ret) {
    int r;

    char inbuf[256];
    char outbuf[256];

    uint32_t mask_rx;

    int wd_backup;
    prop_t *prop;
    int (*hdlr)(const char *, char *) = NULL;
    int i;

    double gain = 0;

    mask_rx = cm_chanmask_get("/var/crimson/state/cm/chanmask-rx");

    sscanf(data, "%lf", &gain);

    sprintf(inbuf, "%lf", gain);

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }

#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_rx_##ch##_rf_gain_val;
        CHANNELS
#undef X

        // call the handler directly
        r = hdlr(inbuf, outbuf);
        if (RETURN_SUCCESS != r) {
            return r;
        }

        // disable inotify, write the value back to the file, re-enable inotify
        prop = get_prop_from_hdlr(hdlr);
        wd_backup = prop->wd;
        prop->wd = -1;
        set_property(prop->path, inbuf);
        prop->wd = wd_backup;
    }

    return RETURN_SUCCESS;
}

static int hdlr_cm_tx_gain_val(const char *data, char *ret) {
    int r;

    char inbuf[256];
    char outbuf[256];

    uint32_t mask_tx;

    int wd_backup;
    prop_t *prop;
    int (*hdlr)(const char *, char *) = NULL;
    int i;

    double gain = 0;

    mask_tx = cm_chanmask_get("/var/crimson/state/cm/chanmask-tx");

    sscanf(data, "%lf", &gain);

    sprintf(inbuf, "%lf", gain);

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_tx & (1 << i))) {
            continue;
        }

#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_tx_##ch##_rf_gain_val;
        CHANNELS
#undef X

        // call the handler directly
        r = hdlr(inbuf, outbuf);
        if (RETURN_SUCCESS != r) {
            return r;
        }

        // disable inotify, write the value back to the file, re-enable inotify
        prop = get_prop_from_hdlr(hdlr);
        wd_backup = prop->wd;
        prop->wd = -1;
        set_property(prop->path, inbuf);
        prop->wd = wd_backup;
    }

    return RETURN_SUCCESS;
}

static int hdlr_cm_trx_freq_val(const char *data, char *ret) {
    int r;

    char inbuf[256];
    char outbuf[256];

    uint32_t sync_mode;
    uint32_t sync_mask;

    uint32_t mask_rx;
    uint32_t mask_tx;

    int wd_backup;
    prop_t *prop;
    int (*hdlr)(const char *, char *) = NULL;
    int i;

    double freq = 0;

    read_hps_reg("sync_mode", &sync_mode);
    read_hps_reg("sync_mask", &sync_mask);

    mask_rx = cm_chanmask_get("/var/crimson/state/cm/chanmask-rx");
    mask_tx = cm_chanmask_get("/var/crimson/state/cm/chanmask-tx");

    sync_mask = (mask_tx << 16) | mask_rx;
    if (0 == sync_mask) {
        return RETURN_SUCCESS;
    }

    sscanf(data, "%lf", &freq);

    sync_mode |= 1; // internal sync
    write_hps_reg("sync_mode", sync_mode);

    write_hps_reg("sync_mask", sync_mask);

    sprintf(inbuf, "%lf", freq);

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }

#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_rx_##ch##_rf_gain_val;
        CHANNELS
#undef X

        // call the handler directly
        r = hdlr(inbuf, outbuf);
        if (RETURN_SUCCESS != r) {
            return r;
        }

        // disable inotify, write the value back to the file, re-enable inotify
        prop = get_prop_from_hdlr(hdlr);
        wd_backup = prop->wd;
        prop->wd = -1;
        set_property(prop->path, inbuf);
        prop->wd = wd_backup;
    }

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_tx & (1 << i))) {
            continue;
        }

#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_tx_##ch##_rf_freq_val;
        CHANNELS
#undef X

        // call the handler directly
        r = hdlr(inbuf, outbuf);
        if (RETURN_SUCCESS != r) {
            return r;
        }

        // disable inotify, write the value back to the file, re-enable inotify
        prop = get_prop_from_hdlr(hdlr);
        wd_backup = prop->wd;
        prop->wd = -1;
        set_property(prop->path, inbuf);
        prop->wd = wd_backup;
    }

    return RETURN_SUCCESS;
}

static int hdlr_cm_trx_nco_adj(const char *data, char *ret) {
    int r;

    char inbuf[256];
    char outbuf[256];

    uint32_t sync_mode;
    uint32_t sync_mask;

    uint32_t mask_rx;
    uint32_t mask_tx;

    int wd_backup;
    prop_t *prop;
    int (*hdlr)(const char *, char *) = NULL;
    int i;

    double freq = 0;

    read_hps_reg("sync_mode", &sync_mode);
    read_hps_reg("sync_mask", &sync_mask);

    mask_rx = cm_chanmask_get("/var/crimson/state/cm/chanmask-rx");
    mask_tx = cm_chanmask_get("/var/crimson/state/cm/chanmask-tx");

    sync_mask = (mask_tx << 16) | mask_rx;
    if (0 == sync_mask) {
        return RETURN_SUCCESS;
    }

    sscanf(data, "%lf", &freq);

    sync_mode |= 1; // internal sync
    write_hps_reg("sync_mode", sync_mode);

    write_hps_reg("sync_mask", sync_mask);

    sprintf(inbuf, "%lf", freq);

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }

#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_rx_##ch##_dsp_nco_adj;
        CHANNELS
#undef X

        // call the handler directly
        r = hdlr(inbuf, outbuf);
        if (RETURN_SUCCESS != r) {
            return r;
        }

        // disable inotify, write the value back to the file, re-enable inotify
        prop = get_prop_from_hdlr(hdlr);
        wd_backup = prop->wd;
        prop->wd = -1;
        set_property(prop->path, inbuf);
        prop->wd = wd_backup;
    }

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_tx & (1 << i))) {
            continue;
        }
#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_tx_##ch##_dsp_nco_adj;
        CHANNELS
#undef X

        // call the handler directly
        r = hdlr(inbuf, outbuf);
        if (RETURN_SUCCESS != r) {
            return r;
        }

        // disable inotify, write the value back to the file, re-enable inotify
        prop = get_prop_from_hdlr(hdlr);
        wd_backup = prop->wd;
        prop->wd = -1;
        set_property(prop->path, inbuf);
        prop->wd = wd_backup;
    }

    return RETURN_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/* --------------------------------- TIME ----------------------------------- */
/* -------------------------------------------------------------------------- */

static int hdlr_time_clk_pps(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_time_clk_cur_time(const char *data, char *ret) {
    long double time;
    sscanf(data, "%Lf", &time);
    write_hps_reg("sys9", (uint32_t)(((uint64_t)time) & 0x00000000FFFFFFFF));
    write_hps_reg("sys10",
                  (uint32_t)(((uint64_t)time) >> 32) & 0x00000000FFFFFFFF);

    write_hps_reg("sys11",
                  (uint32_t)(time - (uint64_t)time) & 0x00000000FFFFFFFF);
    write_hps_reg("sys13", 1);
    write_hps_reg("sys13", 0);
    return RETURN_SUCCESS;
}

static int hdlr_time_source_vco(const char *data, char *ret) {
    if (strcmp(data, "external") == 0) {
        strcpy(buf, "clk -v 1\r");
    } else if (strcmp(data, "internal") == 0) {
        strcpy(buf, "clk -v 0\r");
    }
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_time_source_sync(const char *data, char *ret) {
    if (strcmp(data, "external") == 0) {
        strcpy(buf, "clk -n 1\r");
    } else if (strcmp(data, "internal") == 0) {
        strcpy(buf, "clk -n 0\r");
    }
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

// 10 MHz clock
static int hdlr_time_source_ref(const char *data, char *ret) {
    if (strcmp(data, "external") == 0) {
        strcpy(buf, "clk -t 1\r");
    } else if (strcmp(data, "internal") == 0) {
        strcpy(buf, "clk -t 0\r");
    }
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

// External Source Buffer Select
static int hdlr_time_source_extsine(const char *data, char *ret) {
    if (strcmp(data, "sine") == 0) {
        strcpy(buf, "HMC -h 1 -b 1\r");
        send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    } else if (strcmp(data, "LVPECL") == 0) {
        strcpy(buf, "HMC -h 1 -b 0\r");
        send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    } else {
        strcpy(buf, "HMC -h 1 -B\r");
        send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        read_uart(uart_synth_fd);
        strcpy(ret, (char *)uart_ret_buf);
    }
    return RETURN_SUCCESS;
}

// Toggle SPI Sync
static int hdlr_time_sync_lmk_sync_tgl_jesd(const char *data, char *ret) {
    if (strcmp(data, "0") != 0) {
        strcpy(buf, "sync -k\r");
    }
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

// Toggle SPI Sync
static int hdlr_time_sync_lmk_sync_tgl_pll(const char *data, char *ret) {
    if (strcmp(data, "0") != 0) {
        strcpy(buf, "sync -q\r");
    }
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

// Resync output edges with Ref
static int hdlr_time_sync_lmk_resync_jesd(const char *data, char *ret) {
    if (strcmp(data, "0") != 0) {
        strcpy(buf, "sync -j\r");
    }
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

// Resync output edges with Ref
static int hdlr_time_sync_lmk_resync_pll(const char *data, char *ret) {
    if (strcmp(data, "0") != 0) {
        strcpy(buf, "sync -p\r");
    }
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

// Resync output edges with Ref
static int hdlr_time_sync_lmk_resync_all(const char *data, char *ret) {
    if (strcmp(data, "0") != 0) {
        strcpy(buf, "sync -r\r");
    }
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

// TODO: Enable DevClock Output
static int hdlr_time_source_devclk(const char *data, char *ret) {
    if (strcmp(data, "external") == 0) {
        strcpy(buf, "clk -t 1\r");
    } else if (strcmp(data, "internal") == 0) {
        strcpy(buf, "clk -t 0\r");
    }
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

// TODO: Enable PLL Output
static int hdlr_time_source_pll(const char *data, char *ret) {
    if (strcmp(data, "external") == 0) {
        strcpy(buf, "clk -t 1\r");
    } else if (strcmp(data, "internal") == 0) {
        strcpy(buf, "clk -t 0\r");
    }
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld(const char *data, char *ret) {
    strcpy(buf, "status -l\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd_pll1(const char *data, char *ret) {
    strcpy(buf, "status -l 11\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd_pll2(const char *data, char *ret) {
    strcpy(buf, "status -l 12\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_pll_pll1(const char *data, char *ret) {
    strcpy(buf, "status -l 21\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_pll_pll2(const char *data, char *ret) {
    strcpy(buf, "status -l 22\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol(const char *data, char *ret) {
    strcpy(buf, "status -o\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd_pll1(const char *data, char *ret) {
    strcpy(buf, "status -o 11\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd_pll2(const char *data, char *ret) {
    strcpy(buf, "status -o 12\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_pll_pll1(const char *data, char *ret) {
    strcpy(buf, "status -o 21\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_pll_pll2(const char *data, char *ret) {
    strcpy(buf, "status -o 22\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_board_dump(const char *data, char *ret) {
    // Send the uart commands and read back the output and write to file.
    // Diagnostic Dump of Clk Board
    strcpy(buf, "board -e\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    PRINT(DUMP, "[Board: Time Regdump] %s\n", uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_board_test(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_time_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -t\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_board_led(const char *data, char *ret) {
    strcpy(buf, "board -l ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_time_about_id(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_time_about_serial(const char *data, char *ret) {
    strcpy(buf, "status -s\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_mcudevid(const char *data, char *ret) {
    strcpy(buf, "status -d\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_mcurev(const char *data, char *ret) {
    strcpy(buf, "status -v\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_mcufuses(const char *data, char *ret) {
    strcpy(buf, "status -f\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_fw_ver(const char *data, char *ret) {
    strcpy(buf, "board -v\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/* --------------------------------- FPGA ----------------------------------- */
/* -------------------------------------------------------------------------- */

// Dumps all of the board logs for TX, RX, and TIME
static int hdlr_fpga_board_dump(const char *data, char *ret) {
#define X(ch) hdlr_tx_##ch##_rf_board_dump(NULL, NULL);
    CHANNELS
#undef X
#define X(ch) hdlr_rx_##ch##_rf_board_dump(NULL, NULL);
    CHANNELS
#undef X
    hdlr_time_board_dump(NULL, NULL);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_test(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gle(const char *data, char *ret) {

    if (strcmp(data, "1") == 0) {
        strcpy(buf, "board -g 1\r");
        send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        usleep(50000);

        strcpy(buf, "board -g 1\r");
#define X(ch)                                                                  \
    send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf)),          \
        usleep(50000);
        CHANNELS
#undef X

        strcpy(buf, "board -g 1\r");
#define X(ch)                                                                  \
    send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf)),          \
        usleep(50000);
        CHANNELS
#undef X
    }
    if (strcmp(data, "2") == 0) {
        strcpy(buf, "board -g 2\r");
        send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        usleep(50000);

        strcpy(buf, "board -g 2\r");
#define X(ch)                                                                  \
    send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf)),          \
        usleep(50000);
        CHANNELS
#undef X

        strcpy(buf, "board -g 2\r");
#define X(xh)                                                                  \
    send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf)),          \
        usleep(50000);
        CHANNELS
#undef X
    }
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_temp(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("sys14", &old_val);

    // Mask off temp
    old_val = old_val & 0xff;

    if (old_val >= 128) {
        old_val = old_val - 128;
        sprintf(ret, "temp +%lu degC\n", old_val);
    } else if (old_val < 128) {
        old_val = old_val - 58;
        sprintf(ret, "temp -%lu degC\n", old_val);
    }

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_led(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_rstreq(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_reboot(const char *data, char *ret) {
    if (strcmp(data, "1") == 0) {
        uint32_t reboot;

        // Write 0 to bit[16] of sys 0 in order to reboot
        read_hps_reg("sys0", &reboot);
        reboot = (reboot & 0xFFFEFFFF);
        write_hps_reg("sys0", reboot);
        return RETURN_SUCCESS;
    }
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_jesd_sync(const char *data, char *ret) {
    sync_channels(15);
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_sys_rstreq(const char *data, char *ret) {
    strcpy(buf, "board -r\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    usleep(700000);

    strcpy(buf, "board -r\r");
#define X(ch)                                                                  \
    send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf)),          \
        usleep(50000);
    CHANNELS
#undef X

    strcpy(buf, "board -r\r");
#define X(ch)                                                                  \
    send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf)),          \
        usleep(50000);
    CHANNELS
#undef X

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_flow_control_sfpX_port(const char *data, char *ret,
                                                  unsigned sfp_port) {

    static const unsigned udp_port_max = (1 << 16) - 1;
    static const unsigned sfp_port_max = 1;

    unsigned udp_port;
    uint32_t flc0_reg;
    uint32_t mask;

    if (sfp_port > sfp_port_max) {
        return RETURN_ERROR_PARAM;
    }
    if (1 != sscanf(data, "%u", &udp_port)) {
        return RETURN_ERROR_PARAM;
    }

    udp_port = udp_port > udp_port_max ? udp_port_max : udp_port;

    // if number of sfp_ports ever changes, this code needs to be changed
    // a good reason to use structures to access memory-mapped registers.
    read_hps_reg("flc0", &flc0_reg);
    mask = 0xffff << (sfp_port * 16);
    flc0_reg &= ~mask;
    flc0_reg |= (udp_port << (sfp_port * 16)) & mask;
    write_hps_reg("flc0", flc0_reg);

    sprintf(ret, "%u", udp_port);

    return RETURN_SUCCESS;
}
static inline int hdlr_fpga_board_flow_control_sfpa_port(const char *data,
                                                         char *ret) {
    return hdlr_fpga_board_flow_control_sfpX_port(data, ret, 0);
}
static inline int hdlr_fpga_board_flow_control_sfpb_port(const char *data,
                                                         char *ret) {
    return hdlr_fpga_board_flow_control_sfpX_port(data, ret, 1);
}

static int hdlr_fpga_board_fw_rst(const char *data, char *ret) {
    uint32_t old_val;

    read_hps_reg("sys0", &old_val);
    write_hps_reg("sys0", old_val | 0x10);
    write_hps_reg("sys0", old_val & (~0x10));

    return RETURN_SUCCESS;
}

static int hdlr_fpga_about_id(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_fpga_about_cmp_time(const char *data, char *ret) {
    uint32_t old_val;
    int year, month, day, hour, min;
    read_hps_reg("sys15", &old_val);

    // Get year
    year = (old_val & 0xfff00000) >> 20;
    month = (old_val & 0x000f0000) >> 16;
    day = (old_val & 0x0000f800) >> 11;
    hour = (old_val & 0x000007c0) >> 6;
    min = old_val & 0x0000003f;

    sprintf(ret, "cmp. time %i-%i-%i %i:%i (yyyy-MM-dd HH:mm) \n", year, month,
            day, hour, min);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_about_conf_info(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("sys18", &old_val);
    sprintf(ret, "config. info. 0x%02x \n", old_val);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_about_serial(const char *data, char *ret) {
    uint64_t old_val;
    uint32_t old_val1;
    uint32_t old_val2;
    read_hps_reg("sys16", &old_val1);
    read_hps_reg("sys17", &old_val2);

    // Append values
    old_val = ((uint64_t)old_val2 << 32) | (uint64_t)old_val1;
    sprintf(ret, "serial number 0x%02x%02x \n", old_val2, old_val1);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_trigger_sma_dir(const char *data, char *ret) {
    int r;
    bool val;
    r = valid_trigger_dir(data, &val) || set_sma_dir(val);

    return r;
}

static int hdlr_fpga_trigger_sma_pol(const char *data, char *ret) {
    int r;
    bool val;
    r = valid_trigger_pol(data, &val) || set_sma_pol(val);

    return r;
}

static int hdlr_fpga_about_fw_ver(const char *data, char *ret) {
    uint64_t old_val;
    uint32_t old_val1;
    uint32_t old_val2;
    read_hps_reg("sys3", &old_val2);
    read_hps_reg("sys4", &old_val1);

    old_val2 = old_val2 & 0xff;

    // Append values
    old_val = ((uint64_t)old_val2 << 32) | (uint64_t)old_val1;

    sprintf(ret, "ver. 0x%02x%02x \n", old_val2, old_val1);
    return RETURN_SUCCESS;
}

static int hdlr_server_about_fw_ver(const char *data, char *ret) {
    FILE *fp = NULL;
    char buf[MAX_PROP_LEN] = {0};
    if ((fp = popen("/usr/bin/server -v", "r")) == NULL) {
        PRINT(ERROR, "Error opening pipe!\n");
        return RETURN_ERROR;
    }
    while (fgets(buf, MAX_PROP_LEN, fp) != NULL) {
        strncat(ret, buf, MAX_PROP_LEN);
    }
    if (pclose(fp)) {
        return RETURN_ERROR;
    }
    return RETURN_SUCCESS;
}

static int hdlr_fpga_about_hw_ver(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("sys1", &old_val);

    old_val = (old_val >> 7) & 0xf;

    sprintf(ret, "ver. 0x%02x", old_val);
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_rate(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpa_ip_addr(const char *data, char *ret) {
    uint32_t ip[4];
    if (ipver[0] == IPVER_IPV4) {
        sscanf(data, "%" SCNd32 ".%" SCNd32 ".%" SCNd32 ".%" SCNd32 "", ip,
               ip + 1, ip + 2, ip + 3);
        write_hps_reg("net5",
                      (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
    } else if (ipver[0] == IPVER_IPV6) {
        sscanf(data, "%" SCNx32 ":%" SCNx32 ":%" SCNx32 ":%" SCNx32 "", ip,
               ip + 1, ip + 2, ip + 3);
        write_hps_reg("net1", ip[0]);
        write_hps_reg("net2", ip[1]);
        write_hps_reg("net3", ip[2]);
        write_hps_reg("net4", ip[3]);
    }
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpa_mac_addr(const char *data, char *ret) {
    uint8_t mac[6];
    sscanf(data,
           "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
           mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
    write_hps_reg("net11", (mac[0] << 8) | (mac[1]));
    write_hps_reg("net12",
                  (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpa_ver(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t ver;
    sscanf(data, "%" SCNd8 "", &ver);
    read_hps_reg("net0", &old_val);
    if (ver > 0)
        write_hps_reg("net0", (old_val | 0x4));
    else
        write_hps_reg("net0", (old_val & ~(0x4)));
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpa_pay_len(const char *data, char *ret) {
    uint32_t old_val;
    uint32_t pay_len;
    sscanf(data, "%" SCNd32 "", &pay_len);
    read_hps_reg("net0", &old_val);
    write_hps_reg("net0", (old_val & ~(0xffff0000)) | (pay_len << 16));
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpb_ip_addr(const char *data, char *ret) {
    uint32_t ip[4];
    if (ipver[1] == IPVER_IPV4) {
        sscanf(data, "%" SCNd32 ".%" SCNd32 ".%" SCNd32 ".%" SCNd32 "", ip,
               ip + 1, ip + 2, ip + 3);
        ip[0] = (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]);
        write_hps_reg("net20", ip[0]);
    } else if (ipver[1] == IPVER_IPV6) {
        sscanf(data, "%" SCNx32 ":%" SCNx32 ":%" SCNx32 ":%" SCNx32 "", ip,
               ip + 1, ip + 2, ip + 3);
        write_hps_reg("net16", ip[0]);
        write_hps_reg("net17", ip[1]);
        write_hps_reg("net18", ip[2]);
        write_hps_reg("net19", ip[3]);
    }
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpb_mac_addr(const char *data, char *ret) {
    uint8_t mac[6];
    sscanf(data,
           "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
           mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
    write_hps_reg("net26", (mac[0] << 8) | (mac[1]));
    write_hps_reg("net27",
                  (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpb_ver(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t ver;
    sscanf(data, "%" SCNd8 "", &ver);
    read_hps_reg("net15", &old_val);
    if (ver > 0)
        write_hps_reg("net15", (old_val & ~(1 << 2)) | (1 << 2));
    else
        write_hps_reg("net15", (old_val & ~(1 << 2)));
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpb_pay_len(const char *data, char *ret) {
    uint32_t old_val;
    uint32_t pay_len;
    sscanf(data, "%" SCNd32 "", &pay_len);
    read_hps_reg("net15", &old_val);
    write_hps_reg("net15", (old_val & ~(0xffff0000)) | (pay_len << 16));
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_net_dhcp_en(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_net_hostname(const char *data, char *ret) {
    char name[MAX_PROP_LEN] = {0};
    char command[MAX_PROP_LEN] = {0};
    sscanf(data, "%s", name);

    strcpy(command, "echo ");
    strcat(command, name);
    strcat(command, " > /etc/hostname");
    system(command);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_net_ip_addr(const char *data, char *ret) {
    // Ensure that it is a valid IP address
    char ip_address[MAX_PROP_LEN] = {0};
    char command[MAX_PROP_LEN] = {0};
    sscanf(data, "%s", ip_address);

    struct sockaddr_in sa;
    if (!inet_pton(AF_INET, ip_address, &(sa.sin_addr))) {
        return RETURN_ERROR_PARAM;
    }

    // Write to the file
    strcpy(command, "sed -r -i 's/(\\b[0-9]{1,3}\\.){3}[0-9]{1,3}\\b'/");
    strcat(command, ip_address);
    strcat(command, "/ /etc/init.d/mcu_init.sh");
    system(command);
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gps_time(const char *data, char *ret) {
    uint32_t gps_time_lh = 0, gps_time_uh = 0;
    char gps_split[MAX_PROP_LEN];

    read_hps_reg("sys5", &gps_time_lh);
    read_hps_reg("sys6", &gps_time_uh);

    snprintf(gps_split, MAX_PROP_LEN, "%i", gps_time_uh);
    strncpy(ret, gps_split, MAX_PROP_LEN);
    snprintf(gps_split, MAX_PROP_LEN, "%i", gps_time_lh);
    strncat(ret, gps_split, MAX_PROP_LEN);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gps_frac_time(const char *data, char *ret) {
    uint32_t gps_frac_time_lh = 0, gps_frac_time_uh = 0;
    char gps_split[MAX_PROP_LEN];
    read_hps_reg("sys7", &gps_frac_time_lh);
    read_hps_reg("sys8", &gps_frac_time_uh);

    snprintf(gps_split, MAX_PROP_LEN, "%i", gps_frac_time_uh);
    strncpy(ret, gps_split, MAX_PROP_LEN);
    snprintf(gps_split, MAX_PROP_LEN, "%i", gps_frac_time_lh);
    strncat(ret, gps_split, MAX_PROP_LEN);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gps_sync_time(const char *data, char *ret) {
    uint32_t systime_lh = 0;
    uint32_t systime_uh = 0;
    read_hps_reg("sys5", &systime_lh);
    read_hps_reg("sys6", &systime_uh);
    write_hps_reg("sys9", systime_lh);
    write_hps_reg("sys10", systime_uh);
    write_hps_reg("sys11", 0); // Set frac_time to 0
    write_hps_reg("sys12", 0); // Set frac_time to 0
    write_hps_reg("sys13", 1); // Writing 1, then 0 to sys9 sets the time
    write_hps_reg("sys13", 0); // to what is written in sys7 and sys8

    return RETURN_SUCCESS;
}

/* clang-format off */

/* -------------------------------------------------------------------------- */
/* ---------------------------- PROPERTY TABLE ------------------------------ */
/* -------------------------------------------------------------------------- */

// This is the file property tree which links all XMACRO expanded functions above
// to a file path with some sort of default value. The default values
// should ideally be pulled from the MCU to prevent extensive configuration.

#define DEFINE_FILE_PROP(n, h, p, v) \
    {                                \
        .type = PROP_TYPE_FILE,      \
        .path = n,                   \
        .handler = h,                \
        .permissions = p,            \
        .def_val = v,                \
    },

#define DEFINE_SYMLINK_PROP(n, t)    \
    {                                \
        .type = PROP_TYPE_SYMLINK,   \
        .path = n,                   \
        .symlink_target = t,         \
    },

#define DEFINE_RX_CHANNEL(_c)                                                                                         \
    DEFINE_SYMLINK_PROP("rx_" #_c, "rx/" #_c)                                                                         \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/sma_mode"         , hdlr_rx_##_c##_trigger_sma_mode,        RW, "level")     \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/trig_sel"         , hdlr_rx_##_c##_trigger_trig_sel,        RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/edge_backoff"     , hdlr_rx_##_c##_trigger_edge_backoff,    RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/edge_sample_num"  , hdlr_rx_##_c##_trigger_edge_sample_num, RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/ufl_mode"         , hdlr_rx_##_c##_trigger_ufl_mode,        RW, "level")     \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/ufl_dir"          , hdlr_rx_##_c##_trigger_ufl_dir,         RW, "out")       \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/ufl_pol"          , hdlr_rx_##_c##_trigger_ufl_pol,         RW, "negative")  \
    DEFINE_FILE_PROP("rx/" #_c "/pwr"                      , hdlr_rx_##_c##_pwr,                     RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/stream"                   , hdlr_rx_##_c##_stream,                  RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/sync"                     , hdlr_rx_sync,                           WO, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/rf/freq/val"              , hdlr_rx_##_c##_rf_freq_val,             RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/rf/freq/lut_en"           , hdlr_rx_##_c##_rf_freq_lut_en,          RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/rf/freq/lna"              , hdlr_rx_##_c##_rf_freq_lna,             RW, "1")         \
    DEFINE_FILE_PROP("rx/" #_c "/rf/freq/band"             , hdlr_rx_##_c##_rf_freq_band,            RW, "1")         \
    DEFINE_FILE_PROP("rx/" #_c "/rf/gain/val"              , hdlr_rx_##_c##_rf_gain_val,             RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/rf/atten/val"             , hdlr_rx_##_c##_rf_atten_val,            RW, "127")       \
    DEFINE_FILE_PROP("rx/" #_c "/status/rfpll_lock"        , hdlr_rx_##_c##_status_rfld,             RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/status/adc_alarm"         , hdlr_rx_##_c##_status_adcalarm,         RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/board/dump"               , hdlr_rx_##_c##_rf_board_dump,           WO, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/board/test"               , hdlr_rx_##_c##_rf_board_test,           WO, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/board/temp"               , hdlr_rx_##_c##_rf_board_temp,           RW, "20")        \
    DEFINE_FILE_PROP("rx/" #_c "/board/led"                , hdlr_rx_##_c##_rf_board_led,            WO, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/dsp/signed"               , hdlr_rx_##_c##_dsp_signed,              RW, "1")         \
    DEFINE_FILE_PROP("rx/" #_c "/dsp/gain"                 , hdlr_rx_##_c##_dsp_gain,                RW, "10")        \
    DEFINE_FILE_PROP("rx/" #_c "/dsp/rate"                 , hdlr_rx_##_c##_dsp_rate,                RW, "1258850")   \
    DEFINE_FILE_PROP("rx/" #_c "/dsp/nco_adj"              , hdlr_rx_##_c##_dsp_nco_adj,             RW, "-15000000") \
    DEFINE_FILE_PROP("rx/" #_c "/dsp/rstreq"               , hdlr_rx_##_c##_dsp_rstreq,              WO, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/dsp/loopback"             , hdlr_rx_##_c##_dsp_loopback,            RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/about/id"                 , hdlr_rx_##_c##_about_id,                RW, "001")       \
    DEFINE_FILE_PROP("rx/" #_c "/about/serial"             , hdlr_rx_##_c##_about_serial,            RW, "001")       \
    DEFINE_FILE_PROP("rx/" #_c "/about/mcudevid"           , hdlr_rx_##_c##_about_mcudevid,          RW, "001")       \
    DEFINE_FILE_PROP("rx/" #_c "/about/mcurev"             , hdlr_rx_##_c##_about_mcurev,            RW, "001")       \
    DEFINE_FILE_PROP("rx/" #_c "/about/mcufuses"           , hdlr_rx_##_c##_about_mcufuses,          RW, "001")       \
    DEFINE_FILE_PROP("rx/" #_c "/about/fw_ver"             , hdlr_rx_##_c##_about_fw_ver,            RW, VERSION)     \
    DEFINE_FILE_PROP("rx/" #_c "/about/sw_ver"             , hdlr_invalid,                           RO, VERSION)     \
    DEFINE_FILE_PROP("rx/" #_c "/link/vita_en"             , hdlr_rx_##_c##_link_vita_en,            RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/link/iface"               , hdlr_rx_##_c##_link_iface,              RW, "sfpa")      \
    DEFINE_FILE_PROP("rx/" #_c "/link/port"                , hdlr_rx_##_c##_link_port,               RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/link/ip_dest"             , hdlr_rx_##_c##_link_ip_dest,            RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/link/mac_dest"            , hdlr_rx_##_c##_link_mac_dest,           RW, "ff:ff:ff:ff:ff:ff")

#define DEFINE_TX_CHANNEL(_c)                                                                                        \
    DEFINE_SYMLINK_PROP("tx_" #_c, "tx/" #_c)                                                                        \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/sma_mode"         , hdlr_tx_##_c##_trigger_sma_mode,        RW, "level")    \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/trig_sel"         , hdlr_tx_##_c##_trigger_trig_sel,        RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/edge_backoff"     , hdlr_tx_##_c##_trigger_edge_backoff,    RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/edge_sample_num"  , hdlr_tx_##_c##_trigger_edge_sample_num, RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/ufl_dir"          , hdlr_tx_##_c##_trigger_ufl_dir,         RW, "out")      \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/ufl_mode"         , hdlr_tx_##_c##_trigger_ufl_mode,        RW, "level")    \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/ufl_pol"          , hdlr_tx_##_c##_trigger_ufl_pol,         RW, "negative") \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/gating"           , hdlr_tx_##_c##_trigger_gating,          RW, "output")   \
    DEFINE_FILE_PROP("tx/" #_c "/pwr"                      , hdlr_tx_##_c##_pwr,                     RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/sync"                     , hdlr_tx_sync,                           WO, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/dither_en"         , hdlr_tx_##_c##_rf_dac_dither_en,        RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/dither_mixer_en"   , hdlr_tx_##_c##_rf_dac_dither_mixer_en,  RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/dither_sra_sel"    , hdlr_tx_##_c##_rf_dac_dither_sra_sel,   RW, "6")        \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/nco"               , hdlr_tx_##_c##_rf_dac_nco,              RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/temp"              , hdlr_tx_##_c##_rf_dac_temp,             RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/rf/freq/val"              , hdlr_tx_##_c##_rf_freq_val,             RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/rf/freq/lut_en"           , hdlr_tx_##_c##_rf_freq_lut_en,          RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/rf/freq/band"             , hdlr_tx_##_c##_rf_freq_band,            RW, "1")        \
    DEFINE_FILE_PROP("tx/" #_c "/rf/freq/i_bias"           , hdlr_tx_##_c##_rf_freq_i_bias,          RW, "17")       \
    DEFINE_FILE_PROP("tx/" #_c "/rf/freq/q_bias"           , hdlr_tx_##_c##_rf_freq_q_bias,          RW, "17")       \
    DEFINE_FILE_PROP("tx/" #_c "/rf/gain/val"              , hdlr_tx_##_c##_rf_gain_val,             RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/status/rfpll_lock"        , hdlr_tx_##_c##_status_rfld,             RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/status/dacpll_lock"       , hdlr_tx_##_c##_status_dacld,            RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/status/dacpll_centre"     , hdlr_tx_##_c##_status_dacctr,           RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/board/dump"               , hdlr_tx_##_c##_rf_board_dump,           WO, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/board/test"               , hdlr_tx_##_c##_rf_board_test,           WO, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/board/temp"               , hdlr_tx_##_c##_rf_board_temp,           RW, "23")       \
    DEFINE_FILE_PROP("tx/" #_c "/board/led"                , hdlr_tx_##_c##_rf_board_led,            WO, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/dsp/gain"                 , hdlr_tx_##_c##_dsp_gain,                RW, "10")       \
    DEFINE_FILE_PROP("tx/" #_c "/dsp/rate"                 , hdlr_tx_##_c##_dsp_rate,                RW, "1258850")  \
    DEFINE_FILE_PROP("tx/" #_c "/dsp/nco_adj"              , hdlr_tx_##_c##_dsp_nco_adj,             RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/dsp/rstreq"               , hdlr_tx_##_c##_dsp_rstreq,              WO, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/about/id"                 , hdlr_tx_##_c##_about_id,                RW, "001")      \
    DEFINE_FILE_PROP("tx/" #_c "/about/serial"             , hdlr_tx_##_c##_about_serial,            RW, "001")      \
    DEFINE_FILE_PROP("tx/" #_c "/about/mcudevid"           , hdlr_tx_##_c##_about_mcudevid,          RW, "001")      \
    DEFINE_FILE_PROP("tx/" #_c "/about/mcurev"             , hdlr_tx_##_c##_about_mcurev,            RW, "001")      \
    DEFINE_FILE_PROP("tx/" #_c "/about/mcufuses"           , hdlr_tx_##_c##_about_mcufuses,          RW, "001")      \
    DEFINE_FILE_PROP("tx/" #_c "/about/fw_ver"             , hdlr_tx_##_c##_about_fw_ver,            RW, VERSION)    \
    DEFINE_FILE_PROP("tx/" #_c "/about/sw_ver"             , hdlr_invalid,                           RO, VERSION)    \
    DEFINE_FILE_PROP("tx/" #_c "/link/vita_en"             , hdlr_tx_##_c##_link_vita_en,            RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/link/iface"               , hdlr_tx_##_c##_link_iface,              RW, "sfpa")     \
    DEFINE_FILE_PROP("tx/" #_c "/link/port"                , hdlr_tx_##_c##_link_port,               RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/qa/fifo_lvl"              , hdlr_tx_##_c##_qa_fifo_lvl,             RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/qa/oflow"                 , hdlr_tx_##_c##_qa_oflow,                RW, "0")        \
    DEFINE_FILE_PROP("tx/" #_c "/qa/uflow"                 , hdlr_tx_##_c##_qa_uflow,                RW, "0")

#define DEFINE_TIME()                                                                                                \
    DEFINE_FILE_PROP("time/clk/pps"                        , hdlr_time_clk_pps,                      RW, "0")        \
    DEFINE_FILE_PROP("time/clk/cur_time"                   , hdlr_time_clk_cur_time,                 RW, "0.0")      \
    DEFINE_FILE_PROP("time/status/lmk_lockdetect"          , hdlr_time_status_ld,                    RW, "unlocked") \
    DEFINE_FILE_PROP("time/status/lmk_lossoflock"          , hdlr_time_status_lol,                   RW, "unlocked") \
    DEFINE_FILE_PROP("time/status/lmk_lockdetect_jesd_pll1", hdlr_time_status_ld_jesd_pll1,          RW, "unlocked") \
    DEFINE_FILE_PROP("time/status/lmk_lockdetect_jesd_pll2", hdlr_time_status_ld_jesd_pll2,          RW, "unlocked") \
    DEFINE_FILE_PROP("time/status/lmk_lockdetect_pll_pll1" , hdlr_time_status_ld_pll_pll1,           RW, "unlocked") \
    DEFINE_FILE_PROP("time/status/lmk_lockdetect_pll_pll2" , hdlr_time_status_ld_pll_pll2,           RW, "unlocked") \
    DEFINE_FILE_PROP("time/status/lmk_lossoflock_jesd_pll1", hdlr_time_status_lol_jesd_pll1,         RW, "unlocked") \
    DEFINE_FILE_PROP("time/status/lmk_lossoflock_jesd_pll2", hdlr_time_status_lol_jesd_pll2,         RW, "unlocked") \
    DEFINE_FILE_PROP("time/status/lmk_lossoflock_pll_pll1" , hdlr_time_status_lol_pll_pll1,          RW, "unlocked") \
    DEFINE_FILE_PROP("time/status/lmk_lossoflock_pll_pll2" , hdlr_time_status_lol_pll_pll2,          RW, "unlocked") \
    DEFINE_FILE_PROP("time/source/ref"                     , hdlr_time_source_ref,                   RW, "internal") \
    DEFINE_FILE_PROP("time/source/extsine"                 , hdlr_time_source_extsine,               RW, "sine")     \
    DEFINE_FILE_PROP("time/sync/lmk_sync_tgl_jesd"         , hdlr_time_sync_lmk_sync_tgl_jesd,       WO, "0")        \
    DEFINE_FILE_PROP("time/sync/lmk_sync_tgl_pll"          , hdlr_time_sync_lmk_sync_tgl_pll,        WO, "0")        \
    DEFINE_FILE_PROP("time/sync/lmk_sync_resync_jesd"      , hdlr_time_sync_lmk_resync_jesd,         WO, "0")        \
    DEFINE_FILE_PROP("time/sync/lmk_sync_resync_pll"       , hdlr_time_sync_lmk_resync_pll,          WO, "0")        \
    DEFINE_FILE_PROP("time/sync/lmk_resync_all"            , hdlr_time_sync_lmk_resync_all,          WO, "0")        \
    DEFINE_FILE_PROP("time/board/dump"                     , hdlr_time_board_dump,                   WO, "0")        \
    DEFINE_FILE_PROP("time/board/test"                     , hdlr_time_board_test,                   WO, "0")        \
    DEFINE_FILE_PROP("time/board/temp"                     , hdlr_time_board_temp,                   RW, "20")       \
    DEFINE_FILE_PROP("time/board/led"                      , hdlr_time_board_led,                    WO, "0")        \
    DEFINE_FILE_PROP("time/about/id"                       , hdlr_time_about_id,                     RO, "001")      \
    DEFINE_FILE_PROP("time/about/serial"                   , hdlr_time_about_serial,                 RW, "001")      \
    DEFINE_FILE_PROP("time/about/mcudevid"                 , hdlr_time_about_mcudevid,               RW, "001")      \
    DEFINE_FILE_PROP("time/about/mcurev"                   , hdlr_time_about_mcurev,                 RW, "001")      \
    DEFINE_FILE_PROP("time/about/mcufuses"                 , hdlr_time_about_mcufuses,               RW, "001")      \
    DEFINE_FILE_PROP("time/about/fw_ver"                   , hdlr_time_about_fw_ver,                 RW, VERSION)    \
    DEFINE_FILE_PROP("time/about/sw_ver"                   , hdlr_invalid,                           RO, VERSION)

#define DEFINE_FPGA()                                                                                                         \
    DEFINE_FILE_PROP("fpga/trigger/sma_dir"                , hdlr_fpga_trigger_sma_dir,              RW, "out")               \
    DEFINE_FILE_PROP("fpga/trigger/sma_pol"                , hdlr_fpga_trigger_sma_pol,              RW, "negative")          \
    DEFINE_FILE_PROP("fpga/about/fw_ver"                   , hdlr_fpga_about_fw_ver,                 RW, VERSION)             \
    DEFINE_FILE_PROP("fpga/about/server_ver"               , hdlr_server_about_fw_ver,               RW, "")                  \
    DEFINE_FILE_PROP("fpga/about/hw_ver"                   , hdlr_fpga_about_hw_ver,                 RW, VERSION)             \
    DEFINE_FILE_PROP("fpga/about/id"                       , hdlr_fpga_about_id,                     RW, "001")               \
    DEFINE_FILE_PROP("fpga/about/name"                     , hdlr_invalid,                           RO, "crimson_tng")       \
    DEFINE_FILE_PROP("fpga/about/serial"                   , hdlr_fpga_about_serial,                 RW, "001")               \
    DEFINE_FILE_PROP("fpga/about/cmp_time"                 , hdlr_fpga_about_cmp_time,               RW, "yyyy-mm-dd-hh-mm")  \
    DEFINE_FILE_PROP("fpga/about/conf_info"                , hdlr_fpga_about_conf_info,              RW, "0")                 \
    DEFINE_FILE_PROP("fpga/board/dump"                     , hdlr_fpga_board_dump,                   WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/fw_rst"                   , hdlr_fpga_board_fw_rst,                 WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/flow_control/sfpa_port"   , hdlr_fpga_board_flow_control_sfpa_port, RW, "42809")             \
    DEFINE_FILE_PROP("fpga/board/flow_control/sfpb_port"   , hdlr_fpga_board_flow_control_sfpb_port, RW, "42809")             \
    DEFINE_FILE_PROP("fpga/board/gps_time"                 , hdlr_fpga_board_gps_time,               RW, "0")                 \
    DEFINE_FILE_PROP("fpga/board/gps_frac_time"            , hdlr_fpga_board_gps_frac_time,          RW, "0")                 \
    DEFINE_FILE_PROP("fpga/board/gps_sync_time"            , hdlr_fpga_board_gps_sync_time,          RW, "0")                 \
    DEFINE_FILE_PROP("fpga/board/jesd_sync"                , hdlr_fpga_board_jesd_sync,              WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/led"                      , hdlr_fpga_board_led,                    WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/rstreq"                   , hdlr_fpga_board_rstreq,                 WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/reboot"                   , hdlr_fpga_board_reboot,                 RW, "0")                 \
    DEFINE_FILE_PROP("fpga/board/sys_rstreq"               , hdlr_fpga_board_sys_rstreq,             WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/test"                     , hdlr_fpga_board_test,                   WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/temp"                     , hdlr_fpga_board_temp,                   RW, "20")                \
    DEFINE_FILE_PROP("fpga/board/gle"                      , hdlr_fpga_board_gle,                    RW, "0")                 \
    DEFINE_FILE_PROP("fpga/link/rate"                      , hdlr_fpga_link_rate,                    RW, "1250000000")        \
    DEFINE_FILE_PROP("fpga/link/sfpa/ip_addr"              , hdlr_fpga_link_sfpa_ip_addr,            RW, "10.10.10.2")        \
    DEFINE_FILE_PROP("fpga/link/sfpa/mac_addr"             , hdlr_fpga_link_sfpa_mac_addr,           RW, "aa:00:00:00:00:00") \
    DEFINE_FILE_PROP("fpga/link/sfpa/ver"                  , hdlr_fpga_link_sfpa_ver,                RW, "0")                 \
    DEFINE_FILE_PROP("fpga/link/sfpa/pay_len"              , hdlr_fpga_link_sfpa_pay_len,            RW, "1400")              \
    DEFINE_FILE_PROP("fpga/link/sfpb/ip_addr"              , hdlr_fpga_link_sfpb_ip_addr,            RW, "10.10.11.2")        \
    DEFINE_FILE_PROP("fpga/link/sfpb/mac_addr"             , hdlr_fpga_link_sfpb_mac_addr,           RW, "aa:00:00:00:00:01") \
    DEFINE_FILE_PROP("fpga/link/sfpb/ver"                  , hdlr_fpga_link_sfpb_ver,                RW, "0")                 \
    DEFINE_FILE_PROP("fpga/link/sfpb/pay_len"              , hdlr_fpga_link_sfpb_pay_len,            RW, "1400")              \
    DEFINE_FILE_PROP("fpga/link/net/dhcp_en"               , hdlr_fpga_link_net_dhcp_en,             RW, "0")                 \
    DEFINE_FILE_PROP("fpga/link/net/hostname"              , hdlr_fpga_link_net_hostname,            RW, "crimson_tng")       \
    DEFINE_FILE_PROP("fpga/link/net/ip_addr"               , hdlr_fpga_link_net_ip_addr,             RW, "192.168.10.2")

#define DEFINE_CM()                                                    \
    DEFINE_FILE_PROP("cm/chanmask-rx" , hdlr_cm_chanmask_rx , RW, "0") \
    DEFINE_FILE_PROP("cm/chanmask-tx" , hdlr_cm_chanmask_tx , RW, "0") \
    DEFINE_FILE_PROP("cm/rx/atten/val", hdlr_cm_rx_atten_val, WO, "0") \
    DEFINE_FILE_PROP("cm/rx/gain/val" , hdlr_cm_rx_gain_val , WO, "0") \
    DEFINE_FILE_PROP("cm/tx/gain/val" , hdlr_cm_tx_gain_val , WO, "0") \
    DEFINE_FILE_PROP("cm/trx/freq/val", hdlr_cm_trx_freq_val, WO, "0") \
    DEFINE_FILE_PROP("cm/trx/nco_adj" , hdlr_cm_trx_nco_adj , WO, "0")

static prop_t property_table[] = {
#define X(ch) DEFINE_RX_CHANNEL(ch)
    CHANNELS
#undef X
#define X(ch) DEFINE_TX_CHANNEL(ch)
    CHANNELS
#undef X
    DEFINE_TIME()
    DEFINE_FPGA()
    DEFINE_FILE_PROP("save_config", hdlr_save_config, RW, "/home/root/profile.cfg")
    DEFINE_FILE_PROP("load_config", hdlr_load_config, RW, "/home/root/profile.cfg")
    DEFINE_CM()
};

static const size_t num_properties = LEN(property_table);

static const char *tostr(const int num)
{
    char* str = calloc(32, sizeof(*str));
    sprintf(str, "%d", num);
    return str;
}

/* clang-format on */

/* -------------------------------------------------------------------------- */
/* -------------------------- EXTERNED FUNCTIONS ---------------------------- */
/* -------------------------------------------------------------------------- */

// Some elements (like ports) from the property table when the XMACRO was introduced.
// This function puts them back.
void patch_table(void)
{
    const int base = 42820;
    const int offset = LEN(names);

    // RX Ports
#define X(ch) \
    set_property("rx/" #ch "/link/port", tostr(base + INT(ch)));
    CHANNELS
#undef X

    // RX IP Addresses
#define X(ch) \
    set_property("rx/" #ch "/link/ip_dest", INT(ch) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
    CHANNELS
#undef X

    // TX Ports
#define X(ch) \
    set_property("tx/" #ch "/link/port"  , tostr(base + INT(ch) + offset)); \
    set_property("tx/" #ch "/qa/fifo_lvl", tostr(base + INT(ch) + offset)); \
    set_property("tx/" #ch "/qa/oflow"   , tostr(base + INT(ch) + offset)); \
    set_property("tx/" #ch "/qa/uflow"   , tostr(base + INT(ch) + offset));
    CHANNELS
#undef X

    // Using tostr() calls malloc internally (but with very bytes). There will be some
    // memory leaks, as the string is copied to the property table char array,
    // but this ok, as the byte count is very small.
}

size_t get_num_prop(void) {
    return num_properties;
}

prop_t *get_prop(size_t idx) {
    return (property_table + idx);
}

prop_t *get_prop_from_wd(int wd) {
    size_t i;
    for (i = 0; i < num_properties; i++) {
        if (property_table[i].wd == wd)
            return (property_table + i);
    }
    return NULL;
}

prop_t *get_prop_from_hdlr(int (*hdlr)(const char *, char *)) {
    size_t i;
    for (i = 0; i < num_properties; i++) {
        if (property_table[i].handler == hdlr) {
            return &property_table[i];
        }
    }
    return NULL;
}

int resolve_symbolic_property_name(const char *prop, char *path, size_t n) {

    const char *vcs = "/var/crimson/state/";
    const size_t vcsl = strlen(vcs);
    char origcwd[MAX_PATH_LEN];
    char *temp;
    size_t path_strlen;
    size_t delta;
    int r;

#if MAX_PATH_LEN < PATH_MAX
#error MAX_PATH_LEN is too small
#endif

    getcwd(origcwd, sizeof(origcwd));
    chdir(vcs);
    temp = (void *)realpath(prop, path);
    chdir(origcwd);
    if (NULL == temp) {
        PRINT(ERROR, "unable to find a property corresponding to '%s'\n", prop);
        return RETURN_ERROR_SET_PROP;
    }

    path_strlen = strlen(path);
    r = strncmp(vcs, path, vcsl);
    if (0 == r) {
        delta = path_strlen - vcsl;
        memmove(path, path + vcsl, delta);
        path[delta] = '\0';
    }

    //	if ( 0 != strcmp( path, prop ) ) {
    //		PRINT( INFO, "%s(): resolved symbolic link: '%s' =>
    // '%s'\n",
    // __func__, prop, path );
    //	}

    return RETURN_SUCCESS;
}

prop_t *get_prop_from_cmd(const char *cmd) {
    char path[MAX_PATH_LEN];
    size_t i;

    if (RETURN_SUCCESS ==
        resolve_symbolic_property_name(cmd, path, sizeof(path))) {
        cmd = path;
    }

    for (i = 0; i < num_properties; i++) {
        if ((strcmp(property_table[i].path, cmd) == 0) &&
            (strlen(property_table[i].path) == strlen(cmd)))
            return (property_table + i);
    }

    // no matching prop found
    return NULL;
}

static inline const char *get_home_dir(void) {
    return getpwuid(getuid())->pw_dir;
}

void pass_uart_synth_fd(int fd) {
    uart_synth_fd = fd;
}

void pass_uart_tx_fd(int *fd) {
    uart_tx_fd = fd;
}

void pass_uart_rx_fd(int *fd) {
    uart_rx_fd = fd;
}

char *get_abs_path(prop_t *prop, char *path) {
    strcpy(path, "/var/crimson");
    strcat(path, "/state/");
    strcat(path, prop->path);
    return path;
}

char *get_abs_dir(prop_t *prop, char *path) {
    size_t len = 0;
    size_t i = 0;
    while (prop->path[i]) {
        if (prop->path[i] == '/')
            len = i;
        i++;
    }

    strcpy(path, "/var/crimson");
    strcat(path, "/state/");

    size_t temp_len = 0;

    if (len != 0) {
        temp_len = strlen(path);
        memcpy(path + temp_len, prop->path, len);
        path[temp_len + len] = '\0';
    }

    return path;
}

char *get_root(prop_t *prop, char *root) {
    int i;
    for (i = 0; prop->path[i] != '/' && prop->path[i] != '\0'; i++) {
        root[i] = prop->path[i];
    }
    root[i] = 0;

    return root;
}

void pass_profile_pntr_prop(uint8_t *load, uint8_t *save, char *load_path,
                            char *save_path) {
    _load_profile = load;
    _save_profile = save;
    _load_profile_path = load_path;
    _save_profile_path = save_path;
}

// XXX
// Uses zeroth file descriptor for RX And TX for now until a way is found to
// convert the channel mask into a integer.
void sync_channels(uint8_t chan_mask) {
    char str_chan_mask[MAX_PROP_LEN] = "";
    sprintf(str_chan_mask + strlen(str_chan_mask), "%" PRIu8 "", 15);
    // usleep(300000); // Some wait time for the reset to be ready
    /* Bring the ADCs & DACs into 'demo' mode for JESD */

    // RX - ADCs
    strcpy(buf, "power -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -a 1\r");
    send_uart_comm(uart_rx_fd[0], (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd[0]);

    // TX - DACs
    strcpy(buf, "power -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -d 1\r");
    send_uart_comm(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd[0]);

    /***********************************
     * Start loop.
     * Issue JESD, then read to see if
     * bad
     **********************************/
    char key[] = "00\r";
    char dacalarmA[] = "ff\r";
    char dacalarmB[] = "ff\r";

#ifdef SYNC_CHECK_DAC_LOOP
    for (int i = 0; i < 15; i += 1) {

        // Put FPGA JESD core in reset
        write_hps_reg("res_rw7", 0x40000000);
        write_hps_reg("res_rw7", 0);

        /* Initiate the SYSREF sequence for jesd
         * Set all boards' SYSREF detection gate to ON */
        strcpy(buf, "board -c ");
        strcat(buf, str_chan_mask);
        strcat(buf, " -s 1\r");
        send_uart_comm(uart_rx_fd[0], (uint8_t *)buf, strlen(buf));
        read_uart(uart_rx_fd[0]);
        strcpy(buf, "board -c ");
        strcat(buf, str_chan_mask);
        strcat(buf, " -s 1\r");
        send_uart_comm(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));
        read_uart(uart_tx_fd[0]);

        /* Trigger a SYSREF pulse */
        // JESD core out of reset
        usleep(100000); // Some wait time for MCUs to be ready
        strcpy(buf, "clk -y\r");
        send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        read_uart(uart_synth_fd);

        // Do it again

        strcpy(buf, "board -c ");
        strcat(buf, str_chan_mask);
        strcat(buf, " -s 1\r");
        send_uart_comm(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));
        read_uart(uart_tx_fd[0]);
        usleep(100000); // Some wait time for MCUs to be ready
        strcpy(buf, "clk -y\r");
        send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        read_uart(uart_synth_fd);
        usleep(100000); // Some wait time for MCUs to be ready

        // CHECK IF ALARMS
        strcpy(buf, "dac -c a -s\r");
        send_uart_comm(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));
        read_uart(uart_tx_fd[0]);

        strcpy(dacalarmA, (char *)uart_ret_buf);

        // CHECK IF ALARMS
        strcpy(buf, "dac -c d -s\r");
        send_uart_comm(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));
        read_uart(uart_tx_fd[0]);

        strcpy(dacalarmB, (char *)uart_ret_buf);

        if ((dacalarmA[0] == key[0]) && (dacalarmA[1] == key[1]) &&
            (dacalarmB[0] == key[0]) && (dacalarmB[1] == key[1])) {
            break;
        } else {
            usleep(200000); // Some wait time for MCUs to be ready
        }
    }
    /* Turn off all boards' SYSREF detection gates */
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 0\r");
    send_uart_comm(uart_rx_fd[0], (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd[0]);
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 0\r");
    send_uart_comm(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd[0]);

#else
    // Put FPGA JESD core in reset
    write_hps_reg("res_rw7", 0x20000000);
    write_hps_reg("res_rw7", 0);

    /* Initiate the SYSREF sequence for jesd
     * Set all boards' SYSREF detection gate to ON */
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 1\r");
    send_uart_comm(uart_rx_fd[0], (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd[0]);
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 1\r");
    send_uart_comm(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd[0]);

    /* Trigger a SYSREF pulse */
    // JESD core out of reset
    write_hps_reg("res_rw7", 0);

    usleep(100000); // Some wait time for MCUs to be ready
    strcpy(buf, "clk -y\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);

    /* Turn off all boards' SYSREF detection gates */
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 0\r");
    send_uart_comm(uart_rx_fd[0], (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd[0]);
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 0\r");
    send_uart_comm(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd[0]);

#endif
}

void set_pll_frequency(int uart_fd, uint64_t reference, pllparam_t *pll,
                       bool tx, size_t channel) {
    // extract pll1 variables and pass to MCU (ADF4355/ADF5355)

    // Send Reference to MCU ( No Need ATM since fixed reference )
    strcpy(buf, "rf -v ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(reference / 1000));
    // Send reference in kHz
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

    // write ADF4355/5355 R
    strcpy(buf, "rf -r ");
    sprintf(buf + strlen(buf), "%" PRIu16 "", pll->R);
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

    // write ADF4355/ADF5355 N
    strcpy(buf, "rf -n ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", pll->N);
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

    // write ADF4355/ADF5355 D
    strcpy(buf, "rf -d ");
    sprintf(buf + strlen(buf), "%" PRIu16 "", pll->d);
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

    // write ADF4355/ADF5355 feedback mode
    strcpy(buf, "rf -t ");
    sprintf(buf + strlen(buf), "%" PRIu8 "", pll->divFBen);
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

    // write ADF4355/ADF5355 Output RF Power
    strcpy(buf, "rf -g ");
    sprintf(buf + strlen(buf), "%" PRIu8 "", 1 /*pll->power*/);
    // default to lower mid power
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

    double freq = pll->vcoFreq / pll->d;

    if (synth_lut_is_enabled(tx, channel)) {
        synth_rec_t rec;
        int ret = synth_lut_get(tx, channel, freq, &rec);
        if (EXIT_SUCCESS != ret) {
            PRINT(ERROR, "synth_lut_get( %u, %u, %f ) failed (%d,%s)\n", tx,
                  channel, freq, ret, strerror(ret));
        } else {
            PRINT(INFO,
                  "Setting %s %c @ %u MHz with parameters { %u, %u, %u}\n",
                  tx ? "TX" : "RX", 'A' + channel, (unsigned)(freq / 1000000),
                  rec.core, rec.band, rec.bias);
            snprintf(buf, sizeof(buf), "rf -c %c -A 0 -C %u -B %u -I %u\r",
                     'a' + channel, rec.core, rec.band, rec.bias);
            send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));
        }
    } else {
        // If synth lut is disabled, set autocal flat to enable fall-back
        // behaviour.
        snprintf(buf, sizeof(buf), "rf -c %c -A 1\r", 'a' + channel);
        send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));
    }

    // ADF output power level not presently specified.
    strcpy(buf, "rf -g ");

    // write ADF4355/ADF5355 Output Frequency
    strcpy(buf, "rf -f ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(freq / 1000));
    // Send output frequency in kHz
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));
    usleep(100000);
}

int set_pll_frequency2(int actual_uart_fd, uint64_t reference,
                       pllparam_t *pll) {
    int r;
    // extract pll1 variables and pass to MCU (ADF4355/ADF5355)

    // Send Reference to MCU ( No Need ATM since fixed reference )
    snprintf(buf, sizeof(buf), "rf -v %" PRIu32 "\r",
             (uint32_t)(reference / 1000)); // Send reference in kHz
    r = write(actual_uart_fd, buf, strlen(buf));
    if (strlen(buf) != r) {
        r = errno;
        goto out;
    }

    // write ADF4355/5355 R
    snprintf(buf, sizeof(buf), "rf -r %" PRIu16 "\r", pll->R);
    r = write(actual_uart_fd, buf, strlen(buf));
    if (strlen(buf) != r) {
        r = errno;
        goto out;
    }

    // write ADF4355/ADF5355 N
    snprintf(buf, sizeof(buf), "rf -n %" PRIu32 "\r", pll->N);
    r = write(actual_uart_fd, buf, strlen(buf));
    if (strlen(buf) != r) {
        r = errno;
        goto out;
    }

    // write ADF4355/ADF5355 D
    snprintf(buf, sizeof(buf), "rf -d %" PRIu16 "\r", pll->d);
    r = write(actual_uart_fd, buf, strlen(buf));
    if (strlen(buf) != r) {
        r = errno;
        goto out;
    }

    // write ADF4355/ADF5355 feedback mode
    snprintf(buf, sizeof(buf), "rf -t %" PRIu8 "\r", pll->divFBen);
    r = write(actual_uart_fd, buf, strlen(buf));
    if (strlen(buf) != r) {
        r = errno;
        goto out;
    }

    // write ADF4355/ADF5355 Output RF Power
    // default to lower mid power
    snprintf(buf, sizeof(buf), "rf -g %" PRIu8 "\r", 1 /*pll->power*/);
    r = write(actual_uart_fd, buf, strlen(buf));
    if (strlen(buf) != r) {
        r = errno;
        goto out;
    }

    // write ADF4355/ADF5355 Output Frequency
    // Send output frequency in kHz
    snprintf(buf, sizeof(buf), "rf -f %" PRIu32 "\r",
             (uint32_t)(pll->vcoFreq / pll->d / 1000));
    r = write(actual_uart_fd, buf, strlen(buf));
    if (strlen(buf) != r) {
        r = errno;
        goto out;
    }

    size_t tries;
    for (tries = 0; tries < 10; tries++) {
        read(actual_uart_fd, buf, sizeof(buf));
        usleep(10000);
    }

    r = EXIT_SUCCESS;

out:
    if (EXIT_SUCCESS != r) {
        buf[strlen(buf)] = '\0';
        PRINT(ERROR, "failed to send command '%s' (%d,%s)\n", buf, errno,
              strerror(errno));
    }

    return r;
}

int set_freq_internal(const bool tx, const unsigned channel,
                      const double freq) {

    typedef int (*fp_t)(const char *, char *);

    static const fp_t rx_fp[] = {
#define X(ch) hdlr_rx_##ch##_rf_freq_val,
    CHANNELS
#undef X
    };

    static const fp_t tx_fp[] = {
#define X(ch) hdlr_tx_##ch##_rf_freq_val,
    CHANNELS
#undef X
    };

    int r;

    char req_buf[MAX_PROP_LEN];
    char rsp_buf[MAX_PROP_LEN];

    if (channel > (tx ? ARRAY_SIZE(tx_fp) : ARRAY_SIZE(rx_fp))) {
        r = E2BIG;
        PRINT(ERROR, "channel %u is invalid (%d,%s)\n", channel, r,
              strerror(r));
        goto out;
    }

    const fp_t *fp = tx ? tx_fp : rx_fp;

    memset(req_buf, '\0', sizeof(req_buf));
    memset(rsp_buf, '\0', sizeof(rsp_buf));

    // N.B. the print formatter in this case must be equal to the one in
    // hdlr_XX_X_rf_freq_val
    snprintf(req_buf, sizeof(req_buf), "%lf", freq);

    r = fp[channel](req_buf, rsp_buf);
    if (RETURN_SUCCESS != r) {
        PRINT(ERROR, "function call to hdlr_XX_X_rf_freq_val() failed (%d)\n",
              r);
        r = EIO;
        goto out;
    }

    double actual_freq = 0;
    if (1 != sscanf(rsp_buf, "%lf", &actual_freq) || actual_freq != freq) {
        r = EIO;
        PRINT(ERROR, "%s %c: expected: %f, actual: %f\n", tx ? "TX" : "RX",
              'A' + channel, freq, actual_freq);
        goto out;
    }

    flush_uart_comm(tx ? uart_tx_fd[channel] : uart_rx_fd[channel]);

    r = EXIT_SUCCESS;

out:
    return r;
}
