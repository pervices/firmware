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

#if defined(TATE_NRNT)

#if 1 /* Removes headers for quick gcc -E diagnostics for XMACRO stuffs */
    #include "properties.h"

    #include "array-utils.h"
    #include "mmap.h"
    #include "property_manager.h"
    #include "time_it.h"

    #include <ctype.h>
    #include <stdbool.h>
    #include <stdio.h>
    #include <string.h>
    #include <math.h>
#endif

#include <signal.h>
#include "gpio_pins.h"
#include "led.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include "variant_config/tate_sample_rate_config.h"
#include "variant_config/tate_fpga_config.h"
#include "variant_config/tate_rtm_config.h"

#define ALTERNATE_TREE_DEFAULTS_PATH "/etc/cyan/alternate_tree_defaults.cfg"

// Alias PLL_CORE_REF_FREQ_HZ for clarity
#define LO_STEPSIZE PLL_CORE_REF_FREQ_HZ

//a factor used to biased sample rate rounding to round down closer to 1 encourages rounding down, closer to 0 encourages rounding up
#define RATE_ROUND_BIAS 0.75

// Minimum NCO for the NCO on the DAC (not the FPGA)
#define MIN_DAC_NCO 0

//the code that uses these assumes the tx mcu is expecting an attenuator code (attenuation = step size * code)
//AB in this variable names stands for all bands, and they are relevant to all bands
#define MIN_RF_ATTEN_TX_AB 0.0
#define MAX_RF_ATTEN_TX_AB 30.0
#define RF_ATTEN_STEP_TX 2.0
#define MIN_RF_GAIN_TX_AB MIN_RF_ATTEN_TX_AB
#define MAX_RF_GAIN_TX_AB MAX_RF_ATTEN_TX_AB

//The voltage range used to control and amplifier in high band
//The full range is larger, but outside of this range it is very non-linear
#define MIN_GAIN_V_TX_HB_GAIN 0.8
#define MAX_GAIN_V_TX_HB_GAIN 2.2
//Gain range of a high band only amplifier, note that this is in addition to the gains affecting all bands
#define MIN_GAIN_TX_HB 0.0
#define MAX_GAIN_TX_HB 23.0

//Compnent properties in rx, used to figure out how to set up game
//This are likely to change between variants, both thier values and how they are used
#define AM1081_GAIN 17
#define AM1075_GAIN 18
//The actual range of the LMH6401 vga is -6 to 26, but the MCU cannot accept negative numbers
#define LMH6401_MAX_GAIN 32
#define LMH6401_MIN_GAIN 0
#define LTC5586_MAX_GAIN 15
#define LTC5586_MIN_GAIN 8
// Maximum setting on the variable attenuator(s) in mid and high band
// Currently it is the attenuation from the attenuator on LTC 5586
#define MID_HIGH_MAX_ATTEN 31

//used for rf freq val calc when in high band
#define HB_STAGE2_MIXER_FREQ 1800000000

#define IPVER_IPV4 0
#define IPVER_IPV6 1

#define STREAM_ON  1
#define STREAM_OFF 0

static uint8_t rx_power[NUM_RX_CHANNELS];
static uint8_t tx_power[NUM_TX_CHANNELS];

//Registers used to store dsp gain
//ch0 uses [7:0] of the map[0], ch1 uses [15:8] of map[0], ch4 uses [7:0] of map[1]
#if NUM_RX_CHANNELS > 0
    static const char *rxg_map[4] = { "rxga", "rxge", "rxgi", "rxgm" };
#endif
#if NUM_TX_CHANNELS > 0
    static const char *txg_map[4] = { "txga", "txge", "txgi", "txgm" };
#endif

#define MAX_POSSIBLE_CHANNELS 16
static const char *rx_possible_reg4_map[MAX_POSSIBLE_CHANNELS] = { "rxa4", "rxb4", "rxc4", "rxd4", "rxe4", "rxf4", "rxg4", "rxh4", "rxi4", "rxj4", "rxk4", "rxl4", "rxm4", "rxn4", "rxo4", "rxp4" };
static const char *tx_possible_reg4_map[MAX_POSSIBLE_CHANNELS] = { "txa4", "txb4", "txc4", "txd4", "txe4", "txf4", "txg4", "txh4", "txi4", "txj4", "txk4", "txl4", "txm4", "txn4", "txo4", "txp4" };

// Registers contianing the src port for rx and dst port for tx overlap but are not identical
// In the furture they should be entirely seperate
// When using DDR, tx will use ports txa15-txa18 for sfp a, txb15-18 for sfp b...
// When not using DDR tx destination ports will be locaiton 0 in the following array for ch 0, location 1 for ch1, location 2 for ch2...
// Rx will always be location 0 for ch 0, location 1 for ch1...
// Currently the code assumes DDR is used for 40G and never used for 100G
#define NUM_DEVICE_SIDE_PORTS 16
static const char *device_side_port_map[NUM_DEVICE_SIDE_PORTS] = { "txa15", "txa16", "txa17", "txa18", "txb15", "txb16", "txb17", "txb18", "txc15", "txc16", "txc17", "txc18", "txd15", "txd16", "txd17", "txd18", };

// Set to true after changing rx to 1G so it doesn't get set to it again
// Only used with USE_3G_AS_1G
static uint8_t rx_3g_set_to_1g[NUM_RX_CHANNELS] = {0};

// Maximum user set delay for i or q
const int max_iq_delay = 32;

static uint_fast8_t jesd_enabled = 0;

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
int sfp_max_reset_attempts = 10;
// SFP always came up in 90/90 tests, so reboot on SFP fail has been disabled
int sfp_max_reboot_attempts = 5;
int max_brd_reboot_attempts = 5;

static uint8_t rx_stream[NUM_RX_CHANNELS] = {0};

//the time async_pwr started running, used when calculating if it timed out
//pwr_board_timeout is in seconds
#define PWR_TIMEOUT 15
#if NUM_RX_CHANNELS > 0
    static time_t rx_async_start_time[NUM_RX_CHANNELS];
#endif
#if NUM_TX_CHANNELS > 0
    static time_t tx_async_start_time[NUM_TX_CHANNELS];
#endif

uint8_t *_save_profile;
uint8_t *_load_profile;
char *_save_profile_path;
char *_load_profile_path;

static const uint8_t ipver[] = {
    IPVER_IPV4,
    IPVER_IPV4,
};

const int jesd_reset_delay = 100000;
const int jesd_mask_delay = 200000;

int jesd_master_reset();
static int hdlr_jesd_reset_master(const char *data, char *ret);

int set_lo_frequency_rx(int uart_fd, uint64_t reference, pllparam_t *pll, int channel);
int set_lo_frequency_tx(int uart_fd, uint64_t reference, pllparam_t *pll, int channel);

typedef enum {
    pulsed = 0,
    continuous = 1,
    unspecified_sysref
} sysref_modes;

sysref_modes current_sysref_mode = unspecified_sysref;

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

    int32_t total_bytes = 0;
    uint32_t cur_bytes = 0;

    const long t0 = time_it();

    while (contains((char *)uart_ret_buf, '>', total_bytes) < 1) {
        int recv_error_code = recv_uart_comm(uartfd, (uint8_t *)(uart_ret_buf + total_bytes), &cur_bytes, MAX_UART_RET_LEN -
        total_bytes);
        if(recv_error_code == RETURN_ERROR_PARAM) {
            PRINT(ERROR, "UART return buffer to small, some data was lost\n");
            return RETURN_ERROR_INSUFFICIENT_BUFFER;
        } else if(recv_error_code == RETURN_ERROR_UART_TIMEOUT) {
            // Add null terminator to the start of uart_ret_buf (effectively emptying it) in the event of an error
            PRINT(ERROR, "UART read request timed out\n");
            uart_ret_buf[0] = 0;
            return RETURN_ERROR_UART_TIMEOUT;
        } else if(recv_error_code == RETURN_ERROR_UART_INVALID_FD) {
            // Add null terminator to the start of uart_ret_buf (effectively emptying it) in the event of an error
            PRINT(ERROR, "UART invalid file descriptor\n");
            uart_ret_buf[0] = 0;
            return RETURN_ERROR_UART_INVALID_FD;
        }else if(recv_error_code) {
            PRINT(ERROR, "unrecongnized error\n");
            // Add null terminator to the start of uart_ret_buf (effectively emptying it) in the event of an error
            uart_ret_buf[0] = 0;
            return RETURN_ERROR;
        }

        total_bytes += cur_bytes;
    }
    const long t1 = time_it();

    // This print is very useful when debugging, but is normally comented out because it fills up the journal
    PRINT(INFO, "read uart %ld : %s\n", t1 - t0, uart_ret_buf);
    return RETURN_SUCCESS;
}

// Finds the optimal value for the sample rate blocks
static uint16_t get_optimal_sr_factor(double *rate, double dsp_rate) {
    double max_factor = 65536; // 2^16
    // 16 bits are used to store the sample factor
    uint16_t sample_factor;

    /*the upper sample factor is lower because the rate is divided by it*/
    if(rate == 0) {
        return max_factor;\
    }
    uint16_t upper_sample_factor = floor(dsp_rate/(*rate));
    double upper_rate = dsp_rate/upper_sample_factor;
    uint16_t lower_sample_factor = ceil(dsp_rate/(*rate));
    double lower_rate = dsp_rate/lower_sample_factor;
    double rate_range = upper_rate - lower_rate;
    double lower_diff = *rate - lower_rate;

    if(lower_diff/rate_range > RATE_ROUND_BIAS) {
        sample_factor = upper_sample_factor;
        *rate = upper_rate;
    } else {
        sample_factor = lower_sample_factor;
        *rate = lower_rate;
    }
    /* The above calculations output the number of samples to the DAC per sample send by the host*/
    /* the register for sample factor takes the number of samples that need to be added per sample from the host*/
    if(sample_factor != 0) {
        sample_factor--;
    }
    return sample_factor;
}

// reg: the register to write to
// shift: the point in the register to write the value to (the value is 16 bit)
// desired: the target length of the VITA 49 part of the packet, not including the trailer if applicable
uint32_t set_payload_len(char* reg, uint32_t shift, uint32_t desired) {
    // Cap payload length
    if(desired > RX_MAX_PAYLOAD) {
        desired = RX_MAX_PAYLOAD;
    }
#ifdef S1000
    // Restrict desired to legal values due to integer rounding
    uint32_t actual = desired - (desired % 4);
#elif defined(S3000)
    // payload length for 3G is hard coded
    uint32_t actual = RX_MAX_PAYLOAD;
#endif

    write_hps_reg_mask(reg, actual << shift, 0xffff << shift);

    return actual;
}

// Waits for the FPGA to finish reseting
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
void wait_for_fpga_reset() {
    uint32_t sys18_val;
    // Delay to avoid repeatedly reading the register to quickly, which is likely the cause of linux freezes
    do {
        usleep(50000);
        read_hps_reg("sys18", &sys18_val);
    } while (sys18_val & 0x00ff0000);
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
        fscanf(data_file, "%li", value);
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

    fprintf(data_file, "%li", value);
    fclose(data_file);
}

int network_speed_cache = 0;
int is_network_speed_cached = 0;
static int get_network_speed() {
    if(is_network_speed_cached) {
        return network_speed_cache;
    } else {
        uint32_t network_config = 0;
        read_hps_reg("res_ro12", &network_config);
        network_speed_cache = (network_config >> 4) & 0xff;
        is_network_speed_cached = 1;
        return network_speed_cache;
    }
}

uint32_t is_hps_only() {
    // The flag is always high in old versions of the FPGA
    // Assume the build is not HPS only if it was compiled before that flag was introduced
    // sys15 stores the compile time, see hdlr_fpga_about_cmp_time for a detailed breakdown of the register
    uint32_t compile_time = 0;
    read_hps_reg("sys15", &compile_time);
    if(compile_time < 0x7e71d000) {
        return 0;
    }


    uint32_t flag = 0;
    read_hps_reg("res_ro12", &flag);
    flag = (flag >> 30) & 0x1;
    return flag;

}

uint32_t is_ddr_used() {
    uint32_t val = 0;
    read_hps_reg("res_ro12", &val);
    val = (val >> 31) & 0x1;

    return val;
}


#if NUM_TX_CHANNELS > 0
    static int get_tx_dst_port_map_loc(int chan) {
        if(is_ddr_used()) {
            return tx_dst_port_map[chan];
        } else {
            return chan;
        }
    }
#endif

// XXX
// Statement Expressions are bad... but this code will be replaced soon anyway.
#define set_reg_bits(name, shift, mask, val)                                   \
    ({                                                                         \
        int _r;                                                                \
        uint32_t _t = 0;                                                       \
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
/* -------------------------------- GATE ------------------------------------ */
/* -------------------------------------------------------------------------- */

#if NUM_TX_CHANNELS > 0
    static int tx_set_gating_mode(const char *chan, bool dsp) {
        char reg_name[8];
        snprintf(reg_name, sizeof(reg_name), "tx%s6", chan);
        return set_reg_bits(reg_name, 18, 1, dsp);
    }

    static int tx_valid_gating_mode(const char *data, bool *dsp) {
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
#endif

    static int valid_trigger_mode(const char *data, bool *edge) {

        if (false) {
        } else if (0 == strncmp("edge", data, strlen("edge"))) {
            *edge = true;
        } else if (0 == strncmp("level", data, strlen("level"))) {
            *edge = false;
        } else {
            PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)");
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

    if(tx) {
        snprintf(regname_msw, sizeof(regname_msw), tx_nsamp_msw_map[(*chan)-'a']);
        snprintf(regname_lsw, sizeof(regname_lsw), tx_nsamp_lsw_map[(*chan)-'a']);
    } else {
        snprintf(regname_msw, sizeof(regname_msw), "%s%s%u", tx ? "tx" : "rx", chan,
             tx ? 7 : 10);
        snprintf(regname_lsw, sizeof(regname_lsw), "%s%s%u", tx ? "tx" : "rx", chan,
             tx ? 8 : 11);
    }

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
    if(tx) {
        return set_reg_bits(tx_trig_map[(*chan)-'a'], 10, 0b11, sel);
    }
    else {
        return set_reg_bits(rx_trig_map[(*chan)-'a'], 10, 0b11, sel);
    }
}

static int set_trigger_mode(bool sma, bool tx, const char *chan, bool edge) {
    if(tx && sma) {
        return set_reg_bits(tx_trig_map[(*chan)-'a'], 0, 1, edge);
    } else if(tx && !sma) {
        return set_reg_bits(tx_trig_map[(*chan)-'a'], 4, 1, edge);
    } else if( !tx && sma) {
        return set_reg_bits(rx_trig_map[(*chan)-'a'], 0, 1, edge);
    } else if (!tx && !sma) {
        return set_reg_bits(rx_trig_map[(*chan)-'a'], 4, 1, edge);
    }
    return -1;
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

#define X(ch)                                                              \
    static int hdlr_tx_##ch##_trigger_sma_mode(const char *data, char *ret) {  \
        int r;                                                                 \
        bool val = 0;                                                          \
        r = valid_trigger_mode(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_trigger_mode(true, true, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_edge_backoff(const char *data,           \
                                                   char *ret) {                \
        int r;                                                                 \
        uint32_t val = 0;                                                          \
        r = valid_edge_backoff(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_edge_backoff(true, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_edge_sample_num(const char *data, char *ret) {\
        int r;                                                                 \
        uint64_t val = 0;                                                          \
        r = valid_edge_sample_num(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        \
        /* Adjustment to number of samples requested,the number written to the register and the actual number will be different */\
        /* This adjustment will result in the correct final number */\
        if(MAX_SAMPLE_RATE == 1000) {\
            char s_rate[100];\
            get_property("tx/" STR(ch) "/dsp/rate", s_rate, 100);\
            double rate = 0;\
            sscanf(s_rate, "%lf", &rate);\
            if( rate == 1000000000 ) {\
                val = (uint64_t)(val/4)*4;\
                snprintf(ret, MAX_PROP_LEN, "%lu", val);\
                val -= 20;\
            } else if (rate >= 500000000 ) {\
                val = (uint64_t)(val/2)*2;\
                snprintf(ret, MAX_PROP_LEN, "%lu", val);\
                val -= 10;\
            } else if (rate >= 250000000) {\
                val = (uint64_t)(val/2)*2;\
                snprintf(ret, MAX_PROP_LEN, "%lu", val);\
                val -= 4; \
            } else if (rate >= 100000000) {\
                val = (uint64_t)(val/2)*2;\
                snprintf(ret, MAX_PROP_LEN, "%lu", val);\
                val -= 2;\
            }\
        } else if(MAX_SAMPLE_RATE == 3000) {\
            val = (uint64_t)(val/8)*8;\
            snprintf(ret, MAX_PROP_LEN, "%lu", val);\
            val -= 48;\
        } else {\
            PRINT(ERROR, "function not implemented for variants with maximum sample rate: %i\n", MAX_SAMPLE_RATE);\
        }\
        r = set_edge_sample_num(true, #ch, val);        \
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_trig_sel(const char *data, char *ret) {  \
        int r;                                                                 \
        uint32_t val = 0;                                                          \
        r = valid_trigger_sel(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_trigger_sel(true, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_ufl_dir(const char *data, char *ret) {   \
        int r;                                                                 \
        bool val = 0;                                                          \
        r = valid_trigger_dir(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_trigger_ufl_dir(true, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_ufl_mode(const char *data, char *ret) {  \
        int r;                                                                 \
        bool val = 0;                                                          \
        r = valid_trigger_mode(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_trigger_mode(false, true, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_ufl_pol(const char *data, char *ret) {   \
        int r;                                                                 \
        bool val = 0;                                                          \
        r = valid_trigger_pol(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_trigger_ufl_pol(true, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_time_disable(const char *data, char *ret) {   \
        uint32_t val;                                                          \
        int r = valid_trig_time_disable(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trig_time_disable(true, #ch, val);\
        } else {\
            return r;\
        }\
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_trigger_time_gate_logic(const char *data, char *ret) {   \
        uint32_t val;                                                          \
        int r = valid_time_gate_logic(data, &val);\
        if(r == RETURN_SUCCESS) {\
            return set_trig_time_gate_logic(true, #ch, val);\
        } else {\
            return r;\
        }\
    }
TX_CHANNELS
#undef X

#define X(ch)\
    static int hdlr_rx_##ch##_trigger_sma_mode(const char *data, char *ret) {  \
        int r;                                                                 \
        bool val = 0;                                                          \
        r = valid_trigger_mode(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_trigger_mode(true, false, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_edge_backoff(const char *data,           \
                                                   char *ret) {                \
        int r;                                                                 \
        uint32_t val = 0;                                                          \
        r = valid_edge_backoff(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_edge_backoff(false, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_edge_sample_num(const char *data,        \
                                                      char *ret) {             \
        int r;                                                                 \
        uint64_t val = 0;                                                          \
        r = valid_edge_sample_num(data, &val);\
        val = (val / NSAMPS_MULTIPLE_RX) * NSAMPS_MULTIPLE_RX;\
        val = val * DEAULT_OTW_RX / 16;\
        uint64_t actual_nsamps = val * 16 / DEAULT_OTW_RX;\
        snprintf(ret, MAX_PROP_LEN, "%lu", actual_nsamps);                                             \
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_edge_sample_num(false, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_trig_sel(const char *data, char *ret) {  \
        int r;                                                                 \
        uint32_t val = 0;                                                          \
        r = valid_trigger_sel(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_trigger_sel(false, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_ufl_dir(const char *data, char *ret) {   \
        int r;                                                                 \
        bool val = 0;                                                          \
        r = valid_trigger_dir(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_trigger_ufl_dir(false, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_ufl_mode(const char *data, char *ret) {  \
        int r;                                                                 \
        bool val = 0;                                                          \
        r = valid_trigger_mode(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_trigger_mode(false, false, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_ufl_pol(const char *data, char *ret) {   \
        int r;                                                                 \
        bool val = 0;                                                          \
        r = valid_trigger_pol(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = set_trigger_ufl_pol(false, #ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_time_disable(const char *data, char *ret) {   \
        uint32_t val;                                                              \
        return valid_trig_time_disable(data, &val) ||                          \
             set_trig_time_disable(false, #ch, val);                              \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_trigger_time_gate_logic(const char *data, char *ret) {   \
        uint32_t val;                                                              \
        return valid_time_gate_logic(data, &val) ||                                        \
            set_trig_time_gate_logic(false, #ch, val);                             \
    }
RX_CHANNELS
#undef X

// Every uart send command must be accompanied by a uart read command
// so that the command prompt '>' is respected before the next send uart
// command can be used. This removes the need for delay calls in the uart
// send function.
static int ping(const int fd, uint8_t *buf, const size_t len) {
    //sets the first byte of the turn buffer to null, effectively clearing it
    uart_ret_buf[0] = 0;
    send_uart_comm(fd, buf, len);
    int error_code = read_uart(fd);
    return error_code;
}
static void ping_write_only(const int fd, uint8_t *buf, const size_t len) {
    //sets the first byte of the turn buffer to null, effectively clearing it
    uart_ret_buf[0] = 0;
    send_uart_comm(fd, buf, len);
}
//ping with a check to see if a board is inserted into the desired channel, does nothing if there is no board
//ch is used only to know where in the array to check if a board is present, fd is still used to say where to send the data
static int ping_rx(const int fd, uint8_t *buf, const size_t len, int ch) {
    if(!(rx_power[ch] & PWR_NO_BOARD)) {
        int error_code = ping(fd, buf, len);
        //Due to hardware issues some boards will report as on even when the slot is empty
        if(error_code) {
            rx_power[ch] = rx_power[ch] | PWR_NO_BOARD;
            PRINT(ERROR, "Board %i failed to repond to uart, assumming the slot is empty\n", ch);
        }
        return error_code;
    //empties the uart return buffer
    } else {
        uart_ret_buf[0] = 0;
        return 0;
    }
}
static int ping_tx(const int fd, uint8_t *buf, const size_t len, int ch) {
    if(!(tx_power[ch] & PWR_NO_BOARD)) {
        int error_code = ping(fd, buf, len);
        //Due to hardware issues some boards will report as on even when the slot is empty
        if(error_code) {
            tx_power[ch] = tx_power[ch] | PWR_NO_BOARD;
            PRINT(ERROR, "Board %i failed to repond to uart, assumming the slot is empty\n", ch);
        }
        return error_code;
    //empties the uart return buffer
    } else {
        uart_ret_buf[0] = 0;
        return 0;
    }
}

// Verifies the rf pll is good. Returns 1 if the pll is locked
int check_rf_pll(int ch, bool is_tx) {
    snprintf(buf, sizeof(buf), "status -l\r");
    if(is_tx) {
        ping(uart_tx_fd[ch], (uint8_t *)buf, strlen(buf));
    } else {
        ping(uart_rx_fd[ch], (uint8_t *)buf, strlen(buf));
    }
    int pll_chan; // dummy variable used to deal with the pll channel number being different
    int result;
    sscanf((char *)uart_ret_buf, "CHAN: 0x%x, PLL Lock Detect: 0x%x", &pll_chan, &result);
    return result;
}

/* -------------------------------------------------------------------------- */
/* --------------------------------- TX ------------------------------------- */
/* -------------------------------------------------------------------------- */

#define X(ch)                                                              \
    \
    /*Interface for setting the dac nco on every dac. Currently only ch0 is used*/\
    static int hdlr_tx_##ch##_dac_nco_freq(const char *data, char *ret) {      \
        double freq;                                                           \
        sscanf(data, "%lf", &freq);                                            \
        uint32_t freq_hz = 0;                                                  \
        uint32_t freq_mhz = 0;                                                 \
        \
        /*Currently this function only takes positive values*/\
        if(freq < MIN_DAC_NCO) freq = MIN_DAC_NCO;\
        else if (freq > MAX_DAC_NCO) freq = MAX_DAC_NCO;\
        \
        if(freq != 0 && MAX_SAMPLE_RATE == 3000) {\
            freq = 0;\
            PRINT(ERROR, "The DAC can only to be set to 0 when operating at 3Gsps");\
        }\
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
        /*command format:*/\
        /*nco -t (type) -n (number) -a (?) -h (6 least significant digits of freq) -m freq in megahertz rounded down*/\
        snprintf(buf, MAX_PROP_LEN, "nco -t d -n 0 -a 0 -h %" PRIu32 " -m %" PRIu32 " -s\r", freq_hz, freq_mhz);\
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)); \
        snprintf(ret, MAX_PROP_LEN, "%lu", (uint64_t)(freq_mhz * 1000000 + freq_hz));\
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*Interface for setting the nco on every channel. Currently only ch0 is used*/\
    static int hdlr_tx_##ch##_ch_nco_freq(const char *data, char *ret) {      \
        /*The channelizer nco is not used in the current configuration*/\
        snprintf(ret, MAX_PROP_LEN, "0");\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*Interface for setting the nco on every channel. Currently only ch0 is used*/\
    static int hdlr_tx_##ch##_dac_gain_atten(const char *data, char *ret) {      \
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
        snprintf(buf, MAX_PROP_LEN, "dac -c 0 -g %" PRIu32 "\r", gaincode);\
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_lo_freq(const char *data, char *ret) {        \
        uint64_t freq = 0;                                                      \
        sscanf(data, "%" SCNd64 "", &freq);                                     \
        char fullpath[PROP_PATH_LEN] = "tx/" STR(ch) "/rf/band";                \
        int band;                                                               \
        char band_read[3];                                                      \
                                                                                \
        /* if freq = 0, mute PLL */                                             \
        if (freq == 0) {                                                        \
            strcpy(buf, "lmx -k\r");                                            \
            ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
            snprintf(ret, MAX_PROP_LEN, "%i", 0);                                              \
            return RETURN_SUCCESS;                                              \
        }                                                                       \
        \
        /* check band: if HB, subtract freq to account for cascaded mixers*/    \
        get_property(fullpath,band_read,3);                                   \
        sscanf(band_read, "%i", &band);                                         \
        if (band == 2) {                                                        \
            freq -= HB_STAGE2_MIXER_FREQ;                                      \
        }                                                                      \
                                                                                \
        \
        freq = lround((freq / (double)LO_STEPSIZE)) * LO_STEPSIZE;\
        \
        /* if freq out of bounds, mute lmx*/                                    \
        if ((freq < LMX2595_RFOUT_MIN_HZ) || (freq > LMX2595_RFOUT_MAX_HZ)) {   \
            strcpy(buf, "lmx -k\r");                                            \
            ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
            PRINT(ERROR,"LMX Freq Invalid \n");                                 \
            snprintf(ret, MAX_PROP_LEN, "%i", 0);                                              \
            return RETURN_ERROR;                                                \
        }                                                                       \
                                                                               \
        /* run the pll calc algorithm */                                       \
        pllparam_t pll = pll_def_lmx2595;                                      \
        long double outfreq = 0;                                                \
        /* Attempt to find an lo setting for the desired frequency using default R divider*/\
        /* NOTE: setFreq finds the pll settings and sores then in the provided struct */\
        /* It does not actually set the frequency */\
        outfreq = setFreq(&freq, &pll);                                         \
                                                                                \
        /* Attempt to find an lo setting for desired frequency using other R dividers*/\
        while (((pll.N < pll.n_min) && (pll.R < pll.r_max)) || (uint64_t)outfreq != freq) {                    \
            pll.R = pll.R + 1;                                                  \
            outfreq = setFreq(&freq, &pll);                                     \
        }                                                                       \
        \
        /* Fallback to finding a close enough lo */\
        if(outfreq != freq) {\
            pll = pll_def_lmx2595;\
            outfreq = setFreq(&freq, &pll);\
            \
            while (((pll.N < pll.n_min) && (pll.R < pll.r_max))) {\
                pll.R = pll.R + 1;\
                outfreq = setFreq(&freq, &pll);\
            }\
        }\
                                                                               \
        /* Send Parameters over to the MCU */                                  \
        set_lo_frequency_tx(uart_tx_fd[INT_TX(ch)], (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, INT(ch));  \
                                                                               \
        /* if HB add back in freq before printing value to state tree */       \
        if (band == 2) {                                                       \
            outfreq += HB_STAGE2_MIXER_FREQ;                                   \
        }                                                                      \
        /* Save the frequency that is being set into the property */           \
        snprintf(ret, MAX_PROP_LEN, "%Lf", outfreq);                                          \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_band(const char *data, char *ret) {           \
        /* if the setting is a valid band, send to tx board*/                  \
        int band;                                                              \
        sscanf(data, "%i", &band);                                             \
                                                                            \
        switch(band){                                                          \
            case 2:                                                            \
                /*Ensure 100MHz LMX ref is enabled for highband*/              \
                set_property_bit("time/source/enable_rf_ref", INT_TX(ch));     \
                /* Ensure 1.8GHz IF clock is enabled for highband*/            \
                set_property_bit("time/source/enable_rf_if", INT_TX(ch));      \
                /* clear IQ swap because of possible previous use of low band*/\
                set_property("tx/" STR(ch) "/link/iq_swap", "0");              \
                break;                                                         \
            case 1:                                                            \
                /*Ensure 100MHz LMX ref is enabled for midband*/               \
                set_property_bit("time/source/enable_rf_ref", INT_TX(ch));     \
                /* turn off the 1.8GHz IF to reduce noise*/                    \
                clr_property_bit("time/source/enable_rf_if", INT_TX(ch));      \
                /* clear IQ swap because of possible previous use of low band*/\
                set_property("tx/" STR(ch) "/link/iq_swap", "0");              \
                break;                                                         \
            case 9: /*"Superbaseband" mode available on TX*/                   \
            case 0:                                                            \
                /* turn off the 100MHz LMX ref*/                               \
                clr_property_bit("time/source/enable_rf_ref", INT_TX(ch));     \
                /* turn off the 1.8GHz IF*/                                    \
                clr_property_bit("time/source/enable_rf_if", INT_TX(ch));      \
                /*IQ swap to address layout*/                                  \
                set_property("tx/" STR(ch) "/link/iq_swap", "1");              \
                break;                                                         \
            default:                                                           \
                snprintf(buf, MAX_PROP_LEN, "rf -z\r"); /*mute the board to stop transmitting*/\
                ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
                PRINT(ERROR,"unrecognized band\n");                            \
                return RETURN_ERROR_PARAM;                                     \
        }                                                                      \
        snprintf(buf, MAX_PROP_LEN, "rf -b %i\r", band);                       \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)); \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_gain_val(const char *data, char *ret) {       \
        double gain = 0;\
        /*gain of components used by all bands*/\
        double ab_gain = 0;\
        /*gain of band specific components*/\
        double band_gain = 0;\
        int band;\
        char band_read[3];\
        sscanf(data, "%lf", &gain);\
        get_property("tx/" STR(ch) "/rf/band", band_read,3);\
        sscanf(band_read, "%i", &band);\
        if(gain>MAX_RF_GAIN_TX_AB) {\
            ab_gain = MAX_RF_GAIN_TX_AB;\
        }\
        else if (gain<MIN_RF_GAIN_TX_AB) {\
            ab_gain = MIN_RF_GAIN_TX_AB;\
        } else {\
            ab_gain = gain;\
        }\
        if(band == 2) {\
            band_gain = gain-ab_gain;\
            if(band_gain > MAX_GAIN_TX_HB) {\
                band_gain = MAX_GAIN_TX_HB;\
            } else if(band_gain < MIN_GAIN_TX_HB) {\
                band_gain = MIN_GAIN_TX_HB;\
            }\
        }\
        double atten = (((ab_gain)-MIN_RF_GAIN_TX_AB)/(MAX_RF_GAIN_TX_AB-MIN_RF_GAIN_TX_AB)) * (MIN_RF_ATTEN_TX_AB - MAX_RF_ATTEN_TX_AB) + MAX_RF_ATTEN_TX_AB;\
        char s_atten[25];\
        \
        /*Sets and attenuator used by all bands*/\
        snprintf(s_atten, 25, "%f", atten);\
        set_property("tx/" STR(ch) "/rf/atten", s_atten);\
        get_property("tx/" STR(ch) "/rf/atten", s_atten,3);                   \
        sscanf(s_atten, "%lf", &atten);\
        \
        if(band == 2) {\
            int gain_control_mv = 0;\
            gain_control_mv = \
                (int)((band_gain * (MAX_GAIN_V_TX_HB_GAIN - MIN_GAIN_V_TX_HB_GAIN) / (MAX_GAIN_TX_HB - MIN_GAIN_TX_HB) + MIN_GAIN_V_TX_HB_GAIN) * 1000);\
            /*Command format: debug -p <Voltage in mV>*/\
            snprintf(buf, MAX_PROP_LEN, "debug -p %i\r", gain_control_mv);\
            ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
        } else {\
            /*Sets high band amplifier gain to 0 when adjusting the gain in other bands, to prevent a suprise max gain if the user is switching bands*/\
            snprintf(buf, MAX_PROP_LEN, "debug -p 0\r");\
            ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
            \
        }\
        \
        gain = (((atten)-MIN_RF_ATTEN_TX_AB)/(MAX_RF_ATTEN_TX_AB-MIN_RF_ATTEN_TX_AB)) * (MIN_RF_GAIN_TX_AB - MAX_RF_GAIN_TX_AB) + MAX_RF_GAIN_TX_AB;\
        gain += band_gain;\
        snprintf(ret, 25, "%lf", gain);\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
										\
    static int hdlr_tx_##ch##_rf_atten(const char *data, char *ret) {		\
	    float atten;							\
	    sscanf(data, "%f", &atten);						\
	    if(atten > MAX_RF_ATTEN_TX_AB) atten = MAX_RF_ATTEN_TX_AB;\
        float codef = atten / (float)(RF_ATTEN_STEP_TX);\
        uint16_t codei = roundf(codef);\
	    snprintf(buf, MAX_PROP_LEN, "rf -a %hu\r", codei);\
	    ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));		\
	    atten = codei * RF_ATTEN_STEP_TX;\
	    snprintf(ret, MAX_PROP_LEN, "%f", atten);                                     \
										\
	    return RETURN_SUCCESS;						\
    }										\
                                                                               \
    static int hdlr_tx_##ch##_rf_board_dump(const char *data, char *ret) {     \
        /* send the uart commands and read back the output and write to file   \
         */                                                                    \
                                                                               \
        char dac_dump[MAX_PROP_LEN];\
        char gpiox_dump[MAX_PROP_LEN];\
        \
        /* DAC */                                                              \
        strcpy(buf, "dac -D\r");                               \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(dac_dump, MAX_PROP_LEN, "[Board: tx_%s Chip: DAC] %s\n", STR(ch), uart_ret_buf);\
                                                                               \
        /* GPIOX */                                                            \
        strcpy(buf, "dump -g\r");                               \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(gpiox_dump, MAX_PROP_LEN, "[Board: tx_%s Chip: GPIOX] %s\n", STR(ch), uart_ret_buf);\
        \
        snprintf(ret, MAX_PROP_LEN, "%s%s", dac_dump, gpiox_dump);\
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_board_temp(const char *data, char *ret) {     \
        strcpy(buf, "board -t\r");                              \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_status_rfld(const char *data, char *ret) {       \
        strcpy(buf, "status -l\r");                             \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_status_dacld(const char *data, char *ret) {      \
        strcpy(buf, "status -p\r");                             \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    static int hdlr_tx_##ch##_status_lna(const char *data, char *ret) {        \
        snprintf(buf, 10, "rf -S\r");                                          \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)); \
        snprintf(ret, 50, (char *)uart_ret_buf);                               \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_rf_board_led(const char *data, char *ret) {      \
        snprintf(buf, MAX_PROP_LEN, "board -l %s\r", data);\
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* 8 bits from the dsp gain register are ued for each channel (7:0 for a, 15:8 for b...)*/\
    static int hdlr_tx_##ch##_dsp_gain(const char *data, char *ret) {          \
        uint32_t gain;\
        uint32_t clear_util = 0xff;\
        sscanf(data, "%i", &gain);                                            \
        if(gain > 0xff) gain = 0xff;\
        gain = gain << (8*(INT(ch)%4));\
        clear_util = clear_util << (8*(INT(ch)%4));\
        uint32_t reg_val;\
        read_hps_reg(txg_map[(int)(INT(ch)/4)], &reg_val);                                  \
        /*Clears the bits used in the reg for this channel's dsp gain*/\
        reg_val = reg_val & ~clear_util;\
        /*Sets the bits used in the reg for this channel's dsp gain*/\
        reg_val = reg_val | gain;\
        write_hps_reg(txg_map[(int)(INT(ch)/4)], reg_val);            \
        /* Calculates gain set (should be the same as gain was originally)*/\
        gain = reg_val & clear_util;\
        gain = gain >> (8*INT(ch));\
        snprintf(ret, MAX_PROP_LEN, "%i", gain);\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_dsp_rate(const char *data, char *ret) {          \
        uint32_t reg_val = 0;                                                  \
        double rate;                                                           \
        sscanf(data, "%lf", &rate);                                            \
        /*The sample rate only uses 16 bits*/\
        uint16_t sample_factor;\
        /* Whether to bypass the dsp*/\
        uint32_t bypass;\
        \
        /* Keeps the sample rate within the allowable range*/\
        if(rate < MIN_TX_SAMPLE_RATE) rate = MIN_TX_SAMPLE_RATE;\
        if(rate > TX_BASE_SAMPLE_RATE) rate = TX_BASE_SAMPLE_RATE;\
        \
        /* bypasses dsp when at the full sample rate*/\
        if(rate > ((TX_BASE_SAMPLE_RATE-TX_DSP_SAMPLE_RATE)*(1-RATE_ROUND_BIAS))+TX_DSP_SAMPLE_RATE) {\
            rate = TX_BASE_SAMPLE_RATE;\
            sample_factor = 0;\
            bypass = 1;\
        } else {\
            bypass = 0;\
            sample_factor = get_optimal_sr_factor(&rate, TX_DSP_SAMPLE_RATE);\
        }\
        /*bit 0 of tx_0 is used to determine whether or not to bypass the dsp*/\
        read_hps_reg("tx" STR(ch) "2", &reg_val);\
        reg_val = reg_val & ~1;\
        reg_val = reg_val | bypass;\
        write_hps_reg("tx" STR(ch) "2", reg_val);\
        \
        write_hps_reg("tx" STR(ch) "1", sample_factor);                    \
        \
        snprintf(ret, MAX_PROP_LEN, "%lf", rate);\
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*Interface for setting the dsp nco of every channel. Currently only ch0 is used*/\
    static int hdlr_tx_##ch##_dsp_fpga_nco(const char *data, char *ret) {    \
        double freq;                                                           \
        uint32_t old_val = 0;                                                  \
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
        /*the magnitude and direction of the nco are stored in seperate registers*/\
        if(freq < 0) freq = -freq;\
        if(freq > MAX_DSP_NCO) freq = MAX_DSP_NCO;\
                                                                               \
        /* write NCO adj */                                                    \
        uint32_t nco_steps = (uint32_t)round(freq * TX_DSP_NCO_CONST);            \
        write_hps_reg("tx" STR(ch) "10", nco_steps);                           \
        if (direction > 0) {                                                   \
            snprintf(ret, MAX_PROP_LEN, "-%lf", (double)nco_steps / TX_DSP_NCO_CONST);           \
        } else {                                                               \
            snprintf(ret, MAX_PROP_LEN, "%lf", (double)nco_steps / TX_DSP_NCO_CONST);            \
        }                                                                      \
                                                                               \
        /* Likely due to an FPGA ordering issue, enabling iq swap switches the NCO polarity */\
        /* TODO fix the iq swap/NCO polarity in the FPGA and remove this workaround */\
        char iq_swap[2];\
        get_property("tx/" STR(ch) "/link/iq_swap", iq_swap, 2);\
        if(iq_swap[0] != '0') {\
            direction = !direction;\
        } else {\
        }\
        /* write direction */                                                  \
        read_hps_reg(tx_reg4_map[INT(ch)], &old_val);                             \
        write_hps_reg(tx_reg4_map[INT(ch)],                                       \
                      (old_val & ~(0x1 << 13)) | (direction << 13));             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
\
    /*Set the fpga, main dac, and dac channel ncos*/\
    static int hdlr_tx_##ch##_dsp_all_nco(const char *data, char *ret) {    \
        double target_nco = 0;\
        double actual_nco = 0;\
        double last_nco = 0;\
        sscanf(data, "%lf", &target_nco);\
        char nco_s[50];\
        \
        /*Sets the nco in the dsp*/\
        snprintf(nco_s, 50, "%lf", target_nco - actual_nco);\
        set_property("tx/" STR(ch) "/dsp/fpga_nco", nco_s);\
        get_property("tx/" STR(ch) "/dsp/fpga_nco", nco_s, 50);       \
        sscanf(nco_s, "%lf", &last_nco);\
        actual_nco = actual_nco + last_nco;\
        \
        /*Sets the nco in the channelizer part of the dac (see page 2 of AD9176 for clarification on channels)*/\
        snprintf(nco_s, 50, "%lf", target_nco - actual_nco);\
        set_property("tx/" STR(ch) "/rf/dac/nco/chfreq", nco_s);\
        get_property("tx/" STR(ch) "/rf/dac/nco/chfreq", nco_s, 50);\
        sscanf(nco_s, "%lf", &last_nco);\
        actual_nco = actual_nco + last_nco;\
        \
        snprintf(nco_s, 50, "%lf", target_nco - actual_nco);\
        set_property("tx/" STR(ch) "/rf/dac/nco/dacfreq", nco_s);\
        get_property("tx/" STR(ch) "/rf/dac/nco/dacfreq", nco_s, 50);\
        sscanf(nco_s, "%lf", &last_nco);\
        actual_nco = actual_nco + last_nco;\
        \
        snprintf(ret, MAX_PROP_LEN, "%lf", actual_nco);\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*resets dsp (including buffer)*/\
    static int hdlr_tx_##ch##_dsp_rstreq(const char *data, char *ret) {        \
        uint32_t old_val = 0;                                                      \
        /* Puts dsp in reset to clear uflow/oflow count, then takes it out of reset*/\
        /* Delays present because it must be in reset for an amount of time*/\
        /* Resets twice because resets don't work properly and need to be cleared*/\
        read_hps_reg(tx_reg4_map[INT(ch)], &old_val);                           \
        write_hps_reg(tx_reg4_map[INT(ch)], old_val | 0x2);                     \
        usleep(10000);\
        write_hps_reg(tx_reg4_map[INT(ch)], old_val &(~0x2));                   \
        write_hps_reg(tx_reg4_map[INT(ch)], old_val | 0x2);                     \
        usleep(10000);\
        write_hps_reg(tx_reg4_map[INT(ch)], old_val &(~0x2));                   \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    /* Selects the size of samples */\
    /* Takes the size of half the complex pair (i.e. for sc16 set to 16 */\
    static int hdlr_tx_##ch##_dsp_sample_bandwidth(const char *data, char *ret) {        \
        int wire_format = 0;\
        sscanf(data, "%i", &wire_format);\
        \
        if(MAX_SAMPLE_RATE == 1000) {\
            /* Wire formate cannot be set on 1G*/\
            if(wire_format != DEAULT_OTW_TX) {\
                PRINT(ERROR, "Unsupported wire format selected: sc%i. Defaulting to sc %i\n", wire_format, DEAULT_OTW_TX);\
                wire_format = DEAULT_OTW_TX;\
            }\
        } else if(MAX_SAMPLE_RATE == 3000) {\
            if(wire_format == 12) {\
                write_hps_reg_mask(tx_reg4_map[INT(ch)], 0x40000, 0x40000);\
            } else if(wire_format == 16) {\
                write_hps_reg_mask(tx_reg4_map[INT(ch)], 0x00000, 0x40000);\
            } else {\
                PRINT(ERROR, "Unsupported wire format selected: sc%i. Defaulting to sc %i\n", wire_format, 16);\
                wire_format = 16;\
                write_hps_reg_mask(tx_reg4_map[INT(ch)], 0x00000, 0x40000);\
            }\
        } else {\
            PRINT(ERROR, "function not implemented for variants with maximum sample rate: %i\n", MAX_SAMPLE_RATE);\
        }\
        snprintf(ret, MAX_PROP_LEN, "%i", wire_format);\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    static int hdlr_tx_##ch##_jesd_delay_iq(const char *data, char *ret) {      \
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
        int32_t ch_select = 1 << (INT(ch) + 16);\
        int32_t reg_val = (q_delay << 6) | i_delay;\
         write_hps_reg("res_rw12", ch_select);\
         write_hps_reg("res_rw13", reg_val | 0x1000);\
         write_hps_reg("res_rw13", reg_val);\
        snprintf(ret, MAX_PROP_LEN, "%i %i\n", i_delay, q_delay);\
        return RETURN_SUCCESS;\
    }                                                                          \
    \
    /*0 for big endian packets, anything else for little endian*/\
    static int hdlr_tx_##ch##_endian_swap(const char *data, char *ret) {      \
        int endian = 0;\
        sscanf(data, "%i", &endian);\
        int bit_val;\
        if(endian && LITTLE_ENDIAN_SUPPORTED) {\
            bit_val = 0x20000;\
        } else {\
            bit_val = 0;\
        }\
        write_hps_reg_mask(tx_reg4_map[INT(ch)], bit_val, 0x20000);\
        if(endian && LITTLE_ENDIAN_SUPPORTED) {\
            snprintf(ret, MAX_PROP_LEN, "1");\
        } else {\
            snprintf(ret, MAX_PROP_LEN, "0");\
        }\
        return RETURN_SUCCESS;\
    }                                                                          \
    \
    static int hdlr_tx_##ch##_about_id(const char *data, char *ret) {          \
        /* don't need to do anything, save the ID in the file system */        \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_vita_en(const char *data, char *ret) {      \
        uint32_t old_val = 0;                                                      \
        read_hps_reg(tx_reg4_map[INT(ch)], &old_val);                          \
        /*Bit 14 enables*/\
        if (strcmp(data, "1") == 0) {                                          \
            write_hps_reg(tx_reg4_map[INT(ch)], old_val | (1 << 14));\
            /*Resets dsp to get rid of false underflows of dsp not already in reset, since when vita is low the system is always trying to stream*/\
            /*And detects underflows since it is not being sent any data*/\
            if(old_val & 0x2) {\
                set_property("tx/" STR(ch) "/dsp/rstreq", "1");\
            }\
        } else                                                                   \
            write_hps_reg(tx_reg4_map[INT(ch)], old_val & ~(1 << 14));         \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_iface(const char *data, char *ret) {        \
        /* TODO: FW support for streaming to management port required */       \
        snprintf(ret, MAX_PROP_LEN, tx_sfp_map[INT(ch)]);                                  \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    static int hdlr_tx_##ch##_link_port(const char *data, char *ret) {      \
        uint32_t port;                                                         \
        sscanf(data, "%" SCNd32 "", &port);                                    \
        write_hps_reg(device_side_port_map[get_tx_dst_port_map_loc(INT(ch))], port);\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_link_iq_swap(const char *data, char *ret) {      \
        int swap;                                                            \
        sscanf(data, "%i", &swap);                                           \
        uint32_t old_val = 0;                                                      \
        read_hps_reg(tx_reg4_map[INT(ch)], &old_val);                          \
        if ( swap == 1)                                            \
            write_hps_reg(tx_reg4_map[INT(ch)], old_val | (1 << 12));          \
        else                                                                   \
            write_hps_reg(tx_reg4_map[INT(ch)], old_val & ~(1 << 12));         \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*Interface for checking the fifo of every channel. Currently only ch0 is used*/\
    static int hdlr_tx_##ch##_qa_fifo_lvl(const char *data, char *ret) {    \
        uint32_t lvl;                                                          \
        char lvl_reg[20];\
        snprintf(lvl_reg, 20, "res_ro%i", INT(ch)+4);\
        read_hps_reg(lvl_reg, &lvl);                                         \
        lvl &= 0xffff;                                                         \
        snprintf(ret, MAX_PROP_LEN, "%u", lvl);                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*Interface for checking the oflow of every channel. Currently only ch0 is used*/\
    static int hdlr_tx_##ch##_qa_oflow(const char *data, char *ret) {    \
        /* Registers are 32 bits, the 64 bit count is split across 2 registers*/\
        uint32_t lsb_count;                                                        \
        uint32_t msb_count;                                                        \
        read_hps_reg(tx_oflow_map_lsb[INT(ch)], &lsb_count);                   \
        read_hps_reg(tx_oflow_map_msb[INT(ch)], &msb_count);                   \
        uint64_t count = (((uint64_t) msb_count) << 32) | lsb_count;\
        snprintf(ret, MAX_PROP_LEN, "%lu", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*Interface for checking the oflow of every channel. Currently only ch0 is used*/\
    static int hdlr_tx_##ch##_qa_uflow(const char *data, char *ret) {    \
        /* Registers are 32 bits, the 64 bit count is split across 2 registers*/\
        uint32_t lsb_count;                                                        \
        uint32_t msb_count;                                                        \
        read_hps_reg(tx_uflow_map_lsb[INT(ch)], &lsb_count);                   \
        read_hps_reg(tx_uflow_map_msb[INT(ch)], &msb_count);                   \
        uint64_t count = (((uint64_t) msb_count) << 32) | lsb_count;\
        snprintf(ret, MAX_PROP_LEN, "%lu", count);                                             \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*Turns the board on or off, and performs none of the other steps in the turn on/off process*/\
    /*pwr must be used to complete the power on process*/\
    static int hdlr_tx_##ch##_pwr_board(const char *data, char *ret) {               \
        if(tx_power[INT(ch)] & PWR_NO_BOARD) {\
            /*Technically this should be an error, but it would trigger everytime an unused slot does anything, clogging up error logs*/\
            return RETURN_SUCCESS;\
        }\
        uint8_t power;                                                         \
        sscanf(data, "%" SCNd8 "", &power);                                    \
                                                                               \
        char pwr_cmd [40];                                                 \
        if(power>=PWR_ON) {\
            set_property("time/sync/sysref_mode", "continuous");\
            snprintf(pwr_cmd, 40, "rfe_control %d on", INT_TX(ch));                    \
            system(pwr_cmd);                                                   \
            tx_power[INT(ch)] = PWR_HALF_ON;\
        } else {\
            /* This function is meant to block until after power is either on or off. However a hardware issue can cause unpopulated boards to never be detected as off*/\
            snprintf(pwr_cmd, 40, "rfe_control %d off %i", INT_TX(ch), PWR_TIMEOUT);\
            system(pwr_cmd);                                                   \
            tx_power[INT(ch)] = PWR_OFF;\
        }\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    /*Turns the board on or off, and performs none of the other steps in the turn on/off process*/\
    /*pwr must be used to complete the power on process*/\
    /*returns the pid of the powr on process*/\
    static int hdlr_tx_##ch##_async_pwr_board(const char *data, char *ret) {               \
        uint8_t power;                                                         \
        sscanf(data, "%" SCNd8 "", &power);                                    \
        set_property("time/sync/sysref_mode", "continuous");\
        \
        char pwr_cmd[30];\
        char str_state[25];\
        if(power>=PWR_ON) {\
            strcpy(str_state, "on");\
        } else {\
            strcpy(str_state, "off");\
        }\
        snprintf(pwr_cmd, 30, "/usr/bin/rfe_control %i %s n", INT_TX(ch), str_state);\
        system(pwr_cmd);\
        time(&tx_async_start_time[INT(ch)]);\
        \
        return RETURN_SUCCESS;\
    }                                                                          \
    \
    /*waits for async_pwr_board to finished*/\
    static int hdlr_tx_##ch##_wait_async_pwr(const char *data, char *ret) {               \
        \
        /* Polls to check if a board has turned on */\
        do {\
            snprintf(buf, sizeof(buf), "/usr/bin/rfe_control %i check", INT_TX(ch));\
            FILE *check_result;\
            check_result = popen(buf, "r");\
            size_t bytes_read = fread(buf, 1, sizeof(buf), check_result);\
            if(bytes_read == 0) {\
                tx_power[INT(ch)] = PWR_NO_BOARD;\
                PRINT(ERROR,"Error in script controlling power for tx board %i, the slot will not be used\n", INT(ch));\
                return errno;\
            } else if(strstr(buf, "on")) {\
                tx_power[INT(ch)] = PWR_HALF_ON;\
                PRINT(INFO,"Tx board %i powered on within %lus\n", INT(ch), time(NULL) - tx_async_start_time[INT(ch)]);\
                return RETURN_SUCCESS;\
            }\
        \
        } while (time(NULL) < tx_async_start_time[INT(ch)] + PWR_TIMEOUT);\
        \
        tx_power[INT(ch)] = PWR_NO_BOARD;\
        PRINT(ERROR,"Timeout while powering tx board %i, the slot will not be used\n", INT(ch));\
        \
        return RETURN_SUCCESS;\
    }                                                                          \
    \
    static int hdlr_tx_##ch##_jesd_reset(const char *data, char *ret) {       \
        if(tx_power[INT(ch)] & PWR_NO_BOARD) {\
            /*Technically this should be an error, but it would trigger everytime an unused slot does anything, clogging up error logs*/\
            return RETURN_SUCCESS;\
        }\
        int reset = 0;                                                            \
        sscanf(data, "%i", &reset);                                           \
        if (reset){\
            set_property("tx/" STR(ch) "/jesd/unmask", "1");\
            /* Gives time for sysref unmask to update, can probably be shorted */\
            usleep(jesd_mask_delay);\
            write_hps_reg_mask("res_rw7", 1 << (INDIVIDUAL_RESET_BIT_OFFSET_TX + INT(ch)), 1 << (INDIVIDUAL_RESET_BIT_OFFSET_TX + INT(ch)));\
            write_hps_reg_mask("res_rw7", 0, 1 << (INDIVIDUAL_RESET_BIT_OFFSET_TX + INT(ch)));\
            /* Issues sysref pulse if not in continuous */\
            if(current_sysref_mode != continuous) {\
                set_property("time/sync/lmk_sync_tgl_jesd", "1");\
            };\
            usleep(jesd_reset_delay);\
            set_property("tx/" STR(ch) "/jesd/unmask", "0");\
            return RETURN_SUCCESS;\
        }\
        else {\
            return RETURN_SUCCESS;\
        }\
    }\
    \
    static int hdlr_tx_##ch##_jesd_pll_locked(const char *data, char *ret) {       \
        uint32_t lock_status = 0;\
        write_hps_reg("res_rw8", LR_NUM_TX(ch));\
        read_hps_reg("res_ro20", &lock_status);\
        lock_status = lock_status & 0x2;\
        if(lock_status) {\
            snprintf(ret, 10, "locked");\
        } else {\
            snprintf(ret, 10, "unlocked");\
        }\
        \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    /* Negative: pretend there's no board there and turn the board off */\
    /* 0: disable the board but to not turn it off (use pwr_board if you actually want to turn it off) */\
    /* Positive: enable the board/finish power on process */\
    static int hdlr_tx_##ch##_pwr(const char *data, char *ret) {         \
        uint32_t old_val = 0;                                                  \
        int num_lna_attempts = 0;                                              \
        int8_t power = 0;                                                     \
        sscanf(data, "%" SCNd8 "", &power);                                    \
        \
        /* Pretend the slot is empty, then act like power off was requested*/\
        if(power < 0) {\
            tx_power[INT(ch)] = PWR_NO_BOARD;\
            write_hps_reg_mask(tx_reg4_map[INT(ch)], 0x2, 0x2);\
            snprintf(buf, 40, "rfe_control %d off n", INT_TX(ch));\
            system(buf);\
            power = 0;\
        }\
                                                                               \
        /* check if power is already enabled */                                \
        if (power >= PWR_ON && (tx_power[INT(ch)] & PWR_ON))                   \
            return RETURN_SUCCESS;                                             \
                                                                               \
        /* power on */                                                         \
        if (power >= PWR_ON) {                                                 \
            \
            if(tx_power[INT(ch)] == (PWR_OFF & (~PWR_NO_BOARD))) {\
                set_property("tx/" STR(ch) "/board/pwr_board", "1");\
            }\
            if(!(tx_power[INT(ch)] & PWR_NO_BOARD)) {\
                tx_power[INT(ch)] = PWR_ON;\
            } else {\
                tx_power[INT(ch)] = PWR_ON | PWR_NO_BOARD;\
            }\
                                                                               \
            /* reset JESD */                                              \
            if(jesd_enabled && !(tx_power[INT(ch)] & PWR_NO_BOARD)) {\
                if(property_good("tx/" STR(ch) "/jesd/status") != 1) {\
                    set_property("tx/" STR(ch) "/jesd/reset", "1");\
                }\
            }\
            /* Check if low noise aplifier is in a good condition */           \
            while(1) {                                                         \
                hdlr_tx_##ch##_status_lna("1", buf);                           \
                if(strncmp(buf, "LNA_RDY: 1", 10) == 0) {                      \
                    PRINT(INFO, "LNA is good\n");                              \
                    break;                                                     \
                } else if(num_lna_attempts >= 10){                             \
                    PRINT(ERROR, "Failed to start lna after 10 attempts\n");   \
                    break;                                                     \
                } else {                                                       \
                    PRINT(INFO, "The lna is in a bad state, attempting to restart\n");\
                    num_lna_attempts ++;                                       \
                    snprintf(buf, 20, "rf -L r\r");                            \
                    ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
                }\
            }\
                                                                               \
            /* Enables channel */                    \
            read_hps_reg(tx_reg4_map[INT(ch)], &old_val);                           \
            write_hps_reg(tx_reg4_map[INT(ch)], old_val | 0x100);                   \
            \
            /* Puts dsp in reset to clear uflow/oflow count, then takes it out of reset*/\
            /* Delays present because it must be in reset for an amount of time*/\
            /* Resets twice because resets don't work properly and need to be cleared*/\
            read_hps_reg(tx_reg4_map[INT(ch)], &old_val);                           \
            write_hps_reg(tx_reg4_map[INT(ch)], old_val | 0x2);                     \
            usleep(10000);\
            write_hps_reg(tx_reg4_map[INT(ch)], old_val &(~0x2));                   \
            write_hps_reg(tx_reg4_map[INT(ch)], old_val | 0x2);                     \
            usleep(10000);\
            write_hps_reg(tx_reg4_map[INT(ch)], old_val &(~0x2));                   \
                                                                               \
            /* Turns the power indicator light on */\
            /* The indicator light turns on when the board boots, and gets turned off without the board being turned off as a workaround for JESD links not re-establishing when rebooting boards*/\
            snprintf(buf, 20, "board -w 1\r");                                 \
            ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
            /* power off */                                                    \
        } else {                                                               \
            \
            if(property_good("tx/" STR(ch) "/jesd/status")) {\
                /* mute the channel */                                             \
                strcpy(buf, "rf -z\r");                          \
                ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));            \
                snprintf(buf, 20, "board -w 0\r");\
                ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
                if(!(tx_power[INT(ch)] & PWR_NO_BOARD)) {\
                    tx_power[INT(ch)] = PWR_HALF_ON;\
                } else {\
                    tx_power[INT(ch)] = PWR_OFF | PWR_NO_BOARD;\
                }\
            } else {\
                /* kill the channel */                                             \
                strcpy(buf, "board -k\r");                          \
                ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));            \
                set_property("tx/" STR(ch) "/board/pwr_board", "0");\
                if(!(tx_power[INT(ch)] & PWR_NO_BOARD)) {\
                    tx_power[INT(ch)] = PWR_OFF;\
                } else {\
                    tx_power[INT(ch)] = PWR_OFF | PWR_NO_BOARD;\
                }\
            }\
                                                                               \
            /* turn off the 100MHz LMX ref*/                                   \
            clr_property_bit("time/source/enable_rf_ref", INT_TX(ch));         \
            /* turn off the 1.8GHz IF*/                                        \
            clr_property_bit("time/source/enable_rf_if", INT_TX(ch));          \
                                                                               \
            /* disable DSP cores */                                            \
            read_hps_reg(tx_reg4_map[INT(ch)], &old_val);                          \
            write_hps_reg(tx_reg4_map[INT(ch)], old_val | 0x2);                    \
                                                                               \
            /* disable channel */                                              \
            read_hps_reg(tx_reg4_map[INT(ch)], &old_val);                          \
            write_hps_reg(tx_reg4_map[INT(ch)], old_val &(~0x100));                \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_reboot(const char *data, char *ret) {            \
        if(tx_power[INT(ch)] == PWR_NO_BOARD) {\
            /*Technically this should be an error, but it would trigger everytime an unused slot does anything, clogging up error logs*/\
            return RETURN_SUCCESS;\
        }\
        int reboot;                                                            \
        sscanf(data, "%i", &reboot);                                           \
                                                                               \
        if (reboot == 1) {                                                     \
            /*This will cause an error if this runs during initialization*/\
            /*This will wait until the board is done booting*/\
            set_property("tx/" STR(ch) "/pwr", "0");                           \
            set_property("tx/" STR(ch) "/pwr", "1");                           \
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_jesd_status(const char *data, char *ret) {       \
        if(tx_power[INT(ch)] == PWR_ON || tx_power[INT(ch)] == PWR_HALF_ON) {\
            strcpy(buf, "status -g\r");                                            \
            ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)); \
            snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
        } else if (tx_power[INT(ch)] == PWR_NO_BOARD) {\
            snprintf(ret, 50, "No board detected in slot");\
        } else {\
            snprintf(ret, 50, "Board off");\
        }\
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_serial(const char *data, char *ret) {      \
        strcpy(buf, "status -s\r");                                            \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_mcudevid(const char *data, char *ret) {    \
        strcpy(buf, "status -d\r");                                            \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_mcurev(const char *data, char *ret) {      \
        strcpy(buf, "status -v\r");                                            \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_mcufuses(const char *data, char *ret) {    \
        strcpy(buf, "status -f\r");                                            \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_tx_##ch##_about_fw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -v\r");                                             \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }\
    \
    static int hdlr_tx_##ch##_about_hw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -h\r");                                             \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)); \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }\
    \
    /*TODO: make sw_ver only return the mcu version*/\
    static int hdlr_tx_##ch##_about_sw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -v\r");                                             \
        ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }
TX_CHANNELS
#undef X

/* -------------------------------------------------------------------------- */
/* --------------------------------- RX ------------------------------------- */
/* -------------------------------------------------------------------------- */

#define X(ch)                                                               \
    static int hdlr_rx_##ch##_rf_freq_val(const char *data, char *ret) {        \
        if(rx_power[INT(ch)] & PWR_NO_BOARD) {\
            /*Technically this should be an error, but it would trigger everytime an unused slot does anything, clogging up error logs*/\
            return RETURN_SUCCESS;\
        }\
        uint64_t freq = 0;                                                      \
        sscanf(data, "%" SCNd64 "", &freq);                                     \
        char fullpath[PROP_PATH_LEN] = "rx/" STR(ch) "/rf/freq/band";     \
        int band;                                                               \
        char band_read[3];                                                      \
                                                                                \
        /* if freq = 0, mute PLL */                                             \
        if (freq == 0) {                                                        \
            strcpy(buf, "lmx -k\r");                                            \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));          \
            snprintf(ret, MAX_PROP_LEN, "%i", 0);                                             \
            return RETURN_SUCCESS;                                              \
        }                                                                       \
        \
        freq = lround((freq / (double)LO_STEPSIZE)) * LO_STEPSIZE;\
                                                                                \
        /* if freq out of bounds, mute lmx*/                                    \
        if ((freq < LMX2595_RFOUT_MIN_HZ) || (freq > LMX2595_RFOUT_MAX_HZ)) {   \
            strcpy(buf, "lmx -k\r");                                            \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));          \
            PRINT(ERROR,"LMX Freq Invalid \n");                                 \
            snprintf(ret, MAX_PROP_LEN, "%i", 0);                                             \
            return RETURN_ERROR;                                                \
        }                                                                       \
                                                                                \
        /* check band: if HB, subtract freq to account for cascaded mixers*/    \
        get_property(fullpath, band_read, 3);                                   \
        sscanf(band_read, "%i", &band);                                         \
        if (band == 2) {                                                        \
            freq -= HB_STAGE2_MIXER_FREQ;                                       \
        }                                                                       \
                                                                                \
        /* run the pll calc algorithm */                                        \
        pllparam_t pll = pll_def_lmx2595;                                       \
        long double outfreq = 0;                                                \
        /* Attempt to find an lo setting for the desired frequency using default R divider*/\
        /* NOTE: setFreq finds the pll settings and sores then in the provided struct */\
        /* It does not actually set the frequency */\
        outfreq = setFreq(&freq, &pll);                                         \
                                                                                \
        /* Attempt to find an lo setting for desired frequency using other R dividers*/\
        while (((pll.N < pll.n_min) && (pll.R < pll.r_max)) || (uint64_t)outfreq != freq) {                    \
            pll.R = pll.R + 1;                                                  \
            outfreq = setFreq(&freq, &pll);                                     \
        }                                                                       \
        \
        /* Fallback to finding a close enough lo */\
        if(outfreq != freq) {\
            pll = pll_def_lmx2595;\
            outfreq = setFreq(&freq, &pll);\
            \
            while (((pll.N < pll.n_min) && (pll.R < pll.r_max))) {\
                pll.R = pll.R + 1;\
                outfreq = setFreq(&freq, &pll);\
            }\
        }\
                                                                                \
        /* Send Parameters over to the MCU (the part that actually sets the lo)*/                                   \
        if(set_lo_frequency_rx(uart_rx_fd[INT_RX(ch)], (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, INT(ch))) {\
            /* if HB add back in freq before printing value to state tree */        \
            if (band == 2) {                                                        \
                outfreq += HB_STAGE2_MIXER_FREQ;                                    \
            }                                                                       \
            /* Save the frequency that is being set into the property */            \
            snprintf(ret, MAX_PROP_LEN, "%Lf", outfreq);                            \
        } else {\
            PRINT(ERROR, "PLL lock failed when attempting to set freq to %lf\n", outfreq);\
            snprintf(ret, MAX_PROP_LEN, "0");                                  \
        }\
                                                                                \
                                                                                \
        return RETURN_SUCCESS;                                                  \
    }                                                                           \
                                                                                                                                                         \
                                                                               \
    static int hdlr_rx_##ch##_rf_freq_lna(const char *data, char *ret) {       \
        snprintf(buf, MAX_PROP_LEN, "rf -l %s\r", data);\
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_freq_band(const char *data, char *ret) {      \
        int16_t band = 0;                                                          \
        sscanf(data, "%hi", &band);                                             \
                                                                               \
        switch(band){                                                          \
            case 2:                                                            \
                /* turn on the 100MHz LMX ref*/                                \
                set_property_bit("time/source/enable_rf_ref", INT_RX(ch));     \
                /* Ensure 1.8GHz IF clock is enabled for highband*/            \
                set_property_bit("time/source/enable_rf_if", INT_RX(ch));      \
                /* clear IQ swap because of possible previous use of low band*/\
                set_property("rx/" STR(ch) "/link/iq_swap", "0");              \
                break;                                                         \
            case 1:                                                            \
                /* need to set the 100MHz LMX ref*/                            \
                set_property_bit("time/source/enable_rf_ref", INT_RX(ch));     \
                /* turn off the 1.8GHz IF*/                                    \
                clr_property_bit("time/source/enable_rf_if", INT_RX(ch));      \
                /* clear IQ swap because of possible previous use of low band*/\
                set_property("rx/" STR(ch) "/link/iq_swap", "0");              \
                break;                                                         \
            case 0:                                                            \
                /* turn off the 100MHz LMX ref*/                               \
                clr_property_bit("time/source/enable_rf_ref", INT_RX(ch));     \
                /* turn off the 1.8GHz IF*/                                    \
                clr_property_bit("time/source/enable_rf_if", INT_RX(ch));      \
                /* IQ swap to address layout*/                                 \
                set_property("rx/" STR(ch) "/link/iq_swap", "1");              \
                break;                                                         \
            default:                                                           \
                PRINT(ERROR,"unrecognized band\n");                            \
                return RETURN_ERROR_PARAM;                                     \
        }                                                                      \
        snprintf(buf, MAX_PROP_LEN, "rf -b %hhx\r", (uint8_t) band);\
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*sets the gain on the variable amplifier*/\
    static int hdlr_rx_##ch##_rf_gain_ampl(const char *data, char *ret) {      \
        uint8_t band = 0;                                                      \
        int32_t gain = 0;\
        int32_t vga_gain = 0;                                               \
        int32_t atten;                                                      \
        sscanf(data, "%i", &gain);                                             \
        \
        char band_read[3];                                                     \
        get_property("rx/" STR(ch) "/rf/freq/band",&band_read[0],3);                 \
        sscanf(band_read, "%hhu", &band);                                            \
                                                                               \
        /*Sets low band variable amplifer*/\
        if(band == 0) {\
            if (gain > LMH6401_MAX_GAIN) {                                     \
                gain = LMH6401_MAX_GAIN;                                       \
            } else if (gain < LMH6401_MIN_GAIN) {                              \
                gain = LMH6401_MIN_GAIN;                                       \
            }                                                                  \
            atten = LMH6401_MAX_GAIN - gain;                                   \
            /*Variable amplifer takes attenuation value instead of a gain*/ \
            PRINT(INFO, "Setting variable amplifier to: %i\n", gain);\
            snprintf(buf, MAX_PROP_LEN, "vga -a %i\r", atten);\
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));         \
            snprintf(ret, MAX_PROP_LEN, "%i", gain);\
        /*Sets mid/high band variable amplifer*/\
        } else if(band == 1 || band == 2) {\
            if(RTM_VER==3) {\
                /*RTM3 does not use one of the amplifiers in high and mid band*/\
                if(gain > LTC5586_MAX_GAIN - LTC5586_MIN_GAIN) gain = LTC5586_MAX_GAIN - LTC5586_MIN_GAIN;\
                else if (gain < 0) gain = 0;\
                \
                PRINT(INFO, "Setting mixer amplifier to: %i\n", gain);\
                snprintf(buf, MAX_PROP_LEN, "rf -g %i\r", gain + LTC5586_MIN_GAIN);\
                ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));         \
                snprintf(ret, MAX_PROP_LEN, "%i", gain);\
            } else {\
                if(gain > LTC5586_MAX_GAIN - LTC5586_MIN_GAIN) {                \
                    vga_gain = gain - LTC5586_MAX_GAIN + LTC5586_MIN_GAIN;      \
                    gain = LTC5586_MAX_GAIN - LTC5586_MIN_GAIN;                 \
                }                                                               \
                else if (gain < 0) {                                            \
                    vga_gain = gain;                                            \
                    gain = 0;                                                   \
                }                                                               \
                                                                                \
                if (vga_gain > LMH6401_MAX_GAIN) {                              \
                    vga_gain = LMH6401_MAX_GAIN;                                \
                } else if (vga_gain < LMH6401_MIN_GAIN) {                       \
                    vga_gain = LMH6401_MIN_GAIN;                                \
                }                                                               \
                atten = LMH6401_MAX_GAIN - vga_gain;                            \
                /*Variable amplifer takes attenuation value instead of a gain*/ \
                PRINT(INFO, "Setting variable amplifier to: %i\n", vga_gain);\
                snprintf(buf, MAX_PROP_LEN, "vga -a %i\r", atten);                             \
                ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
                \
                PRINT(INFO, "Setting mixer amplifier to: %i\n", gain);\
                snprintf(buf, MAX_PROP_LEN, "rf -g %i\r", gain + LTC5586_MIN_GAIN);\
                ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));         \
                snprintf(ret, MAX_PROP_LEN, "%i", gain+vga_gain);                              \
            }\
        } else {\
            PRINT(ERROR, "Invalid band (%hhu) detected when setting gain\n", band);\
            return RETURN_ERROR;\
        }\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* Sets the digital gain amplifier*/\
    /* NOTE: do not set when setting other gains, brings down JESD */\
    static int hdlr_rx_##ch##_rf_gain_adc_digital(const char *data, char *ret) {      \
        double requested_gain = 0;\
        sscanf(data, "%lf", &requested_gain);                                             \
        if(requested_gain < 0) {\
            snprintf(ret, MAX_PROP_LEN, "-1\n");\
            return RETURN_SUCCESS;\
        } else if(rx_stream[INT(ch)] == STREAM_ON) {\
            PRINT(ERROR, "RX is currently streaming. Setting the ADC digital gain will disrupt that. Skipping setting the ADC gain\n");\
            /* Get existing gain without setting it */\
            snprintf(buf, MAX_PROP_LEN, "adc -D\r");\
        } else {\
            /* The unit the function takes is dB*1000. i.e. 120000 = 12dB */\
            int32_t requested_gain_int = (int32_t) (requested_gain * 1000);\
            snprintf(buf, MAX_PROP_LEN, "adc -d %i\r", requested_gain_int);\
        }\
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
        int32_t actual_gain_int_a = 0;\
        int32_t actual_gain_int_b = 0;\
        sscanf((char *)uart_ret_buf, "Digital gain (dB*1000): CHA: %i, CHB: %i\n", &actual_gain_int_a, &actual_gain_int_b);\
        double actual_gain = (double)actual_gain_int_a/1000;\
        \
        /* Warns the user if there is a mismatch between ADC channel A or B (should be impossible when going through our API)*/\
        if(actual_gain_int_a != actual_gain_int_b) {\
            double actual_gain_b = (double)actual_gain_int_b/1000;\
            PRINT(ERROR, "Mistmatch between ADC gains. A: %lf, B: %lf\n", actual_gain, actual_gain_b);\
        }\
        \
        snprintf(ret, MAX_PROP_LEN, "%lf\n", actual_gain);\
        if(rx_stream[INT(ch)] != STREAM_ON) {\
            /* Resets JESD if the ADC digital gain was set */\
            set_property("fpga/jesd/jesd_reset_master", "1");\
        }\
        \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*sets variable amplifiers, variable attentuators, bypassable amplifiers to achieve desired gain*/\
    /*Note: this sets it bassed on the current band, any time the band is changed, this must be updated*/\
    static int hdlr_rx_##ch##_rf_gain_val(const char *data, char *ret) {       \
        const char fullpath[PROP_PATH_LEN] = "rx/" STR(ch) "/rf/freq/band";    \
        int target_gain = 0;                                                              \
        int current_gain = 0;                                                          \
        int atten = 0;                                                             \
        int band = 0;                                                              \
        char band_read[3];                                                     \
                                                                               \
        sscanf(data, "%i", &target_gain);                                             \
        \
        get_property(&fullpath[0],&band_read[0],3);                                  \
        sscanf(band_read, "%i", &band);                                        \
                                                                               \
        if (band == 0) {                                                       \
            char s_gain[50];\
            snprintf(s_gain, 50, "%i", target_gain);\
            set_property("rx/" STR(ch) "/rf/gain/ampl",s_gain);                  \
            get_property("rx/" STR(ch) "/rf/gain/ampl", s_gain, 50);                 \
            sscanf(s_gain, "%i", &current_gain);                                             \
        } else if (band == 1 || band == 2) {                                                \
            /* Step 1: assume variable attentuators are at their minimum, note a current_gain of 0 is miminum gain maximum attenuation*/\
            /* Step 2: attempt to set the variable amplifiers to the requested gain*/\
            /* Step 3: if the variable amplifiers weren't enough, set the lna*/\
            /* Step 3: if the lna + variable amplifiers caused overshoot, set the amplifier to be lower*/\
            current_gain += MID_HIGH_MAX_ATTEN;\
            \
            /*Process: sets variable amplifiers, if the amplifiers weren't enough use the lna, then set the variable attenuator to handle overshoot from the lna*/\
            char s_gain[50];\
            snprintf(s_gain, 50, "%i", target_gain - current_gain);\
            set_property("rx/" STR(ch) "/rf/gain/ampl", s_gain);\
            get_property("rx/" STR(ch) "/rf/gain/ampl", s_gain, 50);\
            int ampl_gain = 0;\
            sscanf(s_gain, "%i", &ampl_gain);\
            current_gain+=ampl_gain;\
            if(current_gain < target_gain) {\
                /*1 means bypass the lna*/\
                set_property("rx/" STR(ch) "/rf/freq/lna", "0");\
                if(band == 1) {\
                    current_gain += AM1081_GAIN;\
                } else if(band == 2) {\
                    current_gain += AM1075_GAIN;\
                } else {\
                    PRINT(ERROR, "Band changed mid function. This code should be unreachable.\n");\
                    set_property("rx/" STR(ch) "/rf/freq/lna", "1");\
                }\
            } else {\
                set_property("rx/" STR(ch) "/rf/freq/lna", "1");\
            }\
            if(current_gain > target_gain) {\
                atten = current_gain - target_gain;\
            } else {\
                atten = 0;\
            }\
            char s_atten[10];\
            snprintf(s_atten, 10, "%u", atten);\
            set_property("rx/" STR(ch) "/rf/atten/val", s_atten);\
            get_property("rx/" STR(ch) "/rf/atten/val", s_atten, 10);\
            sscanf(s_atten, "%i", &atten);\
            current_gain-= atten;\
            \
        } else {\
            PRINT(ERROR, "Invalid band: %i when setting gain\n", band);\
            snprintf(ret, MAX_PROP_LEN, "0");\
            return RETURN_ERROR_PARAM;\
        }\
                                                                               \
        snprintf(ret, 10, "%i", current_gain);                                              \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_atten_val(const char *data, char *ret) {      \
        /*LTC5586 Atten Range: 0dB to 31dB*/                                   \
        int atten;                                                             \
        sscanf(data, "%i", &atten);                                            \
                                                                               \
        if (atten > MID_HIGH_MAX_ATTEN)                                                        \
            atten = MID_HIGH_MAX_ATTEN;                                                        \
        else if (atten < 0)                                                    \
            atten = 0;                                                         \
                                                                               \
        snprintf(buf, MAX_PROP_LEN, "rf -a %i\r", atten);\
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));             \
                                                                               \
        snprintf(ret, MAX_PROP_LEN, "%i", atten);                                             \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_board_dump(const char *data, char *ret) {     \
        /* send the uart commands and read back the output and write to file   \
         */                                                                    \
                                                                               \
        char adc_dump[MAX_PROP_LEN];\
        char mixer_dump[MAX_PROP_LEN];\
        char adc_driver_dump[MAX_PROP_LEN];\
        \
        /* ADC */                                                              \
        strcpy(buf, "dump -a\r");                               \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
        snprintf(adc_dump, MAX_PROP_LEN, "[Board: rx_%s Chip: ADC] %s\n", STR(ch), (char *)uart_ret_buf);\
                                                                               \
        /* Mixer */                                                            \
        strcpy(buf, "dump -x\r");                               \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
        snprintf(mixer_dump, MAX_PROP_LEN, "[Board: rx_%s Chip: GPIOX] %s\n", STR(ch), (char *)uart_ret_buf);\
                                                                               \
        if( RTM_VER==3 || RTM_VER == 4 ) {\
            snprintf(ret, MAX_PROP_LEN, "%s%s", adc_dump, mixer_dump);\
            return RETURN_SUCCESS;\
            /* Due to hardware issues the ADC driver cannot be read from on RTM 3 or 4*/\
        } else {\
            /* ADC Driver */                                                       \
            strcpy(buf, "dump -v\r");                               \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
            snprintf(adc_driver_dump, MAX_PROP_LEN, "[Board: rx_%s Chip: ADC Driver] %s\n", STR(ch), (char *)uart_ret_buf);\
            snprintf(ret, MAX_PROP_LEN, "%s%s%s", adc_dump, mixer_dump, adc_driver_dump);\
            return RETURN_SUCCESS;\
        }\
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
        strcpy(buf, "board -u\r");                              \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_status_rfld(const char *data, char *ret) {       \
        strcpy(buf, "status -l\r");                             \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
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
    static int hdlr_rx_##ch##_status_lna(const char *data, char *ret) {   \
        snprintf(buf, 10, "rf -S\r");                                          \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)); \
        snprintf(ret, 50, (char *)uart_ret_buf);                               \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_rf_board_led(const char *data, char *ret) {      \
        snprintf(buf, MAX_PROP_LEN, "board -l %s\r", data);\
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
        uint32_t gain;\
        uint32_t gain_mask = 0xff<< (8*(INT(ch)%4));\
        sscanf(data, "%i", &gain);                                            \
        if(gain > 0xff) gain = 0xff;\
        uint32_t gain_bits = gain << (8*(INT(ch)%4));\
        write_hps_reg_mask(rxg_map[(int)(INT(ch)/4)], gain_bits, gain_mask);            \
        snprintf(ret, MAX_PROP_LEN, "%i", gain);\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_rate(const char *data, char *ret) {          \
        double rate;                                                           \
        sscanf(data, "%lf", &rate);                                            \
        uint16_t factor = 0;\
        /*Bypasses dsp and half band filer 2. Bypasses dsp when 1*/\
        uint32_t bypass = 0;\
        \
        /* Keeps the sample rate within the allowable range*/\
        if(rate < MIN_RX_SAMPLE_RATE) rate = MIN_RX_SAMPLE_RATE;\
        if(rate > RX_BASE_SAMPLE_RATE) rate = RX_BASE_SAMPLE_RATE;\
        /* If sample rate is roundable to RX_BASE_SAMPLE_RATE (which bypass all dsp stuff */\
        /* Due to issues with the 3G to 1G conversion the rate on rx is actually limited to 500Msps */\
        if(rate > ((RX_DSP_SAMPLE_RATE*RATE_ROUND_BIAS)+(RX_BASE_SAMPLE_RATE*(1-RATE_ROUND_BIAS))) && !USE_3G_AS_1G) {\
            rate = RX_BASE_SAMPLE_RATE;\
            /*the factor does not matter when bypassing the dsp*/\
            factor = 0;\
            bypass = 2;\
            snprintf(ret, MAX_PROP_LEN, "%lf", RX_BASE_SAMPLE_RATE); \
        \
        /*If sample rate is roundable to RX_DSP_SAMPLE_RATE (which bypasses some dsp stuff)*/\
        } else if(rate > RX_DSP_SAMPLE_RATE * (0.5* RATE_ROUND_BIAS + 0.5)) {\
            rate = RX_DSP_SAMPLE_RATE;\
            /*the factor does not matter when bypassing the dsp*/\
            factor = 0;\
            bypass = 1;\
            snprintf(ret, MAX_PROP_LEN, "%lf", RX_DSP_SAMPLE_RATE); \
        } else {\
            bypass = 0;\
            factor = get_optimal_sr_factor(&rate, RX_DSP_SAMPLE_RATE);\
            /*Returns the actual sample rate set*/\
            snprintf(ret, MAX_PROP_LEN, "%lf", RX_DSP_SAMPLE_RATE / (double)(factor + 1)); \
        }\
        \
        /*Sets the resamp factor*/\
        write_hps_reg("rx" STR(ch) "1", factor);                      \
        /*Set whether to bypass dsp and fir*/\
        write_hps_reg("rx" STR(ch) "2", bypass);                      \
        \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_fpga_nco(const char *data, char *ret) {      \
        double freq = 0;                                                       \
        uint32_t old_val = 0;                                                  \
        uint8_t direction = 0;                                                 \
        char read_s[50];\
        get_property("rx/" STR(ch) "/dsp/rate", read_s, 50);\
        double rate;\
        sscanf(read_s, "%lf", &rate);\
        get_property("rx/" STR(ch) "/about/fw_ver", read_s, 50);\
        /* strstr will return NULL if the Rx3 string is not found, in which case the RX board is not a 3G board*/\
        char *rx_hw_is_3g = strstr(read_s, "Rx3");\
        \
        /* nco shift caused by hardware configuration */\
        double hardware_shift;\
        if(USE_3G_AS_1G && (rx_hw_is_3g != NULL)) {\
            hardware_shift = RX_NCO_SHIFT_3G_TO_1G;\
        } else {\
            hardware_shift = 0;\
        }\
                                                                               \
        /* The dsp (including the nco is bypassed at or above the RX_DSP_SAMPLE_RATE */                      \
        /* FPGA NCO bypassed, note that it may still report non 0 to compensate for hardware shifts */\
        if (rate >= RX_DSP_SAMPLE_RATE) {\
            snprintf(ret, MAX_PROP_LEN, "%lf", hardware_shift);\
        /* Normal FPGA NCO usage */\
        } else {\
            sscanf(data, "%lf", &freq);                                        \
            \
            /* Shift the frequency attempted by variable components to account for the fixed components*/\
            freq += hardware_shift;\
            /* NCO uses a bit on one register to direction, another for magnitude */\
            if(freq < 0) {\
                direction = 1;\
                freq = -freq;\
            } else {\
                direction = 0;\
            }\
                                                                                \
            /* write NCO adj */                                                    \
            uint32_t nco_steps = (uint32_t)round(freq * RX_DSP_NCO_CONST);            \
            write_hps_reg("rx" STR(ch) "0", nco_steps);                            \
            \
            /* write direction */                                                  \
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                              \
            write_hps_reg(rx_reg4_map[INT(ch)],                                        \
                        (old_val & ~(0x1 << 13)) | (direction << 13));           \
            \
            double actual_freq;\
            if(direction) {\
                actual_freq = (-(double)nco_steps / RX_DSP_NCO_CONST) - hardware_shift;\
            } else {\
                actual_freq = (double)nco_steps / RX_DSP_NCO_CONST - hardware_shift;\
            }\
            snprintf(ret, MAX_PROP_LEN, "%lf", actual_freq);\
        }\
        \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_rstreq(const char *data, char *ret) {        \
        uint32_t old_val = 0;                                                  \
        read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                              \
        /* Leaves dsp in reset if it already was in reset*/\
        if(!(old_val & 2)) {\
            write_hps_reg(rx_reg4_map[INT(ch)], old_val & ~0x2);                       \
        }\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_dsp_loopback(const char *data, char *ret) {      \
        uint32_t old_val = 0;                                                  \
        read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                              \
        if (strcmp(data, "1") == 0)                                            \
            write_hps_reg(rx_reg4_map[INT(ch)], (old_val & ~0x1e00) | 0x400);      \
        else                                                                   \
            write_hps_reg(rx_reg4_map[INT(ch)], (old_val & ~0x1e00) | 0x000);      \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    /* Selects the size of samples */\
    /* Takes the size of half the complex pair (i.e. for sc16 set to 16 */\
    static int hdlr_rx_##ch##_dsp_sample_bandwidth(const char *data, char *ret) {        \
        int wire_format = 0;\
        sscanf(data, "%i", &wire_format);\
        \
        if(MAX_SAMPLE_RATE == 1000) {\
            /* Wire formate cannot be set on 1G*/\
            if(wire_format != DEAULT_OTW_RX) {\
                PRINT(ERROR, "Unsupported wire format selected: sc%i. Defaulting to sc %i\n", wire_format, DEAULT_OTW_RX);\
                wire_format = DEAULT_OTW_RX;\
            }\
        /* NOTE: prior to May 2024 3G FPGA only supported sc12 for 3G */\
        } else if(MAX_SAMPLE_RATE == 3000) {\
            if(wire_format == 12) {\
                write_hps_reg_mask(rx_reg4_map[INT(ch)], 0x40000, 0x40000);\
            } else if(wire_format == 16) {\
                write_hps_reg_mask(rx_reg4_map[INT(ch)], 0x00000, 0x40000);\
            } else {\
                PRINT(ERROR, "Unsupported wire format selected: sc%i. Defaulting to sc %i\n", wire_format, 16);\
                wire_format = 16;\
                write_hps_reg_mask(rx_reg4_map[INT(ch)], 0x00000, 0x40000);\
            }\
        } else {\
            PRINT(ERROR, "function not implemented for variants with maximum sample rate: %i\n", MAX_SAMPLE_RATE);\
        }\
        snprintf(ret, MAX_PROP_LEN, "%i", wire_format);\
        return RETURN_SUCCESS;                                                 \
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
        write_hps_reg("res_rw12", ch_select);\
        write_hps_reg("res_rw13", reg_val | 0x1000);\
        write_hps_reg("res_rw13", reg_val);\
        snprintf(ret, MAX_PROP_LEN, "%i %i", i_delay, q_delay);\
        return RETURN_SUCCESS;\
    }                                                                          \
    \
    /*0 for big endian packets, anything else for little endian*/\
    static int hdlr_rx_##ch##_endian_swap(const char *data, char *ret) {      \
        int endian = 0;\
        sscanf(data, "%i", &endian);\
        int bit_val;\
        if(endian && LITTLE_ENDIAN_SUPPORTED) {\
            bit_val = 0x20000;\
        } else {\
            bit_val = 0;\
        }\
        write_hps_reg_mask(rx_reg4_map[INT(ch)], bit_val, 0x20000);\
        if(endian && LITTLE_ENDIAN_SUPPORTED) {\
            snprintf(ret, MAX_PROP_LEN, "1");\
        } else {\
            snprintf(ret, MAX_PROP_LEN, "0");\
        }\
        return RETURN_SUCCESS;\
    }                                                                          \
    \
    static int hdlr_rx_##ch##_about_id(const char *data, char *ret) {          \
        /* don't need to do anything, save the ID in the file system */        \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_link_vita_en(const char *data, char *ret) {      \
        uint32_t old_val = 0;                                                  \
        read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                              \
        if (strcmp(data, "1") == 0)                                            \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val | (1 << 14));              \
        else                                                                   \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val & ~(1 << 14));             \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_link_iq_swap(const char *data, char *ret) {      \
        uint32_t old_val = 0;                                                      \
        read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                          \
        if (strcmp(data, "1") == 0)                                            \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val | (1 << 12));          \
        else                                                                   \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val & ~(1 << 12));         \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    static int hdlr_rx_##ch##_iq_gain_correction(const char *data, char *ret) {      \
           double iq_gain_error = 0;                                           \
           sscanf(data, "%lf", &iq_gain_error);                                \
           PRINT(INFO,"data as entered %lf\n", iq_gain_error);\
           /* Gain is expected to be between -0.5 .. +0.5dB */                 \
           if ( iq_gain_error >= 0.5 ){                                        \
               iq_gain_error = 0.5;                                             \
           }                                                                   \
           if ( iq_gain_error <= -0.5 ){                                       \
               iq_gain_error = -0.5;                                            \
           }                                                                   \
           PRINT(INFO,"data after limit check: %lf\n", iq_gain_error);\
           PRINT(INFO,"Flag 0\n");                                              \
           double step_size = 0.015625;                                          \
            double iq_gain_correction = (iq_gain_error/step_size);                \
            PRINT(INFO,"intermediate conversion: %lf\n",iq_gain_correction);               \
            uint8_t iq_gain_factor = 0x20;                                                 \
            if ( iq_gain_correction >= 0) {                                                \
                iq_gain_factor += (uint8_t) iq_gain_correction;                            \
            } else { /* less than zero */                                                  \
                iq_gain_correction = (uint8_t) ((int)iq_gain_correction) * -1;             \
                iq_gain_factor = (uint8_t) 0x20 - iq_gain_correction;                      \
            }                                                                              \
            if(iq_gain_error == 0.5){                                                       \
                iq_gain_factor -= 0x01;                                                     \
            }                                                                               \
            PRINT(INFO,"data after float to hex conversion: %x\n",iq_gain_factor);          \
          sprintf(ret,"%f",(uint8_t) iq_gain_correction * step_size);                      \
          strcpy(buf, "iq -g ");                                                           \
          sprintf(buf + strlen(buf), "%x", iq_gain_factor);                                \
          strcat(buf, "\r");                                                               \
          ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));           \
          return RETURN_SUCCESS;                                                           \
    }                                                                                       \
    static int hdlr_rx_##ch##_iq_phase_correction(const char *data, char *ret) {            \
        double iq_phase_error = 0;                                                          \
        /*input range: -2.5 to 2.5, output range:0x0000 to 0x01FF, offset: 0x100, */        \
        sscanf(data,"%lf",&iq_phase_error);                                                 \
        PRINT(INFO,"data as entered %lf\n",iq_phase_error);                                 \
        if (iq_phase_error >= 2.5){                                                         \
            iq_phase_error = 2.5;                                                           \
        }                                                                                   \
        if (iq_phase_error <= -2.5){                                                        \
            iq_phase_error = -2.5;                                                          \
        }                                                                                   \
        double inv_step_size = 102.4;                                                       \
        PRINT(INFO, "data after limit check: %lf \n",iq_phase_error);                       \
        double iq_phase_correction = (iq_phase_error*inv_step_size);                        \
        PRINT(INFO,"intermediate conversion: %lf\n",iq_phase_correction);                   \
        uint32_t iq_phase_factor = 0x100;                                                    \
        if(iq_phase_correction >= 0){                                                       \
            iq_phase_factor += (uint32_t) iq_phase_correction;                               \
        } else{                                                                             \
            iq_phase_correction = (uint32_t) ((int)iq_phase_correction) * -1;                \
            iq_phase_factor = (uint32_t) 0x100 -iq_phase_correction;                         \
        }                                                                                   \
        if(iq_phase_error == 2.5){                                                          \
            iq_phase_factor -= 0x01;                                                        \
        }                                                                                   \
        sprintf(ret,"%f",(unsigned long) iq_phase_correction * (1/inv_step_size));       \
        strcpy(buf, "iq -p ");                                                              \
        sprintf(buf + strlen(buf), "%x", iq_phase_factor);                                  \
        strcat(buf, "\r");                                                                  \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));              \
        return RETURN_SUCCESS;                                                              \
    }                                                                          \
    static int hdlr_rx_##ch##_iq_dco_i(const char *data, char *ret) {      \
        double iq_dcoffset_i = 0;                                                          \
        /*input range: -200 to 200, output range:0x0000 to 0x00FF, offset: 0x80, */        \
        sscanf(data,"%lf",&iq_dcoffset_i);                                                 \
        PRINT(INFO,"data as entered %lf\n",iq_dcoffset_i);                                 \
        if (iq_dcoffset_i >= 200){                                                         \
            iq_dcoffset_i = 200;                                                           \
        }                                                                                   \
        if (iq_dcoffset_i <= -200){                                                        \
            iq_dcoffset_i = -200;                                                          \
        }                                                                                   \
        double step_size = 1.5625;                                                       \
        PRINT(INFO, "data after limit check: %lf \n",iq_dcoffset_i);                       \
        double iq_dcoi_correction = (iq_dcoffset_i/step_size);                        \
        PRINT(INFO,"intermediate conversion: %lf\n",iq_dcoi_correction);              \
        uint8_t iq_dcoi_factor = 0x80;                                                    \
        if(iq_dcoi_correction >= 0){                                                       \
            iq_dcoi_factor += (uint8_t) iq_dcoi_correction;                               \
        } else {                                                                             \
            iq_dcoi_correction = (uint8_t) ((int)iq_dcoi_correction) * -1;                \
            iq_dcoi_factor = (uint8_t) 0x80 -iq_dcoi_correction;                         \
        }                                                                                   \
        if(iq_dcoffset_i == 200){                                                          \
            iq_dcoi_factor -= 0x01;                                                        \
        }                                                                                   \
        sprintf(ret,"%d",(uint8_t) iq_dcoi_correction);                                     \
        strcpy(buf, "iq -c ");                                                              \
        sprintf(buf + strlen(buf), "%x", iq_dcoi_factor);                                  \
        strcat(buf, "\r");                                                                  \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));              \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    static int hdlr_rx_##ch##_iq_dco_q(const char *data, char *ret) {      \
           /* To do */                                         \
        double iq_dcoffset_q = 0;                                                          \
        /*input range: -200 to 200, output range:0x0000 to 0x00FF, offset: 0x80, */        \
        sscanf(data,"%lf",&iq_dcoffset_q);                                                 \
        PRINT(INFO,"data as entered %lf\n",iq_dcoffset_q);                                 \
        if (iq_dcoffset_q >= 200){                                                         \
            iq_dcoffset_q = 200;                                                           \
        }                                                                                   \
        if (iq_dcoffset_q <= -200){                                                        \
            iq_dcoffset_q = -200;                                                          \
        }                                                                                   \
        double step_size = 1.5625;                                                       \
        PRINT(INFO, "data after limit check: %lf \n",iq_dcoffset_q);                       \
        double iq_dcoq_correction = (iq_dcoffset_q/step_size);                        \
        PRINT(INFO,"intermediate conversion: %lf\n",iq_dcoq_correction);              \
        uint8_t iq_dcoq_factor = 0x80;                                                    \
        if(iq_dcoq_correction >= 0){                                                       \
            iq_dcoq_factor += (uint8_t) iq_dcoq_correction;                               \
        } else {                                                                             \
            iq_dcoq_correction = (uint8_t) ((int)iq_dcoq_correction) * -1;                \
            iq_dcoq_factor = (uint8_t) 0x80 -iq_dcoq_correction;                         \
        }                                                                                   \
        if(iq_dcoffset_q == 200){                                                          \
            iq_dcoq_factor -= 0x01;                                                        \
        }                                                                                   \
        PRINT(INFO,"data after float to hex conversion: %x\n",iq_dcoq_factor);             \
        sprintf(ret,"%d",(uint8_t) iq_dcoq_correction);                                     \
        strcpy(buf, "iq -C ");                                                              \
        sprintf(buf + strlen(buf), "%x", iq_dcoq_factor);                                  \
        strcat(buf, "\r");                                                                  \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));              \
        return RETURN_SUCCESS;                                                 \
    }                                                                                   \
    static int hdlr_rx_##ch##_iq_hd2_ix(const char *data, char *ret) {                      \
                                                                                    \
            uint8_t harmonic_distortion;                                               \
            sscanf(data,"%hhu",&harmonic_distortion);                                      \
            PRINT(INFO,"data as entered %d \n",harmonic_distortion);                    \
            if(harmonic_distortion >= 255){                                            \
                harmonic_distortion = 255;                                             \
            }                                                                          \
            if(harmonic_distortion <= 0){                                              \
                harmonic_distortion = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",harmonic_distortion);       \
            strcpy(buf, "iq -z ");                                                     \
            sprintf(buf+ strlen(buf),"%x",harmonic_distortion);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                                   \
    static int hdlr_rx_##ch##_iq_hd2_iy(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t harmonic_distortion;                                               \
            sscanf(data,"%hhu",&harmonic_distortion);                                      \
            PRINT(INFO,"data as entered %d \n",harmonic_distortion);                    \
            if(harmonic_distortion >= 255){                                            \
                harmonic_distortion = 255;                                             \
            }                                                                          \
            if(harmonic_distortion <= 0){                                              \
                harmonic_distortion = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",harmonic_distortion);       \
            strcpy(buf, "iq -y ");                                                     \
            sprintf(buf+ strlen(buf),"%x",harmonic_distortion);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                               \
    static int hdlr_rx_##ch##_iq_hd2_qx(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t harmonic_distortion;                                               \
            sscanf(data,"%hhu",&harmonic_distortion);                                      \
            PRINT(INFO,"data as entered %d \n",harmonic_distortion);                    \
            if(harmonic_distortion >= 255){                                            \
                harmonic_distortion = 255;                                             \
            }                                                                          \
            if(harmonic_distortion <= 0){                                              \
                harmonic_distortion = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",harmonic_distortion);       \
            strcpy(buf, "iq -Z ");                                                     \
            sprintf(buf+ strlen(buf),"%x",harmonic_distortion);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                               \
    static int hdlr_rx_##ch##_iq_hd2_qy(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t harmonic_distortion;                                               \
            sscanf(data,"%hhu",&harmonic_distortion);                                      \
            PRINT(INFO,"data as entered %d \n",harmonic_distortion);                    \
            if(harmonic_distortion >= 255){                                            \
                harmonic_distortion = 255;                                             \
            }                                                                          \
            if(harmonic_distortion <= 0){                                              \
                harmonic_distortion = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",harmonic_distortion);       \
            strcpy(buf, "iq -Y ");                                                     \
            sprintf(buf+ strlen(buf),"%x",harmonic_distortion);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                               \
    static int hdlr_rx_##ch##_iq_hd3_ix(const char *data, char *ret) {                      \
                                                                                    \
            uint8_t harmonic_distortion;                                               \
            sscanf(data,"%hhu",&harmonic_distortion);                                      \
            PRINT(INFO,"data as entered %d \n",harmonic_distortion);                    \
            if(harmonic_distortion >= 255){                                            \
                harmonic_distortion = 255;                                             \
            }                                                                          \
            if(harmonic_distortion <= 0){                                              \
                harmonic_distortion = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",harmonic_distortion);       \
            strcpy(buf, "iq -x ");                                                     \
            sprintf(buf+ strlen(buf),"%x",harmonic_distortion);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                                   \
    static int hdlr_rx_##ch##_iq_hd3_iy(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t harmonic_distortion;                                               \
            sscanf(data,"%hhu",&harmonic_distortion);                                      \
            PRINT(INFO,"data as entered %d \n",harmonic_distortion);                    \
            if(harmonic_distortion >= 255){                                            \
                harmonic_distortion = 255;                                             \
            }                                                                          \
            if(harmonic_distortion <= 0){                                              \
                harmonic_distortion = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",harmonic_distortion);       \
            strcpy(buf, "iq -w ");                                                     \
            sprintf(buf+ strlen(buf),"%x",harmonic_distortion);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                               \
    static int hdlr_rx_##ch##_iq_hd3_qx(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t harmonic_distortion;                                               \
            sscanf(data,"%hhu",&harmonic_distortion);                                      \
            PRINT(INFO,"data as entered %d \n",harmonic_distortion);                    \
            if(harmonic_distortion >= 255){                                            \
                harmonic_distortion = 255;                                             \
            }                                                                          \
            if(harmonic_distortion <= 0){                                              \
                harmonic_distortion = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",harmonic_distortion);       \
            strcpy(buf, "iq -X ");                                                     \
            sprintf(buf+ strlen(buf),"%x",harmonic_distortion);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                               \
    static int hdlr_rx_##ch##_iq_hd3_qy(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t harmonic_distortion;                                               \
            sscanf(data,"%hhu",&harmonic_distortion);                                      \
            PRINT(INFO,"data as entered %d \n",harmonic_distortion);                    \
            if(harmonic_distortion >= 255){                                            \
                harmonic_distortion = 255;                                             \
            }                                                                          \
            if(harmonic_distortion <= 0){                                              \
                harmonic_distortion = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",harmonic_distortion);       \
            strcpy(buf, "iq -W ");                                                     \
            sprintf(buf+ strlen(buf),"%x",harmonic_distortion);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                               \
    static int hdlr_rx_##ch##_iq_im2_ix(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t im_input;                                               \
            sscanf(data,"%hhu",&im_input);                                      \
            PRINT(INFO,"data as entered %d \n",im_input);                    \
            if(im_input >= 255){                                            \
                im_input = 255;                                             \
            }                                                                          \
            if(im_input <= 0){                                              \
                im_input = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",im_input);       \
            strcpy(buf, "iq -h ");                                                     \
            sprintf(buf+ strlen(buf),"%x",im_input);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                               \
    static int hdlr_rx_##ch##_iq_im2_qx(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t im_input;                                               \
            sscanf(data,"%hhu",&im_input);                                      \
            PRINT(INFO,"data as entered %d \n",im_input);                    \
            if(im_input >= 255){                                            \
                im_input = 255;                                             \
            }                                                                          \
            if(im_input <= 0){                                              \
                im_input = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",im_input);       \
            strcpy(buf, "iq -H ");                                                     \
            sprintf(buf+ strlen(buf),"%x",im_input);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                               \
    static int hdlr_rx_##ch##_iq_im3_ix(const char *data, char *ret) {                      \
                                                                                    \
            uint8_t im_input;                                               \
            sscanf(data,"%hhu",&im_input);                                      \
            PRINT(INFO,"data as entered %d \n",im_input);                    \
            if(im_input >= 255){                                            \
                im_input = 255;                                             \
            }                                                                          \
            if(im_input <= 0){                                              \
                im_input = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",im_input);  \
            strcpy(buf, "iq -k ");                                                     \
            sprintf(buf + strlen(buf),"%x",im_input);                       \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                                   \
    static int hdlr_rx_##ch##_iq_im3_iy(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t im_input;                                               \
            sscanf(data,"%hhu",&im_input);                                      \
            PRINT(INFO,"data as entered %d \n",im_input);                    \
            if(im_input >= 255){                                            \
                im_input = 255;                                             \
            }                                                                          \
            if(im_input <= 0){                                              \
                im_input = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",im_input);       \
            strcpy(buf, "iq -l ");                                                     \
            sprintf(buf+ strlen(buf),"%x",im_input);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                               \
    static int hdlr_rx_##ch##_iq_im3_qx(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t im_input;                                               \
            sscanf(data,"%hhu",&im_input);                                      \
            PRINT(INFO,"data as entered %d \n",im_input);                    \
            if(im_input >= 255){                                            \
                im_input = 255;                                             \
            }                                                                          \
            if(im_input <= 0){                                              \
                im_input = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",im_input);       \
            strcpy(buf, "iq -K ");                                                     \
            sprintf(buf+ strlen(buf),"%x",im_input);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                               \
    static int hdlr_rx_##ch##_iq_im3_qy(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t im_input;                                               \
            sscanf(data,"%hhu",&im_input);                                      \
            PRINT(INFO,"data as entered %d \n",im_input);                    \
            if(im_input >= 255){                                            \
                im_input = 255;                                             \
            }                                                                          \
            if(im_input <= 0){                                              \
                im_input = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",im_input);       \
            strcpy(buf, "iq -L ");                                                     \
            sprintf(buf+ strlen(buf),"%x",im_input);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                                  \
    static int hdlr_rx_##ch##_iq_ip3cc(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t cc_adj;                                               \
            sscanf(data,"%hhu",&cc_adj);                                      \
            PRINT(INFO,"data as entered %d \n",cc_adj);                    \
            if(cc_adj >= 3){                                            \
                cc_adj = 3;                                             \
            }                                                                          \
            if(cc_adj <= 0){                                              \
                cc_adj = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",cc_adj);       \
            strcpy(buf, "iq -m ");                                                     \
            sprintf(buf+ strlen(buf),"%x",cc_adj);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                               \
    static int hdlr_rx_##ch##_iq_ip3ic(const char *data, char *ret) {                  \
                                                                                    \
            uint8_t ic_adj;                                               \
            sscanf(data,"%hhu",&ic_adj);                                      \
            PRINT(INFO,"data as entered %d \n",ic_adj);                    \
            if(ic_adj >= 7){                                            \
                ic_adj = 7;                                             \
            }                                                                          \
            if(ic_adj <= 0){                                              \
                ic_adj = 0;                                              \
            }                                                                          \
            PRINT(INFO,"data after int to hex conversion: %x\n",ic_adj);       \
            strcpy(buf, "iq -n ");                                                     \
            sprintf(buf+ strlen(buf),"%x",ic_adj);                             \
            strcat(buf, "\r");                                                         \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));     \
            return RETURN_SUCCESS;                                                     \
    }                                                                               \
                                                                               \
    static int hdlr_rx_##ch##_link_iface(const char *data, char *ret) {        \
        /* TODO: FW support for streaming to management port required */       \
        snprintf(ret, MAX_PROP_LEN, rx_sfp_map[INT(ch)]);                                  \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /* Destination UDP port for rx*/\
    static int hdlr_rx_##ch##_link_port(const char *data, char *ret) {         \
        uint32_t port;                                                         \
        sscanf(data, "%" SCNd32 "", &port);                                    \
        write_hps_reg("rx" STR(ch) "8", port);                                 \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    static int hdlr_rx_##ch##_link_src_port(const char *data, char *ret) {         \
        uint32_t port;                                                         \
        sscanf(data, "%" SCNd32 "", &port);                                    \
        write_hps_reg(device_side_port_map[INT(ch)], port);   \
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
            write_hps_reg_mask(rx_reg4_map[INT(ch)], 0x002, 0x102);\
            rx_stream[INT(ch)] = STREAM_OFF;\
            /*Ignores sma (enabling normal stream command)*/\
            set_property("rx/" STR(ch) "/trigger/trig_sel", "0");\
            /*turn time disable off*/\
            set_property("rx/" STR(ch) "/trigger/time_disable", "0");\
        }else {\
            rx_stream[INT(ch)] = STREAM_ON;\
            /*Stream when sma trigger (has the side effect of disabling normal stream commands)*/\
            set_property("rx/" STR(ch) "/trigger/trig_sel", "1");\
            /*turn time disable off*/\
            set_property("rx/" STR(ch) "/trigger/time_disable", "1");\
            /*enable channel and take the dsp out of reset*/\
            write_hps_reg_mask(rx_reg4_map[INT(ch)], 0x100, 0x102);\
        }\
        return RETURN_SUCCESS;                                                 \
    } \
    \
    static int hdlr_rx_##ch##_stream(const char *data, char *ret) {            \
        uint32_t old_val = 0;                                                  \
        uint8_t stream = 0;                                                    \
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
            if (rx_power[INT(ch)] & PWR_ON) {                                 \
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
                snprintf(ret, MAX_PROP_LEN, "%u", 0); /* Alert File Tree */                   \
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
        if(rx_power[INT(ch)] & PWR_NO_BOARD) {\
            /*Technically this should be an error, but it would trigger everytime an unused slot does anything, clogging up error logs*/\
            if(power>=PWR_ON) {\
                rx_power[INT(ch)] = PWR_HALF_ON | PWR_NO_BOARD;\
            } else {\
                rx_power[INT(ch)] = PWR_HALF_ON | PWR_NO_BOARD;\
            }\
            return RETURN_SUCCESS;\
        }\
                                                                               \
        char pwr_cmd [40];                                                 \
        if(power>=PWR_ON) {\
            set_property("time/sync/sysref_mode", "continuous");\
            snprintf(pwr_cmd, 40, "rfe_control %d on", INT_RX(ch));                    \
            system(pwr_cmd);                                                   \
            rx_power[INT(ch)] = PWR_HALF_ON;\
        } else {\
            rx_3g_set_to_1g[INT(ch)] = 0;\
            /* This function is meant to block until after power is either on or off. However a hardware issue can cause unpopulated boards to never be detected as off*/\
            snprintf(pwr_cmd, 40, "rfe_control %d off %i", INT_RX(ch), PWR_TIMEOUT);\
            system(pwr_cmd);                                                   \
            rx_power[INT(ch)] = PWR_OFF;\
        }\
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    /*Turns the board on or off, and performs none of the other steps in the turn on/off process*/\
    /*pwr must be used to complete the power on process*/\
    /*returns the pid of the powr on process*/\
    static int hdlr_rx_##ch##_async_pwr_board(const char *data, char *ret) {               \
        uint8_t power;                                                         \
        sscanf(data, "%" SCNd8 "", &power);                                    \
        set_property("time/sync/sysref_mode", "continuous");\
        \
        char pwr_cmd[30];\
        char str_state[25];\
        if(power>=PWR_ON) {\
            strcpy(str_state, "on");\
        } else {\
            strcpy(str_state, "off");\
        }\
        snprintf(pwr_cmd, 30, "/usr/bin/rfe_control %i %s n", INT_RX(ch), str_state);\
        system(pwr_cmd);\
        time(&rx_async_start_time[INT(ch)]);\
        \
        return RETURN_SUCCESS;\
    }                                                                          \
    /*waits for the rx board to turn on with a timeout. If the timeout occurs, assume the board is not connected*/\
    static int hdlr_rx_##ch##_wait_async_pwr(const char *data, char *ret) {               \
        \
        /* Polls to check if a board has turned on */\
        do {\
            snprintf(buf, sizeof(buf), "/usr/bin/rfe_control %i check", INT_RX(ch));\
            FILE *check_result;\
            check_result = popen(buf, "r");\
            size_t bytes_read = fread(buf, 1, sizeof(buf), check_result);\
            if(bytes_read == 0) {\
                rx_power[INT(ch)] = PWR_NO_BOARD;\
                PRINT(ERROR,"Error in script controlling power for rx board %i, the slot will not be used\n", INT(ch));\
                return errno;\
            } else if(strstr(buf, "on")) {\
                rx_power[INT(ch)] = PWR_HALF_ON;\
                PRINT(INFO,"Rx board %i powered on within %lus\n", INT(ch), time(NULL) - rx_async_start_time[INT(ch)]);\
                return RETURN_SUCCESS;\
            }\
        \
        } while (time(NULL) < rx_async_start_time[INT(ch)] + PWR_TIMEOUT);\
        \
        rx_power[INT(ch)] = PWR_NO_BOARD;\
        PRINT(ERROR,"Timeout while powering rx board %i, the slot will not be used\n", INT(ch));\
        \
        return RETURN_SUCCESS;\
    }                                                                          \
    \
    static int hdlr_rx_##ch##_jesd_reset(const char *data, char *ret) {       \
        if(rx_power[INT(ch)] & PWR_NO_BOARD) {\
            /*Technically this should be an error, but it would trigger everytime an unused slot does anything, clogging up error logs*/\
            return RETURN_SUCCESS;\
        }\
        int reset = 0;                                                            \
        sscanf(data, "%i", &reset);                                           \
        if (reset){\
            set_property("rx/" STR(ch) "/jesd/unmask", "1");\
            /* Gives time for sysref unmask to update, can probably be shorted */\
            usleep(jesd_mask_delay);\
            write_hps_reg_mask("res_rw7", 1 << (INDIVIDUAL_RESET_BIT_OFFSET_RX + INT(ch)), 1 << (INDIVIDUAL_RESET_BIT_OFFSET_RX + INT(ch)));\
            write_hps_reg_mask("res_rw7", 1 << (INDIVIDUAL_RESET_BIT_OFFSET_RX + INT(ch)), 1 << (INDIVIDUAL_RESET_BIT_OFFSET_RX + INT(ch)));\
            /* Issues sysref pulse if not in continuous */\
            if(current_sysref_mode != continuous) {\
                set_property("time/sync/lmk_sync_tgl_jesd", "1");\
            };\
            usleep(jesd_reset_delay);\
            set_property("rx/" STR(ch) "/jesd/unmask", "0");\
            return RETURN_SUCCESS;\
        }\
        else {\
            return RETURN_SUCCESS;\
        }\
    }                                                                          \
    \
    static int hdlr_rx_##ch##_jesd_pll_locked(const char *data, char *ret) {       \
        uint32_t lock_status = 0;\
        write_hps_reg("res_rw8", LR_NUM_RX(ch));\
        read_hps_reg("res_ro20", &lock_status);\
        lock_status = lock_status & 0x2;\
        if(lock_status) {\
            snprintf(ret, 10, "locked");\
        } else {\
            snprintf(ret, 10, "unlocked");\
        }\
        \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    /* The dev clocks take a small amount of time to stablize. Make sure there is sufficient time between when they are inverted and when the ADC is turned on*/\
    static int hdlr_rx_##ch##_invert_devclk(const char *data, char *ret) {       \
        if(rx_power[INT(ch)] & PWR_NO_BOARD) {\
            /*Technically this should be an error, but it would trigger everytime an unused slot does anything, clogging up error logs*/\
            return RETURN_SUCCESS;\
        }\
        int invert;                                                            \
        sscanf(data, "%i", &invert);                                           \
        if (invert) {\
            snprintf(buf, 40, "clk -r %i -p 1\r", INT_RX(ch));\
            ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));\
            usleep(1);\
        } else {\
            snprintf(buf, 40, "clk -r %i -p 0\r", INT_RX(ch));\
            ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));\
            usleep(1);\
        }\
        \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    static int hdlr_rx_##ch##_jesd_error(const char *data, char *ret) {       \
        /*The onehot jesd core converted into hex*/\
        int start;                                                            \
        sscanf(data, "%i", &start);\
        if ( start == 1 ){\
            uint32_t rx_err_VAL = 0;                                           \
            bool flag_ok = true;                                               \
            strcpy(ret,"");                                                    \
            for (int j = 0; j<=1; j++){ /* check err0 and err1 registers */    \
                write_hps_reg("net6", 0x1 << INT(ch));                         \
                write_hps_reg("net7", 0x18+j);                                 \
                write_hps_reg("net9", 0x1);\
                write_hps_reg("net9", 0x0);\
                read_hps_reg("res_ro30", &rx_err_VAL);                         \
                /*Checking for errors*/\
                if (rx_err_VAL == 0 ){                                         \
                    PRINT(INFO, "rx_err%i has no errors\n",j);                 \
                } else {                                                       \
                    /* note legacy FPGA will report value 0xDEADBEEF */        \
                    flag_ok = false;                                           \
                    PRINT(INFO, "Bad link for Rx : %d, rx_err%i is : %X\n",\
                        INT(ch), j, rx_err_VAL);                               \
                    strcpy(ret, "err");                                        \
                    sprintf(ret + strlen(ret), "%i", j);                       \
                    strcat(ret, ": 0x");                                       \
                    sprintf(ret + strlen(ret), "%x", rx_err_VAL);              \
                    strcat(ret,"\n");                                          \
                    PRINT(INFO, "Resetting Errors\n");                         \
                    write_hps_reg("net6", 0x1 << INT(ch));                     \
                    write_hps_reg("net7", 0x18+j);                             \
                    write_hps_reg("net8", rx_err_VAL);                         \
                    write_hps_reg("net9", 0x2);\
                    write_hps_reg("net9", 0x0);\
                    PRINT(INFO, "rx_err%i been reset\n",j);                    \
                }\
            }/*rof j (err0 and err1)*/                                         \
            if (flag_ok) {                                                     \
                snprintf(ret, sizeof("good"), "good");                         \
            }                                                                  \
        }/*fi start*/                                                          \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    /* Negative: pretend there's no board there and turn the board off */\
    /* 0: disable the board but to not turn it off (use pwr_board if you actually want to turn it off) */\
    /* Positive: enable the board/finish power on process */\
    static int hdlr_rx_##ch##_pwr(const char *data, char *ret) {               \
        if(rx_power[INT(ch)] & PWR_NO_BOARD) {\
            /*Technically this should be an error, but it would trigger everytime an unused slot does anything, clogging up error logs*/\
            return RETURN_SUCCESS;\
        }\
        uint32_t old_val = 0;                                                  \
        int num_lna_attempts = 0;                                              \
        int8_t power = 0;                                                     \
        sscanf(data, "%" SCNd8 "", &power);                                    \
        \
        /* Pretend the slot is empty*/\
        if(power < 0) {\
            rx_power[INT(ch)] = PWR_NO_BOARD;\
            write_hps_reg_mask(rx_reg4_map[INT(ch)], 0x2, 0x2);\
            snprintf(buf, 40, "rfe_control %d off n", INT_RX(ch));\
            system(buf);\
            return RETURN_SUCCESS;\
        }\
                                                                               \
        /* check if power is already enabled */                                \
        if (power >= PWR_ON && (rx_power[INT(ch)] & PWR_ON))                    \
            return RETURN_SUCCESS;                                             \
                                                                               \
        /* power on */                                                         \
        if (power >= PWR_ON) {                                                 \
            /*Avoids attempting to turn on a  board if its off or already turned on but not initialized*/\
            if((rx_power[INT(ch)] & ~PWR_NO_BOARD) == PWR_OFF) {\
                set_property("rx/" STR(ch) "/board/pwr_board", "1");\
            }\
            \
            if(USE_3G_AS_1G && !rx_3g_set_to_1g[INT(ch)]) {\
                /* Tells the 3G board to operate in 1G mode */\
                snprintf(buf, MAX_PROP_LEN, "board -i 1000\r");\
                ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
                rx_3g_set_to_1g[INT(ch)] = 1;\
            }\
            \
            /* Sets board state to PWR_ON, jesd_reset_all will only attempt to reset boards that are set to on*/\
            rx_power[INT(ch)] = PWR_ON | (rx_power[INT(ch)] & PWR_NO_BOARD);\
                                                                               \
            /* disable dsp */                                         \
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                               \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val & ~0x100);                      \
            \
            /* Check if low noise aplifier is in a good condition*/            \
            while(1) {                                                         \
                hdlr_rx_##ch##_status_lna("1", buf);                           \
                if(strncmp(buf, "LNA_RDY: 1", 10) == 0) {                      \
                    PRINT(INFO, "LNA is good\n");                              \
                    break;                                                     \
                } else if(num_lna_attempts >= 10){                             \
                    PRINT(ERROR, "Failed to start lna after 10 attempts\n");   \
                    break;                                                     \
                } else {                                                       \
                    PRINT(INFO, "The lna is in a bad state, attempting to restart\n");\
                    num_lna_attempts ++;                                       \
                    snprintf(buf, 20, "rf -L r\r");                            \
                    ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
                }\
            }\
                                                                    \
            /* reset JESD */                                              \
            if(jesd_enabled && !(rx_power[INT(ch)] & PWR_NO_BOARD)) {\
                if(property_good("rx/" STR(ch) "/jesd/status") != 1) {\
                    /* Attempts to reset JESD if it is down, but does not attempt to reboot the unit or reconfigure sysref delays*/\
                    set_property("rx/" STR(ch) "/jesd/reset", "1");\
                }\
            }\
                                                                               \
            /* Puts DSP in reset (should be in reset whenever not stream, use the stream property to take it out of reset */\
            read_hps_reg(rx_reg4_map[INT(ch)], &old_val);                           \
            write_hps_reg(rx_reg4_map[INT(ch)], old_val | 0x2);                     \
            \
            /* Turns the power indicator light on */\
            /* The indicator light turns on when the board boots, and gets turned off without the board being turned off as a workaround for JESD links not re-establishing when rebooting boards*/\
            snprintf(buf, 20, "board -w 1\r");                                 \
            ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
            \
            /* power off & stream off */                                       \
        } else {                                                               \
            /* Turn the power indicator light off but not the entire board if JESD is good */\
            /* This is a temporary solution to the issue of JESD not restablishing after rebooting boards */\
            if(property_good("rx/" STR(ch) "/jesd/status")) {\
                snprintf(buf, 20, "board -w 0\r");\
                ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
                rx_power[INT(ch)] = PWR_HALF_ON | (rx_power[INT(ch)] & PWR_NO_BOARD);\
            } else {\
                set_property("rx/" STR(ch) "/board/pwr_board", "0");\
                rx_power[INT(ch)] = PWR_OFF | (rx_power[INT(ch)] & PWR_NO_BOARD);\
            }\
                                                                               \
            rx_stream[INT(ch)] = STREAM_OFF;                                   \
                                                                               \
            /* turn off the 100MHz LMX ref*/                                   \
            clr_property_bit("time/source/enable_rf_ref", INT_RX(ch));         \
            /* turn off the 1.8GHz IF*/                                        \
            clr_property_bit("time/source/enable_rf_if", INT_RX(ch));          \
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
    /*Reboot the board, performing minimal initialization*/\
    static int hdlr_rx_##ch##_reboot(const char *data, char *ret) {            \
        if(rx_power[INT(ch)] & PWR_NO_BOARD) {\
            /*Technically this should be an error, but it would trigger everytime an unused slot does anything, clogging up error logs*/\
            return RETURN_SUCCESS;\
        }\
        int reboot;                                                            \
        sscanf(data, "%i", &reboot);                                           \
                                                                               \
        if (reboot == 1) {                                                     \
            /*This will cause an error if this runs during initialization*/\
            /*This will wait until the board is done booting*/\
            set_property("rx/" STR(ch) "/pwr", "0");\
            set_property("rx/" STR(ch) "/pwr", "1");\
        }                                                                      \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_serial(const char *data, char *ret) {      \
        strcpy(buf, "status -s\r");                                            \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_mcudevid(const char *data, char *ret) {    \
        strcpy(buf, "status -d\r");                                            \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_mcurev(const char *data, char *ret) {      \
        strcpy(buf, "status -v\r");                                            \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_mcufuses(const char *data, char *ret) {    \
        strcpy(buf, "status -f\r");                                            \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    static int hdlr_rx_##ch##_about_fw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -v\r");                                             \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    static int hdlr_rx_##ch##_about_hw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -h\r");                                             \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
                                                                               \
    /*TODO: make sw_ver only return the mcu version*/\
    static int hdlr_rx_##ch##_about_sw_ver(const char *data, char *ret) {      \
        strcpy(buf, "board -v\r");                                             \
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));                \
        snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);                                     \
                                                                               \
        return RETURN_SUCCESS;                                                 \
    }                                                                          \
    \
    static int hdlr_rx_##ch##_jesd_status(const char *data, char *ret) {       \
        /* res_ro11 holds link data with bit 0 high indicating rx board 0 */   \
        /* link is up, bit 1 high indicating that rx board link 1 is up, etc */\
        if(rx_power[INT(ch)] & PWR_NO_BOARD) {\
            snprintf(ret, 50, "No board detected in slot");\
        }\
        else if((rx_power[INT(ch)] & PWR_ON) || (rx_power[INT(ch)] & PWR_HALF_ON)) {\
            uint32_t reg_val = 0;                                                  \
            read_hps_reg("res_ro11", &reg_val);                                    \
            uint8_t shift = (int)(CHR(ch) - 'a');                                  \
            uint32_t anded = reg_val & (1 << shift);                               \
            if (anded > 0){                                                        \
                snprintf(ret, sizeof("good"), "good");                             \
            } else {                                                               \
                snprintf(ret, sizeof("bad"), "bad");                               \
            }                                                                      \
        } else {\
            snprintf(ret, 50, "Board off");\
        }\
        return RETURN_SUCCESS;                                                 \
    }\
    /*-1 indicates do nothing, 1 indicates respond to sysref, 0 indicates do not respond to sysref*/\
    static int hdlr_rx_##ch##_jesd_mask(const char *data, char *ret) {\
        int mask = 0;\
        sscanf(data, "%i", &mask);\
        if(mask < 0) {\
            return RETURN_SUCCESS;\
        } else if(mask > 0) {\
            snprintf(buf, MAX_PROP_LEN, "board -s 1\r");\
        } else {\
            snprintf(buf, MAX_PROP_LEN, "board -s 0\r");\
        }\
        ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
        \
        return RETURN_SUCCESS;\
    }
RX_CHANNELS
#undef X

#define X(ch)                                                              \
    static int hdlr_tx_##ch##_trigger_gating(const char *data, char *ret) {    \
        int r;                                                                 \
        bool val = 0;                                                          \
        r = tx_valid_gating_mode(data, &val);\
        if(r != RETURN_SUCCESS) return r;\
        else {\
            r = tx_set_gating_mode(#ch, val);        \
            \
        }\
        return r;                                                              \
    }                                                                          
TX_CHANNELS
#undef X


/* -------------------------------------------------------------------------- */
/* ------------------------------ CHANNEL MASK ------------------------------ */
/* -------------------------------------------------------------------------- */

static uint16_t cm_chanmask_get(const char *path) {
    uint16_t r = 0;

    char mask_s[10];
    get_property(path, mask_s,10);
    sscanf(mask_s, "%hux", &r);

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

static int hdlr_cm_chanmask_tx(const char *data, char *ret) {
    uint32_t mask;

    if (1 != sscanf(data, "%x", &mask)) {
        return RETURN_ERROR_PARAM;
    }

    mask &= 0xffff;
    snprintf(ret, MAX_PROP_LEN, "%x", mask);

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

    mask_rx = cm_chanmask_get(STATE_DIR "/cm/chanmask-rx");

    sscanf(data, "%d", &atten);

    snprintf(inbuf, 256, "%d", atten);

    for (i = 0; i < NUM_RX_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }
#define X(ch)                                                              \
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

    mask_rx = cm_chanmask_get(STATE_DIR "/cm/chanmask-rx");

    sscanf(data, "%lf", &gain);

    snprintf(inbuf, 256, "%lf", gain);

    for (i = 0; i < NUM_RX_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }

#define X(ch)                                                              \
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

    mask_tx = cm_chanmask_get(STATE_DIR "/cm/chanmask-tx");

    sscanf(data, "%lf", &gain);

    snprintf(inbuf, 256, "%lf", gain);

    for (i = 0; i < NUM_TX_CHANNELS; i++) {

        if (0 == (mask_tx & (1 << i))) {
            continue;
        }

#define X(ch)                                                              \
    if (i == INT_TX(ch))                                                          \
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
            write_hps_reg_mask(tx_reg4_map[n], request_bit, 1 << 16);
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

    mask_rx = cm_chanmask_get(STATE_DIR "/cm/chanmask-rx");
    mask_tx = cm_chanmask_get(STATE_DIR "/cm/chanmask-tx");

    sync_mask = (mask_tx << 16) | mask_rx;
    if (0 == sync_mask) {
        return RETURN_SUCCESS;
    }

    sscanf(data, "%lf", &freq);

    sync_mode |= 1; // internal sync
    write_hps_reg("sync_mode", sync_mode);

    write_hps_reg("sync_mask", sync_mask);

    snprintf(inbuf, 256, "%lf", freq);

    for (i = 0; i < NUM_RX_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }

#define X(ch)                                                              \
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

#define X(ch)                                                              \
    if (i == INT_TX(ch))                                                          \
        hdlr = hdlr_tx_##ch##_rf_lo_freq;
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

    mask_rx = cm_chanmask_get(STATE_DIR "/cm/chanmask-rx");
    mask_tx = cm_chanmask_get(STATE_DIR "/cm/chanmask-tx");

    sync_mask = (mask_tx << 16) | mask_rx;
    if (0 == sync_mask) {
        return RETURN_SUCCESS;
    }

    sscanf(data, "%lf", &freq);

    sync_mode |= 1; // internal sync
    write_hps_reg("sync_mode", sync_mode);

    write_hps_reg("sync_mask", sync_mask);

    snprintf(inbuf, 256, "%lf", freq);

    for (i = 0; i < NUM_RX_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }

#define X(ch)                                                              \
    if (i == INT(ch))                                                          \
        hdlr = hdlr_rx_##ch##_dsp_fpga_nco;
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
#define X(ch)                                                              \
    if (i == INT_TX(ch))                                                          \
        hdlr = hdlr_tx_##ch##_dsp_ch0fpga_nco;                                 \
        hdlr = hdlr_tx_##ch##_dsp_ch1fpga_nco;                                 \
        hdlr = hdlr_tx_##ch##_dsp_ch2fpga_nco;                                 \
        hdlr = hdlr_tx_##ch##_dsp_ch3fpga_nco;                                 \
        hdlr = hdlr_tx_##ch##_dsp_ch4fpga_nco;                                 \
        hdlr = hdlr_tx_##ch##_dsp_ch5fpga_nco;                                 \
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
//0 stops all force streaming. To start streaming set this using a value where each bit corresponds to ech channel
//ie 1 to only stream from ch A, 2 for chB, 4 for chC, 5 for chA and chC
//using -1 for streaming all
static int hdlr_cm_rx_force_stream(const char *data, char *ret) {
    int64_t stream = 0;
    sscanf(data, "%li", &stream);
    char path_buffer[MAX_PATH_LEN];
    if(stream != 0) {
        //stop any force streaming by bringing the trigger low
        //force the trigger input to always read as high
        set_property("fpga/trigger/sma_override", "1");
        //sets the sma trigger to activate when it is low (override bit will make it high)
        //the sma trigger should be inactive from here until the end of the function
        set_property("fpga/trigger/sma_pol", "negative");
        // configure the channels specified for force streaming, and ensure others are not
        for(int n = 0; n < NUM_RX_CHANNELS; n++) {
            if(stream & 1 << n) {
                snprintf(path_buffer, MAX_PATH_LEN, "rx/%c/prime_trigger_stream", n+'a');
                set_property(path_buffer, "1");
                snprintf(path_buffer, MAX_PATH_LEN, "rx/%c/trigger/sma_mode", n+'a');
                set_property(path_buffer, "level");
            } else {
                snprintf(path_buffer, MAX_PATH_LEN, "rx/%c/prime_trigger_stream", n+'a');
                set_property(path_buffer, "0");
            }
        }
        //sets the sma to activate when high (sma_override is forcing it high)
        //this starts the streaming for all channels at once
        set_property("fpga/trigger/sma_pol", "positive");
    } else {
        //sets the sma trigger to activate when it is low (override bit will make it high)
        //the sma trigger should be inactive from here until the end of the function
        set_property("fpga/trigger/sma_pol", "negative");
        //stops streaming on everything, note that it does not clean up a lot of the changes done when activating synchronized force streaming
        for(int n = 0; n < NUM_RX_CHANNELS; n++) {
            //stops any existing force streaming
            snprintf(path_buffer, MAX_PATH_LEN, "rx/%c/prime_trigger_stream", n+'a');
            set_property(path_buffer, "0");
        }
        //stop ignoring the trigger input state in case it will be used later
        set_property("fpga/trigger/sma_override", "0");
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
            ping_write_only(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        }

        return RETURN_SUCCESS;
    }

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
    uint64_t frational_time = (time - (uint64_t)time);
    // lower half
    write_hps_reg("sys11", (uint32_t)(((uint64_t)frational_time) & 0x00000000FFFFFFFF));
    // upper half
    write_hps_reg("sys12",
                  (uint32_t)(((uint64_t)frational_time) >> 32) & 0x00000000FFFFFFFF);

    // Toggling this bit sets the time
    write_hps_reg_mask("sys13", 1, 1);
    write_hps_reg_mask("sys13", 0, 1);

    return RETURN_SUCCESS;
}

// Controls both the source of pps (internal vs external) and whether the port is output or input
static int hdlr_time_set_time_source(const char *data, char *ret) {
    uint32_t external;
    if (strcmp(data, "external") == 0) {
        external = 2;
    } else if (strcmp(data, "internal") == 0) {
        external = 0;
    } else {
        PRINT(ERROR, "Invalid argument: '%s'\nValid arguments: external, internal\n", data ? data : "(null)");
        return RETURN_ERROR_PARAM;
    }
    write_hps_reg_mask("sys13", external, 2);
    return RETURN_SUCCESS;
}

// Controls whether the time board LMX2595 that provides the 100MHz reference to the mid and highband RF mixers is enabled
// To track whether we can turn off the clock, we will track this as a 32-bit hex number
// the bottom 16 bits will each correspond to an rf slot, which will be set high by band select, or low by pwr 0
// if any of the top 16 bits are high, the clock will not be turned off
static int hdlr_time_set_time_en_rf_ref(const char *data, char *ret) {
    uint32_t enable;
    if (1 != sscanf(data, "0x%x", &enable)) {
        if (1 != sscanf(data, "%" SCNu32, &enable)) {
            PRINT(ERROR,"Unrecognized channel (%s), persistently enabling clock\n", data);
            enable = 0x10000;
        }
    }

    if (enable) {
        PRINT(VERBOSE,"enable rf_ref\n");
        strcpy(buf, "lmx -l 2 -M 0\r");
    } else {
        PRINT(VERBOSE,"mute rf_ref\n");
        strcpy(buf, "lmx -l 2 -M 1\r");
    }
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));

    snprintf(ret, MAX_PROP_LEN, "0x%08x", enable);
    return RETURN_SUCCESS;
}

// Controls enable for time board LMX2595 providing 1.8GHz IF for highband RF mixers
// To track whether we can disable it, we will track this as a 32-bit hex number
// bottom 16 bits each correspond to an rf slot, set high by band select or low by pwr 0
// if any of the top 16 bits are high, the clock will not be turned off
static int hdlr_time_set_time_en_rf_if(const char *data, char *ret) {
    uint32_t enable;
    if (1 != sscanf(data, "0x%x", &enable)) {
        if (1 != sscanf(data, "%" SCNu32, &enable)) {
            PRINT(ERROR,"Unrecognized channel (%s), persistently enabling clock\n", data);
            enable = 0x10000;
        }
    }
    
    if (enable) {
        PRINT(VERBOSE,"enable rf_if");
        strcpy(buf, "lmx -l 4 -M 0\r");
    } else {
        PRINT(VERBOSE,"mute rf_if");
        strcpy(buf, "lmx -l 4 -M 1\r");
    }
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));

    snprintf(ret, MAX_PROP_LEN, "0x%08x", enable);
    return RETURN_SUCCESS;
}

static int hdlr_time_clk_cur_time(const char *data, char *ret) {
    long double time;
    sscanf(data, "%Lf", &time);
    // Write the number of whole seconds
    // lower half
    write_hps_reg("sys9", (uint32_t)(((uint64_t)time) & 0x00000000FFFFFFFF));
    // upper half
    write_hps_reg("sys10",
                  (uint32_t)(((uint64_t)time) >> 32) & 0x00000000FFFFFFFF);

    // Write the fractional seconds in ticks
    uint64_t frational_time = (time - (uint64_t)time);
    // lower half
    write_hps_reg("sys11", (uint32_t)(((uint64_t)frational_time) & 0x00000000FFFFFFFF));
    // upper half
    write_hps_reg("sys12",
                  (uint32_t)(((uint64_t)frational_time) >> 32) & 0x00000000FFFFFFFF);

    // Toggling this bit sets the time
    write_hps_reg_mask("sys13", 1, 1);
    write_hps_reg_mask("sys13", 0, 1);
    return RETURN_SUCCESS;
}

static int hdlr_time_clk_cmd(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_time_status_good(const char *data, char *ret) {
     strcpy(buf, "status -g\r");
     ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
     snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);

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
        PRINT(INFO, "Setting clock reference source to external\n");
        strcpy(buf, "clk -t 1\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        // Leave the property as what is was set to. Included for clarity, should be redundant
        snprintf(ret, MAX_PROP_LEN, "%s", data);
    } else if (strcmp(data, "internal") == 0) {
        // ensure 10MHz reference is set
        set_property("time/source/freq_mhz", "10");
        PRINT(INFO, "Setting clock reference source to internal\n");
        strcpy(buf, "clk -t 0\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        // Leave the property as what is was set to. Included for clarity, should be redundant
        snprintf(ret, MAX_PROP_LEN, "%s", data);
    } else {  // just get the current state of the reference
        strcpy(buf, "clk -i\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        // save the UART result to the state tree
        strcpy(ret, (char *)uart_ret_buf);
    }
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    // Waits for clock to stabilize
    usleep(1000000);
    if(property_good("time/status/status_good") != 1) {
        PRINT(ERROR, "PLL loss of lock detected after changing clock reference source\n");
        snprintf(ret, MAX_PROP_LEN, "pll_lock_failure");
    }
    return RETURN_SUCCESS;
}

static int hdlr_time_source_freq(const char *data, char *ret) {
    uint16_t freq;
    bool ext = false;
    sscanf(data, "%hu", &freq);

    switch(freq){
        case 5:
            // set to external ref
            ext = true;
            // prep time board command for 5Mhz ref
            strcpy(buf, "clk -f 1\r");
            break;
        case 10:
        default:
            // prep prep time board command for 10MHz ref
            strcpy(buf, "clk -f 0\r");
            // set freq to ensure correct val when written back to state tree
            freq = 10;
            break;
    }
    // send command to time board
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));

    // set to external reference if appropriate
    if (ext) {
        set_property("time/source/ref", "external");
    }

    // write actual frequency back to state tree
    snprintf(ret, MAX_PROP_LEN, "%u", freq);
    return RETURN_SUCCESS;
}

// choose pulsed or continuous SYSREF
static int hdlr_time_sync_sysref_mode(const char *data, char *ret) {
    if ( (strcmp(data, "pulsed") == 0) || (strcmp(data, "0") == 0) ) {
        if(current_sysref_mode == pulsed) {
            PRINT(INFO, "SYSREF MODE 'pulsed' already active.\n");
            return RETURN_SUCCESS;
        } else {
            PRINT(INFO, "SYSREF MODE 'pulsed' selected.\n");
            strcpy(buf, "sync -c 0\r");
            current_sysref_mode = pulsed;
            ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
            usleep(1100000);
            return RETURN_SUCCESS;
        }
    } else if ( (strcmp(data, "continuous") == 0) || (strcmp(data, "1") == 0) )  {
        if(current_sysref_mode == continuous) {
            PRINT(INFO, "SYSREF MODE 'continuous' already active.\n");
            return RETURN_SUCCESS;
        } else {
            PRINT(INFO, "SYSREF MODE 'continuous' selected.\n");
            strcpy(buf, "sync -c 1\r");
            current_sysref_mode = continuous;
            ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
            usleep(1100000);
            return RETURN_SUCCESS;
        }
    } else {
        PRINT(ERROR, "SYSREF MODE must be 'continuous' or 'pulsed'.\n");
        current_sysref_mode = unspecified_sysref;
        return RETURN_ERROR;
    }
}

// Toggle SPI Sync
static int hdlr_time_sync_lmk_sync_tgl_jesd(const char *data, char *ret) {
    if (strcmp(data, "0") != 0) {
        strcpy(buf, "clk -y\r");
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
    strcpy(buf, "status -l\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd0_pll1(const char *data, char *ret) {
    strcpy(buf, "status -l 11\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd0_pll2(const char *data, char *ret) {
    strcpy(buf, "status -l 12\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd1_pll1(const char *data, char *ret) {
    strcpy(buf, "status -l 21\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd1_pll2(const char *data, char *ret) {
    strcpy(buf, "status -l 22\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd2_pll1(const char *data, char *ret) {
    strcpy(buf, "status -l 31\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd2_pll2(const char *data, char *ret) {
    strcpy(buf, "status -l 32\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol(const char *data, char *ret) {
    // strcpy(buf, "status -o\r");
    // ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    // snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd0_pll1(const char *data, char *ret) {
    strcpy(buf, "status -o 11\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd0_pll2(const char *data, char *ret) {
    strcpy(buf, "status -o 12\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd1_pll1(const char *data, char *ret) {
    strcpy(buf, "status -o 21\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd1_pll2(const char *data, char *ret) {
    strcpy(buf, "status -o 22\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd2_pll1(const char *data, char *ret) {
    strcpy(buf, "status -o 31\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd2_pll2(const char *data, char *ret) {
    strcpy(buf, "status -o 32\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}

static int hdlr_time_board_dump(const char *data, char *ret) {
    // Send the uart commands and read back the output and write to file.
    // Diagnostic Dump of Clk Board
    strcpy(buf, "board -e\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));

    snprintf(ret, MAX_PROP_LEN, "%s", (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_board_test(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

// Get temperature results in a crash with the current MCU code
static int hdlr_time_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -t\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_board_led(const char *data, char *ret) {
    snprintf(buf, MAX_PROP_LEN, "board -l %s\r", data);
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_time_about_id(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_time_about_serial(const char *data, char *ret) {
    strcpy(buf, "status -s\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_mcudevid(const char *data, char *ret) {
    strcpy(buf, "status -d\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_mcurev(const char *data, char *ret) {
    strcpy(buf, "status -v\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_mcufuses(const char *data, char *ret) {
    strcpy(buf, "status -f\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_fw_ver(const char *data, char *ret) {
    strcpy(buf, "board -v\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_about_hw_ver(const char *data, char *ret) {
    strcpy(buf, "board -h\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    snprintf(ret, MAX_PROP_LEN, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/* --------------------------------- FPGA ----------------------------------- */
/* -------------------------------------------------------------------------- */

// Dumps all of the board logs for TX, RX, and TIME
static int hdlr_fpga_board_dump(const char *data, char *ret) {
    char data_buff[MAX_PROP_LEN];
    char ret_buff[MAX_PROP_LEN];
#define X(ch) hdlr_tx_##ch##_rf_board_dump(data_buff, ret_buff);
    TX_CHANNELS
#undef X
#define X(ch) hdlr_rx_##ch##_rf_board_dump(data_buff, ret_buff);
    RX_CHANNELS
#undef X
    hdlr_time_board_dump(data_buff, ret_buff);

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
#define X(ch)                                                              \
    ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)), usleep(50000);
        RX_CHANNELS
#undef X

        strcpy(buf, "board -g 1\r");
#define X(ch)                                                              \
    ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)), usleep(50000);
        TX_CHANNELS
#undef X
    }
    if (strcmp(data, "2") == 0) {
        strcpy(buf, "board -g 2\r");
        ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        usleep(50000);

        strcpy(buf, "board -g 2\r");
#define X(ch)                                                              \
    ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)), usleep(50000);
        RX_CHANNELS
#undef X

        strcpy(buf, "board -g 2\r");
#define X(ch)                                                              \
    ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)), usleep(50000);
        TX_CHANNELS
#undef X
    }
    return RETURN_SUCCESS;
}

static int hdlr_fpga_qa_ecc_a_errors(const char *data, char *ret) {
    uint32_t val = 0;
    read_hps_reg("ecc0", &val);

    snprintf(ret, MAX_PROP_LEN, "0x%x\n", val);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_qa_ecc_b_errors(const char *data, char *ret) {
    uint32_t val = 0;
    read_hps_reg("ecc1", &val);

    snprintf(ret, MAX_PROP_LEN, "0x%x\n", val);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_temp(const char *data, char *ret) {
    uint32_t old_val = 0;
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

static int hdlr_fpga_board_rstreq_all_dsp(const char *data, char *ret) {
    // assert reset
    write_hps_reg_mask("res_rw7", 0x80000000, 0x80000000);

    // de-assert reset
    write_hps_reg_mask("res_rw7", 0, 0x80000000);

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

int check_sfp() {
    uint32_t sys18_val = 0;
    read_hps_reg("sys18", &sys18_val);

    // The first 4 bits indicate which sfp ports have cables attatched, the next 4 indicate which links are established
    uint32_t sfp_link_established = (sys18_val >> 24 & 0xf);
    uint32_t sfp_module_present = sys18_val >> 28;
    // The bits used to detect which SFPs are inserted is flipped compared to which SFPs are working
    sfp_module_present = ((sfp_module_present & 0x1) << 3) | ((sfp_module_present & 0x2) << 1) | ((sfp_module_present & 0x4) >> 1) | ((sfp_module_present & 0x8) >> 3);

    if( sfp_module_present == sfp_link_established) {
        PRINT(INFO, "SFP present and established: %x\n", sfp_link_established);
        return RETURN_SUCCESS;
    } else {
        PRINT(ERROR, "SFP link failed to establish with status: %x, re-attempting\n", sys18_val);
        PRINT(ERROR, "SFP established: %x\n", sfp_link_established);
        PRINT(ERROR, "SFP present: %x\n", sfp_module_present);
        return RETURN_ERROR;
    }
}

// 0: do nothing
// 1: reset SFP
// 2: reset SFPs only if they are down
static int hdlr_fpga_link_sfp_reset(const char *data, char *ret) {
    //does not reset if the user write a 0
    int reset = 0;
    sscanf(data, "%i", &reset);
    if(!reset) return RETURN_SUCCESS;

    if(reset == 2) {
        // SFP is up, skip reset
        if(check_sfp() == RETURN_SUCCESS) {
            return RETURN_SUCCESS;
        }
    }

    for(int n = 0; n < sfp_max_reset_attempts; n++) {

        // Reset FPGA starting at SFPs
        set_property("fpga/reset", "2");

        if(check_sfp() == RETURN_SUCCESS) {
            // Reset counter for number of failed boots
            update_interboot_variable("cons_sfp_fail_count", 0);
            return RETURN_SUCCESS;
        }
    }
    PRINT(ERROR, "Failed to establish sfp link after %i attempts rebooting\n", sfp_max_reset_attempts);

    // TEMPORARY: reboot the server if unable to establish JESD links
    int64_t failed_count = 0;
    read_interboot_variable("cons_sfp_fail_count", &failed_count);
    if(failed_count < sfp_max_reboot_attempts) {
        update_interboot_variable("cons_sfp_fail_count", failed_count + 1);
        system("systemctl reboot");
    } else {
        PRINT(ERROR, "Unable to establish SFP links despite multiple reboots. The system will not attempt another reboot to bring up SFP links until a successful attempt to establish links\n");
    }


    return RETURN_ERROR;
}

static int hdlr_fpga_clear_regs(const char *data, char *ret) {
    // Clears the ports for tx regs, the ones in use will be set later in the boot process
    for(int n = 0; n < NUM_DEVICE_SIDE_PORTS; n++) {
        write_hps_reg(device_side_port_map[n], 0);
    }

    // Puts all possible DSPs in reset, the ones in use will be set later
    // Required when using a variant of the server with a lower channel count than the FPGA
    // On most FPGA variants, the DSP related to any JESD not in use must be in reset for any JESD links to establish
    for(int n = 0; n < MAX_POSSIBLE_CHANNELS; n++) {
        write_hps_reg(rx_possible_reg4_map[n], 0x2);
    }
    for(int n = 0; n < MAX_POSSIBLE_CHANNELS; n++) {
        write_hps_reg(tx_possible_reg4_map[n], 0x2);
    }

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_sys_rstreq(const char *data, char *ret) {
    int reboot = 0;
    sscanf(data, "%i", &reboot);

    if (reboot == 0) {
        return RETURN_SUCCESS;
    }
    strcpy(buf, "board -r\r");
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    usleep(700000);

    strcpy(buf, "board -r\r");
#define X(ch)                                                              \
    ping_rx(uart_rx_fd[INT_RX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)), usleep(50000);
    RX_CHANNELS
#undef X

    strcpy(buf, "board -r\r");
#define X(ch)                                                              \
    ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch)), usleep(50000);
    TX_CHANNELS
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
static inline int hdlr_fpga_board_flow_control_sfpc_port(const char *data,
                                                         char *ret) {
    return hdlr_fpga_board_flow_control_sfpX_port(data, ret, 2);
}
static inline int hdlr_fpga_board_flow_control_sfpd_port(const char *data,
                                                         char *ret) {
    return hdlr_fpga_board_flow_control_sfpX_port(data, ret, 3);
}

static int hdlr_fpga_board_fw_rst(const char *data, char *ret) {
    uint32_t old_val = 0;

    read_hps_reg("sys0", &old_val);
    write_hps_reg("sys0", old_val | 0x10);
    write_hps_reg("sys0", old_val & (~0x10));

    return RETURN_SUCCESS;
}

static int hdlr_fpga_about_id(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_fpga_about_cmp_time(const char *data, char *ret) {
    uint32_t old_val = 0;
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
    uint32_t old_val = 0;
    read_hps_reg("sys18", &old_val);
    snprintf(ret, MAX_PROP_LEN, "config. info. 0x%02x \n", old_val);

    return RETURN_SUCCESS;
}

// Note: this is the rate the FPGA build has, not what the server has
// This property exists for FPGA versioning, actual behaviour will depend on what the server was compiled with
static int hdlr_fpga_about_rate(const char *data, char *ret) {
    uint32_t val = 0;
    read_hps_reg("res_ro12", &val);
    val = (val >> 12) & 0xf;
    snprintf(ret, MAX_PROP_LEN, "%u", val);

    return RETURN_SUCCESS;
}

// Note: this is a count of how many rx channels the FPGA build has, not the number of rx channels available
// See system/num_rx for the number of rx channels available
// This property exists for FPGA versioning, and because the same FPGA build is used for different configurations
static int hdlr_fpga_about_num_rx(const char *data, char *ret) {
    uint32_t val = 0;
    read_hps_reg("res_ro12", &val);
    val = (val >> 16) & 0xf;
    snprintf(ret, MAX_PROP_LEN, "%u", val);

    return RETURN_SUCCESS;
}

// Note: this is a count of how many tx channels the FPGA build has, not the number of rx channels available
// See system/num_rx for the number of rx channels available
// This property exists for FPGA versioning, and because the same FPGA build is used for different configurations
static int hdlr_fpga_about_num_tx(const char *data, char *ret) {
    uint32_t val = 0;
    read_hps_reg("res_ro12", &val);
    val = (val >> 20) & 0xf;
    snprintf(ret, MAX_PROP_LEN, "%u", val);

    return RETURN_SUCCESS;
}

// Note: this is the rtm the FPGA was compiled for not the server
// This is for checking the FPGA version only, the server will act based on the RTM set at its compile time
static int hdlr_fpga_about_rtm(const char *data, char *ret) {
    uint32_t val = 0;
    read_hps_reg("res_ro12", &val);
    val = (val >> 24) & 0xf;
    if(val != RTM_VER) {
        PRINT(ERROR, "RTM version mismatch. Server was compiled for RTM %i but the FPGA was compiled for RTM %u\n", RTM_VER, val);
    }

    snprintf(ret, MAX_PROP_LEN, "%u", val);

    return RETURN_SUCCESS;
}

// Whether the FPGA is compiled for a 1G or 3G backplane
// Usually 1G uses a 1G backplane or 3G uses a 3G backplane, but it is possible to modify 3G to be used as 1G
static int hdlr_fpga_about_backplane_pinout(const char *data, char *ret) {
    uint32_t val = 0;
    read_hps_reg("res_ro12", &val);
    val = (val >> 28) & 0x3;

    snprintf(ret, MAX_PROP_LEN, "%u", val);

    return RETURN_SUCCESS;
}

// Indicates if FPGA is hps only. hps only mode is only used for development or in the main image fails
// HPS builds are a minimal build meant to be able to boot into Linux and nother else
static int hdlr_fpga_about_hps_only(const char *data, char *ret) {
    snprintf(ret, MAX_PROP_LEN, "%u", is_hps_only());
    return RETURN_SUCCESS;
}

// if 1 DDR is used in this FPGA build, 0 indicates FIFO only
// FIFO is used when DDR can't keep up, but has very little space
// This is for FPGA versioning only, use system/get_max_buffer_level to get the maximum buffer level (which is what matters to external programs)
static int hdlr_fpga_about_ddr_used(const char *data, char *ret) {

    snprintf(ret, MAX_PROP_LEN, "%u", is_ddr_used());

    return RETURN_SUCCESS;
}

// Gets the speed grade of the FPGA (HU1, HU2, HU3)
static int hdlr_fpga_about_speed_grade(const char *data, char *ret) {

    uint32_t val = 0;
    read_hps_reg("res_ro16", &val);

    snprintf(ret, MAX_PROP_LEN, "%u", val);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_about_serial(const char *data, char *ret) {
    uint32_t chip_id_msb = 0;
    uint32_t chip_id_lsb = 0;

    // Load the chip ID
    write_hps_reg_mask("res_rw7", ~0, 1 << 24);
    write_hps_reg_mask("res_rw7", 0, 1 << 24);

    // Read the chip ID with 100ms timeout (1000 * 100us)
    for(int n = 0; n < 1000 && (!chip_id_msb || !chip_id_lsb); n++) {
        usleep(100);
        read_hps_reg("sys16", &chip_id_lsb);
        read_hps_reg("sys17", &chip_id_msb);
    }

    // Append values
    snprintf(ret, MAX_PROP_LEN, "%02x%02x\n", chip_id_msb, chip_id_lsb);

    return RETURN_SUCCESS;
}

// Does nothing on Cyan since there are seperate input and output pins
static int hdlr_fpga_trigger_sma_dir(const char *data, char *ret) {
    int r;
    bool val = 0;
    r = valid_trigger_dir(data, &val);
    if(r != RETURN_SUCCESS) return r;
    else {
        r = set_sma_dir(val);
    }
    return r;
}

static int hdlr_fpga_trigger_sma_pol(const char *data, char *ret) {
    int r;
    bool val = 0;
    r = valid_trigger_pol(data, &val);
    if(r != RETURN_SUCCESS) return r;
    else {
        r = set_sma_pol(val);
    }
    return r;
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

static int hdlr_fpga_about_sw_ver(const char *data, char *ret) {
    /* FPGA version info
     * We just read this from the FPGA and store it in the state tree.
     * per issue 8871:
     * wRsvdROReg12[ 0] = ECC_ENABLED_DDRA; (0 or 1, 1 if enabled)
     * wRsvdROReg12[11: 4] = XG_ETH ; (100 or 40 0x64 or 0x28)
     * wRsvdROReg12[13:12] = JESD_THROUGHPUT ; (1 or 3 GSPS)
     * wRsvdROReg12[31:14] = 18'h1c0de ; Reserved value that never changes, If this is 00000, that means the FPGA is older than when this register was implemented
     */
    uint32_t reg_val;
    read_hps_reg("res_ro12", &reg_val);
    snprintf(ret, MAX_PROP_LEN, "0x%08x\n", reg_val);
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
    FILE *i2c_return;
    char i2c_value[512];

    i2c_return = popen("cat /sys/bus/i2c/devices/1-0050/eeprom", "r");
    fgets(i2c_value, sizeof(i2c_value), i2c_return);
    pclose(i2c_return);
    snprintf(ret, MAX_PROP_LEN, "%s",i2c_value);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_rate(const char *data, char *ret) {
    snprintf(ret, MAX_PROP_LEN, "%li", get_network_speed()*(int64_t)1000000000);
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
    uint32_t old_val = 0;
    uint8_t ver = 0;
    sscanf(data, "%" SCNd8 "", &ver);
    read_hps_reg("net0", &old_val);
    if (ver > 0)
        write_hps_reg("net0", (old_val | 0x4));
    else
        write_hps_reg("net0", (old_val & ~(0x4)));
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpa_pay_len(const char *data, char *ret) {
    uint32_t pay_len = 0;
    sscanf(data, "%" SCNd32 "", &pay_len);

    uint32_t actual_pay_len = set_payload_len("net0", 16, pay_len);

    snprintf(ret, MAX_PROP_LEN, "%u\n", actual_pay_len);
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
    uint32_t old_val = 0;
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
    uint32_t pay_len;
    sscanf(data, "%" SCNd32 "", &pay_len);

    uint32_t actual_pay_len = set_payload_len("net15", 16, pay_len);

    snprintf(ret, MAX_PROP_LEN, "%u\n", actual_pay_len);
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
    uint32_t old_val = 0;
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
    uint32_t pay_len;
    sscanf(data, "%" SCNd32 "", &pay_len);

    uint32_t actual_pay_len = set_payload_len("net30", 16, pay_len);

    snprintf(ret, MAX_PROP_LEN, "%u\n", actual_pay_len);
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
    uint32_t old_val = 0;
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
    uint32_t pay_len;
    sscanf(data, "%" SCNd32 "", &pay_len);

    uint32_t actual_pay_len = set_payload_len("net45", 16, pay_len);

    snprintf(ret, MAX_PROP_LEN, "%u\n", actual_pay_len);
    return RETURN_SUCCESS;
}





static int hdlr_fpga_link_net_dhcp_en(const char *data, char *ret) {
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_net_hostname(const char *data, char *ret) {
    char name[MAX_PROP_LEN] = {0};
    char command[MAX_PROP_LEN] = {0};
    sscanf(data, "%s", name);

    snprintf(command, MAX_PROP_LEN, "echo %s > /etc/hostname", name);
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
    snprintf(command, MAX_PROP_LEN, "sed -r -i 's/(\\b[0-9]{1,3}\\.){3}[0-9]{1,3}\\b'/%s/ /etc/init.d/mcu_init.sh", ip_address);
    system(command);
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_rx_sample_bandwidth(const char *data, char *ret) {
    int result = 0;
    for(int chan = 0; chan < NUM_RX_CHANNELS; chan++) {
        char prop_path[PROP_PATH_LEN];
        snprintf(prop_path, PROP_PATH_LEN, "rx/%c/dsp/rx_sample_bandwidth", chan +'a');
        set_property(prop_path, data);
        char reply[50];
        get_property(prop_path, reply, sizeof(reply));
        int reply_i;
        sscanf(reply, "%i", &reply_i);
        if(chan != 0 && reply_i != result) {
            PRINT(ERROR, "Unable to set wire format correctly. Assuming ch A is correct\n");
        } else {
            result = reply_i;
        }
    }

    snprintf(ret, MAX_PROP_LEN, "%i", result);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_tx_sample_bandwidth(const char *data, char *ret) {
    int result = 0;
    for(int chan = 0; chan < NUM_RX_CHANNELS; chan++) {
        char prop_path[PROP_PATH_LEN];
        snprintf(prop_path, PROP_PATH_LEN, "tx/%c/dsp/tx_sample_bandwidth", chan +'a');
        set_property(prop_path, data);
        char reply[50];
        get_property(prop_path, reply, sizeof(reply));
        int reply_i;
        sscanf(reply, "%i", &reply_i);
        if(chan != 0 && reply_i != result) {
            PRINT(ERROR, "Unable to set wire format correctly. Assuming ch A is correct\n");
        } else {
            result = reply_i;
        }
    }

    snprintf(ret, MAX_PROP_LEN, "%i", result);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gps_time(const char *data, char *ret) {
    uint32_t gps_time_lh = 0, gps_time_uh = 0;

    read_hps_reg("sys5", &gps_time_lh);
    read_hps_reg("sys6", &gps_time_uh);

    snprintf(ret, MAX_PROP_LEN, "%i%i", gps_time_uh, gps_time_lh);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gps_frac_time(const char *data, char *ret) {
    uint32_t gps_frac_time_lh = 0, gps_frac_time_uh = 0;
    read_hps_reg("sys7", &gps_frac_time_lh);
    read_hps_reg("sys8", &gps_frac_time_uh);

    uint64_t gps_frac_time = (uint64_t) gps_frac_time_lh + (((uint64_t) gps_frac_time_uh) * 1000000000);

    snprintf(ret, MAX_PROP_LEN, "%lu", gps_frac_time);

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
    write_hps_reg_mask("sys13", 1, 1); // Writing 1, then 0 to sys9 sets the time
    write_hps_reg_mask("sys13", 0, 1); // to what is written in sys7 and sys8

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
    uint8_t led_state = get_led_state();

    sscanf(data, "%i", &reset_type);

    if (reset_type == 0) {
        // No reset requested
    }
    else if (reset_type == 1){       // global reset bit 30
        write_hps_reg_mask("res_rw7", (1 << 30), (1 << 30));
        write_hps_reg_mask("res_rw7", 0, (1 << 30));
    }
    else if (reset_type == 2) { // 40G reset bit 29
        write_hps_reg_mask("res_rw7", (1 << 29), (1 << 29));
        write_hps_reg_mask("res_rw7", 0, (1 << 29));
    }
    else if (reset_type == 3) { // JESD reset bit 28
        write_hps_reg_mask("res_rw7", (1 << 28), (1 << 28));
        write_hps_reg_mask("res_rw7", 0, (1 << 28));
    }
    else if (reset_type == 4) { // DSP reset bit 27
        write_hps_reg_mask("res_rw7", (1 << 27), (1 << 27));
        write_hps_reg_mask("res_rw7", 0, (1 << 27));
    } else {
        PRINT(ERROR, "Invalid reset request: %i\n", reset_type);
        reset_type = -1;
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
    // Waits for the reset sequence to finish
    wait_for_fpga_reset();

    // Resetting the FPGA results in user visible LEDs being reset to their
    // default value. This sets it back to the state it was in before the FPGA reset.
    set_led_state(led_state);

    snprintf(ret, MAX_PROP_LEN, "%u\n", reset_type);
    return RETURN_SUCCESS;
}

static int hdlr_system_get_max_buffer_level(const char *data, char *ret) {
    uint32_t max_buffer_level = 0;
    read_hps_reg("res_ro14", &max_buffer_level);
    snprintf(ret, MAX_PROP_LEN, "%u", max_buffer_level);

    return RETURN_SUCCESS;
}

static int hdlr_system_get_buffer_level_multiple(const char *data, char *ret) {
    uint32_t buffer_level_multiple = 0;
    read_hps_reg("res_ro15", &buffer_level_multiple);
    snprintf(ret, MAX_PROP_LEN, "%u", buffer_level_multiple);

    return RETURN_SUCCESS;
}

// Performs self calibration, not required by most versions
// Note: may also require UHD to perform tasks
// Intended flow:
// UHD checks this property, if non-zero skip self calibration since it has already been done/not required
// UHD performs the self calibration process, and this property gets set to 1
// Currently, UHD controls the entire self-calibration process and this just stores whether or not it has been performed
static int hdlr_system_self_calibration(const char *data, char *ret) {
    int self_calibration_completed = 0;
    sscanf(data, "%i", &self_calibration_completed);

    if(self_calibration_completed) {
        snprintf(ret, MAX_PROP_LEN, "%i", 1);
    } else {
        snprintf(ret, MAX_PROP_LEN, "%i", 0);
    }

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

#define PROJECT_NAME "cyan_nrnt"

//defines the file prop using the new (2021-10-19) method of deciding whether or not to turn the board on first
//everything using DEFINE_FILE_PROP will set rx/pwr or tx/pwr to 1 for anything contianing rx or tx in the path
//Things using this will only turn on rx or tx if specified in the define
//See properties.h for the corresponding constants (prop_pwr_t)
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

#define DEFINE_RX_WAIT_PWR(_c) \
    DEFINE_FILE_PROP_P("rx/" #_c "/board/wait_async_pwr", hdlr_rx_##_c##_wait_async_pwr, RW, "0", SP, #_c)\
    DEFINE_FILE_PROP_P("rx/" #_c "/jesd/unmask"              , hdlr_rx_##_c##_jesd_mask,            RW, "0", SP, #_c)

#define DEFINE_RX_BOARD_PWR(_c) \
    DEFINE_FILE_PROP_P("rx/" #_c "/board/pwr_board"       , hdlr_rx_##_c##_pwr_board,               RW, "0", SP, #_c)\

#define DEFINE_RX_INVERT_DEV_CLOCK(_c) \
    DEFINE_FILE_PROP_P("rx/" #_c "/jesd/invert_devclk"     , hdlr_rx_##_c##_invert_devclk,             RW, "0", SP, #_c)\

#define DEFINE_RX_PWR_REBOOT(_c)    \
    /*async_pwr_board is initializeed with a default value of on after pwr board is initialized with off to ensure the board is off at the start*/\
    DEFINE_FILE_PROP_P("rx/" #_c "/board/async_pwr"        , hdlr_rx_##_c##_async_pwr_board,         RW, "1", SP, #_c)   \
    /* Mask sysref whenever not resetting JESD or powering on */\
    DEFINE_FILE_PROP_P("rx/" #_c "/reboot"                 , hdlr_rx_##_c##_reboot,                  RW, "0", SP, #_c)\
    /* This is defined here because it is set by a meta property in the FPGA section*/\
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/rx_sample_bandwidth" , hdlr_rx_##_c##_dsp_sample_bandwidth, RW, S_DEAULT_OTW_RX, SP, #_c)\


#define DEFINE_RX_CHANNEL(_c)                                                                                         \
    DEFINE_SYMLINK_PROP("rx_" #_c, "rx/" #_c)                                                                         \
    DEFINE_FILE_PROP_P("rx/" #_c "/jesd/status"            , hdlr_rx_##_c##_jesd_status,             RW, "bad", SP, #_c)\
    DEFINE_FILE_PROP_P("rx/" #_c "/jesd/reset"             , hdlr_rx_##_c##_jesd_reset,             RW, "0", SP, #_c)\
    DEFINE_FILE_PROP_P("rx/" #_c "/jesd/error"             , hdlr_rx_##_c##_jesd_error,             RW, "0", SP, #_c)\
    DEFINE_FILE_PROP_P("rx/" #_c "/pwr"                    , hdlr_rx_##_c##_pwr,                     RW, "1", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/jesd/pll_locked"          , hdlr_rx_##_c##_jesd_pll_locked,         RW, "poke", SP, #_c)      \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/id"                 , hdlr_rx_##_c##_about_id,                RW, "001", RP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/serial"             , hdlr_rx_##_c##_about_serial,            RW, "001", RP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/mcudevid"           , hdlr_rx_##_c##_about_mcudevid,          RW, "001", RP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/mcurev"             , hdlr_rx_##_c##_about_mcurev,            RW, "001", RP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/mcufuses"           , hdlr_rx_##_c##_about_mcufuses,          RW, "001", RP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/fw_ver"             , hdlr_rx_##_c##_about_fw_ver,            RW, VERSION, RP, #_c)     \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/hw_ver"             , hdlr_rx_##_c##_about_hw_ver,            RW, VERSION, RP, #_c)     \
    DEFINE_FILE_PROP_P("rx/" #_c "/about/sw_ver"             , hdlr_rx_##_c##_about_sw_ver,            RW, VERSION, RP, #_c)     \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/sma_mode"         , hdlr_rx_##_c##_trigger_sma_mode,        RW, "level", SP, #_c)     \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/trig_sel"         , hdlr_rx_##_c##_trigger_trig_sel,        RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/edge_backoff"     , hdlr_rx_##_c##_trigger_edge_backoff,    RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/edge_sample_num"  , hdlr_rx_##_c##_trigger_edge_sample_num, RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/ufl_mode"         , hdlr_rx_##_c##_trigger_ufl_mode,        RW, "level", SP, #_c)     \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/ufl_dir"          , hdlr_rx_##_c##_trigger_ufl_dir,         RW, "out", SP, #_c)       \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/ufl_pol"          , hdlr_rx_##_c##_trigger_ufl_pol,         RW, "negative", SP, #_c)  \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/time_disable"     , hdlr_rx_##_c##_trigger_time_disable,    RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/trigger/time_gate_logic"  , hdlr_rx_##_c##_trigger_time_gate_logic, RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/stream"                   , hdlr_rx_##_c##_stream,                  RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/freq/val"              , hdlr_rx_##_c##_rf_freq_val,             RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/freq/lna"              , hdlr_rx_##_c##_rf_freq_lna,             RW, "1", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/link/iq_swap"             , hdlr_rx_##_c##_link_iq_swap,            RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/freq/band"             , hdlr_rx_##_c##_rf_freq_band,            RW, "-1", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/gain/ampl"             , hdlr_rx_##_c##_rf_gain_ampl,             RW, "0", RP, #_c)        \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/gain/adc_digital"      , hdlr_rx_##_c##_rf_gain_adc_digital,     RW, "-1", RP, #_c)        \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/gain/val"              , hdlr_rx_##_c##_rf_gain_val,             RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/atten/val"             , hdlr_rx_##_c##_rf_atten_val,            RW, "31", RP, #_c)        \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_gaincor"         , hdlr_rx_##_c##_iq_gain_correction,      RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_phasecor"        , hdlr_rx_##_c##_iq_phase_correction,     RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_dcoffset_i"      , hdlr_rx_##_c##_iq_dco_i,                RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_dcoffset_q"      , hdlr_rx_##_c##_iq_dco_q,                RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_hd2ix"           , hdlr_rx_##_c##_iq_hd2_ix,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_hd2iy"           , hdlr_rx_##_c##_iq_hd2_iy,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_hd2qx"           , hdlr_rx_##_c##_iq_hd2_qx,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_hd2qy"           , hdlr_rx_##_c##_iq_hd2_qy,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_hd3ix"           , hdlr_rx_##_c##_iq_hd3_ix,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_hd3iy"           , hdlr_rx_##_c##_iq_hd3_iy,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_hd3qx"           , hdlr_rx_##_c##_iq_hd3_qx,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_hd3qy"           , hdlr_rx_##_c##_iq_hd3_qy,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_im2ix"           , hdlr_rx_##_c##_iq_im2_ix,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_im2qx"           , hdlr_rx_##_c##_iq_im2_qx,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_im3ix"           , hdlr_rx_##_c##_iq_im3_ix,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_im3iy"           , hdlr_rx_##_c##_iq_im3_iy,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_im3qx"           , hdlr_rx_##_c##_iq_im3_qx,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_im3qy"           , hdlr_rx_##_c##_iq_im3_qy,               RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_ip3cc"           , hdlr_rx_##_c##_iq_ip3cc,                RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/rf/iq/iq_ip3ic"           , hdlr_rx_##_c##_iq_ip3ic,                RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/status/rfpll_lock"        , hdlr_rx_##_c##_status_rfld,             RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/status/adc_alarm"         , hdlr_rx_##_c##_status_adcalarm,         RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/status/lna"               , hdlr_rx_##_c##_status_lna,              RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/board/dump"               , hdlr_rx_##_c##_rf_board_dump,           RW, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/board/test"               , hdlr_rx_##_c##_rf_board_test,           WO, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/board/temp"               , hdlr_rx_##_c##_rf_board_temp,           RW, "20", RP, #_c)        \
    DEFINE_FILE_PROP_P("rx/" #_c "/board/led"                , hdlr_rx_##_c##_rf_board_led,            WO, "0", RP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/signed"               , hdlr_rx_##_c##_dsp_signed,              RW, "1", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/gain"                 , hdlr_rx_##_c##_dsp_gain,                RW, "255", SP, #_c)        \
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/rate"                 , hdlr_rx_##_c##_dsp_rate,                RW, "1258850", SP, #_c)   \
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/nco_adj"              , hdlr_rx_##_c##_dsp_fpga_nco,            RW, "-15000000", SP, #_c) \
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/rstreq"               , hdlr_rx_##_c##_dsp_rstreq,              WO, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/dsp/loopback"             , hdlr_rx_##_c##_dsp_loopback,            RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/link/vita_en"             , hdlr_rx_##_c##_link_vita_en,            RW, "1", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/link/iface"               , hdlr_rx_##_c##_link_iface,              RW, "sfpa", SP, #_c)      \
    DEFINE_FILE_PROP_P("rx/" #_c "/link/port"                , hdlr_rx_##_c##_link_port,               RW, "0", SP, #_c)         \
    /* Note: the registers for rx src port and tx dst port overlap but don't match perfectly*/\
    /* This property exists to make sure the rx src port is not 0, do not expose it in UHD until a solution to the above problem is implemented */\
    DEFINE_FILE_PROP_P("rx/" #_c "/link/src_port"            , hdlr_rx_##_c##_link_src_port,           RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/link/ip_dest"             , hdlr_rx_##_c##_link_ip_dest,            RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/link/mac_dest"            , hdlr_rx_##_c##_link_mac_dest,           RW, "ff:ff:ff:ff:ff:ff", SP, #_c)\
    DEFINE_FILE_PROP_P("rx/" #_c "/link/jesd_num"            , hdlr_invalid,                                   RO, "0", SP, #_c)\
    DEFINE_FILE_PROP_P("rx/" #_c "/prime_trigger_stream"     , hdlr_rx_##_c##_prime_trigger_stream,                           RW, "0", RP, #_c)\
    DEFINE_FILE_PROP_P("rx/" #_c "/jesd/delay_iq"            , hdlr_rx_##_c##_jesd_delay_iq,            RW, "0 0", SP, #_c)         \
    DEFINE_FILE_PROP_P("rx/" #_c "/link/endian_swap"            , hdlr_rx_##_c##_endian_swap,            RW, "0", SP, #_c)         \

#define DEFINE_TX_WAIT_PWR(_c) \
    DEFINE_FILE_PROP_P("tx/" #_c "/board/wait_async_pwr", hdlr_tx_##_c##_wait_async_pwr, RW, "0", SP, #_c)

#define DEFINE_TX_BOARD_PWR(_c) \
    DEFINE_FILE_PROP_P("tx/" #_c "/board/pwr_board"                , hdlr_tx_##_c##_pwr_board,                     RW, "0", SP, #_c)   \

#define DEFINE_TX_PWR_REBOOT(_c)    \
    /*async_pwr_board is initializeed with a default value of on after pwr board is initialized with off to ensure the board is off at the start*/\
    DEFINE_FILE_PROP_P("tx/" #_c "/board/async_pwr"          , hdlr_tx_##_c##_async_pwr_board,      RW, "1", SP, #_c)   \
    DEFINE_FILE_PROP_P("tx/" #_c "/reboot"                   , hdlr_tx_##_c##_reboot,                  RW, "0", SP, #_c)\
    /* This is defined here because it is set by a meta property in the FPGA section*/\
    DEFINE_FILE_PROP_P("tx/" #_c "/dsp/tx_sample_bandwidth" , hdlr_tx_##_c##_dsp_sample_bandwidth, RW, S_DEAULT_OTW_TX, SP, #_c)


#define DEFINE_TX_CHANNEL(_c)                                                                                         \
    DEFINE_SYMLINK_PROP("tx_" #_c, "tx/" #_c)                                                                         \
    DEFINE_FILE_PROP_P("tx/" #_c "/jesd/status"            , hdlr_tx_##_c##_jesd_status,             RW, "bad", SP, #_c)   \
    DEFINE_FILE_PROP_P("tx/" #_c "/jesd/reset"             , hdlr_tx_##_c##_jesd_reset,              RW, "0", SP, #_c)     \
    DEFINE_FILE_PROP_P("tx/" #_c "/dsp/rstreq"               , hdlr_tx_##_c##_dsp_rstreq,              WO, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/pwr"                    , hdlr_tx_##_c##_pwr,                     RW, "1", SP, #_c)     \
    DEFINE_FILE_PROP_P("tx/" #_c "/jesd/pll_locked"          , hdlr_tx_##_c##_jesd_pll_locked,         RW, "poke", SP, #_c)      \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/sma_mode"         , hdlr_tx_##_c##_trigger_sma_mode,        RW, "level", SP, #_c)     \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/trig_sel"         , hdlr_tx_##_c##_trigger_trig_sel,        RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/edge_backoff"     , hdlr_tx_##_c##_trigger_edge_backoff,    RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/edge_sample_num"  , hdlr_tx_##_c##_trigger_edge_sample_num, RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/ufl_dir"          , hdlr_tx_##_c##_trigger_ufl_dir,         RW, "out", SP, #_c)       \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/ufl_mode"         , hdlr_tx_##_c##_trigger_ufl_mode,        RW, "level", SP, #_c)     \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/ufl_pol"          , hdlr_tx_##_c##_trigger_ufl_pol,         RW, "negative", SP, #_c)  \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/time_disable"     , hdlr_tx_##_c##_trigger_time_disable,    RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/time_gate_logic"  , hdlr_tx_##_c##_trigger_time_gate_logic, RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/trigger/gating"           , hdlr_tx_##_c##_trigger_gating,          RW, "output", SP, #_c)    \
    DEFINE_FILE_PROP_P("tx/" #_c "/link/vita_en"             , hdlr_tx_##_c##_link_vita_en,            RW, "1", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/link/iface"               , hdlr_tx_##_c##_link_iface,              RW, "sfpa", SP, #_c)      \
    DEFINE_FILE_PROP_P("tx/" #_c "/link/port"                , hdlr_tx_##_c##_link_port,               RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/link/iq_swap"             , hdlr_tx_##_c##_link_iq_swap,            RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/qa/fifo_lvl"              , hdlr_tx_##_c##_qa_fifo_lvl,             RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/qa/oflow"                 , hdlr_tx_##_c##_qa_oflow,                RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/qa/uflow"                 , hdlr_tx_##_c##_qa_uflow,                RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/dsp/gain"                 , hdlr_tx_##_c##_dsp_gain,                RW, "127", SP, #_c)        \
    DEFINE_FILE_PROP_P("tx/" #_c "/dsp/rate"                 , hdlr_tx_##_c##_dsp_rate,                RW, "1258850", SP, #_c)   \
    DEFINE_FILE_PROP_P("tx/" #_c "/dsp/fpga_nco"             , hdlr_tx_##_c##_dsp_fpga_nco,            RW, "0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/dsp/all_nco"              , hdlr_tx_##_c##_dsp_all_nco,             RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/dac/nco/dacfreq"       , hdlr_tx_##_c##_dac_nco_freq,            RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/dac/nco/chfreq"        , hdlr_tx_##_c##_ch_nco_freq,             RW, "0", TP, #_c)         \
    DEFINE_SYMLINK_PROP("tx/" #_c "/rf/dac/nco/all_nco", "tx/" #_c "/dsp/all_nco")                                    \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/dac/gain/atten"        , hdlr_tx_##_c##_dac_gain_atten,          RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/band"                  , hdlr_tx_##_c##_rf_band,                 RW, "-1", TP, #_c)        \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/atten"                 , hdlr_tx_##_c##_rf_atten,                RW, "31", TP, #_c)        \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/gain/val"              , hdlr_tx_##_c##_rf_gain_val,             RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/rf/lo_freq"               , hdlr_tx_##_c##_rf_lo_freq,              RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/id"                 , hdlr_tx_##_c##_about_id,                RW, "001", TP, #_c)       \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/serial"             , hdlr_tx_##_c##_about_serial,            RW, "001", TP, #_c)       \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/mcudevid"           , hdlr_tx_##_c##_about_mcudevid,          RW, "001", TP, #_c)       \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/mcurev"             , hdlr_tx_##_c##_about_mcurev,            RW, "001", TP, #_c)       \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/mcufuses"           , hdlr_tx_##_c##_about_mcufuses,          RW, "001", TP, #_c)       \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/fw_ver"             , hdlr_tx_##_c##_about_fw_ver,            RW, VERSION, TP, #_c)     \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/hw_ver"             , hdlr_tx_##_c##_about_hw_ver,            RW, VERSION, TP, #_c)     \
    DEFINE_FILE_PROP_P("tx/" #_c "/about/sw_ver"             , hdlr_tx_##_c##_about_sw_ver,            RW, VERSION, TP, #_c)     \
    DEFINE_FILE_PROP_P("tx/" #_c "/board/temp"               , hdlr_tx_##_c##_rf_board_temp,           RW, "23", TP, #_c)        \
    DEFINE_FILE_PROP_P("tx/" #_c "/status/rfpll_lock"        , hdlr_tx_##_c##_status_rfld,             RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/status/dacpll_lock"       , hdlr_tx_##_c##_status_dacld,            RW, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/board/led"                , hdlr_tx_##_c##_rf_board_led,            WO, "0", TP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/board/dump"               , hdlr_tx_##_c##_rf_board_dump,           RW, "0", TP, #_c)\
    DEFINE_FILE_PROP_P("tx/" #_c "/jesd/delay_iq"            , hdlr_tx_##_c##_jesd_delay_iq,            RW, "0 0", SP, #_c)         \
    DEFINE_FILE_PROP_P("tx/" #_c "/link/endian_swap"            , hdlr_tx_##_c##_endian_swap,            RW, "0", SP, #_c)
//    DEFINE_FILE_PROP_P("tx/" #_c "/rf/dac/temp"              , hdlr_tx_##_c##_rf_dac_temp,             RW, "0")
//    DEFINE_FILE_PROP_P("tx/" #_c "/board/test"               , hdlr_tx_##_c##_rf_board_test,           WO, "0")

#define DEFINE_TIME()                                                                                                 \
    DEFINE_FILE_PROP_P("time/reboot"                         , hdlr_time_reboot,                       RW, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/clk/pps"                        , hdlr_time_clk_pps,                      RW, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/clk/cur_time"                   , hdlr_time_clk_cur_time,                 RW, "0.0", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/clk/cmd"                        , hdlr_time_clk_cmd,                      RW, "0.0", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/status/lmk_lockdetect"          , hdlr_time_status_ld,                    RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lossoflock"          , hdlr_time_status_lol,                   RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lockdetect_jesd0_pll1", hdlr_time_status_ld_jesd0_pll1,        RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lockdetect_jesd0_pll2", hdlr_time_status_ld_jesd0_pll2,        RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lockdetect_jesd1_pll1", hdlr_time_status_ld_jesd1_pll1,        RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lockdetect_jesd1_pll2", hdlr_time_status_ld_jesd1_pll2,        RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lockdetect_jesd2_pll1", hdlr_time_status_ld_jesd2_pll1,        RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lockdetect_jesd2_pll2", hdlr_time_status_ld_jesd2_pll2,        RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lossoflock_jesd0_pll1", hdlr_time_status_lol_jesd0_pll1,       RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lossoflock_jesd0_pll2", hdlr_time_status_lol_jesd0_pll2,       RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lossoflock_jesd1_pll1", hdlr_time_status_lol_jesd1_pll1,       RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lossoflock_jesd1_pll2", hdlr_time_status_lol_jesd1_pll2,       RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lossoflock_jesd2_pll1", hdlr_time_status_lol_jesd2_pll1,       RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/status/lmk_lossoflock_jesd2_pll2", hdlr_time_status_lol_jesd2_pll2,       RW, "poke", SP, NAC)      \
    DEFINE_FILE_PROP_P("time/source/ref"                     , hdlr_time_source_ref,                   RW, "0", SP, NAC)  \
    DEFINE_FILE_PROP_P("time/source/freq_mhz"                 , hdlr_time_source_freq,                 RW, "10", SP, NAC)  \
    DEFINE_FILE_PROP_P("time/source/set_time_source"        , hdlr_time_set_time_source,               RW, "internal", SP, NAC)  \
    DEFINE_FILE_PROP_P("time/source/enable_rf_ref"           , hdlr_time_set_time_en_rf_ref,           RW, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/source/enable_rf_if"            , hdlr_time_set_time_en_rf_if,            RW, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/sync/sysref_mode"             , hdlr_time_sync_sysref_mode,             RW, "continuous", SP, NAC)   \
    DEFINE_FILE_PROP_P("time/sync/lmk_sync_tgl_jesd"         , hdlr_time_sync_lmk_sync_tgl_jesd,       WO, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/sync/lmk_sync_resync_jesd"      , hdlr_time_sync_lmk_resync_jesd,         WO, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/sync/lmk_resync_all"            , hdlr_time_sync_lmk_resync_all,          WO, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/board/dump"                     , hdlr_time_board_dump,                   RW, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/board/test"                     , hdlr_time_board_test,                   WO, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/board/led"                      , hdlr_time_board_led,                    WO, "0", SP, NAC)         \
    DEFINE_FILE_PROP_P("time/about/id"                       , hdlr_time_about_id,                     RO, "001", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/about/serial"                   , hdlr_time_about_serial,                 RW, "001", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/about/mcudevid"                 , hdlr_time_about_mcudevid,               RW, "001", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/about/mcurev"                   , hdlr_time_about_mcurev,                 RW, "001", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/about/mcufuses"                 , hdlr_time_about_mcufuses,               RW, "001", SP, NAC)       \
    DEFINE_FILE_PROP_P("time/about/fw_ver"                   , hdlr_time_about_fw_ver,                 RW, VERSION, SP, NAC)     \
    DEFINE_FILE_PROP_P("time/about/hw_ver"                   , hdlr_time_about_hw_ver,                 RW, VERSION, SP, NAC)     \
    DEFINE_FILE_PROP_P("time/about/sw_ver"                   , hdlr_invalid,                           RO, VERSION, SP, NAC)\
    DEFINE_FILE_PROP_P("time/status/status_good"             , hdlr_time_status_good,                  RW, "bad", SP, NAC)\
    DEFINE_FILE_PROP_P("time/board/temp"                     , hdlr_time_board_temp,                   RW, "0", SP, NAC)

#define DEFINE_FPGA()                                                                                                         \
    DEFINE_FILE_PROP_P("fpga/reset"                          , hdlr_fpga_reset,                        RW, "1", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/link/sfp_reset"                 , hdlr_fpga_link_sfp_reset,               RW, "2", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/clear_tx_ports"                 , hdlr_fpga_clear_regs,          RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/user/regs"                      , hdlr_fpga_user_regs,                    RW, "0.0", SP, NAC)               \
    DEFINE_FILE_PROP_P("fpga/trigger/sma_dir"                , hdlr_fpga_trigger_sma_dir,              RW, "in", SP, NAC)               \
    DEFINE_FILE_PROP_P("fpga/trigger/sma_pol"                , hdlr_fpga_trigger_sma_pol,              RW, "negative", SP, NAC)          \
    DEFINE_FILE_PROP_P("fpga/trigger/sma_override"           , hdlr_fpga_trigger_sma_override,         RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/about/fw_ver"                   , hdlr_fpga_about_fw_ver,                 RW, VERSION, SP, NAC)             \
    DEFINE_FILE_PROP_P("fpga/about/sw_ver"                   , hdlr_fpga_about_sw_ver,                 RW, VERSION, SP, NAC)             \
    DEFINE_FILE_PROP_P("fpga/about/server_ver"               , hdlr_server_about_fw_ver,               RW, "", SP, NAC)                  \
    DEFINE_FILE_PROP_P("fpga/about/hw_ver"                   , hdlr_fpga_about_hw_ver,                 RW, VERSION, SP, NAC)             \
    DEFINE_FILE_PROP_P("fpga/about/id"                       , hdlr_fpga_about_id,                     RW, "001", SP, NAC)               \
    DEFINE_FILE_PROP_P("fpga/about/name"                     , hdlr_invalid,                           RO, PROJECT_NAME, SP, NAC)        \
    DEFINE_FILE_PROP_P("fpga/about/serial"                   , hdlr_fpga_about_serial,                 RW, "001", SP, NAC)               \
    DEFINE_FILE_PROP_P("fpga/about/cmp_time"                 , hdlr_fpga_about_cmp_time,               RW, "yyyy-mm-dd-hh-mm", SP, NAC)  \
    DEFINE_FILE_PROP_P("fpga/about/conf_info"                , hdlr_fpga_about_conf_info,              RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/about/imgparam/rate"            , hdlr_fpga_about_rate,              RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/about/imgparam/num_rx"          , hdlr_fpga_about_num_rx,              RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/about/imgparam/num_tx"          , hdlr_fpga_about_num_tx,              RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/about/imgparam/rtm"             , hdlr_fpga_about_rtm,              RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/about/imgparam/backplane_pinout", hdlr_fpga_about_backplane_pinout,              RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/about/imgparam/hps_only"        , hdlr_fpga_about_hps_only,              RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/about/imgparam/ddr_used"        , hdlr_fpga_about_ddr_used,              RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/about/imgparam/speed_grade"     , hdlr_fpga_about_speed_grade,            RW, "0", SP, NAC)\
    DEFINE_FILE_PROP_P("fpga/board/dump"                     , hdlr_fpga_board_dump,                   WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/fw_rst"                   , hdlr_fpga_board_fw_rst,                 WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/flow_control/sfpa_port"   , hdlr_fpga_board_flow_control_sfpa_port, RW, "42809", SP, NAC)             \
    DEFINE_FILE_PROP_P("fpga/board/flow_control/sfpb_port"   , hdlr_fpga_board_flow_control_sfpb_port, RW, "42810", SP, NAC)             \
    DEFINE_FILE_PROP_P("fpga/board/flow_control/sfpc_port"   , hdlr_fpga_board_flow_control_sfpc_port, RW, "42811", SP, NAC)             \
    DEFINE_FILE_PROP_P("fpga/board/flow_control/sfpd_port"   , hdlr_fpga_board_flow_control_sfpd_port, RW, "42812", SP, NAC)             \
    DEFINE_FILE_PROP_P("fpga/board/gps_time"                 , hdlr_fpga_board_gps_time,               RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/gps_frac_time"            , hdlr_fpga_board_gps_frac_time,          RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/gps_sync_time"            , hdlr_fpga_board_gps_sync_time,          RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/led"                      , hdlr_fpga_board_led,                    WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/rstreq_all_dsp"           , hdlr_fpga_board_rstreq_all_dsp,         WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/rstreq"                   , hdlr_fpga_board_rstreq,                 WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/reboot"                   , hdlr_fpga_board_reboot,                 RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/sys_rstreq"               , hdlr_fpga_board_sys_rstreq,             WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/test"                     , hdlr_fpga_board_test,                   WO, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/board/temp"                     , hdlr_fpga_board_temp,                   RW, "20", SP, NAC)                \
    DEFINE_FILE_PROP_P("fpga/board/gle"                      , hdlr_fpga_board_gle,                    RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/qa/ecc_a_errors"                , hdlr_fpga_qa_ecc_a_errors,              RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/qa/ecc_b_errors"                , hdlr_fpga_qa_ecc_b_errors,              RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/link/rate"                      , hdlr_fpga_link_rate,                    RW, "5000000000", SP, NAC)        \
    DEFINE_FILE_PROP_P("fpga/link/sfpa/ip_addr"              , hdlr_fpga_link_sfpa_ip_addr,            RW, "10.10.10.2", SP, NAC)        \
    DEFINE_FILE_PROP_P("fpga/link/sfpa/mac_addr"             , hdlr_fpga_link_sfpa_mac_addr,           RW, "aa:00:00:00:00:00", SP, NAC) \
    DEFINE_FILE_PROP_P("fpga/link/sfpa/ver"                  , hdlr_fpga_link_sfpa_ver,                RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/link/sfpa/pay_len"              , hdlr_fpga_link_sfpa_pay_len,            RW, "8896", SP, NAC)              \
    DEFINE_FILE_PROP_P("fpga/link/sfpb/ip_addr"              , hdlr_fpga_link_sfpb_ip_addr,            RW, "10.10.11.2", SP, NAC)        \
    DEFINE_FILE_PROP_P("fpga/link/sfpb/mac_addr"             , hdlr_fpga_link_sfpb_mac_addr,           RW, "aa:00:00:00:00:01", SP, NAC) \
    DEFINE_FILE_PROP_P("fpga/link/sfpb/ver"                  , hdlr_fpga_link_sfpb_ver,                RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/link/sfpb/pay_len"              , hdlr_fpga_link_sfpb_pay_len,            RW, "8896", SP, NAC)              \
    DEFINE_FILE_PROP_P("fpga/link/sfpc/ip_addr"              , hdlr_fpga_link_sfpc_ip_addr,            RW, "10.10.12.2", SP, NAC)        \
    DEFINE_FILE_PROP_P("fpga/link/sfpc/mac_addr"             , hdlr_fpga_link_sfpc_mac_addr,           RW, "aa:00:00:00:00:02", SP, NAC) \
    DEFINE_FILE_PROP_P("fpga/link/sfpc/ver"                  , hdlr_fpga_link_sfpc_ver,                RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/link/sfpc/pay_len"              , hdlr_fpga_link_sfpc_pay_len,            RW, "8896", SP, NAC)              \
    DEFINE_FILE_PROP_P("fpga/link/sfpd/ip_addr"              , hdlr_fpga_link_sfpd_ip_addr,            RW, "10.10.13.2", SP, NAC)        \
    DEFINE_FILE_PROP_P("fpga/link/sfpd/mac_addr"             , hdlr_fpga_link_sfpd_mac_addr,           RW, "aa:00:00:00:00:03", SP, NAC) \
    DEFINE_FILE_PROP_P("fpga/link/sfpd/ver"                  , hdlr_fpga_link_sfpd_ver,                RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/link/sfpd/pay_len"              , hdlr_fpga_link_sfpd_pay_len,            RW, "8896", SP, NAC)              \
    DEFINE_FILE_PROP_P("fpga/link/net/dhcp_en"               , hdlr_fpga_link_net_dhcp_en,             RW, "0", SP, NAC)                 \
    DEFINE_FILE_PROP_P("fpga/link/net/hostname"              , hdlr_fpga_link_net_hostname,            RW, PROJECT_NAME, SP, NAC)        \
    DEFINE_FILE_PROP_P("fpga/link/net/ip_addr"               , hdlr_fpga_link_net_ip_addr,             RW, "192.168.10.2", SP, NAC)\
    /* Size of half of a complex pair in bytes*/\
    DEFINE_FILE_PROP_P("fpga/link/rx_sample_bandwidth"       , hdlr_fpga_link_rx_sample_bandwidth,     RW, S_DEAULT_OTW_RX, SP, NAC)\
    DEFINE_FILE_PROP_P("fpga/link/tx_sample_bandwidth"       , hdlr_fpga_link_tx_sample_bandwidth,     RW, S_DEAULT_OTW_TX, SP, NAC)

#define DEFINE_FPGA_POST()                                                                                                         \
    DEFINE_FILE_PROP_P("fpga/jesd/jesd_reset_master"            , hdlr_jesd_reset_master,                      RW, "1", SP, NAC)               \

#define DEFINE_GPIO(_p)                                                                                                        \
    DEFINE_FILE_PROP_P("gpio/gpio" #_p                       , hdlr_gpio_##_p##_pin,                   RW, "0", SP, NAC)

#define DEFINE_CM()                                                    \
    DEFINE_FILE_PROP_P("cm/chanmask-rx" , hdlr_cm_chanmask_rx , RW, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/chanmask-tx" , hdlr_cm_chanmask_tx , RW, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/rx/atten/val", hdlr_cm_rx_atten_val, WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/rx/gain/val" , hdlr_cm_rx_gain_val , WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/tx/gain/val" , hdlr_cm_tx_gain_val , WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/tx/force_stream" , hdlr_cm_tx_force_stream , RW, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/trx/freq/val", hdlr_cm_trx_freq_val, WO, "0", SP, NAC) \
    DEFINE_FILE_PROP_P("cm/trx/fpga_nco" , hdlr_cm_trx_fpga_nco , WO, "0", SP, NAC)\
    DEFINE_FILE_PROP_P("cm/rx/force_stream", hdlr_cm_rx_force_stream , RW, "0", SP, NAC)

// Contians information about the configuration
#define DEFINE_SYSTEM_INFO()\
    DEFINE_FILE_PROP_P("system/num_rx"                   , hdlr_invalid,                           RO, S_NUM_RX, SP, NAC)\
    DEFINE_FILE_PROP_P("system/num_tx"                   , hdlr_invalid,                           RO, S_NUM_TX, SP, NAC)\
    DEFINE_FILE_PROP_P("system/max_rate"                 , hdlr_invalid,                           RO, S_MAX_RATE, SP, NAC)\
    DEFINE_FILE_PROP_P("system/get_max_buffer_level"     , hdlr_system_get_max_buffer_level,       RW, "1", SP, NAC)\
    DEFINE_FILE_PROP_P("system/get_buffer_level_multiple", hdlr_system_get_buffer_level_multiple,  RW, "1", SP, NAC)\
    /* Wire format has been moved to the FPGA link, this is here to allow older UHD versions to still find it*/\
    DEFINE_SYMLINK_PROP("system/otw_rx", "fpga/link/rx_sample_bandwidth")\
    DEFINE_SYMLINK_PROP("system/otw_tx", "fpga/link/tx_sample_bandwidth")\
    DEFINE_FILE_PROP_P("system/nsamps_multiple_rx"       , hdlr_invalid,                           RO, S_NSAMPS_MULTIPLE_RX, SP, NAC)\
    DEFINE_FILE_PROP_P("system/self_calibration"         , hdlr_system_self_calibration,           RW, "1", SP, NAC)\
    DEFINE_FILE_PROP_P("system/flags/USE_3G_AS_1G"       , hdlr_invalid,                           RO, S_USE_3G_AS_1G, SP, NAC)\

static prop_t property_table[] = {
// Turns off rx boards
#define X(ch) DEFINE_RX_BOARD_PWR(ch)
    RX_CHANNELS
#undef X
// Turns off tx boards
#define X(ch) DEFINE_TX_BOARD_PWR(ch)
    TX_CHANNELS
#undef X
// Initialize time boards
    DEFINE_TIME()
// Initialize FPGA
    DEFINE_FPGA()
// Inverts dev clock going to each rx board
// The dev clocks to rx need a small amount of time to stabilize after inverting
// Having them get inverted before initializing the FPGA allows them to stablize by the time the rx boards turn on
#define X(ch) DEFINE_RX_INVERT_DEV_CLOCK(ch)
    RX_CHANNELS
#undef X
// Power on rx/tx boards, but don't wait for them to finish booting
#define X(ch) DEFINE_RX_PWR_REBOOT(ch)
    RX_CHANNELS
#undef X
#define X(ch) DEFINE_TX_PWR_REBOOT(ch)
    TX_CHANNELS
#undef X

// Waits for boards to finish booting
#define X(ch) DEFINE_RX_WAIT_PWR(ch)
    RX_CHANNELS
#undef X
#define X(ch) DEFINE_TX_WAIT_PWR(ch)
    TX_CHANNELS
#undef X

// Initialize rx/tx boards
#define X(ch) DEFINE_RX_CHANNEL(ch)
    RX_CHANNELS
#undef X
#define X(ch) DEFINE_TX_CHANNEL(ch)
    TX_CHANNELS
#undef X

#define X(_p, io) DEFINE_GPIO(_p)
    GPIO_PINS
#undef X
    DEFINE_FILE_PROP_P("gpio/override_en"                    , hdlr_gpio_override_en,                  RW, "0", SP, NAC)
    DEFINE_FILE_PROP_P("gpio/gpio_all"                       , hdlr_gpio_gpio_all,                     RW, "0", SP, NAC)
    DEFINE_FILE_PROP_P("save_config", hdlr_save_config, RW, "/home/root/profile.cfg", SP, NAC)
    DEFINE_FILE_PROP_P("load_config", hdlr_load_config, RW, "/home/root/profile.cfg", SP, NAC)
    DEFINE_CM()
    DEFINE_FPGA_POST()
    DEFINE_SYSTEM_INFO()
};

static const size_t num_properties = ARRAY_SIZE(property_table);

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
    char str[32];
    snprintf(str, 32, "%i", value);
    set_default_str(path, str);
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

#define X(ch) \
    set_default_int("rx/" #ch "/link/port", base_port + INT(ch));\
    set_default_str("rx/" #ch "/link/ip_dest", rx_ip_dst[INT(ch)]); \
    set_default_int("rx/" #ch "/link/src_port", base_port + (INT(ch)*4)); \
    set_default_int("rx/" #ch "/link/jesd_num", rx_jesd_map[INT(ch)]);

    RX_CHANNELS
#undef X

    // Sysref should effectively be  read on the rising edge of devclock
    // This can invert devclock for rx boards to effectively turn a falling edge into a rising edge
    #define X(ch) \
        set_default_int("rx/" #ch "/jesd/invert_devclk", RX_SYSREF_FALLING_EDGE^INVERT_DEV_CLOCK_HARDWARE_SWAP);

        RX_CHANNELS
    #undef X

#if NUM_TX_CHANNELS > 0
    int tx_dsp_port_map_loc;
    #define X(ch) \
        tx_dsp_port_map_loc = get_tx_dst_port_map_loc(INT(ch));\
        set_default_int("tx/" #ch "/link/port", base_port + tx_dsp_port_map_loc*4);

        TX_CHANNELS
    #undef X
#endif

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

    const char *vcs = STATE_DIR "/";
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

    if ( 0 != strcmp( path, prop ) ) {
#ifdef DEBUG
        PRINT( INFO, "%s(): resolved symbolic link: '%s' => '%s'\n", __func__, prop, path );
#endif
    }

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
        if ((strcmp(property_table[i].path, cmd) == 0) && (strlen(property_table[i].path) == strlen(cmd))) {
            return (property_table + i);
        }
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

#ifdef RTM3
// JESD fails 50% of the time currently on 3G RTM3
// As a workaround, increase the maximum number of server restarts for it
const int jesd_max_server_restart_attempts = 10;
#else
// RTM4 and later have very reliable JESD, so if more than 3 attempts are required for JESD somthing is wrong
const int jesd_max_server_restart_attempts = 3;
#endif

// Performs a master JESD reset up to 3 times
// Returns 0 for success, not 0 for failure. This is to enable bit flags for specific error codes in the future
// Must set sysref to continuous before calling this function and set it to pulsed after. Not done as part of this function since it is meant to be called in a loop if there are issues establishing JESD, and switching sysref mode is slow
int jesd_master_reset() {
    char prop_path[PROP_PATH_LEN];
    // Stores the original value for the dsp reset registers. The value at the end should match the value it started with
    uint32_t original_rx4[NUM_RX_CHANNELS];
    uint32_t original_tx4[NUM_TX_CHANNELS];
    // Sysref pulses and the start of sysref continuous mode have a bad output. Mask sysref to mitigate the issue

    //Takes rx channels dsp out of reset if they are in use. When channels are in reset JESD sync is ignored.
    //Not taking them out of reset will result in them being out of alignment, and inconsistent behaviour if all channels are in reset
    for(int chan = 0; chan < NUM_RX_CHANNELS; chan++) {
        read_hps_reg(rx_reg4_map[chan], &original_rx4[chan]);
        if(rx_power[chan]==PWR_HALF_ON || rx_power[chan]==PWR_ON) {
            write_hps_reg_mask(rx_reg4_map[chan], 0x0, 0x2);
        } else {
            write_hps_reg_mask(rx_reg4_map[chan], 0x2, 0x2);
        }
    }
    // Gives time for sysref unmask to update
    usleep(jesd_mask_delay);

    //Takes tx channels dsp out of reset if they are in use. When channels are in reset JESD sync is ignored
    //Not taking them out of reset will result in them being out of alignment, and inconsistent behaviour if all channels are in reset
    for(int chan = 0; chan < NUM_TX_CHANNELS; chan++) {
        read_hps_reg(tx_reg4_map[chan], &original_tx4[chan]);
        if(tx_power[chan]==PWR_HALF_ON || tx_power[chan]==PWR_ON) {
            write_hps_reg_mask(tx_reg4_map[chan], 0x0, 0x2);
        } else {
            write_hps_reg_mask(tx_reg4_map[chan], 0x2, 0x2);
        }
    }

    int attempts = 0;
    while ( attempts < JESD_MAX_RESET_ATTEMPTS ) {
        int is_bad_attempt = 0;

        //Unmask all channels for next attempt
        for(int chan = 0; chan < NUM_RX_CHANNELS; chan++) {
            if(rx_power[chan]==PWR_HALF_ON || rx_power[chan]==PWR_ON) {
                snprintf(prop_path, PROP_PATH_LEN, "rx/%c/jesd/unmask", chan+'a');
                set_property(prop_path, "1");
            } else {
            }
        }

        // TODO: add function on tx board instead of using register write
        // Enables SYSREF_MODE_ONESHOT, must be done to enable listening for sysref
        snprintf(buf, MAX_PROP_LEN, "dac -r 03a -w 2\r");
#define X(ch)                                                              \
        if(tx_power[INT(ch)]==PWR_HALF_ON || tx_power[INT(ch)]==PWR_ON) {\
            ping_tx(uart_tx_fd[INT_TX(ch)], (uint8_t *)buf, strlen(buf), INT(ch));\
        }
        TX_CHANNELS
#undef X

        // Reset JESD IP
        set_property("fpga/reset", "3");
        // Reinint rx JESD without resetting IP (alternative to resetting the IP
        // Reining via reg writes break RTM3 USE_3G_AS_1G
        // Resetting the IP is prefered but sometimes this works when resetting the IP doesn't
        // for(int chan = 0; chan < NUM_RX_CHANNELS; chan++) {
        //     write_jesd_reg_mask(chan + JESD_SHIFT_RX, 0x54, 0x5, 0x5);
        // }
        // // Reinint tx JESD
        // for(int chan = 0; chan < NUM_TX_CHANNELS; chan++) {
        //     write_jesd_reg_mask(chan + JESD_SHIFT_TX, 0x54, 0x5, 0x5);
        // }

        //Wait for links to go down
        usleep(jesd_reset_delay);

        // Issues sysref pulse
        set_property("time/sync/lmk_sync_tgl_jesd", "1");

        //Wait for links to re-establish
        usleep(jesd_reset_delay);

        //Immediately mask all channels.
        for(int chan = 0; chan < NUM_RX_CHANNELS; chan++) {
            if(rx_power[chan]==PWR_HALF_ON || rx_power[chan]==PWR_ON) {
                snprintf(prop_path, PROP_PATH_LEN, "rx/%c/jesd/unmask", chan+'a');
                set_property(prop_path, "0");
            } else {
            }
        }

        //Checks if all rx JESDs are working
        for(int chan = 0; chan < NUM_RX_CHANNELS; chan++) {
            if(rx_power[chan]==PWR_HALF_ON || rx_power[chan]==PWR_ON) {
                snprintf(prop_path, PROP_PATH_LEN, "rx/%c/jesd/status", chan+'a');
                if(property_good(prop_path) != 1) {
                    PRINT(ERROR, "JESD link for rx channel %c failed on master attempt %i\n", chan+'a', attempts);
                    is_bad_attempt = 1;
                }
            }
        }

        //Checks if all tx JESDs are working
        for(int chan = 0; chan < NUM_TX_CHANNELS; chan++) {
            if(tx_power[chan]==PWR_HALF_ON || tx_power[chan]==PWR_ON) {
                snprintf(prop_path, PROP_PATH_LEN, "tx/%c/jesd/status", chan+'a');
                if(property_good(prop_path) != 1) {
                    PRINT(ERROR, "JESD link for tx channel %c failed on master attempt %i\n", chan+'a', attempts);
                    is_bad_attempt = 1;
                }
            }
        }

        if(!is_bad_attempt) {
            break;
        }
        attempts++;
    }

    // Sets whether the dsp is in reset to what is was prior to this function
    for(int chan = 0; chan < NUM_RX_CHANNELS; chan++) {
        if(rx_power[chan]==PWR_HALF_ON || rx_power[chan]==PWR_ON) {
            write_hps_reg_mask(rx_reg4_map[chan], original_rx4[chan], 0x2);
            snprintf(prop_path, PROP_PATH_LEN, "rx/%c/jesd/unmask", chan+'a');
            set_property(prop_path, "0");
        } else {
            // Leave dsp in reset if there is no board in that slot, note this line should be redundant since the dsp will be left in reset
            write_hps_reg_mask(rx_reg4_map[chan], 0x2, 0x2);
        }
    }

    // Sets whether the dsp is in reset to what is was prior to this function
    for(int chan = 0; chan < NUM_TX_CHANNELS; chan++) {
        write_hps_reg_mask(tx_reg4_map[chan], original_tx4[chan], 0x2);
    }

    if(attempts >= JESD_MAX_RESET_ATTEMPTS) return 1;
    else return 0;
}

#ifdef S1000
//sets sysref delay in VCO clock cycles
void set_analog_sysref_delay(int analog_sysref_delay) {
    snprintf(buf, MAX_PROP_LEN, "adly -l 7 -c a -s %i\r", analog_sysref_delay);
    ping(uart_synth_fd, (uint8_t *)buf, strlen(buf));
}
#elif defined(S3000)
    // analog sysref delay disabled on 3G since JESD reinit is not allowed without a reboot of the boards
#else
    #error Invalid maximum sample rate specified (MHz), must be: S1000, S3000
#endif

// Returns 1 is jesd links come up, 1 if any links fail
static int hdlr_jesd_reset_master(const char *data, char *ret) {
    //Do nothing is 0 is provided, reset all if everything else is provided
    int reset = 0;
    sscanf(data, "%i", &reset);
    if (!reset){
        return RETURN_SUCCESS;
    }

    // Enables individual resets (primarily used to prevent individual resets from being triggered until all JESD is up
    jesd_enabled = 1;

    set_property("time/sync/sysref_mode", "pulsed");

    int64_t analog_sysref_delay = 0;
    if(read_interboot_variable("analog_sysref_delay", &analog_sysref_delay)) {
        analog_sysref_delay = DEFAULT_ANALOG_SYSREF_DELAY;
    }

#ifdef S1000
    set_analog_sysref_delay(analog_sysref_delay);
#elif defined(S3000)
    // analog sysref delay disabled on 3G since JESD reinit is not allowed without a reboot of the boards
#else
    #error Invalid maximum sample rate specified (MHz), must be: S1000, S3000
#endif

    // Note this is set to 0 for success, any other value for failure
    int jesd_master_error = jesd_master_reset();

#ifdef S1000
    // Test all possible values of sysref delay until one works if the previously save/default failed
    if(jesd_master_error) {
        PRINT(ERROR, "Attempt to bring up JESD with an analog sysref delay of %i failed\n", analog_sysref_delay);

        for(int n = 0; n < NUM_ANALOG_SYSREF_DELAYS; n++) {
            analog_sysref_delay = possible_analog_sysref_delays[n];
            set_analog_sysref_delay(analog_sysref_delay);
            PRINT(ERROR, "Attempting to bring up JESD with an analog sysref delay of %i\n", analog_sysref_delay);
            jesd_master_error = jesd_master_reset();
            if(!jesd_master_error) {
                break;
            }
            PRINT(ERROR, "Attempt to bring up JESD with an analog sysref delay of %i failed\n", analog_sysref_delay);
        }
    }
#elif defined(S3000)
    // analog sysref delay disabled on 3G since JESD reinit is not allowed without a reboot of the boards
#else
    #error Invalid maximum sample rate specified (MHz), must be: S1000, S3000
#endif

    if(!jesd_master_error) {
        update_interboot_variable("cons_jesd_fail_count", 0);
        update_interboot_variable("analog_sysref_delay", analog_sysref_delay);
    } else {
        int64_t failed_count = 0;
        read_interboot_variable("cons_jesd_fail_count", &failed_count);
        if(failed_count < jesd_max_server_restart_attempts) {
            PRINT(ERROR, "Unable to establish JESD links despite individual resets. Attempting to restart the server\n");
            update_interboot_variable("cons_jesd_fail_count", failed_count + 1);
            PRINT(ERROR, "Restarting server\n");
            system("systemctl restart cyan-server");
            // Waits for the server reboot command to restart the server
            while(1) {
                usleep(1000);
            }
        } else {
            PRINT(ERROR, "Unable to establish JESD links despite multiple server restarts. The system will not attempt another server restart to bring up JESD links until a successful attempt to establish links\n");
            snprintf(ret, MAX_PROP_LEN, "1");
        }
    }

    // This print message is down here instead of in jesd_master_reset in order to make it closer to the end of server boot to make it easier to spot
    if(!jesd_master_error) {
        PRINT(INFO, "All JESD successfully established\n");
        snprintf(ret, MAX_PROP_LEN, "good");
        return RETURN_SUCCESS;
    } else {
        snprintf(ret, MAX_PROP_LEN, "bad");
        return RETURN_ERROR;
    }
}

int set_lo_frequency_rx(int uart_fd, uint64_t reference, pllparam_t *pll, int channel) {
    // extract lo variables and pass to MCU (LMX2595)

    double freq = (pll->vcoFreq / pll->d) + (pll->x2en * pll->vcoFreq / pll->d);

    // Ensure that the LoGen board is powered on
    snprintf(buf, MAX_PROP_LEN, "lmx -O 0\r");
    ping_rx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // Reinitialize the LMX. For some reason the initialization on server boot, doesn't seem to be enough
    snprintf(buf, MAX_PROP_LEN, "lmx -k\r");
    ping_rx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // Send Reference in MHz to MCU
    snprintf(buf, MAX_PROP_LEN, "lmx -o %" PRIu32 "\r", (uint32_t)(reference / 1000000));
    ping_rx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // write LMX R
    snprintf(buf, MAX_PROP_LEN, "lmx -r ""%" PRIu16 """\r", pll->R);
    ping_rx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // write LMX N
    snprintf(buf, MAX_PROP_LEN, "lmx -n %" PRIu32 "\r", pll->N);
    ping_rx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // write LMX D
    snprintf(buf, MAX_PROP_LEN, "lmx -d %" PRIu16 "\r", pll->d);
    ping_rx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // write LMX Output RF Power
    // default to high power
    snprintf(buf, MAX_PROP_LEN, "lmx -p %" PRIu8 "\r", 45 /*TODO: pll->power*/);
    ping_rx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // write LMX Output Frequency in MHz
    snprintf(buf, MAX_PROP_LEN, "lmx -f %" PRIu32 "\r", (uint32_t)(freq / 1000000));
    ping_rx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    //Wait for PLL to lock, timeout after 100ms
    struct timespec timeout_start;
    int time_ret = clock_gettime(CLOCK_MONOTONIC_COARSE, &timeout_start);
    const int timeout_ns = 100000000;

    if(time_ret) {
        PRINT(ERROR, "Get time failed with %s. Waiting %ims instead of polling\n", strerror(errno), timeout_ns/1000000);
        usleep(timeout_ns/1000);
        return check_rf_pll(channel, false);
    }

    int lock_failed = 0;
    // Polling loop waiting for PLL to finish locking
    while(!check_rf_pll(channel, false)) {
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
        // Mute PLL to avoid transmitting with an enexpected frequency
        strcpy(buf, "rf -c " STR(ch) " -z\r");
        ping(uart_fd, (uint8_t *)buf, strlen(buf));
        PRINT(ERROR, "Rx PLL unlocked. Muting PLL\n");
        return 0;
    } else {
        // success
        return 1;
    }
}

//At time of write, the only differences between set_lo_frequency rx and tx is the ping function used
//which is used to avoid sending uart commands to unpopulated boards
int set_lo_frequency_tx(int uart_fd, uint64_t reference, pllparam_t *pll, int channel) {
    // extract lo variables and pass to MCU (LMX2595)

    double freq = (pll->vcoFreq / pll->d) + (pll->x2en * pll->vcoFreq / pll->d);

    // Ensure that the LoGen board is powered on
    snprintf(buf, MAX_PROP_LEN, "lmx -O 0\r");
    ping_tx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // Reinitialize the LMX. For some reason the initialization on server boot, doesn't seem to be enough
    snprintf(buf, MAX_PROP_LEN, "lmx -k\r");
    ping_tx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // Send Reference in MHz to MCU
    snprintf(buf, MAX_PROP_LEN, "lmx -o %" PRIu32 "\r", (uint32_t)(reference / 1000000));
    ping_tx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // write LMX R
    snprintf(buf, MAX_PROP_LEN, "lmx -r %" PRIu16 "\r", pll->R);
    ping_tx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // write LMX N
    snprintf(buf, MAX_PROP_LEN, "lmx -n %" PRIu32 "\r", pll->N);
    ping_tx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // write LMX D
    snprintf(buf, MAX_PROP_LEN, "lmx -d %" PRIu16 "\r", pll->d);
    ping_tx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // write LMX Output RF Power
    // default to high power
    snprintf(buf, MAX_PROP_LEN, "lmx -p %" PRIu8 "\r", 60 /*TODO: pll->power*/);
    ping_tx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    // write LMX Output Frequency in MHz
    snprintf(buf, MAX_PROP_LEN, "lmx -f %" PRIu32 "\r", (uint32_t)(freq / 1000000));
    ping_tx(uart_fd, (uint8_t *)buf, strlen(buf), channel);

    //Wait for PLL to lock, timeout after 100ms
    struct timespec timeout_start;
    int time_ret = clock_gettime(CLOCK_MONOTONIC_COARSE, &timeout_start);
    const int timeout_ns = 100000000;

    if(time_ret) {
        PRINT(ERROR, "Get time failed with %s. Waiting %ims instead of polling\n", strerror(errno), timeout_ns/1000000);
        usleep(timeout_ns/1000);
        return check_rf_pll(channel, true);
    }

    int lock_failed = 0;
    // Polling loop waiting for PLL to finish locking
    while(!check_rf_pll(channel, true)) {
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
        // Mute PLL to avoid transmitting with an enexpected frequency
        strcpy(buf, "rf -c " STR(ch) " -z\r");
        ping(uart_fd, (uint8_t *)buf, strlen(buf));
        PRINT(ERROR, "Tx PLL unlocked. Muting PLL\n");
        return 0;
    } else {
        // success
        return 1;
    }
}

#endif //defined(TATE_NRNT)
