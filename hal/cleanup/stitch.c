static int hdlr_tx_a_rf_dac_dither_en(const char *data, char *ret) {
    int r;
    int en;

    r = sscanf(data, "%d", &en);
    if (1 != r) {
        return RETURN_ERROR;
    }
    if (en < 0 || en > 1) {
        return RETURN_ERROR_PARAM;
    }
    snprintf(buf, sizeof(buf), "dac -c a -l %u\r", en);
    sprintf(ret, "%u", en);
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_dac_dither_mixer_en(const char *data, char *ret) {
    int r;
    int en;

    r = sscanf(data, "%d", &en);
    if (1 != r) {
        return RETURN_ERROR;
    }
    if (en < 0 || en > 1) {
        return RETURN_ERROR_PARAM;
    }
    snprintf(buf, sizeof(buf), "dac -c a -3 %u\r", en);
    sprintf(ret, "%u", en);
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_dac_dither_sra_sel(const char *data, char *ret) {
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

    // 96 dB (max) := 0x0, 90 dB := 0x1, ... 6 dB (min) := 0xf
    sel = 16 - (db / 6);

    snprintf(buf, sizeof(buf), "dac -c a -b %u\r", sel);
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_dac_nco(const char *data, char *ret) {
    double freq;
    sscanf(data, "%lf", &freq);
    uint64_t nco_steps = (uint64_t)round(freq * DAC_NCO_CONST);
    sprintf(ret, "%lf", (double)nco_steps / DAC_NCO_CONST);

    strcpy(buf, "dac -c a -e 0 -n ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    strcpy(buf, "dac -o ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_dac_temp(const char *data, char *ret) {
    strcpy(buf, "board -c a -t\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_freq_val(const char *data, char *ret) {
    uint64_t freq = 0;
    sscanf(data, "%" SCNd64 "", &freq);

    // if freq = 0, mute PLL
    if (freq == 0) {
        strcpy(buf, "rf -c a -z\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

        return RETURN_SUCCESS;
    }

    // if freq out of bounds, kill channel
    if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
        strcpy(buf, "board -c a -k\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

        // Turn OFF TXA on HPS
        uint32_t old_val;

        // disable DSP cores
        read_hps_reg("txa4", &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + 0);
        write_hps_reg("txa4", old_val | 0x2);

        // disable channel
        read_hps_reg("txa4", &old_val);
        write_hps_reg("txa4", old_val & (~0x100));

        tx_power[0] = PWR_OFF;

        PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting "
                     "Down TXA.\n");

        return RETURN_ERROR;
    }

    // run the pll calc algorithm
    pllparam_t pll;
    long double outfreq = 0;
    outfreq = setFreq(&freq, &pll);

    strcpy(buf, "rf -c a \r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    // TODO: pll1.power setting TBD (need to modify pllparam_t)

    // Send Parameters over to the MCU
    set_pll_frequency(uart_tx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                      0);

    sprintf(ret, "%lf", outfreq);

    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_freq_band(const char *data, char *ret) {
    strcpy(buf, "rf -c a -b ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_freq_i_bias(const char *data, char *ret) {
    sscanf(data, "%i", &(i_bias[0]));
    strcpy(buf, "rf -c a -i ");
    sprintf(buf + strlen(buf), "%i", i_bias[0]);
    strcat(buf, " -q ");
    sprintf(buf + strlen(buf), "%i", q_bias[0]);
    strcat(buf, " -m\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_freq_q_bias(const char *data, char *ret) {
    sscanf(data, "%i", &(q_bias[0]));
    strcpy(buf, "rf -c a -i ");
    sprintf(buf + strlen(buf), "%i", i_bias[0]);
    strcat(buf, " -q ");
    sprintf(buf + strlen(buf), "%i", q_bias[0]);
    strcat(buf, " -m\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_gain_val(const char *data, char *ret) {
    int gain;
    sscanf(data, "%i", &gain);

    // 0   -> 126	attenuation only
    // 127		0dB

    if (gain > 127)
        gain = 127;
    else if (gain < 0)
        gain = 0;

    strcpy(buf, "rf -c a -a ");
    sprintf(buf + strlen(buf), "%i", 127 - gain);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_board_dump(const char *data, char *ret) {
    // send the uart commands and read back the output and write to file

    // DAC
    strcpy(buf, "dump -c a -d\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);

    // GPIOX
    strcpy(buf, "dump -c a -g\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_board_test(const char *data, char *ret) {
    // TODO: MCU code cleanup
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -c a -t\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_a_status_rfld(const char *data, char *ret) {
    strcpy(buf, "status -c a -l\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_a_status_dacld(const char *data, char *ret) {
    strcpy(buf, "status -c a -p\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_a_status_dacctr(const char *data, char *ret) {
    strcpy(buf, "status -c a -e\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_board_led(const char *data, char *ret) {
    strcpy(buf, "board -l\r");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_dsp_gain(const char *data, char *ret) {
    // TODO: FW code
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_dsp_rate(const char *data, char *ret) {
    uint32_t old_val;
    uint16_t base_factor, resamp_factor;
    double base_err, resamp_err;
    double rate;
    sscanf(data, "%lf", &rate);

    // get the error for base rate
    base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
    resamp_factor =
        get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

    // set the appropriate sample rate
    memset(ret, 0, MAX_PROP_LEN);

    if (resamp_err < base_err) {
        write_hps_reg("txa1", resamp_factor);
        read_hps_reg("txa4", &old_val);
        write_hps_reg("txa4", old_val | (1 << 15));
        sprintf(ret, "%lf", RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));
        // Set gain adjustment
        read_hps_reg("txga", &old_val);
        write_hps_reg("txga", (old_val & ~(0xff << 0)) |
                                  (interp_gain_lut[(resamp_factor)] << 0));
    } else {
        write_hps_reg("txa1", base_factor);
        read_hps_reg("txa4", &old_val);
        write_hps_reg("txa4", old_val & ~(1 << 15));
        sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1));
        // Set gain adjustment
        read_hps_reg("txga", &old_val);
        write_hps_reg("txga", (old_val & ~(0xff << 0)) |
                                  (interp_gain_lut[(base_factor)] << 0));
    }

    return RETURN_SUCCESS;
}

static int hdlr_tx_a_dsp_nco_adj(const char *data, char *ret) {
    double freq;
    uint32_t old_val;
    uint8_t direction;

    // check for a minus or plus sign at the front
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

    // write NCO adj
    uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);
    write_hps_reg("txa0", nco_steps);
    if (direction > 0) {
        sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
    } else {
        sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
    }

    // write direction
    read_hps_reg("txa4", &old_val);
    write_hps_reg("txa4", (old_val & ~(0x1 << 13)) | (direction << 13));
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_dsp_rstreq(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("txa4", &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + 0);
    write_hps_reg("txa4", old_val | 0x2);
    write_hps_reg("txa4", old_val & ~0x2);
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_about_id(const char *data, char *ret) {
    // don't need to do anything, save the ID in the file system
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_link_vita_en(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("txa4", &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("txa4", old_val | (1 << 14));
    else
        write_hps_reg("txa4", old_val & ~(1 << 14));

    // sync_channels( 15 );

    return RETURN_SUCCESS;
}

static int hdlr_tx_a_link_iface(const char *data, char *ret) {
    // TODO: FW support for streaming to management port required
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_link_port(const char *data, char *ret) {
    uint32_t port;
    sscanf(data, "%" SCNd32 "", &port);
    write_hps_reg("txa5", port);
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_qa_fifo_lvl(const char *data, char *ret) {
    uint32_t lvl;
    read_hps_reg("res_ro4", &lvl);
    lvl &= 0xffff;
    sprintf(ret, "%u", lvl);
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_qa_oflow(const char *data, char *ret) {
    uint32_t count;
    // this is technically a 64-bit register, but we currently only need the
    // bottom 32-bits
    read_hps_reg("flc14", &count);
    sprintf(ret, "%u", count);
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_qa_uflow(const char *data, char *ret) {
    uint32_t count;
    // this is technically a 64-bit register, but we currently only need the
    // bottom 32-bits
    read_hps_reg("flc6", &count);
    sprintf(ret, "%u", count);
    return RETURN_SUCCESS;
}

static int hdlr_tx_a_pwr(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t power;
    uint8_t i;
    sscanf(data, "%" SCNd8 "", &power);

    // check if power is already enabled
    if (power >= PWR_ON && tx_power[0] == PWR_ON)
        return RETURN_SUCCESS;

    // power on
    if (power >= PWR_ON) {
        tx_power[0] = PWR_ON;

        // board commands
        strcpy(buf, "board -c a -d\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
        usleep(200000);

        // disable dsp channels
        for (i = 0; i < (NUM_CHANNELS * 2); i++) {
            read_hps_reg(reg4[i], &old_val);
            write_hps_reg(reg4[i], old_val & ~0x100);
        }

        // send sync pulse
        sync_channels(15);

        // enable active dsp channels, and reset the DSP
        for (i = 0; i < NUM_CHANNELS; i++) {
            if (tx_power[i] == PWR_ON) {
                read_hps_reg(reg4[i + 4], &old_val);
                write_hps_reg(reg4[i + 4], old_val | 0x100);
                read_hps_reg(reg4[i + 4], &old_val);
                PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + i);
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

        // power off
    } else {
        // kill the channel
        strcpy(buf, "board -c a -k\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

        // disable DSP cores
        read_hps_reg("txa4", &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + i);
        write_hps_reg("txa4", old_val | 0x2);

        // disable channel
        read_hps_reg("txa4", &old_val);
        write_hps_reg("txa4", old_val & (~0x100));

        tx_power[0] = PWR_OFF;
    }

    return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_freq_val(const char *data, char *ret) {
    uint64_t freq = 0;
    sscanf(data, "%" SCNd64 "", &freq);

    // if freq = 0, mute PLL
    if (freq == 0) {
        strcpy(buf, "rf -c a -z\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

        return RETURN_SUCCESS;
    }

    // if freq out of bounds, kill channel
    if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
        strcpy(buf, "board -c a -k\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

        // Turn OFF RXA on HPS
        uint32_t old_val;

        // disable DSP core
        read_hps_reg("rxa4", &old_val);
        write_hps_reg("rxa4", old_val | 0x2);

        // disable channel
        read_hps_reg("rxa4", &old_val);
        write_hps_reg("rxa4", old_val & (~0x100));

        rx_power[0] = PWR_OFF;
        rx_stream[0] = STREAM_OFF;

        PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting "
                     "Down RXA.\n");

        return RETURN_ERROR;
    }

    // run the pll calc algorithm
    pllparam_t pll;
    long double outfreq = 0;
    outfreq = setFreq(&freq, &pll);

    strcpy(buf, "rf -c a \r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

    // TODO: pll1.power setting TBD (need to modify pllparam_t)

    // Send Parameters over to the MCU
    set_pll_frequency(uart_rx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                      0);

    sprintf(ret, "%lf", outfreq);

    return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_freq_lna(const char *data, char *ret) {
    strcpy(buf, "rf -c a -l ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_freq_band(const char *data, char *ret) {
    strcpy(buf, "rf -c a -b ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_gain_val(const char *data, char *ret) {
    int gain;
    sscanf(data, "%i", &gain);

    if (gain > 126)
        gain = 126;
    else if (gain < 0)
        gain = 0;

    if (gain % 2)
        gain++; // Odd Number

    // 0 -> 126 gain
    strcpy(buf, "vga -c a -g ");
    sprintf(buf + strlen(buf), "%i", gain >> 1);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_atten_val(const char *data, char *ret) {
    int atten;
    sscanf(data, "%i", &atten);

    if (atten > 127)
        atten = 127;
    else if (atten < 0)
        atten = 0;

    strcpy(buf, "rf -c a -a ");
    sprintf(buf + strlen(buf), "%i", atten);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_board_dump(const char *data, char *ret) {
    // send the uart commands and read back the output and write to file

    // ADC
    strcpy(buf, "dump -c a -a\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);

    // GPIOX
    strcpy(buf, "dump -c a -g\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);

    // ADC Driver
    strcpy(buf, "dump -c a -v\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_board_test(const char *data, char *ret) {
    // TODO: MCU code cleanup
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -c a -t\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_a_status_rfld(const char *data, char *ret) {
    strcpy(buf, "status -c a -l\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_a_status_adcalarm(const char *data, char *ret) {
    strcpy(buf, "status -c a -a\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_board_led(const char *data, char *ret) {
    strcpy(buf, "board -l\r");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_dsp_signed(const char *data, char *ret) {
    uint32_t old_val, sign;
    sscanf(data, "%u", &sign);
    sign = sign ? 0 : 1;

    read_hps_reg("rxa4", &old_val);
    old_val &= ~(1 << 4);
    write_hps_reg("rxa4", old_val | (sign << 4));
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_dsp_gain(const char *data, char *ret) {
    // TODO: FW code
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_dsp_rate(const char *data, char *ret) {
    uint32_t old_val;
    uint16_t base_factor, resamp_factor;
    double base_err, resamp_err;
    double rate;
    sscanf(data, "%lf", &rate);

    // get the error for base rate
    base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
    resamp_factor =
        get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

    // set the appropriate sample rate
    memset(ret, 0, MAX_PROP_LEN);
    int gain_factor;

    if (resamp_err < base_err) {
        write_hps_reg("rxa1", resamp_factor);
        read_hps_reg("rxa4", &old_val);
        write_hps_reg("rxa4", old_val | (1 << 15));
        sprintf(ret, "%lf", RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));
        // Set gain adjustment
        gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
        read_hps_reg("rxga", &old_val);
        write_hps_reg("rxga", (old_val & ~(0xff << 0)) |
                                  (((uint16_t)gain_factor) << 0));
    } else {
        write_hps_reg("rxa1", base_factor);
        read_hps_reg("rxa4", &old_val);
        write_hps_reg("rxa4", old_val & ~(1 << 15));
        sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1));
        // Set gain adjustment
        gain_factor = decim_gain_lut[(base_factor)];
        read_hps_reg("rxga", &old_val);
        write_hps_reg("rxga", (old_val & ~(0xff << 0)) |
                                  (((uint16_t)gain_factor) << 0));
    }

    return RETURN_SUCCESS;
}

static int hdlr_rx_a_dsp_nco_adj(const char *data, char *ret) {
    double freq;
    uint32_t old_val;
    uint8_t direction;

    // check for a minus or plus sign at the front
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

    // write NCO adj
    uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);
    write_hps_reg("rxa0", nco_steps);
    if (direction > 0) {
        sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
    } else {
        sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
    }

    // write direction
    read_hps_reg("rxa4", &old_val);
    write_hps_reg("rxa4", (old_val & ~(0x1 << 13)) | (direction << 13));
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_dsp_rstreq(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rxa4", &old_val);
    write_hps_reg("rxa4", old_val | 0x2);
    write_hps_reg("rxa4", old_val & ~0x2);
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_dsp_loopback(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rxa4", &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("rxa4", (old_val & ~0x1e00) | 0x400);
    else
        write_hps_reg("rxa4", (old_val & ~0x1e00) | 0x000);
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_about_id(const char *data, char *ret) {
    // don't need to do anything, save the ID in the file system
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_link_vita_en(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rxa4", &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("rxa4", old_val | (1 << 14));
    else
        write_hps_reg("rxa4", old_val & ~(1 << 14));

    // sync_channels( 15 );

    return RETURN_SUCCESS;
}

static int hdlr_rx_a_link_iface(const char *data, char *ret) {
    // TODO: FW support for streaming to management port required
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_link_port(const char *data, char *ret) {
    uint32_t port;
    sscanf(data, "%" SCNd32 "", &port);
    write_hps_reg("rxa8", port);
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_link_ip_dest(const char *data, char *ret) {
    uint8_t ip[4];
    sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
           ip + 2, ip + 3);
    write_hps_reg("rxa5",
                  (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_link_mac_dest(const char *data, char *ret) {
    uint8_t mac[6];
    sscanf(data,
           "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
           mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
    write_hps_reg("rxa6", (mac[0] << 8) | (mac[1]));
    write_hps_reg("rxa7",
                  (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
    return RETURN_SUCCESS;
}

static int hdlr_rx_a_stream(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t stream;
    sscanf(data, "%" SCNd8 "", &stream);

    // if stream > 1, check the status of the stream
    if (stream > 1) {
        sprintf(ret, "%u", rx_stream[0]); // Alert File Tree
        return RETURN_SUCCESS;
    }

    // Stream is already ON or OFF then return
    if (stream == rx_stream[0])
        return RETURN_SUCCESS;

    // Otherwise make the change accordingly
    if (stream > 0) { // TURN THE STREAM ON
        if (rx_power[0] == PWR_ON) {
            read_hps_reg(reg4[0], &old_val);
            write_hps_reg(reg4[0], old_val | 0x100);

            read_hps_reg(reg4[0], &old_val);
            write_hps_reg(reg4[0], old_val | 0x2);
            write_hps_reg(reg4[0], old_val & (~0x2));

            rx_stream[0] = STREAM_ON;
        } else {
            // Do not turn ON stream if channel is OFF
            sprintf(ret, "%u", 0); // Alert File Tree
        }
    } else { // TURN THE STREAM OFF
        // disable DSP core
        read_hps_reg("rxa4", &old_val);
        write_hps_reg("rxa4", old_val | 0x2);

        // disable channel
        read_hps_reg("rxa4", &old_val);
        write_hps_reg("rxa4", old_val & (~0x100));

        rx_stream[0] = STREAM_OFF;
    }

    return RETURN_SUCCESS;
}

static int hdlr_rx_a_pwr(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t power;
    uint8_t i;
    sscanf(data, "%" SCNd8 "", &power);

    // check if power is already enabled
    if (power >= PWR_ON && rx_power[0] == PWR_ON)
        return RETURN_SUCCESS;

    // power on
    if (power >= PWR_ON) {
        rx_power[0] = PWR_ON;

        // board command
        strcpy(buf, "board -c a -d\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
        usleep(200000);

        // disable dsp channels
        for (i = 0; i < (NUM_CHANNELS * 2); i++) {
            read_hps_reg(reg4[i], &old_val);
            write_hps_reg(reg4[i], old_val & ~0x100);
        }

        // send sync pulse
        sync_channels(15);

        // Enable active dsp channels, and reset DSP
        for (i = 0; i < NUM_CHANNELS; i++) {
            if (tx_power[i] == PWR_ON) {
                read_hps_reg(reg4[i + 4], &old_val);
                write_hps_reg(reg4[i + 4], old_val | 0x100);
                read_hps_reg(reg4[i + 4], &old_val);
                PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + i);
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

        // power off & stream off
    } else {
        rx_power[0] = PWR_OFF;
        rx_stream[0] = STREAM_OFF;

        // kill the channel
        strcpy(buf, "board -c a -k\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

        // disable DSP core
        read_hps_reg("rxa4", &old_val);
        write_hps_reg("rxa4", old_val | 0x2);

        // disable channel
        read_hps_reg("rxa4", &old_val);
        write_hps_reg("rxa4", old_val & (~0x100));
    }
    return RETURN_SUCCESS;
}

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
    snprintf(buf, sizeof(buf), "dac -c b -l %u\r", en);
    sprintf(ret, "%u", en);
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
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
    snprintf(buf, sizeof(buf), "dac -c b -3 %u\r", en);
    sprintf(ret, "%u", en);
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
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

    // 96 dB (max) := 0x0, 90 dB := 0x1, ... 6 dB (min) := 0xf
    sel = 16 - (db / 6);

    snprintf(buf, sizeof(buf), "dac -c b -b %u\r", sel);
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_dac_nco(const char *data, char *ret) {
    double freq;
    sscanf(data, "%lf", &freq);
    uint64_t nco_steps = (uint64_t)round(freq * DAC_NCO_CONST);
    sprintf(ret, "%lf", (double)nco_steps / DAC_NCO_CONST);

    strcpy(buf, "dac -c b -e 1 -n ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    strcpy(buf, "dac -o ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_dac_temp(const char *data, char *ret) {
    strcpy(buf, "board -c b -t\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_freq_val(const char *data, char *ret) {
    uint64_t freq = 0;
    sscanf(data, "%" SCNd64 "", &freq);

    // if freq = 0, mute PLL
    if (freq == 0) {
        strcpy(buf, "rf -c b -z\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

        return RETURN_SUCCESS;
    }

    // if freq out of bounds, kill channel
    if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
        strcpy(buf, "board -c b -k\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

        // Turn OFF TXB on HPS
        uint32_t old_val;

        // disable the DSP cores
        read_hps_reg("txb4", &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + 1);
        write_hps_reg("txb4", old_val | 0x2);

        // disable 10G transmission
        read_hps_reg("txb4", &old_val);
        write_hps_reg("txb4", old_val & (~0x100));

        tx_power[1] = PWR_OFF;

        PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting "
                     "Down TXB.\n");

        return RETURN_ERROR;
    }

    // run the pll calc algorithm
    pllparam_t pll;
    long double outfreq = 0;
    outfreq = setFreq(&freq, &pll);

    strcpy(buf, "rf -c b \r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    // TODO: pll1.power setting TBD (need to modify pllparam_t)

    // Send Parameters over to the MCU
    set_pll_frequency(uart_tx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                      1);

    sprintf(ret, "%lf", outfreq);

    return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_freq_band(const char *data, char *ret) {
    strcpy(buf, "rf -c b -b ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_freq_i_bias(const char *data, char *ret) {
    sscanf(data, "%i", &(i_bias[1]));
    strcpy(buf, "rf -c b -i ");
    sprintf(buf + strlen(buf), "%i", i_bias[1]);
    strcat(buf, " -q ");
    sprintf(buf + strlen(buf), "%i", q_bias[1]);
    strcat(buf, " -m\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_freq_q_bias(const char *data, char *ret) {
    sscanf(data, "%i", &(q_bias[1]));
    strcpy(buf, "rf -c b -i ");
    sprintf(buf + strlen(buf), "%i", i_bias[1]);
    strcat(buf, " -q ");
    sprintf(buf + strlen(buf), "%i", q_bias[1]);
    strcat(buf, " -m\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_gain_val(const char *data, char *ret) {
    int gain;
    sscanf(data, "%i", &gain);

    // 0   -> 126	attenuation only
    // 127		0dB

    if (gain > 127)
        gain = 127;
    else if (gain < 0)
        gain = 0;

    strcpy(buf, "rf -c b -a ");
    sprintf(buf + strlen(buf), "%i", 127 - gain);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_board_dump(const char *data, char *ret) {
    // send the uart commands and read back the output and write to file

    // DAC
    strcpy(buf, "dump -c b -d\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    PRINT(DUMP, "[Board: tx_b Chip: DAC] %s\n", uart_ret_buf);

    // GPIOX
    strcpy(buf, "dump -c b -g\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    PRINT(DUMP, "[Board: tx_b Chip: GPIOX] %s\n", uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_board_test(const char *data, char *ret) {
    // TODO: MCU code cleanup
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -c b -t\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_b_status_rfld(const char *data, char *ret) {
    strcpy(buf, "status -c b -l\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_b_status_dacld(const char *data, char *ret) {
    strcpy(buf, "status -c b -p\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_b_status_dacctr(const char *data, char *ret) {
    strcpy(buf, "status -c b -e\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_board_led(const char *data, char *ret) {
    strcpy(buf, "board -l\r");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_dsp_gain(const char *data, char *ret) {
    // TODO: FW code
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_dsp_rate(const char *data, char *ret) {
    uint32_t old_val;
    uint16_t base_factor, resamp_factor;
    double base_err, resamp_err;
    double rate;
    sscanf(data, "%lf", &rate);

    // get the error for base rate
    base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
    resamp_factor =
        get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

    // set the appropriate sample rate
    memset(ret, 0, MAX_PROP_LEN);

    if (resamp_err < base_err) {
        write_hps_reg("txb1", resamp_factor);
        read_hps_reg("txb4", &old_val);
        write_hps_reg("txb4", old_val | (1 << 15));
        sprintf(ret, "%lf", RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));
        // Set gain adjustment
        read_hps_reg("txga", &old_val);
        write_hps_reg("txga", (old_val & ~(0xff << 8)) |
                                  (interp_gain_lut[(resamp_factor)] << 8));
    } else {
        write_hps_reg("txb1", base_factor);
        read_hps_reg("txb4", &old_val);
        write_hps_reg("txb4", old_val & ~(1 << 15));
        sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1));
        // Set gain adjustment
        read_hps_reg("txga", &old_val);
        write_hps_reg("txga", (old_val & ~(0xff << 8)) |
                                  (interp_gain_lut[(base_factor)] << 8));
    }

    return RETURN_SUCCESS;
}

static int hdlr_tx_b_dsp_nco_adj(const char *data, char *ret) {
    double freq;
    uint32_t old_val;
    uint8_t direction;

    // check for a minus or plus sign at the front
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

    // write NCO adj
    uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);
    write_hps_reg("txb0", nco_steps);
    if (direction > 0) {
        sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
    } else {
        sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
    }

    // write direction
    read_hps_reg("txb4", &old_val);
    write_hps_reg("txb4", (old_val & ~(0x1 << 13)) | (direction << 13));
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_dsp_rstreq(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("txb4", &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + 1);
    write_hps_reg("txb4", old_val | 0x2);
    write_hps_reg("txb4", old_val & ~0x2);
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_about_id(const char *data, char *ret) {
    // don't need to do anything, save the ID in the file system
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_link_vita_en(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("txb4", &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("txb4", old_val | (1 << 14));
    else
        write_hps_reg("txb4", old_val & ~(1 << 14));

    // sync_channels( 15 );

    return RETURN_SUCCESS;
}

static int hdlr_tx_b_link_iface(const char *data, char *ret) {
    // TODO: FW support for streaming to management port required
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_link_port(const char *data, char *ret) {
    uint32_t port;
    sscanf(data, "%" SCNd32 "", &port);
    write_hps_reg("txb5", port);
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_qa_fifo_lvl(const char *data, char *ret) {
    uint32_t lvl;
    read_hps_reg("res_ro5", &lvl);
    lvl &= 0xffff;
    sprintf(ret, "%u", lvl);
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_qa_oflow(const char *data, char *ret) {
    uint32_t count;
    // this is technically a 64-bit register, but we currently only need the
    // bottom 32-bits
    read_hps_reg("flc16", &count);
    sprintf(ret, "%u", count);
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_qa_uflow(const char *data, char *ret) {
    uint32_t count;
    // this is technically a 64-bit register, but we currently only need the
    // bottom 32-bits
    read_hps_reg("flc8", &count);
    sprintf(ret, "%u", count);
    return RETURN_SUCCESS;
}

static int hdlr_tx_b_pwr(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t power;
    uint8_t i;
    sscanf(data, "%" SCNd8 "", &power);

    // check it power is already enabled
    if (power >= PWR_ON && tx_power[1] == PWR_ON)
        return RETURN_SUCCESS;

    // power on
    if (power >= PWR_ON) {
        tx_power[1] = PWR_ON;

        // board commands
        strcpy(buf, "board -c b -d\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
        usleep(200000);

        // disable dsp channels
        for (i = 0; i < (NUM_CHANNELS * 2); i++) {
            read_hps_reg(reg4[i], &old_val);
            write_hps_reg(reg4[i], old_val & ~0x100);
        }

        // send sync pulse
        sync_channels(15);

        // enable active dsp channels, and reset the DSP
        for (i = 0; i < NUM_CHANNELS; i++) {
            if (tx_power[i] == PWR_ON) {
                read_hps_reg(reg4[i + 4], &old_val);
                write_hps_reg(reg4[i + 4], old_val | 0x100);
                read_hps_reg(reg4[i + 4], &old_val);
                PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + i);
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

        // power off
    } else {
        // kill the channel
        strcpy(buf, "board -c b -k\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

        // disable the DSP cores
        read_hps_reg("txb4", &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + 1);
        write_hps_reg("txb4", old_val | 0x2);

        // disable 10G transmission
        read_hps_reg("txb4", &old_val);
        write_hps_reg("txb4", old_val & (~0x100));

        tx_power[1] = PWR_OFF;
    }

    return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_freq_val(const char *data, char *ret) {
    uint64_t freq = 0;
    sscanf(data, "%" SCNd64 "", &freq);

    // if freq = 0, mute PLL
    if (freq == 0) {
        strcpy(buf, "rf -c b -z\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

        return RETURN_SUCCESS;
    }

    // if freq out of bounds, kill channel
    if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
        strcpy(buf, "board -c b -k\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

        // Turn OFF RXB on HPS
        uint32_t old_val;

        // disable DSP core
        read_hps_reg("rxb4", &old_val);
        write_hps_reg("rxb4", old_val | 0x2);

        // disable channel
        read_hps_reg("rxb4", &old_val);
        write_hps_reg("rxb4", old_val & (~0x100));

        rx_power[1] = PWR_OFF;
        rx_stream[1] = STREAM_OFF;

        PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting "
                     "Down RXB.\n");

        return RETURN_ERROR;
    }

    // run the pll calc algorithm
    pllparam_t pll;
    long double outfreq = 0;
    outfreq = setFreq(&freq, &pll);

    strcpy(buf, "rf -c b \r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

    // TODO: pll1.power setting TBD (need to modify pllparam_t)

    // Send Parameters over to the MCU
    set_pll_frequency(uart_rx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                      1);

    sprintf(ret, "%lf", outfreq);

    return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_freq_lna(const char *data, char *ret) {
    strcpy(buf, "rf -c b -l ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_freq_band(const char *data, char *ret) {
    strcpy(buf, "rf -c b -b ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
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
        gain++; // Odd Number

    // 0 -> 126 gain
    strcpy(buf, "vga -c b -g ");
    sprintf(buf + strlen(buf), "%i", gain >> 1);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_atten_val(const char *data, char *ret) {
    int atten;
    sscanf(data, "%i", &atten);

    if (atten > 127)
        atten = 127;
    else if (atten < 0)
        atten = 0;

    strcpy(buf, "rf -c b -a ");
    sprintf(buf + strlen(buf), "%i", atten);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_board_dump(const char *data, char *ret) {
    // send the uart commands and read back the output and write to file

    // ADC
    strcpy(buf, "dump -c b -a\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    PRINT(DUMP, "[Board: rx_b Chip: ADC] %s\n", uart_ret_buf);

    // GPIOX
    strcpy(buf, "dump -c b -g\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    PRINT(DUMP, "[Board: rx_b Chip: GPIOX] %s\n", uart_ret_buf);

    // ADC Driver
    strcpy(buf, "dump -c b -v\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    PRINT(DUMP, "[Board: rx_b Chip: ADC Driver] %s\n", uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_board_test(const char *data, char *ret) {
    // TODO: MCU code cleanup
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -c b -t\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_b_status_rfld(const char *data, char *ret) {
    strcpy(buf, "status -c b -l\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_b_status_adcalarm(const char *data, char *ret) {
    strcpy(buf, "status -c b -a\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_board_led(const char *data, char *ret) {
    strcpy(buf, "board -l\r");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_dsp_signed(const char *data, char *ret) {
    uint32_t old_val, sign;
    sscanf(data, "%u", &sign);
    sign = sign ? 0 : 1;

    read_hps_reg("rxb4", &old_val);
    old_val &= ~(1 << 4);
    write_hps_reg("rxb4", old_val | (sign << 4));
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_dsp_gain(const char *data, char *ret) {
    // TODO: FW code
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_dsp_rate(const char *data, char *ret) {
    uint32_t old_val;
    uint16_t base_factor, resamp_factor;
    double base_err, resamp_err;
    double rate;
    sscanf(data, "%lf", &rate);

    // get the error for base rate
    base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
    resamp_factor =
        get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

    // set the appropriate sample rate
    memset(ret, 0, MAX_PROP_LEN);
    int gain_factor;

    if (resamp_err < base_err) {
        write_hps_reg("rxb1", resamp_factor);
        read_hps_reg("rxb4", &old_val);
        write_hps_reg("rxb4", old_val | (1 << 15));
        sprintf(ret, "%lf", RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));
        // Set gain adjustment
        gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
        read_hps_reg("rxga", &old_val);
        write_hps_reg("rxga", (old_val & ~(0xff << 8)) |
                                  (((uint16_t)gain_factor) << 8));
    } else {
        write_hps_reg("rxb1", base_factor);
        read_hps_reg("rxb4", &old_val);
        write_hps_reg("rxb4", old_val & ~(1 << 15));
        sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1));
        // Set gain adjustment
        gain_factor = decim_gain_lut[(base_factor)];
        read_hps_reg("rxga", &old_val);
        write_hps_reg("rxga", (old_val & ~(0xff << 8)) |
                                  (((uint16_t)gain_factor) << 8));
    }

    return RETURN_SUCCESS;
}

static int hdlr_rx_b_dsp_nco_adj(const char *data, char *ret) {
    double freq;
    uint32_t old_val;
    uint8_t direction;

    // check for a minus or plus sign at the front
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

    // write NCO adj
    uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);
    write_hps_reg("rxb0", nco_steps);
    if (direction > 0) {
        sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
    } else {
        sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
    }

    // write direction
    read_hps_reg("rxb4", &old_val);
    write_hps_reg("rxb4", (old_val & ~(0x1 << 13)) | (direction << 13));
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_dsp_rstreq(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rxb4", &old_val);
    write_hps_reg("rxb4", old_val | 0x2);
    write_hps_reg("rxb4", old_val & ~0x2);
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_dsp_loopback(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rxb4", &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("rxb4", (old_val & ~0x1e00) | 0x400);
    else
        write_hps_reg("rxb4", (old_val & ~0x1e00) | 0x200);
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_about_id(const char *data, char *ret) {
    // don't need to do anything, save the ID in the file system
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_link_vita_en(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rxb4", &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("rxb4", old_val | (1 << 14));
    else
        write_hps_reg("rxb4", old_val & ~(1 << 14));

    // sync_channels( 15 );

    return RETURN_SUCCESS;
}

static int hdlr_rx_b_link_iface(const char *data, char *ret) {
    // TODO: FW support for streaming to management port required
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_link_port(const char *data, char *ret) {
    uint32_t port;
    sscanf(data, "%" SCNd32 "", &port);
    write_hps_reg("rxb8", port);
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_link_ip_dest(const char *data, char *ret) {
    uint8_t ip[4];
    sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
           ip + 2, ip + 3);
    write_hps_reg("rxb5",
                  (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_link_mac_dest(const char *data, char *ret) {
    uint8_t mac[6];
    sscanf(data,
           "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
           mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
    write_hps_reg("rxb6", (mac[0] << 8) | (mac[1]));
    write_hps_reg("rxb7",
                  (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
    return RETURN_SUCCESS;
}

static int hdlr_rx_b_stream(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t stream;
    sscanf(data, "%" SCNd8 "", &stream);

    // if stream > 1, check the status of the stream
    if (stream > 1) {
        sprintf(ret, "%u", rx_stream[1]); // Alert File Tree
        return RETURN_SUCCESS;
    }

    // Stream is already ON or OFF then return
    if (stream == rx_stream[1])
        return RETURN_SUCCESS;

    // Otherwise make the change accordingly
    if (stream > 0) { // TURN THE STREAM ON
        if (rx_power[1] == PWR_ON) {
            read_hps_reg(reg4[1], &old_val);
            write_hps_reg(reg4[1], old_val | 0x100);

            read_hps_reg(reg4[1], &old_val);
            write_hps_reg(reg4[1], old_val | 0x2);
            write_hps_reg(reg4[1], old_val & (~0x2));

            rx_stream[1] = STREAM_ON;
        } else {
            // Do not turn ON stream if channel is OFF
            sprintf(ret, "%u", 0); // Alert File Tree
        }
    } else { // TURN THE STREAM OFF
        // disable DSP core
        read_hps_reg("rxb4", &old_val);
        write_hps_reg("rxb4", old_val | 0x2);

        // disable channel
        read_hps_reg("rxb4", &old_val);
        write_hps_reg("rxb4", old_val & (~0x100));

        rx_stream[1] = STREAM_OFF;
    }

    return RETURN_SUCCESS;
}

static int hdlr_rx_b_pwr(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t power;
    uint8_t i;
    sscanf(data, "%" SCNd8 "", &power);

    // check it power is already enabled
    if (power >= PWR_ON && rx_power[1] == PWR_ON)
        return RETURN_SUCCESS;

    // power on
    if (power >= PWR_ON) {
        rx_power[1] = PWR_ON;

        // board commands
        strcpy(buf, "board -c b -d\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
        usleep(200000);

        // disable dsp channels
        for (i = 0; i < (NUM_CHANNELS * 2); i++) {
            read_hps_reg(reg4[i], &old_val);
            write_hps_reg(reg4[i], old_val & ~0x100);
        }

        // send sync pulse
        sync_channels(15);

        // Enable active dsp channels, and reset DSP
        for (i = 0; i < NUM_CHANNELS; i++) {
            if (tx_power[i] == PWR_ON) {
                read_hps_reg(reg4[i + 4], &old_val);
                write_hps_reg(reg4[i + 4], old_val | 0x100);
                read_hps_reg(reg4[i + 4], &old_val);
                PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + i);
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

        // power off
    } else {
        rx_power[1] = PWR_OFF;
        rx_stream[1] = STREAM_OFF;

        // kill the channel
        strcpy(buf, "board -c b -k\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

        // disable DSP core
        read_hps_reg("rxb4", &old_val);
        write_hps_reg("rxb4", old_val | 0x2);

        // disable channel
        read_hps_reg("rxb4", &old_val);
        write_hps_reg("rxb4", old_val & (~0x100));
    }
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_dac_dither_en(const char *data, char *ret) {
    int r;
    int en;

    r = sscanf(data, "%d", &en);
    if (1 != r) {
        return RETURN_ERROR;
    }
    if (en < 0 || en > 1) {
        return RETURN_ERROR_PARAM;
    }
    snprintf(buf, sizeof(buf), "dac -c c -l %u\r", en);
    sprintf(ret, "%u", en);
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_dac_dither_mixer_en(const char *data, char *ret) {
    int r;
    int en;

    r = sscanf(data, "%d", &en);
    if (1 != r) {
        return RETURN_ERROR;
    }
    if (en < 0 || en > 1) {
        return RETURN_ERROR_PARAM;
    }
    snprintf(buf, sizeof(buf), "dac -c c -3 %u\r", en);
    sprintf(ret, "%u", en);
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_dac_dither_sra_sel(const char *data, char *ret) {
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

    // 96 dB (max) := 0x0, 90 dB := 0x1, ... 6 dB (min) := 0xf
    sel = 16 - (db / 6);

    snprintf(buf, sizeof(buf), "dac -c c -b %u\r", sel);
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_dac_nco(const char *data, char *ret) {
    double freq;
    sscanf(data, "%lf", &freq);
    uint64_t nco_steps = (uint64_t)round(freq * DAC_NCO_CONST);
    sprintf(ret, "%lf", (double)nco_steps / DAC_NCO_CONST);

    strcpy(buf, "dac -c c -e 0 -n ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    strcpy(buf, "dac -o ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_dac_temp(const char *data, char *ret) {
    strcpy(buf, "board -c c -t\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_freq_val(const char *data, char *ret) {
    uint64_t freq = 0;
    sscanf(data, "%" SCNd64 "", &freq);

    // if freq = 0, mute PLL
    if (freq == 0) {
        strcpy(buf, "rf -c c -z\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

        return RETURN_SUCCESS;
    }

    // if freq out of bounds, kill channel
    if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
        strcpy(buf, "board -c c -k\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

        // Turn OFF TXC on HPS
        uint32_t old_val;

        // disable the DSP cores
        read_hps_reg("txc4", &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + 2);
        write_hps_reg("txc4", old_val | 0x2);

        // disable 10G transmission
        read_hps_reg("txc4", &old_val);
        write_hps_reg("txc4", old_val & (~0x100));

        tx_power[2] = PWR_OFF;

        PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting "
                     "Down TXC.\n");

        return RETURN_ERROR;
    }

    // run the pll calc algorithm
    pllparam_t pll;
    long double outfreq = 0;
    outfreq = setFreq(&freq, &pll);

    strcpy(buf, "rf -c c \r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    // TODO: pll1.power setting TBD (need to modify pllparam_t)

    // Send Parameters over to the MCU
    set_pll_frequency(uart_tx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                      2);

    sprintf(ret, "%lf", outfreq);

    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_freq_band(const char *data, char *ret) {
    strcpy(buf, "rf -c c -b ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_freq_i_bias(const char *data, char *ret) {
    sscanf(data, "%i", &(i_bias[2]));
    strcpy(buf, "rf -c c -i ");
    sprintf(buf + strlen(buf), "%i", i_bias[2]);
    strcat(buf, " -q ");
    sprintf(buf + strlen(buf), "%i", q_bias[2]);
    strcat(buf, " -m\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_freq_q_bias(const char *data, char *ret) {
    sscanf(data, "%i", &(q_bias[2]));
    strcpy(buf, "rf -c c -i ");
    sprintf(buf + strlen(buf), "%i", i_bias[2]);
    strcat(buf, " -q ");
    sprintf(buf + strlen(buf), "%i", q_bias[2]);
    strcat(buf, " -m\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_gain_val(const char *data, char *ret) {
    int gain;
    sscanf(data, "%i", &gain);

    // 0   -> 126	attenuation only
    // 127		0dB

    if (gain > 127)
        gain = 127;
    else if (gain < 0)
        gain = 0;

    strcpy(buf, "rf -c c -a ");
    sprintf(buf + strlen(buf), "%i", 127 - gain);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_board_dump(const char *data, char *ret) {
    // send the uart commands and read back the output and write to file

    // DAC
    strcpy(buf, "dump -c c -d\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    PRINT(DUMP, "[Board: tx_c Chip: DAC] %s\n", uart_ret_buf);

    // GPIOX
    strcpy(buf, "dump -c c -g\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    PRINT(DUMP, "[Board: tx_c Chip: GPIOX] %s\n", uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_board_test(const char *data, char *ret) {
    // TODO: MCU code cleanup
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -c c -t\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_c_status_rfld(const char *data, char *ret) {
    strcpy(buf, "status -c c -l\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_c_status_dacld(const char *data, char *ret) {
    strcpy(buf, "status -c c -p\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_c_status_dacctr(const char *data, char *ret) {
    strcpy(buf, "status -c c -e\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_board_led(const char *data, char *ret) {
    strcpy(buf, "board -l\r");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_dsp_gain(const char *data, char *ret) {
    // TODO: FW code
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_dsp_rate(const char *data, char *ret) {
    uint32_t old_val;
    uint16_t base_factor, resamp_factor;
    double base_err, resamp_err;
    double rate;
    sscanf(data, "%lf", &rate);

    // get the error for base rate
    base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
    resamp_factor =
        get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

    // set the appropriate sample rate
    memset(ret, 0, MAX_PROP_LEN);

    if (resamp_err < base_err) {
        write_hps_reg("txc1", resamp_factor);
        read_hps_reg("txc4", &old_val);
        write_hps_reg("txc4", old_val | (1 << 15));
        sprintf(ret, "%lf", RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));
        // Set gain adjustment
        read_hps_reg("txga", &old_val);
        write_hps_reg("txga", (old_val & ~(0xff << 16)) |
                                  (interp_gain_lut[(resamp_factor)] << 16));
    } else {
        write_hps_reg("txc1", base_factor);
        read_hps_reg("txc4", &old_val);
        write_hps_reg("txc4", old_val & ~(1 << 15));
        sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1));
        // Set gain adjustment
        read_hps_reg("txga", &old_val);
        write_hps_reg("txga", (old_val & ~(0xff << 16)) |
                                  (interp_gain_lut[(base_factor)] << 16));
    }

    return RETURN_SUCCESS;
}

static int hdlr_tx_c_dsp_nco_adj(const char *data, char *ret) {
    double freq;
    uint32_t old_val;
    uint8_t direction;

    // check for a minus or plus sign at the front
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

    // write NCO adj
    uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);
    write_hps_reg("txc0", nco_steps);
    if (direction > 0) {
        sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
    } else {
        sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
    }

    // write direction
    read_hps_reg("txc4", &old_val);
    write_hps_reg("txc4", (old_val & ~(0x1 << 13)) | (direction << 13));
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_dsp_rstreq(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("txc4", &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + 2);
    write_hps_reg("txc4", old_val | 0x2);
    write_hps_reg("txc4", old_val & ~0x2);
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_about_id(const char *data, char *ret) {
    // don't need to do anything, save the ID in the file system
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_link_vita_en(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("txc4", &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("txc4", old_val | (1 << 14));
    else
        write_hps_reg("txc4", old_val & ~(1 << 14));

    // sync_channels( 15 );

    return RETURN_SUCCESS;
}

static int hdlr_tx_c_link_iface(const char *data, char *ret) {
    // TODO: FW support for streaming to management port required
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_link_port(const char *data, char *ret) {
    uint32_t port;
    sscanf(data, "%" SCNd32 "", &port);
    write_hps_reg("txc5", port);
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_qa_fifo_lvl(const char *data, char *ret) {
    uint32_t lvl;
    read_hps_reg("res_ro6", &lvl);
    lvl &= 0xffff;
    sprintf(ret, "%u", lvl);
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_qa_oflow(const char *data, char *ret) {
    uint32_t count;
    // this is technically a 64-bit register, but we currently only need the
    // bottom 32-bits
    read_hps_reg("flc18", &count);
    sprintf(ret, "%u", count);
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_qa_uflow(const char *data, char *ret) {
    uint32_t count;
    // this is technically a 64-bit register, but we currently only need the
    // bottom 32-bits
    read_hps_reg("flc10", &count);
    sprintf(ret, "%u", count);
    return RETURN_SUCCESS;
}

static int hdlr_tx_c_pwr(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t power;
    uint8_t i;
    sscanf(data, "%" SCNd8 "", &power);

    // check it power is already enabled
    if (power >= PWR_ON && tx_power[2] == PWR_ON)
        return RETURN_SUCCESS;

    // power on
    if (power >= PWR_ON) {
        tx_power[2] = PWR_ON;

        // board commands
        strcpy(buf, "board -c c -d\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
        usleep(200000);

        // disable dsp channels
        for (i = 0; i < (NUM_CHANNELS * 2); i++) {
            read_hps_reg(reg4[i], &old_val);
            write_hps_reg(reg4[i], old_val & ~0x100);
        }

        // send sync pulse
        sync_channels(15);

        // enable active dsp channels, and reset the DSP
        for (i = 0; i < NUM_CHANNELS; i++) {
            if (tx_power[i] == PWR_ON) {
                read_hps_reg(reg4[i + 4], &old_val);
                write_hps_reg(reg4[i + 4], old_val | 0x100);
                read_hps_reg(reg4[i + 4], &old_val);
                PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + i);
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

        // power off
    } else {
        // kill the channel
        strcpy(buf, "board -c c -k\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

        // disable the DSP cores
        read_hps_reg("txc4", &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + 2);
        write_hps_reg("txc4", old_val | 0x2);

        // disable 10G transmission
        read_hps_reg("txc4", &old_val);
        write_hps_reg("txc4", old_val & (~0x100));

        tx_power[2] = PWR_OFF;
    }

    return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_freq_val(const char *data, char *ret) {
    uint64_t freq = 0;
    sscanf(data, "%" SCNd64 "", &freq);

    // if freq = 0, mute PLL
    if (freq == 0) {
        strcpy(buf, "rf -c c -z\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

        return RETURN_SUCCESS;
    }

    // if freq out of bounds, kill channel
    if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
        strcpy(buf, "board -c c -k\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

        // Turn OFF RXC on HPS
        uint32_t old_val;

        // disable the DSP cores
        read_hps_reg("rxc4", &old_val);
        write_hps_reg("rxc4", old_val | 0x2);

        // disable 10G transmission
        read_hps_reg("rxc4", &old_val);
        write_hps_reg("rxc4", old_val & (~0x100));

        rx_power[2] = PWR_OFF;
        rx_stream[2] = STREAM_OFF;

        PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting "
                     "Down RXC.\n");

        return RETURN_ERROR;
    }

    // run the pll calc algorithm
    pllparam_t pll;
    long double outfreq = 0;
    outfreq = setFreq(&freq, &pll);

    strcpy(buf, "rf -c c \r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

    // TODO: pll1.power setting TBD (need to modify pllparam_t)

    // Send Parameters over to the MCU
    set_pll_frequency(uart_rx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                      2);

    sprintf(ret, "%lf", outfreq);

    return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_freq_lna(const char *data, char *ret) {
    strcpy(buf, "rf -c c -l ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_freq_band(const char *data, char *ret) {
    strcpy(buf, "rf -c c -b ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_gain_val(const char *data, char *ret) {
    int gain;
    sscanf(data, "%i", &gain);

    if (gain > 126)
        gain = 126;
    else if (gain < 0)
        gain = 0;

    if (gain % 2)
        gain++; // Odd Number

    // 0 -> 126 gain
    strcpy(buf, "vga -c c -g ");
    sprintf(buf + strlen(buf), "%i", gain >> 1);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_atten_val(const char *data, char *ret) {
    int atten;
    sscanf(data, "%i", &atten);

    if (atten > 127)
        atten = 127;
    else if (atten < 0)
        atten = 0;

    strcpy(buf, "rf -c c -a ");
    sprintf(buf + strlen(buf), "%i", atten);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_board_dump(const char *data, char *ret) {
    // send the uart commands and read back the output and write to file

    // ADC
    strcpy(buf, "dump -c c -a\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    PRINT(DUMP, "[Board: rx_c Chip: ADC] %s\n", uart_ret_buf);

    // GPIOX
    strcpy(buf, "dump -c c -g\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    PRINT(DUMP, "[Board: rx_c Chip: GPIOX] %s\n", uart_ret_buf);

    // ADC Driver
    strcpy(buf, "dump -c c -v\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    PRINT(DUMP, "[Board: rx_c Chip: ADC Driver] %s\n", uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_board_test(const char *data, char *ret) {
    // TODO: MCU code cleanup
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -c c -t\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_c_status_rfld(const char *data, char *ret) {
    strcpy(buf, "status -c c -l\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_c_status_adcalarm(const char *data, char *ret) {
    strcpy(buf, "status -c c -a\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_board_led(const char *data, char *ret) {
    strcpy(buf, "board -l\r");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_dsp_signed(const char *data, char *ret) {
    uint32_t old_val, sign;
    sscanf(data, "%u", &sign);
    sign = sign ? 0 : 1;

    read_hps_reg("rxc4", &old_val);
    old_val &= ~(1 << 4);
    write_hps_reg("rxc4", old_val | (sign << 4));
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_dsp_gain(const char *data, char *ret) {
    // TODO: FW code
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_dsp_rate(const char *data, char *ret) {
    uint32_t old_val;
    uint16_t base_factor, resamp_factor;
    double base_err, resamp_err;
    double rate;
    sscanf(data, "%lf", &rate);

    // get the error for base rate
    base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
    resamp_factor =
        get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

    // set the appropriate sample rate
    memset(ret, 0, MAX_PROP_LEN);
    int gain_factor;
    if (resamp_err < base_err) {
        write_hps_reg("rxc1", resamp_factor);
        read_hps_reg("rxc4", &old_val);
        write_hps_reg("rxc4", old_val | (1 << 15));
        sprintf(ret, "%lf", RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));
        // Set gain adjustment
        gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
        read_hps_reg("rxga", &old_val);
        write_hps_reg("rxga", (old_val & ~(0xff << 16)) |
                                  (((uint16_t)gain_factor) << 16));
    } else {
        write_hps_reg("rxc1", base_factor);
        read_hps_reg("rxc4", &old_val);
        write_hps_reg("rxc4", old_val & ~(1 << 15));
        sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1));
        // Set gain adjustment
        gain_factor = decim_gain_lut[(base_factor)];
        read_hps_reg("rxga", &old_val);
        write_hps_reg("rxga", (old_val & ~(0xff << 16)) |
                                  (((uint16_t)gain_factor) << 16));
    }

    return RETURN_SUCCESS;
}

static int hdlr_rx_c_dsp_nco_adj(const char *data, char *ret) {
    double freq;
    uint32_t old_val;
    uint8_t direction;

    // check for a minus or plus sign at the front
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

    // write NCO adj
    uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);
    write_hps_reg("rxc0", nco_steps);
    if (direction > 0) {
        sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
    } else {
        sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
    }

    // write direction
    read_hps_reg("rxc4", &old_val);
    write_hps_reg("rxc4", (old_val & ~(0x1 << 13)) | (direction << 13));
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_dsp_rstreq(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rxc4", &old_val);
    write_hps_reg("rxc4", old_val | 0x2);
    write_hps_reg("rxc4", old_val & ~0x2);
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_dsp_loopback(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rxc4", &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("rxc4", (old_val & ~0x1e00) | 0x400);
    else
        write_hps_reg("rxc4", (old_val & ~0x1e00) | 0x000);
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_about_id(const char *data, char *ret) {
    // don't need to do anything, save the ID in the file system
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_link_vita_en(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rxc4", &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("rxc4", old_val | (1 << 14));
    else
        write_hps_reg("rxc4", old_val & ~(1 << 14));

    // sync_channels( 15 );

    return RETURN_SUCCESS;
}

static int hdlr_rx_c_link_iface(const char *data, char *ret) {
    // TODO: FW support for streaming to management port required
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_link_port(const char *data, char *ret) {
    uint32_t port;
    sscanf(data, "%" SCNd32 "", &port);
    write_hps_reg("rxc8", port);
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_link_ip_dest(const char *data, char *ret) {
    uint8_t ip[4];
    sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
           ip + 2, ip + 3);
    write_hps_reg("rxc5",
                  (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_link_mac_dest(const char *data, char *ret) {
    uint8_t mac[6];
    sscanf(data,
           "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
           mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
    write_hps_reg("rxc6", (mac[0] << 8) | (mac[1]));
    write_hps_reg("rxc7",
                  (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
    return RETURN_SUCCESS;
}

static int hdlr_rx_c_stream(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t stream;
    sscanf(data, "%" SCNd8 "", &stream);

    // if stream > 1, check the status of the stream
    if (stream > 1) {
        sprintf(ret, "%u", rx_stream[2]); // Alert File Tree
        return RETURN_SUCCESS;
    }

    // Stream is already ON or OFF then return
    if (stream == rx_stream[2])
        return RETURN_SUCCESS;

    // Otherwise make the change accordingly
    if (stream > 0) { // TURN THE STREAM ON
        if (rx_power[2] == PWR_ON) {
            read_hps_reg(reg4[2], &old_val);
            write_hps_reg(reg4[2], old_val | 0x100);

            read_hps_reg(reg4[2], &old_val);
            write_hps_reg(reg4[2], old_val | 0x2);
            write_hps_reg(reg4[2], old_val & (~0x2));

            rx_stream[2] = STREAM_ON;
        } else {
            // Do not turn ON stream if channel is OFF
            sprintf(ret, "%u", 0); // Alert File Tree
        }
    } else { // TURN THE STREAM OFF
        // disable DSP core
        read_hps_reg("rxc4", &old_val);
        write_hps_reg("rxc4", old_val | 0x2);

        // disable channel
        read_hps_reg("rxc4", &old_val);
        write_hps_reg("rxc4", old_val & (~0x100));

        rx_stream[2] = STREAM_OFF;
    }

    return RETURN_SUCCESS;
}

static int hdlr_rx_c_pwr(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t power;
    uint8_t i;
    sscanf(data, "%" SCNd8 "", &power);

    // check it power is already enabled
    if (power >= PWR_ON && rx_power[2] == PWR_ON)
        return RETURN_SUCCESS;

    // power on
    if (power >= PWR_ON) {
        rx_power[2] = PWR_ON;

        // board commands
        strcpy(buf, "board -c c -d\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
        usleep(200000);

        // disable dsp channels
        for (i = 0; i < (NUM_CHANNELS * 2); i++) {
            read_hps_reg(reg4[i], &old_val);
            write_hps_reg(reg4[i], old_val & ~0x100);
        }

        // send sync pulse
        sync_channels(15);

        // Enable active dsp channels, and reset DSP
        for (i = 0; i < NUM_CHANNELS; i++) {
            if (tx_power[i] == PWR_ON) {
                read_hps_reg(reg4[i + 4], &old_val);
                write_hps_reg(reg4[i + 4], old_val | 0x100);
                read_hps_reg(reg4[i + 4], &old_val);
                PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + i);
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

        // power off
    } else {
        rx_power[2] = PWR_OFF;
        rx_stream[2] = STREAM_OFF;

        // kill the channel
        strcpy(buf, "board -c c -k\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

        // disable the DSP cores
        read_hps_reg("rxc4", &old_val);
        write_hps_reg("rxc4", old_val | 0x2);

        // disable 10G transmission
        read_hps_reg("rxc4", &old_val);
        write_hps_reg("rxc4", old_val & (~0x100));
    }
    return RETURN_SUCCESS;
}
static int hdlr_tx_d_rf_dac_dither_en(const char *data, char *ret) {
    int r;
    int en;

    r = sscanf(data, "%d", &en);
    if (1 != r) {
        return RETURN_ERROR;
    }
    if (en < 0 || en > 1) {
        return RETURN_ERROR_PARAM;
    }
    snprintf(buf, sizeof(buf), "dac -c d -l %u\r", en);
    sprintf(ret, "%u", en);
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_dac_dither_mixer_en(const char *data, char *ret) {
    int r;
    int en;

    r = sscanf(data, "%d", &en);
    if (1 != r) {
        return RETURN_ERROR;
    }
    if (en < 0 || en > 1) {
        return RETURN_ERROR_PARAM;
    }
    snprintf(buf, sizeof(buf), "dac -c d -3 %u\r", en);
    sprintf(ret, "%u", en);
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_dac_dither_sra_sel(const char *data, char *ret) {
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

    // 96 dB (max) := 0x0, 90 dB := 0x1, ... 6 dB (min) := 0xf
    sel = 16 - (db / 6);

    snprintf(buf, sizeof(buf), "dac -c d -b %u\r", sel);
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_dac_nco(const char *data, char *ret) {
    double freq;
    sscanf(data, "%lf", &freq);
    uint64_t nco_steps = (uint64_t)round(freq * DAC_NCO_CONST);
    sprintf(ret, "%lf", (double)nco_steps / DAC_NCO_CONST);

    strcpy(buf, "dac -c d -e 1 -n ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    strcpy(buf, "dac -o ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_dac_temp(const char *data, char *ret) {
    strcpy(buf, "board -c d -t\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_freq_val(const char *data, char *ret) {
    uint64_t freq = 0;
    sscanf(data, "%" SCNd64 "", &freq);

    // if freq = 0, mute PLL
    if (freq == 0) {
        strcpy(buf, "rf -c d -z\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

        return RETURN_SUCCESS;
    }

    // if freq out of bounds, kill channel
    if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
        strcpy(buf, "board -c d -k\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

        // Turn OFF TXD on HPS
        uint32_t old_val;

        // disable the DSP cores
        read_hps_reg("txd4", &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + 3);
        write_hps_reg("txd4", old_val | 0x2);

        // disable 10G transmission
        read_hps_reg("txd4", &old_val);
        write_hps_reg("txd4", old_val & (~0x100));

        tx_power[3] = PWR_OFF;

        PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting "
                     "Down TXD.\n");

        return RETURN_ERROR;
    }

    // run the pll calc algorithm
    pllparam_t pll;
    long double outfreq = 0;
    outfreq = setFreq(&freq, &pll);

    strcpy(buf, "rf -c d \r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    // TODO: pll1.power setting TBD (need to modify pllparam_t)

    // Send Parameters over to the MCU
    set_pll_frequency(uart_tx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                      3);

    sprintf(ret, "%lf", outfreq);

    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_freq_band(const char *data, char *ret) {
    strcpy(buf, "rf -c d -b ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_freq_i_bias(const char *data, char *ret) {
    sscanf(data, "%i", &(i_bias[3]));
    strcpy(buf, "rf -c d -i ");
    sprintf(buf + strlen(buf), "%i", i_bias[3]);
    strcat(buf, " -q ");
    sprintf(buf + strlen(buf), "%i", q_bias[3]);
    strcat(buf, " -m\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_freq_q_bias(const char *data, char *ret) {
    sscanf(data, "%i", &(q_bias[3]));
    strcpy(buf, "rf -c d -i ");
    sprintf(buf + strlen(buf), "%i", i_bias[3]);
    strcat(buf, " -q ");
    sprintf(buf + strlen(buf), "%i", q_bias[3]);
    strcat(buf, " -m\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_gain_val(const char *data, char *ret) {
    int gain;
    sscanf(data, "%i", &gain);

    // 0   -> 126	attenuation only
    // 127		0dB

    if (gain > 127)
        gain = 127;
    else if (gain < 0)
        gain = 0;

    strcpy(buf, "rf -c d -a ");
    sprintf(buf + strlen(buf), "%i", 127 - gain);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_board_dump(const char *data, char *ret) {
    // send the uart commands and read back the output and write to file

    // DAC
    strcpy(buf, "dump -c d -d\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    PRINT(DUMP, "[Board: tx_d Chip: DAC] %s\n", uart_ret_buf);

    // GPIOX
    strcpy(buf, "dump -c d -g\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    PRINT(DUMP, "[Board: tx_d Chip: GPIOX] %s\n", uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_board_test(const char *data, char *ret) {
    // TODO: MCU code cleanup
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -c d -t\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_d_status_rfld(const char *data, char *ret) {
    strcpy(buf, "status -c d -l\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_d_status_dacld(const char *data, char *ret) {
    strcpy(buf, "status -c d -p\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_d_status_dacctr(const char *data, char *ret) {
    strcpy(buf, "status -c d -e\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_tx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_board_led(const char *data, char *ret) {
    strcpy(buf, "board -l\r");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_dsp_gain(const char *data, char *ret) {
    // TODO: FW code
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_dsp_rate(const char *data, char *ret) {
    uint32_t old_val;
    uint16_t base_factor, resamp_factor;
    double base_err, resamp_err;
    double rate;
    sscanf(data, "%lf", &rate);

    // get the error for base rate
    base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
    resamp_factor =
        get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

    // set the appropriate sample rate
    memset(ret, 0, MAX_PROP_LEN);

    if (resamp_err < base_err) {
        write_hps_reg("txd1", resamp_factor);
        read_hps_reg("txd4", &old_val);
        write_hps_reg("txd4", old_val | (1 << 15));
        sprintf(ret, "%lf", RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));
        // Set gain adjustment
        read_hps_reg("txga", &old_val);
        write_hps_reg("txga", (old_val & ~(0xff << 24)) |
                                  (interp_gain_lut[(resamp_factor)] << 24));
    } else {
        write_hps_reg("txd1", base_factor);
        read_hps_reg("txd4", &old_val);
        write_hps_reg("txd4", old_val & ~(1 << 15));
        sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1));
        // Set gain adjustment
        read_hps_reg("txga", &old_val);
        write_hps_reg("txga", (old_val & ~(0xff << 24)) |
                                  (interp_gain_lut[(base_factor)] << 24));
    }

    return RETURN_SUCCESS;
}

static int hdlr_tx_d_dsp_nco_adj(const char *data, char *ret) {
    double freq;
    uint32_t old_val;
    uint8_t direction;

    // check for a minus or plus sign at the front
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

    // write NCO adj
    uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);
    write_hps_reg("txd0", nco_steps);
    if (direction > 0) {
        sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
    } else {
        sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
    }

    // write direction
    read_hps_reg("txd4", &old_val);
    write_hps_reg("txd4", (old_val & ~(0x1 << 13)) | (direction << 13));
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_dsp_rstreq(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("txd4", &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + 3);
    write_hps_reg("txd4", old_val | 0x2);
    write_hps_reg("txd4", old_val & ~0x2);
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_about_id(const char *data, char *ret) {
    // don't need to do anything, save the ID in the file system
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_link_vita_en(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("txd4", &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("txd4", old_val | (1 << 14));
    else
        write_hps_reg("txd4", old_val & ~(1 << 14));

    // sync_channels( 15 );

    return RETURN_SUCCESS;
}

static int hdlr_tx_d_link_iface(const char *data, char *ret) {
    // TODO: FW support for streaming to management port required
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_link_port(const char *data, char *ret) {
    uint32_t port;
    sscanf(data, "%" SCNd32 "", &port);
    write_hps_reg("txd5", port);
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_qa_fifo_lvl(const char *data, char *ret) {
    uint32_t lvl;
    read_hps_reg("res_ro7", &lvl);
    lvl &= 0xffff;
    sprintf(ret, "%u", lvl);
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_qa_oflow(const char *data, char *ret) {
    uint32_t count;
    // this is technically a 64-bit register, but we currently only need the
    // bottom 32-bits
    read_hps_reg("flc20", &count);
    sprintf(ret, "%u", count);
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_qa_uflow(const char *data, char *ret) {
    uint32_t count;
    // this is technically a 64-bit register, but we currently only need the
    // bottom 32-bits
    read_hps_reg("flc12", &count);
    sprintf(ret, "%u", count);
    return RETURN_SUCCESS;
}

static int hdlr_tx_d_pwr(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t power;
    uint8_t i;
    sscanf(data, "%" SCNd8 "", &power);

    // check it power is already enabled
    if (power >= PWR_ON && tx_power[3] == PWR_ON)
        return RETURN_SUCCESS;

    // power on
    if (power >= PWR_ON) {
        tx_power[3] = PWR_ON;

        // board commands
        strcpy(buf, "board -c d -d\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));
        usleep(200000);

        // disable dsp channels
        for (i = 0; i < (NUM_CHANNELS * 2); i++) {
            read_hps_reg(reg4[i], &old_val);
            write_hps_reg(reg4[i], old_val & ~0x100);
        }

        // send sync pulse
        sync_channels(15);

        // enable active dsp channels, and reset the DSP
        for (i = 0; i < NUM_CHANNELS; i++) {
            if (tx_power[i] == PWR_ON) {
                read_hps_reg(reg4[i + 4], &old_val);
                write_hps_reg(reg4[i + 4], old_val | 0x100);
                read_hps_reg(reg4[i + 4], &old_val);
                PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + i);
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

        // power off
    } else {
        // kill the channel
        strcpy(buf, "board -c d -k\r");
        send_uart_comm(uart_tx_fd, (uint8_t *)buf, strlen(buf));

        // disable the DSP cores
        read_hps_reg("txd4", &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + 3);
        write_hps_reg("txd4", old_val | 0x2);

        // disable 10G transmission
        read_hps_reg("txd4", &old_val);
        write_hps_reg("txd4", old_val & (~0x100));

        tx_power[3] = PWR_OFF;
    }
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_freq_val(const char *data, char *ret) {
    uint64_t freq = 0;
    sscanf(data, "%" SCNd64 "", &freq);

    // if freq = 0, mute PLL
    if (freq == 0) {
        strcpy(buf, "rf -c d -z\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

        return RETURN_SUCCESS;
    }

    // if freq out of bounds, kill channel
    if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
        strcpy(buf, "board -c d -k\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

        // Turn OFF RXD on HPS
        uint32_t old_val;

        // disable the DSP cores
        read_hps_reg("rxd4", &old_val);
        write_hps_reg("rxd4", old_val | 0x2);

        // disable 10G transmission
        read_hps_reg("rxd4", &old_val);
        write_hps_reg("rxd4", old_val & (~0x100));

        rx_power[3] = PWR_OFF;
        rx_stream[3] = STREAM_OFF;

        PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting "
                     "Down RXD.\n");

        return RETURN_ERROR;
    }

    // run the pll calc algorithm
    pllparam_t pll;
    long double outfreq = 0;
    outfreq = setFreq(&freq, &pll);

    strcpy(buf, "rf -c d \r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

    // TODO: pll1.power setting TBD (need to modify pllparam_t)

    // Send Parameters over to the MCU
    set_pll_frequency(uart_rx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                      3);

    sprintf(ret, "%lf", outfreq);

    return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_freq_lna(const char *data, char *ret) {
    strcpy(buf, "rf -c d -l ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_freq_band(const char *data, char *ret) {
    strcpy(buf, "rf -c d -b ");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_gain_val(const char *data, char *ret) {
    int gain;
    sscanf(data, "%i", &gain);

    if (gain > 126)
        gain = 126;
    else if (gain < 0)
        gain = 0;

    if (gain % 2)
        gain++; // Odd Number

    // 0 -> 126 gain
    strcpy(buf, "vga -c d -g ");
    sprintf(buf + strlen(buf), "%i", gain >> 1);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_atten_val(const char *data, char *ret) {
    int atten;
    sscanf(data, "%i", &atten);

    if (atten > 127)
        atten = 127;
    else if (atten < 0)
        atten = 0;

    strcpy(buf, "rf -c d -a ");
    sprintf(buf + strlen(buf), "%i", atten);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

    return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_board_dump(const char *data, char *ret) {
    // send the uart commands and read back the output and write to file

    // ADC
    strcpy(buf, "dump -c d -a\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    PRINT(DUMP, "[Board: rx_d Chip: ADC] %s\n", uart_ret_buf);

    // GPIOX
    strcpy(buf, "dump -c d -g\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    PRINT(DUMP, "[Board: rx_d Chip: GPIOX] %s\n", uart_ret_buf);

    // ADC Driver
    strcpy(buf, "dump -c d -v\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    PRINT(DUMP, "[Board: rx_d Chip: ADC Driver] %s\n", uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_board_test(const char *data, char *ret) {
    // TODO: MCU code cleanup
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_board_temp(const char *data, char *ret) {
    strcpy(buf, "board -c d -t\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_d_status_rfld(const char *data, char *ret) {
    strcpy(buf, "status -c d -l\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_d_status_adcalarm(const char *data, char *ret) {
    strcpy(buf, "status -c d -a\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    read_uart(uart_rx_fd);
    strcpy(ret, (char *)uart_ret_buf);

    return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_board_led(const char *data, char *ret) {
    strcpy(buf, "board -l\r");
    strcat(buf, data);
    strcat(buf, "\r");
    send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_dsp_signed(const char *data, char *ret) {
    uint32_t old_val, sign;
    sscanf(data, "%u", &sign);
    sign = sign ? 0 : 1;

    read_hps_reg("rxd4", &old_val);
    old_val &= ~(1 << 4);
    write_hps_reg("rxd4", old_val | (sign << 4));
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_dsp_gain(const char *data, char *ret) {
    // TODO: FW code
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_dsp_rate(const char *data, char *ret) {
    uint32_t old_val;
    uint16_t base_factor, resamp_factor;
    double base_err, resamp_err;
    double rate;
    sscanf(data, "%lf", &rate);

    // get the error for base rate
    base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
    resamp_factor =
        get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

    // set the appropriate sample rate
    memset(ret, 0, MAX_PROP_LEN);
    int gain_factor;

    if (resamp_err < base_err) {
        write_hps_reg("rxd1", resamp_factor);
        read_hps_reg("rxd4", &old_val);
        write_hps_reg("rxd4", old_val | (1 << 15));
        sprintf(ret, "%lf", RESAMP_SAMPLE_RATE / (double)(resamp_factor + 1));
        // Set gain adjustment
        gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
        read_hps_reg("rxga", &old_val);
        write_hps_reg("rxga", (old_val & ~(0xff << 24)) |
                                  (((uint16_t)gain_factor) << 24));
    } else {
        write_hps_reg("rxd1", base_factor);
        read_hps_reg("rxd4", &old_val);
        write_hps_reg("rxd4", old_val & ~(1 << 15));
        sprintf(ret, "%lf", BASE_SAMPLE_RATE / (double)(base_factor + 1));
        // Set gain adjustment
        gain_factor = decim_gain_lut[(base_factor)];
        read_hps_reg("rxga", &old_val);
        write_hps_reg("rxga", (old_val & ~(0xff << 24)) |
                                  (((uint16_t)gain_factor) << 24));
    }

    return RETURN_SUCCESS;
}

static int hdlr_rx_d_dsp_nco_adj(const char *data, char *ret) {
    double freq;
    uint32_t old_val;
    uint8_t direction;

    // check for a minus or plus sign at the front
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

    // write NCO adj
    uint32_t nco_steps = (uint32_t)round(freq * DSP_NCO_CONST);
    write_hps_reg("rxd0", nco_steps);
    if (direction > 0) {
        sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
    } else {
        sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
    }

    // write direction
    read_hps_reg("rxd4", &old_val);
    write_hps_reg("rxd4", (old_val & ~(0x1 << 13)) | (direction << 13));
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_dsp_rstreq(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rxd4", &old_val);
    write_hps_reg("rxd4", old_val | 0x2);
    write_hps_reg("rxd4", old_val & ~0x2);
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_dsp_loopback(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rxd4", &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("rxd4", (old_val & ~0x1e00) | 0x400);
    else
        write_hps_reg("rxd4", (old_val & ~0x1e00) | 0x200);
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_about_id(const char *data, char *ret) {
    // don't need to do anything, save the ID in the file system
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_link_vita_en(const char *data, char *ret) {
    uint32_t old_val;
    read_hps_reg("rxd4", &old_val);
    if (strcmp(data, "1") == 0)
        write_hps_reg("rxd4", old_val | (1 << 14));
    else
        write_hps_reg("rxd4", old_val & ~(1 << 14));

    // sync_channels( 15 );

    return RETURN_SUCCESS;
}

static int hdlr_rx_d_link_iface(const char *data, char *ret) {
    // TODO: FW support for streaming to management port required
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_link_port(const char *data, char *ret) {
    uint32_t port;
    sscanf(data, "%" SCNd32 "", &port);
    write_hps_reg("rxd8", port);
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_link_ip_dest(const char *data, char *ret) {
    uint8_t ip[4];
    sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
           ip + 2, ip + 3);
    write_hps_reg("rxd5",
                  (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_link_mac_dest(const char *data, char *ret) {
    uint8_t mac[6];
    sscanf(data,
           "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
           mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
    write_hps_reg("rxd6", (mac[0] << 8) | (mac[1]));
    write_hps_reg("rxd7",
                  (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
    return RETURN_SUCCESS;
}

static int hdlr_rx_d_stream(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t stream;
    sscanf(data, "%" SCNd8 "", &stream);

    // if stream > 1, check the status of the stream
    if (stream > 1) {
        sprintf(ret, "%u", rx_stream[3]); // Alert File Tree
        return RETURN_SUCCESS;
    }

    // Stream is already ON or OFF then return
    if (stream == rx_stream[3])
        return RETURN_SUCCESS;

    // Otherwise make the change accordingly
    if (stream > 0) { // TURN THE STREAM ON
        if (rx_power[3] == PWR_ON) {
            read_hps_reg(reg4[3], &old_val);
            write_hps_reg(reg4[3], old_val | 0x100);

            read_hps_reg(reg4[3], &old_val);
            write_hps_reg(reg4[3], old_val | 0x2);
            write_hps_reg(reg4[3], old_val & (~0x2));

            rx_stream[3] = STREAM_ON;
        } else {
            // Do not turn ON stream if channel is OFF
            sprintf(ret, "%u", 0); // Alert File Tree
        }
    } else { // TURN THE STREAM OFF
        // disable DSP core
        read_hps_reg("rxd4", &old_val);
        write_hps_reg("rxd4", old_val | 0x2);

        // disable channel
        read_hps_reg("rxd4", &old_val);
        write_hps_reg("rxd4", old_val & (~0x100));

        rx_stream[3] = STREAM_OFF;
    }

    return RETURN_SUCCESS;
}

static int hdlr_rx_d_pwr(const char *data, char *ret) {
    uint32_t old_val;
    uint8_t power;
    uint8_t i;
    sscanf(data, "%" SCNd8 "", &power);

    // check it power is already enabled
    if (power >= PWR_ON && rx_power[3] == PWR_ON)
        return RETURN_SUCCESS;

    // power on
    if (power >= PWR_ON) {
        rx_power[3] = PWR_ON;

        // board commands
        strcpy(buf, "board -c d -d\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));
        usleep(200000);

        // disable dsp channels
        for (i = 0; i < (NUM_CHANNELS * 2); i++) {
            read_hps_reg(reg4[i], &old_val);
            write_hps_reg(reg4[i], old_val & ~0x100);
        }

        // send sync pulse
        sync_channels(15);

        // Enable active dsp channels, and reset DSP
        for (i = 0; i < NUM_CHANNELS; i++) {
            if (tx_power[i] == PWR_ON) {
                read_hps_reg(reg4[i + 4], &old_val);
                write_hps_reg(reg4[i + 4], old_val | 0x100);
                read_hps_reg(reg4[i + 4], &old_val);
                PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, 'A' + i);
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

        // power off
    } else {
        rx_power[3] = PWR_OFF;
        rx_stream[3] = STREAM_OFF;

        // kill the channel
        strcpy(buf, "board -c d -k\r");
        send_uart_comm(uart_rx_fd, (uint8_t *)buf, strlen(buf));

        // disable the DSP cores
        read_hps_reg("rxd4", &old_val);
        write_hps_reg("rxd4", old_val | 0x2);

        // disable 10G transmission
        read_hps_reg("rxd4", &old_val);
        write_hps_reg("rxd4", old_val & (~0x100));
    }
    return RETURN_SUCCESS;
}
