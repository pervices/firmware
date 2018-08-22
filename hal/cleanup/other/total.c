/*
 * Properties are written once and expanded for the number of channels specified via
 * a C-like template system known as the X-Macro: https://en.wikipedia.org/wiki/X_Macro
 *
 * Don't forget to line up the backslashes when you're done.
 */

#define STR(ch) #ch
#define CHR(ch) #ch[0]
#define INT(ch) (CHR(ch) - 'a')

#define LIST_OF_CHANNELS                                                                                               \
    X(a)

/* ================================================================================================================== */
/* ----------------------------------------------------- MISC ------------------------------------------------------- */
/* ================================================================================================================== */

static int hdlr_tx_sync(const char *data, char *ret) {
    uint32_t old_val;

    // toggle the bit sys0[6]
    read_hps_reg("sys0", &old_val);
    write_hps_reg("sys0", old_val | 0x40);
    write_hps_reg("sys0", old_val & (~0x40));

    return RETURN_SUCCESS;
}

/* ================================================================================================================== */
/* ------------------------------------------------------- TX ------------------------------------------------------- */
/* ================================================================================================================== */

#define X(ch)                                                                                                          \
    static int hdlr_tx_##ch##_rf_dac_dither_en(const char *data, char *ret) {                                          \
        int r;                                                                                                         \
        int en;                                                                                                        \
                                                                                                                       \
        r = sscanf(data, "%d", &en);                                                                                   \
        if (1 != r) {                                                                                                  \
            return RETURN_ERROR;                                                                                       \
        }                                                                                                              \
        if (en < 0 || en > 1) {                                                                                        \
            return RETURN_ERROR_PARAM;                                                                                 \
        }                                                                                                              \
        snprintf(buf, sizeof(buf), "dac -c " STR(ch) " -l %u\r", en);                                                  \
        sprintf(ret, "%u", en);                                                                                        \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_dac_dither_mixer_en(const char *data, char *ret) {                                    \
        int r;                                                                                                         \
        int en;                                                                                                        \
                                                                                                                       \
        r = sscanf(data, "%d", &en);                                                                                   \
        if (1 != r) {                                                                                                  \
            return RETURN_ERROR;                                                                                       \
        }                                                                                                              \
        if (en < 0 || en > 1) {                                                                                        \
            return RETURN_ERROR_PARAM;                                                                                 \
        }                                                                                                              \
        snprintf(buf, sizeof(buf), "dac -c " STR(ch) " -3 %u\r", en);                                                  \
        sprintf(ret, "%u", en);                                                                                        \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_dac_dither_sra_sel(const char *data, char *ret) {                                     \
        int r;                                                                                                         \
        int db;                                                                                                        \
        int sel;                                                                                                       \
                                                                                                                       \
        r = sscanf(data, "%d", &db);                                                                                   \
        if (1 != r) {                                                                                                  \
            return RETURN_ERROR;                                                                                       \
        }                                                                                                              \
        if (db < 6 || db > 96 || 0 != db % 6) {                                                                        \
            return RETURN_ERROR_PARAM;                                                                                 \
        }                                                                                                              \
                                                                                                                       \
        sprintf(ret, "%u", db);                                                                                        \
                                                                                                                       \
        /* 96 dB (max) := 0x0, 90 dB := 0x1, ... 6 dB (min) := 0xf */                                                  \
        sel = 16 - (db / 6);                                                                                           \
                                                                                                                       \
        snprintf(buf, sizeof(buf), "dac -c " STR(ch) " -b %u\r", sel);                                                 \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_dac_nco(const char *data, char *ret) {                                                \
        double freq;                                                                                                   \
        sscanf(data, "%lf", &freq);                                                                                    \
        uint64_t nco_steps = (uint64_t)round(freq * DAC_NCO_CONST);                                                    \
        sprintf(ret, "%lf", (double)nco_steps / DAC_NCO_CONST);                                                        \
                                                                                                                       \
        strcpy(buf, "dac -c " STR(ch) " -e 0 -n ");                                                                    \
        sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));                                        \
        strcat(buf, "\r");                                                                                             \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
                                                                                                                       \
        strcpy(buf, "dac -o ");                                                                                        \
        sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);                                                \
        strcat(buf, "\r");                                                                                             \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_dac_temp(const char *data, char *ret) {                                               \
        strcpy(buf, "board -c " STR(ch) " -t\r");                                                                      \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_tx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_freq_val(const char *data, char *ret) {                                               \
        uint64_t freq = 0;                                                                                             \
        sscanf(data, "%" SCNd64 "", &freq);                                                                            \
                                                                                                                       \
        /* if freq = 0, mute PLL */                                                                                    \
        if (freq == 0) {                                                                                               \
            strcpy(buf, "rf -c " STR(ch) " -z\r");                                                                     \
            send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                          \
                                                                                                                       \
            return RETURN_SUCCESS;                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        /* if freq out of bounds, kill channel*/                                                                       \
        if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {                                                \
            strcpy(buf, "board -c " STR(ch) " -k\r");                                                                  \
            send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                          \
                                                                                                                       \
            /* Turn OFF TX on HPS */                                                                                   \
            uint32_t old_val;                                                                                          \
                                                                                                                       \
            /* disable DSP cores */                                                                                    \
            read_hps_reg("tx" STR(ch) "4", &old_val);                                                                  \
            PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper(CHAR(ch)));                                       \
            write_hps_reg("tx" STR(ch) "4", old_val | 0x2);                                                            \
                                                                                                                       \
            /* disable channel */                                                                                      \
            read_hps_reg("tx" STR(ch) "4", &old_val);                                                                  \
            write_hps_reg("tx" STR(ch) "4", old_val &(~0x100));                                                        \
                                                                                                                       \
            tx_power[INT(ch)] = PWR_OFF;                                                                               \
                                                                                                                       \
            PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "                                               \
                         "Shutting Down TX" STR(ch) ".\n");                                                            \
                                                                                                                       \
            return RETURN_ERROR;                                                                                       \
        }                                                                                                              \
                                                                                                                       \
        /* run the pll calc algorithm */                                                                               \
        pllparam_t pll;                                                                                                \
        long double outfreq = 0;                                                                                       \
        outfreq = setFreq(&freq, &pll);                                                                                \
                                                                                                                       \
        strcpy(buf, "rf -c " STR(ch) " \r");                                                                           \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
                                                                                                                       \
        /* TODO: pll1.power setting TBD (need to modify pllparam_t) */                                                 \
                                                                                                                       \
        /* Send Parameters over to the MCU */                                                                          \
        set_pll_frequency(uart_tx_fd[INT(ch)], (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true, INT(ch));                   \
                                                                                                                       \
        sprintf(ret, "%lf", outfreq);                                                                                  \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_freq_band(const char *data, char *ret) {                                              \
        strcpy(buf, "rf -c " STR(ch) " -b ");                                                                          \
        strcat(buf, data);                                                                                             \
        strcat(buf, "\r");                                                                                             \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_freq_i_bias(const char *data, char *ret) {                                            \
        sscanf(data, "%i", &(i_bias[INT(ch)]));                                                                        \
        strcpy(buf, "rf -c " STR(ch) " -i ");                                                                          \
        sprintf(buf + strlen(buf), "%i", i_bias[INT(ch)]);                                                             \
        strcat(buf, " -q ");                                                                                           \
        sprintf(buf + strlen(buf), "%i", q_bias[INT(ch)]);                                                             \
        strcat(buf, " -m\r");                                                                                          \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_freq_q_bias(const char *data, char *ret) {                                            \
        sscanf(data, "%i", &(q_bias[INT(ch)]));                                                                        \
        strcpy(buf, "rf -c " STR(ch) " -i ");                                                                          \
        sprintf(buf + strlen(buf), "%i", i_bias[INT(ch)]);                                                             \
        strcat(buf, " -q ");                                                                                           \
        sprintf(buf + strlen(buf), "%i", q_bias[INT(ch)]);                                                             \
        strcat(buf, " -m\r");                                                                                          \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_gain_val(const char *data, char *ret) {                                               \
        int gain;                                                                                                      \
        sscanf(data, "%i", &gain);                                                                                     \
                                                                                                                       \
        /*   0 -> 126 attenuation only */                                                                              \
        /* 127    0dB */                                                                                               \
                                                                                                                       \
        if (gain > 127)                                                                                                \
            gain = 127;                                                                                                \
        else if (gain < 0)                                                                                             \
            gain = 0;                                                                                                  \
                                                                                                                       \
        strcpy(buf, "rf -c " STR(ch) " -a ");                                                                          \
        sprintf(buf + strlen(buf), "%i", 127 - gain);                                                                  \
        strcat(buf, "\r");                                                                                             \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_board_dump(const char *data, char *ret) {                                             \
        /* send the uart commands and read back the output and write to file                                           \
         */                                                                                                            \
                                                                                                                       \
        /* DAC */                                                                                                      \
        strcpy(buf, "dump -c " STR(ch) " -d\r");                                                                       \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_tx_fd[INT(ch)]);                                                                                \
        PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);                                                     \
                                                                                                                       \
        /* GPIOX */                                                                                                    \
        strcpy(buf, "dump -c " STR(ch) " -g\r");                                                                       \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_tx_fd[INT(ch)]);                                                                                \
        PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);                                                   \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_board_test(const char *data, char *ret) {                                             \
        /* TODO: MCU code cleanup */                                                                                   \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_board_temp(const char *data, char *ret) {                                             \
        strcpy(buf, "board -c " STR(ch) " -t\r");                                                                      \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_tx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_status_rfld(const char *data, char *ret) {                                               \
        strcpy(buf, "status -c " STR(ch) " -l\r");                                                                     \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_tx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_status_dacld(const char *data, char *ret) {                                              \
        strcpy(buf, "status -c " STR(ch) " -p\r");                                                                     \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_tx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_status_dacctr(const char *data, char *ret) {                                             \
        strcpy(buf, "status -c " STR(ch) " -e\r");                                                                     \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_tx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_rf_board_led(const char *data, char *ret) {                                              \
        strcpy(buf, "board -l\r");                                                                                     \
        strcat(buf, data);                                                                                             \
        strcat(buf, "\r");                                                                                             \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_dsp_gain(const char *data, char *ret) {                                                  \
        /* TODO: FW code */                                                                                            \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_dsp_rate(const char *data, char *ret) {                                                  \
        uint32_t old_val;                                                                                              \
        uint16_t base_factor, resamp_factor;                                                                           \
        double base_err, resamp_err;                                                                                   \
        double rate;                                                                                                   \
        sscanf(data, "%lf", &rate);                                                                                    \
                                                                                                                       \
        /* get the error for base rate */                                                                              \
        base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);                                        \
        resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);                                  \
                                                                                                                       \
        /* set the appropriate sample rate */                                                                          \
        memset(ret, 0, MAX_PROP_LEN);                                                                                  \
                                                                                                                       \
        if (resamp_err < base_err) {                                                                                   \
            write_hps_reg("tx" STR(ch) "1", resamp_factor);                                                            \
            read_hps_reg("tx" STR(ch) "4", &old_val);                                                                  \
            write_hps_reg("tx" STR(ch) "4", old_val | (1 << 15));                                                      \
            sprintf(ret, "%lf", RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));                                     \
            /* Set gain adjustment */                                                                                  \
            read_hps_reg("txg" STR(ch), &old_val);                                                                     \
            write_hps_reg("txg" STR(ch), (old_val & ~(0xff << 0)) | (interp_gain_lut[(resamp_factor)] << 0));          \
        } else {                                                                                                       \
            write_hps_reg("tx" STR(ch) "1", base_factor);                                                              \
            read_hps_reg("tx" STR(ch) "4", &old_val);                                                                  \
            write_hps_reg("tx" STR(ch) "4", old_val & ~(1 << 15));                                                     \
            sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1));                                         \
            /* Set gain adjustment */                                                                                  \
            read_hps_reg("txg" STR(ch), &old_val);                                                                     \
            write_hps_reg("txg" STR(ch), (old_val & ~(0xff << 0)) | (interp_gain_lut[(base_factor)] << 0));            \
        }                                                                                                              \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_dsp_nco_adj(const char *data, char *ret) {                                               \
        double freq;                                                                                                   \
        uint32_t old_val;                                                                                              \
        uint8_t direction;                                                                                             \
                                                                                                                       \
        /* check for a minus or plus sign at the front */                                                              \
        if (data[0] == '-') {                                                                                          \
            sscanf(data + 1, "%lf", &freq);                                                                            \
            direction = 1;                                                                                             \
        } else if (data[0] == '+') {                                                                                   \
            sscanf(data + 1, "%lf", &freq);                                                                            \
            direction = 0;                                                                                             \
        } else {                                                                                                       \
            sscanf(data, "%lf", &freq);                                                                                \
            direction = 0;                                                                                             \
        }                                                                                                              \
                                                                                                                       \
        /* write NCO adj */                                                                                            \
        uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);                                                    \
        write_hps_reg("tx" STR(ch) "0", nco_steps);                                                                    \
        if (direction > 0) {                                                                                           \
            sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);                                                   \
        } else {                                                                                                       \
            sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);                                                    \
        }                                                                                                              \
                                                                                                                       \
        /* write direction */                                                                                          \
        read_hps_reg("tx" STR(ch) "4", &old_val);                                                                      \
        write_hps_reg("tx" STR(ch) "4", (old_val & ~(0x1 << 13)) | (direction << 13));                                 \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_dsp_rstreq(const char *data, char *ret) {                                                \
        uint32_t old_val;                                                                                              \
        read_hps_reg("tx" STR(ch) "4", &old_val);                                                                      \
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper(CHR(ch)));                                            \
        write_hps_reg("tx" STR(ch) "4", old_val | 0x2);                                                                \
        write_hps_reg("tx" STR(ch) "4", old_val & ~0x2);                                                               \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_about_id(const char *data, char *ret) {                                                  \
        /* don't need to do anything, save the ID in the file system */                                                \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_link_vita_en(const char *data, char *ret) {                                              \
        uint32_t old_val;                                                                                              \
        read_hps_reg("tx" STR(ch) "4", &old_val);                                                                      \
        if (strcmp(data, "1") == 0)                                                                                    \
            write_hps_reg("tx" STR(ch) "4", old_val | (1 << 14));                                                      \
        else                                                                                                           \
            write_hps_reg("tx" STR(ch) "4", old_val & ~(1 << 14));                                                     \
                                                                                                                       \
        /* sync_channels( 15 ); */                                                                                     \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_link_iface(const char *data, char *ret) {                                                \
        /* TODO: FW support for streaming to management port required */                                               \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_link_port(const char *data, char *ret) {                                                 \
        uint32_t port;                                                                                                 \
        sscanf(data, "%" SCNd32 "", &port);                                                                            \
        write_hps_reg("tx" STR(ch) "5", port);                                                                         \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    /* WARNING:                                                                                                        \
     * DOES NOT PORT WELL.                                                                                             \
     * r04 uses different offsets for channels starting at index 4? */                                                 \
    static int hdlr_tx_##ch##_qa_fifo_lvl(const char *data, char *ret) {                                               \
        uint32_t lvl;                                                                                                  \
        read_hps_reg("res_ro4", &lvl);                                                                                 \
        lvl &= 0xffff;                                                                                                 \
        sprintf(ret, "%u", lvl);                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    /* WARNING:                                                                                                        \
       DOES NOT PORT WELL.                                                                                             \
       flc14 uses different offsets for chanenls starting at index 14? */                                              \
    static int hdlr_tx_##ch##_qa_oflow(const char *data, char *ret) {                                                  \
        uint32_t count;                                                                                                \
        /* this is technically a 64-bit register, but we currently only need                                           \
         * the bottom 32-bits */                                                                                       \
        read_hps_reg("flc14", &count);                                                                                 \
        sprintf(ret, "%u", count);                                                                                     \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    /* WARNING:                                                                                                        \
     * DOES NOT PORT WELL.                                                                                             \
     * flc6 uses different offsets for channels starting at index 6? */                                                \
    static int hdlr_tx_##ch##_qa_uflow(const char *data, char *ret) {                                                  \
        uint32_t count;                                                                                                \
        /* this is technically a 64-bit register, but we currently only need                                           \
         * the bottom 32-bits */                                                                                       \
        read_hps_reg("flc6", &count);                                                                                  \
        sprintf(ret, "%u", count);                                                                                     \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_pwr(const char *data, char *ret) {                                                       \
        uint32_t old_val;                                                                                              \
        uint8_t power;                                                                                                 \
        uint8_t i;                                                                                                     \
        sscanf(data, "%" SCNd8 "", &power);                                                                            \
                                                                                                                       \
        /* check if power is already enabled */                                                                        \
        if (power >= PWR_ON && tx_power[INT(ch)] == PWR_ON)                                                            \
            return RETURN_SUCCESS;                                                                                     \
                                                                                                                       \
        /* power on */                                                                                                 \
        if (power >= PWR_ON) {                                                                                         \
            tx_power[INT(ch)] = PWR_ON;                                                                                \
                                                                                                                       \
            /* board commands */                                                                                       \
            strcpy(buf, "board -c " STR(ch) " -d\r");                                                                  \
            send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                          \
            usleep(200000);                                                                                            \
                                                                                                                       \
            /* disable dsp channels */                                                                                 \
            for (i = 0; i < (NUM_CHANNELS * 2); i++) {                                                                 \
                read_hps_reg(reg4[i], &old_val);                                                                       \
                write_hps_reg(reg4[i], old_val & ~0x100);                                                              \
            }                                                                                                          \
                                                                                                                       \
            /* send sync pulse */                                                                                      \
            sync_channels(15);                                                                                         \
                                                                                                                       \
            /* enable active dsp channels, and reset the DSP */                                                        \
            for (i = 0; i < NUM_CHANNELS; i++) {                                                                       \
                if (tx_power[i] == PWR_ON) {                                                                           \
                    read_hps_reg(reg4[i + 4], &old_val);                                                               \
                    write_hps_reg(reg4[i + 4], old_val | 0x100);                                                       \
                    read_hps_reg(reg4[i + 4], &old_val);                                                               \
                    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper(CHAR(ch)));                               \
                    write_hps_reg(reg4[i + 4], old_val | 0x2);                                                         \
                    write_hps_reg(reg4[i + 4], old_val &(~0x2));                                                       \
                }                                                                                                      \
                if (rx_power[i] == PWR_ON) {                                                                           \
                    read_hps_reg(reg4[i], &old_val);                                                                   \
                    write_hps_reg(reg4[i], old_val | 0x100);                                                           \
                    read_hps_reg(reg4[i], &old_val);                                                                   \
                    write_hps_reg(reg4[i], old_val | 0x2);                                                             \
                    write_hps_reg(reg4[i], old_val &(~0x2));                                                           \
                }                                                                                                      \
            }                                                                                                          \
                                                                                                                       \
            /* power off */                                                                                            \
        } else {                                                                                                       \
            /* kill the channel */                                                                                     \
            strcpy(buf, "board -c " STR(ch) " -k\r");                                                                  \
            send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                          \
                                                                                                                       \
            /* disable DSP cores */                                                                                    \
            read_hps_reg("tx" STR(ch) "4", &old_val);                                                                  \
            PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper(CHAR(ch)));                                       \
            write_hps_reg("tx" STR(ch) "4", old_val | 0x2);                                                            \
                                                                                                                       \
            /* disable channel */                                                                                      \
            read_hps_reg("tx" STR(ch) "4", &old_val);                                                                  \
            write_hps_reg("tx" STR(ch) "4", old_val &(~0x100));                                                        \
                                                                                                                       \
            tx_power[INT(ch)] = PWR_OFF;                                                                               \
        }                                                                                                              \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_about_serial(const char *data, char *ret) {                                              \
        strcpy(buf, "status -s\r");                                                                                    \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_tx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##about_mcudevid(const char *data, char *ret) {                                             \
        strcpy(buf, "status -d\r");                                                                                    \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_tx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##about_mcurev(const char *data, char *ret) {                                               \
        strcpy(buf, "status -v\r");                                                                                    \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_tx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##about_mcufuses(const char *data, char *ret) {                                             \
        strcpy(buf, "status -f\r");                                                                                    \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_tx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##about_fw_ver(const char *data, char *ret) {                                               \
        strcpy(buf, "board -v\r");                                                                                     \
        send_uart_comm(uart_tx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_tx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }

LIST_OF_CHANNELS
#undef X

/* ================================================================================================================== */
/* ------------------------------------------------------- RX ------------------------------------------------------- */
/* ================================================================================================================== */

#define X(ch)                                                                                                          \
    static int hdlr_rx_##ch##_rf_freq_val(const char *data, char *ret) {                                               \
        uint64_t freq = 0;                                                                                             \
        sscanf(data, "%" SCNd64 "", &freq);                                                                            \
                                                                                                                       \
        /* if freq = 0, mute PLL */                                                                                    \
        if (freq == 0) {                                                                                               \
            strcpy(buf, "rf -c " STR(ch) " -z\r");                                                                     \
            send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                          \
                                                                                                                       \
            return RETURN_SUCCESS;                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        /* if freq out of bounds, kill channel */                                                                      \
        if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {                                                \
            strcpy(buf, "board -c " STR(ch) " -k\r");                                                                  \
            send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                          \
                                                                                                                       \
            /* Turn OFF RX on HPS */                                                                                   \
            uint32_t old_val;                                                                                          \
                                                                                                                       \
            /* disable DSP core */                                                                                     \
            read_hps_reg("rx" STR(ch) "4", &old_val);                                                                  \
            write_hps_reg("rx" STR(ch) "4", old_val | 0x2);                                                            \
                                                                                                                       \
            /* disable channel */                                                                                      \
            read_hps_reg("rx" STR(ch) "4", &old_val);                                                                  \
            write_hps_reg("rx" STR(ch) "4", old_val &(~0x100));                                                        \
                                                                                                                       \
            rx_power[INT(ch)] = PWR_OFF;                                                                               \
            rx_stream[INT(ch)] = STREAM_OFF;                                                                           \
                                                                                                                       \
            PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "                                               \
                         "Shutting Down RX" STR(ch) ".\n");                                                            \
                                                                                                                       \
            return RETURN_ERROR;                                                                                       \
        }                                                                                                              \
                                                                                                                       \
        /* run the pll calc algorithm */                                                                               \
        pllparam_t pll;                                                                                                \
        long double outfreq = 0;                                                                                       \
        outfreq = setFreq(&freq, &pll);                                                                                \
                                                                                                                       \
        strcpy(buf, "rf -c " STR(ch) " \r");                                                                           \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
                                                                                                                       \
        /* TODO: pll1.power setting TBD (need to modify pllparam_t) */                                                 \
                                                                                                                       \
        /* Send Parameters over to the MCU */                                                                          \
        set_pll_frequency(uart_rx_fd[INT(ch)], (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false, INT(ch));                  \
                                                                                                                       \
        sprintf(ret, "%lf", outfreq);                                                                                  \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_rf_freq_lna(const char *data, char *ret) {                                               \
        strcpy(buf, "rf -c " STR(ch) " -l ");                                                                          \
        strcat(buf, data);                                                                                             \
        strcat(buf, "\r");                                                                                             \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_rf_freq_band(const char *data, char *ret) {                                              \
        strcpy(buf, "rf -c " STR(ch) " -b ");                                                                          \
        strcat(buf, data);                                                                                             \
        strcat(buf, "\r");                                                                                             \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_rf_gain_val(const char *data, char *ret) {                                               \
        int gain;                                                                                                      \
        sscanf(data, "%i", &gain);                                                                                     \
                                                                                                                       \
        if (gain > 126)                                                                                                \
            gain = 126;                                                                                                \
        else if (gain < 0)                                                                                             \
            gain = 0;                                                                                                  \
                                                                                                                       \
        if (gain % 2)                                                                                                  \
            gain++; /* Odd Number */                                                                                   \
                                                                                                                       \
        /* 0 -> 126 gain */                                                                                            \
        strcpy(buf, "vga -c " STR(ch) " -g ");                                                                         \
        sprintf(buf + strlen(buf), "%i", gain >> 1);                                                                   \
        strcat(buf, "\r");                                                                                             \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_rf_atten_val(const char *data, char *ret) {                                              \
        int atten;                                                                                                     \
        sscanf(data, "%i", &atten);                                                                                    \
                                                                                                                       \
        if (atten > 127)                                                                                               \
            atten = 127;                                                                                               \
        else if (atten < 0)                                                                                            \
            atten = 0;                                                                                                 \
                                                                                                                       \
        strcpy(buf, "rf -c " STR(ch) " -a ");                                                                          \
        sprintf(buf + strlen(buf), "%i", atten);                                                                       \
        strcat(buf, "\r");                                                                                             \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_rf_board_dump(const char *data, char *ret) {                                             \
        /* send the uart commands and read back the output and write to file                                           \
         */                                                                                                            \
                                                                                                                       \
        /* ADC */                                                                                                      \
        strcpy(buf, "dump -c " STR(ch) " -a\r");                                                                       \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_rx_fd[INT(ch)]);                                                                                \
        PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);                                                     \
                                                                                                                       \
        /* GPIOX */                                                                                                    \
        strcpy(buf, "dump -c " STR(ch) " -g\r");                                                                       \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_rx_fd[INT(ch)]);                                                                                \
        PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);                                                   \
                                                                                                                       \
        /* ADC Driver */                                                                                               \
        strcpy(buf, "dump -c " STR(ch) " -v\r");                                                                       \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_rx_fd[INT(ch)]);                                                                                \
        PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);                                              \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_rf_board_test(const char *data, char *ret) {                                             \
        /* TODO: MCU code cleanup */                                                                                   \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_rf_board_temp(const char *data, char *ret) {                                             \
        strcpy(buf, "board -c " STR(ch) " -t\r");                                                                      \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_rx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_status_rfld(const char *data, char *ret) {                                               \
        strcpy(buf, "status -c " STR(ch) " -l\r");                                                                     \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_rx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_status_adcalarm(const char *data, char *ret) {                                           \
        strcpy(buf, "status -c " STR(ch) " -a\r");                                                                     \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_rx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_rf_board_led(const char *data, char *ret) {                                              \
        strcpy(buf, "board -l\r");                                                                                     \
        strcat(buf, data);                                                                                             \
        strcat(buf, "\r");                                                                                             \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_dsp_signed(const char *data, char *ret) {                                                \
        uint32_t old_val, sign;                                                                                        \
        sscanf(data, "%u", &sign);                                                                                     \
        sign = sign ? 0 : 1;                                                                                           \
                                                                                                                       \
        read_hps_reg("rx" STR(ch) "4", &old_val);                                                                      \
        old_val &= ~(1 << 4);                                                                                          \
        write_hps_reg("rx" STR(ch) "4", old_val | (sign << 4));                                                        \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_dsp_gain(const char *data, char *ret) {                                                  \
        /* TODO: FW code */                                                                                            \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_dsp_rate(const char *data, char *ret) {                                                  \
        uint32_t old_val;                                                                                              \
        uint16_t base_factor, resamp_factor;                                                                           \
        double base_err, resamp_err;                                                                                   \
        double rate;                                                                                                   \
        sscanf(data, "%lf", &rate);                                                                                    \
                                                                                                                       \
        /* get the error for base rate */                                                                              \
        base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);                                        \
        resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);                                  \
                                                                                                                       \
        /* set the appropriate sample rate */                                                                          \
        memset(ret, 0, MAX_PROP_LEN);                                                                                  \
        int gain_factor;                                                                                               \
                                                                                                                       \
        if (resamp_err < base_err) {                                                                                   \
            write_hps_reg("rx" STR(ch) "1", resamp_factor);                                                            \
            read_hps_reg("rx" STR(ch) "4", &old_val);                                                                  \
            write_hps_reg("rx" STR(ch) "4", old_val | (1 << 15));                                                      \
            sprintf(ret, "%lf", RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));                                     \
            /*Set gain adjustment */                                                                                   \
            gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;                                               \
            read_hps_reg("rxg" STR(ch), &old_val);                                                                     \
            write_hps_reg("rxg" STR(ch), (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));                   \
        } else {                                                                                                       \
            write_hps_reg("rx" STR(ch) "1", base_factor);                                                              \
            read_hps_reg("rx" STR(ch) "4", &old_val);                                                                  \
            write_hps_reg("rx" STR(ch) "4", old_val & ~(1 << 15));                                                     \
            sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1));                                         \
            /*Set gain adjustment*/                                                                                    \
            gain_factor = decim_gain_lut[(base_factor)];                                                               \
            read_hps_reg("rxg" STR(ch), &old_val);                                                                     \
            write_hps_reg("rxg" STR(ch), (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));                   \
        }                                                                                                              \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_dsp_nco_adj(const char *data, char *ret) {                                               \
        double freq;                                                                                                   \
        uint32_t old_val;                                                                                              \
        uint8_t direction;                                                                                             \
                                                                                                                       \
        /* check for a minus or plus sign at the front */                                                              \
        if (data[0] == '-') {                                                                                          \
            sscanf(data + 1, "%lf", &freq);                                                                            \
            direction = 1;                                                                                             \
        } else if (data[0] == '+') {                                                                                   \
            sscanf(data + 1, "%lf", &freq);                                                                            \
            direction = 0;                                                                                             \
        } else {                                                                                                       \
            sscanf(data, "%lf", &freq);                                                                                \
            direction = 0;                                                                                             \
        }                                                                                                              \
                                                                                                                       \
        /* write NCO adj */                                                                                            \
        uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);                                                    \
        write_hps_reg("rx" STR(ch) "0", nco_steps);                                                                    \
        if (direction > 0) {                                                                                           \
            sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);                                                   \
        } else {                                                                                                       \
            sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);                                                    \
        }                                                                                                              \
                                                                                                                       \
        /* write direction */                                                                                          \
        read_hps_reg("rx" STR(ch) "4", &old_val);                                                                      \
        write_hps_reg("rx" STR(ch) "4", (old_val & ~(0x1 << 13)) | (direction << 13));                                 \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_dsp_rstreq(const char *data, char *ret) {                                                \
        uint32_t old_val;                                                                                              \
        read_hps_reg("rx" STR(ch) "4", &old_val);                                                                      \
        write_hps_reg("rx" STR(ch) "4", old_val | 0x2);                                                                \
        write_hps_reg("rx" STR(ch) "4", old_val & ~0x2);                                                               \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_dsp_loopback(const char *data, char *ret) {                                              \
        uint32_t old_val;                                                                                              \
        read_hps_reg("rx" STR(ch) "4", &old_val);                                                                      \
        if (strcmp(data, "1") == 0)                                                                                    \
            write_hps_reg("rx" STR(ch) "4", (old_val & ~0x1e00) | 0x400);                                              \
        else                                                                                                           \
            write_hps_reg("rx" STR(ch) "4", (old_val & ~0x1e00) | 0x000);                                              \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_about_id(const char *data, char *ret) {                                                  \
        /* don't need to do anything, save the ID in the file system */                                                \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_link_vita_en(const char *data, char *ret) {                                              \
        uint32_t old_val;                                                                                              \
        read_hps_reg("rx" STR(ch) "4", &old_val);                                                                      \
        if (strcmp(data, "1") == 0)                                                                                    \
            write_hps_reg("rx" STR(ch) "4", old_val | (1 << 14));                                                      \
        else                                                                                                           \
            write_hps_reg("rx" STR(ch) "4", old_val & ~(1 << 14));                                                     \
                                                                                                                       \
        /*sync_channels( 15 ); */                                                                                      \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_link_iface(const char *data, char *ret) {                                                \
        /* TODO: FW support for streaming to management port required */                                               \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_link_port(const char *data, char *ret) {                                                 \
        uint32_t port;                                                                                                 \
        sscanf(data, "%" SCNd32 "", &port);                                                                            \
        write_hps_reg("rx" STR(ch) "8", port);                                                                         \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_link_ip_dest(const char *data, char *ret) {                                              \
        uint8_t ip[4];                                                                                                 \
        sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1, ip + 2, ip + 3);                       \
        write_hps_reg("rx" STR(ch) "5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_link_mac_dest(const char *data, char *ret) {                                             \
        uint8_t mac[6];                                                                                                \
        sscanf(data, "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "", mac, mac + 1, mac + 2,       \
               mac + 3, mac + 4, mac + 5);                                                                             \
        write_hps_reg("rx" STR(ch) "6", (mac[0] << 8) | (mac[1]));                                                     \
        write_hps_reg("rx" STR(ch) "7", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);                     \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_stream(const char *data, char *ret) {                                                    \
        uint32_t old_val;                                                                                              \
        uint8_t stream;                                                                                                \
        sscanf(data, "%" SCNd8 "", &stream);                                                                           \
                                                                                                                       \
        /* if stream > 1, check the status of the stream */                                                            \
        if (stream > 1) {                                                                                              \
            sprintf(ret, "%u", rx_stream[INT(ch)]); /* Alert File Tree */                                              \
            return RETURN_SUCCESS;                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        /* Stream is already ON or OFF then return */                                                                  \
        if (stream == rx_stream[INT(ch)])                                                                              \
            return RETURN_SUCCESS;                                                                                     \
                                                                                                                       \
        /* Otherwise make the change accordingly */                                                                    \
        if (stream > 0) { /* TURN THE STREAM ON */                                                                     \
            if (rx_power[INT(ch)] == PWR_ON) {                                                                         \
                read_hps_reg(reg4[INT(ch)], &old_val);                                                                 \
                write_hps_reg(reg4[INT(ch)], old_val | 0x100);                                                         \
                                                                                                                       \
                read_hps_reg(reg4[INT(ch)], &old_val);                                                                 \
                write_hps_reg(reg4[INT(ch)], old_val | 0x2);                                                           \
                write_hps_reg(reg4[INT(ch)], old_val &(~0x2));                                                         \
                                                                                                                       \
                rx_stream[INT(ch)] = STREAM_ON;                                                                        \
            } else {                                                                                                   \
                /* Do not turn ON stream if channel is OFF */                                                          \
                sprintf(ret, "%u", 0); /* Alert File Tree */                                                           \
            }                                                                                                          \
        } else { /* TURN THE STREAM OFF */                                                                             \
            /* disable DSP core */                                                                                     \
            read_hps_reg("rx" STR(ch) "4", &old_val);                                                                  \
            write_hps_reg("rx" STR(ch) "4", old_val | 0x2);                                                            \
                                                                                                                       \
            /* disable channel */                                                                                      \
            read_hps_reg("rx" STR(ch) "4", &old_val);                                                                  \
            write_hps_reg("rx" STR(ch) "4", old_val &(~0x100));                                                        \
                                                                                                                       \
            rx_stream[INT(ch)] = STREAM_OFF;                                                                           \
        }                                                                                                              \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_pwr(const char *data, char *ret) {                                                       \
        uint32_t old_val;                                                                                              \
        uint8_t power;                                                                                                 \
        uint8_t i;                                                                                                     \
        sscanf(data, "%" SCNd8 "", &power);                                                                            \
                                                                                                                       \
        /* check if power is already enabled */                                                                        \
        if (power >= PWR_ON && rx_power[INT(ch)] == PWR_ON)                                                            \
            return RETURN_SUCCESS;                                                                                     \
                                                                                                                       \
        /* power on */                                                                                                 \
        if (power >= PWR_ON) {                                                                                         \
            rx_power[INT(ch)] = PWR_ON;                                                                                \
                                                                                                                       \
            /* board command */                                                                                        \
            strcpy(buf, "board -c " STR(ch) " -d\r");                                                                  \
            send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                          \
            usleep(200000);                                                                                            \
                                                                                                                       \
            /* disable dsp channels */                                                                                 \
            for (i = 0; i < (NUM_CHANNELS * 2); i++) {                                                                 \
                read_hps_reg(reg4[i], &old_val);                                                                       \
                write_hps_reg(reg4[i], old_val & ~0x100);                                                              \
            }                                                                                                          \
                                                                                                                       \
            /* send sync pulse */                                                                                      \
            sync_channels(15);                                                                                         \
                                                                                                                       \
            /* Enable active dsp channels, and reset DSP */                                                            \
            for (i = 0; i < NUM_CHANNELS; i++) {                                                                       \
                if (tx_power[i] == PWR_ON) {                                                                           \
                    read_hps_reg(reg4[i + 4], &old_val);                                                               \
                    write_hps_reg(reg4[i + 4], old_val | 0x100);                                                       \
                    read_hps_reg(reg4[i + 4], &old_val);                                                               \
                    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper(CHAR(ch)));                               \
                    write_hps_reg(reg4[i + 4], old_val | 0x2);                                                         \
                    write_hps_reg(reg4[i + 4], old_val &(~0x2));                                                       \
                }                                                                                                      \
                if (rx_stream[i] == STREAM_ON) {                                                                       \
                    read_hps_reg(reg4[i], &old_val);                                                                   \
                    write_hps_reg(reg4[i], old_val | 0x100);                                                           \
                    read_hps_reg(reg4[i], &old_val);                                                                   \
                    write_hps_reg(reg4[i], old_val | 0x2);                                                             \
                    write_hps_reg(reg4[i], old_val &(~0x2));                                                           \
                }                                                                                                      \
            }                                                                                                          \
                                                                                                                       \
            /* power off & stream off */                                                                               \
        } else {                                                                                                       \
            rx_power[INT(ch)] = PWR_OFF;                                                                               \
            rx_stream[INT(ch)] = STREAM_OFF;                                                                           \
                                                                                                                       \
            /* kill the channel */                                                                                     \
            strcpy(buf, "board -c " STR(ch) " -k\r");                                                                  \
            send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                          \
                                                                                                                       \
            /* disable DSP core */                                                                                     \
            read_hps_reg("rx" STR(ch) "4", &old_val);                                                                  \
            write_hps_reg("rx" STR(ch) "4", old_val | 0x2);                                                            \
                                                                                                                       \
            /* disable channel */                                                                                      \
            read_hps_reg("rx" STR(ch) "4", &old_val);                                                                  \
            write_hps_reg("rx" STR(ch) "4", old_val &(~0x100));                                                        \
        }                                                                                                              \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_about_serial(const char *data, char *ret) {                                              \
        strcpy(buf, "status -s\r");                                                                                    \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_rx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_about_mcudevid(const char *data, char *ret) {                                            \
        strcpy(buf, "status -d\r");                                                                                    \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_rx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_about_mcurev(const char *data, char *ret) {                                              \
        strcpy(buf, "status -v\r");                                                                                    \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_rx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_about_mcufuses(const char *data, char *ret) {                                            \
        strcpy(buf, "status -f\r");                                                                                    \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_rx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_about_fw_ver(const char *data, char *ret) {                                              \
        strcpy(buf, "board -v\r");                                                                                     \
        send_uart_comm(uart_rx_fd[INT(ch)], (uint8_t *)buf, strlen(buf));                                              \
        read_uart(uart_rx_fd[INT(ch)]);                                                                                \
        strcpy(ret, (char *)uart_ret_buf);                                                                             \
                                                                                                                       \
        return RETURN_SUCCESS;                                                                                         \
    }

LIST_OF_CHANNELS
#undef X

/* ================================================================================================================== */
/* --------------------------------------------------- TRIGGER------------------------------------------------------- */
/* ================================================================================================================== */

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
    snprintf(regname, sizeof(regname), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 9 : 12);
    return set_reg_bits(regname, 0, -1, backoff);
}

static int set_edge_sample_num(bool tx, const char *chan, uint64_t num) {
    int r;
    uint32_t val_msw;
    uint32_t val_lsw;

    char regname_msw[8];
    char regname_lsw[8];

    snprintf(regname_msw, sizeof(regname_msw), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 7 : 10);
    snprintf(regname_lsw, sizeof(regname_lsw), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 8 : 11);

    val_msw = num >> 32;
    val_lsw = num & 0xffffffff;

    return set_reg_bits(regname_msw, 0, -1, val_msw) || set_reg_bits(regname_lsw, 0, -1, val_lsw);
}

static int set_trigger_ufl_dir(bool tx, const char *chan, bool in) {
    char reg_name[8];
    snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 6 : 9);
    return set_reg_bits(reg_name, 9, 1, in);
}

static int set_trigger_sel(bool tx, const char *chan, uint32_t sel) {
    char reg_name[8];
    snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 6 : 9);
    return set_reg_bits(reg_name, 10, 0b11, sel);
}

static int set_trigger_mode(bool sma, bool tx, const char *chan, bool edge) {
    unsigned shift;
    char reg_name[8];
    snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 6 : 9);
    shift = sma ? 0 : 4;
    return set_reg_bits(reg_name, shift, 1, edge);
}

static int set_trigger_ufl_pol(bool tx, const char *chan, bool positive) {
    char reg_name[8];
    snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 6 : 9);
    return set_reg_bits(reg_name, 8, 1, positive);
}

#define X(ch)                                                                                                          \
    static int hdlr_tx_##ch##_trigger_sma_mode(const char *data, char *ret) {                                          \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, #ch, val);                                  \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_trigger_edge_backoff(const char *data, char *ret) {                                      \
        uint32_t val;                                                                                                  \
        int r;                                                                                                         \
        r = valid_edge_backoff(data, &val) || set_edge_backoff(true, #ch, val);                                        \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_trigger_edge_sample_num(const char *data, char *ret) {                                   \
        uint64_t val;                                                                                                  \
        int r;                                                                                                         \
        r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, #ch, val);                                  \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_trigger_trig_sel(const char *data, char *ret) {                                          \
        uint32_t val;                                                                                                  \
        int r;                                                                                                         \
        r = valid_trigger_sel(data, &val) || set_trigger_sel(true, #ch, val);                                          \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_trigger_ufl_dir(const char *data, char *ret) {                                           \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, #ch, val);                                      \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_trigger_ufl_mode(const char *data, char *ret) {                                          \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, #ch, val);                                 \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_tx_##ch##_trigger_ufl_pol(const char *data, char *ret) {                                           \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, #ch, val);                                      \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_trigger_sma_mode(const char *data, char *ret) {                                          \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, #ch, val);                                 \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_trigger_edge_backoff(const char *data, char *ret) {                                      \
        uint32_t val;                                                                                                  \
        int r;                                                                                                         \
        r = valid_edge_backoff(data, &val) || set_edge_backoff(false, #ch, val);                                       \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_trigger_edge_sample_num(const char *data, char *ret) {                                   \
        uint64_t val;                                                                                                  \
        int r;                                                                                                         \
        r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, #ch, val);                                 \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_trigger_trig_sel(const char *data, char *ret) {                                          \
        uint32_t val;                                                                                                  \
        int r;                                                                                                         \
        r = valid_trigger_sel(data, &val) || set_trigger_sel(false, #ch, val);                                         \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_trigger_ufl_dir(const char *data, char *ret) {                                           \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, #ch, val);                                     \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_trigger_ufl_mode(const char *data, char *ret) {                                          \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_trigger_mode(data, &val) || set_trigger_mode(false, false, #ch, val);                                \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_rx_##ch##_trigger_ufl_pol(const char *data, char *ret) {                                           \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, #ch, val);                                     \
        return r;                                                                                                      \
    }

LIST_OF_CHANNELS
#undef X

#define X(ch)                                                                                      \
    static int hdlr_tx_##ch##_trigger_gating(const char *data, char *ret) {                                            \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_gating_mode(data, &val) || set_gating_mode(#ch, val);                                                \
        return r;                                                                                                      \
    }

LIST_OF_CHANNELS
#undef X

/* ================================================================================================================== */
/* ----------------------------------------------------- TIME ------------------------------------------------------- */
/* ================================================================================================================== */

static int hdlr_time_clk_pps(const char *data, char *ret) {
    // Insert MCU/MEM command

    return RETURN_SUCCESS;
}

static int hdlr_time_clk_cur_time(const char *data, char *ret) {
    // test by reading it before writing to it
    // uint32_t intpart, fracpart;
    // read_hps_reg( "sys5", &intpart);
    // read_hps_reg( "sys6", &fracpart);
    // printf("Time is: %lf seconds\n", (double)intpart + ((double)fracpart / 100000000) );

    // double time;
    // sscanf(data, "%lf", &time);
    // write_hps_reg( "sys7", (uint32_t)time);
    // write_hps_reg( "sys8", time - (uint32_t)time);

    long double time;
    sscanf(data, "%Lf", &time);
    write_hps_reg("sys9", (uint32_t)(((uint64_t)time) & 0x00000000FFFFFFFF));
    write_hps_reg("sys10", (uint32_t)(((uint64_t)time) >> 32) & 0x00000000FFFFFFFF);

    write_hps_reg("sys11", (uint32_t)(time - (uint64_t)time) & 0x00000000FFFFFFFF);
    // write_hps_reg("sys12", (uint32_t)((time-(uint64_t)time)>>32) & 0x00000000FFFFFFFF);
    // toggle the set register
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
    // send the uart commands and read back the output and write to file

    // Diagnostic Dump of Clk Board
    strcpy(buf, "board -e\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_synth_fd);
    PRINT(DUMP, "[Board: Time Regdump] %s\n", uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_time_board_test(const char *data, char *ret) {
    // TODO: MCU code cleanup
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
    // Do Nothing, store in filesystem
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

/* ================================================================================================================== */
/* ----------------------------------------------------- FPGA ------------------------------------------------------- */
/* ================================================================================================================== */

static int hdlr_fpga_board_dump(const char *data, char *ret) {

    // dump all of the board logs
    hdlr_tx_a_rf_board_dump(NULL, NULL); /* Can use XMACRO for this. */
    hdlr_tx_b_rf_board_dump(NULL, NULL);
    hdlr_tx_c_rf_board_dump(NULL, NULL);
    hdlr_tx_d_rf_board_dump(NULL, NULL);
    hdlr_rx_a_rf_board_dump(NULL, NULL);
    hdlr_rx_b_rf_board_dump(NULL, NULL);
    hdlr_rx_c_rf_board_dump(NULL, NULL);
    hdlr_rx_d_rf_board_dump(NULL, NULL);
    hdlr_time_board_dump(NULL, NULL);

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_test(const char *data, char *ret) {
    // TODO: MCU code cleanup
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gle(const char *data, char *ret) {

    if (strcmp(data, "1") == 0) {
        strcpy(buf, "board -g 1\r");
        send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        usleep(50000);

        strcpy(buf, "board -g 1\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
        usleep(50000);

        strcpy(buf, "board -g 1\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
        usleep(50000);
    }
    if (strcmp(data, "2") == 0) {
        strcpy(buf, "board -g 2\r");
        send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
        usleep(50000);

        strcpy(buf, "board -g 2\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
        usleep(50000);

        strcpy(buf, "board -g 2\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
        usleep(50000);
    }
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_temp(const char *data, char *ret) {
    // strcpy(buf, "board -t\r");
    // send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
    // read_uart(NO_FWD_CMD);
    // strcpy(ret, (char*)uart_ret_buf);
    uint32_t old_val;
    read_hps_reg("sys14", &old_val);

    // mask off temp
    old_val = old_val & 0xff;

    // if value >= 0x80 (=128), subtract 0x80 and convert to int
    if (old_val >= 128) {
        old_val = old_val - 128;
        sprintf(ret, "temp +%lu degC\n", old_val);
    }
    // if value < 0x80, subtract 0x3a (=58) and convert to negative int
    else if (old_val < 128) {
        old_val = old_val - 58;
        sprintf(ret, "temp -%lu degC\n", old_val);
    }

    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_led(const char *data, char *ret) {
    // strcpy(buf, "board -l ");
    // strcat(buf, data);
    // strcat(buf, "\r");
    // send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_rstreq(const char *data, char *ret) {
    // strcpy(buf, "fpga -r \r");
    // send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

    /* TODO: Implement DIG Board FPGA Reset */

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
    // strcpy(buf, "fpga -o \r");
    // send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
    sync_channels(15);
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_sys_rstreq(const char *data, char *ret) {
    strcpy(buf, "board -r\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    usleep(700000);

    strcpy(buf, "board -r\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    usleep(50000);

    strcpy(buf, "board -r\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    usleep(50000);

    /* TODO: Implement DIG board Reset */
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_flow_control_sfpX_port(const char *data, char *ret, unsigned sfp_port) {

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
static inline int hdlr_fpga_board_flow_control_sfpa_port(const char *data, char *ret) {
    return hdlr_fpga_board_flow_control_sfpX_port(data, ret, 0);
}
static inline int hdlr_fpga_board_flow_control_sfpb_port(const char *data, char *ret) {
    return hdlr_fpga_board_flow_control_sfpX_port(data, ret, 1);
}

static int hdlr_fpga_board_fw_rst(const char *data, char *ret) {
    uint32_t old_val;

    // toggle the bit sys0[4]
    read_hps_reg("sys0", &old_val);
    write_hps_reg("sys0", old_val | 0x10);
    write_hps_reg("sys0", old_val & (~0x10));

    return RETURN_SUCCESS;
}

static int hdlr_fpga_about_id(const char *data, char *ret) {
    // don't need to do anything, save the ID in the file system
    return RETURN_SUCCESS;
}

static int hdlr_fpga_about_cmp_time(const char *data, char *ret) {
    uint32_t old_val;
    int year, month, day, hour, min;
    read_hps_reg("sys15", &old_val);
    // get year
    year = (old_val & 0xfff00000) >> 20;
    month = (old_val & 0x000f0000) >> 16;
    day = (old_val & 0x0000f800) >> 11;
    hour = (old_val & 0x000007c0) >> 6;
    min = old_val & 0x0000003f;

    sprintf(ret, "cmp. time %i-%i-%i %i:%i (yyyy-MM-dd HH:mm) \n", year, month, day, hour, min);

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

    // append values
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

// TODO: Move FWversion code to ARM, edit MAKE file with version info, refer to MCU code
static int hdlr_fpga_about_fw_ver(const char *data, char *ret) {
    // strcpy(buf, "board -v\r");
    // send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
    // read_uart(NO_FWD_CMD);
    // strcpy(ret, (char*)uart_ret_buf);
    uint64_t old_val;
    uint32_t old_val1;
    uint32_t old_val2;
    read_hps_reg("sys3", &old_val2);
    read_hps_reg("sys4", &old_val1);

    // bits sys3[7:0]
    old_val2 = old_val2 & 0xff;

    // append values
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

    // bits sys1[10:7]
    old_val = (old_val >> 7) & 0xf;

    sprintf(ret, "ver. 0x%02x", old_val);
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_rate(const char *data, char *ret) {
    // TODO: Need to implement in FW
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpa_ip_addr(const char *data, char *ret) {
    uint32_t ip[4];
    if (ipver[0] == IPVER_IPV4) {
        sscanf(data, "%" SCNd32 ".%" SCNd32 ".%" SCNd32 ".%" SCNd32 "", ip, ip + 1, ip + 2, ip + 3);
        write_hps_reg("net5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
    } else if (ipver[0] == IPVER_IPV6) {
        sscanf(data, "%" SCNx32 ":%" SCNx32 ":%" SCNx32 ":%" SCNx32 "", ip, ip + 1, ip + 2, ip + 3);
        write_hps_reg("net1", ip[0]);
        write_hps_reg("net2", ip[1]);
        write_hps_reg("net3", ip[2]);
        write_hps_reg("net4", ip[3]);
    }
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpa_mac_addr(const char *data, char *ret) {
    uint8_t mac[6];
    sscanf(data, "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "", mac, mac + 1, mac + 2, mac + 3,
           mac + 4, mac + 5);
    write_hps_reg("net11", (mac[0] << 8) | (mac[1]));
    write_hps_reg("net12", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
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
        sscanf(data, "%" SCNd32 ".%" SCNd32 ".%" SCNd32 ".%" SCNd32 "", ip, ip + 1, ip + 2, ip + 3);
        ip[0] = (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]);
        write_hps_reg("net20", ip[0]);
    } else if (ipver[1] == IPVER_IPV6) {
        sscanf(data, "%" SCNx32 ":%" SCNx32 ":%" SCNx32 ":%" SCNx32 "", ip, ip + 1, ip + 2, ip + 3);
        write_hps_reg("net16", ip[0]);
        write_hps_reg("net17", ip[1]);
        write_hps_reg("net18", ip[2]);
        write_hps_reg("net19", ip[3]);
    }
    return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpb_mac_addr(const char *data, char *ret) {
    uint8_t mac[6];
    sscanf(data, "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "", mac, mac + 1, mac + 2, mac + 3,
           mac + 4, mac + 5);
    write_hps_reg("net26", (mac[0] << 8) | (mac[1]));
    write_hps_reg("net27", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
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

static int hdlr_fpga_link_net_dhcp_en(const char *data, char *ret) { return RETURN_SUCCESS; }

static int hdlr_fpga_link_net_hostname(const char *data, char *ret) {
    // write to the file
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
    // ensure that it is a valid IP address
    char ip_address[MAX_PROP_LEN] = {0};
    char command[MAX_PROP_LEN] = {0};
    sscanf(data, "%s", ip_address);

    struct sockaddr_in sa;
    if (!inet_pton(AF_INET, ip_address, &(sa.sin_addr))) {
        return RETURN_ERROR_PARAM;
    }

    // write to the file
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
    write_hps_reg("sys11", 0); // set frac_time to 0
    write_hps_reg("sys12", 0); // set frac_time to 0
    write_hps_reg("sys13", 1); // writing 1, then 0 to sys9 sets the time
    write_hps_reg("sys13", 0); // to what is written in sys7 and sys8

    return RETURN_SUCCESS;
}
