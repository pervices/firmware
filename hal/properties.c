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

#include "properties.h"
#include "mmap.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "array-utils.h"

#include "property_manager.h"
#include "synth_lut.h"

#define BASE_SAMPLE_RATE 325000000.0   // SPS
#define RESAMP_SAMPLE_RATE 260000000.0 // SPS
#define IPVER_IPV4 0
#define IPVER_IPV6 1
// truncate DSP NCO CONST last 4 digits to ensure dac NCO can divide evenly
#define DSP_NCO_CONST ((double)13.215283987692307692307692307692307692307692307692307690000) // (2^32) / (322265625)
#define DAC_NCO_CONST ((double)216519.21285435076923076923076923076923076923076923076919296) // (2^48) / (4*322265625)

/* Forwarding is now obsolete */
//#define FWD_CMD 	1
//#define NO_FWD_CMD	0

#define PWR_ON 1
#define PWR_OFF 0

#define STREAM_ON 1
#define STREAM_OFF 0

#define FREQ_XOVER_PNT 100000000 // 100 MHz is the crossover frequency for high and low band

#define NUM_CHANNELS 4

// static global variables
static int uart_synth_fd = 0;
static int uart_tx_fd = 0;
static int uart_rx_fd = 0;
static uint8_t uart_ret_buf[MAX_UART_RET_LEN] = {};
static char buf[MAX_PROP_LEN] = {};

// by default the board is powered off
static uint8_t rx_power[] = {PWR_OFF, PWR_OFF, PWR_OFF, PWR_OFF};
static uint8_t tx_power[] = {PWR_OFF, PWR_OFF, PWR_OFF, PWR_OFF};
static uint8_t rx_stream[] = {STREAM_OFF, STREAM_OFF, STREAM_OFF, STREAM_OFF};
const static char *reg4[] = {"rxa4", "rxb4", "rxc4", "rxd4", "txa4", "txb4", "txc4", "txd4"};
static int i_bias[] = {17, 17, 17, 17};
static int q_bias[] = {17, 17, 17, 17};

// profile pointers
uint8_t *_save_profile;
uint8_t *_load_profile;
char *_save_profile_path;
char *_load_profile_path;

// state variables
static uint8_t ipver[2] = {IPVER_IPV4, IPVER_IPV4};

// helper function to check if the buffer contains a character, strstr() won't work because no NULL terminator
static int contains(const char *str, char letter, int size) {
    int i = 0, cnt = 0;
    for (i = 0; i < size; i++) {
        if (str[i] == letter)
            cnt++;
    }
    return cnt;
}

// helper function to read back from UART after a UART command
static int read_uart(int uartfd) {
    char buf[MAX_UART_LEN] = {};
    memset(buf, 0, MAX_UART_LEN);

    uint16_t total_bytes = 0, cur_bytes = 0;

    while (contains(buf, '>', total_bytes) < 1) {
        if (recv_uart_comm(uartfd, ((uint8_t *)buf) + total_bytes, &cur_bytes, MAX_UART_LEN - total_bytes)) {
            return 0;
        }
        total_bytes += cur_bytes;
    }

    /* Forwarding is now obsolete */

    // if fwd, remove everything prior to the second message
    // if (fwd) {
    //	uint16_t pos = 0, real_size = 0;
    //	while (buf[pos] != '>') pos++;
    //	pos++;
    //	real_size = total_bytes - pos;
    //	memcpy(buf, buf + pos, real_size);
    //	memset(buf + real_size, 0, MAX_UART_LEN - real_size);
    //}

    printf("%s\n", buf);
    strncpy((char *)uart_ret_buf, buf, MAX_UART_RET_LEN - 1);
    return RETURN_SUCCESS;
}

// helper function to find the optimal value for the sample rate blocks
static uint16_t get_optimal_sr_factor(double rate, double base_rate, double *err) {
    double max_factor = 65536; // 2^16
    double min_factor = 1;
    double lower_factor_violation = 0;
    double upper_factor_violation = 0;

    double lower_factor = (double)floor(base_rate / rate);
    double upper_factor = lower_factor + 1;

    // error bounds check
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

// PRINT( VERBOSE, "%s(): set_reg_bits( name: %s, shift: %u, mask: %p, val: %p )\n", __func__, name, shift, (void
// *)mask, (void *)val );
// XXX: @CF: 20171108: Statement Expressions are bad... but this code will be replaced soon anyway
#define set_reg_bits(name, shift, mask, val)                                                                           \
    ({                                                                                                                 \
        int _r;                                                                                                        \
        uint32_t _t;                                                                                                   \
        _r = read_hps_reg(name, &_t);                                                                                  \
        if (RETURN_SUCCESS != _r) {                                                                                    \
            PRINT(ERROR, "read_hps_reg( '%s' ) failed: %d\n", name, _r);                                               \
        } else {                                                                                                       \
            _t &= ~(mask << shift);                                                                                    \
            _t |= val << shift;                                                                                        \
            _r = write_hps_reg(name, _t);                                                                              \
            if (RETURN_SUCCESS != _r) {                                                                                \
                PRINT(ERROR, "write_hps_reg( '%s' ) failed: %d\n", name, _r);                                          \
            }                                                                                                          \
        }                                                                                                              \
        _r;                                                                                                            \
    })

static int set_sma_dir(bool in) { return set_reg_bits("sys2", 4, 1, in); }

static int set_sma_pol(bool positive) { return set_reg_bits("sys2", 6, 1, positive); }

// Beginning of property functions, very long because each property needs to be
// handled explicitly
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
    int (*hdlr)(const char *, char *);
    int i;

    int atten = 0;

    mask_rx = cm_chanmask_get("/var/crimson/state/cm/chanmask-rx");

    sscanf(data, "%lf", &atten);

    sprintf(inbuf, "%lf", atten);

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }

        switch (i) {
        case 0:
            hdlr = hdlr_rx_a_rf_atten_val;
            break;
        case 1:
            hdlr = hdlr_rx_b_rf_atten_val;
            break;
        case 2:
            hdlr = hdlr_rx_c_rf_atten_val;
            break;
        case 3:
            hdlr = hdlr_rx_d_rf_atten_val;
            break;
        default:
            continue;
        }

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
    int (*hdlr)(const char *, char *);
    int i;

    double gain = 0;

    mask_rx = cm_chanmask_get("/var/crimson/state/cm/chanmask-rx");

    sscanf(data, "%lf", &gain);

    sprintf(inbuf, "%lf", gain);

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_rx & (1 << i))) {
            continue;
        }

        switch (i) {
        case 0:
            hdlr = hdlr_rx_a_rf_gain_val;
            break;
        case 1:
            hdlr = hdlr_rx_b_rf_gain_val;
            break;
        case 2:
            hdlr = hdlr_rx_c_rf_gain_val;
            break;
        case 3:
            hdlr = hdlr_rx_d_rf_gain_val;
            break;
        default:
            continue;
        }

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
    int (*hdlr)(const char *, char *);
    int i;

    double gain = 0;

    mask_tx = cm_chanmask_get("/var/crimson/state/cm/chanmask-tx");

    sscanf(data, "%lf", &gain);

    sprintf(inbuf, "%lf", gain);

    for (i = 0; i < NUM_CHANNELS; i++) {

        if (0 == (mask_tx & (1 << i))) {
            continue;
        }

        switch (i) {
        case 0:
            hdlr = hdlr_tx_a_rf_gain_val;
            break;
        case 1:
            hdlr = hdlr_tx_b_rf_gain_val;
            break;
        case 2:
            hdlr = hdlr_tx_c_rf_gain_val;
            break;
        case 3:
            hdlr = hdlr_tx_d_rf_gain_val;
            break;
        default:
            continue;
        }

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
    int (*hdlr)(const char *, char *);
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

        switch (i) {
        case 0:
            hdlr = hdlr_rx_a_rf_freq_val;
            break;
        case 1:
            hdlr = hdlr_rx_b_rf_freq_val;
            break;
        case 2:
            hdlr = hdlr_rx_c_rf_freq_val;
            break;
        case 3:
            hdlr = hdlr_rx_d_rf_freq_val;
            break;
        default:
            continue;
        }

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

        switch (i) {
        case 0:
            hdlr = hdlr_tx_a_rf_freq_val;
            break;
        case 1:
            hdlr = hdlr_tx_b_rf_freq_val;
            break;
        case 2:
            hdlr = hdlr_tx_c_rf_freq_val;
            break;
        case 3:
            hdlr = hdlr_tx_d_rf_freq_val;
            break;
        default:
            continue;
        }

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
    int (*hdlr)(const char *, char *);
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

        switch (i) {
        case 0:
            hdlr = hdlr_rx_a_dsp_nco_adj;
            break;
        case 1:
            hdlr = hdlr_rx_b_dsp_nco_adj;
            break;
        case 2:
            hdlr = hdlr_rx_c_dsp_nco_adj;
            break;
        case 3:
            hdlr = hdlr_rx_d_dsp_nco_adj;
            break;
        default:
            continue;
        }

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

        switch (i) {
        case 0:
            hdlr = hdlr_tx_a_dsp_nco_adj;
            break;
        case 1:
            hdlr = hdlr_tx_b_dsp_nco_adj;
            break;
        case 2:
            hdlr = hdlr_tx_c_dsp_nco_adj;
            break;
        case 3:
            hdlr = hdlr_tx_d_dsp_nco_adj;
            break;
        default:
            continue;
        }

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

static int hdlr_XX_X_rf_freq_lut_en(const char *data, char *ret, const bool tx, const size_t channel) {
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

static int hdlr_rx_a_rf_freq_lut_en(const char *data, char *ret) {
    return hdlr_XX_X_rf_freq_lut_en(data, ret, false, 0);
}
static int hdlr_rx_b_rf_freq_lut_en(const char *data, char *ret) {
    return hdlr_XX_X_rf_freq_lut_en(data, ret, false, 1);
}
static int hdlr_rx_c_rf_freq_lut_en(const char *data, char *ret) {
    return hdlr_XX_X_rf_freq_lut_en(data, ret, false, 2);
}
static int hdlr_rx_d_rf_freq_lut_en(const char *data, char *ret) {
    return hdlr_XX_X_rf_freq_lut_en(data, ret, false, 3);
}
static int hdlr_tx_a_rf_freq_lut_en(const char *data, char *ret) {
    return hdlr_XX_X_rf_freq_lut_en(data, ret, true, 0);
}
static int hdlr_tx_b_rf_freq_lut_en(const char *data, char *ret) {
    return hdlr_XX_X_rf_freq_lut_en(data, ret, true, 1);
}
static int hdlr_tx_c_rf_freq_lut_en(const char *data, char *ret) {
    return hdlr_XX_X_rf_freq_lut_en(data, ret, true, 2);
}
static int hdlr_tx_d_rf_freq_lut_en(const char *data, char *ret) {
    return hdlr_XX_X_rf_freq_lut_en(data, ret, true, 3);
}

#define DEFINE_FILE_PROP(n, h, p, v)                                                                                   \
    { .type = PROP_TYPE_FILE, .path = n, .handler = h, .permissions = p, .def_val = v, }

#define DEFINE_SYMLINK_PROP(n, t)                                                                                      \
    { .type = PROP_TYPE_SYMLINK, .path = n, .symlink_target = t, }

// XXX: @CF: If the port were encoded as an integer it would be easier to define (possibly can still using some wild
// macro expansions)
// TODO: @CF: We should be using pmt's for arguments to refactor common parsing code; handlers should be passed
// integers, doubles, and so on, not necessarily strings.
// TODO: @CF: Alphabetically sort the properties
// TODO: @CF: Enumerate properties and assign them via enumeration rather than automatic indexing
#define DEFINE_RX_CHANNEL(_c, _p, _ip)                                                                                 \
    DEFINE_SYMLINK_PROP("rx_" #_c, "rx/" #_c),                                                                         \
        DEFINE_FILE_PROP("rx/" #_c "/trigger/sma_mode", hdlr_rx_##_c##_trigger_sma_mode, RW, "level"),                 \
        DEFINE_FILE_PROP("rx/" #_c "/trigger/trig_sel", hdlr_rx_##_c##_trigger_trig_sel, RW, "0"),                     \
        DEFINE_FILE_PROP("rx/" #_c "/trigger/edge_backoff", hdlr_rx_##_c##_trigger_edge_backoff, RW, "0"),             \
        DEFINE_FILE_PROP("rx/" #_c "/trigger/edge_sample_num", hdlr_rx_##_c##_trigger_edge_sample_num, RW, "0"),       \
        DEFINE_FILE_PROP("rx/" #_c "/trigger/ufl_mode", hdlr_rx_##_c##_trigger_ufl_mode, RW, "level"),                 \
        DEFINE_FILE_PROP("rx/" #_c "/trigger/ufl_dir", hdlr_rx_##_c##_trigger_ufl_dir, RW, "out"),                     \
        DEFINE_FILE_PROP("rx/" #_c "/trigger/ufl_pol", hdlr_rx_##_c##_trigger_ufl_pol, RW, "negative"),                \
        DEFINE_FILE_PROP("rx/" #_c "/pwr", hdlr_rx_##_c##_pwr, RW, "0"),                                               \
        DEFINE_FILE_PROP("rx/" #_c "/stream", hdlr_rx_##_c##_stream, RW, "0"),                                         \
        DEFINE_FILE_PROP("rx/" #_c "/sync", hdlr_rx_sync, WO, "0"),                                                    \
        DEFINE_FILE_PROP("rx/" #_c "/rf/freq/val", hdlr_rx_##_c##_rf_freq_val, RW, "0"),                               \
        DEFINE_FILE_PROP("rx/" #_c "/rf/freq/lut_en", hdlr_rx_##_c##_rf_freq_lut_en, RW, "0"),                         \
        DEFINE_FILE_PROP("rx/" #_c "/rf/freq/lna", hdlr_rx_##_c##_rf_freq_lna, RW, "1"),                               \
        DEFINE_FILE_PROP("rx/" #_c "/rf/freq/band", hdlr_rx_##_c##_rf_freq_band, RW, "1"),                             \
        DEFINE_FILE_PROP("rx/" #_c "/rf/gain/val", hdlr_rx_##_c##_rf_gain_val, RW, "0"),                               \
        DEFINE_FILE_PROP("rx/" #_c "/rf/atten/val", hdlr_rx_##_c##_rf_atten_val, RW, "127"),                           \
        DEFINE_FILE_PROP("rx/" #_c "/status/rfpll_lock", hdlr_rx_##_c##_status_rfld, RW, "0"),                         \
        DEFINE_FILE_PROP("rx/" #_c "/status/adc_alarm", hdlr_rx_##_c##_status_adcalarm, RW, "0"),                      \
        DEFINE_FILE_PROP("rx/" #_c "/board/dump", hdlr_rx_##_c##_rf_board_dump, WO, "0"),                              \
        DEFINE_FILE_PROP("rx/" #_c "/board/test", hdlr_rx_##_c##_rf_board_test, WO, "0"),                              \
        DEFINE_FILE_PROP("rx/" #_c "/board/temp", hdlr_rx_##_c##_rf_board_temp, RW, "20"),                             \
        DEFINE_FILE_PROP("rx/" #_c "/board/led", hdlr_rx_##_c##_rf_board_led, WO, "0"),                                \
        DEFINE_FILE_PROP("rx/" #_c "/dsp/signed", hdlr_rx_##_c##_dsp_signed, RW, "1"),                                 \
        DEFINE_FILE_PROP("rx/" #_c "/dsp/gain", hdlr_rx_##_c##_dsp_gain, RW, "10"),                                    \
        DEFINE_FILE_PROP("rx/" #_c "/dsp/rate", hdlr_rx_##_c##_dsp_rate, RW, "1258850"),                               \
        DEFINE_FILE_PROP("rx/" #_c "/dsp/nco_adj", hdlr_rx_##_c##_dsp_nco_adj, RW, "-15000000"),                       \
        DEFINE_FILE_PROP("rx/" #_c "/dsp/rstreq", hdlr_rx_##_c##_dsp_rstreq, WO, "0"),                                 \
        DEFINE_FILE_PROP("rx/" #_c "/dsp/loopback", hdlr_rx_##_c##_dsp_loopback, RW, "0"),                             \
        DEFINE_FILE_PROP("rx/" #_c "/about/id", hdlr_rx_##_c##_about_id, RW, "001"),                                   \
        DEFINE_FILE_PROP("rx/" #_c "/about/serial", hdlr_rx_about_serial, RW, "001"),                                  \
        DEFINE_FILE_PROP("rx/" #_c "/about/mcudevid", hdlr_rx_about_mcudevid, RW, "001"),                              \
        DEFINE_FILE_PROP("rx/" #_c "/about/mcurev", hdlr_rx_about_mcurev, RW, "001"),                                  \
        DEFINE_FILE_PROP("rx/" #_c "/about/mcufuses", hdlr_rx_about_mcufuses, RW, "001"),                              \
        DEFINE_FILE_PROP("rx/" #_c "/about/fw_ver", hdlr_rx_about_fw_ver, RW, VERSION),                                \
        DEFINE_FILE_PROP("rx/" #_c "/about/sw_ver", hdlr_invalid, RO, VERSION),                                        \
        DEFINE_FILE_PROP("rx/" #_c "/link/vita_en", hdlr_rx_##_c##_link_vita_en, RW, "0"),                             \
        DEFINE_FILE_PROP("rx/" #_c "/link/iface", hdlr_rx_##_c##_link_iface, RW, "sfpa"),                              \
        DEFINE_FILE_PROP("rx/" #_c "/link/port", hdlr_rx_##_c##_link_port, RW, #_p),                                   \
        DEFINE_FILE_PROP("rx/" #_c "/link/ip_dest", hdlr_rx_##_c##_link_ip_dest, RW, _ip),                             \
        DEFINE_FILE_PROP("rx/" #_c "/link/mac_dest", hdlr_rx_##_c##_link_mac_dest, RW, "ff:ff:ff:ff:ff:ff")

// XXX: @CF: if the port were encoded as an integer it would be easier to define
// XXX: @CF: we should be using pmt's for arguments to refactor common parsing code.
// XXX: @CF: handlers should be passed integers, doubles, and so on, not necessarily strings.
#define DEFINE_TX_CHANNEL(_c, _p)                                                                                      \
    DEFINE_SYMLINK_PROP("tx_" #_c, "tx/" #_c),                                                                         \
        DEFINE_FILE_PROP("tx/" #_c "/trigger/sma_mode", hdlr_tx_##_c##_trigger_sma_mode, RW, "level"),                 \
        DEFINE_FILE_PROP("tx/" #_c "/trigger/trig_sel", hdlr_tx_##_c##_trigger_trig_sel, RW, "0"),                     \
        DEFINE_FILE_PROP("tx/" #_c "/trigger/edge_backoff", hdlr_tx_##_c##_trigger_edge_backoff, RW, "0"),             \
        DEFINE_FILE_PROP("tx/" #_c "/trigger/edge_sample_num", hdlr_tx_##_c##_trigger_edge_sample_num, RW, "0"),       \
        DEFINE_FILE_PROP("tx/" #_c "/trigger/ufl_dir", hdlr_tx_##_c##_trigger_ufl_dir, RW, "out"),                     \
        DEFINE_FILE_PROP("tx/" #_c "/trigger/ufl_mode", hdlr_tx_##_c##_trigger_ufl_mode, RW, "level"),                 \
        DEFINE_FILE_PROP("tx/" #_c "/trigger/ufl_pol", hdlr_tx_##_c##_trigger_ufl_pol, RW, "negative"),                \
        DEFINE_FILE_PROP("tx/" #_c "/trigger/gating", hdlr_tx_##_c##_trigger_gating, RW, "output"),                    \
        DEFINE_FILE_PROP("tx/" #_c "/pwr", hdlr_tx_##_c##_pwr, RW, "0"),                                               \
        DEFINE_FILE_PROP("tx/" #_c "/sync", hdlr_tx_sync, WO, "0"),                                                    \
        DEFINE_FILE_PROP("tx/" #_c "/rf/dac/dither_en", hdlr_tx_##_c##_rf_dac_dither_en, RW, "0"),                     \
        DEFINE_FILE_PROP("tx/" #_c "/rf/dac/dither_mixer_en", hdlr_tx_##_c##_rf_dac_dither_mixer_en, RW, "0"),         \
        DEFINE_FILE_PROP("tx/" #_c "/rf/dac/dither_sra_sel", hdlr_tx_##_c##_rf_dac_dither_sra_sel, RW, "6"),           \
        DEFINE_FILE_PROP("tx/" #_c "/rf/dac/nco", hdlr_tx_##_c##_rf_dac_nco, RW, "0"),                                 \
        DEFINE_FILE_PROP("tx/" #_c "/rf/dac/temp", hdlr_tx_##_c##_rf_dac_temp, RW, "0"),                               \
        DEFINE_FILE_PROP("tx/" #_c "/rf/freq/val", hdlr_tx_##_c##_rf_freq_val, RW, "0"),                               \
        DEFINE_FILE_PROP("tx/" #_c "/rf/freq/lut_en", hdlr_tx_##_c##_rf_freq_lut_en, RW, "0"),                         \
        DEFINE_FILE_PROP("tx/" #_c "/rf/freq/band", hdlr_tx_##_c##_rf_freq_band, RW, "1"),                             \
        DEFINE_FILE_PROP("tx/" #_c "/rf/freq/i_bias", hdlr_tx_##_c##_rf_freq_i_bias, RW, "17"),                        \
        DEFINE_FILE_PROP("tx/" #_c "/rf/freq/q_bias", hdlr_tx_##_c##_rf_freq_q_bias, RW, "17"),                        \
        DEFINE_FILE_PROP("tx/" #_c "/rf/gain/val", hdlr_tx_##_c##_rf_gain_val, RW, "0"),                               \
        DEFINE_FILE_PROP("tx/" #_c "/status/rfpll_lock", hdlr_tx_##_c##_status_rfld, RW, "0"),                         \
        DEFINE_FILE_PROP("tx/" #_c "/status/dacpll_lock", hdlr_tx_##_c##_status_dacld, RW, "0"),                       \
        DEFINE_FILE_PROP("tx/" #_c "/status/dacpll_centre", hdlr_tx_##_c##_status_dacctr, RW, "0"),                    \
        DEFINE_FILE_PROP("tx/" #_c "/board/dump", hdlr_tx_##_c##_rf_board_dump, WO, "0"),                              \
        DEFINE_FILE_PROP("tx/" #_c "/board/test", hdlr_tx_##_c##_rf_board_test, WO, "0"),                              \
        DEFINE_FILE_PROP("tx/" #_c "/board/temp", hdlr_tx_##_c##_rf_board_temp, RW, "23"),                             \
        DEFINE_FILE_PROP("tx/" #_c "/board/led", hdlr_tx_##_c##_rf_board_led, WO, "0"),                                \
        DEFINE_FILE_PROP("tx/" #_c "/dsp/gain", hdlr_tx_##_c##_dsp_gain, RW, "10"),                                    \
        DEFINE_FILE_PROP("tx/" #_c "/dsp/rate", hdlr_tx_##_c##_dsp_rate, RW, "1258850"),                               \
        DEFINE_FILE_PROP("tx/" #_c "/dsp/nco_adj", hdlr_tx_##_c##_dsp_nco_adj, RW, "0"),                               \
        DEFINE_FILE_PROP("tx/" #_c "/dsp/rstreq", hdlr_tx_##_c##_dsp_rstreq, WO, "0"),                                 \
        DEFINE_FILE_PROP("tx/" #_c "/about/id", hdlr_tx_##_c##_about_id, RW, "001"),                                   \
        DEFINE_FILE_PROP("tx/" #_c "/about/serial", hdlr_tx_about_serial, RW, "001"),                                  \
        DEFINE_FILE_PROP("tx/" #_c "/about/mcudevid", hdlr_tx_about_mcudevid, RW, "001"),                              \
        DEFINE_FILE_PROP("tx/" #_c "/about/mcurev", hdlr_tx_about_mcurev, RW, "001"),                                  \
        DEFINE_FILE_PROP("tx/" #_c "/about/mcufuses", hdlr_tx_about_mcufuses, RW, "001"),                              \
        DEFINE_FILE_PROP("tx/" #_c "/about/fw_ver", hdlr_tx_about_fw_ver, RW, VERSION),                                \
        DEFINE_FILE_PROP("tx/" #_c "/about/sw_ver", hdlr_invalid, RO, VERSION),                                        \
        DEFINE_FILE_PROP("tx/" #_c "/link/vita_en", hdlr_tx_##_c##_link_vita_en, RW, "0"),                             \
        DEFINE_FILE_PROP("tx/" #_c "/link/iface", hdlr_tx_##_c##_link_iface, RW, "sfpa"),                              \
        DEFINE_FILE_PROP("tx/" #_c "/link/port", hdlr_tx_##_c##_link_port, RW, #_p),                                   \
        DEFINE_FILE_PROP("tx/" #_c "/qa/fifo_lvl", hdlr_tx_##_c##_qa_fifo_lvl, RW, #_p),                               \
        DEFINE_FILE_PROP("tx/" #_c "/qa/oflow", hdlr_tx_##_c##_qa_oflow, RW, #_p),                                     \
        DEFINE_FILE_PROP("tx/" #_c "/qa/uflow", hdlr_tx_##_c##_qa_uflow, RW, #_p)

#define DEFINE_TIME()                                                                                                  \
    DEFINE_FILE_PROP("time/clk/pps", hdlr_time_clk_pps, RW, "0"),                                                      \
        DEFINE_FILE_PROP("time/clk/cur_time", hdlr_time_clk_cur_time, RW, "0.0"),                                      \
        DEFINE_FILE_PROP("time/status/lmk_lockdetect", hdlr_time_status_ld, RW, "unlocked"),                           \
        DEFINE_FILE_PROP("time/status/lmk_lossoflock", hdlr_time_status_lol, RW, "unlocked"),                          \
        DEFINE_FILE_PROP("time/status/lmk_lockdetect_jesd_pll1", hdlr_time_status_ld_jesd_pll1, RW, "unlocked"),       \
        DEFINE_FILE_PROP("time/status/lmk_lockdetect_jesd_pll2", hdlr_time_status_ld_jesd_pll2, RW, "unlocked"),       \
        DEFINE_FILE_PROP("time/status/lmk_lockdetect_pll_pll1", hdlr_time_status_ld_pll_pll1, RW, "unlocked"),         \
        DEFINE_FILE_PROP("time/status/lmk_lockdetect_pll_pll2", hdlr_time_status_ld_pll_pll2, RW, "unlocked"),         \
        DEFINE_FILE_PROP("time/status/lmk_lossoflock_jesd_pll1", hdlr_time_status_lol_jesd_pll1, RW, "unlocked"),      \
        DEFINE_FILE_PROP("time/status/lmk_lossoflock_jesd_pll2", hdlr_time_status_lol_jesd_pll2, RW, "unlocked"),      \
        DEFINE_FILE_PROP("time/status/lmk_lossoflock_pll_pll1", hdlr_time_status_lol_pll_pll1, RW, "unlocked"),        \
        DEFINE_FILE_PROP("time/status/lmk_lossoflock_pll_pll2", hdlr_time_status_lol_pll_pll2, RW, "unlocked"),        \
        DEFINE_FILE_PROP("time/source/ref", hdlr_time_source_ref, RW, "internal"),                                     \
        DEFINE_FILE_PROP("time/source/extsine", hdlr_time_source_extsine, RW, "sine"),                                 \
        DEFINE_FILE_PROP("time/sync/lmk_sync_tgl_jesd", hdlr_time_sync_lmk_sync_tgl_jesd, WO, "0"),                    \
        DEFINE_FILE_PROP("time/sync/lmk_sync_tgl_pll", hdlr_time_sync_lmk_sync_tgl_pll, WO, "0"),                      \
        DEFINE_FILE_PROP("time/sync/lmk_sync_resync_jesd", hdlr_time_sync_lmk_resync_jesd, WO, "0"),                   \
        DEFINE_FILE_PROP("time/sync/lmk_sync_resync_pll", hdlr_time_sync_lmk_resync_pll, WO, "0"),                     \
        DEFINE_FILE_PROP("time/sync/lmk_resync_all", hdlr_time_sync_lmk_resync_all, WO, "0"),                          \
        DEFINE_FILE_PROP("time/board/dump", hdlr_time_board_dump, WO, "0"),                                            \
        DEFINE_FILE_PROP("time/board/test", hdlr_time_board_test, WO, "0"),                                            \
        DEFINE_FILE_PROP("time/board/temp", hdlr_time_board_temp, RW, "20"),                                           \
        DEFINE_FILE_PROP("time/board/led", hdlr_time_board_led, WO, "0"),                                              \
        DEFINE_FILE_PROP("time/about/id", hdlr_time_about_id, RO, "001"),                                              \
        DEFINE_FILE_PROP("time/about/serial", hdlr_time_about_serial, RW, "001"),                                      \
        DEFINE_FILE_PROP("time/about/mcudevid", hdlr_time_about_mcudevid, RW, "001"),                                  \
        DEFINE_FILE_PROP("time/about/mcurev", hdlr_time_about_mcurev, RW, "001"),                                      \
        DEFINE_FILE_PROP("time/about/mcufuses", hdlr_time_about_mcufuses, RW, "001"),                                  \
        DEFINE_FILE_PROP("time/about/fw_ver", hdlr_time_about_fw_ver, RW, VERSION),                                    \
        DEFINE_FILE_PROP("time/about/sw_ver", hdlr_invalid, RO, VERSION)

#define DEFINE_FPGA()                                                                                                  \
    DEFINE_FILE_PROP("fpga/trigger/sma_dir", hdlr_fpga_trigger_sma_dir, RW, "out"),                                    \
        DEFINE_FILE_PROP("fpga/trigger/sma_pol", hdlr_fpga_trigger_sma_pol, RW, "negative"),                           \
        DEFINE_FILE_PROP("fpga/about/fw_ver", hdlr_fpga_about_fw_ver, RW, VERSION),                                    \
        DEFINE_FILE_PROP("fpga/about/server_ver", hdlr_server_about_fw_ver, RW, ""),                                   \
        DEFINE_FILE_PROP("fpga/about/hw_ver", hdlr_fpga_about_hw_ver, RW, VERSION),                                    \
        DEFINE_FILE_PROP("fpga/about/id", hdlr_fpga_about_id, RW, "001"),                                              \
        DEFINE_FILE_PROP("fpga/about/name", hdlr_invalid, RO, "crimson_tng"),                                          \
        DEFINE_FILE_PROP("fpga/about/serial", hdlr_fpga_about_serial, RW, "001"),                                      \
        DEFINE_FILE_PROP("fpga/about/cmp_time", hdlr_fpga_about_cmp_time, RW, "yyyy-mm-dd-hh-mm"),                     \
        DEFINE_FILE_PROP("fpga/about/conf_info", hdlr_fpga_about_conf_info, RW, "0"),                                  \
        DEFINE_FILE_PROP("fpga/board/dump", hdlr_fpga_board_dump, WO, "0"),                                            \
        DEFINE_FILE_PROP("fpga/board/fw_rst", hdlr_fpga_board_fw_rst, WO, "0"),                                        \
        DEFINE_FILE_PROP("fpga/board/flow_control/sfpa_port", hdlr_fpga_board_flow_control_sfpa_port, RW, "42809"),    \
        DEFINE_FILE_PROP("fpga/board/flow_control/sfpb_port", hdlr_fpga_board_flow_control_sfpb_port, RW, "42809"),    \
        DEFINE_FILE_PROP("fpga/board/gps_time", hdlr_fpga_board_gps_time, RW, "0"),                                    \
        DEFINE_FILE_PROP("fpga/board/gps_frac_time", hdlr_fpga_board_gps_frac_time, RW, "0"),                          \
        DEFINE_FILE_PROP("fpga/board/gps_sync_time", hdlr_fpga_board_gps_sync_time, RW, "0"),                          \
        DEFINE_FILE_PROP("fpga/board/jesd_sync", hdlr_fpga_board_jesd_sync, WO, "0"),                                  \
        DEFINE_FILE_PROP("fpga/board/led", hdlr_fpga_board_led, WO, "0"),                                              \
        DEFINE_FILE_PROP("fpga/board/rstreq", hdlr_fpga_board_rstreq, WO, "0"),                                        \
        DEFINE_FILE_PROP("fpga/board/reboot", hdlr_fpga_board_reboot, RW, "0"),                                        \
        DEFINE_FILE_PROP("fpga/board/sys_rstreq", hdlr_fpga_board_sys_rstreq, WO, "0"),                                \
        DEFINE_FILE_PROP("fpga/board/test", hdlr_fpga_board_test, WO, "0"),                                            \
        DEFINE_FILE_PROP("fpga/board/temp", hdlr_fpga_board_temp, RW, "20"),                                           \
        DEFINE_FILE_PROP("fpga/board/gle", hdlr_fpga_board_gle, RW, "0"),                                              \
        DEFINE_FILE_PROP("fpga/link/rate", hdlr_fpga_link_rate, RW, "1250000000"),                                     \
        DEFINE_FILE_PROP("fpga/link/sfpa/ip_addr", hdlr_fpga_link_sfpa_ip_addr, RW, "10.10.10.2"),                     \
        DEFINE_FILE_PROP("fpga/link/sfpa/mac_addr", hdlr_fpga_link_sfpa_mac_addr, RW, "aa:00:00:00:00:00"),            \
        DEFINE_FILE_PROP("fpga/link/sfpa/ver", hdlr_fpga_link_sfpa_ver, RW, "0"),                                      \
        DEFINE_FILE_PROP("fpga/link/sfpa/pay_len", hdlr_fpga_link_sfpa_pay_len, RW, "1400"),                           \
        DEFINE_FILE_PROP("fpga/link/sfpb/ip_addr", hdlr_fpga_link_sfpb_ip_addr, RW, "10.10.11.2"),                     \
        DEFINE_FILE_PROP("fpga/link/sfpb/mac_addr", hdlr_fpga_link_sfpb_mac_addr, RW, "aa:00:00:00:00:01"),            \
        DEFINE_FILE_PROP("fpga/link/sfpb/ver", hdlr_fpga_link_sfpb_ver, RW, "0"),                                      \
        DEFINE_FILE_PROP("fpga/link/sfpb/pay_len", hdlr_fpga_link_sfpb_pay_len, RW, "1400"),                           \
        DEFINE_FILE_PROP("fpga/link/net/dhcp_en", hdlr_fpga_link_net_dhcp_en, RW, "0"),                                \
        DEFINE_FILE_PROP("fpga/link/net/hostname", hdlr_fpga_link_net_hostname, RW, "crimson_tng"),                    \
        DEFINE_FILE_PROP("fpga/link/net/ip_addr", hdlr_fpga_link_net_ip_addr, RW, "192.168.10.2")

#define DEFINE_CM()                                                                                                    \
    DEFINE_FILE_PROP("cm/chanmask-rx", hdlr_cm_chanmask_rx, RW, "0"),                                                  \
        DEFINE_FILE_PROP("cm/chanmask-tx", hdlr_cm_chanmask_tx, RW, "0"),                                              \
        DEFINE_FILE_PROP("cm/rx/atten/val", hdlr_cm_rx_atten_val, WO, "0"),                                            \
        DEFINE_FILE_PROP("cm/rx/gain/val", hdlr_cm_rx_gain_val, WO, "0"),                                              \
        DEFINE_FILE_PROP("cm/tx/gain/val", hdlr_cm_tx_gain_val, WO, "0"),                                              \
        DEFINE_FILE_PROP("cm/trx/freq/val", hdlr_cm_trx_freq_val, WO, "0"),                                            \
        DEFINE_FILE_PROP("cm/trx/nco_adj", hdlr_cm_trx_nco_adj, WO, "0")

// Beginning of property table
static prop_t property_table[] = {

    DEFINE_RX_CHANNEL(a, 42820, "10.10.10.10"),
    DEFINE_RX_CHANNEL(b, 42821, "10.10.11.10"),
    DEFINE_RX_CHANNEL(c, 42822, "10.10.10.10"),
    DEFINE_RX_CHANNEL(d, 42823, "10.10.11.10"),

    DEFINE_TX_CHANNEL(a, 42824),
    DEFINE_TX_CHANNEL(b, 42825),
    DEFINE_TX_CHANNEL(c, 42826),
    DEFINE_TX_CHANNEL(d, 42827),

    DEFINE_TIME(),
    DEFINE_FPGA(),

    DEFINE_FILE_PROP("save_config", hdlr_save_config, RW, "/home/root/profile.cfg"),
    DEFINE_FILE_PROP("load_config", hdlr_load_config, RW, "/home/root/profile.cfg"),

    DEFINE_CM(),
};
static size_t num_properties = sizeof(property_table) / sizeof(property_table[0]);

// Beginning of functions
size_t get_num_prop(void) { return num_properties; }

prop_t *get_prop(size_t idx) { return (property_table + idx); }

prop_t *get_prop_from_wd(int wd) {
    size_t i;
    for (i = 0; i < num_properties; i++) {
        if (property_table[i].wd == wd)
            return (property_table + i);
    }

    // no matching prop found
    return NULL;
}

prop_t *get_prop_from_hdlr(int (*hdlr)(const char *, char *)) {
    size_t i;
    for (i = 0; i < num_properties; i++) {
        if (property_table[i].handler == hdlr) {
            return &property_table[i];
        }
    }

    // no matching prop found
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
    //		PRINT( INFO, "%s(): resolved symbolic link: '%s' => '%s'\n", __func__, prop, path );
    //	}

    return RETURN_SUCCESS;
}

prop_t *get_prop_from_cmd(const char *cmd) {
    char path[MAX_PATH_LEN];
    size_t i;

    if (RETURN_SUCCESS == resolve_symbolic_property_name(cmd, path, sizeof(path))) {
        cmd = path;
    }

    for (i = 0; i < num_properties; i++) {
        if ((strcmp(property_table[i].path, cmd) == 0) && (strlen(property_table[i].path) == strlen(cmd)))
            return (property_table + i);
    }

    // no matching prop found
    return NULL;
}

static inline const char *get_home_dir(void) { return getpwuid(getuid())->pw_dir; }

void pass_uart_synth_fd(int fd) { uart_synth_fd = fd; }

void pass_uart_tx_fd(int fd) { uart_tx_fd = fd; }

void pass_uart_rx_fd(int fd) { uart_rx_fd = fd; }

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

void pass_profile_pntr_prop(uint8_t *load, uint8_t *save, char *load_path, char *save_path) {
    _load_profile = load;
    _save_profile = save;
    _load_profile_path = load_path;
    _save_profile_path = save_path;
}

void sync_channels(uint8_t chan_mask) {
    char str_chan_mask[MAX_PROP_LEN] = "";
    sprintf(str_chan_mask + strlen(str_chan_mask), "%" PRIu8 "", 15);
    // usleep(300000); // Some wait time for the reset to be ready
    /* Bring the ADCs & DACs into 'demo' mode for JESD */

    // RX - ADCs
    strcpy(buf, "power -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -a 1\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);

    // TX - DACs
    strcpy(buf, "power -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -d 1\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);

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
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
        read_uart(uart_rx_fd);
        strcpy(buf, "board -c ");
        strcat(buf, str_chan_mask);
        strcat(buf, " -s 1\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
        read_uart(uart_tx_fd);

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
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
        read_uart(uart_tx_fd);
        usleep(100000); // Some wait time for MCUs to be ready
        strcpy(buf, "clk -y\r");
        send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        read_uart(uart_synth_fd);
        usleep(100000); // Some wait time for MCUs to be ready

        // CHECK IF ALARMS
        strcpy(buf, "dac -c a -s\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
        read_uart(uart_tx_fd);

        strcpy(dacalarmA, (char *)uart_ret_buf);

        // CHECK IF ALARMS
        strcpy(buf, "dac -c d -s\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
        read_uart(uart_tx_fd);

        strcpy(dacalarmB, (char *)uart_ret_buf);

        if ((dacalarmA[0] == key[0]) && (dacalarmA[1] == key[1]) && (dacalarmB[0] == key[0]) &&
            (dacalarmB[1] == key[1])) {
            break;
        } else {
            usleep(200000); // Some wait time for MCUs to be ready
        }
    }
    /* Turn off all boards' SYSREF detection gates */
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 0\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 0\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);

#else
    // Put FPGA JESD core in reset
    write_hps_reg("res_rw7", 0x20000000);
    write_hps_reg("res_rw7", 0);

    /* Initiate the SYSREF sequence for jesd
     * Set all boards' SYSREF detection gate to ON */
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 1\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 1\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);

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
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 0\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);

#endif
}

void set_pll_frequency(int uart_fd, uint64_t reference, pllparam_t *pll, bool tx, size_t channel) {
    // extract pll1 variables and pass to MCU (ADF4355/ADF5355)

    // Send Reference to MCU ( No Need ATM since fixed reference )
    strcpy(buf, "rf -v ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(reference / 1000)); // Send reference in kHz
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
    sprintf(buf + strlen(buf), "%" PRIu8 "", 1 /*pll->power*/); // default to lower mid power
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

    double freq = pll->vcoFreq / pll->d;

    if (synth_lut_is_enabled(tx, channel)) {
        synth_rec_t rec;
        int ret = synth_lut_get(tx, channel, freq, &rec);
        if (EXIT_SUCCESS != ret) {
            PRINT(ERROR, "synth_lut_get( %u, %u, %f ) failed (%d,%s)\n", tx, channel, freq, ret, strerror(ret));
        } else {
            PRINT(INFO, "Setting %s %c @ %u MHz with parameters { %u, %u, %u }\n", tx ? "TX" : "RX", 'A' + channel,
                  (unsigned)(freq / 1000000), rec.core, rec.band, rec.bias);
            snprintf(buf, sizeof(buf), "rf -c %c -A 0 -C %u -B %u -I %u\r", 'a' + channel, rec.core, rec.band,
                     rec.bias);
            send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));
        }
    } else {
        // If synth lut is disabled, set autocal flat to enable fall-back behaviour.
        snprintf(buf, sizeof(buf), "rf -c %c -A 1\r", 'a' + channel);
        send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));
    }

    // ADF output power level not presently specified.
    strcpy(buf, "rf -g ");

    // write ADF4355/ADF5355 Output Frequency
    strcpy(buf, "rf -f ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(freq / 1000)); // Send output frequency in kHz
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

    usleep(100000);
}

int set_pll_frequency2(int actual_uart_fd, uint64_t reference, pllparam_t *pll) {
    int r;
    // extract pll1 variables and pass to MCU (ADF4355/ADF5355)

    // Send Reference to MCU ( No Need ATM since fixed reference )
    snprintf(buf, sizeof(buf), "rf -v %" PRIu32 "\r", (uint32_t)(reference / 1000)); // Send reference in kHz
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
    snprintf(buf, sizeof(buf), "rf -f %" PRIu32 "\r", (uint32_t)(pll->vcoFreq / pll->d / 1000));
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
        PRINT(ERROR, "failed to send command '%s' (%d,%s)\n", buf, errno, strerror(errno));
    }

    return r;
}

int set_freq_internal(const bool tx, const unsigned channel, const double freq) {

    typedef int (*fp_t)(const char *, char *);

    static const fp_t rx_fp[] = {
        hdlr_rx_a_rf_freq_val,
        hdlr_rx_b_rf_freq_val,
        hdlr_rx_c_rf_freq_val,
        hdlr_rx_d_rf_freq_val,
    };

    static const fp_t tx_fp[] = {
        hdlr_tx_a_rf_freq_val,
        hdlr_tx_b_rf_freq_val,
        hdlr_tx_c_rf_freq_val,
        hdlr_tx_d_rf_freq_val,
    };

    int r;

    char req_buf[MAX_PROP_LEN];
    char rsp_buf[MAX_PROP_LEN];

    if (channel > (tx ? ARRAY_SIZE(tx_fp) : ARRAY_SIZE(rx_fp))) {
        r = E2BIG;
        PRINT(ERROR, "channel %u is invalid (%d,%s)\n", channel, r, strerror(r));
        goto out;
    }

    const fp_t *fp = tx ? tx_fp : rx_fp;

    memset(req_buf, '\0', sizeof(req_buf));
    memset(rsp_buf, '\0', sizeof(rsp_buf));

    // N.B. the print formatter in this case must be equal to the one in hdlr_XX_X_rf_freq_val
    snprintf(req_buf, sizeof(req_buf), "%lf", freq);

    r = fp[channel](req_buf, rsp_buf);
    if (RETURN_SUCCESS != r) {
        PRINT(ERROR, "function call to hdlr_XX_X_rf_freq_val() failed (%d)\n", r);
        r = EIO;
        goto out;
    }

    double actual_freq = 0;
    if (1 != sscanf(rsp_buf, "%lf", &actual_freq) || actual_freq != freq) {
        r = EIO;
        PRINT(ERROR, "%s %c: expected: %f, actual: %f\n", tx ? "TX" : "RX", 'A' + channel, freq, actual_freq);
        goto out;
    }

    flush_uart_comm(tx ? uart_tx_fd : uart_rx_fd);

    r = EXIT_SUCCESS;

out:
    return r;
}
