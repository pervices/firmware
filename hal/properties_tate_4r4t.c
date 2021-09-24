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

/* clang-format off */

#if defined(TATE_4R4T)

#if 1 /* Removes headers for quick gcc -E diagnostics for XMACRO stuffs */
    #include "properties.h"

    #include "array-utils.h"
    #include "mmap.h"
    #include "property_manager.h"
    #include "synth_lut.h"
    #include "time_it.h"

    #include <ctype.h>
    #include <stdbool.h>
    #include <stdio.h>
    #include <string.h>
    #include <math.h>
#endif

#include <signal.h>
#include "channels.h"
#include "gpio_pins.h"

// Sample rates are in samples per second (SPS).
#define BASE_SAMPLE_RATE   500000000.0  //After base rate
#define RESAMP_SAMPLE_RATE 160000000.0  //After 4/5 resampling //NB: Tate 64t does NOT support 4/5 resampling
// (2 ^ 32) / (BASE_SAMPLE_RATE)
#define DSP_NCO_CONST \
    ((double)8.589934592)

#define MIN_RF_GAIN_TX \
    ((double)-13)
#define MAX_RF_GAIN_TX \
    ((double)17)
#define MIN_RF_ATTEN_TX \
    ((double)0)
#define MAX_RF_ATTEN_TX \
    ((double)30)

//Compnent properties in rx, used to figure out how to set up game
//This are likely to change between variants, both thier values and how they are used
#define AM1081_GAIN 17
#define AM1075_GAIN 18
#define LTC5586_MAX_GAIN 15
#define LTC5586_MIN_GAIN 8
#define LTC5586_MAX_ATTEN 31
#define LTC5586_MIN_ATTEN 0

//used in rx gain calculations so that the user specifying a gain of 0 results in minimum gain
#define RX_LOW_GAIN_OFFSET 6
#define RX_MID_GAIN_OFFSET 23
#define RX_HIGH_GAIN_OFFSET 23

//used for rf freq val calc when in high band
#define HB_STAGE2_MIXER_FREQ 1800000000

#define IPVER_IPV4 0
#define IPVER_IPV6 1

#define STREAM_ON  1
#define STREAM_OFF 0

//contains the registers used for rx_4 for each channel
//most registers follow the pattern rxa0 for ch a, rxb0 for ch b
//Unlike most channels rx_4 uses a different patttern
static const char *rx_reg4_map[4] = { "rxa4", "rxe4", "rxi4", "rxm4" };

static const char *tx_reg4_map[4] = { "txa4", "txe4", "txi4", "txm4" };

// A typical VAUNT file descriptor layout may look something like this:
// RX = { 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1  }
// TX = { 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1  }
// Assuming file descriptors 0 and 1 are both valid.
// For TATE, as there sixteen channels, with each file descriptor pointing to a
// unique MCU:
// RX = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }
// TX = {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 }
// Note that RX is -1 here as TATE is very configurable with its backplane
// configuration setup. One client may want 16 TX and 0 RX lines, while
// another may want 8 TX and 8 RX lines:
// RX = {  0,  1,  2,  3,  4,  5,  6,  7, -1, -1, -1, -1, -1, -1, -1, -1 }
// TX = {  0,  1,  2,  3,  4,  5,  6,  7, -1, -1, -1, -1, -1, -1, -1, -1 }
static int *uart_tx_fd = NULL;
static int *uart_rx_fd = NULL;

// For VAUNT and TATE there is always one MCU for time control.
static int uart_synth_fd = 0;

static uint8_t uart_ret_buf[MAX_UART_RET_LEN] = { 0x00 };
static char buf[MAX_PROP_LEN] = { '\0' };
int max_attempts = 10;
int jesd_good_code = 0xf;

//Of the following PWR, only PWR_OFF and PWR_ON are valid inputs, the rest are used internally to check the status of things
//indicates that no board is present
#define PWR_NO_BOARD 2
//indicates that either the board if off, if no board is present it will default to this value until the fact that the baord is missing is detected
#define PWR_OFF 0
//indicates that the board is on, but the rest of the startup sequence has not been doner
#define PWR_HALF_ON 3
#define PWR_ON 1
static uint8_t rx_power[NUM_CHANNELS] = {0};

static uint8_t tx_power[NUM_CHANNELS] = {0};

static uint8_t rx_stream[NUM_CHANNELS] = {0};

static pid_t rx_async_pwr_pid[NUM_CHANNELS] = {0};
static pid_t tx_async_pwr_pid[NUM_CHANNELS] = {0};
//the time async_pwr started running, used when calculating if it timed out
//timeout is in seconds
#define timeout 15
static time_t rx_async_start_time[NUM_CHANNELS] = {0};
static time_t tx_async_start_time[NUM_CHANNELS] = {0};

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

    const long t0 = time_it();

    while (contains(buf, '>', total_bytes) < 1) {
        if (recv_uart_comm(uartfd, ((uint8_t *)buf) + total_bytes, &cur_bytes, MAX_UART_LEN - total_bytes)) {
            strcpy((char *)uart_ret_buf, "");

            return RETURN_ERROR;
        }
        total_bytes += cur_bytes;
    }
    const long t1 = time_it();

    printf("read uart %ld : %s\n", t1 - t0, buf);
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

static int set_sma_dir(bool in) { return set_reg_bits("sys2", 4, 1, in); }

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

#define X(ch, io, crx, ctx)                                                              \
    static int hdlr_rx_##ch##_rf_freq_lut_en(const char *data, char *ret) {    \
        return hdlr_XX_X_rf_freq_lut_en(data, ret, false, INT(ch));            \
    }
CHANNELS
#undef X

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

#define X(ch, io, crx, ctx)                                                              \
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

// Every uart send command must be accompanied by a uart read command
// so that the command prompt '>' is respected before the next send uart
// command can be used. This removes the need for delay calls in the uart
// send function.
static void ping(const int fd, uint8_t *buf, const size_t len) {
    send_uart_comm(fd, buf, len);
    read_uart(fd);
}
static void ping_write_only(const int fd, uint8_t *buf, const size_t len) {
    send_uart_comm(fd, buf, len);
}
//ping with a check to see if a board is inserted into the desired channel, does nothing if there is no board
//ch is used only to know where in the array to check if a board is present, fd is still used to say where to send the data
static void ping_rx(const int fd, uint8_t *buf, const size_t len, int ch) {
    if(rx_power[ch] != PWR_NO_BOARD) {
        send_uart_comm(fd, buf, len);
        read_uart(fd);
    }
}
static void ping_write_only_rx(const int fd, uint8_t *buf, const size_t len, int ch) {
    if(rx_power[ch] != PWR_NO_BOARD) {
        send_uart_comm(fd, buf, len);
    }
}
static void ping_tx(const int fd, uint8_t *buf, const size_t len, int ch) {
    if(tx_power[ch] != PWR_NO_BOARD) {
        send_uart_comm(fd, buf, len);
        read_uart(fd);
    }
}
static void ping_write_only_tx(const int fd, uint8_t *buf, const size_t len, int ch) {
    if(tx_power[ch] != PWR_NO_BOARD) {
        send_uart_comm(fd, buf, len);
    }
}
/* -------------------------------------------------------------------------- */
/* --------------------------------- TX ------------------------------------- */
/* -------------------------------------------------------------------------- */

#define X(ch, io, crx, ctx)                                                              \
    static int hdlr_tx_##ch##_dac_nco_dac0freq(const char *data, char *ret) {  \
        double freq;                                                           \
        sscanf(data, "%lf", &freq);                                            \
        uint32_t freq_hz = 0;                                                  \
        uint32_t freq_mhz = 0;                                                 \
                                                                               \
        /* split the frequency into MHz + Hz */                                \
        if (freq < 1000000){                                                   \
            freq_hz = freq;                                                    \
        }                                                                      \
        else {                                                                 \
            freq_mhz = (uint32_t)(freq / 1000000);                             \
            freq_hz = (uint32_t)(freq - freq_mhz*1000000);                     \
        }                                                                      \
                                                                               \
        strcpy(buf, "nco -t d -n 0 -a 0 -h ");                                 \
        sprintf(buf + strlen(buf),"%" PRIu32 "", freq_hz);                     \
        sprintf(buf + strlen(buf)," -m %" PRIu32 "", freq_mhz);                \
        strcat(buf, " -s\r");                                                  \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_nco_dac1freq(const char *data, char *ret) {  \
        double freq;                                                           \
        sscanf(data, "%lf", &freq);                                            \
        uint32_t freq_hz = 0;                                                  \
        uint32_t freq_mhz = 0;                                                 \
                                                                               \
        /* split the frequency into MHz + Hz */                                \
        if (freq < 1000000){                                                   \
            freq_hz = freq;                                                    \
        }                                                                      \
        else {                                                                 \
            freq_mhz = (uint32_t)(freq / 1000000);                             \
            freq_hz = (uint32_t)(freq - freq_mhz*1000000);                     \
        }                                                                      \
                                                                               \
        strcpy(buf, "nco -t d -n 1 -a 0 -h ");                                 \
        sprintf(buf + strlen(buf),"%" PRIu32 "", freq_hz);                     \
        sprintf(buf + strlen(buf)," -m %" PRIu32 "", freq_mhz);                \
        strcat(buf, " -s\r");                                                  \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_nco_ch0freq(const char *data, char *ret) {   \
        double freq;                                                           \
        sscanf(data, "%lf", &freq);                                            \
        uint32_t freq_hz = 0;                                                  \
        uint32_t freq_mhz = 0;                                                 \
                                                                               \
        /* split the frequency into MHz + Hz */                                \
        if (freq < 1000000){                                                   \
            freq_hz = freq;                                                    \
        }                                                                      \
        else {                                                                 \
            freq_mhz = (uint32_t)(freq / 1000000);                             \
            freq_hz = (uint32_t)(freq - freq_mhz*1000000);                     \
        }                                                                      \
                                                                               \
        strcpy(buf, "nco -t c -n 0 -a 0 -h ");                                 \
        sprintf(buf + strlen(buf),"%" PRIu32 "", freq_hz);                     \
        sprintf(buf + strlen(buf)," -m %" PRIu32 "", freq_mhz);                \
        strcat(buf, " -s\r");                                                  \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_nco_ch1freq(const char *data, char *ret) {   \
        double freq;                                                           \
        sscanf(data, "%lf", &freq);                                            \
        uint32_t freq_hz = 0;                                                  \
        uint32_t freq_mhz = 0;                                                 \
                                                                               \
        /* split the frequency into MHz + Hz */                                \
        if (freq < 1000000){                                                   \
            freq_hz = freq;                                                    \
        }                                                                      \
        else {                                                                 \
            freq_mhz = (uint32_t)(freq / 1000000);                             \
            freq_hz = (uint32_t)(freq - freq_mhz*1000000);                     \
        }                                                                      \
                                                                               \
        strcpy(buf, "nco -t c -n 1 -a 0 -h ");                                 \
        sprintf(buf + strlen(buf),"%" PRIu32 "", freq_hz);                     \
        sprintf(buf + strlen(buf)," -m %" PRIu32 "", freq_mhz);                \
        strcat(buf, " -s\r");                                                  \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_nco_ch2freq(const char *data, char *ret) {   \
        double freq;                                                           \
        sscanf(data, "%lf", &freq);                                            \
        uint32_t freq_hz = 0;                                                  \
        uint32_t freq_mhz = 0;                                                 \
                                                                               \
        /* split the frequency into MHz + Hz */                                \
        if (freq < 1000000){                                                   \
            freq_hz = freq;                                                    \
        }                                                                      \
        else {                                                                 \
            freq_mhz = (uint32_t)(freq / 1000000);                             \
            freq_hz = (uint32_t)(freq - freq_mhz*1000000);                     \
        }                                                                      \
                                                                               \
        strcpy(buf, "nco -t c -n 2 -a 0 -h ");                                 \
        sprintf(buf + strlen(buf),"%" PRIu32 "", freq_hz);                     \
        sprintf(buf + strlen(buf)," -m %" PRIu32 "", freq_mhz);                \
        strcat(buf, " -s\r");                                                  \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_nco_ch3freq(const char *data, char *ret) {   \
        double freq;                                                           \
        sscanf(data, "%lf", &freq);                                            \
        uint32_t freq_hz = 0;                                                  \
        uint32_t freq_mhz = 0;                                                 \
                                                                               \
        /* split the frequency into MHz + Hz */                                \
        if (freq < 1000000){                                                   \
            freq_hz = freq;                                                    \
        }                                                                      \
        else {                                                                 \
            freq_mhz = (uint32_t)(freq / 1000000);                             \
            freq_hz = (uint32_t)(freq - freq_mhz*1000000);                     \
        }                                                                      \
                                                                               \
        strcpy(buf, "nco -t c -n 3 -a 0 -h ");                                 \
        sprintf(buf + strlen(buf),"%" PRIu32 "", freq_hz);                     \
        sprintf(buf + strlen(buf)," -m %" PRIu32 "", freq_mhz);                \
        strcat(buf, " -s\r");                                                  \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_nco_ch4freq(const char *data, char *ret) {   \
        double freq;                                                           \
        sscanf(data, "%lf", &freq);                                            \
        uint32_t freq_hz = 0;                                                  \
        uint32_t freq_mhz = 0;                                                 \
                                                                               \
        /* split the frequency into MHz + Hz */                                \
        if (freq < 1000000){                                                   \
            freq_hz = freq;                                                    \
        }                                                                      \
        else {                                                                 \
            freq_mhz = (uint32_t)(freq / 1000000);                             \
            freq_hz = (uint32_t)(freq - freq_mhz*1000000);                     \
        }                                                                      \
                                                                               \
        strcpy(buf, "nco -t c -n 4 -a 0 -h ");                                 \
        sprintf(buf + strlen(buf),"%" PRIu32 "", freq_hz);                     \
        sprintf(buf + strlen(buf)," -m %" PRIu32 "", freq_mhz);                \
        strcat(buf, " -s\r");                                                  \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_nco_ch5freq(const char *data, char *ret) {   \
        double freq;                                                           \
        sscanf(data, "%lf", &freq);                                            \
        uint32_t freq_hz = 0;                                                  \
        uint32_t freq_mhz = 0;                                                 \
                                                                               \
        /* split the frequency into MHz + Hz */                                \
        if (freq < 1000000){                                                   \
            freq_hz = freq;                                                    \
        }                                                                      \
        else {                                                                 \
            freq_mhz = (uint32_t)(freq / 1000000);                             \
            freq_hz = (uint32_t)(freq - freq_mhz*1000000);                     \
        }                                                                      \
                                                                               \
        strcpy(buf, "nco -t c -n 5 -a 0 -h ");                                 \
        sprintf(buf + strlen(buf),"%" PRIu32 "", freq_hz);                     \
        sprintf(buf + strlen(buf)," -m %" PRIu32 "", freq_mhz);                \
        strcat(buf, " -s\r");                                                  \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_gain_ch0atten(const char *data, char *ret) { \
        double atten;                                                          \
        sscanf(data, "%lf", &atten);                                           \
        uint32_t gaincode = 0;                                                 \
                                                                               \
        /* limit min atten (max gain) */                                       \
        if (atten < -6.018 ) {                                                 \
            atten = -6.018;                                                    \
        }                                                                      \
                                                                               \
        /* convert from atten (dB) to gain code (dac register) */              \
        gaincode = (uint32_t)( 2048 * pow( 10, (atten/-20) ));                 \
                                                                               \
        /* send uart command */                                                \
        strcpy(buf, "dac -c 0 -g ");                                           \
        sprintf(buf + strlen(buf),"%" PRIu32 "", gaincode);                    \
        strcat(buf, "\r");                                                     \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_gain_ch1atten(const char *data, char *ret) { \
        double atten;                                                          \
        sscanf(data, "%lf", &atten);                                           \
        uint32_t gaincode = 0;                                                 \
                                                                               \
        /* limit min atten (max gain) */                                       \
        if (atten < -6.018 ) {                                                 \
            atten = -6.018;                                                    \
        }                                                                      \
                                                                               \
        /* convert from atten (dB) to gain code (dac register) */              \
        gaincode = (uint32_t)( 2048 * pow( 10, (atten/-20) ));                 \
                                                                               \
        /* send uart command */                                                \
        strcpy(buf, "dac -c 1 -g ");                                           \
        sprintf(buf + strlen(buf),"%" PRIu32 "", gaincode);                    \
        strcat(buf, "\r");                                                     \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_gain_ch2atten(const char *data, char *ret) { \
        double atten;                                                          \
        sscanf(data, "%lf", &atten);                                           \
        uint32_t gaincode = 0;                                                 \
                                                                               \
        /* limit min atten (max gain) */                                       \
        if (atten < -6.018 ) {                                                 \
            atten = -6.018;                                                    \
        }                                                                      \
                                                                               \
        /* convert from atten (dB) to gain code (dac register) */              \
        gaincode = (uint32_t)( 2048 * pow( 10, (atten/-20) ));                 \
                                                                               \
        /* send uart command */                                                \
        strcpy(buf, "dac -c 2 -g ");                                           \
        sprintf(buf + strlen(buf),"%" PRIu32 "", gaincode);                    \
        strcat(buf, "\r");                                                     \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_gain_ch3atten(const char *data, char *ret) { \
        double atten;                                                          \
        sscanf(data, "%lf", &atten);                                           \
        uint32_t gaincode = 0;                                                 \
                                                                               \
        /* limit min atten (max gain) */                                       \
        if (atten < -6.018 ) {                                                 \
            atten = -6.018;                                                    \
        }                                                                      \
                                                                               \
        /* convert from atten (dB) to gain code (dac register) */              \
        gaincode = (uint32_t)( 2048 * pow( 10, (atten/-20) ));                 \
                                                                               \
        /* send uart command */                                                \
        strcpy(buf, "dac -c 3 -g ");                                           \
        sprintf(buf + strlen(buf),"%" PRIu32 "", gaincode);                    \
        strcat(buf, "\r");                                                     \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_gain_ch4atten(const char *data, char *ret) { \
        double atten;                                                          \
        sscanf(data, "%lf", &atten);                                           \
        uint32_t gaincode = 0;                                                 \
                                                                               \
        /* limit min atten (max gain) */                                       \
        if (atten < -6.018 ) {                                                 \
            atten = -6.018;                                                    \
        }                                                                      \
                                                                               \
        /* convert from atten (dB) to gain code (dac register) */              \
        gaincode = (uint32_t)( 2048 * pow( 10, (atten/-20) ));                 \
                                                                               \
        /* send uart command */                                                \
        strcpy(buf, "dac -c 4 -g ");                                           \
        sprintf(buf + strlen(buf),"%" PRIu32 "", gaincode);                    \
        strcat(buf, "\r");                                                     \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dac_gain_ch5atten(const char *data, char *ret) { \
        double atten;                                                          \
        sscanf(data, "%lf", &atten);                                           \
        uint32_t gaincode = 0;                                                 \
                                                                               \
        /* limit min atten (max gain) */                                       \
        if (atten < -6.018 ) {                                                 \
            atten = -6.018;                                                    \
        }                                                                      \
                                                                               \
        /* convert from atten (dB) to gain code (dac register) */              \
        gaincode = (uint32_t)( 2048 * pow( 10, (atten/-20) ));                 \
                                                                               \
        /* send uart command */                                                \
        strcpy(buf, "dac -c 5 -g ");                                           \
        sprintf(buf + strlen(buf),"%" PRIu32 "", gaincode);                    \
        strcat(buf, "\r");                                                     \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
                                                                               \
    static int hdlr_tx_##ch##_rf_dac_temp(const char *data, char *ret) {       \
        strcpy(buf, "board -c " STR(ch) " -t\r");                              \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_lo_freq(const char *data, char *ret) {        \
        uint64_t freq = 0;                                                     \
        sscanf(data, "%" SCNd64 "", &freq);                                    \
                                                                               \
        /* if freq = 0, do nothing */                                          \
        if (freq == 0) {                                                       \
          /* Don't mute channel as FPGA/DAC-CP/DAC-MDP tuning might be used*/  \
          /* strcpy(buf, "rf -c " STR(ch) " -z\r");                        */  \
          /* ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));       */  \
                                                                               \
            return RETURN_SUCCESS;                                             \
        }                                                                      \
                                                                               \
        /* if freq out of bounds, kill channel*/                               \
        if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {        \
            strcpy(buf, "board -c " STR(ch) " -k\r");                          \
            ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));            \
                                                                               \
            /* Turn OFF TX on HPS */                                           \
            uint32_t old_val;                                                  \
                                                                               \
            /* disable DSP cores */                                            \
            read_hps_reg("tx" STR_TX(ctx) "4", &old_val);                          \
            PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__,                   \
                  toupper(CHR(ch)));                                           \
            write_hps_reg("tx" STR_TX(ctx) "4", old_val | 0x2);                    \
                                                                               \
            /* disable channel */                                              \
            read_hps_reg("tx" STR_TX(ctx) "4", &old_val);                          \
            write_hps_reg("tx" STR_TX(ctx) "4", old_val &(~0x100));                \
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
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        /* TODO: pll1.power setting TBD (need to modify pllparam_t) */         \
                                                                               \
        /* Send Parameters over to the MCU */                                  \
        set_pll_frequency(uart_tx_fd[INT_TX(ch)], (uint64_t)PLL_CORE_REF_FREQ_HZ_LMX2595, \
                          &pll, true, INT_TX(ch));                                \
                                                                               \
        sprintf(ret, "%Lf", outfreq);                                          \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_band(const char *data, char *ret) {           \
        /* if the setting is a valid band, send to tx board*/                  \
        int band;                                                              \
        sscanf(data, "%i", &band);                                             \
        if ((band == 0) || (band == 1) || (band == 9)) {                       \
            strcpy(buf, "rf -b ");                                             \
            sprintf(buf + strlen(buf),"%i", band);                             \
            strcat(buf, "\r");                                                 \
        }                                                                      \
        else {  /* otherwise mute the tx board */                              \
            strcpy(buf, "rf -z\r");                                            \
        }                                                                      \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_gain_val(const char *data, char *ret) {       \
        double gain;\
        sscanf(data, "%lf", &gain);\
        if(gain>MAX_RF_GAIN_TX) {\
            gain = MAX_RF_GAIN_TX;\
        }\
        else if (gain<MIN_RF_GAIN_TX) {\
            gain = MIN_RF_GAIN_TX;\
        }\
        double atten = (((gain)-MIN_RF_GAIN_TX)/(MAX_RF_GAIN_TX-MIN_RF_GAIN_TX)) * (MIN_RF_ATTEN_TX - MAX_RF_ATTEN_TX) + MAX_RF_ATTEN_TX;\
        char s_atten[25];\
        \
        snprintf(s_atten, 25, "%f", atten);\
        set_property("tx/" STR(ch) "/rf/atten", s_atten);\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
										\
    static int hdlr_tx_##ch##_rf_atten(const char *data, char *ret) {		\
	    uint16_t atten;							\
	    sscanf(data, "%hu", &atten);						\
	    strcpy(buf, "rf -a ");						\
	    sprintf(buf + strlen(buf),"%u", atten);				\
	    strcat(buf, "\r");							\
	    ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));		\
										\
	    return RETURN_SUCCESS;						\
    }										\
                                                                               \
    static int hdlr_tx_##ch##_rf_board_dump(const char *data, char *ret) {     \
        /* send the uart commands and read back the output and write to file   \
         */                                                                    \
                                                                               \
        /* DAC */                                                              \
        strcpy(buf, "dump -c " STR(ch) " -d\r");                               \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);             \
                                                                               \
        /* GPIOX */                                                            \
        strcpy(buf, "dump -c " STR(ch) " -g\r");                               \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);           \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_board_test(const char *data, char *ret) {     \
        /* TODO: MCU code cleanup */                                           \
        PRINT(INFO, "board/clean: MCU code cleanup");\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_board_temp(const char *data, char *ret) {     \
        strcpy(buf, "board -c " STR(ch) " -t\r");                              \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_status_rfld(const char *data, char *ret) {       \
        strcpy(buf, "status -c " STR(ch) " -l\r");                             \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_status_dacld(const char *data, char *ret) {      \
        strcpy(buf, "status -c " STR(ch) " -p\r");                             \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_board_led(const char *data, char *ret) {      \
        strcpy(buf, "board -l\r");                                             \
        strcat(buf, data);                                                     \
        strcat(buf, "\r");                                                     \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_gain(const char *data, char *ret) {          \
        /* TODO: FW code */                                                    \
        PRINT(INFO,"Dsp gain; FW code");\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_rate(const char *data, char *ret) {          \
        uint32_t old_val;                                                      \
        uint16_t base_factor, resamp_factor;                                   \
        double base_err = 0.0, resamp_err = 0.0;                               \
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
        int channel = INT_TX(ch);                                                 \
        char reg = 'a' + (channel/4)*4;                                        \
        int shift = (channel%4)*8;                                             \
        char reg_name[5];                                                      \
        sprintf(reg_name, "txg%c", reg);                                       \
      /* Disable resampler configuration by setting following to false */      \
      /*  if (resamp_err < base_err) {                                  */     \
        if ( false ) {                                                         \
            write_hps_reg("tx" STR(ch) "1", resamp_factor);                    \
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                          \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val | (1 << 15));              \
            sprintf(ret, "%lf",                                                \
                    RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));         \
            /* Set gain adjustment */                                          \
            read_hps_reg(reg_name, &old_val);                                  \
            write_hps_reg(reg_name,                                            \
                          (old_val & ~(0xff << shift)) |                       \
                              (interp_gain_lut[(resamp_factor)] << shift));    \
        } else {                                                               \
            write_hps_reg("tx" STR(ch) "1", base_factor);                      \
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                          \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val & ~(1 << 15));             \
            sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1)); \
            /* Set gain adjustment */                                          \
            read_hps_reg(reg_name, &old_val);                                  \
            write_hps_reg(reg_name,                                            \
                          (old_val & ~(0xff << shift)) |                       \
                              (interp_gain_lut[(base_factor)] << shift));      \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_ch0fpga_nco(const char *data, char *ret) {   \
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
        write_hps_reg("tx" STR(ch) "10", nco_steps);                           \
        if (direction > 0) {                                                   \
            sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);           \
        } else {                                                               \
            sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);            \
        }                                                                      \
                                                                               \
        /* write direction */                                                  \
        read_hps_reg(tx_reg4_map[INT(ch)], &old_val);                             \
        write_hps_reg(tx_reg4_map[INT(ch)],                                       \
                      (old_val & ~(0x1 << 13)) | (direction << 13));             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_ch1fpga_nco(const char *data, char *ret) {   \
        /*Only ch0 is planned on being used at this point, this has not been modified*/\
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
        write_hps_reg("tx" STR(ch) "11", nco_steps);                           \
        if (direction > 0) {                                                   \
            sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);           \
        } else {                                                               \
            sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);            \
        }                                                                      \
                                                                               \
        /* write direction */                                                  \
        read_hps_reg(tx_reg4_map[INT(ch)], &old_val);                             \
        write_hps_reg("tx" STR(ch) "14",                                       \
                      (old_val & ~(0x1 << 1)) | (direction << 1));             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_ch2fpga_nco(const char *data, char *ret) {   \
        /* CH2 CURRENTLY UNSUPPORTED */                                        \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_ch3fpga_nco(const char *data, char *ret) {   \
        /*Only ch0 is planned on being used at this point, this has not been modified*/\
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
        write_hps_reg("tx" STR(ch) "12", nco_steps);                           \
        if (direction > 0) {                                                   \
            sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);           \
        } else {                                                               \
            sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);            \
        }                                                                      \
                                                                               \
        /* write direction */                                                  \
        read_hps_reg("tx" STR(ch) "14", &old_val);                             \
        write_hps_reg("tx" STR(ch) "14",                                       \
                      (old_val & ~(0x1 << 2)) | (direction << 2));             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_ch4fpga_nco(const char *data, char *ret) {   \
        /*Only ch0 is planned on being used at this point, this has not been modified*/\
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
        write_hps_reg("tx" STR_TX(ctx) "13", nco_steps);                           \
        if (direction > 0) {                                                   \
            sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);           \
        } else {                                                               \
            sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);            \
        }                                                                      \
                                                                               \
        /* write direction */                                                  \
        read_hps_reg("tx" STR_TX(ctx) "14", &old_val);                             \
        write_hps_reg("tx" STR_TX(ctx) "14",                                       \
                      (old_val & ~(0x1 << 3)) | (direction << 3));             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
                                                                               \
    static int hdlr_tx_##ch##_dsp_ch5fpga_nco(const char *data, char *ret) {   \
        /*Only ch0 is planned on being used at this point, this has not been modified*/\
        /* CH5 CURRENTLY UNSUPPORTED */                                        \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_rstreq(const char *data, char *ret) {        \
        uint32_t old_val;                                                      \
        read_hps_reg(tx_reg4_map[INT(ch)], &old_val);                              \
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper(CHR(ch)));    \
        write_hps_reg(tx_reg4_map[INT(ch)], old_val | 0x2);                        \
        write_hps_reg(tx_reg4_map[INT(ch)], old_val & ~0x2);                       \
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
        read_hps_reg(tx_reg4_map[INT(ch)], &old_val);                              \
        if (strcmp(data, "1") == 0)                                            \
            write_hps_reg(tx_reg4_map[INT(ch)], old_val | (1 << 14));              \
        else                                                                   \
            write_hps_reg(tx_reg4_map[INT(ch)], old_val & ~(1 << 14));             \
                                                                               \
        /* sync_channels( 15 ); */                                             \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_iface(const char *data, char *ret) {        \
        /* TODO: FW support for streaming to management port required */       \
        /* NOTE: This is strictly for tate 4t*/                                \
        char channel = CHR(ch);                                                \
        sprintf(ret, "%s%c", "sfp", channel);                                  \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_ch0port(const char *data, char *ret) {      \
        uint32_t port;                                                         \
        sscanf(data, "%" SCNd32 "", &port);                                    \
        write_hps_reg("tx" STR(ch) "15", port);                                 \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_ch1port(const char *data, char *ret) {      \
        /*Only ch0 is planned on being used at this point, this has not been verified*/\
        uint32_t port;                                                         \
        sscanf(data, "%" SCNd32 "", &port);                                    \
        write_hps_reg("tx" STR(ch) "16", port);                                 \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_ch2port(const char *data, char *ret) {      \
        /* CH2 CURRENTLY UNSUPPORTED */                                        \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_ch3port(const char *data, char *ret) {      \
        /*Only ch0 is planned on being used at this point, this has not been verified*/\
        uint32_t port;                                                         \
        sscanf(data, "%" SCNd32 "", &port);                                    \
        write_hps_reg("tx" STR(ch) "17", port);                                 \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_ch4port(const char *data, char *ret) {      \
        /*Only ch0 is planned on being used at this point, this has not been verified*/\
        uint32_t port;                                                         \
        sscanf(data, "%" SCNd32 "", &port);                                    \
        write_hps_reg("tx" STR(ch) "18", port);                                 \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_ch5port(const char *data, char *ret) {      \
        /* CH5 CURRENTLY UNSUPPORTED */                                         \
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
     * DOES NOT PORT WELL.                                                     \
     * r04 uses different offsets for channels starting at index 4? */         \
    static int hdlr_tx_##ch##_qa_ch0fifo_lvl(const char *data, char *ret) {    \
        uint32_t lvl;                                                          \
        read_hps_reg("res_ro4", &lvl);                                         \
        lvl &= 0xffff;                                                         \
        sprintf(ret, "%u", lvl);                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * r04 uses different offsets for channels starting at index 4? */         \
    static int hdlr_tx_##ch##_qa_ch1fifo_lvl(const char *data, char *ret) {    \
        uint32_t lvl;                                                          \
        read_hps_reg("res_ro4", &lvl);                                         \
        lvl &= 0xffff;                                                         \
        sprintf(ret, "%u", lvl);                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * r04 uses different offsets for channels starting at index 4? */         \
    static int hdlr_tx_##ch##_qa_ch2fifo_lvl(const char *data, char *ret) {    \
        uint32_t lvl;                                                          \
        read_hps_reg("res_ro4", &lvl);                                         \
        lvl &= 0xffff;                                                         \
        sprintf(ret, "%u", lvl);                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * r04 uses different offsets for channels starting at index 4? */         \
    static int hdlr_tx_##ch##_qa_ch3fifo_lvl(const char *data, char *ret) {    \
        uint32_t lvl;                                                          \
        read_hps_reg("res_ro4", &lvl);                                         \
        lvl &= 0xffff;                                                         \
        sprintf(ret, "%u", lvl);                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * r04 uses different offsets for channels starting at index 4? */         \
    static int hdlr_tx_##ch##_qa_ch4fifo_lvl(const char *data, char *ret) {    \
        uint32_t lvl;                                                          \
        read_hps_reg("res_ro4", &lvl);                                         \
        lvl &= 0xffff;                                                         \
        sprintf(ret, "%u", lvl);                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * r04 uses different offsets for channels starting at index 4? */         \
    static int hdlr_tx_##ch##_qa_ch5fifo_lvl(const char *data, char *ret) {    \
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
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+14;                     \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
       DOES NOT PORT WELL.                                                     \
       flc14 uses different offsets for chanenls starting at index 14? */      \
    static int hdlr_tx_##ch##_qa_ch0oflow(const char *data, char *ret) {       \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+14;                     \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
       DOES NOT PORT WELL.                                                     \
       flc14 uses different offsets for chanenls starting at index 14? */      \
    static int hdlr_tx_##ch##_qa_ch1oflow(const char *data, char *ret) {       \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+14;                     \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
       DOES NOT PORT WELL.                                                     \
       flc14 uses different offsets for chanenls starting at index 14? */      \
    static int hdlr_tx_##ch##_qa_ch2oflow(const char *data, char *ret) {       \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+14;                     \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
       DOES NOT PORT WELL.                                                     \
       flc14 uses different offsets for chanenls starting at index 14? */      \
    static int hdlr_tx_##ch##_qa_ch3oflow(const char *data, char *ret) {       \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+14;                     \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
       DOES NOT PORT WELL.                                                     \
       flc14 uses different offsets for chanenls starting at index 14? */      \
    static int hdlr_tx_##ch##_qa_ch4oflow(const char *data, char *ret) {       \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+14;                     \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
       DOES NOT PORT WELL.                                                     \
       flc14 uses different offsets for chanenls starting at index 14? */      \
    static int hdlr_tx_##ch##_qa_ch5oflow(const char *data, char *ret) {       \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+14;                     \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * flc6 uses different offsets for channels starting at index 6? */        \
    static int hdlr_tx_##ch##_qa_uflow(const char *data, char *ret) {          \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+6;                      \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * flc6 uses different offsets for channels starting at index 6? */        \
    static int hdlr_tx_##ch##_qa_ch0uflow(const char *data, char *ret) {          \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+6;                      \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * flc6 uses different offsets for channels starting at index 6? */        \
    static int hdlr_tx_##ch##_qa_ch1uflow(const char *data, char *ret) {          \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+6;                      \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * flc6 uses different offsets for channels starting at index 6? */        \
    static int hdlr_tx_##ch##_qa_ch2uflow(const char *data, char *ret) {          \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+6;                      \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * flc6 uses different offsets for channels starting at index 6? */        \
    static int hdlr_tx_##ch##_qa_ch3uflow(const char *data, char *ret) {          \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+6;                      \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * flc6 uses different offsets for channels starting at index 6? */        \
    static int hdlr_tx_##ch##_qa_ch4uflow(const char *data, char *ret) {          \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+6;                      \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * flc6 uses different offsets for channels starting at index 6? */        \
    static int hdlr_tx_##ch##_qa_ch5uflow(const char *data, char *ret) {          \
        int flc_reg_num;                                                       \
        char flc_reg[8];                                                       \
        uint32_t count;                                                        \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        flc_reg_num = ((INT_TX(ch)/4)*38)+((INT_TX(ch)%4)*2)+6;                      \
        sprintf(flc_reg, "flc%d", flc_reg_num);                                \
        read_hps_reg(flc_reg, &count);                                         \
        sprintf(ret, "%u", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*Turns the board on or off, and performs none of the other steps in the turn on/off process*/\
    /*pwr must be used to complete the power on process*/\
    static int hdlr_tx_##ch##_pwr_board(const char *data, char *ret) {               \
        uint8_t power;                                                         \
        sscanf(data, "%" SCNd8 "", &power);                                    \
                                                                               \
        char pwr_cmd [40];                                                 \
        if(power>=PWR_ON) {\
            sprintf(pwr_cmd, "rfe_control %d on", INT_TX(ch));                    \
        } else {\
            sprintf(pwr_cmd, "rfe_control %d off", INT_TX(ch));                    \
        }\
        system(pwr_cmd);                                                   \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
        /*Turns the board on or off, and performs none of the other steps in the turn on/off process*/\
    /*pwr must be used to complete the power on process*/\
    /*returns the pid of the powr on process*/\
    static int hdlr_tx_##ch##_async_pwr_board(const char *data, char *ret) {               \
        uint8_t power;                                                         \
        sscanf(data, "%" SCNd8 "", &power);                                    \
                                                                               \
        pid_t pid = fork();\
        if(pid==0) {\
            char rfe_slot[10];                                                 \
            sprintf(rfe_slot, "%i", INT_TX(ch));                    \
            char str_pwr[10];\
            if(power>=PWR_ON) {\
                strcpy(str_pwr, "on");\
            } else {\
                strcpy(str_pwr, "off");\
            }\
            execl("/usr/bin/rfe_control", "rfe_control", rfe_slot, "on", NULL);\
            PRINT(ERROR, "Failed to launch rfe_control in async pwr tx ch: %i", INT(ch));\
            _Exit(EXIT_ERROR_RFE_CONTROL);\
        } else {\
            sprintf(ret, "%i", pid);\
            time(&tx_async_start_time[INT(ch)]);\
            tx_async_pwr_pid[INT(ch)]=pid;\
        }\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    /*waits for async_pwr_board to finished*/\
    static int hdlr_tx_##ch##_wait_pwr_board(const char *data, char *ret) {               \
        time_t current_time;\
        int8_t finished = -1;\
        int status = 0;\
        if(tx_async_pwr_pid[INT(ch)] <=0) {\
            PRINT(ERROR,"No async pwr to wait for, ch %i\n", INT(ch));\
            return RETURN_ERROR;\
        }\
        do {\
            time(&current_time);\
            finished = waitpid(tx_async_pwr_pid[INT(ch)], &status, WNOHANG);\
        } while(current_time < timeout + tx_async_start_time[INT(ch)] && finished == 0);\
        if (finished == 0) {\
            kill(tx_async_pwr_pid[INT(ch)], SIGTERM);\
            /*collects the stalled pwr_on process*/\
            PRINT(ERROR,"Board %i failed to boot, the slot will not be used\n", INT(ch));\
            waitpid(tx_async_pwr_pid[INT(ch)], &status, 0);\
            tx_power[INT(ch)] = PWR_NO_BOARD;\
            strcpy(ret, "0");\
        } else {\
            tx_power[INT(ch)] = PWR_HALF_ON;\
            PRINT(INFO,"Board %i powered on\n", INT(ch));\
            strcpy(ret, "1");\
        }\
        tx_async_pwr_pid[INT(ch)] = 0;\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    static int hdlr_tx_##ch##_pwr(const char *data, char *ret) {         \
        if(tx_power[INT(ch)] == PWR_NO_BOARD) {\
            /*Technically this should be an error, but it would trigger everytime an unused slot does anything, clogging up error logs*/\
            return RETURN_SUCCESS;\
        }\
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
            if(tx_power[INT(ch)] == PWR_OFF) {\
                /*TODO: change this to use async pwr and timeout*/\
                char pwr_cmd [40];                                                 \
                sprintf(pwr_cmd, "rfe_control %d on n", INT_TX(ch));                    \
                system(pwr_cmd);                                                   \
                                                                               \
                /* board command */           \
                usleep(200000);                                                    \
            }\
                                                                               \
            /* disable dsp channels */                                         \
            for (i = 0; i < (NUM_CHANNELS); i++) {                         \
                read_hps_reg(rx_reg4_map[i], &old_val);                               \
                write_hps_reg(rx_reg4_map[i], old_val & ~0x100);                      \
                read_hps_reg(tx_reg4_map[i], &old_val);                               \
                write_hps_reg(tx_reg4_map[i], old_val & ~0x100);                      \
            }                                                                  \
                                                                               \
            /* send sync pulse */                                              \
            sync_channels(15);                                                 \
                                                                               \
            /* enable active dsp channels, and reset the DSP */                \
            for (i = 0; i < NUM_CHANNELS; i++) {                               \
                if (tx_power[i] == PWR_ON) {                                   \
                    read_hps_reg(tx_reg4_map[i], &old_val);                      \
                    write_hps_reg(tx_reg4_map[i], old_val | 0x100);              \
                    read_hps_reg(tx_reg4_map[i], &old_val);                      \
                    write_hps_reg(tx_reg4_map[i], old_val | 0x2);                \
                    write_hps_reg(tx_reg4_map[i], old_val &(~0x2));              \
                }                                                              \
                if (rx_power[i] == PWR_ON) {                                   \
                    read_hps_reg(rx_reg4_map[i], &old_val);                           \
                    write_hps_reg(rx_reg4_map[i], old_val | 0x100);                   \
                    read_hps_reg(rx_reg4_map[i], &old_val);                           \
                    write_hps_reg(rx_reg4_map[i], old_val | 0x2);                     \
                    write_hps_reg(rx_reg4_map[i], old_val &(~0x2));                   \
                }                                                              \
            }                                                                  \
                                                                               \
            tx_power[INT(ch)] = PWR_ON;\
            /* power off */                                                    \
        } else {                                                               \
            char pwr_cmd [40];                                                 \
            sprintf(pwr_cmd, "rfe_control %d off", INT_TX(ch));                   \
            system(pwr_cmd);                                                   \
            \
            tx_power[INT(ch)] = PWR_OFF;                                       \
            \
            /* kill the channel */                                             \
            strcpy(buf, "board -c " STR(ch) " -k\r");                          \
            ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));            \
                                                                               \
            /* disable DSP cores */                                            \
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                          \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val | 0x2);                    \
                                                                               \
            /* disable channel */                                              \
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                          \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val &(~0x100));                \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_reboot(const char *data, char *ret) {            \
        int reboot;                                                            \
        sscanf(data, "%i", &reboot);                                           \
                                                                               \
        if (reboot == 1) {                                                     \
            strcpy(buf, "board -r\r");                                         \
            ping_write_only_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)); \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_jesd_status(const char *data, char *ret) {       \
        strcpy(buf, "status -g\r");                                            \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_serial(const char *data, char *ret) {      \
        strcpy(buf, "status -s\r");                                            \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_mcudevid(const char *data, char *ret) {    \
        strcpy(buf, "status -d\r");                                            \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_mcurev(const char *data, char *ret) {      \
        strcpy(buf, "status -v\r");                                            \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_mcufuses(const char *data, char *ret) {    \
        strcpy(buf, "status -f\r");                                            \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_fw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -v\r");                                             \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }
CHANNELS
#undef X

/* -------------------------------------------------------------------------- */
/* --------------------------------- RX ------------------------------------- */
/* -------------------------------------------------------------------------- */

#define X(ch, io, crx, ctx)                                                               \
    static int hdlr_rx_##ch##_rf_freq_val(const char *data, char *ret) {        \
        uint64_t freq = 0;                                                      \
        sscanf(data, "%" SCNd64 "", &freq);                                     \
        char fullpath[200] = "/var/cyan/state/rx/" STR(ch) "/rf/freq/band";     \
        int band;                                                               \
        char band_read[3];                                                      \
                                                                                \
        /* if freq = 0, mute PLL */                                             \
        if (freq == 0) {                                                        \
            strcpy(buf, "lmx -k\r");                                            \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));          \
            sprintf(ret, "%i", 0);                                             \
            return RETURN_SUCCESS;                                              \
        }                                                                       \
                                                                                \
        /* if freq out of bounds, mute lmx*/                                    \
        if ((freq < LMX2595_RFOUT_MIN_HZ) || (freq > LMX2595_RFOUT_MAX_HZ)) {   \
            strcpy(buf, "lmx -k\r");                                            \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));          \
            PRINT(ERROR,"LMX Freq Invalid \n");                                 \
            sprintf(ret, "%i", 0);                                             \
            return RETURN_ERROR;                                                \
        }                                                                       \
                                                                                \
        /* check band: if HB, subtract freq to account for cascaded mixers*/    \
        get_property(&fullpath,&band_read,3);                                   \
        sscanf(band_read, "%i", &band);                                         \
        if (band == 2) {                                                        \
            freq -= HB_STAGE2_MIXER_FREQ;                                       \
        }                                                                       \
                                                                                \
        /* run the pll calc algorithm */                                        \
        pllparam_t pll = pll_def_lmx2595;                                       \
        long double outfreq = 0;                                                \
        outfreq = setFreq(&freq, &pll);                                         \
                                                                                \
        while ((pll.N < pll.n_min) && (pll.R < pll.r_max)) {                    \
            pll.R = pll.R + 1;                                                  \
            outfreq = setFreq(&freq, &pll);                                     \
        }                                                                       \
                                                                                \
        /* Send Parameters over to the MCU */                                   \
        set_lo_frequency(uart_rx_fd[INT_RX(ch)], (uint64_t)PLL_CORE_REF_FREQ_HZ_LMX2595, &pll);  \
                                                                                \
        /* if HB add back in freq before printing value to state tree */        \
        if (band == 2) {                                                        \
            outfreq += HB_STAGE2_MIXER_FREQ;                                    \
        }                                                                       \
        /* Save the frequency that is being set into the property */            \
        sprintf(ret, "%Lf", outfreq);                                           \
                                                                                \
        return RETURN_SUCCESS;                                                  \
    }                                                                           \
                                                                                                                                                         \
                                                                               \
    static int hdlr_rx_##ch##_rf_freq_lna(const char *data, char *ret) {       \
        strcpy(buf, "rf -l ");                                  \
        strcat(buf, data);                                                     \
        strcat(buf, "\r");                                                     \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_freq_band(const char *data, char *ret) {      \
        strcpy(buf, "rf -b ");                                  \
        strcat(buf, data);                                                     \
        strcat(buf, "\r");                                                     \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    static int hdlr_rx_##ch##_rf_freq_common_lo(const char *data, char *ret) {      \
        /*Not Yet Implmented in MCU*/\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*sets variable amplifiers, variable attentuators, bypassable amplifiers to achieve desired gain*/\
    /*Note: this sets it bassed on the current band, any time the band is changed, this must be updated*/\
    static int hdlr_rx_##ch##_rf_gain_val(const char *data, char *ret) {       \
        const char fullpath[200] = "/var/cyan/state/rx/" STR(ch) "/rf/freq/band";    \
        int gain;                                                              \
        int net_gain;                                                          \
        int atten;                                                             \
        int band;                                                              \
        char band_read[3];                                                     \
                                                                               \
        sscanf(data, "%i", &gain);                                             \
        get_property(&fullpath[0],&band_read[0],3);                                  \
        sscanf(band_read, "%i", &band);                                        \
                                                                               \
        if (band == 0) {                                                       \
            gain-= RX_LOW_GAIN_OFFSET;\
            /*LMH6401 Gain Range: -6dB to 26dB*/                               \
            if (gain > 26) {                                                   \
                gain = 26;                                                     \
            } else if (gain < -6) {                                            \
                gain = -6;                                                     \
            }                                                                  \
            atten = 26 - gain;                                                 \
            strcpy(buf, "vga -a ");                                            \
            sprintf(buf + strlen(buf), "%i", atten);                           \
            strcat(buf, "\r");                                                 \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));         \
            net_gain = gain;\
            net_gain = gain + RX_LOW_GAIN_OFFSET;\
        } else if (band == 1) {                                                \
            gain-= RX_MID_GAIN_OFFSET;\
            /*lna_bypass means bypass the lna (AM1081) */                      \
            uint8_t lna_bypass;\
            if (gain <= LTC5586_MIN_GAIN) {\
                lna_bypass = 1;\
                atten = LTC5586_MIN_GAIN - gain;\
                gain = LTC5586_MIN_GAIN;\
            } else if (gain <= LTC5586_MAX_GAIN) {\
                lna_bypass = 1;\
                atten = 0;\
            } else if (gain <= AM1081_GAIN + LTC5586_MIN_GAIN) {\
                lna_bypass = 0;\
                atten = AM1081_GAIN + LTC5586_MIN_GAIN - gain;\
                gain = LTC5586_MIN_GAIN;\
            } else if (gain <= AM1081_GAIN + LTC5586_MAX_GAIN) {\
                lna_bypass = 0;\
                atten = 0;\
                gain = gain - AM1081_GAIN;\
            } else {\
                lna_bypass = 0;\
                gain = LTC5586_MAX_GAIN;\
                atten = 0;\
            }\
            \
            /*Sets the property to enable/disable bypassing the fixed amplifier*/\
            char s_lna[5];\
            snprintf(s_lna, 5, "%u", lna_bypass);\
            printf(s_lna);\
            set_property("rx/" STR(ch) "/rf/freq/lna", s_lna);\
            \
            /*Sets gain on the rx board*/\
            strcpy(buf, "rf -g ");                                         \
            sprintf(buf + strlen(buf), "%i", gain);                        \
            strcat(buf, "\r");                                             \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            \
            /*Sets the state tree property to handle the attenuation*/\
            if(atten < LTC5586_MIN_ATTEN) {\
                atten = LTC5586_MIN_ATTEN;\
            } else if(atten > LTC5586_MAX_ATTEN) {\
                atten = LTC5586_MAX_ATTEN;\
            }\
            char s_atten[25];\
            snprintf(s_atten, 25, "%u", atten);\
            set_property("rx/" STR(ch) "/rf/atten/val", s_atten);\
            net_gain = gain + ((1-lna_bypass) * AM1081_GAIN) - atten + RX_MID_GAIN_OFFSET;\
        } else if (band == 2) {                                                \
            gain-= RX_HIGH_GAIN_OFFSET;\
            uint8_t lna_bypass;\
            if (gain <= LTC5586_MIN_GAIN) {\
                lna_bypass = 1;\
                atten = LTC5586_MIN_GAIN - gain;\
                gain = LTC5586_MIN_GAIN;\
            } else if (gain <= LTC5586_MAX_GAIN) {\
                lna_bypass = 1;\
                atten = 0;\
            } else if (gain <= AM1075_GAIN + LTC5586_MIN_GAIN) {\
                lna_bypass = 0;\
                atten = AM1075_GAIN + LTC5586_MIN_GAIN - gain;\
                gain = LTC5586_MIN_GAIN;\
            } else if (gain <= AM1075_GAIN + LTC5586_MAX_GAIN) {\
                lna_bypass = 0;\
                atten = 0;\
                gain = gain - AM1075_GAIN;\
            } else {\
                lna_bypass = 0;\
                gain = LTC5586_MAX_GAIN;\
                atten = 0;\
            }\
            \
            /*Sets the property to enable/disable bypassing the fixed amplifier*/\
            char s_lna[5];\
            snprintf(s_lna, 5, "%u", lna_bypass);\
            printf(s_lna);\
            set_property("rx/" STR(ch) "/rf/freq/lna", s_lna);\
            \
            /*Sets gain on the rx board*/\
            strcpy(buf, "rf -g ");                                         \
            sprintf(buf + strlen(buf), "%i", gain);                        \
            strcat(buf, "\r");                                             \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            /*Sets the state tree property to handle the attenuation*/\
            if(atten < LTC5586_MIN_ATTEN) {\
                atten = LTC5586_MIN_ATTEN;\
            } else if(atten > LTC5586_MAX_ATTEN) {\
                atten = LTC5586_MAX_ATTEN;\
            }\
            char s_atten[25];\
            snprintf(s_atten, 25, "%u", atten);\
            set_property("rx/" STR(ch) "/rf/atten/val", s_atten);\
            net_gain = gain + ((1-lna_bypass) * AM1075_GAIN) - atten + RX_HIGH_GAIN_OFFSET;\
                                                                            \
        } else {                                                               \
            PRINT(ERROR,"band unexpected value while setting gain\n");         \
            return RETURN_ERROR_GET_PROP;                                      \
        }                                                                      \
                                                                               \
        sprintf(ret, "%i", net_gain);                                              \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_atten_val(const char *data, char *ret) {      \
        /*LTC5586 Atten Range: 0dB to 31dB*/                                   \
        int atten;                                                             \
        sscanf(data, "%i", &atten);                                            \
                                                                               \
        if (atten > 31)                                                        \
            atten = 31;                                                        \
        else if (atten < 0)                                                    \
            atten = 0;                                                         \
                                                                               \
        strcpy(buf, "rf -a ");                                                 \
        sprintf(buf + strlen(buf), "%i", atten);                               \
        strcat(buf, "\r");                                                     \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));             \
                                                                               \
        sprintf(ret, "%i", atten);                                             \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_board_dump(const char *data, char *ret) {     \
        /* send the uart commands and read back the output and write to file   \
         */                                                                    \
                                                                               \
        /* ADC */                                                              \
        strcpy(buf, "dump -a\r");                               \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);             \
                                                                               \
        /* GPIOX */                                                            \
        strcpy(buf, "dump -g\r");                               \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);           \
                                                                               \
        /* ADC Driver */                                                       \
        strcpy(buf, "dump -v\r");                               \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
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
        /*Not available*/                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_status_rfld(const char *data, char *ret) {       \
        strcpy(buf, "status -l\r");                             \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_status_adcalarm(const char *data, char *ret) {   \
        /*Not properly implemented in MCU code*/                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_board_led(const char *data, char *ret) {      \
        strcpy(buf, "board -l\r");                                             \
        strcat(buf, data);                                                     \
        strcat(buf, "\r");                                                     \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_signed(const char *data, char *ret) {        \
        uint32_t old_val, sign;                                                \
        sscanf(data, "%u", &sign);                                             \
        sign = sign ? 0 : 1;                                                   \
                                                                               \
        read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                              \
        old_val &= ~(1 << 4);                                                  \
        write_hps_reg(rx_reg4_map[INT(ch)], old_val | (sign << 4));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_gain(const char *data, char *ret) {          \
        /* TODO: FW code */ \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_rate(const char *data, char *ret) {          \
        uint32_t old_val;                                                      \
        uint16_t base_factor, resamp_factor;                                   \
        double base_err = 0.0, resamp_err = 0.0;                               \
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
        char reg = 'a' + (INT(ch)/4)*4;                                        \
        int shift = (INT(ch)%4)*8;                                             \
        char reg_name[5];                                                      \
        sprintf(reg_name, "rxg%c", reg);                                       \
                                                                               \
        /*if (resamp_err < base_err) {*/\
        if (false){     \
            write_hps_reg("rx" STR(ch) "1", resamp_factor);                    \
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                          \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val | (1 << 15));              \
            sprintf(ret, "%lf",                                                \
                    RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));         \
            /*Set gain adjustment */                                           \
            gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;       \
            read_hps_reg(reg_name, &old_val);                                  \
            write_hps_reg(reg_name, (old_val & ~(0xff << shift)) |             \
                                    (((uint16_t)gain_factor) << shift));       \
        } else {                                                               \
            write_hps_reg("rx" STR(ch) "1", base_factor);                      \
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);             \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val & ~(1 << 15));\
            sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1)); \
            /*Set gain adjustment*/                                            \
            gain_factor = decim_gain_lut[(base_factor)];                       \
            read_hps_reg(reg_name, &old_val);                                  \
            write_hps_reg(reg_name, (old_val & ~(0xff << shift)) |             \
                                    (((uint16_t)gain_factor) << shift));       \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_fpga_nco(const char *data, char *ret) {       \
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
        read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                              \
        write_hps_reg(rx_reg4_map[INT(ch)],                                        \
                      (old_val & ~(0x1 << 13)) | (direction << 13));           \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_rstreq(const char *data, char *ret) {        \
        uint32_t old_val;                                                      \
        read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                              \
        write_hps_reg(rx_reg4_map[INT(ch)], old_val | 0x2);                        \
        write_hps_reg(rx_reg4_map[INT(ch)], old_val & ~0x2);                       \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_loopback(const char *data, char *ret) {      \
        uint32_t old_val;                                                      \
        read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                              \
        if (strcmp(data, "1") == 0)                                            \
            write_hps_reg(rx_reg4_map[INT(ch)], (old_val & ~0x1e00) | 0x400);      \
        else                                                                   \
            write_hps_reg(rx_reg4_map[INT(ch)], (old_val & ~0x1e00) | 0x000);      \
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
        read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                              \
        if (strcmp(data, "1") == 0)                                            \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val | (1 << 14));              \
        else                                                                   \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val & ~(1 << 14));             \
                                                                               \
        /*sync_channels( 15 ); */                                              \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_link_iface(const char *data, char *ret) {        \
        /* TODO: FW support for streaming to management port required */       \
        /* NOTE: This is strictly for tate 4r*/                                \
        char channel = CHR(ch);                                                \
        sprintf(ret, "%s%c", "sfp", channel);                                  \
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
    static int hdlr_rx_##ch##_force_stream(const char *data, char *ret) {     \
        /*Forces rx to start sreaming data, only use if the conventional method using the sfp port is not possible*/\
        if(data[0]=='0') {\
            write_hps_reg(rx_reg4_map[INT(ch)], 0x6002);\
            rx_stream[INT(ch)] = STREAM_OFF;\
        }else {\
            write_hps_reg(rx_reg4_map[INT(ch)], 0x2100);\
            rx_stream[INT(ch)] = STREAM_ON;\
        }\
        return RETURN_SUCCESS;                                                 \
    } \
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
                read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                         \
                write_hps_reg(rx_reg4_map[INT(ch)], old_val | 0x100);                 \
                                                                               \
                read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                         \
                write_hps_reg(rx_reg4_map[INT(ch)], old_val | 0x2);                   \
                write_hps_reg(rx_reg4_map[INT(ch)], old_val &(~0x2));                 \
                                                                               \
                rx_stream[INT(ch)] = STREAM_ON;                                \
            } else {                                                           \
                /* Do not turn ON stream if channel is OFF */                  \
                sprintf(ret, "%u", 0); /* Alert File Tree */                   \
            }                                                                  \
        } else { /* TURN THE STREAM OFF */                                     \
            /* disable DSP core */                                             \
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                          \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val | 0x2);                    \
                                                                               \
            /* disable channel */                                              \
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                          \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val &(~0x100));                \
                                                                               \
            rx_stream[INT(ch)] = STREAM_OFF;                                   \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*Turns the board on or off, and performs none of the other steps in the turn on/off process*/\
    /*pwr must be used to complete the power on process*/\
    static int hdlr_rx_##ch##_pwr_board(const char *data, char *ret) {               \
        uint8_t power;                                                         \
        sscanf(data, "%" SCNd8 "", &power);                                    \
                                                                               \
        char pwr_cmd [40];                                                 \
        if(power>=PWR_ON) {\
            sprintf(pwr_cmd, "rfe_control %d on", INT_RX(ch));                    \
        } else {\
            sprintf(pwr_cmd, "rfe_control %d off", INT_RX(ch));                    \
        }\
        system(pwr_cmd);                                                   \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    /*Turns the board on or off, and performs none of the other steps in the turn on/off process*/\
    /*pwr must be used to complete the power on process*/\
    /*returns the pid of the powr on process*/\
    static int hdlr_rx_##ch##_async_pwr_board(const char *data, char *ret) {               \
        uint8_t power;                                                         \
        sscanf(data, "%" SCNd8 "", &power);                                    \
                                                                               \
        pid_t pid = fork();\
        if(pid==0) {\
            char rfe_slot[10];                                                 \
            sprintf(rfe_slot, "%i", INT_RX(ch));                    \
            char str_pwr[10];\
            if(power>=PWR_ON) {\
                strcpy(str_pwr, "on");\
            } else {\
                strcpy(str_pwr, "off");\
            }\
            execl("/usr/bin/rfe_control", "rfe_control", rfe_slot, "on", NULL);\
            PRINT(ERROR, "Failed to launch rfe_control in async pwr ch: %i", INT(ch));\
            _Exit(EXIT_ERROR_RFE_CONTROL);\
        } else {\
            sprintf(ret, "%i", pid);\
            time(&rx_async_start_time[INT(ch)]);\
            rx_async_pwr_pid[INT(ch)]=pid;\
        }\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    /*waits for async_pwr_board to finished*/\
    static int hdlr_rx_##ch##_wait_pwr_board(const char *data, char *ret) {               \
        time_t current_time;\
        int8_t finished = -1;\
        int status = 0;\
        if(rx_async_pwr_pid[INT(ch)] <=0) {\
            PRINT(ERROR,"No async pwr to wait for, ch %i\n", INT(ch));\
            return RETURN_ERROR;\
        }\
        do {\
            time(&current_time);\
            finished = waitpid(rx_async_pwr_pid[INT(ch)], &status, WNOHANG);\
        } while(current_time < timeout + rx_async_start_time[INT(ch)] && finished == 0);\
        if (finished == 0) {\
            kill(rx_async_pwr_pid[INT(ch)], SIGTERM);\
            /*collects the stalled pwr_on process*/\
            waitpid(rx_async_pwr_pid[INT(ch)], &status, 0);\
            PRINT(ERROR,"Board %i failed to boot, the slot will not be used\n", INT(ch));\
            rx_power[INT(ch)] = PWR_NO_BOARD;\
            strcpy(ret, "0");\
        } else {\
            rx_power[INT(ch)] = PWR_HALF_ON;\
            PRINT(INFO,"Board %i powered on\n", INT(ch));\
            strcpy(ret, "1");\
        }\
        rx_async_pwr_pid[INT(ch)] = 0;\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    static int hdlr_rx_##ch##_pwr(const char *data, char *ret) {               \
        if(rx_power[INT(ch)] == PWR_NO_BOARD) {\
            /*Technically this should be an error, but it would trigger everytime an unused slot does anything, clogging up error logs*/\
            return RETURN_SUCCESS;\
        }\
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
            /*Avoids attempting to turn on a  board if its off or already turned on but not initialized*/\
            if(rx_power[INT(ch)] == PWR_OFF) {\
                /*TODO: change this to use async pwr and timeout*/\
                char pwr_cmd [40];                                                 \
                sprintf(pwr_cmd, "rfe_control %d on n", INT_RX(ch));                    \
                system(pwr_cmd);                                                   \
                                                                               \
                /* board command */           \
                usleep(200000);                                                    \
            }\
                                                                               \
            /* disable dsp channels */                                         \
            for (i = 0; i < NUM_CHANNELS; i++) {                         \
                read_hps_reg(rx_reg4_map[i], &old_val);                               \
                write_hps_reg(rx_reg4_map[i], old_val & ~0x100);                      \
            }                                                                  \
            /*temporary disables tx dsp channels*/\
            for (i = 0; i < NUM_CHANNELS; i++) {                         \
                read_hps_reg(tx_reg4_map[i], &old_val);                               \
                write_hps_reg(tx_reg4_map[i], old_val & ~0x100);                      \
            }                                                                  \
                                                                               \
            /* send sync pulse */                                              \
            sync_channels(15);                                                 \
                                                                               \
            /* Enable active dsp channels, and reset DSP */                    \
            for (i = 0; i < NUM_CHANNELS; i++) {                               \
                /*temporarily disabled because its causeing issue with getting rx working*/\
                /*if (tx_power[i] == PWR_ON) {                                   \
                    read_hps_reg(tx_reg4_map[i], &old_val);                      \
                    write_hps_reg(tx_reg4_map[i], old_val | 0x100);              \
                    read_hps_reg(tx_reg4_map[i], &old_val);                      \
                    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__,           \
                          toupper(CHR(ch)));                                   \
                    write_hps_reg(tx_reg4_map[i], old_val | 0x2);                \
                    write_hps_reg(tx_reg4_map[i], old_val &(~0x2));              \
                }*/                                                              \
                if (rx_stream[i] == PWR_ON) {                               \
                    read_hps_reg(rx_reg4_map[i], &old_val);                           \
                    write_hps_reg(rx_reg4_map[i], old_val | 0x100);                   \
                    read_hps_reg(rx_reg4_map[i], &old_val);                           \
                    write_hps_reg(rx_reg4_map[i], old_val | 0x2);                     \
                    write_hps_reg(rx_reg4_map[i], old_val &(~0x2));                   \
                }                                                              \
            }                                                                  \
            rx_power[INT(ch)] = PWR_ON;\
            /* power off & stream off */                                       \
        } else {                                                               \
            char pwr_cmd [40];                                                 \
            sprintf(pwr_cmd, "rfe_control %d off", INT_RX(ch));                   \
            /*system(pwr_cmd);*/                                                   \
                                                                               \
            rx_power[INT(ch)] = PWR_OFF;                                       \
            rx_stream[INT(ch)] = STREAM_OFF;                                   \
                                                                               \
            /* kill the channel */                                             \
            /*strcpy(buf, "board -c " STR(ch) " -k\r");                   */       \
            /*ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));  */          \
                                                                               \
            /* disable DSP core */                                             \
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                          \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val | 0x2);                    \
                                                                               \
            /* disable channel */                                              \
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                          \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val &(~0x100));                \
        }                                                                      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_reboot(const char *data, char *ret) {            \
        int reboot;                                                            \
        sscanf(data, "%i", &reboot);                                           \
                                                                               \
        if (reboot == 1) {                                                     \
            strcpy(buf, "board -r\r");                                         \
            ping_write_only_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)); \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_serial(const char *data, char *ret) {      \
        strcpy(buf, "status -s\r");                                            \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_mcudevid(const char *data, char *ret) {    \
        strcpy(buf, "status -d\r");                                            \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_mcurev(const char *data, char *ret) {      \
        strcpy(buf, "status -v\r");                                            \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_mcufuses(const char *data, char *ret) {    \
        strcpy(buf, "status -f\r");                                            \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_fw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -v\r");                                             \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_jesd_status(const char *data, char *ret) {       \
        /* res_ro11 holds link data with bit 0 high indicating rx board 0 */   \
        /* link is up, bit 1 high indicating that rx board link 1 is up, etc */\
        uint32_t reg_val = 0;                                                  \
        read_hps_reg("res_ro11", &reg_val);                                    \
        uint8_t shift = (int)(CHR(ch) - 'a');                                  \
        uint32_t anded = reg_val & (1 << shift);                               \
        if (anded > 0){                                                        \
            strcpy(ret, "good");                                               \
        } else {                                                               \
            strcpy(ret, "bad");                                                \
        }                                                                      \
        return RETURN_SUCCESS;                                                 \
    }
CHANNELS
#undef X

#define X(ch, io, crx, ctx)                                                              \
    static int hdlr_tx_##ch##_trigger_gating(const char *data, char *ret) {    \
        int r;                                                                 \
        bool val;                                                              \
        r = valid_gating_mode(data, &val) || set_gating_mode(#ch, val);        \
        return r;                                                              \
    }
CHANNELS
#undef X

/* -------------------------------------------------------------------------- */
/* ------------------------------ CHANNEL MASK ------------------------------ */
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

    mask_rx = cm_chanmask_get("/var/cyan/state/cm/chanmask-rx");

    sscanf(data, "%d", &atten);

    sprintf(inbuf, "%d", atten);

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }
#define X(ch, io, crx, ctx)                                                              \
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

    mask_rx = cm_chanmask_get("/var/cyan/state/cm/chanmask-rx");

    sscanf(data, "%lf", &gain);

    sprintf(inbuf, "%lf", gain);

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }

#define X(ch, io, crx, ctx)                                                              \
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

    mask_tx = cm_chanmask_get("/var/cyan/state/cm/chanmask-tx");

    sscanf(data, "%lf", &gain);

    sprintf(inbuf, "%lf", gain);

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_tx & (1 << i))) {
            continue;
        }

#define X(ch, io, crx, ctx)                                                              \
    if (i == INT_TX(ch))                                                          \
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

    mask_rx = cm_chanmask_get("/var/cyan/state/cm/chanmask-rx");
    mask_tx = cm_chanmask_get("/var/cyan/state/cm/chanmask-tx");

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

#define X(ch, io, crx, ctx)                                                              \
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

#define X(ch, io, crx, ctx)                                                              \
    if (i == INT_TX(ch))                                                          \
        hdlr = hdlr_tx_##ch##_rf_lo_freq;
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

static int hdlr_cm_trx_fpga_nco(const char *data, char *ret) {
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

    mask_rx = cm_chanmask_get("/var/cyan/state/cm/chanmask-rx");
    mask_tx = cm_chanmask_get("/var/cyan/state/cm/chanmask-tx");

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

#define X(ch, io, crx, ctx)                                                              \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_rx_##ch##_dsp_fpga_nco;
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
#define X(ch, io, crx, ctx)                                                              \
    if (i == INT_TX(ch))                                                          \
        hdlr = hdlr_tx_##ch##_dsp_ch0fpga_nco;                                 \
        hdlr = hdlr_tx_##ch##_dsp_ch1fpga_nco;                                 \
        hdlr = hdlr_tx_##ch##_dsp_ch2fpga_nco;                                 \
        hdlr = hdlr_tx_##ch##_dsp_ch3fpga_nco;                                 \
        hdlr = hdlr_tx_##ch##_dsp_ch4fpga_nco;                                 \
        hdlr = hdlr_tx_##ch##_dsp_ch5fpga_nco;                                 \
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

static int hdlr_time_reboot(const char *data, char *ret) {
        int reboot;
        sscanf(data, "%i", &reboot);

        if (reboot == 1) {
            strcpy(buf, "board -r\r");
            ping_write_only(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf));
        }

        return RETURN_SUCCESS;
    }

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

static int hdlr_time_clk_cmd(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_time_status_good(const char *data, char *ret) {
     strcpy(buf, "status -g\r");
     ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
     strcpy(ret, (char *)uart_ret_buf);

     return RETURN_SUCCESS;
}

#if 0
static int hdlr_time_source_vco(const char *data, char *ret) {
    if (strcmp(data, "external") == 0) {
        strcpy(buf, "clk -v 1\r");
    } else if (strcmp(data, "internal") == 0) {
        strcpy(buf, "clk -v 0\r");
    }
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
#endif

#if 0
static int hdlr_time_source_sync(const char *data, char *ret) {
    if (strcmp(data, "external") == 0) {
        strcpy(buf, "clk -n 1\r");
    } else if (strcmp(data, "internal") == 0) {
        strcpy(buf, "clk -n 0\r");
    }
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
#endif

// 10 MHz clock
static int hdlr_time_source_ref(const char *data, char *ret) {
    if (strcmp(data, "external") == 0) {
        strcpy(buf, "clk -t 1\r");
    } else if (strcmp(data, "internal") == 0) {
        strcpy(buf, "clk -t 0\r");
    }
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

// External Source Buffer Select
static int hdlr_time_source_extsine(const char *data, char *ret) {
    if (strcmp(data, "sine") == 0) {
        strcpy(buf, "HMC -h 1 -b 1\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    } else if (strcmp(data, "LVPECL") == 0) {
        strcpy(buf, "HMC -h 1 -b 0\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    } else {
        strcpy(buf, "HMC -h 1 -B\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        strcpy(ret, (char *)uart_ret_buf);
    }
    return RETURN_SUCCESS;
}

// choose pulsed or continuous SYSREF
static int hdlr_time_sync_sysref_mode(const char *data, char *ret) {
    if ( (strcmp(data, "pulsed") == 0) || (strcmp(data, "0") == 0) ) {
        PRINT(INFO, "SYSREF MODE 'pulsed' selected.\n");
        strcpy(buf, "debug -l 7 -r 139 -w 2\r");
    } else if ( (strcmp(data, "continuous") == 0) || (strcmp(data, "1") == 0) )  {
        PRINT(INFO, "SYSREF MODE 'continuous' selected.\n");
        strcpy(buf, "debug -l 7 -r 139 -w 3\r");
    } else {
        PRINT(ERROR, "SYSREF MODE must be 'continuous' or 'pulsed'.\n");
        return RETURN_ERROR;
    }
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

// Toggle SPI Sync
static int hdlr_time_sync_lmk_sync_tgl_jesd(const char *data, char *ret) {
    if (strcmp(data, "0") != 0) {
        strcpy(buf, "sync -k\r");
    }
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

// Resync output edges with Ref
static int hdlr_time_sync_lmk_resync_jesd(const char *data, char *ret) {
    if (strcmp(data, "0") != 0) {
        strcpy(buf, "sync -j\r");
    }
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

// Resync output edges with Ref
static int hdlr_time_sync_lmk_resync_all(const char *data, char *ret) {
    if (strcmp(data, "0") != 0) {
        strcpy(buf, "sync -r\r");
    }
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

#if 0
// TODO: Enable DevClock Output
static int hdlr_time_source_devclk(const char *data, char *ret) {
    if (strcmp(data, "external") == 0) {
        strcpy(buf, "clk -t 1\r");
    } else if (strcmp(data, "internal") == 0) {
        strcpy(buf, "clk -t 0\r");
    }
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
#endif

#if 0
// TODO: Enable PLL Output
static int hdlr_time_source_pll(const char *data, char *ret) {
    if (strcmp(data, "external") == 0) {
        strcpy(buf, "clk -t 1\r");
    } else if (strcmp(data, "internal") == 0) {
        strcpy(buf, "clk -t 0\r");
    }
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
#endif

static int hdlr_time_status_ld(const char *data, char *ret) {
    // strcpy(buf, "status -l\r");
    // ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    // strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd0_pll1(const char *data, char *ret) {
    strcpy(buf, "status -l 11\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd0_pll2(const char *data, char *ret) {
    strcpy(buf, "status -l 12\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd1_pll1(const char *data, char *ret) {
    strcpy(buf, "status -l 21\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd1_pll2(const char *data, char *ret) {
    strcpy(buf, "status -l 22\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd2_pll1(const char *data, char *ret) {
    strcpy(buf, "status -l 31\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd2_pll2(const char *data, char *ret) {
    strcpy(buf, "status -l 32\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol(const char *data, char *ret) {
    // strcpy(buf, "status -o\r");
    // ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    // strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd0_pll1(const char *data, char *ret) {
    strcpy(buf, "status -o 11\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd0_pll2(const char *data, char *ret) {
    strcpy(buf, "status -o 12\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd1_pll1(const char *data, char *ret) {
    strcpy(buf, "status -o 21\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd1_pll2(const char *data, char *ret) {
    strcpy(buf, "status -o 22\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd2_pll1(const char *data, char *ret) {
    strcpy(buf, "status -o 31\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd2_pll2(const char *data, char *ret) {
    strcpy(buf, "status -o 32\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_board_dump(const char *data, char *ret) {
    // Send the uart commands and read back the output and write to file.
    // Diagnostic Dump of Clk Board
    strcpy(buf, "board -e\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    PRINT(DUMP, "[Board: Time Regdump] %s\n", uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_board_test(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_time_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -t\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_board_led(const char *data, char *ret) {
    strcpy(buf, "board -l ");
    strcat(buf, data);
    strcat(buf, "\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_time_about_id(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_time_about_serial(const char *data, char *ret) {
    strcpy(buf, "status -s\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_mcudevid(const char *data, char *ret) {
    strcpy(buf, "status -d\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_mcurev(const char *data, char *ret) {
    strcpy(buf, "status -v\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_mcufuses(const char *data, char *ret) {
    strcpy(buf, "status -f\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_fw_ver(const char *data, char *ret) {
    strcpy(buf, "board -v\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/* --------------------------------- FPGA ----------------------------------- */
/* -------------------------------------------------------------------------- */

// Dumps all of the board logs for TX, RX, and TIME
static int hdlr_fpga_board_dump(const char *data, char *ret) {
#define X(ch, io, crx, ctx) hdlr_tx_##ch##_rf_board_dump(NULL, NULL);
    CHANNELS
#undef X
#define X(ch, io, crx, ctx) hdlr_rx_##ch##_rf_board_dump(NULL, NULL);
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
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        usleep(50000);

        strcpy(buf, "board -g 1\r");
#define X(ch, io, crx, ctx)                                                              \
    ping(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf)), usleep(50000);
        CHANNELS
#undef X

        strcpy(buf, "board -g 1\r");
#define X(ch, io, crx, ctx)                                                              \
    ping(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf)), usleep(50000);
        CHANNELS
#undef X
    }
    if (strcmp(data, "2") == 0) {
        strcpy(buf, "board -g 2\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        usleep(50000);

        strcpy(buf, "board -g 2\r");
#define X(ch, io, crx, ctx)                                                              \
    ping(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf)), usleep(50000);
        CHANNELS
#undef X

        strcpy(buf, "board -g 2\r");
#define X(ch, io, crx, ctx)                                                              \
    ping(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf)), usleep(50000);
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
        sprintf(ret, "temp +%u degC\n", old_val);
    } else if (old_val < 128) {
        old_val = old_val - 58;
        sprintf(ret, "temp -%u degC\n", old_val);
    }

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_led(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_rstreq_all_dsp(const char *data, char *ret) {
    uint32_t res_rw7;
    // assert reset
    read_hps_reg("res_rw7", &res_rw7);
    res_rw7 = res_rw7 | 0x80000000;
    write_hps_reg("res_rw7", res_rw7);

    // de-assert reset
    read_hps_reg("res_rw7", &res_rw7);
    res_rw7 = res_rw7 & (~0x80000000);
    write_hps_reg("res_rw7", res_rw7);

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
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    usleep(700000);

    strcpy(buf, "board -r\r");
#define X(ch, io, crx, ctx)                                                              \
    ping(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf)), usleep(50000);
    CHANNELS
#undef X

    strcpy(buf, "board -r\r");
#define X(ch, io, crx, ctx)                                                              \
    ping(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf)), usleep(50000);
    CHANNELS
#undef X

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_flow_control_sfpX_port(const char *data, char *ret,
                                                  unsigned sfp_port) {

    static const unsigned udp_port_max = (1 << 16) - 1;
    static const unsigned sfp_port_max = 3;

    unsigned udp_port;
    uint32_t flc_reg;
    char* flc_reg_addr = "flc0";
    uint32_t mask;
    uint32_t shift_amount;

    if (sfp_port > sfp_port_max) {
        return RETURN_ERROR_PARAM;
    }
    if (1 != sscanf(data, "%u", &udp_port)) {
        return RETURN_ERROR_PARAM;
    }

    udp_port = udp_port > udp_port_max ? udp_port_max : udp_port;

    // if number of sfp_ports ever changes, this code needs to be changed
    // a good reason to use structures to access memory-mapped registers.
    flc_reg_addr = (sfp_port < 2) ? "flc0" : "flc38";
    if (sfp_port < 2) {
        shift_amount = (sfp_port * 16);
    } else {
        shift_amount = ((sfp_port-2) * 16);
    }
    mask = 0xffff << shift_amount;
    read_hps_reg(flc_reg_addr, &flc_reg);
    flc_reg &= ~mask;
    flc_reg |= (udp_port << shift_amount) & mask;
    write_hps_reg(flc_reg_addr, flc_reg);

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
static inline int hdlr_fpga_board_flow_control_sfpc_port(const char *data,
                                                         char *ret) {
    return hdlr_fpga_board_flow_control_sfpX_port(data, ret, 2);
}
static inline int hdlr_fpga_board_flow_control_sfpd_port(const char *data,
                                                         char *ret) {
    return hdlr_fpga_board_flow_control_sfpX_port(data, ret, 3);
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
    uint32_t old_val1;
    uint32_t old_val2;
    read_hps_reg("sys16", &old_val1);
    read_hps_reg("sys17", &old_val2);

    // Append values
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
    uint32_t old_val1;
    uint32_t old_val2;
    read_hps_reg("sys3", &old_val2);
    read_hps_reg("sys4", &old_val1);

    old_val2 = old_val2 & 0xff;

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





static int hdlr_fpga_link_sfpc_ip_addr(const char *data, char *ret) {
    uint32_t ip[4];
    if (ipver[0] == IPVER_IPV4) {
        sscanf(data, "%" SCNd32 ".%" SCNd32 ".%" SCNd32 ".%" SCNd32 "", ip,
               ip + 1, ip + 2, ip + 3);
        write_hps_reg("net35",
                      (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
    } else if (ipver[0] == IPVER_IPV6) {
        sscanf(data, "%" SCNx32 ":%" SCNx32 ":%" SCNx32 ":%" SCNx32 "", ip,
               ip + 1, ip + 2, ip + 3);
        write_hps_reg("net31", ip[0]);
        write_hps_reg("net32", ip[1]);
        write_hps_reg("net33", ip[2]);
        write_hps_reg("net34", ip[3]);
    }
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpc_mac_addr(const char *data, char *ret) {
    uint8_t mac[6];
    sscanf(data,
           "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
           mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
    write_hps_reg("net41", (mac[0] << 8) | (mac[1]));
    write_hps_reg("net42",
                  (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpc_ver(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t ver;
    sscanf(data, "%" SCNd8 "", &ver);
    read_hps_reg("net30", &old_val);
    if (ver > 0)
        write_hps_reg("net30", (old_val | 0x4));
    else
        write_hps_reg("net30", (old_val & ~(0x4)));
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpc_pay_len(const char *data, char *ret) {
    uint32_t old_val;
    uint32_t pay_len;
    sscanf(data, "%" SCNd32 "", &pay_len);
    read_hps_reg("net30", &old_val);
    write_hps_reg("net30", (old_val & ~(0xffff0000)) | (pay_len << 16));
    return RETURN_SUCCESS;
}





static int hdlr_fpga_link_sfpd_ip_addr(const char *data, char *ret) {
    uint32_t ip[4];
    if (ipver[1] == IPVER_IPV4) {
        sscanf(data, "%" SCNd32 ".%" SCNd32 ".%" SCNd32 ".%" SCNd32 "", ip,
               ip + 1, ip + 2, ip + 3);
        ip[0] = (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]);
        write_hps_reg("net50", ip[0]);
    } else if (ipver[1] == IPVER_IPV6) {
        sscanf(data, "%" SCNx32 ":%" SCNx32 ":%" SCNx32 ":%" SCNx32 "", ip,
               ip + 1, ip + 2, ip + 3);
        write_hps_reg("net46", ip[0]);
        write_hps_reg("net47", ip[1]);
        write_hps_reg("net48", ip[2]);
        write_hps_reg("net49", ip[3]);
    }
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpd_mac_addr(const char *data, char *ret) {
    uint8_t mac[6];
    sscanf(data,
           "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
           mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
    write_hps_reg("net56", (mac[0] << 8) | (mac[1]));
    write_hps_reg("net57",
                  (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpd_ver(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t ver;
    sscanf(data, "%" SCNd8 "", &ver);
    read_hps_reg("net45", &old_val);
    if (ver > 0)
        write_hps_reg("net45", (old_val & ~(1 << 2)) | (1 << 2));
    else
        write_hps_reg("net45", (old_val & ~(1 << 2)));
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpd_pay_len(const char *data, char *ret) {
    uint32_t old_val;
    uint32_t pay_len;
    sscanf(data, "%" SCNd32 "", &pay_len);
    read_hps_reg("net45", &old_val);
    write_hps_reg("net45", (old_val & ~(0xffff0000)) | (pay_len << 16));
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

static int hdlr_fpga_user_regs(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_fpga_reset(const char *data, char *ret) {
    /* The reset controllet is like a waterfall:
     * Global Reset -> 40G Reset -> JESD Reset -> DSP Reset
     * Whichever Reset step we begin on will be followed by the others.
     * Reset is initiated by setting one bit high in the res_rw7 register
     * Global Reset    res_rw7[30]      Triggered by writing 1 to state tree
     * 40G Reset       res_rw7[29]      write 2 to state tree
     * JESD Reset      res_rw7[28]      write 3 to state tree
     * DSP Reset       res_rw7[27]      write 4 to state tree
     * Writing 0 to the state tree will not trigger any reset
     */
    int reset_type = 0;
    uint32_t tmp_reg = 0;

    sscanf(data, "%i", &reset_type);

    read_hps_reg("res_rw7", &tmp_reg);

    if (reset_type == 1){       // global reset bit 30
        write_hps_reg("res_rw7", (tmp_reg & (1 << 30)));
    }
    else if (reset_type == 2) { // 40G reset bit 29
        write_hps_reg("res_rw7", (tmp_reg & (1 << 29)));
    }
    else if (reset_type == 3) { // JESD reset bit 28
        write_hps_reg("res_rw7", (tmp_reg & (1 << 28)));
    }
    else if (reset_type == 4) { // DSP reset bit 27
        write_hps_reg("res_rw7", (tmp_reg & (1 << 27)));
    }
    /* register sys[18] shows the reset status
     * the bits are [31:0] chanMode = {
     * w_40gModulePresent,                                                         // 4-bits
     * w_X40gStatusRxPcsReady & w_X40gStatusRxBlockLock & w_X40gStatusRxAmLock,    // 4-bits
     * {2'b00, w_ResetSequencerState},                                             // 8-bits
     * w_ResetSequencerUnknownStateError,                                          // 1-bit
     * w_ResetSequencer40gResetSerialInterfaceTxWaitError,                         // 1-bit
     * w_ResetSequencer40gResetSerialInterfaceRxWaitError,                         // 1-bit
     * 13'b0
     * };
     */
    return RETURN_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/* --------------------------------- GPIO ----------------------------------- */
/* -------------------------------------------------------------------------- */

//Enable manual override:
//mem rw res_rw7 1.
//Registers 0:31= res_rw4[31:0]
//Registers 32:63 = res_rw5[31:0]
//Registers 64:95 = res_rw6[15:0]

static int hdlr_gpio_override_en(const char *data, char *ret) {
    uint32_t old_val = 0;
    // check if override is enabled
    if (strcmp(data, "0") != 0) {
        //Read res_rw7
        read_hps_reg("res_rw7", &old_val);
        //Mask lowest bit with 1 then write to res_rw7
        write_hps_reg("res_rw7", old_val | 0x1);
    } else {
        read_hps_reg("res_rw7", &old_val);
        write_hps_reg("res_rw7",old_val & (~0x1));
    }
    return RETURN_SUCCESS;
}

static int hdlr_gpio_gpio_all(const char *data, char *ret) {
    uint32_t old_val = 0;
    read_hps_reg("res_rw6", &old_val);
    if (strcmp(data, "0") != 0) {
        // since writing 1 to all bits in these two don't need to read first
        write_hps_reg("res_rw4", 0xffffffff);
        write_hps_reg("res_rw5", 0xffffffff);
        // this one needs a read and mask
        write_hps_reg("res_rw6", old_val | 0x0000ffff);
    } else {
        write_hps_reg("res_rw4", 0x0);
        write_hps_reg("res_rw5", 0x0);
        write_hps_reg("res_rw6",old_val & (~0x0000ffff));
    }
    return RETURN_SUCCESS;
}

// X Macro for GPIO
#define X(_p, io)                                                             \
    static int hdlr_gpio_##_p##_pin(const char *data, char *ret) {            \
        uint32_t old_val = 0;                                                 \
        int pin_number = 0;                                                   \
        sscanf(#_p, "%d", &pin_number);                                      \
        char res_reg_addr[8] = "res_rw4";                                     \
        if (pin_number < 32) {                                                \
            memcpy(res_reg_addr, "res_rw4", 8);                               \
        } else if (pin_number < 64) {                                         \
            memcpy(res_reg_addr, "res_rw5", 8);                               \
        } else {                                                              \
            memcpy(res_reg_addr, "res_rw6", 8);                               \
        }                                                                     \
        read_hps_reg(res_reg_addr, &old_val);                                 \
        if (strcmp(data, "0") != 0) {                                         \
            /*Read res_r4 for bits 0 to 31 */                                 \
            write_hps_reg(res_reg_addr, old_val | (1 << (pin_number%32)));    \
        } else {                                                              \
            write_hps_reg(res_reg_addr, old_val & (~(1 << (pin_number%32)))); \
        }                                                                     \
        return RETURN_SUCCESS;                                                \
    }
GPIO_PINS
#undef X

/* clang-format off */

/* -------------------------------------------------------------------------- */
/* ---------------------------- PROPERTY TABLE ------------------------------ */
/* -------------------------------------------------------------------------- */

#define PROJECT_NAME "cyan_4r4t"

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

#define DEFINE_RX_WAIT_PWR(_c) \
    DEFINE_FILE_PROP("rx/" #_c "/wait_pwr_board", hdlr_rx_##_c##_wait_pwr_board, RW, "0")

#define DEFINE_RX_PWR_REBOOT(_c)    \
    DEFINE_FILE_PROP("rx/" #_c "/pwr_board"                      , hdlr_rx_##_c##_pwr_board,                     RW, "0")   \
    /*async_pwr_board is initializeed with a default value of on after pwr board is initialized with off to ensure the board is off at the start*/\
    DEFINE_FILE_PROP("rx/" #_c "/async_pwr_board"                      , hdlr_rx_##_c##_async_pwr_board,                     RW, "1")   \
    DEFINE_FILE_PROP("rx/" #_c "/reboot"                   , hdlr_rx_##_c##_reboot,                  RW, "0")


#define DEFINE_RX_CHANNEL(_c)                                                                                         \
    DEFINE_SYMLINK_PROP("rx_" #_c, "rx/" #_c)                                                                         \
    DEFINE_FILE_PROP("rx/" #_c "/pwr"                      , hdlr_rx_##_c##_pwr,                     RW, "1")         \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/sma_mode"         , hdlr_rx_##_c##_trigger_sma_mode,        RW, "level")     \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/trig_sel"         , hdlr_rx_##_c##_trigger_trig_sel,        RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/edge_backoff"     , hdlr_rx_##_c##_trigger_edge_backoff,    RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/edge_sample_num"  , hdlr_rx_##_c##_trigger_edge_sample_num, RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/ufl_mode"         , hdlr_rx_##_c##_trigger_ufl_mode,        RW, "level")     \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/ufl_dir"          , hdlr_rx_##_c##_trigger_ufl_dir,         RW, "out")       \
    DEFINE_FILE_PROP("rx/" #_c "/trigger/ufl_pol"          , hdlr_rx_##_c##_trigger_ufl_pol,         RW, "negative")  \
    DEFINE_FILE_PROP("rx/" #_c "/stream"                   , hdlr_rx_##_c##_stream,                  RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/sync"                     , hdlr_rx_sync,                           WO, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/rf/freq/val"              , hdlr_rx_##_c##_rf_freq_val,             RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/rf/freq/lut_en"           , hdlr_rx_##_c##_rf_freq_lut_en,          RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/rf/freq/lna"              , hdlr_rx_##_c##_rf_freq_lna,             RW, "1")         \
    DEFINE_FILE_PROP("rx/" #_c "/rf/freq/band"             , hdlr_rx_##_c##_rf_freq_band,            RW, "1")         \
    DEFINE_FILE_PROP("rx/" #_c "/rf/gain/val"              , hdlr_rx_##_c##_rf_gain_val,             RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/rf/atten/val"             , hdlr_rx_##_c##_rf_atten_val,            RW, "31")        \
    DEFINE_FILE_PROP("rx/" #_c "/status/rfpll_lock"        , hdlr_rx_##_c##_status_rfld,             RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/status/adc_alarm"         , hdlr_rx_##_c##_status_adcalarm,         RW, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/board/dump"               , hdlr_rx_##_c##_rf_board_dump,           WO, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/board/test"               , hdlr_rx_##_c##_rf_board_test,           WO, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/board/temp"               , hdlr_rx_##_c##_rf_board_temp,           RW, "20")        \
    DEFINE_FILE_PROP("rx/" #_c "/board/led"                , hdlr_rx_##_c##_rf_board_led,            WO, "0")         \
    DEFINE_FILE_PROP("rx/" #_c "/dsp/signed"               , hdlr_rx_##_c##_dsp_signed,              RW, "1")         \
    DEFINE_FILE_PROP("rx/" #_c "/dsp/gain"                 , hdlr_rx_##_c##_dsp_gain,                RW, "10")        \
    DEFINE_FILE_PROP("rx/" #_c "/dsp/rate"                 , hdlr_rx_##_c##_dsp_rate,                RW, "1258850")   \
    DEFINE_FILE_PROP("rx/" #_c "/dsp/nco_adj"              , hdlr_rx_##_c##_dsp_fpga_nco,            RW, "-15000000") \
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
    DEFINE_FILE_PROP("rx/" #_c "/link/mac_dest"            , hdlr_rx_##_c##_link_mac_dest,           RW, "ff:ff:ff:ff:ff:ff")\
    DEFINE_FILE_PROP("rx/" #_c "/jesd_status"              , hdlr_rx_##_c##_jesd_status,             RW, "bad")\
    DEFINE_FILE_PROP("rx/" #_c "/link/jesd_num"                 , hdlr_invalid,                                   RO, "0")\
    DEFINE_FILE_PROP("rx/" #_c "/force_stream"             , hdlr_rx_##_c##_force_stream,                           RW, "0")

#define DEFINE_TX_WAIT_PWR(_c) \
    DEFINE_FILE_PROP("tx/" #_c "/wait_pwr_board", hdlr_tx_##_c##_wait_pwr_board, RW, "0")

#define DEFINE_TX_PWR_REBOOT(_c)    \
    DEFINE_FILE_PROP("tx/" #_c "/pwr_board"                      , hdlr_tx_##_c##_pwr_board,                     RW, "0")   \
    /*async_pwr_board is initializeed with a default value of on after pwr board is initialized with off to ensure the board is off at the start*/\
    DEFINE_FILE_PROP("tx/" #_c "/async_pwr_board"                      , hdlr_tx_##_c##_async_pwr_board,                     RW, "1")   \
    DEFINE_FILE_PROP("tx/" #_c "/reboot"                   , hdlr_tx_##_c##_reboot,                  RW, "0")

#define DEFINE_TX_CHANNEL(_c)                                                                                         \
    DEFINE_SYMLINK_PROP("tx_" #_c, "tx/" #_c)                                                                         \
    DEFINE_FILE_PROP("tx/" #_c "/pwr"                      , hdlr_tx_##_c##_pwr,                     RW, "1")         \
    DEFINE_FILE_PROP("tx/" #_c "/reboot"                   , hdlr_tx_##_c##_reboot,                  RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/jesd_status"              , hdlr_tx_##_c##_jesd_status,             RW, "bad")       \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/sma_mode"         , hdlr_tx_##_c##_trigger_sma_mode,        RW, "level")     \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/trig_sel"         , hdlr_tx_##_c##_trigger_trig_sel,        RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/edge_backoff"     , hdlr_tx_##_c##_trigger_edge_backoff,    RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/edge_sample_num"  , hdlr_tx_##_c##_trigger_edge_sample_num, RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/ufl_dir"          , hdlr_tx_##_c##_trigger_ufl_dir,         RW, "out")       \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/ufl_mode"         , hdlr_tx_##_c##_trigger_ufl_mode,        RW, "level")     \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/ufl_pol"          , hdlr_tx_##_c##_trigger_ufl_pol,         RW, "negative")  \
    DEFINE_FILE_PROP("tx/" #_c "/trigger/gating"           , hdlr_tx_##_c##_trigger_gating,          RW, "output")    \
    DEFINE_FILE_PROP("tx/" #_c "/link/vita_en"             , hdlr_tx_##_c##_link_vita_en,            RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/link/iface"               , hdlr_tx_##_c##_link_iface,              RW, "sfpa")      \
    DEFINE_FILE_PROP("tx/" #_c "/link/ch0port"             , hdlr_tx_##_c##_link_ch0port,            RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/link/ch1port"             , hdlr_tx_##_c##_link_ch1port,            RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/link/ch3port"             , hdlr_tx_##_c##_link_ch3port,            RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/link/ch4port"             , hdlr_tx_##_c##_link_ch4port,            RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/qa/ch0fifo_lvl"           , hdlr_tx_##_c##_qa_ch0fifo_lvl,          RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/qa/ch1fifo_lvl"           , hdlr_tx_##_c##_qa_ch1fifo_lvl,          RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/qa/ch3fifo_lvl"           , hdlr_tx_##_c##_qa_ch3fifo_lvl,          RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/qa/ch4fifo_lvl"           , hdlr_tx_##_c##_qa_ch4fifo_lvl,          RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/qa/ch0oflow"              , hdlr_tx_##_c##_qa_ch0oflow,             RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/qa/ch1oflow"              , hdlr_tx_##_c##_qa_ch1oflow,             RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/qa/ch3oflow"              , hdlr_tx_##_c##_qa_ch3oflow,             RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/qa/ch4oflow"              , hdlr_tx_##_c##_qa_ch4oflow,             RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/qa/ch0uflow"              , hdlr_tx_##_c##_qa_ch0uflow,             RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/qa/ch1uflow"              , hdlr_tx_##_c##_qa_ch1uflow,             RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/qa/ch3uflow"              , hdlr_tx_##_c##_qa_ch3uflow,             RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/qa/ch4uflow"              , hdlr_tx_##_c##_qa_ch4uflow,             RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/sync"                     , hdlr_tx_sync,                           WO, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/dsp/gain"                 , hdlr_tx_##_c##_dsp_gain,                RW, "10")        \
    DEFINE_FILE_PROP("tx/" #_c "/dsp/rate"                 , hdlr_tx_##_c##_dsp_rate,                RW, "1258850")   \
    DEFINE_FILE_PROP("tx/" #_c "/dsp/ch0fpga_nco"          , hdlr_tx_##_c##_dsp_ch0fpga_nco,         RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/dsp/ch1fpga_nco"          , hdlr_tx_##_c##_dsp_ch1fpga_nco,         RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/dsp/ch3fpga_nco"          , hdlr_tx_##_c##_dsp_ch3fpga_nco,         RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/dsp/ch4fpga_nco"          , hdlr_tx_##_c##_dsp_ch4fpga_nco,         RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/dsp/rstreq"               , hdlr_tx_##_c##_dsp_rstreq,              WO, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/nco/dac0freq"      , hdlr_tx_##_c##_dac_nco_dac0freq,        RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/nco/dac1freq"      , hdlr_tx_##_c##_dac_nco_dac1freq,        RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/nco/ch0freq"       , hdlr_tx_##_c##_dac_nco_ch0freq,         RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/nco/ch1freq"       , hdlr_tx_##_c##_dac_nco_ch1freq,         RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/nco/ch2freq"       , hdlr_tx_##_c##_dac_nco_ch2freq,         RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/nco/ch3freq"       , hdlr_tx_##_c##_dac_nco_ch3freq,         RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/nco/ch4freq"       , hdlr_tx_##_c##_dac_nco_ch4freq,         RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/nco/ch5freq"       , hdlr_tx_##_c##_dac_nco_ch5freq,         RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/gain/ch0atten"     , hdlr_tx_##_c##_dac_gain_ch0atten,       RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/gain/ch1atten"     , hdlr_tx_##_c##_dac_gain_ch1atten,       RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/gain/ch2atten"     , hdlr_tx_##_c##_dac_gain_ch2atten,       RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/gain/ch3atten"     , hdlr_tx_##_c##_dac_gain_ch3atten,       RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/gain/ch4atten"     , hdlr_tx_##_c##_dac_gain_ch4atten,       RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/gain/ch5atten"     , hdlr_tx_##_c##_dac_gain_ch5atten,       RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/band"                  , hdlr_tx_##_c##_rf_band,                 RW, "-1")        \
    DEFINE_FILE_PROP("tx/" #_c "/rf/atten"                 , hdlr_tx_##_c##_rf_atten,                RW, "31")        \
    DEFINE_FILE_PROP("tx/" #_c "/rf/gain/val"              , hdlr_tx_##_c##_rf_gain_val,             RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/rf/lo_freq"               , hdlr_tx_##_c##_rf_lo_freq,              RW, "0")         \
    DEFINE_FILE_PROP("tx/" #_c "/about/id"                 , hdlr_tx_##_c##_about_id,                RW, "001")       \
    DEFINE_FILE_PROP("tx/" #_c "/about/serial"             , hdlr_tx_##_c##_about_serial,            RW, "001")       \
    DEFINE_FILE_PROP("tx/" #_c "/about/mcudevid"           , hdlr_tx_##_c##_about_mcudevid,          RW, "001")       \
    DEFINE_FILE_PROP("tx/" #_c "/about/mcurev"             , hdlr_tx_##_c##_about_mcurev,            RW, "001")       \
    DEFINE_FILE_PROP("tx/" #_c "/about/mcufuses"           , hdlr_tx_##_c##_about_mcufuses,          RW, "001")       \
    DEFINE_FILE_PROP("tx/" #_c "/about/fw_ver"             , hdlr_tx_##_c##_about_fw_ver,            RW, VERSION)     \
    DEFINE_FILE_PROP("tx/" #_c "/about/sw_ver"             , hdlr_invalid,                           RO, VERSION)
//    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/nco"               , hdlr_tx_##_c##_rf_dac_nco,              RW, "0")         \
//     DEFINE_FILE_PROP("tx/" #_c "/status/rfpll_lock"        , hdlr_tx_##_c##_status_rfld,             RW, "0")         \
//     DEFINE_FILE_PROP("tx/" #_c "/status/dacpll_lock"       , hdlr_tx_##_c##_status_dacld,            RW, "0")         \
//    DEFINE_FILE_PROP("tx/" #_c "/rf/dac/temp"              , hdlr_tx_##_c##_rf_dac_temp,             RW, "0")         \
//    DEFINE_FILE_PROP("tx/" #_c "/rf/freq/val"              , hdlr_tx_##_c##_rf_freq_val,             RW, "0")         \
//    DEFINE_FILE_PROP("tx/" #_c "/board/dump"               , hdlr_tx_##_c##_rf_board_dump,           WO, "0")         \
//    DEFINE_FILE_PROP("tx/" #_c "/board/test"               , hdlr_tx_##_c##_rf_board_test,           WO, "0")         \
//    DEFINE_FILE_PROP("tx/" #_c "/board/temp"               , hdlr_tx_##_c##_rf_board_temp,           RW, "23")        \
//    DEFINE_FILE_PROP("tx/" #_c "/board/led"                , hdlr_tx_##_c##_rf_board_led,            WO, "0")         \
    //    DEFINE_FILE_PROP("tx/" #_c "/link/ch2port"             , hdlr_tx_##_c##_link_ch2port,            RW, "0")         \
    //    DEFINE_FILE_PROP("tx/" #_c "/link/ch5port"             , hdlr_tx_##_c##_link_ch5port,            RW, "0")         \
    //    DEFINE_FILE_PROP("tx/" #_c "/qa/ch2fifo_lvl"           , hdlr_tx_##_c##_qa_ch2fifo_lvl,          RW, "0")         \
    //    DEFINE_FILE_PROP("tx/" #_c "/qa/ch5fifo_lvl"           , hdlr_tx_##_c##_qa_ch5fifo_lvl,          RW, "0")         \
    //    DEFINE_FILE_PROP("tx/" #_c "/qa/ch2oflow"              , hdlr_tx_##_c##_qa_ch2oflow,             RW, "0")         \
    //    DEFINE_FILE_PROP("tx/" #_c "/qa/ch5oflow"              , hdlr_tx_##_c##_qa_ch5oflow,             RW, "0")         \
    //    DEFINE_FILE_PROP("tx/" #_c "/qa/ch2uflow"              , hdlr_tx_##_c##_qa_ch2uflow,             RW, "0")         \
    //    DEFINE_FILE_PROP("tx/" #_c "/qa/ch5uflow"              , hdlr_tx_##_c##_qa_ch5uflow,             RW, "0")         \
    //    DEFINE_FILE_PROP("tx/" #_c "/dsp/ch2fpga_nco"          , hdlr_tx_##_c##_dsp_ch2fpga_nco,         RW, "0")         \
    //    DEFINE_FILE_PROP("tx/" #_c "/dsp/ch5fpga_nco"          , hdlr_tx_##_c##_dsp_ch5fpga_nco,         RW, "0")         \
    //DEFINE_FILE_PROP("tx/" #_c "/qa/fifo_lvl"              , hdlr_tx_##_c##_qa_fifo_lvl,             RW, "0")         \
    //DEFINE_FILE_PROP("tx/" #_c "/qa/oflow"                 , hdlr_tx_##_c##_qa_oflow,                RW, "0")         \
    //DEFINE_FILE_PROP("tx/" #_c "/qa/uflow"                 , hdlr_tx_##_c##_qa_uflow,                RW, "0")         \

#define DEFINE_TIME()                                                                                                 \
    DEFINE_FILE_PROP("time/reboot"                         , hdlr_time_reboot,                       RW, "0")         \
    DEFINE_FILE_PROP("time/clk/pps"                        , hdlr_time_clk_pps,                      RW, "0")         \
    DEFINE_FILE_PROP("time/clk/cur_time"                   , hdlr_time_clk_cur_time,                 RW, "0.0")       \
    DEFINE_FILE_PROP("time/clk/cmd"                        , hdlr_time_clk_cmd,                      RW, "0.0")       \
    DEFINE_FILE_PROP("time/status/lmk_lockdetect"          , hdlr_time_status_ld,                    RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lossoflock"          , hdlr_time_status_lol,                   RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lockdetect_jesd0_pll1", hdlr_time_status_ld_jesd0_pll1,        RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lockdetect_jesd0_pll2", hdlr_time_status_ld_jesd0_pll2,        RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lockdetect_jesd1_pll1", hdlr_time_status_ld_jesd1_pll1,        RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lockdetect_jesd1_pll2", hdlr_time_status_ld_jesd1_pll2,        RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lockdetect_jesd2_pll1", hdlr_time_status_ld_jesd2_pll1,        RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lockdetect_jesd2_pll2", hdlr_time_status_ld_jesd2_pll2,        RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lossoflock_jesd0_pll1", hdlr_time_status_lol_jesd0_pll1,       RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lossoflock_jesd0_pll2", hdlr_time_status_lol_jesd0_pll2,       RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lossoflock_jesd1_pll1", hdlr_time_status_lol_jesd1_pll1,       RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lossoflock_jesd1_pll2", hdlr_time_status_lol_jesd1_pll2,       RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lossoflock_jesd2_pll1", hdlr_time_status_lol_jesd2_pll1,       RW, "unlocked")  \
    DEFINE_FILE_PROP("time/status/lmk_lossoflock_jesd2_pll2", hdlr_time_status_lol_jesd2_pll2,       RW, "unlocked")  \
    DEFINE_FILE_PROP("time/source/ref"                     , hdlr_time_source_ref,                   RW, "internal")  \
    DEFINE_FILE_PROP("time/sync/sysref_mode"               , hdlr_time_sync_sysref_mode,             RW, "continuous")    \
    DEFINE_FILE_PROP("time/sync/lmk_sync_tgl_jesd"         , hdlr_time_sync_lmk_sync_tgl_jesd,       WO, "0")         \
    DEFINE_FILE_PROP("time/sync/lmk_sync_resync_jesd"      , hdlr_time_sync_lmk_resync_jesd,         WO, "0")         \
    DEFINE_FILE_PROP("time/sync/lmk_resync_all"            , hdlr_time_sync_lmk_resync_all,          WO, "0")         \
    DEFINE_FILE_PROP("time/board/dump"                     , hdlr_time_board_dump,                   WO, "0")         \
    DEFINE_FILE_PROP("time/board/test"                     , hdlr_time_board_test,                   WO, "0")         \
    DEFINE_FILE_PROP("time/board/led"                      , hdlr_time_board_led,                    WO, "0")         \
    DEFINE_FILE_PROP("time/about/id"                       , hdlr_time_about_id,                     RO, "001")       \
    DEFINE_FILE_PROP("time/about/serial"                   , hdlr_time_about_serial,                 RW, "001")       \
    DEFINE_FILE_PROP("time/about/mcudevid"                 , hdlr_time_about_mcudevid,               RW, "001")       \
    DEFINE_FILE_PROP("time/about/mcurev"                   , hdlr_time_about_mcurev,                 RW, "001")       \
    DEFINE_FILE_PROP("time/about/mcufuses"                 , hdlr_time_about_mcufuses,               RW, "001")       \
    DEFINE_FILE_PROP("time/about/fw_ver"                   , hdlr_time_about_fw_ver,                 RW, VERSION)     \
    DEFINE_FILE_PROP("time/about/sw_ver"                   , hdlr_invalid,                           RO, VERSION)\
    DEFINE_FILE_PROP("time/status/status_good"             , hdlr_time_status_good,                  RW, "bad")
    //DEFINE_FILE_PROP("time/board/temp"                     , hdlr_time_board_temp,                   RW, "20")        \


#define DEFINE_FPGA()                                                                                                         \
    DEFINE_FILE_PROP("fpga/user/regs"                      , hdlr_fpga_user_regs,                    RW, "0.0")               \
    DEFINE_FILE_PROP("fpga/reset"                          , hdlr_fpga_reset,                        RW, "0")                 \
    DEFINE_FILE_PROP("fpga/trigger/sma_dir"                , hdlr_fpga_trigger_sma_dir,              RW, "out")               \
    DEFINE_FILE_PROP("fpga/trigger/sma_pol"                , hdlr_fpga_trigger_sma_pol,              RW, "negative")          \
    DEFINE_FILE_PROP("fpga/about/fw_ver"                   , hdlr_fpga_about_fw_ver,                 RW, VERSION)             \
    DEFINE_FILE_PROP("fpga/about/sw_ver"                   , hdlr_invalid,                           RO, VERSION)             \
    DEFINE_FILE_PROP("fpga/about/server_ver"               , hdlr_server_about_fw_ver,               RW, "")                  \
    DEFINE_FILE_PROP("fpga/about/hw_ver"                   , hdlr_fpga_about_hw_ver,                 RW, VERSION)             \
    DEFINE_FILE_PROP("fpga/about/id"                       , hdlr_fpga_about_id,                     RW, "001")               \
    DEFINE_FILE_PROP("fpga/about/name"                     , hdlr_invalid,                           RO, PROJECT_NAME)        \
    DEFINE_FILE_PROP("fpga/about/serial"                   , hdlr_fpga_about_serial,                 RW, "001")               \
    DEFINE_FILE_PROP("fpga/about/cmp_time"                 , hdlr_fpga_about_cmp_time,               RW, "yyyy-mm-dd-hh-mm")  \
    DEFINE_FILE_PROP("fpga/about/conf_info"                , hdlr_fpga_about_conf_info,              RW, "0")                 \
    DEFINE_FILE_PROP("fpga/board/dump"                     , hdlr_fpga_board_dump,                   WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/fw_rst"                   , hdlr_fpga_board_fw_rst,                 WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/flow_control/sfpa_port"   , hdlr_fpga_board_flow_control_sfpa_port, RW, "42809")             \
    DEFINE_FILE_PROP("fpga/board/flow_control/sfpb_port"   , hdlr_fpga_board_flow_control_sfpb_port, RW, "42810")             \
    DEFINE_FILE_PROP("fpga/board/flow_control/sfpc_port"   , hdlr_fpga_board_flow_control_sfpc_port, RW, "42811")             \
    DEFINE_FILE_PROP("fpga/board/flow_control/sfpd_port"   , hdlr_fpga_board_flow_control_sfpd_port, RW, "42812")             \
    DEFINE_FILE_PROP("fpga/board/gps_time"                 , hdlr_fpga_board_gps_time,               RW, "0")                 \
    DEFINE_FILE_PROP("fpga/board/gps_frac_time"            , hdlr_fpga_board_gps_frac_time,          RW, "0")                 \
    DEFINE_FILE_PROP("fpga/board/gps_sync_time"            , hdlr_fpga_board_gps_sync_time,          RW, "0")                 \
    DEFINE_FILE_PROP("fpga/board/jesd_sync"                , hdlr_fpga_board_jesd_sync,              WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/led"                      , hdlr_fpga_board_led,                    WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/rstreq_all_dsp"           , hdlr_fpga_board_rstreq_all_dsp,         WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/rstreq"                   , hdlr_fpga_board_rstreq,                 WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/reboot"                   , hdlr_fpga_board_reboot,                 RW, "0")                 \
    DEFINE_FILE_PROP("fpga/board/sys_rstreq"               , hdlr_fpga_board_sys_rstreq,             WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/test"                     , hdlr_fpga_board_test,                   WO, "0")                 \
    DEFINE_FILE_PROP("fpga/board/temp"                     , hdlr_fpga_board_temp,                   RW, "20")                \
    DEFINE_FILE_PROP("fpga/board/gle"                      , hdlr_fpga_board_gle,                    RW, "0")                 \
    DEFINE_FILE_PROP("fpga/link/rate"                      , hdlr_fpga_link_rate,                    RW, "5000000000")        \
    DEFINE_FILE_PROP("fpga/link/sfpa/ip_addr"              , hdlr_fpga_link_sfpa_ip_addr,            RW, "10.10.10.2")        \
    DEFINE_FILE_PROP("fpga/link/sfpa/mac_addr"             , hdlr_fpga_link_sfpa_mac_addr,           RW, "aa:00:00:00:00:00") \
    DEFINE_FILE_PROP("fpga/link/sfpa/ver"                  , hdlr_fpga_link_sfpa_ver,                RW, "0")                 \
    DEFINE_FILE_PROP("fpga/link/sfpa/pay_len"              , hdlr_fpga_link_sfpa_pay_len,            RW, "8900")              \
    DEFINE_FILE_PROP("fpga/link/sfpb/ip_addr"              , hdlr_fpga_link_sfpb_ip_addr,            RW, "10.10.11.2")        \
    DEFINE_FILE_PROP("fpga/link/sfpb/mac_addr"             , hdlr_fpga_link_sfpb_mac_addr,           RW, "aa:00:00:00:00:01") \
    DEFINE_FILE_PROP("fpga/link/sfpb/ver"                  , hdlr_fpga_link_sfpb_ver,                RW, "0")                 \
    DEFINE_FILE_PROP("fpga/link/sfpb/pay_len"              , hdlr_fpga_link_sfpb_pay_len,            RW, "8900")              \
    DEFINE_FILE_PROP("fpga/link/sfpc/ip_addr"              , hdlr_fpga_link_sfpc_ip_addr,            RW, "10.10.12.2")        \
    DEFINE_FILE_PROP("fpga/link/sfpc/mac_addr"             , hdlr_fpga_link_sfpc_mac_addr,           RW, "aa:00:00:00:00:02") \
    DEFINE_FILE_PROP("fpga/link/sfpc/ver"                  , hdlr_fpga_link_sfpc_ver,                RW, "0")                 \
    DEFINE_FILE_PROP("fpga/link/sfpc/pay_len"              , hdlr_fpga_link_sfpc_pay_len,            RW, "8900")              \
    DEFINE_FILE_PROP("fpga/link/sfpd/ip_addr"              , hdlr_fpga_link_sfpd_ip_addr,            RW, "10.10.13.2")        \
    DEFINE_FILE_PROP("fpga/link/sfpd/mac_addr"             , hdlr_fpga_link_sfpd_mac_addr,           RW, "aa:00:00:00:00:03") \
    DEFINE_FILE_PROP("fpga/link/sfpd/ver"                  , hdlr_fpga_link_sfpd_ver,                RW, "0")                 \
    DEFINE_FILE_PROP("fpga/link/sfpd/pay_len"              , hdlr_fpga_link_sfpd_pay_len,            RW, "8900")              \
    DEFINE_FILE_PROP("fpga/link/net/dhcp_en"               , hdlr_fpga_link_net_dhcp_en,             RW, "0")                 \
    DEFINE_FILE_PROP("fpga/link/net/hostname"              , hdlr_fpga_link_net_hostname,            RW, PROJECT_NAME)        \
    DEFINE_FILE_PROP("fpga/link/net/ip_addr"               , hdlr_fpga_link_net_ip_addr,             RW, "192.168.10.2")


#define DEFINE_GPIO(_p)                                                                                                        \
    DEFINE_FILE_PROP("gpio/gpio" #_p                       , hdlr_gpio_##_p##_pin,                   RW, "0")

#define DEFINE_CM()                                                    \
    DEFINE_FILE_PROP("cm/chanmask-rx" , hdlr_cm_chanmask_rx , RW, "0") \
    DEFINE_FILE_PROP("cm/chanmask-tx" , hdlr_cm_chanmask_tx , RW, "0") \
    DEFINE_FILE_PROP("cm/rx/atten/val", hdlr_cm_rx_atten_val, WO, "0") \
    DEFINE_FILE_PROP("cm/rx/gain/val" , hdlr_cm_rx_gain_val , WO, "0") \
    DEFINE_FILE_PROP("cm/tx/gain/val" , hdlr_cm_tx_gain_val , WO, "0") \
    DEFINE_FILE_PROP("cm/trx/freq/val", hdlr_cm_trx_freq_val, WO, "0") \
    DEFINE_FILE_PROP("cm/trx/fpga_nco" , hdlr_cm_trx_fpga_nco , WO, "0")

static prop_t property_table[] = {
    DEFINE_TIME()
    //power off then on reboot rx/tx boards, but don't wait for them to finish booting
#define X(ch, io, crx, ctx) DEFINE_RX_PWR_REBOOT(ch)
    CHANNELS
#undef X
#define X(ch, io, crx, ctx) DEFINE_TX_PWR_REBOOT(ch)
    CHANNELS
#undef X

//waits for boards to finish booting
#define X(ch, rx, crx, ctx) DEFINE_RX_WAIT_PWR(ch)
    CHANNELS
#undef X
#define X(ch, rx, crx, ctx) DEFINE_TX_WAIT_PWR(ch)
    CHANNELS
#undef X

#define X(ch, io, crx, ctx) DEFINE_RX_CHANNEL(ch)
    CHANNELS
#undef X
#define X(ch, io, crx, ctx) DEFINE_TX_CHANNEL(ch)
    CHANNELS
#undef X
    DEFINE_FPGA()
#define X(_p, io) DEFINE_GPIO(_p)
    GPIO_PINS
#undef X
    DEFINE_FILE_PROP("gpio/override_en"                    , hdlr_gpio_override_en,                  RW, "0")                 \
    DEFINE_FILE_PROP("gpio/gpio_all"                       , hdlr_gpio_gpio_all,                     RW, "0")
    DEFINE_FILE_PROP("save_config", hdlr_save_config, RW, "/home/root/profile.cfg")
    DEFINE_FILE_PROP("load_config", hdlr_load_config, RW, "/home/root/profile.cfg")
    DEFINE_CM()
};

static const size_t num_properties = ARRAY_SIZE(property_table);

static char *tostr(const int num)
{
    char* str = calloc(32, sizeof(*str));
    sprintf(str, "%d", num);
    return str;
}

static void set_default_str(const char* const path, const char* const str)
{
    prop_t* prop = get_prop_from_cmd(path);
    strcpy(prop->def_val, str);
}

static void set_default_int(const char* const path, const int value)
{
    char* str = tostr(value);
    set_default_str(path, str);
    free(str);
}

/* clang-format on */

/* -------------------------------------------------------------------------- */
/* -------------------------- EXTERNED FUNCTIONS ---------------------------- */
/* -------------------------------------------------------------------------- */

void dump_tree(void) {
    for (int i = 0; i < num_properties; i++) {
        struct prop p = property_table[i];
        printf("%40s == %40s\n", p.path, p.def_val);
    }
}

void patch_tree(void) {
    const int base_port = 42836;

#define X(ch, io, crx, ctx) set_default_int("rx/" #ch "/link/port", base_port + INT(ch));
    CHANNELS
#undef X

set_default_str("rx/a/link/ip_dest","10.10.10.10");
set_default_str("rx/b/link/ip_dest","10.10.11.10");
set_default_str("rx/c/link/ip_dest","10.10.12.10");
set_default_str("rx/d/link/ip_dest","10.10.13.10");

#define X(ch, io, crx, ctx)                                                                                       \
    set_default_int("tx/" #ch "/link/ch0port", base_port + INT_TX(ch)*4 + 0 + NUM_CHANNELS);               \
    set_default_int("tx/" #ch "/link/ch1port", base_port + INT_TX(ch)*4 + 1 + NUM_CHANNELS);               \
    set_default_int("tx/" #ch "/link/ch3port", base_port + INT_TX(ch)*4 + 2 + NUM_CHANNELS);               \
    set_default_int("tx/" #ch "/link/ch4port", base_port + INT_TX(ch)*4 + 3 + NUM_CHANNELS);

    CHANNELS
#undef X
}

size_t get_num_prop(void) { return num_properties; }

prop_t *get_prop(size_t idx) { return (property_table + idx); }

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

    const char *vcs = "/var/cyan/state/";
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

void pass_uart_synth_fd(int fd) { uart_synth_fd = fd; }

void pass_uart_tx_fd(int *fd) { uart_tx_fd = fd; }

void pass_uart_rx_fd(int *fd) { uart_rx_fd = fd; }

char *get_abs_path(prop_t *prop, char *path) {
    strcpy(path, "/var/cyan");
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

    strcpy(path, "/var/cyan");
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
// This also needs to be extended to convert the chan_mask to a specific RFE
// for tate.
void sync_channels(uint8_t chan_mask) {
    uint32_t reg_val;
    int i_reset = 0;
    bool jesd_good = false;
    char str_chan_mask[MAX_PROP_LEN] = "";
    sprintf(str_chan_mask + strlen(str_chan_mask), "%" PRIu8 "", 15);
    // usleep(300000); // Some wait time for the reset to be ready
    /* Bring the ADCs & DACs into 'demo' mode for JESD */

    //During init, we want to set the RFE to mute,
    //So that the transient spurs that happen when
    //you initialy turn on a channel or run a jesd sync
    //don't make it to the SMA.

    // // RX - ADCs
    // strcpy(buf, "power -c ");
    // strcat(buf, str_chan_mask);
    // strcat(buf, " -a 1\r");
    // ping(uart_rx_fd[0], (uint8_t *)buf, strlen(buf));

    // // TX - DACs
    // strcpy(buf, "power -c ");
    // strcat(buf, str_chan_mask);
    // strcat(buf, " -d 1\r");
    // ping(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));

    /***********************************
     * Start loop.
     * Issue JESD, then read to see if
     * bad
     **********************************/
    // FPGA SFP IP reset
    write_hps_reg("res_rw7", 0x20000000);
    write_hps_reg("res_rw7", 0);

    // Set time board to continuous mode.
    strcpy(buf, "debug -l 7 -r 139 -w 3\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));

    usleep(2000000); // Wait 2 seconds to allow jesd link to go down

    while ((i_reset < max_attempts) && (jesd_good == false)) {
        i_reset++;
        // FPGA JESD IP reset
        write_hps_reg("res_rw7",0x10000000);
        write_hps_reg("res_rw7", 0);
        usleep(400000); // Some wait time for MCUs to be ready
        /* Trigger a SYSREF pulse */
        // strcpy(buf, "sync -k\r");
        // ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        // usleep(200000); // Some wait time for MCUs to be ready
        read_hps_reg("res_ro11", &reg_val);
        if ((reg_val  & 0xff)== jesd_good_code) {
            PRINT(INFO, "all JESD links good after %i JESD IP resets\n", i_reset);
            jesd_good = true;
        }
    }
    if (jesd_good != true) {
        PRINT(ERROR, "some JESD links bad after %i JESD IP resets\n", i_reset);
    }

    //Return to pulsed Sysref Mode
    strcpy(buf, "debug -l 7 -r 139 -w 2\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));

    return;
}

void set_pll_frequency(int uart_fd, uint64_t reference, pllparam_t *pll,
                       bool tx, uint32_t channel) {
    // extract pll1 variables and pass to MCU (ADF4355/ADF5355)

    // Send Reference to MCU ( No Need ATM since fixed reference )
    strcpy(buf, "rf -v ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(reference / 1000));
    // Send reference in kHz
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // write ADF4355/5355 R
    strcpy(buf, "rf -r ");
    sprintf(buf + strlen(buf), "%" PRIu16 "", pll->R);
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // write ADF4355/ADF5355 N
    strcpy(buf, "rf -n ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", pll->N);
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // write ADF4355/ADF5355 D
    strcpy(buf, "rf -d ");
    sprintf(buf + strlen(buf), "%" PRIu16 "", pll->d);
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // write ADF4355/ADF5355 feedback mode
    strcpy(buf, "rf -t ");
    sprintf(buf + strlen(buf), "%" PRIu8 "", pll->divFBen);
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // write ADF4355/ADF5355 Output RF Power
    strcpy(buf, "rf -g ");
    sprintf(buf + strlen(buf), "%" PRIu8 "", 1 /*pll->power*/);
    // default to lower mid power
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

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
            ping(uart_fd, (uint8_t *)buf, strlen(buf));
        }
    } else {
        // If synth lut is disabled, set autocal flat to enable fall-back
        // behaviour.
        snprintf(buf, sizeof(buf), "rf -c %c -A 1\r", 'a' + channel);
        ping(uart_fd, (uint8_t *)buf, strlen(buf));
    }

    // ADF output power level not presently specified.
    strcpy(buf, "rf -g ");

    // write ADF4355/ADF5355 Output Frequency
    strcpy(buf, "rf -f ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(freq / 1000));
    // Send output frequency in kHz
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));
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
#define X(ch, io, crx, ctx) hdlr_rx_##ch##_rf_freq_val,
        CHANNELS
#undef X
    };

    static const fp_t tx_fp[] = {
#define X(ch, io, crx, ctx) hdlr_tx_##ch##_rf_lo_freq,
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

void set_lo_frequency(int uart_fd, uint64_t reference, pllparam_t *pll) {
    // extract lo variables and pass to MCU (LMX2595)

    double freq = pll->vcoFreq / pll->d;

    // Ensure that the LoGen board is powered on
    strcpy(buf, "lmx -O 0\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // Reinitialize the LMX. For some reason the initialization on server boot, doesn't seem to be enough
    strcpy(buf, "lmx -k\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // Send Reference in MHz to MCU
    strcpy(buf, "lmx -o ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(reference / 1000000));
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // write LMX R
    strcpy(buf, "lmx -r ");
    sprintf(buf + strlen(buf), "%" PRIu16 "", pll->R);
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // write LMX N
    strcpy(buf, "lmx -n ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", pll->N);
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // write LMX D
    strcpy(buf, "lmx -d ");
    sprintf(buf + strlen(buf), "%" PRIu16 "", pll->d);
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // write LMX Output RF Power
    strcpy(buf, "lmx -p ");
    sprintf(buf + strlen(buf), "%" PRIu8 "", 60 /*TODO: pll->power*/);
    // default to high power
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // write LMX Output Frequency in MHz
    strcpy(buf, "lmx -f ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(freq / 1000000));
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));
    usleep(100000);
}

#endif
