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

#if defined(VAUNT)

#if 1 /* Removes headers for quick gcc -E diagnostics for XMACRO stuffs */
    #include "properties.h"

    #include "array-utils.h"
    #include "led.h"
    #include "mmap.h"
    #include "property_manager.h"
    #include "synth_lut.h"

    #include <ctype.h>
    #include <stdbool.h>
    #include <stdio.h>
    #include <string.h>
    #include <math.h>
#endif

#include <sys/stat.h>
#include "channels.h"

#include "variant_config/vaunt_rtm_config.h"
#include "variant_config/vaunt_special_config.h"

#define ALTERNATE_TREE_DEFAULTS_PATH "/etc/crimson/alternate_tree_defaults.cfg"
#define NO_LMX_SUPPORT "RTM6 and RTM7 hardware does not support common LO"

// Alias PLL_CORE_REF_FREQ_HZ for clarity
#define LO_STEPSIZE PLL_CORE_REF_FREQ_HZ
#define LO_STEPSIZE_S PLL_CORE_REF_FREQ_HZ_S

// Number to divide the base sample rate by to get the maximum rate the host can request
int link_rate_divisor = 1;

// Tick rate of Crimson's internal clock
// It is currently equal to the device's base sample sample
#define TICK_RATE get_base_sample_rate()

double get_base_sample_rate() {
    // FPGA register reports sample rate in MHz, this function returns Hz
    uint32_t read_val;
    read_hps_reg("res_ro3",&read_val);
    read_val = ( read_val >> 20) & 0xFFF;
    switch(read_val){
        case 300:
            link_rate_divisor = 1;
            return 300000000.0;
        case 325:
            // 10G link cannot take 325Msps, so the user/host is limited to 2162.5Msps
            // Internal FPGA calcs can still use the base rate
            link_rate_divisor = 2;
            return 325000000.0;
        default:
            PRINT(ERROR, "Unexpected base sample rate reported by FPGA\n",read_val);
            return 0;
    }
}

double get_dsp_nco_const() {
    // (2 ^ 32) / (1 * BASE_SAMPLE_RATE)
    uint32_t read_val;
    read_hps_reg("res_ro3",&read_val);
    read_val = ( read_val >> 20) & 0xFFF;
    switch(read_val){
        case 300:
            return 14.3165576533333333333333333333333333;
        case 325:
            return 13.215283987692307692307692307692307692307692307692307690000;
        default:
            PRINT(ERROR, "Unexpected base sample rate reported by FPGA\n",read_val);
            return 0;
    }
}

double get_dac_nco_const() {
    // (2 ^ 48) / (4 * BASE_SAMPLE_RATE)
    uint32_t read_val;
    read_hps_reg("res_ro3",&read_val);
    read_val = ( read_val >> 20) & 0xFFF;
    switch(read_val){
        case 300:
            return 234562.480592213333333333333333333333;
        case 325:
            return 216519.21285435076923076923076923076923076923076923076919296;
        default:
            PRINT(ERROR, "Unexpected base sample rate reported by FPGA\n",read_val);
            return 0;
    }
}

#define AVERY_IF 650000000UL

#define REF_FREQ 5000000 // core reference frequency is 5MHz, needed by LMX2595

#define IPVER_IPV4 0
#define IPVER_IPV6 1

#define PWR_ON  1
#define PWR_OFF 0

// Values indicating whether JESD is initialized of unitialized for that channel
#define JESD_INIT 1
#define JESD_UNINIT 0

#define STREAM_ON  1
#define STREAM_OFF 0

// Maximum number of times the LO will be reset if unlocked
#define MAX_AUTOCAL_ATTEMPTS 5
#define MAX_RESET_WAIT 5000 // * 100us = 0.5s

// set to 1 for DEBUG PRINTS related to EEPROM
#define DEBUG_PRINT_EEPROM 0

#if RX_40GHZ_FE
    #define MAX_RF_FREQ    40650000000ULL
    #define S_MAX_RF_FREQ "40650000000"
    #define PLL_ABSOLUTE_MAX (40000000000ULL + PLL1_RFOUT_MAX_HZ)
#else
    #define MAX_RF_FREQ    6180000000ULL
    #define S_MAX_RF_FREQ "6180000000"
    #define PLL_ABSOLUTE_MAX PLL1_RFOUT_MAX_HZ
#endif

//Defines maximum LO and performs a sanity check to make sure said LO is theoretically achievable by hardware
#if MAX_RF_FREQ > PLL_ABSOLUTE_MAX
    #error "Desired LO range greater than theoretical hardware limit"
#endif
const int64_t MAX_LO = MAX_RF_FREQ;

const int jesd_max_server_restart_attempts = 4;

// Maximum user set delay for i or q
const uint8_t max_iq_delay = 32;

// Minimum delay required to clear the buffer when resetting the dsp
const int buffer_reset_delay = 10;

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

static uint8_t rx_power[] = {
#define X(ch) PWR_OFF,
    RX_CHANNELS
#undef X
};

static uint8_t rx_jesd[] = {
#define X(ch) JESD_UNINIT,
    RX_CHANNELS
#undef X
};

static uint8_t rx_stream[] = {
#define X(ch) STREAM_OFF,
    RX_CHANNELS
#undef X
};

static const char *reg4[] = {
#define X(ch) "rx"STR(ch)"4",
    RX_CHANNELS
#undef X
#define X(ch) "tx"STR(ch)"4",
    TX_CHANNELS
#undef X
};

#if (NUM_TX_CHANNELS > 0)
static uint8_t tx_power[] = {
    #define X(ch) PWR_OFF,
    TX_CHANNELS
    #undef X
};

static uint8_t tx_jesd[] = {
#define X(ch) JESD_UNINIT,
    TX_CHANNELS
#undef X
};

static int i_bias[] = {
#define X(ch) 17,
    TX_CHANNELS
#undef X
};

static int q_bias[] = {
#define X(ch) 17,
    TX_CHANNELS
#undef X
};
#endif //(NUM_TX_CHANNELS > 0)

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
    char buf[MAX_UART_RET_LEN] = {};
    memset(buf, 0, MAX_UART_RET_LEN);

    uint32_t total_bytes = 0, cur_bytes = 0;

    while (contains(buf, '>', total_bytes) < 1) {
        if (recv_uart_comm(uartfd, ((uint8_t *)buf) + total_bytes, &cur_bytes,
                           MAX_UART_RET_LEN - total_bytes)) {
            return 0;
        }
        total_bytes += cur_bytes;
    }

    printf("%s\n", buf);
    snprintf((char *)uart_ret_buf, MAX_UART_RET_LEN, "%s", buf);
    return RETURN_SUCCESS;
}

// Finds the optimal value for the sample rate blocks
static uint16_t get_optimal_sr_factor(double rate, double base_rate,
                                      double *err) {
    double max_factor = 65535; // 2^16 - 1
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

#if (NUM_TX_CHANNELS > 0)
static int hdlr_tx_sync(const char *data, char *ret) {
    uint32_t old_val;

    // toggle the bit sys0[6]
    read_hps_reg("sys0", &old_val);
    write_hps_reg("sys0", old_val | 0x40);
    write_hps_reg("sys0", old_val & (~0x40));

    return RETURN_SUCCESS;
}

static int set_gating_mode(const char *chan, bool dsp) {
    char reg_name[8];
    snprintf(reg_name, sizeof(reg_name), "tx%s6", chan);
    return set_reg_bits(reg_name, 18, 1, dsp);
}

static int valid_gating_mode(const char *data, bool *dsp) {
    if (strncmp("dsp", data, strlen("dsp")) == 0) {
        *dsp = true;
    } else if (strncmp("output", data, strlen("output")) == 0) {
        *dsp = false;
    } else {
        return RETURN_ERROR_PARAM;
    }
    return RETURN_SUCCESS;
}
#endif //(NUM_TX_CHANNELS > 0)

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
            snprintf(ret, MAX_PROP_LEN, "%c", '0');
        }
    } else {
        synth_lut_disable(tx, channel);
    }

    return r;
}

#define X(ch)                                                                  \
    static int hdlr_rx_##ch##_rf_freq_lut_en(const char *data, char *ret) {    \
        return hdlr_XX_X_rf_freq_lut_en(data, ret, false, INT(ch));            \
    }
RX_CHANNELS
#undef X

#if (NUM_TX_CHANNELS > 0)
#define X(ch)                                                                  \
static int hdlr_tx_##ch##_rf_freq_lut_en(const char *data, char *ret) {    \
    return hdlr_XX_X_rf_freq_lut_en(data, ret, true, INT(ch));             \
}
TX_CHANNELS
#undef X
#endif //(NUM_TX_CHANNELS > 0)

/* -------------------------------------------------------------------------- */
/* -------------------------------- GATE ------------------------------------ */
/* -------------------------------------------------------------------------- */

static int valid_trigger_mode(const char *data, bool *edge) {

    if (strncmp("edge", data, strlen("edge")) == 0) {
        *edge = true;
    } else if (strncmp("level", data, strlen("level")) == 0) {
        *edge = false;
    } else {
        return RETURN_ERROR_PARAM;
    }

    return RETURN_SUCCESS;
}

static int valid_trigger_pol(const char *data, bool *positive) {

    if (strncmp("positive", data, strlen("positive")) == 0) {
        *positive = true;
    } else if (strncmp("negative", data, strlen("negative")) == 0) {
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
    if (strncmp("in", data, strlen("in")) == 0) {
        *in = true;
    } else if (strncmp("out", data, strlen("out")) == 0) {
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

    if(num != 0) {
        // Maps from the number of samples the user requested to the value to write to the register
        const uint64_t minus_one = num - 1;
        const uint64_t val = minus_one - (minus_one % 4) + 8;

        val_msw = val >> 32;
        val_lsw = val & 0xffffffff;
    } else {
        val_msw = 0;
        val_lsw = 0;
    }

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
        if( *val < MIN_EDGE_SAMPLE_NUM && *val != 0 ) {
            *val = MIN_EDGE_SAMPLE_NUM;
        }
        return RETURN_SUCCESS;
    } else {
        PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)");
        return RETURN_ERROR_PARAM;
    }
}

static int valid_time_gate_logic(const char *data, uint32_t *val) {
    int r;
    r = sscanf(data, "%" PRIu32, val);
    if (1 == r && (*val == 0 || *val == 1)) {
        return RETURN_SUCCESS;
    } else {
        PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)");
        return RETURN_ERROR_PARAM;
    }
}

static int valid_trig_time_disable(const char *data, uint32_t *val) {
    int r;
    r = sscanf(data, "%" PRIu32, val);
    if (1 == r && (*val == 0 || *val == 1)) {
        return RETURN_SUCCESS;
    } else {
        PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)");
        return RETURN_ERROR_PARAM;
    }
}

static int set_trig_time_gate_logic(bool tx, const char *chan, uint32_t val) {
    char reg_name[8];
    snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan,
             tx ? 6 : 9);    
    return set_reg_bits(reg_name, (tx?16:13), 1, val);
}

static int set_trig_time_disable(bool tx, const char *chan, uint32_t val) {
    char reg_name[8];
    snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan,
             tx ? 6 : 9);    
    return set_reg_bits(reg_name, (tx?17:14), 1, val);
}

#define X(ch)                                                                  \
    static int hdlr_rx_##ch##_trigger_sma_mode(const char *data, char *ret) {  \
        bool val;                                                              \
        int r = valid_trigger_mode(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trigger_mode(true, false, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_edge_backoff(const char *data,           \
                                                   char *ret) {                \
        uint32_t val;                                                          \
        int r = valid_edge_backoff(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_edge_backoff(false, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_edge_sample_num(const char *data,        \
                                                      char *ret) {             \
        uint64_t val;                                                          \
        int r = valid_edge_sample_num(data, &val);\
        if(r == RETURN_SUCCESS) {\
            r = set_edge_sample_num(false, #ch, val);\
            snprintf(ret, MAX_PROP_LEN, "%llu", val);\
            return r;\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_trig_sel(const char *data, char *ret) {  \
        uint32_t val;                                                          \
        int r = valid_trigger_sel(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trigger_sel(false, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_ufl_dir(const char *data, char *ret) {   \
        bool val;                                                              \
        int r = valid_trigger_dir(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trigger_ufl_dir(false, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_ufl_mode(const char *data, char *ret) {  \
        bool val;                                                              \
        int r = valid_trigger_mode(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trigger_mode(false, false, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_ufl_pol(const char *data, char *ret) {   \
        bool val;                                                              \
        int r = valid_trigger_pol(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trigger_ufl_pol(false, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_time_disable(const char *data, char *ret) {   \
        uint32_t val;                                                              \
        int r = valid_trig_time_disable(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trig_time_disable(false, #ch, val);                              \
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_time_gate_logic(const char *data, char *ret) {   \
        uint32_t val;                                                              \
        int r = valid_time_gate_logic(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trig_time_gate_logic(false, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                                 \
                                                                                      \
    /* Number of samples provessed during the last trigger */\
    static int hdlr_rx_##ch##_trigger_sample_counter(const char *data, char *ret) {\
        uint32_t val1;\
        uint32_t val2;\
        uint64_t val;\
        read_hps_reg("rxa13", &val2);\
        read_hps_reg("rxa14", &val1);\
        val = ((uint64_t)val1 << 32) | (val2);\
        snprintf(ret, MAX_PROP_LEN, "%llu\n", val);\
        return RETURN_SUCCESS;\
    }
RX_CHANNELS
#undef X

// Every uart send command must be accompanied by a uart read command
// so that the command prompt '>' is respected before the next send uart
// command can be used. This removes the need for delay calls in the uart
// send function.
static void ping(const int fd, uint8_t* buf, const size_t len)
{
    send_uart_comm(fd, buf, len);
    read_uart(fd);
}

// Verifies the rf pll is good. Returns 1 if the pll is locked
int check_rf_pll(int ch, int uart_fd) {
    snprintf(buf, sizeof(buf), "status -c %c -l\r", (char)ch + 'a');
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    int pll_chan; // dummy variable used to deal with the pll channel number being different
    int result = 0; // if sscanf fails, assume the PLL is unlocked
    if(sscanf((char *)uart_ret_buf, "CHAN: 0x%x, PLL Lock Detect: 0x%x", &pll_chan, &result) != 2) {
        PRINT(ERROR, "sscanf failure in check_rf_pll()\n");
    }
    return result;
}

/* -------------------------------------------------------------------------- */
/* --------------------------------- TX ------------------------------------- */
/* -------------------------------------------------------------------------- */
#define X(ch)                                                                  \
    static int hdlr_tx_##ch##_rf_dac_dither_en(const char *data, char *ret) {  \
        int en;                                                                \
                                                                               \
        if (sscanf(data, "%d", &en) != 1) {                                    \
            return RETURN_ERROR;                                               \
        }                                                                      \
        if (en < 0 || en > 1) {                                                \
            return RETURN_ERROR_PARAM;                                         \
        }                                                                      \
        snprintf(buf, sizeof(buf), "dac -c " STR(ch) " -l %u\r", en);          \
        snprintf(ret, MAX_PROP_LEN, "%u", en);                                 \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_dac_dither_mixer_en(const char *data,         \
                                                     char *ret) {              \
        int en;                                                                \
                                                                               \
        if (sscanf(data, "%d", &en) != 1 ) {                                   \
            return RETURN_ERROR;                                               \
        }                                                                      \
        if (en < 0 || en > 1) {                                                \
            return RETURN_ERROR_PARAM;                                         \
        }                                                                      \
        snprintf(buf, sizeof(buf), "dac -c " STR(ch) " -3 %u\r", en);          \
        snprintf(ret, MAX_PROP_LEN, "%u", en);                                 \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_dac_dither_sra_sel(const char *data,          \
                                                    char *ret) {               \
        int db;                                                                \
        int sel;                                                               \
                                                                               \
        if (sscanf(data, "%d", &db) != 1) {                                    \
            return RETURN_ERROR;                                               \
        }                                                                      \
        if (db < 6 || db > 96 || 0 != db % 6) {                                \
            return RETURN_ERROR_PARAM;                                         \
        }                                                                      \
                                                                               \
        snprintf(ret, MAX_PROP_LEN, "%u", db);                                 \
                                                                               \
        /* 96 dB (max) := 0x0, 90 dB := 0x1, ... 6 dB (min) := 0xf */          \
        sel = 16 - (db / 6);                                                   \
                                                                               \
        snprintf(buf, sizeof(buf), "dac -c " STR(ch) " -b %u\r", sel);         \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_dac_nco(const char *data, char *ret) {        \
        double freq;                                                           \
        sscanf(data, "%lf", &freq);                                            \
        uint64_t nco_steps = (uint64_t)round(freq * get_dac_nco_const());      \
        snprintf(ret, MAX_PROP_LEN, "%lf", (double)nco_steps / get_dac_nco_const());\
                                                                               \
        snprintf(buf, MAX_PROP_LEN,                                            \
            "dac -c " STR(ch) " -n %" PRIu32 " -o %" PRIu32 "\r",              \
            (uint32_t)(nco_steps >> 32), (uint32_t)nco_steps);                 \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_dac_temp(const char *data, char *ret) {       \
        strcpy(buf, "board -c " STR(ch) " -t\r");                              \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_freq_val(const char *data, char *ret) {       \
        uint64_t freq = 0;                                                     \
        sscanf(data, "%" SCNd64 "", &freq);                                    \
                                                                               \
        /* if freq = 0 or below allowed range, mute PLL */                     \
        if (freq < MIN_LO) {                                                   \
            if(freq != 0) {                                                    \
                PRINT(ERROR, "Requested tx lo %lu to low. Muting PLL\n", freq);\
            }                                                                  \
            strcpy(buf, "rf -c " STR(ch) " -z\r");                             \
            ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));            \
            snprintf(ret, MAX_PROP_LEN, "0");                                  \
                                                                               \
            return RETURN_SUCCESS;                                             \
        }                                                                      \
        /* if freq out of bounds, kill channel*/                               \
        if (freq > MAX_LO) {                                                   \
            strcpy(buf, "board -c " STR(ch) " -k\r");                          \
            ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));            \
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
            PRINT(ERROR, "Requested Synthesizer Frequency is > %lu Hz: "       \
                         "Shutting Down TX" STR(ch) ".\n", MAX_LO);            \
                                                                               \
            return RETURN_ERROR;                                               \
        }                                                                      \
                                                                               \
        pllparam_t pll;                                                        \
        /* load the reference frequency and such for RF PLL*/                  \
        if (PRODUCT_RTM_VER <= 10 || (PRODUCT_RTM_VER == 11 && INT(ch) < 2)) {                 \
            pll = pll_def_adf5355;                                             \
        } else {                                                               \
            pll = pll_def_lmx2572;                                             \
        }                                                                      \
        long double outfreq = 0;                                               \
                                                                               \
        /* round the requested freq to the nearest multiple of PLL ref */      \
        float n = (float)freq / pll.ref_freq;                                  \
        freq = round(n) * pll.ref_freq;                                        \
                                                                               \
        /* Ensure the requested freq is greater than the minimum */            \
        while(freq < pll.rf_out_min) {                                         \
            freq += pll.ref_freq;                                              \
        }                                                                      \
                                                                               \
        /* run the pll calc algorithm */                                       \
        outfreq = setFreq(&freq, &pll);                                        \
                                                                               \
        while ((pll.N < pll.n_min) && (pll.R < pll.r_max)) {                   \
            PRINT(INFO, "Retrying pll calc\n");                                \
            pll.R = pll.R + 1;                                                 \
            outfreq = setFreq(&freq, &pll);                                    \
        }                                                                      \
                                                                               \
        /* TODO: pll1.power setting TBD (need to modify pllparam_t) */         \
                                                                               \
        /* RTM10 and older, and RTM11 channel A, B use adf5355 */              \
        if (PRODUCT_RTM_VER <= 10 || (PRODUCT_RTM_VER == 11 && INT(ch) < 2)) {                 \
            strcpy(buf, "rf -c " STR(ch) " \r");                               \
            ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));            \
            if(!set_pll_frequency(uart_tx_fd[INT(ch)],                         \
                (uint64_t)LO_STEPSIZE, &pll, true, INT(ch), true))             \
            {                                                                  \
                PRINT(ERROR,                                                   \
                    "PLL lock failed when attempting to set freq to %lf\n",    \
                    outfreq);                                                  \
                snprintf(ret, MAX_PROP_LEN, "0");                              \
            }                                                                  \
        } else { /* RTM >= 11 use lmx2572 */                                   \
            set_lo_frequency(uart_tx_fd[INT(ch)], &pll, INT(ch));              \
        }                                                                      \
        snprintf(ret, MAX_PROP_LEN, "%Lf", outfreq);                           \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_freq_band(const char *data, char *ret) {      \
        snprintf(buf, MAX_PROP_LEN, "rf -c " STR(ch) " -b %s\r", data);        \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_common_lo(const char *data, char *ret) {      \
        if (PRODUCT_RTM_VER <= 7) {                                                    \
            /* common LO not supported by RTM6/7 hardware */                   \
            snprintf(ret, sizeof(NO_LMX_SUPPORT), NO_LMX_SUPPORT);             \
            return EXIT_SUCCESS;                                               \
        }                                                                      \
        /* TODO: make writing zero restore restore switch to correct setting */\
        /* TODO: make rf_freq_val avoid setting switch if common_lo on */      \
        int enable;                                                            \
        sscanf(data, "%i", &enable);                                           \
                                                                               \
        if (enable == 1 ) {                                                    \
            strcpy(buf, "rf -c " STR(ch) " -e 1\r");                           \
            ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));            \
        } else {                                                               \
            strcpy(buf, "rf -c " STR(ch) " -e 0\r");                           \
            ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));            \
        }                                                                      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_freq_i_bias(const char *data, char *ret) {    \
        sscanf(data, "%i", &(i_bias[INT(ch)]));                                \
        snprintf(buf, MAX_PROP_LEN, "rf -c " STR(ch) " -i %i -q %i -m\r",      \
            i_bias[INT(ch)], q_bias[INT(ch)]);                                 \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_freq_q_bias(const char *data, char *ret) {    \
        sscanf(data, "%i", &(q_bias[INT(ch)]));                                \
        snprintf(buf, MAX_PROP_LEN, "rf -c " STR(ch) " -i %i -q %i -m\r",      \
            i_bias[INT(ch)], q_bias[INT(ch)]);                                 \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_gain_val(const char *data, char *ret) {       \
        uint8_t gain;                                                          \
        sscanf(data, "%" SCNu8, &gain);                                        \
                                                                               \
        if (gain > 127) { gain = 127; }                                        \
        snprintf(buf, MAX_PROP_LEN, "rf -c " STR(ch) " -a %u\r", 127 - gain);  \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        snprintf(ret, MAX_PROP_LEN, "%" PRIu8, gain);                          \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_board_dump(const char *data, char *ret) {     \
        /* send the uart commands and read back the output and write to file */\
                                                                               \
        /* DAC */                                                              \
        strcpy(buf, "dump -c " STR(ch) " -d\r");                               \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);             \
                                                                               \
        /* GPIOX */                                                            \
        strcpy(buf, "dump -c " STR(ch) " -g\r");                               \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
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
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_status_rfld(const char *data, char *ret) {       \
        if(check_rf_pll(INT(ch), uart_tx_fd[INT(ch)])) {\
            snprintf(ret, MAX_PROP_LEN, "Locked\n");                           \
        } else {                                                               \
            snprintf(ret, MAX_PROP_LEN, "Unlocked\n");                         \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_status_dacld(const char *data, char *ret) {      \
        strcpy(buf, "status -c " STR(ch) " -p\r");                             \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_status_dacctr(const char *data, char *ret) {     \
        strcpy(buf, "status -c " STR(ch) " -e\r");                             \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_board_led(const char *data, char *ret) {      \
        snprintf(buf, MAX_PROP_LEN, "board -l %s\r", data);                    \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
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
        uint16_t base_factor;                                                  \
        double base_err = 0.0;                                                 \
        double rate;                                                           \
        sscanf(data, "%lf", &rate);                                            \
        \
        /* Limit for full rate DAC */\
        if(INT(ch) == 0 || INT(ch) == 1) {\
            rate = fmin(rate, get_base_sample_rate() / link_rate_divisor);\
        /* Limit for quarter rate DAC */\
        } else if (INT(ch) == 2 || INT(ch) == 3){\
            rate = fmin(rate, get_base_sample_rate() / (fmin(link_rate_divisor, 4)));\
        }\
                                                                               \
        /* get the error for base rate */                                      \
        base_factor =                                                          \
            get_optimal_sr_factor(rate, get_base_sample_rate(), &base_err);    \
        /* set the appropriate sample rate */                                  \
        int channel = INT(ch);                                                 \
        int shift = (channel%4)*8;                                             \
        write_hps_reg("tx" STR(ch) "1", base_factor);                          \
        snprintf(ret, MAX_PROP_LEN, "%lf",                                     \
            get_base_sample_rate() / (double)(base_factor + 1));               \
        /* Set gain adjustment */                                              \
        read_hps_reg("txga", &old_val);                                        \
        write_hps_reg("txga", (old_val & ~(0xff << shift)) |                   \
            (interp_gain_lut[(base_factor)] << shift));                        \
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
        /* Cap to allowable range */                                           \
        if(INT(ch) == 0 || INT(ch) == 1) {                                     \
            freq = fmin(freq, get_base_sample_rate() / 2.0);                   \
        /* On Crimson tx c/d operate at quarter rate*/                         \
        } else if(INT(ch) == 2 || INT(ch) == 3) {                              \
            freq = fmin(freq, get_base_sample_rate() / 8.0);                   \
        } else {                                                               \
            PRINT(ERROR, "Frequency range limit not set for this channel\n");  \
        }                                                                      \
                                                                               \
        /* write NCO adj */                                                    \
        uint32_t nco_steps = (uint32_t)round(freq * get_dsp_nco_const());      \
        write_hps_reg("tx" STR(ch) "0", nco_steps);                            \
        if (direction > 0) {                                                   \
            snprintf(ret, MAX_PROP_LEN, "-%lf",                                \
                (double)nco_steps / get_dsp_nco_const());                      \
        } else {                                                               \
            snprintf(ret, MAX_PROP_LEN, "%lf",                                 \
                (double)nco_steps / get_dsp_nco_const());                      \
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
    /* Put dsp in reset to clear uflow/oflow count, then take it out of reset*/\
    /* Delays present because it must be in reset for an amount of time*/      \
    /* Resets twice because resets don't work properly and need to be cleared*/\
        write_hps_reg_mask(reg4[INT(ch) + 4], 0x2, 0x2);                       \
        usleep(buffer_reset_delay);                                            \
        write_hps_reg_mask(reg4[INT(ch) + 4], 0x0, 0x2);                       \
        usleep(buffer_reset_delay);                                            \
        write_hps_reg_mask(reg4[INT(ch) + 4], 0x2, 0x2);                       \
        usleep(buffer_reset_delay);                                            \
        write_hps_reg_mask(reg4[INT(ch) + 4], 0x0, 0x2);                       \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_endian_swap(const char *data, char *ret) {       \
        /*0 for big endian packets, anything else for little endian*/          \
        int endian = 0;                                                        \
        sscanf(data, "%i", &endian);                                           \
        uint32_t bit_val;                                                      \
        if(endian) {                                                           \
            bit_val = 0x20000;                                                 \
        } else {                                                               \
            bit_val = 0;                                                       \
        }                                                                      \
        write_hps_reg_mask(reg4[INT(ch)+4], bit_val, 0x20000);                 \
        if(endian) {                                                           \
            snprintf(ret, MAX_PROP_LEN, "1");                                  \
        } else {                                                               \
            snprintf(ret, MAX_PROP_LEN, "0");                                  \
        }                                                                      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_jesd_delay_iq(const char *data, char *ret) {     \
        uint8_t i_delay = 0;                                                   \
        uint8_t q_delay = 0;                                                   \
        sscanf(data, "%" SCNu8 " %" SCNu8, &i_delay, &q_delay);                \
        if(i_delay > max_iq_delay ) {                                          \
            PRINT(ERROR, "invalid i delay %u. Setting to max value of %u.\n",  \
                i_delay, max_iq_delay);                                        \
            i_delay = max_iq_delay;                                            \
        }                                                                      \
        if(q_delay > max_iq_delay) {                                           \
            PRINT(ERROR, "invalid q delay %u. Setting to max value of %u.\n",  \
                q_delay, max_iq_delay);                                        \
            q_delay = max_iq_delay;                                            \
        }                                                                      \
        int32_t ch_select = 1 << (INT(ch) + 4);                                \
        int32_t reg_val = (q_delay << 6) | i_delay;                            \
        write_hps_reg("res_rw1", ch_select);                                   \
        write_hps_reg("res_rw0", reg_val | 0x1000);                            \
        write_hps_reg("res_rw0", reg_val);                                     \
        snprintf(ret, MAX_PROP_LEN, "%u %u\n", i_delay, q_delay);              \
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
        snprintf(ret, MAX_PROP_LEN, "sfp%c", (INT(ch)%2)+'a');                 \
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
        snprintf(ret, MAX_PROP_LEN, "%u", lvl);                                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
       DOES NOT PORT WELL.                                                     \
       flc14 uses different offsets for chanenls starting at index 14? */      \
    static int hdlr_tx_##ch##_qa_oflow(const char *data, char *ret) {          \
        uint32_t count;                                                        \
        char flc_reg[8];                                                       \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        sprintf(flc_reg, "flc%d", 14+(INT(ch)*2));                             \
        read_hps_reg(flc_reg, &count);                                         \
        snprintf(ret, MAX_PROP_LEN, "%u", count);                              \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* XXX:                                                                    \
     * DOES NOT PORT WELL.                                                     \
     * flc6 uses different offsets for channels starting at index 6? */        \
    static int hdlr_tx_##ch##_qa_uflow(const char *data, char *ret) {          \
        uint32_t count;                                                        \
        char flc_reg[8];                                                       \
        /* this is technically a 64-bit register, but we currently only need   \
         * the bottom 32-bits */                                               \
        sprintf(flc_reg, "flc%d", 6+(INT(ch)*2));                              \
        read_hps_reg(flc_reg, &count);                                         \
        snprintf(ret, MAX_PROP_LEN, "%u", count);                              \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_pwr(const char *data, char *ret) {               \
        uint32_t old_val;                                                      \
        uint8_t power;                                                         \
        sscanf(data, "%" SCNd8 "", &power);                                    \
                                                                               \
        /* check if power is already enabled */                                \
        if (power >= PWR_ON && tx_power[INT(ch)] == PWR_ON)                    \
            return RETURN_SUCCESS;                                             \
                                                                               \
        if (power >= PWR_ON) { /* power on */                                  \
            tx_power[INT(ch)] = PWR_ON;                                        \
                                                                               \
            /* disable dsp channel */                                          \
            read_hps_reg(reg4[INT(CH) + 4], &old_val);                         \
            write_hps_reg(reg4[INT(CH) + 4], old_val & ~0x100);                \
                                                                               \
            /* Resets all JESD */\
            /* Also resets all DSPs */\
            /* Only reset on boot to avoid resetting other channels during operation */\
            if(tx_jesd[INT(ch)] == JESD_UNINIT) {\
                sync_channels(15);\
            }\
            tx_jesd[INT(ch)] = JESD_INIT;\
            /* TODO: add check/warning if JESD is down*/\
                                                                               \
            /* enable dsp channels, and reset the DSP */                       \
            read_hps_reg(reg4[INT(ch) + 4], &old_val);                         \
            write_hps_reg(reg4[INT(ch) + 4], old_val | 0x100);                 \
            read_hps_reg(reg4[INT(ch) + 4], &old_val);                         \
            /* Toggles dsp reset to clear the buffer*/                         \
            /* Must be put in reset, taken out of reset, put back in reset to properly reset*/\
            write_hps_reg_mask(reg4[INT(ch) + 4], 0x2, 0x2);                   \
            usleep(buffer_reset_delay);                                        \
            write_hps_reg_mask(reg4[INT(ch) + 4], 0x0, 0x2);                   \
            usleep(buffer_reset_delay);                                        \
            write_hps_reg_mask(reg4[INT(ch) + 4], 0x2, 0x2);                   \
                                                                               \
        } else { /* power off */                                               \
            /* kill the channel */                                             \
            strcpy(buf, "board -c " STR(ch) " -k\r");                          \
            ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));            \
                                                                               \
            /* Toggles dsp reset to clear the buffer*/\
            /* Must be put in reset, taken out of reset, put back in reset to properly reset*/\
            write_hps_reg_mask(reg4[INT(ch) + 4], 0x2, 0x2);\
            usleep(buffer_reset_delay);\
            write_hps_reg_mask(reg4[INT(ch) + 4], 0x0, 0x2);\
            usleep(buffer_reset_delay);\
            /* leaves the in reset to disable DSP cores */\
            write_hps_reg_mask(reg4[INT(ch) + 4], 0x2, 0x2);\
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
    static int hdlr_tx_##ch##_jesd_status(const char *data, char *ret) {       \
        snprintf(buf, MAX_PROP_LEN, "status -c %s -g\r", STR(ch));             \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_serial(const char *data, char *ret) {      \
        strcpy(buf, "status -s\r");                                            \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_mcudevid(const char *data, char *ret) {    \
        strcpy(buf, "status -d\r");                                            \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_mcurev(const char *data, char *ret) {      \
        strcpy(buf, "status -v\r");                                            \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_mcufuses(const char *data, char *ret) {    \
        strcpy(buf, "status -f\r");                                            \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_hw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -h\r");                                             \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_fw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -v\r");                                             \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_sma_mode(const char *data, char *ret) {  \
        bool val;                                                              \
        int r = valid_trigger_mode(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trigger_mode(true, true, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_edge_backoff(const char *data,           \
    char *ret) {                                                               \
        uint32_t val;                                                          \
        int r = valid_edge_backoff(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_edge_backoff(true, #ch, val);                                  \
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_edge_sample_num(const char *data,        \
    char *ret) {                                                               \
        uint64_t val;                                                          \
        int r = valid_edge_sample_num(data, &val);\
        if(r == RETURN_SUCCESS) {\
            r = set_edge_sample_num(false, #ch, val);\
            snprintf(ret, MAX_PROP_LEN, "%llu", val);\
            return r;\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_trig_sel(const char *data, char *ret) {  \
        uint32_t val;                                                          \
        int r = valid_trigger_sel(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trigger_sel(true, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_ufl_dir(const char *data, char *ret) {   \
        bool val;                                                              \
        int r = valid_trigger_dir(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trigger_ufl_dir(true, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_ufl_mode(const char *data, char *ret) {  \
        bool val;                                                              \
        int r = valid_trigger_mode(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trigger_mode(false, true, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_ufl_pol(const char *data, char *ret) {   \
        bool val;                                                              \
        int r = valid_trigger_pol(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trigger_ufl_pol(true, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_gating(const char *data, char *ret) {    \
        bool val;                                                              \
        int r = valid_gating_mode(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_gating_mode(#ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_time_disable(const char *data, char *ret) {   \
        uint32_t val;                                                              \
        int r = valid_trig_time_disable(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trig_time_disable(true, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_time_gate_logic(const char *data, char *ret) {   \
        uint32_t val;                                                              \
        int r = valid_time_gate_logic(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trig_time_gate_logic(true, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                           \
                                                                                \
    /* Number of samples provessed during the last trigger */\
    static int hdlr_tx_##ch##_trigger_sample_counter(const char *data, char *ret) {\
        uint32_t val1;\
        uint32_t val2;\
        uint64_t val;\
        read_hps_reg("txa17", &val2);\
        read_hps_reg("txa18", &val1);\
        val = (val2);\
        val = ((uint64_t)val1 << 32) | (val2);\
        snprintf(ret, MAX_PROP_LEN, "%llu\n", val);\
        return RETURN_SUCCESS; \
    }
TX_CHANNELS
#undef X

/* -------------------------------------------------------------------------- */
/* --------------------------------- RX ------------------------------------- */
/* -------------------------------------------------------------------------- */

#define X(ch)                                                                  \
    static int hdlr_rx_##ch##_rf_freq_val(const char *data, char *ret) {       \
        uint64_t freq = 0;                                                     \
        uint64_t lmx_freq = 0; /* for use with avery 40GHz FE */               \
        long double outfreq = 0;                                               \
        pllparam_t pll;                                                        \
        sscanf(data, "%" SCNd64 "", &freq);                                    \
                                                                               \
        /* if freq = 0 or below allowed range, mute PLL */                     \
        if (freq < MIN_LO) {                                                   \
            if(freq != 0) {                                                    \
                PRINT(ERROR, "Requested rx lo %lu to low. Muting PLL\n", freq);\
            }                                                                  \
            strcpy(buf, "rf -c " STR(ch) " -z\r");                             \
            ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));            \
                                                                               \
            snprintf(ret, MAX_PROP_LEN, "0");                                  \
                                                                               \
            return RETURN_SUCCESS;                                             \
        }                                                                      \
                                                                               \
        /* NOTE: in RX_40GHZ_FE the 40GHz board is physically connected where tx would normally be */\
        if (RX_40GHZ_FE) {                                                     \
            if (freq > MAX_RF_FREQ) { /*out of bounds, too high*/              \
                /* mute FE LO, RF LO will be muted when freq > MAX_LO below*/  \
                strcpy(buf, "rf -c " STR(ch) " -z\r");                         \
                ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));        \
            } else if (freq > 20000000000) { /*Front end high band 20GHz - 40GHz*/\
                /* select the band*/                                           \
                strcpy(buf, "rf -c " STR(ch) " -b 3\r");                       \
                ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));        \
                /* load the reference frequency and such for LMX2595*/         \
                pll = pll_def_lmx2595_avery;                                   \
                /* round the requested freq to the nearest multiple of phase*/ \
                /* detector frequency around the 650MHz target IF*/            \
                /* multiplied by four times because of ADAR2004 quadrupler*/   \
                float n = ((float)freq - AVERY_IF ) / (4 * pll.ref_freq / pll.R);\
                lmx_freq = round(n) * pll.ref_freq / pll.R;                    \
                /* run the pll calc algorithm */                               \
                outfreq = setFreq(&lmx_freq, &pll);                            \
                while ((pll.N < pll.n_min) && (pll.R < pll.r_max)) {           \
                    PRINT(INFO, "Retrying pll calc\n");                        \
                    pll.R = pll.R + 1;                                         \
                    outfreq = setFreq(&lmx_freq, &pll);                        \
                }                                                              \
                lmx_freq = (uint64_t)outfreq;                                  \
                /* set internal band of ADAR2004 in MHz*/                      \
                strcpy(buf, "rf -c " STR(ch) " -f ");                          \
                sprintf(buf + strlen(buf), "%" PRIu64 "", lmx_freq/1000000);   \
                strcat(buf, "\r");                                             \
                ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));        \
                /* TODO: pll1.power setting TBD (need to modify pllparam_t) */ \
                /* Send Parameters over to the MCU */                          \
                set_lo_frequency(uart_tx_fd[INT(ch)], &pll, INT(ch));          \
                /* set the lmx to use output B */                              \
                if (lmx_freq >= 7500000000) {                                  \
                    /* set outB to use VCO directly */                         \
                    strcpy(buf, "lmx -c " STR(ch) " -J 4\r");                  \
                } else {                                                       \
                    /* set outB to use CH_DIV directly */                      \
                    strcpy(buf, "lmx -c " STR(ch) " -J 3\r");                  \
                }                                                              \
                ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));        \
                /* set lmx_freq to account for ADAR2004 quadrupler when print to state tree*/ \
                lmx_freq *= 4;                                                 \
                /* set the freq to 650MHz so normal RF chain centered on IF */ \
                freq = AVERY_IF;                                               \
            } else if (freq > 6000000000) { /*Front end mid band 6GHz - 40GHz*/\
                /* select the band*/                                           \
                strcpy(buf, "rf -c " STR(ch) " -b 2\r");                       \
                ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));        \
                /* load the reference frequency and such for LMX2595*/         \
                pll = pll_def_lmx2595_avery;                                   \
                /* round the requested freq to the nearest multiple of phase*/ \
                /* detector frequency around the 650MHz target IF*/            \
                float n = ((float)freq - AVERY_IF ) / ( pll.ref_freq / pll.R );\
                lmx_freq = round(n) * pll.ref_freq / pll.R;                    \
                /* run the pll calc algorithm */                               \
                outfreq = setFreq(&lmx_freq, &pll);                            \
                while ((pll.N < pll.n_min) && (pll.R < pll.r_max)) {           \
                    PRINT(INFO, "Retrying pll calc\n");                        \
                    pll.R = pll.R + 1;                                         \
                    outfreq = setFreq(&lmx_freq, &pll);                        \
                }                                                              \
                lmx_freq = (uint64_t)outfreq;                                  \
                /* TODO: pll1.power setting TBD (need to modify pllparam_t) */ \
                /* Send Parameters over to the MCU */                          \
                set_lo_frequency(uart_tx_fd[INT(ch)], &pll, INT(ch));          \
                /* uses output A by default */                                 \
                /* set the freq to 650MHz so normal RF chain centered on IF */ \
                freq = AVERY_IF;                                               \
            } else { /* Front end low band */                                  \
                strcpy(buf, "rf -c " STR(ch) " -b 1\r");                       \
                ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));        \
            }                                                                  \
        } /*fi RX_40GHZ_FE*/                                                   \
        /* if freq out of bounds, kill channel */                              \
        if (freq > MAX_LO) {                                                   \
            strcpy(buf, "board -c " STR(ch) " -k\r");                          \
            ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));            \
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
            PRINT(ERROR, "Requested Synthesizer Frequency is > %lu Hz: "       \
                         "Shutting Down RX" STR(ch) ".\n", MAX_LO);            \
                                                                               \
            return RETURN_ERROR;                                               \
        }                                                                      \
                                                                               \
        /* load the reference frequency and such for RF PLL*/                  \
        if (PRODUCT_RTM_VER <= 10 || (PRODUCT_RTM_VER == 11 && INT(ch) < 2)) {                 \
            pll = pll_def_adf5355;                                             \
        } else {                                                               \
                pll = pll_def_lmx2572;                                         \
        }                                                                      \
                                                                               \
        /* round the requested freq to the nearest multiple of PLL ref */      \
        float n = (float)freq / pll.ref_freq;                                  \
        freq = round(n) * pll.ref_freq;                                        \
                                                                               \
        /* Ensure the requested freq is greater than the minimum */            \
        while(freq < pll.rf_out_min) {                                         \
            freq += pll.ref_freq;                                              \
        }                                                                      \
                                                                               \
        /* run the pll calc algorithm */                                       \
        outfreq = setFreq(&freq, &pll);                                        \
                                                                               \
        while ((pll.N < pll.n_min) && (pll.R < pll.r_max)) {                   \
            PRINT(INFO, "Retrying pll calc\n");                                \
            pll.R = pll.R + 1;                                                 \
            outfreq = setFreq(&freq, &pll);                                    \
        }                                                                      \
                                                                               \
        /* TODO: pll1.power setting TBD (need to modify pllparam_t) */         \
                                                                               \
        if (PRODUCT_RTM_VER <= 10 || (PRODUCT_RTM_VER == 11 && INT(ch) < 2) ) { /* adf5355 */  \
            strcpy(buf, "rf -c " STR(ch) " \r");                               \
            ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));            \
            if(!set_pll_frequency(uart_rx_fd[INT(ch)],                         \
                (uint64_t)LO_STEPSIZE, &pll, false, INT(ch), true))            \
            {                                                                  \
                PRINT(ERROR,                                                   \
                    "PLL lock failed when attempting to set freq to %lf\n",    \
                    outfreq);                                                  \
                snprintf(ret, MAX_PROP_LEN, "0");                              \
            }                                                                  \
        } else { /* RTM >= 11 use lmx2572 */                                   \
                set_lo_frequency(uart_rx_fd[INT(ch)], &pll, INT(ch));          \
        }                                                                      \
        outfreq += (long double)lmx_freq;                                      \
        snprintf(ret, MAX_PROP_LEN, "%Lf", outfreq);                           \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_freq_lna(const char *data, char *ret) {       \
        strcpy(buf, "rf -c " STR(ch) " -l ");                                  \
        strcat(buf, data);                                                     \
        strcat(buf, "\r");                                                     \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_freq_band(const char *data, char *ret) {      \
        snprintf(buf, MAX_PROP_LEN, "rf -c " STR(ch) " -b %s\r", data);        \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        if (RX_40GHZ_FE) {                                                     \
            /* for crimson baseband, FE board must be baseband, otherwise FE
             * band set in hdlr_rx_##ch##_rf_freq_val() */                     \
            uint8_t highband;                                                  \
            sscanf(data, "%" SCNu8 "", &highband);                             \
            if (highband == 0) {                                               \
                snprintf(buf, MAX_PROP_LEN, "rf -c " STR(ch) " -b 1\r");       \
                ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));        \
            }                                                                  \
        }                                                                      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_common_lo(const char *data, char *ret) {      \
        if (PRODUCT_RTM_VER <= 7) {                                                    \
            /* common LO not supported by RTM6/7 hardware */                   \
            snprintf(ret, sizeof(NO_LMX_SUPPORT), NO_LMX_SUPPORT);             \
            return EXIT_SUCCESS;                                               \
        }                                                                      \
        /* TODO: make writing zero restore restore switch to correct setting */\
        /* TODO: make rf_freq_val avoid setting switch if common_lo on */      \
        int enable;                                                            \
        sscanf(data, "%i", &enable);                                             \
                                                                               \
        if (enable == 1 ) {                                                    \
            strcpy(buf, "rf -c " STR(ch) " -e 1\r");                           \
            ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));            \
        } else {                                                               \
            strcpy(buf, "rf -c " STR(ch) " -e 0\r");                           \
            ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));            \
        }                                                                      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                          \
    static int hdlr_rx_##ch##_rf_gain_val(const char *data, char *ret) {       \
        uint8_t gain;                                                          \
        sscanf(data, "%" SCNu8, &gain);                                        \
                                                                               \
        /* LMH6521 gain has 6-bit control word [5:0] but for some reason here  \
         * we use bits [6:1], and if bit 0 is high we increase the gain? */    \
        if (gain > 126) { gain = 126; }                                        \
        if (gain % 2) { gain++; } /* round odd number up to next even number*/ \
                                                                               \
        snprintf(buf, MAX_PROP_LEN, "vga -c " STR(ch) " -g %" PRIu8 "\r",      \
            gain >> 1);                                                        \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        snprintf(ret, MAX_PROP_LEN, "%" PRIu8, gain);                          \
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
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
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
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);             \
                                                                               \
        /* GPIOX */                                                            \
        strcpy(buf, "dump -c " STR(ch) " -g\r");                               \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);           \
                                                                               \
        /* ADC Driver */                                                       \
        strcpy(buf, "dump -c " STR(ch) " -v\r");                               \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
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
        strcpy(buf, "board -c " STR(ch) " -u\r");                              \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_status_rfld(const char *data, char *ret) {       \
        if(check_rf_pll(INT(ch), uart_rx_fd[INT(ch)])) {\
            snprintf(ret, MAX_PROP_LEN, "Locked\n");\
        } else {\
            snprintf(ret, MAX_PROP_LEN, "Unlocked\n");\
        }\
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_status_adcalarm(const char *data, char *ret) {   \
        strcpy(buf, "status -c " STR(ch) " -a\r");                             \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_board_led(const char *data, char *ret) {      \
        strcpy(buf, "board -l\r");                                             \
        strcat(buf, data);                                                     \
        strcat(buf, "\r");                                                     \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
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
        uint16_t base_factor;                                                  \
        double base_err = 0.0;                                                 \
        double rate;                                                           \
        sscanf(data, "%lf", &rate);                                            \
        \
        /* Caps the rate to the maximum */\
        rate = fmin(rate, get_base_sample_rate() / link_rate_divisor);\
                                                                               \
        /* get the error for base rate */                                      \
        base_factor =                                                          \
            get_optimal_sr_factor(rate, get_base_sample_rate(), &base_err);    \
        /* set the appropriate sample rate */                                  \
        memset(ret, 0, MAX_PROP_LEN);                                          \
        int channel = INT(ch);                                                 \
        int shift = (channel%4)*8;                                             \
                                                                               \
        int gain_factor;                                                       \
            write_hps_reg("rx" STR(ch) "1", base_factor);                      \
            snprintf(ret, MAX_PROP_LEN, "%lf", get_base_sample_rate() / (double)(base_factor + 1)); \
            /*Set gain adjustment*/                                            \
            gain_factor = decim_gain_lut[(base_factor)];                       \
            read_hps_reg("rxga", &old_val);                                    \
            write_hps_reg("rxga", (old_val & ~(0xff << shift)) |               \
                                  (((uint16_t)gain_factor) << shift));         \
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
        /* Cap to allowable range */\
        freq = abs(freq);\
        freq = fmin(freq, get_base_sample_rate() / 2.0);\
        \
        /* write NCO adj */                                                    \
        uint32_t nco_steps = (uint32_t)round(freq * get_dsp_nco_const());      \
        write_hps_reg("rx" STR(ch) "0", nco_steps);                            \
        if (direction > 0) {                                                   \
            snprintf(ret, MAX_PROP_LEN, "-%lf", (double)nco_steps / get_dsp_nco_const());\
        } else {                                                               \
            snprintf(ret, MAX_PROP_LEN, "%lf", (double)nco_steps / get_dsp_nco_const());\
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
    /*0 for big endian packets, anything else for little endian*/\
    static int hdlr_rx_##ch##_endian_swap(const char *data, char *ret) {      \
        int endian = 0;\
        sscanf(data, "%i", &endian);\
        int bit_val;\
        if(endian) {\
            bit_val = 0x20000;\
        } else {\
            bit_val = 0;\
        }\
        write_hps_reg_mask(reg4[INT(ch)], bit_val, 0x20000);\
        if(endian) {\
            snprintf(ret, MAX_PROP_LEN, "1");\
        } else {\
            snprintf(ret, MAX_PROP_LEN, "0");\
        }\
        return RETURN_SUCCESS;\
    }                                                                          \
                                                                                \
    static int hdlr_rx_##ch##_jesd_delay_iq(const char *data, char *ret) {      \
        int i_delay = 0;\
        int q_delay = 0;\
        sscanf(data, "%i %i", &i_delay, &q_delay);\
        if(i_delay < 0) {\
            PRINT(ERROR, "i delay must be equal to or greater than 0. Setting i delay to 0.\n");\
            i_delay = 0;\
        } else if(i_delay > max_iq_delay ) {\
            PRINT(ERROR, "i delay must be less than or equal to %i. Setting i delay to %i.\n", max_iq_delay, max_iq_delay);\
            i_delay = max_iq_delay;\
        }\
        if(q_delay < 0) {\
            PRINT(ERROR, "q delay must be equal to or greater than 0. Setting q delay to 0\n");\
            q_delay = 0;\
        } else if(q_delay > max_iq_delay) {\
            q_delay = max_iq_delay;\
            PRINT(ERROR, "q delay must be less than or equal to %i. Setting i delay to %i.\n", max_iq_delay, max_iq_delay);\
        }\
        int32_t ch_select = 1 << INT(ch);\
        int32_t reg_val = (q_delay << 6) | i_delay;\
        write_hps_reg("res_rw1", ch_select);\
        write_hps_reg("res_rw0", reg_val | 0x1000);\
        write_hps_reg("res_rw0", reg_val);\
        snprintf(ret, MAX_PROP_LEN, "%i %i", i_delay, q_delay);\
        return RETURN_SUCCESS;\
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
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_link_iface(const char *data, char *ret) {        \
        /* TODO: FW support for streaming to management port required */       \
        char channel = (INT(ch)%2)+'a';                                        \
        snprintf(ret, MAX_PROP_LEN, "%s%c", "sfp", channel);                                  \
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
    static int hdlr_rx_##ch##_prime_trigger_stream(const char *data, char *ret) {     \
        /*Forces rx to start sreaming data, only use if the conventional method using the sfp port is not possible*/\
        if(data[0]=='0') {\
            /*disables channel and puts the dsp in reset*/\
            write_hps_reg_mask(reg4[INT(ch)], 0x002, 0x102);\
            rx_stream[INT(ch)] = STREAM_OFF;\
            /*Ignores sma (enabling normal stream command)*/\
            set_property("rx/" STR(ch) "/trigger/trig_sel", "0");\
            /*turn time disable off*/\
            set_property("rx/" STR(ch) "/trigger/time_disable", "0");\
        } else {\
            rx_stream[INT(ch)] = STREAM_ON;\
            /*Stream when sma trigger (has the side effect of disabling normal stream commands)*/\
            set_property("rx/" STR(ch) "/trigger/trig_sel", "1");\
            /*disable time trigger*/\
            set_property("rx/" STR(ch) "/trigger/time_disable", "1");\
            /*enable channel and take the dsp out of reset*/\
            write_hps_reg_mask(reg4[INT(ch)], 0x100, 0x102);\
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
            snprintf(ret, MAX_PROP_LEN, "%u", rx_stream[INT(ch)]); /* Alert File Tree */      \
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
                snprintf(ret, MAX_PROP_LEN, "%u", 0); /* Alert File Tree */                   \
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
            /* disable dsp channels */                                         \
            read_hps_reg(reg4[INT(CH)], &old_val);                               \
            write_hps_reg(reg4[INT(CH)], old_val & ~0x100);                      \
                                                                               \
            /* Resets all JESD */\
            /* Also resets all DSPs */\
            /* Only reset on boot to avoid resetting other channels during operation */\
            if(rx_jesd[INT(ch)] == JESD_UNINIT) {\
                sync_channels(15);\
            }\
            rx_jesd[INT(ch)] = JESD_INIT;\
            /* TODO: add check/warning if JESD is down*/\
                                                                               \
            /* Enable active dsp channels, and reset DSP */                    \
            if (rx_stream[INT(ch)] == STREAM_ON) {                               \
                read_hps_reg(reg4[INT(ch)], &old_val);                           \
                write_hps_reg(reg4[INT(ch)], old_val | 0x100);                   \
                read_hps_reg(reg4[INT(ch)], &old_val);                           \
                write_hps_reg(reg4[INT(ch)], old_val | 0x2);                     \
                write_hps_reg(reg4[INT(ch)], old_val &(~0x2));                   \
            }                                                              \
                                                                               \
            /* power off & stream off */                                       \
        } else {                                                               \
            rx_power[INT(ch)] = PWR_OFF;                                       \
            rx_stream[INT(ch)] = STREAM_OFF;                                   \
                                                                               \
            if (RX_40GHZ_FE) {                                                 \
                /* mute the front end board */                                 \
                strcpy(buf, "rf -c " STR(ch) " -z\r");                         \
                ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));        \
            }                                                                  \
                                                                               \
            /* kill the channel */                                             \
            strcpy(buf, "board -c " STR(ch) " -k\r");                          \
            ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));            \
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
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_mcudevid(const char *data, char *ret) {    \
        strcpy(buf, "status -d\r");                                            \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_mcurev(const char *data, char *ret) {      \
        strcpy(buf, "status -v\r");                                            \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_mcufuses(const char *data, char *ret) {    \
        strcpy(buf, "status -f\r");                                            \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_hw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -h\r");                                             \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_fw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -v\r");                                             \
        ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));      \
        strcpy(ret, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }\
    \
    /* res_ro0 bits 0,1 indicate ch A, bits 2,3 indicate ch B... */\
    static int hdlr_rx_##ch##_jesd_status(const char *data, char *ret) {       \
        uint32_t reg_val = 0;                                                  \
        read_hps_reg("res_ro0", &reg_val);                                    \
        uint8_t shift = 2 * INT(ch);                                           \
        uint32_t ch_jesd_info = 3 & (reg_val >> shift);                        \
        if (ch_jesd_info == 3){                                                \
            snprintf(ret, sizeof("good"), "good");                             \
        } else {                                                               \
            snprintf(ret, sizeof("bad"), "bad");                               \
        }                                                                      \
        return RETURN_SUCCESS;                                                 \
    }
RX_CHANNELS
#undef X

#if (RX_40GHZ_FE)
#define X(ch)                                                                  \
    static int hdlr_rx_##ch##_fe_lna(const char *data, char *ret) {            \
        uint8_t enable;                                                        \
        sscanf(data, "%" SCNd8 "", &enable);                                   \
        if(enable) {                                                           \
            strcpy(buf, "rf -c " STR(ch) " -l 0\r");                           \
            strcpy(ret, "1");                                                  \
        } else {                                                               \
            strcpy(buf, "rf -c " STR(ch) " -l 1\r");                           \
            strcpy(ret, "0");                                                  \
        }                                                                      \
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_fe_gain(const char *data, char *ret) {           \
        uint8_t gain;                                                          \
        sscanf(data, "%" SCNd8 "", &gain);                                     \
        if(gain > 7) { gain = 7; }                                             \
        snprintf(buf, MAX_PROP_LEN, "rf -c " STR(ch) " -g %" PRIu8 "\r", gain);\
        ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                \
        snprintf(ret, MAX_PROP_LEN, "%" PRIu8, gain);                          \
        return RETURN_SUCCESS;                                                 \
    }
RX_CHANNELS
#undef X
#endif // (RX_40GHZ_FE)

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
    snprintf(ret, MAX_PROP_LEN, "%x", mask);

    return RETURN_SUCCESS;
}

// Issues reboot command to rx board, does not wait for reboot to complete
// NOTE: sysref must be continuous during reboot
static int hdlr_cm_quick_reboot_rx(const char *data, char *ret) {
    int32_t request;

    sscanf(data, "%i", &request);

    if(request > 0) {
        snprintf(buf, MAX_PROP_LEN, "board -r\r");
        // Issues reboot command (in Vaunt all radio front ends are on the same board so rebooting one reboots all)
        send_uart_comm(uart_rx_fd[0], (uint8_t *)buf, strlen(buf));
        snprintf(ret, MAX_PROP_LEN, "1");
    } else {
        snprintf(ret, MAX_PROP_LEN, "0");
    }

    return RETURN_SUCCESS;
}

// Issues reboot command to tx board, does not wait for reboot to complete
// NOTE: sysref must be continuous during reboot
static int hdlr_cm_quick_reboot_tx(const char *data, char *ret) {
    int32_t request;

    sscanf(data, "%i", &request);

    if(request > 0) {
        snprintf(buf, MAX_PROP_LEN, "board -r\r");
        // Issues reboot command (in Vaunt all radio front ends are on the same board so rebooting one reboots all)
        send_uart_comm(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));
        snprintf(ret, MAX_PROP_LEN, "1");
    } else {
        snprintf(ret, MAX_PROP_LEN, "0");
    }

    return RETURN_SUCCESS;
}

// Issues reboot command to rx board, does not wait for reboot to complete
// NOTE: sysref must be continuous during reboot
static int hdlr_cm_wait_for_reboot_rx(const char *data, char *ret) {
    int32_t request;

    sscanf(data, "%i", &request);

    if(request > 0) {
        // Waits for the boot message to fnish printing
        read_uart(uart_rx_fd[0]);
        int bytes_read = strnlen((char *)uart_ret_buf, MAX_PROP_LEN);
        if(bytes_read != 0) {
            snprintf(ret, MAX_PROP_LEN, "reboot complete");
        } else {
            snprintf(ret, MAX_PROP_LEN, "reboot failed");
        }
    } else {
        snprintf(ret, MAX_PROP_LEN, "no action");
    }

    return RETURN_SUCCESS;
}

// Issues reboot command to tx board, does not wait for reboot to complete
// NOTE: sysref must be continuous during reboot
static int hdlr_cm_wait_for_reboot_tx(const char *data, char *ret) {
    int32_t request;

    sscanf(data, "%i", &request);

    if(request > 0) {
        // Waits for the boot message to fnish printing
        read_uart(uart_tx_fd[0]);
        int bytes_read = strnlen((char *)uart_ret_buf, MAX_PROP_LEN);
        if(bytes_read != 0) {
            snprintf(ret, MAX_PROP_LEN, "reboot complete");
        } else {
            snprintf(ret, MAX_PROP_LEN, "reboot failed");
        }
    } else {
        snprintf(ret, MAX_PROP_LEN, "no action");
    }

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

    mask_rx = cm_chanmask_get("/var/volatile/crimson/state/cm/chanmask-rx");

    sscanf(data, "%d", &atten);

    sprintf(inbuf, "%d", atten);

    for (i = 0; i < NUM_RX_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }
#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_rx_##ch##_rf_atten_val;
        RX_CHANNELS
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

    mask_rx = cm_chanmask_get("/var/volatile/crimson/state/cm/chanmask-rx");

    sscanf(data, "%lf", &gain);

    sprintf(inbuf, "%lf", gain);

    for (i = 0; i < NUM_RX_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }

#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_rx_##ch##_rf_gain_val;
        RX_CHANNELS
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

#if (NUM_TX_CHANNELS > 0)
static int hdlr_cm_chanmask_tx(const char *data, char *ret) {
    uint32_t mask;
    
    if (1 != sscanf(data, "%x", &mask)) {
        return RETURN_ERROR_PARAM;
    }
    
    mask &= 0xffff;
    snprintf(ret, MAX_PROP_LEN, "%x", mask);
    
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

    mask_tx = cm_chanmask_get("/var/volatile/crimson/state/cm/chanmask-tx");

    sscanf(data, "%lf", &gain);

    sprintf(inbuf, "%lf", gain);

    for (i = 0; i < NUM_TX_CHANNELS; i++) {

        if (0 == (mask_tx & (1 << i))) {
            continue;
        }

#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_tx_##ch##_rf_gain_val;
        TX_CHANNELS
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

// Enables streaming at a target buffer level, instead of relying on start and end of burst commands
// 0 stops all force streaming. To start streaming set this using a value where each bit corresponds to each channel
// ie 1 to only stream from ch A, 2 for chB, 4 for chC, 5 for chA and chC
// using -1 for streaming all
static int hdlr_cm_tx_force_stream(const char *data, char *ret) {
    int32_t request = 0;
    sscanf(data, "%i", &request);

    for(int n = 0; n < NUM_TX_CHANNELS; n++) {
        int32_t request_bit = (request >> n) & 1;
        request_bit = request_bit << 16;
        // Setting bit 16 to high enables this modes, low sets it to normal
        write_hps_reg_mask(reg4[n + 4], request_bit, 1 << 16);
    }

    return RETURN_SUCCESS;
}
#endif //(NUM_TX_CHANNELS > 0)

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

    mask_rx = cm_chanmask_get("/var/volatile/crimson/state/cm/chanmask-rx");
    mask_tx = cm_chanmask_get("/var/volatile/crimson/state/cm/chanmask-tx");

    sync_mask = (mask_tx << 16) | mask_rx;
    if (0 == sync_mask) {
        return RETURN_SUCCESS;
    }

    sscanf(data, "%lf", &freq);

    sync_mode |= 1; // internal sync
    write_hps_reg("sync_mode", sync_mode);

    write_hps_reg("sync_mask", sync_mask);

    sprintf(inbuf, "%lf", freq);

    for (i = 0; i < NUM_RX_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }

#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_rx_##ch##_rf_gain_val;
        RX_CHANNELS
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

    for (i = 0; i < NUM_TX_CHANNELS; i++) {

        if (0 == (mask_tx & (1 << i))) {
            continue;
        }

#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_tx_##ch##_rf_freq_val;
        TX_CHANNELS
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

    mask_rx = cm_chanmask_get("/var/volatile/crimson/state/cm/chanmask-rx");
    mask_tx = cm_chanmask_get("/var/volatile/crimson/state/cm/chanmask-tx");

    sync_mask = (mask_tx << 16) | mask_rx;
    if (0 == sync_mask) {
        return RETURN_SUCCESS;
    }

    sscanf(data, "%lf", &freq);

    sync_mode |= 1; // internal sync
    write_hps_reg("sync_mode", sync_mode);

    write_hps_reg("sync_mask", sync_mask);

    sprintf(inbuf, "%lf", freq);

    for (i = 0; i < NUM_RX_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }

#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_rx_##ch##_dsp_nco_adj;
        RX_CHANNELS
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

    for (i = 0; i < NUM_TX_CHANNELS; i++) {

        if (0 == (mask_tx & (1 << i))) {
            continue;
        }
#define X(ch)                                                                  \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_tx_##ch##_dsp_nco_adj;
        TX_CHANNELS
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

//makes all rx begin streaming data in phase with each other, using an sma trigger
//0 stops all fore streaming. To start streaming set this using a value where each bit corresponds to ech channel
//ie 1 to only stream from ch A, 2 for chB, 4 for chC, 5 for chA and chC
//using -1 for streaming all
static int hdlr_cm_rx_force_stream(const char *data, char *ret) {
    int64_t stream = 0;
    sscanf(data, "%lli", &stream);
    char path_buffer[MAX_PATH_LEN];
    if(stream != 0) {
        //stop any force streaming by bringing the trigger low
        //sets the sma trigger to act as an input
        set_property("fpga/trigger/sma_dir", "input");
        //force the trigger input to always read as high
        set_property("fpga/trigger/sma_override", "1");
        //sets the sma trigger to activate when it is low (override bit will make it high)
        //the sma trigger should be inactive from here until the end of the function
        set_property("fpga/trigger/sma_pol", "negative");
        // configure the channels specified for force streaming, and ensure others are not
        for(int n = 0; n < NUM_RX_CHANNELS; n++) {
            if(stream & 1 << n) {
                sprintf(path_buffer, "rx/%c/prime_trigger_stream", n+'a');
                set_property(path_buffer, "1");
                sprintf(path_buffer, "rx/%c/trigger/sma_mode", n+'a');
                set_property(path_buffer, "level");
            } else {
                sprintf(path_buffer, "rx/%c/prime_trigger_stream", n+'a');
                set_property(path_buffer, "0");
            }
        }
        //sets the sma to activate when high (sma_override is forcing it high)
        // this starts the streaming for all channels at once
        set_property("fpga/trigger/sma_pol", "positive");
    } else {
        //sets the sma trigger to activate when it is low (override bit will make it high)
        //the sma trigger should be inactive from here until the end of the function
        set_property("fpga/trigger/sma_pol", "negative");
        //stops streaming on everything, note that it does not clean up a lot of the changes done when activating synchronized force streaming
        for(int n = 0; n < NUM_RX_CHANNELS; n++) {
            //stops any existing force streaming
            sprintf(path_buffer, "rx/%c/prime_trigger_stream", n+'a');
            set_property(path_buffer, "0");
        }
        //stop ignoring the trigger state in case it will be used later
        set_property("fpga/trigger/sma_override", "0");
    }
    return RETURN_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/* --------------------------------- TIME ----------------------------------- */
/* -------------------------------------------------------------------------- */
// Sets the time, updates on the next PPS
// Note sure what the difference in intended use is, since in both cases the clock's full seconds only increments on pps
static int hdlr_time_clk_pps(const char *data, char *ret) {
    long double time;
    sscanf(data, "%Lf", &time);
    // Write the number of whole seconds
    // lower half
    write_hps_reg("sys9", (uint32_t)(((uint64_t)time) & 0x00000000FFFFFFFF));
    // upper half
    write_hps_reg("sys10",
                  (uint32_t)(((uint64_t)time) >> 32) & 0x00000000FFFFFFFF);

    // Write the fractional seconds in ticks
    uint64_t fractional_time = (uint64_t) round((time - (double)((uint64_t)time)) * TICK_RATE);
    // lower half
    write_hps_reg("sys11", (uint32_t)(((uint64_t)fractional_time) & 0x00000000FFFFFFFF));
    // upper half
    write_hps_reg("sys12", (uint32_t)(((uint64_t)fractional_time) >> 32) & 0x00000000FFFFFFFF);

    // Toggling this bit sets the time
    write_hps_reg_mask("sys13", 1, 1);
    write_hps_reg_mask("sys13", 0, 1);

    return RETURN_SUCCESS;
}

static int hdlr_time_clk_set_time(const char *data, char *ret) {
    // Note that this function is used by both server.c and crimson-fpga-time-sync.sh (and by extension crimson-fpga-time-sync.service). Ensure that if this funciton or its state tree path are updated, those uses are also updated
    long double time;
    sscanf(data, "%Lf", &time);
    // Write the number of whole seconds
    // lower half
    write_hps_reg("sys9", (uint32_t)(((uint64_t)time) & 0x00000000FFFFFFFF));
    // upper half
    write_hps_reg("sys10",
                  (uint32_t)(((uint64_t)time) >> 32) & 0x00000000FFFFFFFF);

    // Write the fractional seconds in ticks
    uint64_t fractional_time = (uint64_t) round((time - (double)((uint64_t)time)) * TICK_RATE);
    // lower half
    write_hps_reg("sys11", (uint32_t)(((uint64_t)fractional_time) & 0x00000000FFFFFFFF));
    // upper half
    write_hps_reg("sys12", (uint32_t)(((uint64_t)fractional_time) >> 32) & 0x00000000FFFFFFFF);

    // Toggling this bit sets the time
    write_hps_reg_mask("sys13", 1, 1);
    write_hps_reg_mask("sys13", 0, 1);
    return RETURN_SUCCESS;
}

static int hdlr_time_clk_cmd(const char* data, char* ret) {
    return RETURN_SUCCESS;
}

// Checks if there has been a PPS in the past second
// Poll this to verify the PPS is working
static int hdlr_time_clk_pps_dtc(const char* data, char* ret) {
    uint32_t pps_detected;
    read_hps_reg("sys21", &pps_detected);
    
    snprintf(ret, MAX_PROP_LEN, "%u", pps_detected & 0x1);
    return RETURN_SUCCESS;
}

// 10 MHz clock
static int hdlr_time_source_ref(const char *data, char *ret) {
    if (strcmp(data, "external") == 0) {
        strcpy(buf, "clk -t 1\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        if(strstr((char *)uart_ret_buf,"Unlocked") != NULL)
        {
            PRINT(ERROR, "clocks unlocked setting reference to external\n");
            strcpy(ret, "Error Unlocked PLL with External Reference");
        }
    } else if (strcmp(data, "internal") == 0) {
        strcpy(buf, "clk -t 0\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        if(strstr((char *)uart_ret_buf,"Unlocked") != NULL)
        {
            PRINT(ERROR, "clocks unlocked setting reference to internal\n");
            strcpy(ret, "Error Unlocked PLL with Internal Reference");
        }
    } else { // just get the current state of the reference
        strcpy(buf, "clk -i\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        // save the UART result to the state tree
        strcpy(ret, (char *)uart_ret_buf);
    }
    return RETURN_SUCCESS;
}

static int hdlr_time_set_time_source(const char *data, char *ret) {
    // Sets pps mode to input
    if (strcmp(data, "external") == 0) {
        write_hps_reg_mask("sys13", 2, 0x2);
    // Sets pps mode to output
    } else if (strcmp(data, "internal") == 0) {
        write_hps_reg_mask("sys13", 0, 0x2);
    // Other time sources not supported
    } else {
        snprintf(ret, MAX_PROP_LEN, "Invalid time source\nValid options are: external, internal\n");
    }
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

// Internal Source Tuning
static int hdlr_time_source_vtune(const char *data, char *ret) {
    // read the vtune value
    uint16_t vtune = 0;
    sscanf(data, "%" SCNd16 "", &vtune);
    // send it to the mcu
    strcpy(buf, "debug -o ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", vtune);
    strcat(buf, "\r");
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

// Toggle SPI Sync
static int hdlr_time_sync_lmk_sync_tgl_pll(const char *data, char *ret) {
    if (strcmp(data, "0") != 0) {
        strcpy(buf, "sync -q\r");
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
static int hdlr_time_sync_lmk_resync_pll(const char *data, char *ret) {
    if (strcmp(data, "0") != 0) {
        strcpy(buf, "sync -p\r");
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

// Resync output edges with Ref
static int hdlr_time_sync_sysref_mode(const char *data, char *ret) {
    if (strcmp(data, "pulsed") == 0) {
        strcpy(buf, "sync -c 0\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    } else if (strcmp(data, "continuous") == 0) {
        strcpy(buf, "sync -c 1\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    } else {
        strcpy(buf, "sync -c 0\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        strcpy(ret, "pulsed");
    }
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
    strcpy(buf, "status -l\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd_pll1(const char *data, char *ret) {
    strcpy(buf, "status -l 11\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd_pll2(const char *data, char *ret) {
    strcpy(buf, "status -l 12\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol(const char *data, char *ret) {
    strcpy(buf, "status -o\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd_pll1(const char *data, char *ret) {
    strcpy(buf, "status -o 11\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd_pll2(const char *data, char *ret) {
    strcpy(buf, "status -o 12\r");
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

static int hdlr_time_about_hw_ver(const char *data, char *ret) {
    strcpy(buf, "board -h\r");
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
#define X(ch) hdlr_tx_##ch##_rf_board_dump(NULL, NULL);
    TX_CHANNELS
#undef X
#define X(ch) hdlr_rx_##ch##_rf_board_dump(NULL, NULL);
    RX_CHANNELS
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
#define X(ch)                                                                  \
    ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf)),          \
        usleep(50000);
        RX_CHANNELS
#undef X

        strcpy(buf, "board -g 1\r");
#define X(ch)                                                                  \
    ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf)),          \
        usleep(50000);
        TX_CHANNELS
#undef X
    }
    if (strcmp(data, "2") == 0) {
        strcpy(buf, "board -g 2\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        usleep(50000);

        strcpy(buf, "board -g 2\r");
#define X(ch)                                                                  \
    ping(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf)),          \
        usleep(50000);
        RX_CHANNELS
#undef X

        strcpy(buf, "board -g 2\r");
#define X(xh)                                                                  \
    ping(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf)),          \
        usleep(50000);
        TX_CHANNELS
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
        snprintf(ret, MAX_PROP_LEN, "temp +%u degC\n", old_val);
    } else if (old_val < 128) {
        old_val = old_val - 58;
        snprintf(ret, MAX_PROP_LEN, "temp -%u degC\n", old_val);
    }

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_led(const char *data, char *ret) {
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
    //DEBUG USE ONLY: To ensure consistency, JESD sync's are managed
    //through the reset controller.

    if (strcmp(data, "0") != 0) {
        //Issue JESD reset request.
        set_property("fpga/board/reg_rst_req", "16");
    }

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_rst_softsys(const char *data, char *ret) {
    //This function is for debug use only: we should depreciate it.
    //This performs a soft system reset (does not restart server)
    //It reboots all the boards, and resets the 10G handler.

    if (strcmp(data, "0") != 0) {
        //Reboot time board
        strcpy(buf, "board -r\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        usleep(1000000);

        //Reboot Rx board
        strcpy(buf, "board -r\r");
        ping(uart_rx_fd[0], (uint8_t *)buf, strlen(buf));

        //Reboot Tx board
        strcpy(buf, "board -r\r");
        ping(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));

        //Issue reset request from 10G handler
        set_property("fpga/board/reg_rst_req", "12");

        //Ensure sysref is pulsed.
        set_property("time/sync/sysref_mode", "pulsed");
    }
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

    snprintf(ret, MAX_PROP_LEN, "%u", udp_port);

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

static int hdlr_fpga_board_init_regs(const char *data, char *ret) {
    uint8_t init;
    sscanf(data, "%" SCNu8 "", &init);

    if(init) {
        snprintf(ret, MAX_PROP_LEN,"1");
        mmap_init_regs(0);
    }

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_reg_rst_req(const char *data, char *ret) {
    uint32_t reset;
    uint32_t status = 1;
    uint16_t time = 0;
    sscanf(data, "%" SCNu32 "", &reset);
    uint8_t jesd_reset = 0;
    uint8_t led_reset = 0;

    switch(reset)
    {
        case 0:
            // writing all high bits will not trigger reset, then we will still
            // read the status register
            reset = 0x3f;
            break;
        case 1: 
            PRINT(INFO,"hps pll reset\n");
        case 3:
            PRINT(INFO,"failover pll reset\n");
        case 5:
            PRINT(INFO,"link pll reset\n");
        case 7:
            PRINT(INFO,"HPS reset\n");
            led_reset = get_led_state();
        case 8:
            PRINT(INFO,"Chip ID reset\n");
        case 9:
            PRINT(INFO,"Temperature logic reset\n");
        case 10:
            PRINT(INFO,"led control reset\n");
        case 11:
            PRINT(INFO,"10G reset\n");
        case 12:
            PRINT(INFO,"10G Handler reset\n");
        case 13:
            PRINT(INFO,"10G Response reset\n");
        case 14:
            PRINT(INFO,"GPIO reset\n");
        case 15:
            PRINT(INFO,"system time module reset\n");
        case 16:
            PRINT(INFO,"JESD reset\n");
            // Indiates JESD was reset and cleanup is required
            jesd_reset = 1;
            // Prepared for JESD reset (since IP block is being reset
            sync_channels_prep(15);
        case 17:
            PRINT(INFO,"tx chain reset\n");
        case 18:
            PRINT(INFO,"rx chain reset\n");
        case 19:
            PRINT(INFO,"status core fifo reset\n");
            break;
        default:
            PRINT(ERROR, "invalid reset state: %" PRIu32 "\n", reset);
            snprintf(ret, MAX_PROP_LEN,"invalid reset state: %" PRIu32 "\n", reset);
            return RETURN_SUCCESS;
    }

    uint32_t reset_code = reset;
    reset_code |= 0xffffffc0; // keep bits 32:6 high
    write_hps_reg("rst_req0", reset_code);

    // poll rst_stat0
    do {
        usleep(100);
        time++;
        read_hps_reg("rst_stat0", &status);
        // Bit 23 of rst_stat0 indicates PLLs are locked
        // Remaining bits should be zero.
    } while (status != 0x800000 && time < MAX_RESET_WAIT);

    if(time >= MAX_RESET_WAIT) {
        PRINT(ERROR, "Timeout while waiting for FPGA reset\n");
    }
    
    // write the register back to its default state
    write_hps_reg("rst_req0", 0xffffffff);

    // set the leds back to their beginning state
    if(led_reset){
        set_led_state(led_reset);
    }

    // Cleans up JESD reset status if JESD was reset
    if(jesd_reset) {
        sync_channels_cleanup(15);
    }

    snprintf(ret, MAX_PROP_LEN, "%u\n", reset);
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_reg_rst_status(const char *data, char *ret) {
    uint32_t old_val;

    read_hps_reg("rst_stat0", &old_val);
    snprintf(ret, MAX_PROP_LEN, "rst_stat0: %x\n", old_val);

    read_hps_reg("rst_stat1", &old_val);
    snprintf(ret, MAX_PROP_LEN, "rst_stat1: %x\n", old_val);

    //TODO: Implement clearing of loss-of-lock bits
    //write_hps_reg("sys0", old_val | 0x10);
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_fw_rst(const char *data, char *ret) {
    //Depreciated
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

    snprintf(ret, MAX_PROP_LEN, "cmp. time %i-%i-%i %i:%i (yyyy-MM-dd HH:mm) \n", year, month,
            day, hour, min);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_about_conf_info(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("sys18", &old_val);
    snprintf(ret, MAX_PROP_LEN, "config. info. 0x%02x \n", old_val);

    return RETURN_SUCCESS;
}

// Currently our units do not have serial numbers burned into them
// As a workaround the serial number will be rtm followed by serial number of the time mcu
static int hdlr_fpga_about_serial(const char *data, char *ret) {
    //Gets the rtm value of fuse 0 of the time mcu)
    get_property("time/about/mcufuses", buf, MAX_PROP_LEN);
    uint32_t rtm = 0;
    sscanf(buf, "Fuse00: 0x%x", &rtm);

    get_property("time/about/serial", buf, MAX_PROP_LEN);

    // Cuts of the time serial string as the end of the first line
    for(uint32_t n = 0; n < strnlen(buf, MAX_PROP_LEN); n++) {
        if(isspace(buf[n])) {
            buf[n] = 0;
            break;
        }
    }

    snprintf(ret, MAX_PROP_LEN, "%u%s\n", rtm, buf);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_trigger_sma_dir(const char *data, char *ret) {
    bool val;
    int r = valid_trigger_dir(data, &val);
    if(r == RETURN_SUCCESS) {
        return set_sma_dir(val);
    } else {
        return r;
    }
}

static int hdlr_fpga_trigger_sma_pol(const char *data, char *ret) {
    bool val;
    int r = valid_trigger_pol(data, &val);
    if(r == RETURN_SUCCESS) {
        return set_sma_pol(val);
    } else {
        return r;
    }
}

static int hdlr_fpga_trigger_sma_override(const char *data, char *ret) {
    uint8_t val = 0;
    sscanf(data,"%" SCNu8, &val);
    if(val){ val = 1;} else {val=0;}
    snprintf(ret, MAX_PROP_LEN, "%i\n", val);
    return set_reg_bits("sys2", 8, 1, val);
}

static int hdlr_fpga_about_fw_ver(const char *data, char *ret) {
    uint32_t old_val1;
    uint32_t old_val2;
    read_hps_reg("sys3", &old_val2);
    read_hps_reg("sys4", &old_val1);

    old_val2 = old_val2 & 0xff;
    // NOTE server -v prints old_val2 & 0xf, we expect old_val2 & 0xf0 to be 0
    // server -v prints warning if it is not

    snprintf(ret, MAX_PROP_LEN, "ver. 0x%02x%08x \n", old_val2, old_val1);
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
#if defined(RTM6) || defined(RTM7)
    uint32_t old_val;
    read_hps_reg("sys1", &old_val);

    old_val = (old_val >> 7) & 0xf;

    sprintf(ret, "ver. 0x%02x", old_val);
    return RETURN_SUCCESS;
#else
    FILE *fp = NULL;
    char buf[MAX_PROP_LEN] = {0};
    char base_cmd[MAX_PROP_LEN] = "/usr/sbin/i2cget -y 0 0x54 0x";
    char cmd[MAX_PROP_LEN] = {0};
    int readreg = 0;
    int var_count = 0;
    int i = 0;

    // check that EEPROM is programmed properly
    snprintf(cmd, MAX_PROP_LEN, "%s%x", base_cmd, 0);
    if (DEBUG_PRINT_EEPROM) {PRINT(INFO, "%s\n", cmd);}
    if ((fp = popen(cmd, "r")) == NULL) {
        PRINT(ERROR, "Error opening pipe!\n");
        strcpy(ret, "ERROR: EEPROM read failed");
        return RETURN_ERROR;
    }
    fgets(buf, MAX_PROP_LEN, fp);
     if (pclose(fp)) {
        PRINT(ERROR, "Error closing pipe!");
        strcpy(ret, "ERROR: EEPROM read failed");
        return RETURN_ERROR;
    }
    sscanf(buf, "0x%x", &readreg);
    if (DEBUG_PRINT_EEPROM) {PRINT(INFO, "we read  0 = 0x%x\n", readreg);}
    if (readreg != 0xaa ) {
        PRINT(ERROR, "EEPROM not programmed or does not exist");
        strcpy(ret, "ERROR: EEPROM not programmed or does not exist");
        return RETURN_ERROR;
    }

    // check product
    snprintf(cmd, MAX_PROP_LEN, "%s%x", base_cmd, 1);
    if (DEBUG_PRINT_EEPROM) {PRINT(INFO, "%s\n", cmd);}
    if ((fp = popen(cmd, "r")) == NULL) {
        PRINT(ERROR, "Error opening pipe!\n");
        strcpy(ret, "ERROR: EEPROM read failed");
        return RETURN_ERROR;
    }
    fgets(buf, MAX_PROP_LEN, fp);
    if (pclose(fp)) {
        PRINT(ERROR, "Error closing pipe!");
        strcpy(ret, "ERROR: EEPROM read failed");
        return RETURN_ERROR;
    }
    sscanf(buf, "0x%x", &readreg);
    if (DEBUG_PRINT_EEPROM) {PRINT(INFO, "we read 1 = 0x%x\n", readreg);}
    switch(readreg) {
    case 1:
        strcpy(ret, "Crimson ");
        break;
    case 2:
        strcpy(ret, "Cyan ");
        break;
    default:
        strcpy(ret, "Unrecognized ");
    }

    // check board type
    snprintf(cmd, MAX_PROP_LEN, "%s%x", base_cmd, 2);
    if (DEBUG_PRINT_EEPROM) {PRINT(INFO, "%s\n", cmd);}
    if ((fp = popen(cmd, "r")) == NULL) {
        PRINT(ERROR, "Error opening pipe!\n");
        strcat(ret, " ERROR: EEPROM read failed");
        return RETURN_ERROR;
    }
    fgets(buf, MAX_PROP_LEN, fp);
    if (pclose(fp)) {
        PRINT(ERROR, "Error closing pipe!");
        strcat(ret, " ERROR: EEPROM read failed");
        return RETURN_ERROR;
    }
    sscanf(buf, "0x%x", &readreg);
    if (DEBUG_PRINT_EEPROM) {PRINT(INFO, "we read 1 = 0x%x\n", readreg);}
    switch(readreg) {
        case 1:
            strcat(ret, "Time ");
            break;
        case 2:
            strcat(ret, "TX ");
            break;
        case 3:
            strcat(ret, "RX ");
            break;
        case 4:
            strcat(ret, "Dig ");
            break;
        case 5:
            strcat(ret, "BP ");
            break;
        case 6:
            strcat(ret, "PWR ");
            break;
        default:
            strcat(ret, "Unrecognized ");
    }

    // Check revision register
    snprintf(cmd, MAX_PROP_LEN, "%s%x", base_cmd, 3);
    if (DEBUG_PRINT_EEPROM) {PRINT(INFO, "%s\n", cmd);}
    if ((fp = popen(cmd, "r")) == NULL) {
        PRINT(ERROR, "Error opening pipe!\n");
        strcat(ret, " ERROR: EEPROM read failed");
        return RETURN_ERROR;
    }
    fgets(buf, MAX_PROP_LEN, fp);
    if (pclose(fp)) {
        PRINT(ERROR, "Error closing pipe!");
        strcat(ret, " ERROR: EEPROM read failed");
        return RETURN_ERROR;
    }
    sscanf(buf, "0x%x", &readreg);
    if (DEBUG_PRINT_EEPROM) {PRINT(INFO, "we read 1 = 0x%x\n", readreg);}
    snprintf(buf, MAX_PROP_LEN, "RTM %u - ", readreg);
    strcat(ret, buf);

    // Check feature registers
    strcat(ret, "Features:");
    for (i = 0x10; i <= 0x5f; i++) {
        snprintf(cmd, MAX_PROP_LEN, "%s%x", base_cmd, i);
        if (DEBUG_PRINT_EEPROM) {PRINT(INFO, "%s\n", cmd);}
        if ((fp = popen(cmd, "r")) == NULL) {
            PRINT(ERROR, "Error opening pipe!\n");
            strcat(ret, " ERROR: EEPROM read failed");
            return RETURN_ERROR;
        }
        fgets(buf, MAX_PROP_LEN, fp);
        if (pclose(fp)) {
            PRINT(ERROR, "Error closing pipe!");
            strcat(ret, " ERROR: EEPROM read failed");
            return RETURN_ERROR;
        }
        sscanf(buf, "0x%x", &readreg);
        if ((readreg != 0) && (readreg!= 0xff)) {
            var_count++;
            snprintf(buf, MAX_PROP_LEN, " reg 0x%x = 0x%x;", i, readreg);
            strcat(ret, buf);
        }
    }
    if (var_count == 0) {
        strcat(ret, "None");
    }
    strcat(ret, "\n");

    return RETURN_SUCCESS;
#endif
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
    // ensure pay_len is not too large
    // JPOL: 2024-02-29 experimentally determined 8144 is the largest payload crimson will send
    if (pay_len > 8144)
    {
        pay_len = 8144;
    }
    // ensure pay_len is a multiple of 8 (4 bytes per IQ sample and 2 samples per clock means 8 is the minimum step size)
    pay_len -= pay_len % 8;

    read_hps_reg("net0", &old_val);
    write_hps_reg("net0", (old_val & ~(0xffff0000)) | (pay_len << 16));
    snprintf(ret, MAX_PROP_LEN, "%u\n", pay_len);
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
    // ensure pay_len is not too large
    // JPOL: 2024-02-29 experimentally determined 8144 is the largest payload crimson will send
    if (pay_len > 8144)
    {
        pay_len = 8144;
    }
    // ensure pay_len is a multiple of 8 (4 bytes per IQ sample and 2 samples per clock means 8 is the minimum step size)
    pay_len -= pay_len % 8;

    read_hps_reg("net15", &old_val);
    write_hps_reg("net15", (old_val & ~(0xffff0000)) | (pay_len << 16));
    snprintf(ret, MAX_PROP_LEN, "%u\n", pay_len);
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

// Fractional time in ticks
static int hdlr_fpga_board_gps_frac_time(const char *data, char *ret) {
    uint32_t gps_frac_time_lh = 0, gps_frac_time_uh = 0;
    read_hps_reg("sys7", &gps_frac_time_lh);
    read_hps_reg("sys8", &gps_frac_time_uh);

    uint64_t gps_frac_time = (uint64_t) gps_frac_time_lh + (((uint64_t) gps_frac_time_uh) * 1000000000);

    snprintf(ret, MAX_PROP_LEN, "%llu", gps_frac_time);

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

static int hdlr_fpga_user_regs(const char *data, char *ret)
{
    return RETURN_SUCCESS;
}

static int read_interboot_variable(char* data_filename, int64_t* value) {
    struct stat sb;
    
    char data_path[MAX_PROP_LEN];
    
    snprintf(data_path, MAX_PROP_LEN, INTERBOOT_DATA "%s", data_filename);
    
    int file_missing = stat(data_path, &sb);
    
    PRINT(INFO, "Reading from %s\n", data_path);
    
    if(file_missing) {
        *value = 0;
        return file_missing;
    } else {
        FILE *data_file = fopen( data_path, "r");
        fscanf(data_file, "%lli", value);
        fclose(data_file);
        return 0;
    }
}

static void update_interboot_variable(char* data_filename, int64_t value) {
    struct stat sb;
    
    char data_path[MAX_PROP_LEN];
    
    snprintf(data_path, MAX_PROP_LEN, INTERBOOT_DATA "%s", data_filename);
    
    int directory_missing = stat(INTERBOOT_DATA, &sb);
    
    if(directory_missing) {
        mkdir(INTERBOOT_DATA, 0644);
    }
    
    FILE *data_file = fopen( data_path, "w");
    
    fprintf(data_file, "%lli", value);
    fclose(data_file);
}

// Returns 0 is jesd links come up, 1 if any links fail
static int hdlr_fpga_board_rst_postinit(const char *data, char *ret) {
    //Do nothing is 0 is provided, reset all if everything else is provided
    int reset = 0;
    sscanf(data, "%i", &reset);
    if (!reset){
        return RETURN_SUCCESS;
    }

    uint8_t fail_count = 0;
    uint8_t i, j, error_val, ret_val;
    char ch_type;
    char prop_path[PROP_PATH_LEN];
    
    // Check for known bad FPGA state
    uint32_t read_val;
    read_hps_reg("rst_stat0",&read_val);
    if(read_val & 0x1001FFF){
        // Bit 24 means JESD will not work. Bits 12:0 all represent other
        // errors. Usually if any of the error bits is high, bit 24 will be
        // too. In our testing reset state 7 usually worked, but once I needed
        // to use reset state 5. I want this to work every time, so we will just
        // use reset state 1 (the most aggressive reset available).
        fail_count += 1;
        set_property("fpga/board/reg_rst_req", "1");
    } else {
        //Initiate reset request from GPIO interface (GPIO > SystemTime > JESD > DSP)
        set_property("fpga/board/reg_rst_req", "14");
    }

    // Check RX JESD links
    for (i = 0; i < NUM_RX_CHANNELS; i++) {
        snprintf(prop_path, PROP_PATH_LEN, "rx/%c/status/adc_alarm", i+'a');
        set_property(prop_path, "1");
        get_property(prop_path, buf, MAX_PROP_LEN);
        ret_val = sscanf(buf,"Error: 0x%02hhx", &error_val);
        if (!ret_val) {
            PRINT(ERROR,"RX JESD sscanf fail\n");
            fail_count += 1;
        }
        fail_count += error_val;
    }

    // TODO: Check TX JESD links

    // Check RX and TX RF PLLs
    for (j = 0; j < 2; j++) {
        switch(j)
        {
            case 0:
                ch_type = 'r';
                break;
            case 1:
                ch_type = 't';
                break;
            default:
                PRINT(ERROR,"unexpected case. j = %hhu\n",j);
                return RETURN_ERROR;
        }
        for (i = 0; (i < NUM_RX_CHANNELS && ch_type == 'r') || (i < NUM_TX_CHANNELS && ch_type == 'r') ; i++) {
            snprintf(prop_path, PROP_PATH_LEN, "%cx/%c/status/rfpll_lock", ch_type, i+'a');
            set_property(prop_path, "1");
            get_property(prop_path, buf, MAX_PROP_LEN);
            if(strcmp(buf,"Locked") != 0) {
                fail_count += 1;
            }
        }
    }

    if(!fail_count) {
        update_interboot_variable("cons_boot_fail_count", 0);
        PRINT(INFO, "All JESD links established and RF PLLs locked\n");
        snprintf(ret, MAX_PROP_LEN, "good");
        return RETURN_SUCCESS;
    } else {
        int64_t failed_count = 0;
        read_interboot_variable("cons_boot_fail_count", &failed_count);
        if(failed_count < jesd_max_server_restart_attempts) {
            update_interboot_variable("cons_boot_fail_count", failed_count + 1);
            // restarting the server will set the time board to continuous mode then reboot the rx and tx boards
            PRINT(ERROR, "Restarting server\n");
            system("systemctl restart crimson-server");
            // Waits for the server reboot command to restart the server
            while(1) {
                usleep(1000);
            }
        } else {
            // TODO set the LEDs as Victor requested
            PRINT(ERROR, "Unable to establish all JESD links and lock all RF PLLs despite multiple server restarts. The system will not attempt another server restart until a successful boot\n");
            snprintf(ret, MAX_PROP_LEN, "bad");
            return RETURN_ERROR;
        }
    }
}

static int hdlr_max_sample_rate(const char *data, char *ret) {
    uint16_t fpga_samp_rate, mcu_samp_rate;
    uint32_t read_val;
    read_hps_reg("res_ro3",&read_val);
    fpga_samp_rate = ( read_val >> 20) & 0xFFF;
    snprintf(buf, sizeof(buf), "clk -d\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    sscanf((char *)uart_ret_buf, "DEVCLK: %" SCNu32 "Hz", &read_val);
    mcu_samp_rate = read_val / 1000000;
    if (fpga_samp_rate == mcu_samp_rate) {
        snprintf(ret, MAX_PROP_LEN,"%u",read_val);
        return RETURN_SUCCESS;
    } else {
        PRINT(ERROR, "FPGA for %uMHz; TIME board for %uMHz\n", fpga_samp_rate, mcu_samp_rate);
        snprintf(ret, MAX_PROP_LEN, "ERROR: FPGA for %uMHz; TIME board for %uMHz\n", fpga_samp_rate, mcu_samp_rate);
        return RETURN_ERROR;
    }
}

/* clang-format off */

/* -------------------------------------------------------------------------- */
/* ---------------------------- PROPERTY TABLE ------------------------------ */
/* -------------------------------------------------------------------------- */

#define PROJECT_NAME "crimson_tng"

#define DEFINE_FILE_PROP_P(n, h, p, v, e, c) \
    {                                \
        .type = PROP_TYPE_FILE,      \
        .path = n,                   \
        .handler = h,                \
        .permissions = p,            \
        .def_val = v,                \
        .pwr_en = e,\
        .ch = c,\
    },

#define DEFINE_SYMLINK_PROP(n, t)    \
    {                                \
        .type = PROP_TYPE_SYMLINK,   \
        .path = n,                   \
        .symlink_target = t,         \
    },

#define DEFINE_START_RFE_REBOOT()\
    DEFINE_FILE_PROP_P("cm/rx/quick_reboot"                   , hdlr_cm_quick_reboot_rx,                           RW, "1", SP, NAC)\
    DEFINE_FILE_PROP_P("cm/tx/quick_reboot"                   , hdlr_cm_quick_reboot_tx,                           RW, "1", SP, NAC)\

#define DEFINE_WAIT_RFE_REBOOT()\
    DEFINE_FILE_PROP_P("cm/rx/wait_for_reboot"                   , hdlr_cm_wait_for_reboot_rx,                           RW, "1", SP, NAC)\
    DEFINE_FILE_PROP_P("cm/tx/wait_for_reboot"                   , hdlr_cm_wait_for_reboot_tx,                           RW, "1", SP, NAC)\


#define DEFINE_RX_CHANNEL(_c)                                                                                         \
    DEFINE_SYMLINK_PROP("rx_" #_c, "rx/" #_c)                                                                         \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/hw_ver"             , hdlr_rx_##_c##_about_hw_ver,            RW, VERSION, SP, #_c)     \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/freq/common_lo"        , hdlr_rx_##_c##_rf_common_lo,            RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/sma_mode"         , hdlr_rx_##_c##_trigger_sma_mode,        RW, "level", SP, #_c)     \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/trig_sel"         , hdlr_rx_##_c##_trigger_trig_sel,        RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/edge_backoff"     , hdlr_rx_##_c##_trigger_edge_backoff,    RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/edge_sample_num"  , hdlr_rx_##_c##_trigger_edge_sample_num, RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/ufl_mode"         , hdlr_rx_##_c##_trigger_ufl_mode,        RW, "level", SP, #_c)     \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/ufl_dir"          , hdlr_rx_##_c##_trigger_ufl_dir,         RW, "out", SP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/ufl_pol"          , hdlr_rx_##_c##_trigger_ufl_pol,         RW, "negative", SP, #_c)  \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/time_disable"     , hdlr_rx_##_c##_trigger_time_disable,    RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/time_gate_logic"  , hdlr_rx_##_c##_trigger_time_gate_logic, RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/sample_counter"   , hdlr_rx_##_c##_trigger_sample_counter,  RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/pwr"                      , hdlr_rx_##_c##_pwr,                     RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/stream"                   , hdlr_rx_##_c##_stream,                  RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/sync"                     , hdlr_rx_sync,                           WO, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/freq/val"              , hdlr_rx_##_c##_rf_freq_val,             RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/freq/lut_en"           , hdlr_rx_##_c##_rf_freq_lut_en,          RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/freq/lna"              , hdlr_rx_##_c##_rf_freq_lna,             RW, "1", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/freq/band"             , hdlr_rx_##_c##_rf_freq_band,            RW, "1", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/gain/val"              , hdlr_rx_##_c##_rf_gain_val,             RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/atten/val"             , hdlr_rx_##_c##_rf_atten_val,            RW, "127", RP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/status/rfpll_lock"        , hdlr_rx_##_c##_status_rfld,             RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/status/adc_alarm"         , hdlr_rx_##_c##_status_adcalarm,         RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/board/dump"               , hdlr_rx_##_c##_rf_board_dump,           WO, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/board/test"               , hdlr_rx_##_c##_rf_board_test,           WO, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/board/temp"               , hdlr_rx_##_c##_rf_board_temp,           RW, "20", RP, #_c)        \
    DEFINE_FILE_PROP_P("rx/" #_c "/board/led"                , hdlr_rx_##_c##_rf_board_led,            WO, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/signed"               , hdlr_rx_##_c##_dsp_signed,              RW, "1", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/gain"                 , hdlr_rx_##_c##_dsp_gain,                RW, "10", SP, #_c)        \
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/rate"                 , hdlr_rx_##_c##_dsp_rate,                RW, "1258850", SP, #_c)   \
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/nco_adj"              , hdlr_rx_##_c##_dsp_nco_adj,             RW, "-15000000", SP, #_c) \
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/rstreq"               , hdlr_rx_##_c##_dsp_rstreq,              WO, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/loopback"             , hdlr_rx_##_c##_dsp_loopback,            RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/id"                 , hdlr_rx_##_c##_about_id,                RW, "001", RP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/serial"             , hdlr_rx_##_c##_about_serial,            RW, "001", RP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/mcudevid"           , hdlr_rx_##_c##_about_mcudevid,          RW, "001", RP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/mcurev"             , hdlr_rx_##_c##_about_mcurev,            RW, "001", RP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/mcufuses"           , hdlr_rx_##_c##_about_mcufuses,          RW, "001", RP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/fw_ver"             , hdlr_rx_##_c##_about_fw_ver,            RW, VERSION, RP, #_c)     \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/sw_ver"             , hdlr_invalid,                           RO, VERSION, SP, #_c)     \
    DEFINE_FILE_PROP_P("rx/" #_c "/link/vita_en"             , hdlr_rx_##_c##_link_vita_en,            RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/link/iface"               , hdlr_rx_##_c##_link_iface,              RW, "sfpa", SP, #_c)      \
    DEFINE_FILE_PROP_P("rx/" #_c "/link/port"                , hdlr_rx_##_c##_link_port,               RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/link/ip_dest"             , hdlr_rx_##_c##_link_ip_dest,            RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/link/mac_dest"            , hdlr_rx_##_c##_link_mac_dest,           RW, "ff:ff:ff:ff:ff:ff", SP, #_c)\
    DEFINE_FILE_PROP_P("rx/" #_c "/prime_trigger_stream"     , hdlr_rx_##_c##_prime_trigger_stream,  RW, "0", RP, #_c)\
    DEFINE_FILE_PROP_P("rx/" #_c "/link/endian_swap"         , hdlr_rx_##_c##_endian_swap,            RW, "0", SP, #_c)\
    DEFINE_FILE_PROP_P("rx/" #_c "/jesd/delay_iq"            , hdlr_rx_##_c##_jesd_delay_iq,            RW, "0 0", SP, #_c)\

#define DEFINE_RX_CHANNEL_POST(_c)\
    DEFINE_FILE_PROP_P("rx/" #_c "/jesd/status"            , hdlr_rx_##_c##_jesd_status,             RW, "bad", SP, #_c)\

#define DEFINE_RX_40GHZFE_CHANNEL(_c)                                                           \
    DEFINE_FILE_PROP_P("rx/" #_c "/fe/lna"      , hdlr_rx_##_c##_fe_lna,    RW, "0", RP, #_c)   \
    DEFINE_FILE_PROP_P("rx/" #_c "/fe/gain"     , hdlr_rx_##_c##_fe_gain,   RW, "0", RP, #_c)

#define DEFINE_TX_CHANNEL(_c)                                                                                         \
    DEFINE_SYMLINK_PROP("tx_" #_c, "tx/" #_c)                                                                         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/freq/common_lo"        , hdlr_tx_##_c##_rf_common_lo,            RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/sma_mode"         , hdlr_tx_##_c##_trigger_sma_mode,        RW, "level", SP, #_c)     \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/trig_sel"         , hdlr_tx_##_c##_trigger_trig_sel,        RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/edge_backoff"     , hdlr_tx_##_c##_trigger_edge_backoff,    RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/edge_sample_num"  , hdlr_tx_##_c##_trigger_edge_sample_num, RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/ufl_dir"          , hdlr_tx_##_c##_trigger_ufl_dir,         RW, "out", SP, #_c)       \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/ufl_mode"         , hdlr_tx_##_c##_trigger_ufl_mode,        RW, "level", SP, #_c)     \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/ufl_pol"          , hdlr_tx_##_c##_trigger_ufl_pol,         RW, "negative", SP, #_c)  \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/gating"           , hdlr_tx_##_c##_trigger_gating,          RW, "output", SP, #_c)    \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/time_disable"     , hdlr_tx_##_c##_trigger_time_disable,    RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/time_gate_logic"  , hdlr_tx_##_c##_trigger_time_gate_logic, RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/sample_counter"   , hdlr_tx_##_c##_trigger_sample_counter,  RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/pwr"                      , hdlr_tx_##_c##_pwr,                     RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/sync"                     , hdlr_tx_sync,                           WO, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/dac/dither_en"         , hdlr_tx_##_c##_rf_dac_dither_en,        RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/dac/dither_mixer_en"   , hdlr_tx_##_c##_rf_dac_dither_mixer_en,  RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/dac/dither_sra_sel"    , hdlr_tx_##_c##_rf_dac_dither_sra_sel,   RW, "6", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/dac/nco"               , hdlr_tx_##_c##_rf_dac_nco,              RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/dac/temp"              , hdlr_tx_##_c##_rf_dac_temp,             RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/freq/val"              , hdlr_tx_##_c##_rf_freq_val,             RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/freq/lut_en"           , hdlr_tx_##_c##_rf_freq_lut_en,          RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/freq/band"             , hdlr_tx_##_c##_rf_freq_band,            RW, "1", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/freq/i_bias"           , hdlr_tx_##_c##_rf_freq_i_bias,          RW, "17", TP, #_c)        \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/freq/q_bias"           , hdlr_tx_##_c##_rf_freq_q_bias,          RW, "17", TP, #_c)        \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/gain/val"              , hdlr_tx_##_c##_rf_gain_val,             RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/status/rfpll_lock"        , hdlr_tx_##_c##_status_rfld,             RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/status/dacpll_lock"       , hdlr_tx_##_c##_status_dacld,            RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/status/dacpll_centre"     , hdlr_tx_##_c##_status_dacctr,           RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/board/dump"               , hdlr_tx_##_c##_rf_board_dump,           WO, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/board/test"               , hdlr_tx_##_c##_rf_board_test,           WO, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/board/temp"               , hdlr_tx_##_c##_rf_board_temp,           RW, "23",TP, #_c)        \
    DEFINE_FILE_PROP_P("tx/" #_c "/board/led"                , hdlr_tx_##_c##_rf_board_led,            WO, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/dsp/gain"                 , hdlr_tx_##_c##_dsp_gain,                RW, "10", SP, #_c)        \
    DEFINE_FILE_PROP_P("tx/" #_c "/dsp/rate"                 , hdlr_tx_##_c##_dsp_rate,                RW, "1258850", SP, #_c)   \
    DEFINE_FILE_PROP_P("tx/" #_c "/dsp/nco_adj"              , hdlr_tx_##_c##_dsp_nco_adj,             RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/dsp/rstreq"               , hdlr_tx_##_c##_dsp_rstreq,              WO, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/hw_ver"             , hdlr_tx_##_c##_about_hw_ver,            RW, VERSION, TP, #_c)     \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/id"                 , hdlr_tx_##_c##_about_id,                RW, "001", TP, #_c)       \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/serial"             , hdlr_tx_##_c##_about_serial,            RW, "001", TP, #_c)       \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/mcudevid"           , hdlr_tx_##_c##_about_mcudevid,          RW, "001", TP, #_c)       \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/mcurev"             , hdlr_tx_##_c##_about_mcurev,            RW, "001", TP, #_c)       \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/mcufuses"           , hdlr_tx_##_c##_about_mcufuses,          RW, "001", TP, #_c)       \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/fw_ver"             , hdlr_tx_##_c##_about_fw_ver,            RW, VERSION, TP, #_c)     \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/sw_ver"             , hdlr_invalid,                           RO, VERSION, SP, #_c)     \
    DEFINE_FILE_PROP_P("tx/" #_c "/link/vita_en"             , hdlr_tx_##_c##_link_vita_en,            RW, "1", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/link/iface"               , hdlr_tx_##_c##_link_iface,              RW, "sfpa", SP, #_c)      \
    DEFINE_FILE_PROP_P("tx/" #_c "/link/port"                , hdlr_tx_##_c##_link_port,               RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/qa/fifo_lvl"              , hdlr_tx_##_c##_qa_fifo_lvl,             RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/qa/oflow"                 , hdlr_tx_##_c##_qa_oflow,                RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/qa/uflow"                 , hdlr_tx_##_c##_qa_uflow,                RW, "0", SP, #_c)\
    DEFINE_FILE_PROP_P("tx/" #_c "/link/endian_swap"       , hdlr_tx_##_c##_endian_swap,            RW, "0", SP, #_c)\
    DEFINE_FILE_PROP_P("tx/" #_c "/jesd/delay_iq"          , hdlr_tx_##_c##_jesd_delay_iq,            RW, "0 0", SP, #_c)\

#define DEFINE_TX_CHANNEL_POST(_c)\
    DEFINE_FILE_PROP_P("tx/" #_c "/jesd/status"            , hdlr_tx_##_c##_jesd_status,             RW, "bad", SP, #_c)\

#define DEFINE_TIME()                                                                                                 \
    DEFINE_FILE_PROP_P("time/about/hw_ver"                   , hdlr_time_about_hw_ver,                 RW, VERSION, SP, NAC)     \
    DEFINE_FILE_PROP_P("time/about/fw_ver"                   , hdlr_time_about_fw_ver,                 RW, VERSION, SP, NAC)     \
    DEFINE_FILE_PROP_P("time/clk/pps"                        , hdlr_time_clk_pps,                      RW, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/clk/set_time"                   , hdlr_time_clk_set_time,                 WO, "0.0", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/clk/cmd"                        , hdlr_time_clk_cmd,                      RW, "0.0", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/clk/pps_detected"               , hdlr_time_clk_pps_dtc,                  RW, "1", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/status/lmk_lockdetect"          , hdlr_time_status_ld,                    RW, "unlocked", SP, NAC)  \
    DEFINE_FILE_PROP_P("time/status/lmk_lossoflock"          , hdlr_time_status_lol,                   RW, "unlocked", SP, NAC)  \
    DEFINE_FILE_PROP_P("time/status/lmk_lockdetect_jesd_pll1", hdlr_time_status_ld_jesd_pll1,          RW, "unlocked", SP, NAC)  \
    DEFINE_FILE_PROP_P("time/status/lmk_lockdetect_jesd_pll2", hdlr_time_status_ld_jesd_pll2,          RW, "unlocked", SP, NAC)  \
    DEFINE_FILE_PROP_P("time/status/lmk_lossoflock_jesd_pll1", hdlr_time_status_lol_jesd_pll1,         RW, "unlocked", SP, NAC)  \
    DEFINE_FILE_PROP_P("time/status/lmk_lossoflock_jesd_pll2", hdlr_time_status_lol_jesd_pll2,         RW, "unlocked", SP, NAC)  \
    DEFINE_FILE_PROP_P("time/source/ref"                     , hdlr_time_source_ref,                   RW, "0", SP, NAC)  \
    DEFINE_FILE_PROP_P("time/source/set_time_source"        , hdlr_time_set_time_source,               RW, "internal", SP, NAC)  \
    DEFINE_FILE_PROP_P("time/source/extsine"                 , hdlr_time_source_extsine,               RW, "sine", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/source/vtune"                   , hdlr_time_source_vtune,                 RW, "1403", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/sync/lmk_sync_tgl_jesd"         , hdlr_time_sync_lmk_sync_tgl_jesd,       WO, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/sync/lmk_sync_tgl_pll"          , hdlr_time_sync_lmk_sync_tgl_pll,        WO, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/sync/lmk_sync_resync_jesd"      , hdlr_time_sync_lmk_resync_jesd,         WO, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/sync/lmk_sync_resync_pll"       , hdlr_time_sync_lmk_resync_pll,          WO, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/sync/lmk_resync_all"            , hdlr_time_sync_lmk_resync_all,          WO, "0", SP, NAC)         \
    /* Note: default value is continuous because it must be continuous later in boot when rfe boards reboot*/\
    DEFINE_FILE_PROP_P("time/sync/sysref_mode"               , hdlr_time_sync_sysref_mode,          WO, "continuous", SP, NAC)    \
    DEFINE_FILE_PROP_P("time/board/dump"                     , hdlr_time_board_dump,                   WO, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/board/test"                     , hdlr_time_board_test,                   WO, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/board/temp"                     , hdlr_time_board_temp,                   RW, "20", SP, NAC)        \
    DEFINE_FILE_PROP_P("time/board/led"                      , hdlr_time_board_led,                    WO, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/about/id"                       , hdlr_time_about_id,                     RO, "001", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/about/serial"                   , hdlr_time_about_serial,                 RW, "001", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/about/mcudevid"                 , hdlr_time_about_mcudevid,               RW, "001", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/about/mcurev"                   , hdlr_time_about_mcurev,                 RW, "001", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/about/mcufuses"                 , hdlr_time_about_mcufuses,               RW, "001", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/about/sw_ver"                   , hdlr_invalid,                           RO, VERSION, SP, NAC)

    // time/source/vtune must be set to 1403 for time boards populated with AOCJY and 1250 for boards with OX-174

#define DEFINE_FPGA()                                                                                                         \
    DEFINE_FILE_PROP_P("fpga/board/init_regs"                , hdlr_fpga_board_init_regs,              RW, "1", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/reg_rst_req"              , hdlr_fpga_board_reg_rst_req,            RW, "8", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/reg_rst_status"           , hdlr_fpga_board_reg_rst_status,         RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/user/regs"                      , hdlr_fpga_user_regs,                    RW, "0.0", SP, NAC)               \
    DEFINE_FILE_PROP_P("fpga/trigger/sma_dir"                , hdlr_fpga_trigger_sma_dir,              RW, "in", SP, NAC)               \
    DEFINE_FILE_PROP_P("fpga/trigger/sma_pol"                , hdlr_fpga_trigger_sma_pol,              RW, "negative", SP, NAC)          \
    DEFINE_FILE_PROP_P("fpga/trigger/sma_override"           , hdlr_fpga_trigger_sma_override,         RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/about/fw_ver"                   , hdlr_fpga_about_fw_ver,                 RW, VERSION, SP, NAC)             \
    DEFINE_FILE_PROP_P("fpga/about/sw_ver"                   , hdlr_invalid,                           RO, VERSION, SP, NAC)             \
    DEFINE_FILE_PROP_P("fpga/about/server_ver"               , hdlr_server_about_fw_ver,               RW, "", SP, NAC)                  \
    DEFINE_FILE_PROP_P("fpga/about/hw_ver"                   , hdlr_fpga_about_hw_ver,                 RW, VERSION, SP, NAC)             \
    DEFINE_FILE_PROP_P("fpga/about/id"                       , hdlr_fpga_about_id,                     RW, "001", SP, NAC)               \
    DEFINE_FILE_PROP_P("fpga/about/name"                     , hdlr_invalid,                           RO, PROJECT_NAME, SP, NAC)        \
    DEFINE_FILE_PROP_P("fpga/about/serial"                   , hdlr_fpga_about_serial,                 RW, "001", SP, NAC)               \
    DEFINE_FILE_PROP_P("fpga/about/cmp_time"                 , hdlr_fpga_about_cmp_time,               RW, "yyyy-mm-dd-hh-mm", SP, NAC)  \
    DEFINE_FILE_PROP_P("fpga/about/conf_info"                , hdlr_fpga_about_conf_info,              RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/dump"                     , hdlr_fpga_board_dump,                   WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/fw_rst"                   , hdlr_fpga_board_fw_rst,                 WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/flow_control/sfpa_port"   , hdlr_fpga_board_flow_control_sfpa_port, RW, "42809", SP, NAC)             \
    DEFINE_FILE_PROP_P("fpga/board/flow_control/sfpb_port"   , hdlr_fpga_board_flow_control_sfpb_port, RW, "42809", SP, NAC)             \
    DEFINE_FILE_PROP_P("fpga/board/gps_time"                 , hdlr_fpga_board_gps_time,               RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/gps_frac_time"            , hdlr_fpga_board_gps_frac_time,          RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/gps_sync_time"            , hdlr_fpga_board_gps_sync_time,          RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/jesd_sync"                , hdlr_fpga_board_jesd_sync,              WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/led"                      , hdlr_fpga_board_led,                    WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/reboot"                   , hdlr_fpga_board_reboot,                 RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/rst_softsys"              , hdlr_fpga_board_rst_softsys,            WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/test"                     , hdlr_fpga_board_test,                   WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/temp"                     , hdlr_fpga_board_temp,                   RW, "20", SP, NAC)                \
    DEFINE_FILE_PROP_P("fpga/board/gle"                      , hdlr_fpga_board_gle,                    RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/link/rate"                      , hdlr_fpga_link_rate,                    RW, "10000000000", SP, NAC)        \
    DEFINE_FILE_PROP_P("fpga/link/sfpa/ip_addr"              , hdlr_fpga_link_sfpa_ip_addr,            RW, "10.10.10.2", SP, NAC)        \
    DEFINE_FILE_PROP_P("fpga/link/sfpa/mac_addr"             , hdlr_fpga_link_sfpa_mac_addr,           RW, "aa:00:00:00:00:00", SP, NAC) \
    DEFINE_FILE_PROP_P("fpga/link/sfpa/ver"                  , hdlr_fpga_link_sfpa_ver,                RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/link/sfpa/pay_len"              , hdlr_fpga_link_sfpa_pay_len,            RW, "1400", SP, NAC)              \
    DEFINE_FILE_PROP_P("fpga/link/sfpb/ip_addr"              , hdlr_fpga_link_sfpb_ip_addr,            RW, "10.10.11.2", SP, NAC)        \
    DEFINE_FILE_PROP_P("fpga/link/sfpb/mac_addr"             , hdlr_fpga_link_sfpb_mac_addr,           RW, "aa:00:00:00:00:01", SP, NAC) \
    DEFINE_FILE_PROP_P("fpga/link/sfpb/ver"                  , hdlr_fpga_link_sfpb_ver,                RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/link/sfpb/pay_len"              , hdlr_fpga_link_sfpb_pay_len,            RW, "1400", SP, NAC)              \
    DEFINE_FILE_PROP_P("fpga/link/net/dhcp_en"               , hdlr_fpga_link_net_dhcp_en,             RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/link/net/hostname"              , hdlr_fpga_link_net_hostname,            RW, PROJECT_NAME, SP, NAC)        \
    DEFINE_FILE_PROP_P("fpga/link/net/ip_addr"               , hdlr_fpga_link_net_ip_addr,             RW, "192.168.10.2", SP, NAC)

#if (RX_40GHZ_FE) // reduced common settings without tx
    #define DEFINE_CM()                                                    \
    DEFINE_FILE_PROP_P("cm/chanmask-rx" , hdlr_cm_chanmask_rx , RW, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/rx/atten/val", hdlr_cm_rx_atten_val, WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/rx/gain/val" , hdlr_cm_rx_gain_val , WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/rx/freq/val", hdlr_cm_trx_freq_val, WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/rx/nco_adj" , hdlr_cm_trx_nco_adj , WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/rx/force_stream", hdlr_cm_rx_force_stream , RW, "0", SP, NAC)
#else // normal common settings with tx and rx
    #define DEFINE_CM()                                                    \
    DEFINE_FILE_PROP_P("cm/chanmask-rx" , hdlr_cm_chanmask_rx , RW, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/chanmask-tx" , hdlr_cm_chanmask_tx , RW, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/rx/atten/val", hdlr_cm_rx_atten_val, WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/rx/gain/val" , hdlr_cm_rx_gain_val , WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/tx/gain/val" , hdlr_cm_tx_gain_val , WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/tx/force_stream" , hdlr_cm_tx_force_stream , RW, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/trx/freq/val", hdlr_cm_trx_freq_val, WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/trx/nco_adj" , hdlr_cm_trx_nco_adj , WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/rx/force_stream", hdlr_cm_rx_force_stream , RW, "0", SP, NAC)
#endif

#define DEFINE_FPGA_POST()                                                                      \
    DEFINE_FILE_PROP_P("fpga/board/rst_postinit", hdlr_fpga_board_rst_postinit, RW, "1", SP, NAC)

// Contians information about the configuration
#define DEFINE_SYSTEM_INFO()\
    DEFINE_FILE_PROP_P("system/num_rx"                   , hdlr_invalid,                           RO, S_NUM_RX, SP, NAC)\
    DEFINE_FILE_PROP_P("system/num_tx"                   , hdlr_invalid,                           RO, S_NUM_TX, SP, NAC)\
    DEFINE_FILE_PROP_P("system/max_lo"              , hdlr_invalid,                           RO, S_MAX_RF_FREQ, SP, NAC)\
    DEFINE_FILE_PROP_P("system/min_lo"                   , hdlr_invalid,                           RO, MIN_LO_S, SP, NAC)\
    DEFINE_FILE_PROP_P("system/lo_step"              , hdlr_invalid,                           RO, LO_STEPSIZE_S, SP, NAC)\
    DEFINE_FILE_PROP_P("system/max_rate"                 , hdlr_max_sample_rate,                   RW, "1", SP, NAC)\

static prop_t property_table[] = {
    DEFINE_TIME()
    DEFINE_START_RFE_REBOOT()
    DEFINE_WAIT_RFE_REBOOT()
#define X(ch) DEFINE_RX_CHANNEL(ch)
    RX_CHANNELS
#undef X
#if (RX_40GHZ_FE)
    #define X(ch) DEFINE_RX_40GHZFE_CHANNEL(ch)
        RX_CHANNELS
    #undef X
#else
#define X(ch) DEFINE_TX_CHANNEL(ch)
    TX_CHANNELS
#undef X
#endif
    // NOTE: unlike on Cyan, on Crimson FPGA initialization happens last
    DEFINE_FPGA()
    DEFINE_FILE_PROP_P("save_config", hdlr_save_config, RW, "/home/root/profile.cfg", SP, NAC)
    DEFINE_FILE_PROP_P("load_config", hdlr_load_config, RW, "/home/root/profile.cfg", SP, NAC)
    DEFINE_CM()
    DEFINE_FPGA_POST()
    DEFINE_SYSTEM_INFO()
#define X(ch) DEFINE_RX_CHANNEL_POST(ch)
    RX_CHANNELS
#undef X
#define X(ch) DEFINE_TX_CHANNEL_POST(ch)
    TX_CHANNELS
#undef X
};

static const size_t num_properties = ARRAY_SIZE(property_table);

static char *tostr(const int num)
{
    char* str = calloc(32, sizeof(*str));
    sprintf(str, "%d", num);
    return str;
}

static int set_default_str(const char* const path, const char* const str)
{
    prop_t* prop = get_prop_from_cmd(path);
    if(prop) {
        snprintf(prop->def_val, MAX_PROP_LEN, str);
        return RETURN_SUCCESS;
    } else {
        PRINT(ERROR, "Unable to set initial string for property \"%s\", property not found\n", path);
        return RETURN_ERROR_PARAM;
    }
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

void dump_tree(void)
{
    int i;
    for(i = 0; i < num_properties; i++)
    {
        struct prop p = property_table[i];
        printf("%40s == %40s\n", p.path, p.def_val);
    }
}

void patch_tree(void) {
    const int base_port = 42820;

#define X(ch) set_default_int("rx/" #ch "/link/port", base_port + INT(ch));
    RX_CHANNELS
#undef X

#define X(ch) set_default_str("rx/" #ch "/link/ip_dest", ((INT(ch) % 2) == 0) ? "10.10.10.10" : "10.10.11.10");
    RX_CHANNELS
#undef X

#define X(ch) \
    set_default_int("tx/" #ch "/link/port",   base_port + INT(ch) + NUM_TX_CHANNELS); \
    set_default_int("tx/" #ch "/qa/fifo_lvl", base_port + INT(ch) + NUM_TX_CHANNELS); \
    set_default_int("tx/" #ch "/qa/oflow",    base_port + INT(ch) + NUM_TX_CHANNELS); \
    set_default_int("tx/" #ch "/qa/uflow",    base_port + INT(ch) + NUM_TX_CHANNELS);
    TX_CHANNELS
#undef X

    // Read a configuration file to overrid default values of the state tree. Must be done at adjusting the default state tree values

    FILE *alternate_tree_defaults_file;
    char property_args[MAX_PROP_LEN+MAX_PATH_LEN+1];
    // Make sure property_path and property_value are the same size as the buffer for the whole string, to avoid buffer overflows if the path of value component of the line exceed the maximum
    char property_path[MAX_PROP_LEN+MAX_PATH_LEN+1];
    char property_value[MAX_PROP_LEN+MAX_PATH_LEN+1];

    alternate_tree_defaults_file = fopen(ALTERNATE_TREE_DEFAULTS_PATH, "r");
    if(alternate_tree_defaults_file) {
        while( fgets(property_args, MAX_PROP_LEN+MAX_PATH_LEN+1, alternate_tree_defaults_file)) {
            sscanf(property_args, "%s = %s", property_path, property_value);
            int error_code = set_default_str(property_path, property_value);
            if(error_code) {
                PRINT(ERROR, "Unable to set alternative default property from arguments: %s\n", property_args);
            } else {
                PRINT(INFO, "Set default value of property \"%s\" to \"%s\"\n", property_path, property_value);
            }
        }
        fclose(alternate_tree_defaults_file);
    } else {
        PRINT(INFO, "No alternate default file found\n");
        // Clears errno (which will be set to file not found). Not necessary but makes spotting error flags at unknown points easier
        errno = 0;
    }
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

    const char *vcs = "/var/volatile/crimson/state/";
    const size_t vcsl = strnlen(vcs, MAX_PATH_LEN);
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

    path_strlen = strnlen(path, MAX_PATH_LEN);
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

char *get_abs_path(prop_t *prop, char *path, int max_len) {
    snprintf(path, max_len, STATE_DIR "/%s", prop->path);
    return path;
}

char *get_abs_dir(prop_t *prop, char *path, int max_length) {
    size_t dir_path_len = 0;
    size_t i = 0;
    while (prop->path[i]) {
        if (prop->path[i] == '/')
            dir_path_len = i;
        i++;
    }

    snprintf(path, max_length, STATE_DIR "/");

    if (dir_path_len != 0) {
        int state_path_len = strnlen(path, max_length);
        if(max_length > dir_path_len + state_path_len + 1) {
            // The + +1 is space for the null terminator
            snprintf(path + state_path_len, dir_path_len + 1, prop->path);
        } else {
            PRINT(ERROR, "path buffer is to small for path\n");
            abort();
        }
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
    sync_channels_prep(chan_mask);
    sync_channels_cleanup(chan_mask);
}

// Prepares for syncing channels (should happen automatically after prepartion finished)
void sync_channels_prep(uint8_t chan_mask) {
    char str_chan_mask[MAX_PROP_LEN] = "";
    sprintf(str_chan_mask + strlen(str_chan_mask), "%" PRIu8 "", 15);

    // Put JESD into continuous mode
    set_property("time/sync/sysref_mode", "continuous");
    usleep(300); // Wait for Sysref pulse to stabilize and bias around zero.

    // Unmask SYSREF on the FPGA
    write_hps_reg("res_rw7", 0x10000000);

    /* Set all boards' SYSREF detection gate to ON */
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 1\r");
    ping(uart_rx_fd[0], (uint8_t *)buf, strlen(buf));
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 1\r");
    ping(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));

    usleep(200000); // Some wait time for MCUs to be ready
}

// Cleans up state after syncing channels
void sync_channels_cleanup(uint8_t chan_mask) {
    char str_chan_mask[MAX_PROP_LEN] = "";

    // Mask SYSREF on the FPGA
    write_hps_reg("res_rw7", 0);

    /* Turn off all boards' SYSREF detection gates */
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 0\r");
    ping(uart_rx_fd[0], (uint8_t *)buf, strlen(buf));
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 0\r");
    ping(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));

    // Put JESD into pulsed mode
    set_property("time/sync/sysref_mode", "pulsed");
}

// Returns 1 on success, 0 on failure
int set_pll_frequency(int uart_fd, uint64_t reference, pllparam_t *pll,
                       bool tx, size_t channel, bool use_lut_if_possible) {

    snprintf(buf, MAX_PROP_LEN, "rf -c %c \r", channel + 'a');
    ping(uart_fd, (uint8_t *)buf, strlen(buf));
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

    double freq = pll->vcoFreq / pll->d;

    if (synth_lut_is_enabled(tx, channel) && use_lut_if_possible) {
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

    // write ADF4355/ADF5355 Output RF Power
    strcpy(buf, "rf -g ");
    sprintf(buf + strlen(buf), "%" PRIu8 "", 1 /*pll->power*/);
    // default to lower mid power
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // write ADF4355/ADF5355 Output Frequency
    strcpy(buf, "rf -f ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(freq / 1000));
    // Send output frequency in kHz
    strcat(buf, "\r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    //Wait for PLL to lock, timeout after 100ms
    struct timespec timeout_start;
    int time_ret = clock_gettime(CLOCK_MONOTONIC_COARSE, &timeout_start);
    const int timeout_ns = 100000000;

    if(time_ret) {
        PRINT(ERROR, "Get time failed with %s. Waiting %ims instead of polling\n", strerror(errno), timeout_ns/1000000);
        usleep(timeout_ns/1000);
        if (check_rf_pll(channel, uart_fd)) {
            return 1; //success
        } else {
            // Mute PLL to avoid transmitting with an enexpected frequency
            strcpy(buf, "rf -c " STR(ch) " -z\r");
            ping(uart_fd, (uint8_t *)buf, strlen(buf));
            if(tx) {
                PRINT(ERROR, "Tx PLL unlocked. Muting PLL\n");
            } else {
                PRINT(ERROR, "Rx PLL unlocked. Muting PLL\n");
            }
            return 0;
        }
    }

    int lock_failed = 0;
    // Polling loop waiting for PLL to finish locking
    while(!check_rf_pll(channel, uart_fd)) {
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC_COARSE, &current_time);
        int time_difference_ns = (current_time.tv_sec - timeout_start.tv_sec) * 1000000000 + (current_time.tv_nsec - timeout_start.tv_nsec);

        // Timout occured, print error message and
        if(time_difference_ns > timeout_ns) {
            lock_failed = 1;
            break;
        }

        // Wait 1us between polls to avoid spamming logs
        usleep(1);
    }

    if(lock_failed) {
        // If setting the PLL failed using the lookup table, reattempt without it
        if(synth_lut_is_enabled(tx, channel) && use_lut_if_possible) {
            PRINT(ERROR, "PLL lock failed when attempting to use the lookup table. Re-attempting without the lookup table\n");
            return set_pll_frequency(uart_fd, reference, pll, tx, channel, false);
        } else {
            // Mute PLL to avoid transmitting with an enexpected frequency
            snprintf(buf, MAX_PROP_LEN, "rf -c %c -z\r", channel + 'a');
            ping(uart_fd, (uint8_t *)buf, strlen(buf));
            if(tx) {
                PRINT(ERROR, "Tx PLL unlocked. Muting PLL\n");
            } else {
                PRINT(ERROR, "Rx PLL unlocked. Muting PLL\n");
            }
            return 0;
        }
    } else {
        // success
        return 1;
    }
}

void set_lo_frequency(int uart_fd, pllparam_t *pll, uint8_t channel) {
    // extract lo variables and pass to MCU (LMX2595)
#if defined(RTM6) || defined(RTM7)
    // set_lo_frequency not supported by RTM6/7 hardware
    return;
#endif

    // map channel number to chan_mask
    uint8_t chan_mask = 1 << channel;
    // set the chan_mask
    snprintf(buf, MAX_PROP_LEN, "lmx -c %" PRIu8 "\r", chan_mask);
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    double freq = (pll->vcoFreq / pll->d) + (pll->x2en * pll->vcoFreq / pll->d);

    // Reinitialize the LMX. For some reason the initialization on server boot, doesn't seem to be enough
    strcpy(buf, "lmx -k \r");
    ping(uart_fd, (uint8_t *)buf, strlen(buf));

    // Send Phase Detector Frequency in MHz to MCU
    strcpy(buf, "lmx -o ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(pll->ref_freq / pll->R / 1000000));
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

    //Wait for PLL to lock, timeout after 100ms
    struct timespec timeout_start;
    int time_ret = clock_gettime(CLOCK_MONOTONIC_COARSE, &timeout_start);
    const int timeout_ns = 100000000;

    if(time_ret) {
        PRINT(ERROR, "Get time failed with %s. Waiting %ims instead of polling\n", strerror(errno), timeout_ns/1000000);
        usleep(timeout_ns/1000);
        if(!check_rf_pll(channel, uart_fd)){
            // Mute PLL to avoid transmitting with an enexpected frequency
            strcpy(buf, "rf -c " STR(ch) " -z\r");
            ping(uart_fd, (uint8_t *)buf, strlen(buf));
            PRINT(ERROR, "LMX unlocked. Muting PLL\n");
        }
        return;
    }

    // Polling loop waiting for PLL to finish locking
    while(!check_rf_pll(channel, uart_fd)) {
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC_COARSE, &current_time);
        int time_difference_ns = (current_time.tv_sec - timeout_start.tv_sec) * 1000000000 + (current_time.tv_nsec - timeout_start.tv_nsec);

        // Timout occured, print error message and
        if(time_difference_ns > timeout_ns) {
            // Mute PLL to avoid transmitting with an enexpected frequency
            strcpy(buf, "rf -c " STR(ch) " -z\r");
            ping(uart_fd, (uint8_t *)buf, strlen(buf));
            PRINT(ERROR, "LMX unlocked. Muting PLL\n");
            return;
        }

        // Wait 1us between polls to avoid spamming logs
        usleep(1);
    }
}

int set_freq_internal(const bool tx, const unsigned channel,
                      const double freq) {

    typedef int (*fp_t)(const char *, char *);
    if (RX_40GHZ_FE && tx) {
        return RETURN_ERROR_PARAM;
    }

    static const fp_t rx_fp[] = {
#define X(ch) hdlr_rx_##ch##_rf_freq_val,
        RX_CHANNELS
#undef X
    };

    static const fp_t tx_fp[] = {
#define X(ch) hdlr_tx_##ch##_rf_freq_val,
        TX_CHANNELS
#undef X
    };

    int r;

    char req_buf[MAX_PROP_LEN];
    char rsp_buf[MAX_PROP_LEN];

    if (channel > (tx ? ARRAY_SIZE(tx_fp) : ARRAY_SIZE(rx_fp))) {
        r = E2BIG;
        PRINT(ERROR, "channel %u is invalid (%d,%s)\n", channel, r,
              strerror(r));
        return r;
    }

    const fp_t *fp = tx ? tx_fp : rx_fp;

    memset(req_buf, '\0', sizeof(req_buf));
    memset(rsp_buf, '\0', sizeof(rsp_buf));

    // N.B. the print formatter in this case must be equal to the one in
    // hdlr_XX_X_rf_freq_val
    snprintf(req_buf, sizeof(req_buf), "%lf", freq);

    for(int n = 0; n < MAX_AUTOCAL_ATTEMPTS; n++) {
        r = fp[channel](req_buf, rsp_buf);
        if (RETURN_SUCCESS != r) {
            PRINT(ERROR, "function call to hdlr_XX_X_rf_freq_val() failed (%d)\n",
                r);
            flush_uart_comm(tx ? uart_tx_fd[channel] : uart_rx_fd[channel]);
            return EIO;
        }

        double actual_freq = 0;
        if (1 != sscanf(rsp_buf, "%lf", &actual_freq) || actual_freq != freq) {
            PRINT(ERROR, "%s %c: autocal attempt %i failed for freq %lf\n", tx ? "TX" : "RX",
                'A' + channel, n, freq);
        } else {
            // Attempt to set frequency success, no further attemptes required
            return EXIT_SUCCESS;
        }
    }

    // Return error for autocal failure
    // TODO: replace EIO with proper error, the person who chose to use EIO assummed UART errors were the only possible problem
    r = EIO;

    return r;
}

#endif//defined(VAUNT)
