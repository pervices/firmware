# 1 "xmacro.c"
# 1 "<built-in>"
# 1 "<command-line>"
# 31 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 32 "<command-line>" 2
# 1 "xmacro.c"
# 492 "xmacro.c"
static int hdlr_tx_b_rf_dac_dither_en(const char *data, char *ret) {
    int r;
    int en;
    r = sscanf(data, "%d", &en);
    if (1 != r) {
        return RETURN_ERROR;
    }
    if (en < 0 || en > 1) {
        return RETURN_ERROR_PARAM;
    }
    snprintf(buf, sizeof(buf),
             "dac -c "
             "b"
             " -l %u\r",
             en);
    sprintf(ret, "%u", en);
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_dac_dither_mixer_en(const char *data, char *ret) {
    int r;
    int en;
    r = sscanf(data, "%d", &en);
    if (1 != r) {
        return RETURN_ERROR;
    }
    if (en < 0 || en > 1) {
        return RETURN_ERROR_PARAM;
    }
    snprintf(buf, sizeof(buf),
             "dac -c "
             "b"
             " -3 %u\r",
             en);
    sprintf(ret, "%u", en);
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_dac_dither_sra_sel(const char *data, char *ret) {
    int r;
    int db;
    int sel;
    r = sscanf(data, "%d", &db);
    if (1 != r) {
        return RETURN_ERROR;
    }
    if (db < 6 || db > 96 || 0 != db % 6) {
        return RETURN_ERROR_PARAM;
    }
    sprintf(ret, "%u", db);
    sel = 16 - (db / 6);
    snprintf(buf, sizeof(buf),
             "dac -c "
             "b"
             " -b %u\r",
             sel);
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_dac_nco(const char *data, char *ret) {
    double freq;
    sscanf(data, "%lf", &freq);
    uint64_t nco_steps = (uint64_t)round(freq * DAC_NCO_CONST);
    sprintf(ret, "%lf", (double)nco_steps / DAC_NCO_CONST);
    strcpy(buf, "dac -c "
                "b"
                " -e 0 -n ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    strcpy(buf, "dac -o ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_dac_temp(const char *data, char *ret) {
    strcpy(buf, "board -c "
                "b"
                " -t\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd[("b"[0] - 'a')]);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_freq_val(const char *data, char *ret) {
    uint64_t freq = 0;
    sscanf(data, "%" SCNd64 "", &freq);
    if (freq == 0) {
        strcpy(buf, "rf -c "
                    "b"
                    " -z\r");
        send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
        return RETURN_SUCCESS;
    }
    if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
        strcpy(buf, "board -c "
                    "b"
                    " -k\r");
        send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
        uint32_t old_val;
        read_hps_reg("tx"
                     "b"
                     "4",
                     &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper(CHAR(b)));
        write_hps_reg("tx"
                      "b"
                      "4",
                      old_val | 0x2);
        read_hps_reg("tx"
                     "b"
                     "4",
                     &old_val);
        write_hps_reg("tx"
                      "b"
                      "4",
                      old_val & (~0x100));
        tx_power[("b"[0] - 'a')] = PWR_OFF;
        PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                     "Shutting Down TX"
                     "b"
                     ".\n");
        return RETURN_ERROR;
    }
    pllparam_t pll;
    long double outfreq = 0;
    outfreq = setFreq(&freq, &pll);
    strcpy(buf, "rf -c "
                "b"
                " \r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    set_pll_frequency(uart_tx_fd[("b"[0] - 'a')],
                      (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                      ("b"[0] - 'a'));
    sprintf(ret, "%lf", outfreq);
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_freq_band(const char *data, char *ret) {
    strcpy(buf, "rf -c "
                "b"
                " -b ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_freq_i_bias(const char *data, char *ret) {
    sscanf(data, "%i", &(i_bias[("b"[0] - 'a')]));
    strcpy(buf, "rf -c "
                "b"
                " -i ");
    sprintf(buf + strlen(buf), "%i", i_bias[("b"[0] - 'a')]);
    strcat(buf, " -q ");
    sprintf(buf + strlen(buf), "%i", q_bias[("b"[0] - 'a')]);
    strcat(buf, " -m\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_freq_q_bias(const char *data, char *ret) {
    sscanf(data, "%i", &(q_bias[("b"[0] - 'a')]));
    strcpy(buf, "rf -c "
                "b"
                " -i ");
    sprintf(buf + strlen(buf), "%i", i_bias[("b"[0] - 'a')]);
    strcat(buf, " -q ");
    sprintf(buf + strlen(buf), "%i", q_bias[("b"[0] - 'a')]);
    strcat(buf, " -m\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_gain_val(const char *data, char *ret) {
    int gain;
    sscanf(data, "%i", &gain);
    if (gain > 127)
        gain = 127;
    else if (gain < 0)
        gain = 0;
    strcpy(buf, "rf -c "
                "b"
                " -a ");
    sprintf(buf + strlen(buf), "%i", 127 - gain);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_board_dump(const char *data, char *ret) {
    strcpy(buf, "dump -c "
                "b"
                " -d\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd[("b"[0] - 'a')]);
    PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
    strcpy(buf, "dump -c "
                "b"
                " -g\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd[("b"[0] - 'a')]);
    PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_board_test(const char *data, char *ret) {
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -c "
                "b"
                " -t\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd[("b"[0] - 'a')]);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_status_rfld(const char *data, char *ret) {
    strcpy(buf, "status -c "
                "b"
                " -l\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd[("b"[0] - 'a')]);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_status_dacld(const char *data, char *ret) {
    strcpy(buf, "status -c "
                "b"
                " -p\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd[("b"[0] - 'a')]);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_status_dacctr(const char *data, char *ret) {
    strcpy(buf, "status -c "
                "b"
                " -e\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd[("b"[0] - 'a')]);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_board_led(const char *data, char *ret) {
    strcpy(buf, "board -l\r");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_dsp_gain(const char *data, char *ret) {
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_dsp_rate(const char *data, char *ret) {
    uint32_t old_val;
    uint16_t base_factor, resamp_factor;
    double base_err, resamp_err;
    double rate;
    sscanf(data, "%lf", &rate);
    base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
    resamp_factor =
        get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);
    memset(ret, 0, MAX_PROP_LEN);
    if (resamp_err < base_err) {
        write_hps_reg("tx"
                      "b"
                      "1",
                      resamp_factor);
        read_hps_reg("tx"
                     "b"
                     "4",
                     &old_val);
        write_hps_reg("tx"
                      "b"
                      "4",
                      old_val | (1 << 15));
        sprintf(ret, "%lf", RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));
        read_hps_reg("txg"
                     "b",
                     &old_val);
        write_hps_reg("txg"
                      "b",
                      (old_val & ~(0xff << 0)) |
                          (interp_gain_lut[(resamp_factor)] << 0));
    } else {
        write_hps_reg("tx"
                      "b"
                      "1",
                      base_factor);
        read_hps_reg("tx"
                     "b"
                     "4",
                     &old_val);
        write_hps_reg("tx"
                      "b"
                      "4",
                      old_val & ~(1 << 15));
        sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1));
        read_hps_reg("txg"
                     "b",
                     &old_val);
        write_hps_reg("txg"
                      "b",
                      (old_val & ~(0xff << 0)) |
                          (interp_gain_lut[(base_factor)] << 0));
    }
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_dsp_nco_adj(const char *data, char *ret) {
    double freq;
    uint32_t old_val;
    uint8_t direction;
    if (data[0] == '-') {
        sscanf(data + 1, "%lf", &freq);
        direction = 1;
    } else if (data[0] == '+') {
        sscanf(data + 1, "%lf", &freq);
        direction = 0;
    } else {
        sscanf(data, "%lf", &freq);
        direction = 0;
    }
    uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);
    write_hps_reg("tx"
                  "b"
                  "0",
                  nco_steps);
    if (direction > 0) {
        sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
    } else {
        sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
    }
    read_hps_reg("tx"
                 "b"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "b"
                  "4",
                  (old_val & ~(0x1 << 13)) | (direction << 13));
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_dsp_rstreq(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("tx"
                 "b"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("b"[0]));
    write_hps_reg("tx"
                  "b"
                  "4",
                  old_val | 0x2);
    write_hps_reg("tx"
                  "b"
                  "4",
                  old_val & ~0x2);
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_about_id(const char *data, char *ret) {
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_link_vita_en(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("tx"
                 "b"
                 "4",
                 &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("tx"
                      "b"
                      "4",
                      old_val | (1 << 14));
    else
        write_hps_reg("tx"
                      "b"
                      "4",
                      old_val & ~(1 << 14));
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_link_iface(const char *data, char *ret) {
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_link_port(const char *data, char *ret) {
    uint32_t port;
    sscanf(data, "%" SCNd32 "", &port);
    write_hps_reg("tx"
                  "b"
                  "5",
                  port);
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_qa_fifo_lvl(const char *data, char *ret) {
    uint32_t lvl;
    read_hps_reg("res_ro4", &lvl);
    lvl &= 0xffff;
    sprintf(ret, "%u", lvl);
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_qa_oflow(const char *data, char *ret) {
    uint32_t count;
    read_hps_reg("flc14", &count);
    sprintf(ret, "%u", count);
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_qa_uflow(const char *data, char *ret) {
    uint32_t count;
    read_hps_reg("flc6", &count);
    sprintf(ret, "%u", count);
    return RETURN_SUCCESS;
}
static int hdlr_tx_b_pwr(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t power;
    uint8_t i;
    sscanf(data, "%" SCNd8 "", &power);
    if (power >= PWR_ON && tx_power[("b"[0] - 'a')] == PWR_ON)
        return RETURN_SUCCESS;
    if (power >= PWR_ON) {
        tx_power[("b"[0] - 'a')] = PWR_ON;
        strcpy(buf, "board -c "
                    "b"
                    " -d\r");
        send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
        usleep(200000);
        for (i = 0; i < (NUM_CHANNELS * 2); i++) {
            read_hps_reg(reg4[i], &old_val);
            write_hps_reg(reg4[i], old_val & ~0x100);
        }
        sync_channels(15);
        for (i = 0; i < NUM_CHANNELS; i++) {
            if (tx_power[i] == PWR_ON) {
                read_hps_reg(reg4[i + 4], &old_val);
                write_hps_reg(reg4[i + 4], old_val | 0x100);
                read_hps_reg(reg4[i + 4], &old_val);
                PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__,
                      toupper(CHAR(b)));
                write_hps_reg(reg4[i + 4], old_val | 0x2);
                write_hps_reg(reg4[i + 4], old_val & (~0x2));
            }
            if (rx_power[i] == PWR_ON) {
                read_hps_reg(reg4[i], &old_val);
                write_hps_reg(reg4[i], old_val | 0x100);
                read_hps_reg(reg4[i], &old_val);
                write_hps_reg(reg4[i], old_val | 0x2);
                write_hps_reg(reg4[i], old_val & (~0x2));
            }
        }
    } else {
        strcpy(buf, "board -c "
                    "b"
                    " -k\r");
        send_uart_comm(uart_tx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
        read_hps_reg("tx"
                     "b"
                     "4",
                     &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper(CHAR(b)));
        write_hps_reg("tx"
                      "b"
                      "4",
                      old_val | 0x2);
        read_hps_reg("tx"
                     "b"
                     "4",
                     &old_val);
        write_hps_reg("tx"
                      "b"
                      "4",
                      old_val & (~0x100));
        tx_power[("b"[0] - 'a')] = PWR_OFF;
    }
    return RETURN_SUCCESS;
}
# 935 "xmacro.c"
static int hdlr_rx_b_rf_freq_val(const char *data, char *ret) {
    uint64_t freq = 0;
    sscanf(data, "%" SCNd64 "", &freq);
    if (freq == 0) {
        strcpy(buf, "rf -c "
                    "b"
                    " -z\r");
        send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
        return RETURN_SUCCESS;
    }
    if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
        strcpy(buf, "board -c "
                    "b"
                    " -k\r");
        send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
        uint32_t old_val;
        read_hps_reg("rx"
                     "b"
                     "4",
                     &old_val);
        write_hps_reg("rx"
                      "b"
                      "4",
                      old_val | 0x2);
        read_hps_reg("rx"
                     "b"
                     "4",
                     &old_val);
        write_hps_reg("rx"
                      "b"
                      "4",
                      old_val & (~0x100));
        rx_power[("b"[0] - 'a')] = PWR_OFF;
        rx_stream[("b"[0] - 'a')] = STREAM_OFF;
        PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                     "Shutting Down RX"
                     "b"
                     ".\n");
        return RETURN_ERROR;
    }
    pllparam_t pll;
    long double outfreq = 0;
    outfreq = setFreq(&freq, &pll);
    strcpy(buf, "rf -c "
                "b"
                " \r");
    send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    set_pll_frequency(uart_rx_fd[("b"[0] - 'a')],
                      (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                      ("b"[0] - 'a'));
    sprintf(ret, "%lf", outfreq);
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_freq_lna(const char *data, char *ret) {
    strcpy(buf, "rf -c "
                "b"
                " -l ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_freq_band(const char *data, char *ret) {
    strcpy(buf, "rf -c "
                "b"
                " -b ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_gain_val(const char *data, char *ret) {
    int gain;
    sscanf(data, "%i", &gain);
    if (gain > 126)
        gain = 126;
    else if (gain < 0)
        gain = 0;
    if (gain % 2)
        gain++;
    strcpy(buf, "vga -c "
                "b"
                " -g ");
    sprintf(buf + strlen(buf), "%i", gain >> 1);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_atten_val(const char *data, char *ret) {
    int atten;
    sscanf(data, "%i", &atten);
    if (atten > 127)
        atten = 127;
    else if (atten < 0)
        atten = 0;
    strcpy(buf, "rf -c "
                "b"
                " -a ");
    sprintf(buf + strlen(buf), "%i", atten);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_board_dump(const char *data, char *ret) {
    strcpy(buf, "dump -c "
                "b"
                " -a\r");
    send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd[("b"[0] - 'a')]);
    PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
    strcpy(buf, "dump -c "
                "b"
                " -g\r");
    send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd[("b"[0] - 'a')]);
    PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
    strcpy(buf, "dump -c "
                "b"
                " -v\r");
    send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd[("b"[0] - 'a')]);
    PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_board_test(const char *data, char *ret) {
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -c "
                "b"
                " -t\r");
    send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd[("b"[0] - 'a')]);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_status_rfld(const char *data, char *ret) {
    strcpy(buf, "status -c "
                "b"
                " -l\r");
    send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd[("b"[0] - 'a')]);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_status_adcalarm(const char *data, char *ret) {
    strcpy(buf, "status -c "
                "b"
                " -a\r");
    send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd[("b"[0] - 'a')]);
    strcpy(ret, (char *)uart_ret_buf);
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_board_led(const char *data, char *ret) {
    strcpy(buf, "board -l\r");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_dsp_signed(const char *data, char *ret) {
    uint32_t old_val, sign;
    sscanf(data, "%u", &sign);
    sign = sign ? 0 : 1;
    read_hps_reg("rx"
                 "b"
                 "4",
                 &old_val);
    old_val &= ~(1 << 4);
    write_hps_reg("rx"
                  "b"
                  "4",
                  old_val | (sign << 4));
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_dsp_gain(const char *data, char *ret) {
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_dsp_rate(const char *data, char *ret) {
    uint32_t old_val;
    uint16_t base_factor, resamp_factor;
    double base_err, resamp_err;
    double rate;
    sscanf(data, "%lf", &rate);
    base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
    resamp_factor =
        get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);
    memset(ret, 0, MAX_PROP_LEN);
    int gain_factor;
    if (resamp_err < base_err) {
        write_hps_reg("rx"
                      "b"
                      "1",
                      resamp_factor);
        read_hps_reg("rx"
                     "b"
                     "4",
                     &old_val);
        write_hps_reg("rx"
                      "b"
                      "4",
                      old_val | (1 << 15));
        sprintf(ret, "%lf", RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));
        gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
        read_hps_reg("rxg"
                     "b",
                     &old_val);
        write_hps_reg("rxg"
                      "b",
                      (old_val & ~(0xff << 0)) |
                          (((uint16_t)gain_factor) << 0));
    } else {
        write_hps_reg("rx"
                      "b"
                      "1",
                      base_factor);
        read_hps_reg("rx"
                     "b"
                     "4",
                     &old_val);
        write_hps_reg("rx"
                      "b"
                      "4",
                      old_val & ~(1 << 15));
        sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1));
        gain_factor = decim_gain_lut[(base_factor)];
        read_hps_reg("rxg"
                     "b",
                     &old_val);
        write_hps_reg("rxg"
                      "b",
                      (old_val & ~(0xff << 0)) |
                          (((uint16_t)gain_factor) << 0));
    }
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_dsp_nco_adj(const char *data, char *ret) {
    double freq;
    uint32_t old_val;
    uint8_t direction;
    if (data[0] == '-') {
        sscanf(data + 1, "%lf", &freq);
        direction = 1;
    } else if (data[0] == '+') {
        sscanf(data + 1, "%lf", &freq);
        direction = 0;
    } else {
        sscanf(data, "%lf", &freq);
        direction = 0;
    }
    uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);
    write_hps_reg("rx"
                  "b"
                  "0",
                  nco_steps);
    if (direction > 0) {
        sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
    } else {
        sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
    }
    read_hps_reg("rx"
                 "b"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "b"
                  "4",
                  (old_val & ~(0x1 << 13)) | (direction << 13));
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_dsp_rstreq(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rx"
                 "b"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "b"
                  "4",
                  old_val | 0x2);
    write_hps_reg("rx"
                  "b"
                  "4",
                  old_val & ~0x2);
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_dsp_loopback(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rx"
                 "b"
                 "4",
                 &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("rx"
                      "b"
                      "4",
                      (old_val & ~0x1e00) | 0x400);
    else
        write_hps_reg("rx"
                      "b"
                      "4",
                      (old_val & ~0x1e00) | 0x000);
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_about_id(const char *data, char *ret) {
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_link_vita_en(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rx"
                 "b"
                 "4",
                 &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("rx"
                      "b"
                      "4",
                      old_val | (1 << 14));
    else
        write_hps_reg("rx"
                      "b"
                      "4",
                      old_val & ~(1 << 14));
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_link_iface(const char *data, char *ret) {
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_link_port(const char *data, char *ret) {
    uint32_t port;
    sscanf(data, "%" SCNd32 "", &port);
    write_hps_reg("rx"
                  "b"
                  "8",
                  port);
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_link_ip_dest(const char *data, char *ret) {
    uint8_t ip[4];
    sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
           ip + 2, ip + 3);
    write_hps_reg("rx"
                  "b"
                  "5",
                  (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_link_mac_dest(const char *data, char *ret) {
    uint8_t mac[6];
    sscanf(data,
           "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
           mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
    write_hps_reg("rx"
                  "b"
                  "6",
                  (mac[0] << 8) | (mac[1]));
    write_hps_reg("rx"
                  "b"
                  "7",
                  (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_stream(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t stream;
    sscanf(data, "%" SCNd8 "", &stream);
    if (stream > 1) {
        sprintf(ret, "%u", rx_stream[("b"[0] - 'a')]);
        return RETURN_SUCCESS;
    }
    if (stream == rx_stream[("b"[0] - 'a')])
        return RETURN_SUCCESS;
    if (stream > 0) {
        if (rx_power[("b"[0] - 'a')] == PWR_ON) {
            read_hps_reg(reg4[("b"[0] - 'a')], &old_val);
            write_hps_reg(reg4[("b"[0] - 'a')], old_val | 0x100);
            read_hps_reg(reg4[("b"[0] - 'a')], &old_val);
            write_hps_reg(reg4[("b"[0] - 'a')], old_val | 0x2);
            write_hps_reg(reg4[("b"[0] - 'a')], old_val & (~0x2));
            rx_stream[("b"[0] - 'a')] = STREAM_ON;
        } else {
            sprintf(ret, "%u", 0);
        }
    } else {
        read_hps_reg("rx"
                     "b"
                     "4",
                     &old_val);
        write_hps_reg("rx"
                      "b"
                      "4",
                      old_val | 0x2);
        read_hps_reg("rx"
                     "b"
                     "4",
                     &old_val);
        write_hps_reg("rx"
                      "b"
                      "4",
                      old_val & (~0x100));
        rx_stream[("b"[0] - 'a')] = STREAM_OFF;
    }
    return RETURN_SUCCESS;
}
static int hdlr_rx_b_pwr(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t power;
    uint8_t i;
    sscanf(data, "%" SCNd8 "", &power);
    if (power >= PWR_ON && rx_power[("b"[0] - 'a')] == PWR_ON)
        return RETURN_SUCCESS;
    if (power >= PWR_ON) {
        rx_power[("b"[0] - 'a')] = PWR_ON;
        strcpy(buf, "board -c "
                    "b"
                    " -d\r");
        send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
        usleep(200000);
        for (i = 0; i < (NUM_CHANNELS * 2); i++) {
            read_hps_reg(reg4[i], &old_val);
            write_hps_reg(reg4[i], old_val & ~0x100);
        }
        sync_channels(15);
        for (i = 0; i < NUM_CHANNELS; i++) {
            if (tx_power[i] == PWR_ON) {
                read_hps_reg(reg4[i + 4], &old_val);
                write_hps_reg(reg4[i + 4], old_val | 0x100);
                read_hps_reg(reg4[i + 4], &old_val);
                PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__,
                      toupper(CHAR(b)));
                write_hps_reg(reg4[i + 4], old_val | 0x2);
                write_hps_reg(reg4[i + 4], old_val & (~0x2));
            }
            if (rx_stream[i] == STREAM_ON) {
                read_hps_reg(reg4[i], &old_val);
                write_hps_reg(reg4[i], old_val | 0x100);
                read_hps_reg(reg4[i], &old_val);
                write_hps_reg(reg4[i], old_val | 0x2);
                write_hps_reg(reg4[i], old_val & (~0x2));
            }
        }
    } else {
        rx_power[("b"[0] - 'a')] = PWR_OFF;
        rx_stream[("b"[0] - 'a')] = STREAM_OFF;
        strcpy(buf, "board -c "
                    "b"
                    " -k\r");
        send_uart_comm(uart_rx_fd[("b"[0] - 'a')], (uint8_t *)buf, strlen(buf));
        read_hps_reg("rx"
                     "b"
                     "4",
                     &old_val);
        write_hps_reg("rx"
                      "b"
                      "4",
                      old_val | 0x2);
        read_hps_reg("rx"
                     "b"
                     "4",
                     &old_val);
        write_hps_reg("rx"
                      "b"
                      "4",
                      old_val & (~0x100));
    }
    return RETURN_SUCCESS;
}
