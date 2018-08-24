# 1 "properties.c"
# 1 "<built-in>"
# 1 "<command-line>"
# 31 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 32 "<command-line>" 2
# 1 "properties.c"
# 100 "properties.c"
static const char *const names[] = {

    "a"
    "b"
    "c"
    "d"
    "e"
    "f"
    "g"
    "h"
    "i"
    "j"
    "k"
    "l"
    "m"
    "n"
    "o"
    "p"

};
# 126 "properties.c"
static int *uart_tx_fd = NULL;
static int *uart_rx_fd = NULL;
# 144 "properties.c"
static int uart_synth_fd = 0;

static uint8_t uart_ret_buf[MAX_UART_RET_LEN] = {0x00};
static char buf[MAX_PROP_LEN] = {'\0'};

static uint8_t rx_power[] = {

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

};

static uint8_t tx_power[] = {

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

};

static uint8_t rx_stream[] = {

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

};

static const char *reg4[] = {

    "rx"
    "a"
    "4",
    "rx"
    "b"
    "4",
    "rx"
    "c"
    "4",
    "rx"
    "d"
    "4",
    "rx"
    "e"
    "4",
    "rx"
    "f"
    "4",
    "rx"
    "g"
    "4",
    "rx"
    "h"
    "4",
    "rx"
    "i"
    "4",
    "rx"
    "j"
    "4",
    "rx"
    "k"
    "4",
    "rx"
    "l"
    "4",
    "rx"
    "m"
    "4",
    "rx"
    "n"
    "4",
    "rx"
    "o"
    "4",
    "rx"
    "p"
    "4",

    "tx"
    "a"
    "4",
    "tx"
    "b"
    "4",
    "tx"
    "c"
    "4",
    "tx"
    "d"
    "4",
    "tx"
    "e"
    "4",
    "tx"
    "f"
    "4",
    "tx"
    "g"
    "4",
    "tx"
    "h"
    "4",
    "tx"
    "i"
    "4",
    "tx"
    "j"
    "4",
    "tx"
    "k"
    "4",
    "tx"
    "l"
    "4",
    "tx"
    "m"
    "4",
    "tx"
    "n"
    "4",
    "tx"
    "o"
    "4",
    "tx"
    "p"
    "4",

};

static int i_bias[] = {

    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,

};

static int q_bias[] = {

    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,

};

uint8_t *_save_profile;
uint8_t *_load_profile;
char *_save_profile_path;
char *_load_profile_path;

static const uint8_t ipver[] = {
    0,
    0,
};

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

static uint16_t get_optimal_sr_factor(double rate, double base_rate,
                                      double *err) {
  double max_factor = 65536;
  double min_factor = 1;
  double lower_factor_violation = 0;
  double upper_factor_violation = 0;

  double lower_factor = (double)floor(base_rate / rate);
  double upper_factor = lower_factor + 1;

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
  } else {
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
# 310 "properties.c"
static int hdlr_invalid(const char *data, char *ret) {
  PRINT(ERROR, "Cannot invoke a set on this property\n");
  return RETURN_ERROR_SET_PROP;
}

static int hdlr_rx_sync(const char *data, char *ret) {
  uint32_t old_val;

  read_hps_reg("sys0", &old_val);
  write_hps_reg("sys0", old_val | 0x20);
  write_hps_reg("sys0", old_val & (~0x20));

  return RETURN_SUCCESS;
}

static int hdlr_tx_sync(const char *data, char *ret) {
  uint32_t old_val;

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
  return ({
    int _r;
    uint32_t _t;
    _r = read_hps_reg("sys2", &_t);
    if (RETURN_SUCCESS != _r) {
      PRINT(ERROR, "read_hps_reg( '%s' ) failed: %d\n", "sys2", _r);
    } else {
      _t &= ~(1 << 4);
      _t |= in << 4;
      _r = write_hps_reg("sys2", _t);
      if (RETURN_SUCCESS != _r) {
        PRINT(ERROR, "write_hps_reg( '%s' ) failed: %d\n", "sys2", _r);
      }
    }
    _r;
  });
}

static int set_sma_pol(bool positive) {
  return ({
    int _r;
    uint32_t _t;
    _r = read_hps_reg("sys2", &_t);
    if (RETURN_SUCCESS != _r) {
      PRINT(ERROR, "read_hps_reg( '%s' ) failed: %d\n", "sys2", _r);
    } else {
      _t &= ~(1 << 6);
      _t |= positive << 6;
      _r = write_hps_reg("sys2", _t);
      if (RETURN_SUCCESS != _r) {
        PRINT(ERROR, "write_hps_reg( '%s' ) failed: %d\n", "sys2", _r);
      }
    }
    _r;
  });
}

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
# 391 "properties.c"
static int hdlr_rx_a_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("a"[0] - 'a')));
}
static int hdlr_tx_a_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("a"[0] - 'a')));
}
static int hdlr_rx_b_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("b"[0] - 'a')));
}
static int hdlr_tx_b_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("b"[0] - 'a')));
}
static int hdlr_rx_c_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("c"[0] - 'a')));
}
static int hdlr_tx_c_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("c"[0] - 'a')));
}
static int hdlr_rx_d_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("d"[0] - 'a')));
}
static int hdlr_tx_d_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("d"[0] - 'a')));
}
static int hdlr_rx_e_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("e"[0] - 'a')));
}
static int hdlr_tx_e_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("e"[0] - 'a')));
}
static int hdlr_rx_f_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("f"[0] - 'a')));
}
static int hdlr_tx_f_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("f"[0] - 'a')));
}
static int hdlr_rx_g_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("g"[0] - 'a')));
}
static int hdlr_tx_g_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("g"[0] - 'a')));
}
static int hdlr_rx_h_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("h"[0] - 'a')));
}
static int hdlr_tx_h_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("h"[0] - 'a')));
}
static int hdlr_rx_i_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("i"[0] - 'a')));
}
static int hdlr_tx_i_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("i"[0] - 'a')));
}
static int hdlr_rx_j_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("j"[0] - 'a')));
}
static int hdlr_tx_j_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("j"[0] - 'a')));
}
static int hdlr_rx_k_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("k"[0] - 'a')));
}
static int hdlr_tx_k_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("k"[0] - 'a')));
}
static int hdlr_rx_l_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("l"[0] - 'a')));
}
static int hdlr_tx_l_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("l"[0] - 'a')));
}
static int hdlr_rx_m_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("m"[0] - 'a')));
}
static int hdlr_tx_m_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("m"[0] - 'a')));
}
static int hdlr_rx_n_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("n"[0] - 'a')));
}
static int hdlr_tx_n_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("n"[0] - 'a')));
}
static int hdlr_rx_o_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("o"[0] - 'a')));
}
static int hdlr_tx_o_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("o"[0] - 'a')));
}
static int hdlr_rx_p_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, false, ((int)("p"[0] - 'a')));
}
static int hdlr_tx_p_rf_freq_lut_en(const char *data, char *ret) {
  return hdlr_XX_X_rf_freq_lut_en(data, ret, true, ((int)("p"[0] - 'a')));
}

static int set_gating_mode(const char *chan, bool dsp) {
  char reg_name[8];
  snprintf(reg_name, sizeof(reg_name), "tx%s6", chan);
  return ({
    int _r;
    uint32_t _t;
    _r = read_hps_reg(reg_name, &_t);
    if (RETURN_SUCCESS != _r) {
      PRINT(ERROR, "read_hps_reg( '%s' ) failed: %d\n", reg_name, _r);
    } else {
      _t &= ~(1 << 12);
      _t |= dsp << 12;
      _r = write_hps_reg(reg_name, _t);
      if (RETURN_SUCCESS != _r) {
        PRINT(ERROR, "write_hps_reg( '%s' ) failed: %d\n", reg_name, _r);
      }
    }
    _r;
  });
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
  return ({
    int _r;
    uint32_t _t;
    _r = read_hps_reg(regname, &_t);
    if (RETURN_SUCCESS != _r) {
      PRINT(ERROR, "read_hps_reg( '%s' ) failed: %d\n", regname, _r);
    } else {
      _t &= ~(-1 << 0);
      _t |= backoff << 0;
      _r = write_hps_reg(regname, _t);
      if (RETURN_SUCCESS != _r) {
        PRINT(ERROR, "write_hps_reg( '%s' ) failed: %d\n", regname, _r);
      }
    }
    _r;
  });
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

  return ({
           int _r;
           uint32_t _t;
           _r = read_hps_reg(regname_msw, &_t);
           if (RETURN_SUCCESS != _r) {
             PRINT(ERROR, "read_hps_reg( '%s' ) failed: %d\n", regname_msw, _r);
           } else {
             _t &= ~(-1 << 0);
             _t |= val_msw << 0;
             _r = write_hps_reg(regname_msw, _t);
             if (RETURN_SUCCESS != _r) {
               PRINT(ERROR, "write_hps_reg( '%s' ) failed: %d\n", regname_msw,
                     _r);
             }
           }
           _r;
         }) ||
         ({
           int _r;
           uint32_t _t;
           _r = read_hps_reg(regname_lsw, &_t);
           if (RETURN_SUCCESS != _r) {
             PRINT(ERROR, "read_hps_reg( '%s' ) failed: %d\n", regname_lsw, _r);
           } else {
             _t &= ~(-1 << 0);
             _t |= val_lsw << 0;
             _r = write_hps_reg(regname_lsw, _t);
             if (RETURN_SUCCESS != _r) {
               PRINT(ERROR, "write_hps_reg( '%s' ) failed: %d\n", regname_lsw,
                     _r);
             }
           }
           _r;
         });
}

static int set_trigger_ufl_dir(bool tx, const char *chan, bool in) {
  char reg_name[8];
  snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan,
           tx ? 6 : 9);
  return ({
    int _r;
    uint32_t _t;
    _r = read_hps_reg(reg_name, &_t);
    if (RETURN_SUCCESS != _r) {
      PRINT(ERROR, "read_hps_reg( '%s' ) failed: %d\n", reg_name, _r);
    } else {
      _t &= ~(1 << 9);
      _t |= in << 9;
      _r = write_hps_reg(reg_name, _t);
      if (RETURN_SUCCESS != _r) {
        PRINT(ERROR, "write_hps_reg( '%s' ) failed: %d\n", reg_name, _r);
      }
    }
    _r;
  });
}

static int set_trigger_sel(bool tx, const char *chan, uint32_t sel) {
  char reg_name[8];
  snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan,
           tx ? 6 : 9);
  return ({
    int _r;
    uint32_t _t;
    _r = read_hps_reg(reg_name, &_t);
    if (RETURN_SUCCESS != _r) {
      PRINT(ERROR, "read_hps_reg( '%s' ) failed: %d\n", reg_name, _r);
    } else {
      _t &= ~(0b11 << 10);
      _t |= sel << 10;
      _r = write_hps_reg(reg_name, _t);
      if (RETURN_SUCCESS != _r) {
        PRINT(ERROR, "write_hps_reg( '%s' ) failed: %d\n", reg_name, _r);
      }
    }
    _r;
  });
}

static int set_trigger_mode(bool sma, bool tx, const char *chan, bool edge) {
  unsigned shift;
  char reg_name[8];
  snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan,
           tx ? 6 : 9);
  shift = sma ? 0 : 4;
  return ({
    int _r;
    uint32_t _t;
    _r = read_hps_reg(reg_name, &_t);
    if (RETURN_SUCCESS != _r) {
      PRINT(ERROR, "read_hps_reg( '%s' ) failed: %d\n", reg_name, _r);
    } else {
      _t &= ~(1 << shift);
      _t |= edge << shift;
      _r = write_hps_reg(reg_name, _t);
      if (RETURN_SUCCESS != _r) {
        PRINT(ERROR, "write_hps_reg( '%s' ) failed: %d\n", reg_name, _r);
      }
    }
    _r;
  });
}

static int set_trigger_ufl_pol(bool tx, const char *chan, bool positive) {
  char reg_name[8];
  snprintf(reg_name, sizeof(reg_name), "%s%s%u", tx ? "tx" : "rx", chan,
           tx ? 6 : 9);
  return ({
    int _r;
    uint32_t _t;
    _r = read_hps_reg(reg_name, &_t);
    if (RETURN_SUCCESS != _r) {
      PRINT(ERROR, "read_hps_reg( '%s' ) failed: %d\n", reg_name, _r);
    } else {
      _t &= ~(1 << 8);
      _t |= positive << 8;
      _r = write_hps_reg(reg_name, _t);
      if (RETURN_SUCCESS != _r) {
        PRINT(ERROR, "write_hps_reg( '%s' ) failed: %d\n", reg_name, _r);
      }
    }
    _r;
  });
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
# 659 "properties.c"
static int hdlr_tx_a_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "a", val);
  return r;
}
static int hdlr_tx_a_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "a", val);
  return r;
}
static int hdlr_tx_a_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "a", val);
  return r;
}
static int hdlr_tx_a_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "a", val);
  return r;
}
static int hdlr_tx_a_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "a", val);
  return r;
}
static int hdlr_tx_a_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "a", val);
  return r;
}
static int hdlr_tx_a_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "a", val);
  return r;
}
static int hdlr_rx_a_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "a", val);
  return r;
}
static int hdlr_rx_a_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "a", val);
  return r;
}
static int hdlr_rx_a_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "a", val);
  return r;
}
static int hdlr_rx_a_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "a", val);
  return r;
}
static int hdlr_rx_a_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "a", val);
  return r;
}
static int hdlr_rx_a_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "a", val);
  return r;
}
static int hdlr_rx_a_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "a", val);
  return r;
}
static int hdlr_tx_b_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "b", val);
  return r;
}
static int hdlr_tx_b_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "b", val);
  return r;
}
static int hdlr_tx_b_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "b", val);
  return r;
}
static int hdlr_tx_b_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "b", val);
  return r;
}
static int hdlr_tx_b_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "b", val);
  return r;
}
static int hdlr_tx_b_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "b", val);
  return r;
}
static int hdlr_tx_b_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "b", val);
  return r;
}
static int hdlr_rx_b_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "b", val);
  return r;
}
static int hdlr_rx_b_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "b", val);
  return r;
}
static int hdlr_rx_b_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "b", val);
  return r;
}
static int hdlr_rx_b_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "b", val);
  return r;
}
static int hdlr_rx_b_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "b", val);
  return r;
}
static int hdlr_rx_b_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "b", val);
  return r;
}
static int hdlr_rx_b_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "b", val);
  return r;
}
static int hdlr_tx_c_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "c", val);
  return r;
}
static int hdlr_tx_c_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "c", val);
  return r;
}
static int hdlr_tx_c_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "c", val);
  return r;
}
static int hdlr_tx_c_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "c", val);
  return r;
}
static int hdlr_tx_c_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "c", val);
  return r;
}
static int hdlr_tx_c_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "c", val);
  return r;
}
static int hdlr_tx_c_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "c", val);
  return r;
}
static int hdlr_rx_c_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "c", val);
  return r;
}
static int hdlr_rx_c_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "c", val);
  return r;
}
static int hdlr_rx_c_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "c", val);
  return r;
}
static int hdlr_rx_c_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "c", val);
  return r;
}
static int hdlr_rx_c_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "c", val);
  return r;
}
static int hdlr_rx_c_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "c", val);
  return r;
}
static int hdlr_rx_c_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "c", val);
  return r;
}
static int hdlr_tx_d_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "d", val);
  return r;
}
static int hdlr_tx_d_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "d", val);
  return r;
}
static int hdlr_tx_d_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "d", val);
  return r;
}
static int hdlr_tx_d_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "d", val);
  return r;
}
static int hdlr_tx_d_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "d", val);
  return r;
}
static int hdlr_tx_d_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "d", val);
  return r;
}
static int hdlr_tx_d_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "d", val);
  return r;
}
static int hdlr_rx_d_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "d", val);
  return r;
}
static int hdlr_rx_d_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "d", val);
  return r;
}
static int hdlr_rx_d_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "d", val);
  return r;
}
static int hdlr_rx_d_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "d", val);
  return r;
}
static int hdlr_rx_d_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "d", val);
  return r;
}
static int hdlr_rx_d_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "d", val);
  return r;
}
static int hdlr_rx_d_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "d", val);
  return r;
}
static int hdlr_tx_e_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "e", val);
  return r;
}
static int hdlr_tx_e_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "e", val);
  return r;
}
static int hdlr_tx_e_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "e", val);
  return r;
}
static int hdlr_tx_e_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "e", val);
  return r;
}
static int hdlr_tx_e_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "e", val);
  return r;
}
static int hdlr_tx_e_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "e", val);
  return r;
}
static int hdlr_tx_e_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "e", val);
  return r;
}
static int hdlr_rx_e_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "e", val);
  return r;
}
static int hdlr_rx_e_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "e", val);
  return r;
}
static int hdlr_rx_e_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "e", val);
  return r;
}
static int hdlr_rx_e_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "e", val);
  return r;
}
static int hdlr_rx_e_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "e", val);
  return r;
}
static int hdlr_rx_e_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "e", val);
  return r;
}
static int hdlr_rx_e_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "e", val);
  return r;
}
static int hdlr_tx_f_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "f", val);
  return r;
}
static int hdlr_tx_f_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "f", val);
  return r;
}
static int hdlr_tx_f_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "f", val);
  return r;
}
static int hdlr_tx_f_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "f", val);
  return r;
}
static int hdlr_tx_f_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "f", val);
  return r;
}
static int hdlr_tx_f_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "f", val);
  return r;
}
static int hdlr_tx_f_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "f", val);
  return r;
}
static int hdlr_rx_f_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "f", val);
  return r;
}
static int hdlr_rx_f_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "f", val);
  return r;
}
static int hdlr_rx_f_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "f", val);
  return r;
}
static int hdlr_rx_f_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "f", val);
  return r;
}
static int hdlr_rx_f_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "f", val);
  return r;
}
static int hdlr_rx_f_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "f", val);
  return r;
}
static int hdlr_rx_f_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "f", val);
  return r;
}
static int hdlr_tx_g_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "g", val);
  return r;
}
static int hdlr_tx_g_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "g", val);
  return r;
}
static int hdlr_tx_g_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "g", val);
  return r;
}
static int hdlr_tx_g_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "g", val);
  return r;
}
static int hdlr_tx_g_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "g", val);
  return r;
}
static int hdlr_tx_g_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "g", val);
  return r;
}
static int hdlr_tx_g_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "g", val);
  return r;
}
static int hdlr_rx_g_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "g", val);
  return r;
}
static int hdlr_rx_g_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "g", val);
  return r;
}
static int hdlr_rx_g_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "g", val);
  return r;
}
static int hdlr_rx_g_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "g", val);
  return r;
}
static int hdlr_rx_g_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "g", val);
  return r;
}
static int hdlr_rx_g_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "g", val);
  return r;
}
static int hdlr_rx_g_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "g", val);
  return r;
}
static int hdlr_tx_h_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "h", val);
  return r;
}
static int hdlr_tx_h_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "h", val);
  return r;
}
static int hdlr_tx_h_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "h", val);
  return r;
}
static int hdlr_tx_h_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "h", val);
  return r;
}
static int hdlr_tx_h_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "h", val);
  return r;
}
static int hdlr_tx_h_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "h", val);
  return r;
}
static int hdlr_tx_h_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "h", val);
  return r;
}
static int hdlr_rx_h_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "h", val);
  return r;
}
static int hdlr_rx_h_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "h", val);
  return r;
}
static int hdlr_rx_h_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "h", val);
  return r;
}
static int hdlr_rx_h_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "h", val);
  return r;
}
static int hdlr_rx_h_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "h", val);
  return r;
}
static int hdlr_rx_h_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "h", val);
  return r;
}
static int hdlr_rx_h_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "h", val);
  return r;
}
static int hdlr_tx_i_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "i", val);
  return r;
}
static int hdlr_tx_i_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "i", val);
  return r;
}
static int hdlr_tx_i_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "i", val);
  return r;
}
static int hdlr_tx_i_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "i", val);
  return r;
}
static int hdlr_tx_i_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "i", val);
  return r;
}
static int hdlr_tx_i_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "i", val);
  return r;
}
static int hdlr_tx_i_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "i", val);
  return r;
}
static int hdlr_rx_i_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "i", val);
  return r;
}
static int hdlr_rx_i_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "i", val);
  return r;
}
static int hdlr_rx_i_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "i", val);
  return r;
}
static int hdlr_rx_i_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "i", val);
  return r;
}
static int hdlr_rx_i_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "i", val);
  return r;
}
static int hdlr_rx_i_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "i", val);
  return r;
}
static int hdlr_rx_i_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "i", val);
  return r;
}
static int hdlr_tx_j_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "j", val);
  return r;
}
static int hdlr_tx_j_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "j", val);
  return r;
}
static int hdlr_tx_j_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "j", val);
  return r;
}
static int hdlr_tx_j_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "j", val);
  return r;
}
static int hdlr_tx_j_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "j", val);
  return r;
}
static int hdlr_tx_j_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "j", val);
  return r;
}
static int hdlr_tx_j_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "j", val);
  return r;
}
static int hdlr_rx_j_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "j", val);
  return r;
}
static int hdlr_rx_j_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "j", val);
  return r;
}
static int hdlr_rx_j_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "j", val);
  return r;
}
static int hdlr_rx_j_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "j", val);
  return r;
}
static int hdlr_rx_j_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "j", val);
  return r;
}
static int hdlr_rx_j_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "j", val);
  return r;
}
static int hdlr_rx_j_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "j", val);
  return r;
}
static int hdlr_tx_k_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "k", val);
  return r;
}
static int hdlr_tx_k_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "k", val);
  return r;
}
static int hdlr_tx_k_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "k", val);
  return r;
}
static int hdlr_tx_k_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "k", val);
  return r;
}
static int hdlr_tx_k_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "k", val);
  return r;
}
static int hdlr_tx_k_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "k", val);
  return r;
}
static int hdlr_tx_k_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "k", val);
  return r;
}
static int hdlr_rx_k_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "k", val);
  return r;
}
static int hdlr_rx_k_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "k", val);
  return r;
}
static int hdlr_rx_k_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "k", val);
  return r;
}
static int hdlr_rx_k_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "k", val);
  return r;
}
static int hdlr_rx_k_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "k", val);
  return r;
}
static int hdlr_rx_k_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "k", val);
  return r;
}
static int hdlr_rx_k_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "k", val);
  return r;
}
static int hdlr_tx_l_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "l", val);
  return r;
}
static int hdlr_tx_l_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "l", val);
  return r;
}
static int hdlr_tx_l_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "l", val);
  return r;
}
static int hdlr_tx_l_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "l", val);
  return r;
}
static int hdlr_tx_l_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "l", val);
  return r;
}
static int hdlr_tx_l_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "l", val);
  return r;
}
static int hdlr_tx_l_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "l", val);
  return r;
}
static int hdlr_rx_l_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "l", val);
  return r;
}
static int hdlr_rx_l_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "l", val);
  return r;
}
static int hdlr_rx_l_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "l", val);
  return r;
}
static int hdlr_rx_l_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "l", val);
  return r;
}
static int hdlr_rx_l_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "l", val);
  return r;
}
static int hdlr_rx_l_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "l", val);
  return r;
}
static int hdlr_rx_l_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "l", val);
  return r;
}
static int hdlr_tx_m_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "m", val);
  return r;
}
static int hdlr_tx_m_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "m", val);
  return r;
}
static int hdlr_tx_m_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "m", val);
  return r;
}
static int hdlr_tx_m_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "m", val);
  return r;
}
static int hdlr_tx_m_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "m", val);
  return r;
}
static int hdlr_tx_m_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "m", val);
  return r;
}
static int hdlr_tx_m_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "m", val);
  return r;
}
static int hdlr_rx_m_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "m", val);
  return r;
}
static int hdlr_rx_m_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "m", val);
  return r;
}
static int hdlr_rx_m_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "m", val);
  return r;
}
static int hdlr_rx_m_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "m", val);
  return r;
}
static int hdlr_rx_m_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "m", val);
  return r;
}
static int hdlr_rx_m_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "m", val);
  return r;
}
static int hdlr_rx_m_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "m", val);
  return r;
}
static int hdlr_tx_n_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "n", val);
  return r;
}
static int hdlr_tx_n_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "n", val);
  return r;
}
static int hdlr_tx_n_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "n", val);
  return r;
}
static int hdlr_tx_n_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "n", val);
  return r;
}
static int hdlr_tx_n_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "n", val);
  return r;
}
static int hdlr_tx_n_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "n", val);
  return r;
}
static int hdlr_tx_n_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "n", val);
  return r;
}
static int hdlr_rx_n_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "n", val);
  return r;
}
static int hdlr_rx_n_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "n", val);
  return r;
}
static int hdlr_rx_n_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "n", val);
  return r;
}
static int hdlr_rx_n_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "n", val);
  return r;
}
static int hdlr_rx_n_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "n", val);
  return r;
}
static int hdlr_rx_n_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "n", val);
  return r;
}
static int hdlr_rx_n_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "n", val);
  return r;
}
static int hdlr_tx_o_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "o", val);
  return r;
}
static int hdlr_tx_o_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "o", val);
  return r;
}
static int hdlr_tx_o_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "o", val);
  return r;
}
static int hdlr_tx_o_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "o", val);
  return r;
}
static int hdlr_tx_o_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "o", val);
  return r;
}
static int hdlr_tx_o_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "o", val);
  return r;
}
static int hdlr_tx_o_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "o", val);
  return r;
}
static int hdlr_rx_o_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "o", val);
  return r;
}
static int hdlr_rx_o_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "o", val);
  return r;
}
static int hdlr_rx_o_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "o", val);
  return r;
}
static int hdlr_rx_o_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "o", val);
  return r;
}
static int hdlr_rx_o_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "o", val);
  return r;
}
static int hdlr_rx_o_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "o", val);
  return r;
}
static int hdlr_rx_o_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "o", val);
  return r;
}
static int hdlr_tx_p_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, true, "p", val);
  return r;
}
static int hdlr_tx_p_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(true, "p", val);
  return r;
}
static int hdlr_tx_p_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(true, "p", val);
  return r;
}
static int hdlr_tx_p_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(true, "p", val);
  return r;
}
static int hdlr_tx_p_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(true, "p", val);
  return r;
}
static int hdlr_tx_p_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(false, true, "p", val);
  return r;
}
static int hdlr_tx_p_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(true, "p", val);
  return r;
}
static int hdlr_rx_p_trigger_sma_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) || set_trigger_mode(true, false, "p", val);
  return r;
}
static int hdlr_rx_p_trigger_edge_backoff(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_edge_backoff(data, &val) || set_edge_backoff(false, "p", val);
  return r;
}
static int hdlr_rx_p_trigger_edge_sample_num(const char *data, char *ret) {
  uint64_t val;
  int r;
  r = valid_edge_sample_num(data, &val) || set_edge_sample_num(false, "p", val);
  return r;
}
static int hdlr_rx_p_trigger_trig_sel(const char *data, char *ret) {
  uint32_t val;
  int r;
  r = valid_trigger_sel(data, &val) || set_trigger_sel(false, "p", val);
  return r;
}
static int hdlr_rx_p_trigger_ufl_dir(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(false, "p", val);
  return r;
}
static int hdlr_rx_p_trigger_ufl_mode(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_mode(data, &val) ||
      set_trigger_mode(false, false, "p", val);
  return r;
}
static int hdlr_rx_p_trigger_ufl_pol(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(false, "p", val);
  return r;
}
# 1198 "properties.c"
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
  snprintf(buf, sizeof(buf),
           "dac -c "
           "a"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  snprintf(buf, sizeof(buf),
           "dac -c "
           "a"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  sel = 16 - (db / 6);
  snprintf(buf, sizeof(buf),
           "dac -c "
           "a"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "a"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "a"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "a"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "a"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "a"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("a"[0]));
    write_hps_reg("tx"
                  "a"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "a"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "a"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("a"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "a"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "a"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("a"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("a"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "a"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("a"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "a"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("a"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("a"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("a"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "a"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("a"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("a"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "a"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "a"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("a"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "a"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("a"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "a"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "a"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "a"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "a"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "a"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "a"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "a"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "a",
                 &old_val);
    write_hps_reg("txg"
                  "a",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "a"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "a"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "a"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "a",
                 &old_val);
    write_hps_reg("txg"
                  "a",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "a"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "a"
               "4",
               &old_val);
  write_hps_reg("tx"
                "a"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "a"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("a"[0]));
  write_hps_reg("tx"
                "a"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "a"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "a"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "a"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "a"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "a"
                "5",
                port);
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
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("a"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("a"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "a"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("a"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "a"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "a"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("a"[0]));
    write_hps_reg("tx"
                  "a"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "a"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "a"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("a"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_a_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
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
  snprintf(buf, sizeof(buf),
           "dac -c "
           "b"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "b"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "b"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("b"[0] - 'a'))]);
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
    send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "b"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
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
    read_hps_reg("tx"
                 "b"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "b"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("b"[0] - 'a'))] = 0;
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
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("b"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("b"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "b"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("b"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "b"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("b"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("b"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("b"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "b"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("b"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("b"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "b"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("b"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "b"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("b"[0] - 'a'))]);
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
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "b"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "b"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "b"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
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
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
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
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "b"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
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
  if (power >= 1 && tx_power[((int)("b"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("b"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "b"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("b"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
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
    send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "b"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("b"[0]));
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
    tx_power[((int)("b"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_b_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
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
  snprintf(buf, sizeof(buf),
           "dac -c "
           "c"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  snprintf(buf, sizeof(buf),
           "dac -c "
           "c"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  sel = 16 - (db / 6);
  snprintf(buf, sizeof(buf),
           "dac -c "
           "c"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "c"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "c"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "c"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "c"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "c"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("c"[0]));
    write_hps_reg("tx"
                  "c"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "c"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "c"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("c"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "c"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "c"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("c"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("c"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "c"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("c"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "c"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("c"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("c"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("c"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "c"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("c"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("c"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "c"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "c"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("c"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "c"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("c"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "c"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "c"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "c"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "c"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "c"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "c"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "c"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "c",
                 &old_val);
    write_hps_reg("txg"
                  "c",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "c"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "c"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "c"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "c",
                 &old_val);
    write_hps_reg("txg"
                  "c",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "c"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "c"
               "4",
               &old_val);
  write_hps_reg("tx"
                "c"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "c"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("c"[0]));
  write_hps_reg("tx"
                "c"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "c"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "c"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "c"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "c"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "c"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("c"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("c"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "c"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("c"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "c"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "c"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("c"[0]));
    write_hps_reg("tx"
                  "c"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "c"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "c"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("c"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_c_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
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
  snprintf(buf, sizeof(buf),
           "dac -c "
           "d"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  snprintf(buf, sizeof(buf),
           "dac -c "
           "d"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  sel = 16 - (db / 6);
  snprintf(buf, sizeof(buf),
           "dac -c "
           "d"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "d"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "d"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "d"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "d"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "d"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("d"[0]));
    write_hps_reg("tx"
                  "d"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "d"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "d"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("d"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "d"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "d"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("d"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("d"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "d"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("d"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "d"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("d"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("d"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("d"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "d"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("d"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("d"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "d"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "d"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("d"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "d"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("d"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "d"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "d"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "d"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "d"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "d"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "d"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "d"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "d",
                 &old_val);
    write_hps_reg("txg"
                  "d",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "d"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "d"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "d"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "d",
                 &old_val);
    write_hps_reg("txg"
                  "d",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "d"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "d"
               "4",
               &old_val);
  write_hps_reg("tx"
                "d"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "d"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("d"[0]));
  write_hps_reg("tx"
                "d"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "d"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "d"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "d"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "d"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "d"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("d"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("d"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "d"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("d"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "d"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "d"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("d"[0]));
    write_hps_reg("tx"
                  "d"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "d"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "d"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("d"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_d_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_dac_dither_en(const char *data, char *ret) {
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
           "e"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_dac_dither_mixer_en(const char *data, char *ret) {
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
           "e"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_dac_dither_sra_sel(const char *data, char *ret) {
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
           "e"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "e"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "e"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "e"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "e"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "e"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("e"[0]));
    write_hps_reg("tx"
                  "e"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "e"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "e"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("e"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "e"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "e"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("e"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("e"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "e"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("e"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "e"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("e"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("e"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("e"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "e"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("e"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("e"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "e"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "e"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("e"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "e"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("e"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "e"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "e"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "e"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "e"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "e"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "e"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "e"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "e",
                 &old_val);
    write_hps_reg("txg"
                  "e",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "e"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "e"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "e"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "e",
                 &old_val);
    write_hps_reg("txg"
                  "e",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "e"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "e"
               "4",
               &old_val);
  write_hps_reg("tx"
                "e"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "e"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("e"[0]));
  write_hps_reg("tx"
                "e"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "e"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "e"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "e"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "e"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "e"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("e"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("e"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "e"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("e"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "e"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "e"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("e"[0]));
    write_hps_reg("tx"
                  "e"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "e"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "e"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("e"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_e_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_dac_dither_en(const char *data, char *ret) {
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
           "f"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_dac_dither_mixer_en(const char *data, char *ret) {
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
           "f"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_dac_dither_sra_sel(const char *data, char *ret) {
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
           "f"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "f"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "f"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "f"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "f"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "f"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("f"[0]));
    write_hps_reg("tx"
                  "f"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "f"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "f"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("f"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "f"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "f"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("f"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("f"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "f"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("f"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "f"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("f"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("f"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("f"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "f"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("f"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("f"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "f"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "f"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("f"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "f"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("f"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "f"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "f"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "f"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "f"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "f"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "f"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "f"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "f",
                 &old_val);
    write_hps_reg("txg"
                  "f",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "f"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "f"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "f"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "f",
                 &old_val);
    write_hps_reg("txg"
                  "f",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "f"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "f"
               "4",
               &old_val);
  write_hps_reg("tx"
                "f"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "f"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("f"[0]));
  write_hps_reg("tx"
                "f"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "f"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "f"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "f"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "f"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "f"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("f"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("f"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "f"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("f"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "f"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "f"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("f"[0]));
    write_hps_reg("tx"
                  "f"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "f"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "f"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("f"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_f_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_dac_dither_en(const char *data, char *ret) {
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
           "g"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_dac_dither_mixer_en(const char *data, char *ret) {
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
           "g"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_dac_dither_sra_sel(const char *data, char *ret) {
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
           "g"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "g"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "g"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "g"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "g"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "g"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("g"[0]));
    write_hps_reg("tx"
                  "g"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "g"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "g"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("g"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "g"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "g"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("g"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("g"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "g"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("g"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "g"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("g"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("g"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("g"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "g"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("g"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("g"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "g"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "g"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("g"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "g"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("g"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "g"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "g"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "g"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "g"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "g"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "g"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "g"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "g",
                 &old_val);
    write_hps_reg("txg"
                  "g",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "g"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "g"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "g"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "g",
                 &old_val);
    write_hps_reg("txg"
                  "g",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "g"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "g"
               "4",
               &old_val);
  write_hps_reg("tx"
                "g"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "g"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("g"[0]));
  write_hps_reg("tx"
                "g"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "g"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "g"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "g"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "g"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "g"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("g"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("g"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "g"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("g"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "g"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "g"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("g"[0]));
    write_hps_reg("tx"
                  "g"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "g"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "g"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("g"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_g_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_dac_dither_en(const char *data, char *ret) {
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
           "h"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_dac_dither_mixer_en(const char *data, char *ret) {
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
           "h"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_dac_dither_sra_sel(const char *data, char *ret) {
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
           "h"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "h"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "h"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "h"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "h"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "h"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("h"[0]));
    write_hps_reg("tx"
                  "h"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "h"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "h"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("h"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "h"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "h"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("h"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("h"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "h"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("h"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "h"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("h"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("h"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("h"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "h"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("h"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("h"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "h"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "h"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("h"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "h"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("h"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "h"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "h"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "h"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "h"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "h"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "h"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "h"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "h",
                 &old_val);
    write_hps_reg("txg"
                  "h",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "h"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "h"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "h"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "h",
                 &old_val);
    write_hps_reg("txg"
                  "h",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "h"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "h"
               "4",
               &old_val);
  write_hps_reg("tx"
                "h"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "h"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("h"[0]));
  write_hps_reg("tx"
                "h"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "h"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "h"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "h"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "h"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "h"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("h"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("h"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "h"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("h"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "h"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "h"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("h"[0]));
    write_hps_reg("tx"
                  "h"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "h"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "h"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("h"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_h_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_dac_dither_en(const char *data, char *ret) {
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
           "i"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_dac_dither_mixer_en(const char *data, char *ret) {
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
           "i"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_dac_dither_sra_sel(const char *data, char *ret) {
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
           "i"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "i"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "i"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "i"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "i"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "i"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("i"[0]));
    write_hps_reg("tx"
                  "i"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "i"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "i"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("i"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "i"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "i"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("i"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("i"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "i"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("i"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "i"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("i"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("i"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("i"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "i"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("i"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("i"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "i"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "i"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("i"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "i"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("i"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "i"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "i"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "i"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "i"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "i"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "i"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "i"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "i",
                 &old_val);
    write_hps_reg("txg"
                  "i",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "i"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "i"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "i"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "i",
                 &old_val);
    write_hps_reg("txg"
                  "i",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "i"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "i"
               "4",
               &old_val);
  write_hps_reg("tx"
                "i"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "i"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("i"[0]));
  write_hps_reg("tx"
                "i"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "i"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "i"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "i"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "i"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "i"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("i"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("i"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "i"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("i"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "i"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "i"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("i"[0]));
    write_hps_reg("tx"
                  "i"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "i"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "i"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("i"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_i_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_dac_dither_en(const char *data, char *ret) {
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
           "j"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_dac_dither_mixer_en(const char *data, char *ret) {
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
           "j"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_dac_dither_sra_sel(const char *data, char *ret) {
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
           "j"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "j"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "j"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "j"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "j"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "j"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("j"[0]));
    write_hps_reg("tx"
                  "j"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "j"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "j"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("j"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "j"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "j"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("j"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("j"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "j"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("j"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "j"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("j"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("j"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("j"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "j"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("j"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("j"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "j"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "j"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("j"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "j"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("j"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "j"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "j"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "j"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "j"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "j"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "j"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "j"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "j",
                 &old_val);
    write_hps_reg("txg"
                  "j",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "j"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "j"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "j"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "j",
                 &old_val);
    write_hps_reg("txg"
                  "j",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "j"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "j"
               "4",
               &old_val);
  write_hps_reg("tx"
                "j"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "j"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("j"[0]));
  write_hps_reg("tx"
                "j"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "j"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "j"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "j"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "j"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "j"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("j"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("j"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "j"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("j"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "j"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "j"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("j"[0]));
    write_hps_reg("tx"
                  "j"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "j"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "j"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("j"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_j_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_dac_dither_en(const char *data, char *ret) {
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
           "k"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_dac_dither_mixer_en(const char *data, char *ret) {
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
           "k"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_dac_dither_sra_sel(const char *data, char *ret) {
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
           "k"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "k"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "k"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "k"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "k"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "k"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("k"[0]));
    write_hps_reg("tx"
                  "k"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "k"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "k"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("k"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "k"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "k"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("k"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("k"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "k"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("k"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "k"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("k"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("k"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("k"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "k"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("k"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("k"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "k"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "k"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("k"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "k"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("k"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "k"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "k"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "k"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "k"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "k"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "k"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "k"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "k",
                 &old_val);
    write_hps_reg("txg"
                  "k",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "k"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "k"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "k"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "k",
                 &old_val);
    write_hps_reg("txg"
                  "k",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "k"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "k"
               "4",
               &old_val);
  write_hps_reg("tx"
                "k"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "k"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("k"[0]));
  write_hps_reg("tx"
                "k"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "k"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "k"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "k"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "k"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "k"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("k"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("k"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "k"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("k"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "k"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "k"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("k"[0]));
    write_hps_reg("tx"
                  "k"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "k"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "k"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("k"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_k_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_dac_dither_en(const char *data, char *ret) {
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
           "l"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_dac_dither_mixer_en(const char *data, char *ret) {
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
           "l"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_dac_dither_sra_sel(const char *data, char *ret) {
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
           "l"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "l"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "l"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "l"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "l"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "l"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("l"[0]));
    write_hps_reg("tx"
                  "l"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "l"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "l"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("l"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "l"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "l"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("l"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("l"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "l"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("l"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "l"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("l"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("l"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("l"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "l"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("l"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("l"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "l"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "l"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("l"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "l"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("l"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "l"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "l"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "l"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "l"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "l"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "l"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "l"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "l",
                 &old_val);
    write_hps_reg("txg"
                  "l",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "l"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "l"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "l"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "l",
                 &old_val);
    write_hps_reg("txg"
                  "l",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "l"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "l"
               "4",
               &old_val);
  write_hps_reg("tx"
                "l"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "l"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("l"[0]));
  write_hps_reg("tx"
                "l"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "l"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "l"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "l"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "l"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "l"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("l"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("l"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "l"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("l"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "l"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "l"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("l"[0]));
    write_hps_reg("tx"
                  "l"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "l"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "l"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("l"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_l_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_dac_dither_en(const char *data, char *ret) {
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
           "m"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_dac_dither_mixer_en(const char *data, char *ret) {
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
           "m"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_dac_dither_sra_sel(const char *data, char *ret) {
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
           "m"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "m"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "m"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "m"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "m"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "m"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("m"[0]));
    write_hps_reg("tx"
                  "m"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "m"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "m"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("m"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "m"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "m"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("m"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("m"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "m"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("m"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "m"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("m"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("m"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("m"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "m"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("m"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("m"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "m"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "m"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("m"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "m"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("m"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "m"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "m"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "m"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "m"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "m"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "m"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "m"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "m",
                 &old_val);
    write_hps_reg("txg"
                  "m",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "m"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "m"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "m"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "m",
                 &old_val);
    write_hps_reg("txg"
                  "m",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "m"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "m"
               "4",
               &old_val);
  write_hps_reg("tx"
                "m"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "m"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("m"[0]));
  write_hps_reg("tx"
                "m"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "m"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "m"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "m"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "m"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "m"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("m"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("m"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "m"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("m"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "m"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "m"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("m"[0]));
    write_hps_reg("tx"
                  "m"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "m"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "m"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("m"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_m_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_dac_dither_en(const char *data, char *ret) {
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
           "n"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_dac_dither_mixer_en(const char *data, char *ret) {
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
           "n"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_dac_dither_sra_sel(const char *data, char *ret) {
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
           "n"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "n"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "n"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "n"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "n"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "n"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("n"[0]));
    write_hps_reg("tx"
                  "n"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "n"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "n"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("n"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "n"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "n"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("n"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("n"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "n"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("n"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "n"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("n"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("n"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("n"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "n"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("n"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("n"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "n"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "n"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("n"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "n"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("n"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "n"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "n"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "n"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "n"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "n"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "n"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "n"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "n",
                 &old_val);
    write_hps_reg("txg"
                  "n",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "n"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "n"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "n"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "n",
                 &old_val);
    write_hps_reg("txg"
                  "n",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "n"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "n"
               "4",
               &old_val);
  write_hps_reg("tx"
                "n"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "n"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("n"[0]));
  write_hps_reg("tx"
                "n"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "n"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "n"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "n"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "n"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "n"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("n"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("n"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "n"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("n"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "n"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "n"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("n"[0]));
    write_hps_reg("tx"
                  "n"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "n"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "n"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("n"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_n_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_dac_dither_en(const char *data, char *ret) {
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
           "o"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_dac_dither_mixer_en(const char *data, char *ret) {
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
           "o"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_dac_dither_sra_sel(const char *data, char *ret) {
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
           "o"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "o"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "o"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "o"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "o"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "o"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("o"[0]));
    write_hps_reg("tx"
                  "o"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "o"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "o"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("o"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "o"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "o"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("o"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("o"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "o"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("o"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "o"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("o"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("o"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("o"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "o"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("o"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("o"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "o"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "o"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("o"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "o"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("o"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "o"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "o"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "o"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "o"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "o"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "o"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "o"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "o",
                 &old_val);
    write_hps_reg("txg"
                  "o",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "o"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "o"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "o"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "o",
                 &old_val);
    write_hps_reg("txg"
                  "o",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "o"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "o"
               "4",
               &old_val);
  write_hps_reg("tx"
                "o"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "o"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("o"[0]));
  write_hps_reg("tx"
                "o"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "o"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "o"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "o"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "o"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "o"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("o"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("o"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "o"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("o"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "o"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "o"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("o"[0]));
    write_hps_reg("tx"
                  "o"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "o"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "o"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("o"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_o_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_dac_dither_en(const char *data, char *ret) {
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
           "p"
           " -l %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_dac_dither_mixer_en(const char *data, char *ret) {
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
           "p"
           " -3 %u\r",
           en);
  sprintf(ret, "%u", en);
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_dac_dither_sra_sel(const char *data, char *ret) {
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
           "p"
           " -b %u\r",
           sel);
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_dac_nco(const char *data, char *ret) {
  double freq;
  sscanf(data, "%lf", &freq);
  uint64_t nco_steps = (uint64_t)round(
      freq *
      ((double)216519.21285435076923076923076923076923076923076923076919296));
  sprintf(
      ret, "%lf",
      (double)nco_steps /
          ((double)216519.21285435076923076923076923076923076923076923076919296));
  strcpy(buf, "dac -c "
              "p"
              " -e 0 -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  strcpy(buf, "dac -o ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_dac_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "p"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "p"
                " -z\r");
    send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "p"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("tx"
                 "p"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("p"[0]));
    write_hps_reg("tx"
                  "p"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "p"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "p"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("p"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down TX"
                 "p"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "p"
              " \r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_tx_fd[((int)("p"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, true,
                    ((int)("p"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "p"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_freq_i_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(i_bias[((int)("p"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "p"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("p"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("p"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_freq_q_bias(const char *data, char *ret) {
  sscanf(data, "%i", &(q_bias[((int)("p"[0] - 'a'))]));
  strcpy(buf, "rf -c "
              "p"
              " -i ");
  sprintf(buf + strlen(buf), "%i", i_bias[((int)("p"[0] - 'a'))]);
  strcat(buf, " -q ");
  sprintf(buf + strlen(buf), "%i", q_bias[((int)("p"[0] - 'a'))]);
  strcat(buf, " -m\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 127)
    gain = 127;
  else if (gain < 0)
    gain = 0;
  strcpy(buf, "rf -c "
              "p"
              " -a ");
  sprintf(buf + strlen(buf), "%i", 127 - gain);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "p"
              " -d\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("p"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "p"
              " -g\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("p"[0] - 'a'))]);
  PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "p"
              " -t\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "p"
              " -l\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_status_dacld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "p"
              " -p\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_status_dacctr(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "p"
              " -e\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  if (resamp_err < base_err) {
    write_hps_reg("tx"
                  "p"
                  "1",
                  resamp_factor);
    read_hps_reg("tx"
                 "p"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "p"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    read_hps_reg("txg"
                 "p",
                 &old_val);
    write_hps_reg("txg"
                  "p",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(resamp_factor)] << 0));
  } else {
    write_hps_reg("tx"
                  "p"
                  "1",
                  base_factor);
    read_hps_reg("tx"
                 "p"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "p"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    read_hps_reg("txg"
                 "p",
                 &old_val);
    write_hps_reg("txg"
                  "p",
                  (old_val & ~(0xff << 0)) |
                      (interp_gain_lut[(base_factor)] << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("tx"
                "p"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("tx"
               "p"
               "4",
               &old_val);
  write_hps_reg("tx"
                "p"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "p"
               "4",
               &old_val);
  PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("p"[0]));
  write_hps_reg("tx"
                "p"
                "4",
                old_val | 0x2);
  write_hps_reg("tx"
                "p"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("tx"
               "p"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("tx"
                  "p"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("tx"
                  "p"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("tx"
                "p"
                "5",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_qa_fifo_lvl(const char *data, char *ret) {
  uint32_t lvl;
  read_hps_reg("res_ro4", &lvl);
  lvl &= 0xffff;
  sprintf(ret, "%u", lvl);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_qa_oflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc14", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_qa_uflow(const char *data, char *ret) {
  uint32_t count;
  read_hps_reg("flc6", &count);
  sprintf(ret, "%u", count);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && tx_power[((int)("p"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    tx_power[((int)("p"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "p"
                " -d\r");
    send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("p"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_power[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    strcpy(buf, "board -c "
                "p"
                " -k\r");
    send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("tx"
                 "p"
                 "4",
                 &old_val);
    PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("p"[0]));
    write_hps_reg("tx"
                  "p"
                  "4",
                  old_val | 0x2);
    read_hps_reg("tx"
                 "p"
                 "4",
                 &old_val);
    write_hps_reg("tx"
                  "p"
                  "4",
                  old_val & (~0x100));
    tx_power[((int)("p"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_tx_p_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_tx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
# 1699 "properties.c"
static int hdlr_rx_a_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "a"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "a"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "a"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "a"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "a"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "a"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("a"[0] - 'a'))] = 0;
    rx_stream[((int)("a"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "a"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "a"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("a"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("a"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "a"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "a"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
    gain++;
  strcpy(buf, "vga -c "
              "a"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "a"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "a"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("a"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "a"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("a"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "a"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("a"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "a"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "a"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "a"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "a"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "a"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "a"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "a"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "a"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "a",
                 &old_val);
    write_hps_reg("rxg"
                  "a",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "a"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "a"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "a"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "a",
                 &old_val);
    write_hps_reg("rxg"
                  "a",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "a"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "a"
               "4",
               &old_val);
  write_hps_reg("rx"
                "a"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "a"
               "4",
               &old_val);
  write_hps_reg("rx"
                "a"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "a"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "a"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "a"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "a"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "a"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "a"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "a"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "a"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "a"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "a"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "a"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("a"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("a"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("a"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("a"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("a"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("a"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("a"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("a"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("a"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "a"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "a"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "a"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "a"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("a"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("a"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("a"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "a"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("a"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("a"[0] - 'a'))] = 0;
    rx_stream[((int)("a"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "a"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "a"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "a"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "a"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "a"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_a_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("a"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "b"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "b"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
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
    rx_power[((int)("b"[0] - 'a'))] = 0;
    rx_stream[((int)("b"[0] - 'a'))] = 0;
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
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("b"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("b"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "b"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "b"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "b"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("b"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "b"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("b"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "b"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("b"[0] - 'a'))]);
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
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_b_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "b"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_b_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "b"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_b_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
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
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "b",
                 &old_val);
    write_hps_reg("rxg"
                  "b",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
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
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "b",
                 &old_val);
    write_hps_reg("rxg"
                  "b",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "b"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
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
    sprintf(ret, "%u", rx_stream[((int)("b"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("b"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("b"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("b"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("b"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("b"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("b"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("b"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("b"[0] - 'a'))] = 1;
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
    rx_stream[((int)("b"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_b_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("b"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("b"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "b"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("b"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("b"[0] - 'a'))] = 0;
    rx_stream[((int)("b"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "b"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
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
static int hdlr_rx_b_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_b_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_b_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_b_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_b_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("b"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "c"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "c"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "c"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "c"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "c"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "c"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("c"[0] - 'a'))] = 0;
    rx_stream[((int)("c"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "c"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "c"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("c"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("c"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "c"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "c"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
    gain++;
  strcpy(buf, "vga -c "
              "c"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "c"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "c"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("c"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "c"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("c"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "c"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("c"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "c"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "c"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "c"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "c"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "c"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "c"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "c"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "c"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "c",
                 &old_val);
    write_hps_reg("rxg"
                  "c",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "c"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "c"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "c"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "c",
                 &old_val);
    write_hps_reg("rxg"
                  "c",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "c"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "c"
               "4",
               &old_val);
  write_hps_reg("rx"
                "c"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "c"
               "4",
               &old_val);
  write_hps_reg("rx"
                "c"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "c"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "c"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "c"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "c"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "c"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "c"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "c"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "c"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "c"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "c"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "c"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("c"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("c"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("c"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("c"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("c"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("c"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("c"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("c"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("c"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "c"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "c"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "c"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "c"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("c"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("c"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("c"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "c"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("c"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("c"[0] - 'a'))] = 0;
    rx_stream[((int)("c"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "c"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "c"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "c"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "c"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "c"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_c_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("c"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "d"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "d"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "d"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "d"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "d"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "d"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("d"[0] - 'a'))] = 0;
    rx_stream[((int)("d"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "d"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "d"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("d"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("d"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "d"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "d"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
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
    gain++;
  strcpy(buf, "vga -c "
              "d"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "d"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "d"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("d"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "d"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("d"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "d"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("d"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "d"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "d"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "d"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "d"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "d"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "d"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "d"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "d"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "d",
                 &old_val);
    write_hps_reg("rxg"
                  "d",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "d"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "d"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "d"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "d",
                 &old_val);
    write_hps_reg("rxg"
                  "d",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "d"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "d"
               "4",
               &old_val);
  write_hps_reg("rx"
                "d"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "d"
               "4",
               &old_val);
  write_hps_reg("rx"
                "d"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "d"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "d"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "d"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "d"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "d"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "d"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "d"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "d"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "d"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "d"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "d"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("d"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("d"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("d"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("d"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("d"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("d"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("d"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("d"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("d"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "d"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "d"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "d"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "d"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("d"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("d"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("d"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "d"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("d"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("d"[0] - 'a'))] = 0;
    rx_stream[((int)("d"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "d"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "d"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "d"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "d"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "d"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_d_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("d"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "e"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "e"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "e"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "e"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "e"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "e"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("e"[0] - 'a'))] = 0;
    rx_stream[((int)("e"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "e"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "e"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("e"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("e"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "e"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "e"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 126)
    gain = 126;
  else if (gain < 0)
    gain = 0;
  if (gain % 2)
    gain++;
  strcpy(buf, "vga -c "
              "e"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "e"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "e"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("e"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "e"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("e"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "e"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("e"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "e"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "e"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "e"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "e"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "e"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "e"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "e"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "e"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "e",
                 &old_val);
    write_hps_reg("rxg"
                  "e",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "e"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "e"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "e"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "e",
                 &old_val);
    write_hps_reg("rxg"
                  "e",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "e"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "e"
               "4",
               &old_val);
  write_hps_reg("rx"
                "e"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "e"
               "4",
               &old_val);
  write_hps_reg("rx"
                "e"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "e"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "e"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "e"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "e"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "e"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "e"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "e"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "e"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "e"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "e"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "e"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("e"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("e"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("e"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("e"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("e"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("e"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("e"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("e"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("e"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "e"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "e"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "e"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "e"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("e"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("e"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("e"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "e"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("e"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("e"[0] - 'a'))] = 0;
    rx_stream[((int)("e"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "e"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "e"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "e"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "e"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "e"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_e_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("e"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "f"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "f"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "f"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "f"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "f"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "f"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("f"[0] - 'a'))] = 0;
    rx_stream[((int)("f"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "f"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "f"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("f"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("f"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "f"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "f"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 126)
    gain = 126;
  else if (gain < 0)
    gain = 0;
  if (gain % 2)
    gain++;
  strcpy(buf, "vga -c "
              "f"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "f"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "f"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("f"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "f"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("f"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "f"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("f"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "f"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "f"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "f"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "f"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "f"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "f"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "f"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "f"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "f",
                 &old_val);
    write_hps_reg("rxg"
                  "f",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "f"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "f"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "f"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "f",
                 &old_val);
    write_hps_reg("rxg"
                  "f",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "f"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "f"
               "4",
               &old_val);
  write_hps_reg("rx"
                "f"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "f"
               "4",
               &old_val);
  write_hps_reg("rx"
                "f"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "f"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "f"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "f"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "f"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "f"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "f"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "f"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "f"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "f"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "f"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "f"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("f"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("f"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("f"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("f"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("f"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("f"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("f"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("f"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("f"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "f"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "f"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "f"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "f"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("f"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("f"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("f"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "f"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("f"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("f"[0] - 'a'))] = 0;
    rx_stream[((int)("f"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "f"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "f"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "f"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "f"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "f"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_f_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("f"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "g"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "g"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "g"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "g"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "g"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "g"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("g"[0] - 'a'))] = 0;
    rx_stream[((int)("g"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "g"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "g"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("g"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("g"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "g"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "g"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 126)
    gain = 126;
  else if (gain < 0)
    gain = 0;
  if (gain % 2)
    gain++;
  strcpy(buf, "vga -c "
              "g"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "g"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "g"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("g"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "g"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("g"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "g"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("g"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "g"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "g"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "g"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "g"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "g"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "g"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "g"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "g"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "g",
                 &old_val);
    write_hps_reg("rxg"
                  "g",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "g"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "g"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "g"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "g",
                 &old_val);
    write_hps_reg("rxg"
                  "g",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "g"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "g"
               "4",
               &old_val);
  write_hps_reg("rx"
                "g"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "g"
               "4",
               &old_val);
  write_hps_reg("rx"
                "g"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "g"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "g"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "g"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "g"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "g"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "g"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "g"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "g"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "g"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "g"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "g"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("g"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("g"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("g"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("g"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("g"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("g"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("g"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("g"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("g"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "g"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "g"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "g"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "g"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("g"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("g"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("g"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "g"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("g"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("g"[0] - 'a'))] = 0;
    rx_stream[((int)("g"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "g"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "g"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "g"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "g"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "g"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_g_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("g"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "h"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "h"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "h"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "h"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "h"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "h"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("h"[0] - 'a'))] = 0;
    rx_stream[((int)("h"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "h"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "h"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("h"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("h"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "h"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "h"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 126)
    gain = 126;
  else if (gain < 0)
    gain = 0;
  if (gain % 2)
    gain++;
  strcpy(buf, "vga -c "
              "h"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "h"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "h"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("h"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "h"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("h"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "h"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("h"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "h"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "h"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "h"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "h"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "h"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "h"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "h"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "h"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "h",
                 &old_val);
    write_hps_reg("rxg"
                  "h",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "h"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "h"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "h"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "h",
                 &old_val);
    write_hps_reg("rxg"
                  "h",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "h"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "h"
               "4",
               &old_val);
  write_hps_reg("rx"
                "h"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "h"
               "4",
               &old_val);
  write_hps_reg("rx"
                "h"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "h"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "h"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "h"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "h"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "h"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "h"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "h"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "h"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "h"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "h"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "h"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("h"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("h"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("h"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("h"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("h"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("h"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("h"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("h"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("h"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "h"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "h"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "h"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "h"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("h"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("h"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("h"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "h"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("h"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("h"[0] - 'a'))] = 0;
    rx_stream[((int)("h"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "h"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "h"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "h"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "h"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "h"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_h_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("h"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "i"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "i"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "i"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "i"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "i"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "i"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("i"[0] - 'a'))] = 0;
    rx_stream[((int)("i"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "i"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "i"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("i"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("i"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "i"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "i"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 126)
    gain = 126;
  else if (gain < 0)
    gain = 0;
  if (gain % 2)
    gain++;
  strcpy(buf, "vga -c "
              "i"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "i"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "i"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("i"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "i"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("i"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "i"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("i"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "i"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "i"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "i"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "i"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "i"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "i"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "i"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "i"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "i",
                 &old_val);
    write_hps_reg("rxg"
                  "i",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "i"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "i"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "i"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "i",
                 &old_val);
    write_hps_reg("rxg"
                  "i",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "i"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "i"
               "4",
               &old_val);
  write_hps_reg("rx"
                "i"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "i"
               "4",
               &old_val);
  write_hps_reg("rx"
                "i"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "i"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "i"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "i"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "i"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "i"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "i"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "i"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "i"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "i"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "i"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "i"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("i"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("i"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("i"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("i"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("i"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("i"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("i"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("i"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("i"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "i"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "i"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "i"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "i"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("i"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("i"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("i"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "i"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("i"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("i"[0] - 'a'))] = 0;
    rx_stream[((int)("i"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "i"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "i"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "i"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "i"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "i"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_i_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("i"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "j"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "j"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "j"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "j"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "j"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "j"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("j"[0] - 'a'))] = 0;
    rx_stream[((int)("j"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "j"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "j"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("j"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("j"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "j"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "j"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 126)
    gain = 126;
  else if (gain < 0)
    gain = 0;
  if (gain % 2)
    gain++;
  strcpy(buf, "vga -c "
              "j"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "j"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "j"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("j"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "j"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("j"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "j"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("j"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "j"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "j"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "j"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "j"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "j"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "j"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "j"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "j"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "j",
                 &old_val);
    write_hps_reg("rxg"
                  "j",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "j"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "j"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "j"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "j",
                 &old_val);
    write_hps_reg("rxg"
                  "j",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "j"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "j"
               "4",
               &old_val);
  write_hps_reg("rx"
                "j"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "j"
               "4",
               &old_val);
  write_hps_reg("rx"
                "j"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "j"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "j"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "j"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "j"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "j"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "j"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "j"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "j"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "j"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "j"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "j"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("j"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("j"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("j"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("j"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("j"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("j"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("j"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("j"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("j"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "j"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "j"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "j"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "j"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("j"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("j"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("j"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "j"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("j"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("j"[0] - 'a'))] = 0;
    rx_stream[((int)("j"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "j"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "j"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "j"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "j"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "j"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_j_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("j"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "k"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "k"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "k"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "k"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "k"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "k"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("k"[0] - 'a'))] = 0;
    rx_stream[((int)("k"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "k"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "k"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("k"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("k"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "k"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "k"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 126)
    gain = 126;
  else if (gain < 0)
    gain = 0;
  if (gain % 2)
    gain++;
  strcpy(buf, "vga -c "
              "k"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "k"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "k"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("k"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "k"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("k"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "k"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("k"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "k"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "k"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "k"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "k"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "k"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "k"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "k"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "k"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "k",
                 &old_val);
    write_hps_reg("rxg"
                  "k",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "k"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "k"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "k"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "k",
                 &old_val);
    write_hps_reg("rxg"
                  "k",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "k"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "k"
               "4",
               &old_val);
  write_hps_reg("rx"
                "k"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "k"
               "4",
               &old_val);
  write_hps_reg("rx"
                "k"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "k"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "k"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "k"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "k"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "k"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "k"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "k"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "k"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "k"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "k"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "k"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("k"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("k"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("k"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("k"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("k"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("k"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("k"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("k"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("k"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "k"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "k"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "k"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "k"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("k"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("k"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("k"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "k"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("k"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("k"[0] - 'a'))] = 0;
    rx_stream[((int)("k"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "k"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "k"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "k"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "k"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "k"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_k_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("k"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "l"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "l"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "l"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "l"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "l"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "l"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("l"[0] - 'a'))] = 0;
    rx_stream[((int)("l"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "l"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "l"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("l"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("l"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "l"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "l"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 126)
    gain = 126;
  else if (gain < 0)
    gain = 0;
  if (gain % 2)
    gain++;
  strcpy(buf, "vga -c "
              "l"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "l"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "l"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("l"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "l"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("l"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "l"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("l"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "l"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "l"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "l"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "l"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "l"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "l"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "l"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "l"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "l",
                 &old_val);
    write_hps_reg("rxg"
                  "l",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "l"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "l"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "l"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "l",
                 &old_val);
    write_hps_reg("rxg"
                  "l",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "l"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "l"
               "4",
               &old_val);
  write_hps_reg("rx"
                "l"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "l"
               "4",
               &old_val);
  write_hps_reg("rx"
                "l"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "l"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "l"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "l"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "l"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "l"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "l"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "l"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "l"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "l"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "l"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "l"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("l"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("l"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("l"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("l"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("l"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("l"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("l"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("l"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("l"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "l"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "l"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "l"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "l"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("l"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("l"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("l"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "l"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("l"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("l"[0] - 'a'))] = 0;
    rx_stream[((int)("l"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "l"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "l"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "l"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "l"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "l"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_l_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("l"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "m"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "m"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "m"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "m"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "m"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "m"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("m"[0] - 'a'))] = 0;
    rx_stream[((int)("m"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "m"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "m"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("m"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("m"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "m"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "m"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 126)
    gain = 126;
  else if (gain < 0)
    gain = 0;
  if (gain % 2)
    gain++;
  strcpy(buf, "vga -c "
              "m"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "m"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "m"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("m"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "m"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("m"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "m"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("m"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "m"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "m"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "m"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "m"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "m"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "m"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "m"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "m"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "m",
                 &old_val);
    write_hps_reg("rxg"
                  "m",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "m"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "m"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "m"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "m",
                 &old_val);
    write_hps_reg("rxg"
                  "m",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "m"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "m"
               "4",
               &old_val);
  write_hps_reg("rx"
                "m"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "m"
               "4",
               &old_val);
  write_hps_reg("rx"
                "m"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "m"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "m"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "m"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "m"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "m"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "m"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "m"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "m"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "m"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "m"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "m"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("m"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("m"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("m"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("m"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("m"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("m"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("m"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("m"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("m"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "m"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "m"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "m"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "m"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("m"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("m"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("m"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "m"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("m"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("m"[0] - 'a'))] = 0;
    rx_stream[((int)("m"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "m"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "m"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "m"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "m"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "m"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_m_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("m"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "n"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "n"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "n"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "n"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "n"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "n"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("n"[0] - 'a'))] = 0;
    rx_stream[((int)("n"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "n"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "n"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("n"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("n"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "n"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "n"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 126)
    gain = 126;
  else if (gain < 0)
    gain = 0;
  if (gain % 2)
    gain++;
  strcpy(buf, "vga -c "
              "n"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "n"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "n"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("n"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "n"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("n"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "n"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("n"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "n"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "n"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "n"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "n"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "n"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "n"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "n"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "n"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "n",
                 &old_val);
    write_hps_reg("rxg"
                  "n",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "n"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "n"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "n"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "n",
                 &old_val);
    write_hps_reg("rxg"
                  "n",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "n"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "n"
               "4",
               &old_val);
  write_hps_reg("rx"
                "n"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "n"
               "4",
               &old_val);
  write_hps_reg("rx"
                "n"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "n"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "n"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "n"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "n"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "n"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "n"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "n"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "n"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "n"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "n"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "n"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("n"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("n"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("n"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("n"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("n"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("n"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("n"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("n"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("n"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "n"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "n"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "n"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "n"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("n"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("n"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("n"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "n"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("n"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("n"[0] - 'a'))] = 0;
    rx_stream[((int)("n"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "n"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "n"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "n"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "n"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "n"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_n_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("n"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "o"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "o"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "o"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "o"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "o"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "o"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("o"[0] - 'a'))] = 0;
    rx_stream[((int)("o"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "o"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "o"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("o"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("o"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "o"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "o"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 126)
    gain = 126;
  else if (gain < 0)
    gain = 0;
  if (gain % 2)
    gain++;
  strcpy(buf, "vga -c "
              "o"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "o"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "o"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("o"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "o"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("o"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "o"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("o"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "o"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "o"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "o"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "o"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "o"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "o"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "o"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "o"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "o",
                 &old_val);
    write_hps_reg("rxg"
                  "o",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "o"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "o"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "o"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "o",
                 &old_val);
    write_hps_reg("rxg"
                  "o",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "o"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "o"
               "4",
               &old_val);
  write_hps_reg("rx"
                "o"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "o"
               "4",
               &old_val);
  write_hps_reg("rx"
                "o"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "o"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "o"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "o"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "o"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "o"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "o"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "o"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "o"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "o"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "o"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "o"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("o"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("o"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("o"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("o"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("o"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("o"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("o"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("o"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("o"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "o"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "o"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "o"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "o"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("o"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("o"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("o"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "o"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("o"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("o"[0] - 'a'))] = 0;
    rx_stream[((int)("o"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "o"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "o"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "o"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "o"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "o"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_o_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("o"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_rf_freq_val(const char *data, char *ret) {
  uint64_t freq = 0;
  sscanf(data, "%" SCNd64 "", &freq);
  if (freq == 0) {
    strcpy(buf, "rf -c "
                "p"
                " -z\r");
    send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    return RETURN_SUCCESS;
  }
  if ((freq < PLL1_RFOUT_MIN_HZ) || (freq > PLL1_RFOUT_MAX_HZ)) {
    strcpy(buf, "board -c "
                "p"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    uint32_t old_val;
    read_hps_reg("rx"
                 "p"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "p"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "p"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "p"
                  "4",
                  old_val & (~0x100));
    rx_power[((int)("p"[0] - 'a'))] = 0;
    rx_stream[((int)("p"[0] - 'a'))] = 0;
    PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: "
                 "Shutting Down RX"
                 "p"
                 ".\n");
    return RETURN_ERROR;
  }
  pllparam_t pll;
  long double outfreq = 0;
  outfreq = setFreq(&freq, &pll);
  strcpy(buf, "rf -c "
              "p"
              " \r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  set_pll_frequency(uart_rx_fd[((int)("p"[0] - 'a'))],
                    (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll, false,
                    ((int)("p"[0] - 'a')));
  sprintf(ret, "%lf", outfreq);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_rf_freq_lna(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "p"
              " -l ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_rf_freq_band(const char *data, char *ret) {
  strcpy(buf, "rf -c "
              "p"
              " -b ");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_rf_gain_val(const char *data, char *ret) {
  int gain;
  sscanf(data, "%i", &gain);
  if (gain > 126)
    gain = 126;
  else if (gain < 0)
    gain = 0;
  if (gain % 2)
    gain++;
  strcpy(buf, "vga -c "
              "p"
              " -g ");
  sprintf(buf + strlen(buf), "%i", gain >> 1);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_rf_atten_val(const char *data, char *ret) {
  int atten;
  sscanf(data, "%i", &atten);
  if (atten > 127)
    atten = 127;
  else if (atten < 0)
    atten = 0;
  strcpy(buf, "rf -c "
              "p"
              " -a ");
  sprintf(buf + strlen(buf), "%i", atten);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_rf_board_dump(const char *data, char *ret) {
  strcpy(buf, "dump -c "
              "p"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("p"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "p"
              " -g\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("p"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);
  strcpy(buf, "dump -c "
              "p"
              " -v\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("p"[0] - 'a'))]);
  PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_rf_board_test(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_rf_board_temp(const char *data, char *ret) {
  strcpy(buf, "board -c "
              "p"
              " -t\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_status_rfld(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "p"
              " -l\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_status_adcalarm(const char *data, char *ret) {
  strcpy(buf, "status -c "
              "p"
              " -a\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_rf_board_led(const char *data, char *ret) {
  strcpy(buf, "board -l\r");
  strcat(buf, data);
  strcat(buf, "\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_dsp_signed(const char *data, char *ret) {
  uint32_t old_val, sign;
  sscanf(data, "%u", &sign);
  sign = sign ? 0 : 1;
  read_hps_reg("rx"
               "p"
               "4",
               &old_val);
  old_val &= ~(1 << 4);
  write_hps_reg("rx"
                "p"
                "4",
                old_val | (sign << 4));
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_dsp_gain(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_dsp_rate(const char *data, char *ret) {
  uint32_t old_val;
  uint16_t base_factor, resamp_factor;
  double base_err, resamp_err;
  double rate;
  sscanf(data, "%lf", &rate);
  base_factor = get_optimal_sr_factor(rate, 325000000.0, &base_err);
  resamp_factor = get_optimal_sr_factor(rate, 260000000.0, &resamp_err);
  memset(ret, 0, MAX_PROP_LEN);
  int gain_factor;
  if (resamp_err < base_err) {
    write_hps_reg("rx"
                  "p"
                  "1",
                  resamp_factor);
    read_hps_reg("rx"
                 "p"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "p"
                  "4",
                  old_val | (1 << 15));
    sprintf(ret, "%lf", 260000000.0 / (double)(resamp_factor + 1));
    gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
    read_hps_reg("rxg"
                 "p",
                 &old_val);
    write_hps_reg("rxg"
                  "p",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  } else {
    write_hps_reg("rx"
                  "p"
                  "1",
                  base_factor);
    read_hps_reg("rx"
                 "p"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "p"
                  "4",
                  old_val & ~(1 << 15));
    sprintf(ret, "%lf", 325000000.0 / (double)(base_factor + 1));
    gain_factor = decim_gain_lut[(base_factor)];
    read_hps_reg("rxg"
                 "p",
                 &old_val);
    write_hps_reg("rxg"
                  "p",
                  (old_val & ~(0xff << 0)) | (((uint16_t)gain_factor) << 0));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_dsp_nco_adj(const char *data, char *ret) {
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
  uint32_t nco_steps = (uint32_t)round(
      freq *
      ((double)13.215283987692307692307692307692307692307692307692307690000));
  write_hps_reg("rx"
                "p"
                "0",
                nco_steps);
  if (direction > 0) {
    sprintf(
        ret, "-%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  } else {
    sprintf(
        ret, "%lf",
        (double)nco_steps /
            ((double)13.215283987692307692307692307692307692307692307692307690000));
  }
  read_hps_reg("rx"
               "p"
               "4",
               &old_val);
  write_hps_reg("rx"
                "p"
                "4",
                (old_val & ~(0x1 << 13)) | (direction << 13));
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_dsp_rstreq(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "p"
               "4",
               &old_val);
  write_hps_reg("rx"
                "p"
                "4",
                old_val | 0x2);
  write_hps_reg("rx"
                "p"
                "4",
                old_val & ~0x2);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_dsp_loopback(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "p"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "p"
                  "4",
                  (old_val & ~0x1e00) | 0x400);
  else
    write_hps_reg("rx"
                  "p"
                  "4",
                  (old_val & ~0x1e00) | 0x000);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_about_id(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_link_vita_en(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("rx"
               "p"
               "4",
               &old_val);
  if (strcmp(data, "1") == 0)
    write_hps_reg("rx"
                  "p"
                  "4",
                  old_val | (1 << 14));
  else
    write_hps_reg("rx"
                  "p"
                  "4",
                  old_val & ~(1 << 14));
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_link_iface(const char *data, char *ret) {
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_link_port(const char *data, char *ret) {
  uint32_t port;
  sscanf(data, "%" SCNd32 "", &port);
  write_hps_reg("rx"
                "p"
                "8",
                port);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_link_ip_dest(const char *data, char *ret) {
  uint8_t ip[4];
  sscanf(data, "%" SCNd8 ".%" SCNd8 ".%" SCNd8 ".%" SCNd8 "", ip, ip + 1,
         ip + 2, ip + 3);
  write_hps_reg("rx"
                "p"
                "5",
                (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_link_mac_dest(const char *data, char *ret) {
  uint8_t mac[6];
  sscanf(data,
         "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 "",
         mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);
  write_hps_reg("rx"
                "p"
                "6",
                (mac[0] << 8) | (mac[1]));
  write_hps_reg("rx"
                "p"
                "7",
                (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_stream(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t stream;
  sscanf(data, "%" SCNd8 "", &stream);
  if (stream > 1) {
    sprintf(ret, "%u", rx_stream[((int)("p"[0] - 'a'))]);
    return RETURN_SUCCESS;
  }
  if (stream == rx_stream[((int)("p"[0] - 'a'))])
    return RETURN_SUCCESS;
  if (stream > 0) {
    if (rx_power[((int)("p"[0] - 'a'))] == 1) {
      read_hps_reg(reg4[((int)("p"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("p"[0] - 'a'))], old_val | 0x100);
      read_hps_reg(reg4[((int)("p"[0] - 'a'))], &old_val);
      write_hps_reg(reg4[((int)("p"[0] - 'a'))], old_val | 0x2);
      write_hps_reg(reg4[((int)("p"[0] - 'a'))], old_val & (~0x2));
      rx_stream[((int)("p"[0] - 'a'))] = 1;
    } else {
      sprintf(ret, "%u", 0);
    }
  } else {
    read_hps_reg("rx"
                 "p"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "p"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "p"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "p"
                  "4",
                  old_val & (~0x100));
    rx_stream[((int)("p"[0] - 'a'))] = 0;
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_pwr(const char *data, char *ret) {
  uint32_t old_val;
  uint8_t power;
  uint8_t i;
  sscanf(data, "%" SCNd8 "", &power);
  if (power >= 1 && rx_power[((int)("p"[0] - 'a'))] == 1)
    return RETURN_SUCCESS;
  if (power >= 1) {
    rx_power[((int)("p"[0] - 'a'))] = 1;
    strcpy(buf, "board -c "
                "p"
                " -d\r");
    send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    usleep(200000);
    for (i = 0; i < (((int)sizeof(names) / sizeof(*names)) * 2); i++) {
      read_hps_reg(reg4[i], &old_val);
      write_hps_reg(reg4[i], old_val & ~0x100);
    }
    sync_channels(15);
    for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {
      if (tx_power[i] == 1) {
        read_hps_reg(reg4[i + 4], &old_val);
        write_hps_reg(reg4[i + 4], old_val | 0x100);
        read_hps_reg(reg4[i + 4], &old_val);
        PRINT(VERBOSE, "%s(): TX[%c] RESET\n", __func__, toupper("p"[0]));
        write_hps_reg(reg4[i + 4], old_val | 0x2);
        write_hps_reg(reg4[i + 4], old_val & (~0x2));
      }
      if (rx_stream[i] == 1) {
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x100);
        read_hps_reg(reg4[i], &old_val);
        write_hps_reg(reg4[i], old_val | 0x2);
        write_hps_reg(reg4[i], old_val & (~0x2));
      }
    }
  } else {
    rx_power[((int)("p"[0] - 'a'))] = 0;
    rx_stream[((int)("p"[0] - 'a'))] = 0;
    strcpy(buf, "board -c "
                "p"
                " -k\r");
    send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf));
    read_hps_reg("rx"
                 "p"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "p"
                  "4",
                  old_val | 0x2);
    read_hps_reg("rx"
                 "p"
                 "4",
                 &old_val);
    write_hps_reg("rx"
                  "p"
                  "4",
                  old_val & (~0x100));
  }
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_about_serial(const char *data, char *ret) {
  strcpy(buf, "status -s\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_about_mcudevid(const char *data, char *ret) {
  strcpy(buf, "status -d\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_about_mcurev(const char *data, char *ret) {
  strcpy(buf, "status -v\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_about_mcufuses(const char *data, char *ret) {
  strcpy(buf, "status -f\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
static int hdlr_rx_p_about_fw_ver(const char *data, char *ret) {
  strcpy(buf, "board -v\r");
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf));
  read_uart(uart_rx_fd[((int)("p"[0] - 'a'))]);
  strcpy(ret, (char *)uart_ret_buf);
  return RETURN_SUCCESS;
}
# 1709 "properties.c"
static int hdlr_tx_a_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("a", val);
  return r;
}
static int hdlr_tx_b_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("b", val);
  return r;
}
static int hdlr_tx_c_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("c", val);
  return r;
}
static int hdlr_tx_d_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("d", val);
  return r;
}
static int hdlr_tx_e_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("e", val);
  return r;
}
static int hdlr_tx_f_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("f", val);
  return r;
}
static int hdlr_tx_g_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("g", val);
  return r;
}
static int hdlr_tx_h_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("h", val);
  return r;
}
static int hdlr_tx_i_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("i", val);
  return r;
}
static int hdlr_tx_j_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("j", val);
  return r;
}
static int hdlr_tx_k_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("k", val);
  return r;
}
static int hdlr_tx_l_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("l", val);
  return r;
}
static int hdlr_tx_m_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("m", val);
  return r;
}
static int hdlr_tx_n_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("n", val);
  return r;
}
static int hdlr_tx_o_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("o", val);
  return r;
}
static int hdlr_tx_p_trigger_gating(const char *data, char *ret) {
  int r;
  bool val;
  r = valid_gating_mode(data, &val) || set_gating_mode("p", val);
  return r;
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
  int (*hdlr)(const char *, char *) = NULL;
  int i;

  int atten = 0;

  mask_rx = cm_chanmask_get("/var/crimson/state/cm/chanmask-rx");

  sscanf(data, "%lf", &atten);

  sprintf(inbuf, "%lf", atten);

  for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {

    if (0 == (mask_rx & (1 << i))) {
      continue;
    }

    if (i == ((int)("a"[0] - 'a')))
      hdlr = hdlr_rx_a_rf_atten_val;
    if (i == ((int)("b"[0] - 'a')))
      hdlr = hdlr_rx_b_rf_atten_val;
    if (i == ((int)("c"[0] - 'a')))
      hdlr = hdlr_rx_c_rf_atten_val;
    if (i == ((int)("d"[0] - 'a')))
      hdlr = hdlr_rx_d_rf_atten_val;
    if (i == ((int)("e"[0] - 'a')))
      hdlr = hdlr_rx_e_rf_atten_val;
    if (i == ((int)("f"[0] - 'a')))
      hdlr = hdlr_rx_f_rf_atten_val;
    if (i == ((int)("g"[0] - 'a')))
      hdlr = hdlr_rx_g_rf_atten_val;
    if (i == ((int)("h"[0] - 'a')))
      hdlr = hdlr_rx_h_rf_atten_val;
    if (i == ((int)("i"[0] - 'a')))
      hdlr = hdlr_rx_i_rf_atten_val;
    if (i == ((int)("j"[0] - 'a')))
      hdlr = hdlr_rx_j_rf_atten_val;
    if (i == ((int)("k"[0] - 'a')))
      hdlr = hdlr_rx_k_rf_atten_val;
    if (i == ((int)("l"[0] - 'a')))
      hdlr = hdlr_rx_l_rf_atten_val;
    if (i == ((int)("m"[0] - 'a')))
      hdlr = hdlr_rx_m_rf_atten_val;
    if (i == ((int)("n"[0] - 'a')))
      hdlr = hdlr_rx_n_rf_atten_val;
    if (i == ((int)("o"[0] - 'a')))
      hdlr = hdlr_rx_o_rf_atten_val;
    if (i == ((int)("p"[0] - 'a')))
      hdlr = hdlr_rx_p_rf_atten_val;

    r = hdlr(inbuf, outbuf);
    if (RETURN_SUCCESS != r) {
      return r;
    }

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

  for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {

    if (0 == (mask_rx & (1 << i))) {
      continue;
    }

    if (i == ((int)("a"[0] - 'a')))
      hdlr = hdlr_rx_a_rf_gain_val;
    if (i == ((int)("b"[0] - 'a')))
      hdlr = hdlr_rx_b_rf_gain_val;
    if (i == ((int)("c"[0] - 'a')))
      hdlr = hdlr_rx_c_rf_gain_val;
    if (i == ((int)("d"[0] - 'a')))
      hdlr = hdlr_rx_d_rf_gain_val;
    if (i == ((int)("e"[0] - 'a')))
      hdlr = hdlr_rx_e_rf_gain_val;
    if (i == ((int)("f"[0] - 'a')))
      hdlr = hdlr_rx_f_rf_gain_val;
    if (i == ((int)("g"[0] - 'a')))
      hdlr = hdlr_rx_g_rf_gain_val;
    if (i == ((int)("h"[0] - 'a')))
      hdlr = hdlr_rx_h_rf_gain_val;
    if (i == ((int)("i"[0] - 'a')))
      hdlr = hdlr_rx_i_rf_gain_val;
    if (i == ((int)("j"[0] - 'a')))
      hdlr = hdlr_rx_j_rf_gain_val;
    if (i == ((int)("k"[0] - 'a')))
      hdlr = hdlr_rx_k_rf_gain_val;
    if (i == ((int)("l"[0] - 'a')))
      hdlr = hdlr_rx_l_rf_gain_val;
    if (i == ((int)("m"[0] - 'a')))
      hdlr = hdlr_rx_m_rf_gain_val;
    if (i == ((int)("n"[0] - 'a')))
      hdlr = hdlr_rx_n_rf_gain_val;
    if (i == ((int)("o"[0] - 'a')))
      hdlr = hdlr_rx_o_rf_gain_val;
    if (i == ((int)("p"[0] - 'a')))
      hdlr = hdlr_rx_p_rf_gain_val;

    r = hdlr(inbuf, outbuf);
    if (RETURN_SUCCESS != r) {
      return r;
    }

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

  for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {

    if (0 == (mask_tx & (1 << i))) {
      continue;
    }

    if (i == ((int)("a"[0] - 'a')))
      hdlr = hdlr_tx_a_rf_gain_val;
    if (i == ((int)("b"[0] - 'a')))
      hdlr = hdlr_tx_b_rf_gain_val;
    if (i == ((int)("c"[0] - 'a')))
      hdlr = hdlr_tx_c_rf_gain_val;
    if (i == ((int)("d"[0] - 'a')))
      hdlr = hdlr_tx_d_rf_gain_val;
    if (i == ((int)("e"[0] - 'a')))
      hdlr = hdlr_tx_e_rf_gain_val;
    if (i == ((int)("f"[0] - 'a')))
      hdlr = hdlr_tx_f_rf_gain_val;
    if (i == ((int)("g"[0] - 'a')))
      hdlr = hdlr_tx_g_rf_gain_val;
    if (i == ((int)("h"[0] - 'a')))
      hdlr = hdlr_tx_h_rf_gain_val;
    if (i == ((int)("i"[0] - 'a')))
      hdlr = hdlr_tx_i_rf_gain_val;
    if (i == ((int)("j"[0] - 'a')))
      hdlr = hdlr_tx_j_rf_gain_val;
    if (i == ((int)("k"[0] - 'a')))
      hdlr = hdlr_tx_k_rf_gain_val;
    if (i == ((int)("l"[0] - 'a')))
      hdlr = hdlr_tx_l_rf_gain_val;
    if (i == ((int)("m"[0] - 'a')))
      hdlr = hdlr_tx_m_rf_gain_val;
    if (i == ((int)("n"[0] - 'a')))
      hdlr = hdlr_tx_n_rf_gain_val;
    if (i == ((int)("o"[0] - 'a')))
      hdlr = hdlr_tx_o_rf_gain_val;
    if (i == ((int)("p"[0] - 'a')))
      hdlr = hdlr_tx_p_rf_gain_val;

    r = hdlr(inbuf, outbuf);
    if (RETURN_SUCCESS != r) {
      return r;
    }

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

  sync_mode |= 1;
  write_hps_reg("sync_mode", sync_mode);

  write_hps_reg("sync_mask", sync_mask);

  sprintf(inbuf, "%lf", freq);

  for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {

    if (0 == (mask_rx & (1 << i))) {
      continue;
    }

    if (i == ((int)("a"[0] - 'a')))
      hdlr = hdlr_rx_a_rf_gain_val;
    if (i == ((int)("b"[0] - 'a')))
      hdlr = hdlr_rx_b_rf_gain_val;
    if (i == ((int)("c"[0] - 'a')))
      hdlr = hdlr_rx_c_rf_gain_val;
    if (i == ((int)("d"[0] - 'a')))
      hdlr = hdlr_rx_d_rf_gain_val;
    if (i == ((int)("e"[0] - 'a')))
      hdlr = hdlr_rx_e_rf_gain_val;
    if (i == ((int)("f"[0] - 'a')))
      hdlr = hdlr_rx_f_rf_gain_val;
    if (i == ((int)("g"[0] - 'a')))
      hdlr = hdlr_rx_g_rf_gain_val;
    if (i == ((int)("h"[0] - 'a')))
      hdlr = hdlr_rx_h_rf_gain_val;
    if (i == ((int)("i"[0] - 'a')))
      hdlr = hdlr_rx_i_rf_gain_val;
    if (i == ((int)("j"[0] - 'a')))
      hdlr = hdlr_rx_j_rf_gain_val;
    if (i == ((int)("k"[0] - 'a')))
      hdlr = hdlr_rx_k_rf_gain_val;
    if (i == ((int)("l"[0] - 'a')))
      hdlr = hdlr_rx_l_rf_gain_val;
    if (i == ((int)("m"[0] - 'a')))
      hdlr = hdlr_rx_m_rf_gain_val;
    if (i == ((int)("n"[0] - 'a')))
      hdlr = hdlr_rx_n_rf_gain_val;
    if (i == ((int)("o"[0] - 'a')))
      hdlr = hdlr_rx_o_rf_gain_val;
    if (i == ((int)("p"[0] - 'a')))
      hdlr = hdlr_rx_p_rf_gain_val;

    r = hdlr(inbuf, outbuf);
    if (RETURN_SUCCESS != r) {
      return r;
    }

    prop = get_prop_from_hdlr(hdlr);
    wd_backup = prop->wd;
    prop->wd = -1;
    set_property(prop->path, inbuf);
    prop->wd = wd_backup;
  }

  for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {

    if (0 == (mask_tx & (1 << i))) {
      continue;
    }

    if (i == ((int)("a"[0] - 'a')))
      hdlr = hdlr_tx_a_rf_freq_val;
    if (i == ((int)("b"[0] - 'a')))
      hdlr = hdlr_tx_b_rf_freq_val;
    if (i == ((int)("c"[0] - 'a')))
      hdlr = hdlr_tx_c_rf_freq_val;
    if (i == ((int)("d"[0] - 'a')))
      hdlr = hdlr_tx_d_rf_freq_val;
    if (i == ((int)("e"[0] - 'a')))
      hdlr = hdlr_tx_e_rf_freq_val;
    if (i == ((int)("f"[0] - 'a')))
      hdlr = hdlr_tx_f_rf_freq_val;
    if (i == ((int)("g"[0] - 'a')))
      hdlr = hdlr_tx_g_rf_freq_val;
    if (i == ((int)("h"[0] - 'a')))
      hdlr = hdlr_tx_h_rf_freq_val;
    if (i == ((int)("i"[0] - 'a')))
      hdlr = hdlr_tx_i_rf_freq_val;
    if (i == ((int)("j"[0] - 'a')))
      hdlr = hdlr_tx_j_rf_freq_val;
    if (i == ((int)("k"[0] - 'a')))
      hdlr = hdlr_tx_k_rf_freq_val;
    if (i == ((int)("l"[0] - 'a')))
      hdlr = hdlr_tx_l_rf_freq_val;
    if (i == ((int)("m"[0] - 'a')))
      hdlr = hdlr_tx_m_rf_freq_val;
    if (i == ((int)("n"[0] - 'a')))
      hdlr = hdlr_tx_n_rf_freq_val;
    if (i == ((int)("o"[0] - 'a')))
      hdlr = hdlr_tx_o_rf_freq_val;
    if (i == ((int)("p"[0] - 'a')))
      hdlr = hdlr_tx_p_rf_freq_val;

    r = hdlr(inbuf, outbuf);
    if (RETURN_SUCCESS != r) {
      return r;
    }

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

  sync_mode |= 1;
  write_hps_reg("sync_mode", sync_mode);

  write_hps_reg("sync_mask", sync_mask);

  sprintf(inbuf, "%lf", freq);

  for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {

    if (0 == (mask_rx & (1 << i))) {
      continue;
    }

    if (i == ((int)("a"[0] - 'a')))
      hdlr = hdlr_rx_a_dsp_nco_adj;
    if (i == ((int)("b"[0] - 'a')))
      hdlr = hdlr_rx_b_dsp_nco_adj;
    if (i == ((int)("c"[0] - 'a')))
      hdlr = hdlr_rx_c_dsp_nco_adj;
    if (i == ((int)("d"[0] - 'a')))
      hdlr = hdlr_rx_d_dsp_nco_adj;
    if (i == ((int)("e"[0] - 'a')))
      hdlr = hdlr_rx_e_dsp_nco_adj;
    if (i == ((int)("f"[0] - 'a')))
      hdlr = hdlr_rx_f_dsp_nco_adj;
    if (i == ((int)("g"[0] - 'a')))
      hdlr = hdlr_rx_g_dsp_nco_adj;
    if (i == ((int)("h"[0] - 'a')))
      hdlr = hdlr_rx_h_dsp_nco_adj;
    if (i == ((int)("i"[0] - 'a')))
      hdlr = hdlr_rx_i_dsp_nco_adj;
    if (i == ((int)("j"[0] - 'a')))
      hdlr = hdlr_rx_j_dsp_nco_adj;
    if (i == ((int)("k"[0] - 'a')))
      hdlr = hdlr_rx_k_dsp_nco_adj;
    if (i == ((int)("l"[0] - 'a')))
      hdlr = hdlr_rx_l_dsp_nco_adj;
    if (i == ((int)("m"[0] - 'a')))
      hdlr = hdlr_rx_m_dsp_nco_adj;
    if (i == ((int)("n"[0] - 'a')))
      hdlr = hdlr_rx_n_dsp_nco_adj;
    if (i == ((int)("o"[0] - 'a')))
      hdlr = hdlr_rx_o_dsp_nco_adj;
    if (i == ((int)("p"[0] - 'a')))
      hdlr = hdlr_rx_p_dsp_nco_adj;

    r = hdlr(inbuf, outbuf);
    if (RETURN_SUCCESS != r) {
      return r;
    }

    prop = get_prop_from_hdlr(hdlr);
    wd_backup = prop->wd;
    prop->wd = -1;
    set_property(prop->path, inbuf);
    prop->wd = wd_backup;
  }

  for (i = 0; i < ((int)sizeof(names) / sizeof(*names)); i++) {

    if (0 == (mask_tx & (1 << i))) {
      continue;
    }

    if (i == ((int)("a"[0] - 'a')))
      hdlr = hdlr_tx_a_dsp_nco_adj;
    if (i == ((int)("b"[0] - 'a')))
      hdlr = hdlr_tx_b_dsp_nco_adj;
    if (i == ((int)("c"[0] - 'a')))
      hdlr = hdlr_tx_c_dsp_nco_adj;
    if (i == ((int)("d"[0] - 'a')))
      hdlr = hdlr_tx_d_dsp_nco_adj;
    if (i == ((int)("e"[0] - 'a')))
      hdlr = hdlr_tx_e_dsp_nco_adj;
    if (i == ((int)("f"[0] - 'a')))
      hdlr = hdlr_tx_f_dsp_nco_adj;
    if (i == ((int)("g"[0] - 'a')))
      hdlr = hdlr_tx_g_dsp_nco_adj;
    if (i == ((int)("h"[0] - 'a')))
      hdlr = hdlr_tx_h_dsp_nco_adj;
    if (i == ((int)("i"[0] - 'a')))
      hdlr = hdlr_tx_i_dsp_nco_adj;
    if (i == ((int)("j"[0] - 'a')))
      hdlr = hdlr_tx_j_dsp_nco_adj;
    if (i == ((int)("k"[0] - 'a')))
      hdlr = hdlr_tx_k_dsp_nco_adj;
    if (i == ((int)("l"[0] - 'a')))
      hdlr = hdlr_tx_l_dsp_nco_adj;
    if (i == ((int)("m"[0] - 'a')))
      hdlr = hdlr_tx_m_dsp_nco_adj;
    if (i == ((int)("n"[0] - 'a')))
      hdlr = hdlr_tx_n_dsp_nco_adj;
    if (i == ((int)("o"[0] - 'a')))
      hdlr = hdlr_tx_o_dsp_nco_adj;
    if (i == ((int)("p"[0] - 'a')))
      hdlr = hdlr_tx_p_dsp_nco_adj;

    r = hdlr(inbuf, outbuf);
    if (RETURN_SUCCESS != r) {
      return r;
    }

    prop = get_prop_from_hdlr(hdlr);
    wd_backup = prop->wd;
    prop->wd = -1;
    set_property(prop->path, inbuf);
    prop->wd = wd_backup;
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

static int hdlr_time_source_ref(const char *data, char *ret) {
  if (strcmp(data, "external") == 0) {
    strcpy(buf, "clk -t 1\r");
  } else if (strcmp(data, "internal") == 0) {
    strcpy(buf, "clk -t 0\r");
  }
  send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
  return RETURN_SUCCESS;
}

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

static int hdlr_time_sync_lmk_sync_tgl_jesd(const char *data, char *ret) {
  if (strcmp(data, "0") != 0) {
    strcpy(buf, "sync -k\r");
  }
  send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
  return RETURN_SUCCESS;
}

static int hdlr_time_sync_lmk_sync_tgl_pll(const char *data, char *ret) {
  if (strcmp(data, "0") != 0) {
    strcpy(buf, "sync -q\r");
  }
  send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
  return RETURN_SUCCESS;
}

static int hdlr_time_sync_lmk_resync_jesd(const char *data, char *ret) {
  if (strcmp(data, "0") != 0) {
    strcpy(buf, "sync -j\r");
  }
  send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
  return RETURN_SUCCESS;
}

static int hdlr_time_sync_lmk_resync_pll(const char *data, char *ret) {
  if (strcmp(data, "0") != 0) {
    strcpy(buf, "sync -p\r");
  }
  send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
  return RETURN_SUCCESS;
}

static int hdlr_time_sync_lmk_resync_all(const char *data, char *ret) {
  if (strcmp(data, "0") != 0) {
    strcpy(buf, "sync -r\r");
  }
  send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
  return RETURN_SUCCESS;
}

static int hdlr_time_source_devclk(const char *data, char *ret) {
  if (strcmp(data, "external") == 0) {
    strcpy(buf, "clk -t 1\r");
  } else if (strcmp(data, "internal") == 0) {
    strcpy(buf, "clk -t 0\r");
  }
  send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
  return RETURN_SUCCESS;
}

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

static int hdlr_fpga_board_dump(const char *data, char *ret) {

  hdlr_tx_a_rf_board_dump(NULL, NULL);
  hdlr_tx_b_rf_board_dump(NULL, NULL);
  hdlr_tx_c_rf_board_dump(NULL, NULL);
  hdlr_tx_d_rf_board_dump(NULL, NULL);
  hdlr_tx_e_rf_board_dump(NULL, NULL);
  hdlr_tx_f_rf_board_dump(NULL, NULL);
  hdlr_tx_g_rf_board_dump(NULL, NULL);
  hdlr_tx_h_rf_board_dump(NULL, NULL);
  hdlr_tx_i_rf_board_dump(NULL, NULL);
  hdlr_tx_j_rf_board_dump(NULL, NULL);
  hdlr_tx_k_rf_board_dump(NULL, NULL);
  hdlr_tx_l_rf_board_dump(NULL, NULL);
  hdlr_tx_m_rf_board_dump(NULL, NULL);
  hdlr_tx_n_rf_board_dump(NULL, NULL);
  hdlr_tx_o_rf_board_dump(NULL, NULL);
  hdlr_tx_p_rf_board_dump(NULL, NULL);

  hdlr_rx_a_rf_board_dump(NULL, NULL);
  hdlr_rx_b_rf_board_dump(NULL, NULL);
  hdlr_rx_c_rf_board_dump(NULL, NULL);
  hdlr_rx_d_rf_board_dump(NULL, NULL);
  hdlr_rx_e_rf_board_dump(NULL, NULL);
  hdlr_rx_f_rf_board_dump(NULL, NULL);
  hdlr_rx_g_rf_board_dump(NULL, NULL);
  hdlr_rx_h_rf_board_dump(NULL, NULL);
  hdlr_rx_i_rf_board_dump(NULL, NULL);
  hdlr_rx_j_rf_board_dump(NULL, NULL);
  hdlr_rx_k_rf_board_dump(NULL, NULL);
  hdlr_rx_l_rf_board_dump(NULL, NULL);
  hdlr_rx_m_rf_board_dump(NULL, NULL);
  hdlr_rx_n_rf_board_dump(NULL, NULL);
  hdlr_rx_o_rf_board_dump(NULL, NULL);
  hdlr_rx_p_rf_board_dump(NULL, NULL);

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

    send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);

    strcpy(buf, "board -g 1\r");

    send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
  }
  if (strcmp(data, "2") == 0) {
    strcpy(buf, "board -g 2\r");
    send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
    usleep(50000);

    strcpy(buf, "board -g 2\r");

    send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);

    strcpy(buf, "board -g 2\r");

    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
    send_uart_comm(uart_tx_fd[((int)("ch"[0] - 'a'))], (uint8_t *)buf,
                   strlen(buf)),
        usleep(50000);
  }
  return RETURN_SUCCESS;
}

static int hdlr_fpga_board_temp(const char *data, char *ret) {
  uint32_t old_val;
  read_hps_reg("sys14", &old_val);

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

  send_uart_comm(uart_rx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_rx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);

  strcpy(buf, "board -r\r");

  send_uart_comm(uart_tx_fd[((int)("a"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("b"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("c"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("d"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("e"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("f"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("g"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("h"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("i"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("j"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("k"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("l"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("m"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("n"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("o"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);
  send_uart_comm(uart_tx_fd[((int)("p"[0] - 'a'))], (uint8_t *)buf,
                 strlen(buf)),
      usleep(50000);

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
  if (ipver[0] == 0) {
    sscanf(data, "%" SCNd32 ".%" SCNd32 ".%" SCNd32 ".%" SCNd32 "", ip, ip + 1,
           ip + 2, ip + 3);
    write_hps_reg("net5",
                  (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]));
  } else if (ipver[0] == 1) {
    sscanf(data, "%" SCNx32 ":%" SCNx32 ":%" SCNx32 ":%" SCNx32 "", ip, ip + 1,
           ip + 2, ip + 3);
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
  if (ipver[1] == 0) {
    sscanf(data, "%" SCNd32 ".%" SCNd32 ".%" SCNd32 ".%" SCNd32 "", ip, ip + 1,
           ip + 2, ip + 3);
    ip[0] = (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]);
    write_hps_reg("net20", ip[0]);
  } else if (ipver[1] == 1) {
    sscanf(data, "%" SCNx32 ":%" SCNx32 ":%" SCNx32 ":%" SCNx32 "", ip, ip + 1,
           ip + 2, ip + 3);
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

  char ip_address[MAX_PROP_LEN] = {0};
  char command[MAX_PROP_LEN] = {0};
  sscanf(data, "%s", ip_address);

  struct sockaddr_in sa;
  if (!inet_pton(AF_INET, ip_address, &(sa.sin_addr))) {
    return RETURN_ERROR_PARAM;
  }

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
  write_hps_reg("sys11", 0);
  write_hps_reg("sys12", 0);
  write_hps_reg("sys13", 1);
  write_hps_reg("sys13", 0);

  return RETURN_SUCCESS;
}
# 3056 "properties.c"
static prop_t property_table[] = {

    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "a",
        .symlink_target = "rx/"
                          "a",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/trigger/sma_mode",
        .handler = hdlr_rx_a_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/trigger/trig_sel",
        .handler = hdlr_rx_a_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_a_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_a_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_a_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_a_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_a_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/pwr",
        .handler = hdlr_rx_a_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/stream",
        .handler = hdlr_rx_a_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/rf/freq/val",
        .handler = hdlr_rx_a_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_a_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/rf/freq/lna",
        .handler = hdlr_rx_a_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/rf/freq/band",
        .handler = hdlr_rx_a_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/rf/gain/val",
        .handler = hdlr_rx_a_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/rf/atten/val",
        .handler = hdlr_rx_a_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/status/rfpll_lock",
        .handler = hdlr_rx_a_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/status/adc_alarm",
        .handler = hdlr_rx_a_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/board/dump",
        .handler = hdlr_rx_a_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/board/test",
        .handler = hdlr_rx_a_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/board/temp",
        .handler = hdlr_rx_a_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/board/led",
        .handler = hdlr_rx_a_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/dsp/signed",
        .handler = hdlr_rx_a_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/dsp/gain",
        .handler = hdlr_rx_a_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/dsp/rate",
        .handler = hdlr_rx_a_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/dsp/nco_adj",
        .handler = hdlr_rx_a_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/dsp/rstreq",
        .handler = hdlr_rx_a_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/dsp/loopback",
        .handler = hdlr_rx_a_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/about/id",
        .handler = hdlr_rx_a_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/about/serial",
        .handler = hdlr_rx_a_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/about/mcudevid",
        .handler = hdlr_rx_a_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/about/mcurev",
        .handler = hdlr_rx_a_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/about/mcufuses",
        .handler = hdlr_rx_a_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/about/fw_ver",
        .handler = hdlr_rx_a_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/link/vita_en",
        .handler = hdlr_rx_a_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/link/iface",
        .handler = hdlr_rx_a_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/link/port",
        .handler = hdlr_rx_a_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/link/ip_dest",
        .handler = hdlr_rx_a_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "a"
                "/link/mac_dest",
        .handler = hdlr_rx_a_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "b",
        .symlink_target = "rx/"
                          "b",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/trigger/sma_mode",
        .handler = hdlr_rx_b_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/trigger/trig_sel",
        .handler = hdlr_rx_b_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_b_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_b_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_b_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_b_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_b_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/pwr",
        .handler = hdlr_rx_b_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/stream",
        .handler = hdlr_rx_b_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/rf/freq/val",
        .handler = hdlr_rx_b_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_b_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/rf/freq/lna",
        .handler = hdlr_rx_b_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/rf/freq/band",
        .handler = hdlr_rx_b_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/rf/gain/val",
        .handler = hdlr_rx_b_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/rf/atten/val",
        .handler = hdlr_rx_b_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/status/rfpll_lock",
        .handler = hdlr_rx_b_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/status/adc_alarm",
        .handler = hdlr_rx_b_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/board/dump",
        .handler = hdlr_rx_b_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/board/test",
        .handler = hdlr_rx_b_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/board/temp",
        .handler = hdlr_rx_b_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/board/led",
        .handler = hdlr_rx_b_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/dsp/signed",
        .handler = hdlr_rx_b_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/dsp/gain",
        .handler = hdlr_rx_b_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/dsp/rate",
        .handler = hdlr_rx_b_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/dsp/nco_adj",
        .handler = hdlr_rx_b_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/dsp/rstreq",
        .handler = hdlr_rx_b_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/dsp/loopback",
        .handler = hdlr_rx_b_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/about/id",
        .handler = hdlr_rx_b_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/about/serial",
        .handler = hdlr_rx_b_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/about/mcudevid",
        .handler = hdlr_rx_b_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/about/mcurev",
        .handler = hdlr_rx_b_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/about/mcufuses",
        .handler = hdlr_rx_b_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/about/fw_ver",
        .handler = hdlr_rx_b_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/link/vita_en",
        .handler = hdlr_rx_b_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/link/iface",
        .handler = hdlr_rx_b_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/link/port",
        .handler = hdlr_rx_b_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/link/ip_dest",
        .handler = hdlr_rx_b_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "b"
                "/link/mac_dest",
        .handler = hdlr_rx_b_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "c",
        .symlink_target = "rx/"
                          "c",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/trigger/sma_mode",
        .handler = hdlr_rx_c_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/trigger/trig_sel",
        .handler = hdlr_rx_c_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_c_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_c_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_c_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_c_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_c_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/pwr",
        .handler = hdlr_rx_c_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/stream",
        .handler = hdlr_rx_c_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/rf/freq/val",
        .handler = hdlr_rx_c_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_c_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/rf/freq/lna",
        .handler = hdlr_rx_c_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/rf/freq/band",
        .handler = hdlr_rx_c_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/rf/gain/val",
        .handler = hdlr_rx_c_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/rf/atten/val",
        .handler = hdlr_rx_c_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/status/rfpll_lock",
        .handler = hdlr_rx_c_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/status/adc_alarm",
        .handler = hdlr_rx_c_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/board/dump",
        .handler = hdlr_rx_c_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/board/test",
        .handler = hdlr_rx_c_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/board/temp",
        .handler = hdlr_rx_c_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/board/led",
        .handler = hdlr_rx_c_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/dsp/signed",
        .handler = hdlr_rx_c_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/dsp/gain",
        .handler = hdlr_rx_c_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/dsp/rate",
        .handler = hdlr_rx_c_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/dsp/nco_adj",
        .handler = hdlr_rx_c_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/dsp/rstreq",
        .handler = hdlr_rx_c_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/dsp/loopback",
        .handler = hdlr_rx_c_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/about/id",
        .handler = hdlr_rx_c_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/about/serial",
        .handler = hdlr_rx_c_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/about/mcudevid",
        .handler = hdlr_rx_c_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/about/mcurev",
        .handler = hdlr_rx_c_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/about/mcufuses",
        .handler = hdlr_rx_c_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/about/fw_ver",
        .handler = hdlr_rx_c_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/link/vita_en",
        .handler = hdlr_rx_c_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/link/iface",
        .handler = hdlr_rx_c_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/link/port",
        .handler = hdlr_rx_c_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/link/ip_dest",
        .handler = hdlr_rx_c_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "c"
                "/link/mac_dest",
        .handler = hdlr_rx_c_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "d",
        .symlink_target = "rx/"
                          "d",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/trigger/sma_mode",
        .handler = hdlr_rx_d_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/trigger/trig_sel",
        .handler = hdlr_rx_d_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_d_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_d_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_d_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_d_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_d_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/pwr",
        .handler = hdlr_rx_d_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/stream",
        .handler = hdlr_rx_d_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/rf/freq/val",
        .handler = hdlr_rx_d_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_d_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/rf/freq/lna",
        .handler = hdlr_rx_d_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/rf/freq/band",
        .handler = hdlr_rx_d_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/rf/gain/val",
        .handler = hdlr_rx_d_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/rf/atten/val",
        .handler = hdlr_rx_d_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/status/rfpll_lock",
        .handler = hdlr_rx_d_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/status/adc_alarm",
        .handler = hdlr_rx_d_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/board/dump",
        .handler = hdlr_rx_d_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/board/test",
        .handler = hdlr_rx_d_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/board/temp",
        .handler = hdlr_rx_d_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/board/led",
        .handler = hdlr_rx_d_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/dsp/signed",
        .handler = hdlr_rx_d_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/dsp/gain",
        .handler = hdlr_rx_d_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/dsp/rate",
        .handler = hdlr_rx_d_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/dsp/nco_adj",
        .handler = hdlr_rx_d_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/dsp/rstreq",
        .handler = hdlr_rx_d_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/dsp/loopback",
        .handler = hdlr_rx_d_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/about/id",
        .handler = hdlr_rx_d_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/about/serial",
        .handler = hdlr_rx_d_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/about/mcudevid",
        .handler = hdlr_rx_d_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/about/mcurev",
        .handler = hdlr_rx_d_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/about/mcufuses",
        .handler = hdlr_rx_d_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/about/fw_ver",
        .handler = hdlr_rx_d_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/link/vita_en",
        .handler = hdlr_rx_d_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/link/iface",
        .handler = hdlr_rx_d_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/link/port",
        .handler = hdlr_rx_d_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/link/ip_dest",
        .handler = hdlr_rx_d_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "d"
                "/link/mac_dest",
        .handler = hdlr_rx_d_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "e",
        .symlink_target = "rx/"
                          "e",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/trigger/sma_mode",
        .handler = hdlr_rx_e_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/trigger/trig_sel",
        .handler = hdlr_rx_e_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_e_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_e_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_e_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_e_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_e_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/pwr",
        .handler = hdlr_rx_e_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/stream",
        .handler = hdlr_rx_e_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/rf/freq/val",
        .handler = hdlr_rx_e_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_e_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/rf/freq/lna",
        .handler = hdlr_rx_e_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/rf/freq/band",
        .handler = hdlr_rx_e_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/rf/gain/val",
        .handler = hdlr_rx_e_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/rf/atten/val",
        .handler = hdlr_rx_e_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/status/rfpll_lock",
        .handler = hdlr_rx_e_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/status/adc_alarm",
        .handler = hdlr_rx_e_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/board/dump",
        .handler = hdlr_rx_e_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/board/test",
        .handler = hdlr_rx_e_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/board/temp",
        .handler = hdlr_rx_e_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/board/led",
        .handler = hdlr_rx_e_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/dsp/signed",
        .handler = hdlr_rx_e_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/dsp/gain",
        .handler = hdlr_rx_e_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/dsp/rate",
        .handler = hdlr_rx_e_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/dsp/nco_adj",
        .handler = hdlr_rx_e_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/dsp/rstreq",
        .handler = hdlr_rx_e_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/dsp/loopback",
        .handler = hdlr_rx_e_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/about/id",
        .handler = hdlr_rx_e_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/about/serial",
        .handler = hdlr_rx_e_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/about/mcudevid",
        .handler = hdlr_rx_e_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/about/mcurev",
        .handler = hdlr_rx_e_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/about/mcufuses",
        .handler = hdlr_rx_e_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/about/fw_ver",
        .handler = hdlr_rx_e_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/link/vita_en",
        .handler = hdlr_rx_e_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/link/iface",
        .handler = hdlr_rx_e_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/link/port",
        .handler = hdlr_rx_e_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/link/ip_dest",
        .handler = hdlr_rx_e_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "e"
                "/link/mac_dest",
        .handler = hdlr_rx_e_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "f",
        .symlink_target = "rx/"
                          "f",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/trigger/sma_mode",
        .handler = hdlr_rx_f_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/trigger/trig_sel",
        .handler = hdlr_rx_f_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_f_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_f_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_f_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_f_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_f_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/pwr",
        .handler = hdlr_rx_f_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/stream",
        .handler = hdlr_rx_f_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/rf/freq/val",
        .handler = hdlr_rx_f_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_f_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/rf/freq/lna",
        .handler = hdlr_rx_f_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/rf/freq/band",
        .handler = hdlr_rx_f_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/rf/gain/val",
        .handler = hdlr_rx_f_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/rf/atten/val",
        .handler = hdlr_rx_f_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/status/rfpll_lock",
        .handler = hdlr_rx_f_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/status/adc_alarm",
        .handler = hdlr_rx_f_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/board/dump",
        .handler = hdlr_rx_f_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/board/test",
        .handler = hdlr_rx_f_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/board/temp",
        .handler = hdlr_rx_f_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/board/led",
        .handler = hdlr_rx_f_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/dsp/signed",
        .handler = hdlr_rx_f_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/dsp/gain",
        .handler = hdlr_rx_f_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/dsp/rate",
        .handler = hdlr_rx_f_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/dsp/nco_adj",
        .handler = hdlr_rx_f_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/dsp/rstreq",
        .handler = hdlr_rx_f_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/dsp/loopback",
        .handler = hdlr_rx_f_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/about/id",
        .handler = hdlr_rx_f_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/about/serial",
        .handler = hdlr_rx_f_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/about/mcudevid",
        .handler = hdlr_rx_f_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/about/mcurev",
        .handler = hdlr_rx_f_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/about/mcufuses",
        .handler = hdlr_rx_f_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/about/fw_ver",
        .handler = hdlr_rx_f_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/link/vita_en",
        .handler = hdlr_rx_f_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/link/iface",
        .handler = hdlr_rx_f_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/link/port",
        .handler = hdlr_rx_f_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/link/ip_dest",
        .handler = hdlr_rx_f_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "f"
                "/link/mac_dest",
        .handler = hdlr_rx_f_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "g",
        .symlink_target = "rx/"
                          "g",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/trigger/sma_mode",
        .handler = hdlr_rx_g_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/trigger/trig_sel",
        .handler = hdlr_rx_g_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_g_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_g_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_g_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_g_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_g_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/pwr",
        .handler = hdlr_rx_g_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/stream",
        .handler = hdlr_rx_g_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/rf/freq/val",
        .handler = hdlr_rx_g_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_g_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/rf/freq/lna",
        .handler = hdlr_rx_g_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/rf/freq/band",
        .handler = hdlr_rx_g_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/rf/gain/val",
        .handler = hdlr_rx_g_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/rf/atten/val",
        .handler = hdlr_rx_g_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/status/rfpll_lock",
        .handler = hdlr_rx_g_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/status/adc_alarm",
        .handler = hdlr_rx_g_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/board/dump",
        .handler = hdlr_rx_g_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/board/test",
        .handler = hdlr_rx_g_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/board/temp",
        .handler = hdlr_rx_g_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/board/led",
        .handler = hdlr_rx_g_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/dsp/signed",
        .handler = hdlr_rx_g_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/dsp/gain",
        .handler = hdlr_rx_g_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/dsp/rate",
        .handler = hdlr_rx_g_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/dsp/nco_adj",
        .handler = hdlr_rx_g_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/dsp/rstreq",
        .handler = hdlr_rx_g_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/dsp/loopback",
        .handler = hdlr_rx_g_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/about/id",
        .handler = hdlr_rx_g_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/about/serial",
        .handler = hdlr_rx_g_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/about/mcudevid",
        .handler = hdlr_rx_g_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/about/mcurev",
        .handler = hdlr_rx_g_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/about/mcufuses",
        .handler = hdlr_rx_g_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/about/fw_ver",
        .handler = hdlr_rx_g_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/link/vita_en",
        .handler = hdlr_rx_g_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/link/iface",
        .handler = hdlr_rx_g_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/link/port",
        .handler = hdlr_rx_g_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/link/ip_dest",
        .handler = hdlr_rx_g_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "g"
                "/link/mac_dest",
        .handler = hdlr_rx_g_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "h",
        .symlink_target = "rx/"
                          "h",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/trigger/sma_mode",
        .handler = hdlr_rx_h_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/trigger/trig_sel",
        .handler = hdlr_rx_h_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_h_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_h_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_h_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_h_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_h_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/pwr",
        .handler = hdlr_rx_h_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/stream",
        .handler = hdlr_rx_h_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/rf/freq/val",
        .handler = hdlr_rx_h_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_h_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/rf/freq/lna",
        .handler = hdlr_rx_h_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/rf/freq/band",
        .handler = hdlr_rx_h_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/rf/gain/val",
        .handler = hdlr_rx_h_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/rf/atten/val",
        .handler = hdlr_rx_h_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/status/rfpll_lock",
        .handler = hdlr_rx_h_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/status/adc_alarm",
        .handler = hdlr_rx_h_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/board/dump",
        .handler = hdlr_rx_h_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/board/test",
        .handler = hdlr_rx_h_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/board/temp",
        .handler = hdlr_rx_h_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/board/led",
        .handler = hdlr_rx_h_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/dsp/signed",
        .handler = hdlr_rx_h_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/dsp/gain",
        .handler = hdlr_rx_h_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/dsp/rate",
        .handler = hdlr_rx_h_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/dsp/nco_adj",
        .handler = hdlr_rx_h_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/dsp/rstreq",
        .handler = hdlr_rx_h_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/dsp/loopback",
        .handler = hdlr_rx_h_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/about/id",
        .handler = hdlr_rx_h_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/about/serial",
        .handler = hdlr_rx_h_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/about/mcudevid",
        .handler = hdlr_rx_h_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/about/mcurev",
        .handler = hdlr_rx_h_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/about/mcufuses",
        .handler = hdlr_rx_h_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/about/fw_ver",
        .handler = hdlr_rx_h_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/link/vita_en",
        .handler = hdlr_rx_h_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/link/iface",
        .handler = hdlr_rx_h_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/link/port",
        .handler = hdlr_rx_h_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/link/ip_dest",
        .handler = hdlr_rx_h_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "h"
                "/link/mac_dest",
        .handler = hdlr_rx_h_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "i",
        .symlink_target = "rx/"
                          "i",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/trigger/sma_mode",
        .handler = hdlr_rx_i_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/trigger/trig_sel",
        .handler = hdlr_rx_i_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_i_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_i_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_i_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_i_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_i_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/pwr",
        .handler = hdlr_rx_i_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/stream",
        .handler = hdlr_rx_i_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/rf/freq/val",
        .handler = hdlr_rx_i_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_i_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/rf/freq/lna",
        .handler = hdlr_rx_i_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/rf/freq/band",
        .handler = hdlr_rx_i_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/rf/gain/val",
        .handler = hdlr_rx_i_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/rf/atten/val",
        .handler = hdlr_rx_i_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/status/rfpll_lock",
        .handler = hdlr_rx_i_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/status/adc_alarm",
        .handler = hdlr_rx_i_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/board/dump",
        .handler = hdlr_rx_i_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/board/test",
        .handler = hdlr_rx_i_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/board/temp",
        .handler = hdlr_rx_i_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/board/led",
        .handler = hdlr_rx_i_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/dsp/signed",
        .handler = hdlr_rx_i_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/dsp/gain",
        .handler = hdlr_rx_i_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/dsp/rate",
        .handler = hdlr_rx_i_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/dsp/nco_adj",
        .handler = hdlr_rx_i_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/dsp/rstreq",
        .handler = hdlr_rx_i_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/dsp/loopback",
        .handler = hdlr_rx_i_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/about/id",
        .handler = hdlr_rx_i_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/about/serial",
        .handler = hdlr_rx_i_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/about/mcudevid",
        .handler = hdlr_rx_i_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/about/mcurev",
        .handler = hdlr_rx_i_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/about/mcufuses",
        .handler = hdlr_rx_i_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/about/fw_ver",
        .handler = hdlr_rx_i_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/link/vita_en",
        .handler = hdlr_rx_i_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/link/iface",
        .handler = hdlr_rx_i_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/link/port",
        .handler = hdlr_rx_i_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/link/ip_dest",
        .handler = hdlr_rx_i_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "i"
                "/link/mac_dest",
        .handler = hdlr_rx_i_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "j",
        .symlink_target = "rx/"
                          "j",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/trigger/sma_mode",
        .handler = hdlr_rx_j_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/trigger/trig_sel",
        .handler = hdlr_rx_j_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_j_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_j_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_j_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_j_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_j_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/pwr",
        .handler = hdlr_rx_j_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/stream",
        .handler = hdlr_rx_j_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/rf/freq/val",
        .handler = hdlr_rx_j_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_j_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/rf/freq/lna",
        .handler = hdlr_rx_j_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/rf/freq/band",
        .handler = hdlr_rx_j_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/rf/gain/val",
        .handler = hdlr_rx_j_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/rf/atten/val",
        .handler = hdlr_rx_j_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/status/rfpll_lock",
        .handler = hdlr_rx_j_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/status/adc_alarm",
        .handler = hdlr_rx_j_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/board/dump",
        .handler = hdlr_rx_j_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/board/test",
        .handler = hdlr_rx_j_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/board/temp",
        .handler = hdlr_rx_j_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/board/led",
        .handler = hdlr_rx_j_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/dsp/signed",
        .handler = hdlr_rx_j_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/dsp/gain",
        .handler = hdlr_rx_j_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/dsp/rate",
        .handler = hdlr_rx_j_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/dsp/nco_adj",
        .handler = hdlr_rx_j_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/dsp/rstreq",
        .handler = hdlr_rx_j_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/dsp/loopback",
        .handler = hdlr_rx_j_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/about/id",
        .handler = hdlr_rx_j_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/about/serial",
        .handler = hdlr_rx_j_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/about/mcudevid",
        .handler = hdlr_rx_j_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/about/mcurev",
        .handler = hdlr_rx_j_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/about/mcufuses",
        .handler = hdlr_rx_j_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/about/fw_ver",
        .handler = hdlr_rx_j_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/link/vita_en",
        .handler = hdlr_rx_j_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/link/iface",
        .handler = hdlr_rx_j_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/link/port",
        .handler = hdlr_rx_j_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/link/ip_dest",
        .handler = hdlr_rx_j_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "j"
                "/link/mac_dest",
        .handler = hdlr_rx_j_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "k",
        .symlink_target = "rx/"
                          "k",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/trigger/sma_mode",
        .handler = hdlr_rx_k_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/trigger/trig_sel",
        .handler = hdlr_rx_k_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_k_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_k_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_k_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_k_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_k_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/pwr",
        .handler = hdlr_rx_k_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/stream",
        .handler = hdlr_rx_k_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/rf/freq/val",
        .handler = hdlr_rx_k_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_k_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/rf/freq/lna",
        .handler = hdlr_rx_k_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/rf/freq/band",
        .handler = hdlr_rx_k_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/rf/gain/val",
        .handler = hdlr_rx_k_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/rf/atten/val",
        .handler = hdlr_rx_k_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/status/rfpll_lock",
        .handler = hdlr_rx_k_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/status/adc_alarm",
        .handler = hdlr_rx_k_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/board/dump",
        .handler = hdlr_rx_k_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/board/test",
        .handler = hdlr_rx_k_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/board/temp",
        .handler = hdlr_rx_k_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/board/led",
        .handler = hdlr_rx_k_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/dsp/signed",
        .handler = hdlr_rx_k_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/dsp/gain",
        .handler = hdlr_rx_k_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/dsp/rate",
        .handler = hdlr_rx_k_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/dsp/nco_adj",
        .handler = hdlr_rx_k_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/dsp/rstreq",
        .handler = hdlr_rx_k_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/dsp/loopback",
        .handler = hdlr_rx_k_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/about/id",
        .handler = hdlr_rx_k_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/about/serial",
        .handler = hdlr_rx_k_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/about/mcudevid",
        .handler = hdlr_rx_k_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/about/mcurev",
        .handler = hdlr_rx_k_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/about/mcufuses",
        .handler = hdlr_rx_k_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/about/fw_ver",
        .handler = hdlr_rx_k_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/link/vita_en",
        .handler = hdlr_rx_k_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/link/iface",
        .handler = hdlr_rx_k_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/link/port",
        .handler = hdlr_rx_k_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/link/ip_dest",
        .handler = hdlr_rx_k_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "k"
                "/link/mac_dest",
        .handler = hdlr_rx_k_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "l",
        .symlink_target = "rx/"
                          "l",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/trigger/sma_mode",
        .handler = hdlr_rx_l_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/trigger/trig_sel",
        .handler = hdlr_rx_l_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_l_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_l_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_l_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_l_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_l_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/pwr",
        .handler = hdlr_rx_l_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/stream",
        .handler = hdlr_rx_l_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/rf/freq/val",
        .handler = hdlr_rx_l_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_l_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/rf/freq/lna",
        .handler = hdlr_rx_l_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/rf/freq/band",
        .handler = hdlr_rx_l_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/rf/gain/val",
        .handler = hdlr_rx_l_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/rf/atten/val",
        .handler = hdlr_rx_l_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/status/rfpll_lock",
        .handler = hdlr_rx_l_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/status/adc_alarm",
        .handler = hdlr_rx_l_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/board/dump",
        .handler = hdlr_rx_l_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/board/test",
        .handler = hdlr_rx_l_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/board/temp",
        .handler = hdlr_rx_l_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/board/led",
        .handler = hdlr_rx_l_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/dsp/signed",
        .handler = hdlr_rx_l_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/dsp/gain",
        .handler = hdlr_rx_l_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/dsp/rate",
        .handler = hdlr_rx_l_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/dsp/nco_adj",
        .handler = hdlr_rx_l_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/dsp/rstreq",
        .handler = hdlr_rx_l_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/dsp/loopback",
        .handler = hdlr_rx_l_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/about/id",
        .handler = hdlr_rx_l_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/about/serial",
        .handler = hdlr_rx_l_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/about/mcudevid",
        .handler = hdlr_rx_l_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/about/mcurev",
        .handler = hdlr_rx_l_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/about/mcufuses",
        .handler = hdlr_rx_l_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/about/fw_ver",
        .handler = hdlr_rx_l_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/link/vita_en",
        .handler = hdlr_rx_l_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/link/iface",
        .handler = hdlr_rx_l_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/link/port",
        .handler = hdlr_rx_l_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/link/ip_dest",
        .handler = hdlr_rx_l_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "l"
                "/link/mac_dest",
        .handler = hdlr_rx_l_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "m",
        .symlink_target = "rx/"
                          "m",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/trigger/sma_mode",
        .handler = hdlr_rx_m_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/trigger/trig_sel",
        .handler = hdlr_rx_m_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_m_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_m_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_m_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_m_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_m_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/pwr",
        .handler = hdlr_rx_m_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/stream",
        .handler = hdlr_rx_m_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/rf/freq/val",
        .handler = hdlr_rx_m_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_m_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/rf/freq/lna",
        .handler = hdlr_rx_m_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/rf/freq/band",
        .handler = hdlr_rx_m_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/rf/gain/val",
        .handler = hdlr_rx_m_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/rf/atten/val",
        .handler = hdlr_rx_m_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/status/rfpll_lock",
        .handler = hdlr_rx_m_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/status/adc_alarm",
        .handler = hdlr_rx_m_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/board/dump",
        .handler = hdlr_rx_m_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/board/test",
        .handler = hdlr_rx_m_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/board/temp",
        .handler = hdlr_rx_m_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/board/led",
        .handler = hdlr_rx_m_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/dsp/signed",
        .handler = hdlr_rx_m_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/dsp/gain",
        .handler = hdlr_rx_m_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/dsp/rate",
        .handler = hdlr_rx_m_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/dsp/nco_adj",
        .handler = hdlr_rx_m_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/dsp/rstreq",
        .handler = hdlr_rx_m_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/dsp/loopback",
        .handler = hdlr_rx_m_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/about/id",
        .handler = hdlr_rx_m_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/about/serial",
        .handler = hdlr_rx_m_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/about/mcudevid",
        .handler = hdlr_rx_m_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/about/mcurev",
        .handler = hdlr_rx_m_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/about/mcufuses",
        .handler = hdlr_rx_m_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/about/fw_ver",
        .handler = hdlr_rx_m_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/link/vita_en",
        .handler = hdlr_rx_m_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/link/iface",
        .handler = hdlr_rx_m_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/link/port",
        .handler = hdlr_rx_m_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/link/ip_dest",
        .handler = hdlr_rx_m_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "m"
                "/link/mac_dest",
        .handler = hdlr_rx_m_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "n",
        .symlink_target = "rx/"
                          "n",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/trigger/sma_mode",
        .handler = hdlr_rx_n_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/trigger/trig_sel",
        .handler = hdlr_rx_n_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_n_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_n_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_n_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_n_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_n_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/pwr",
        .handler = hdlr_rx_n_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/stream",
        .handler = hdlr_rx_n_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/rf/freq/val",
        .handler = hdlr_rx_n_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_n_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/rf/freq/lna",
        .handler = hdlr_rx_n_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/rf/freq/band",
        .handler = hdlr_rx_n_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/rf/gain/val",
        .handler = hdlr_rx_n_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/rf/atten/val",
        .handler = hdlr_rx_n_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/status/rfpll_lock",
        .handler = hdlr_rx_n_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/status/adc_alarm",
        .handler = hdlr_rx_n_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/board/dump",
        .handler = hdlr_rx_n_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/board/test",
        .handler = hdlr_rx_n_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/board/temp",
        .handler = hdlr_rx_n_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/board/led",
        .handler = hdlr_rx_n_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/dsp/signed",
        .handler = hdlr_rx_n_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/dsp/gain",
        .handler = hdlr_rx_n_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/dsp/rate",
        .handler = hdlr_rx_n_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/dsp/nco_adj",
        .handler = hdlr_rx_n_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/dsp/rstreq",
        .handler = hdlr_rx_n_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/dsp/loopback",
        .handler = hdlr_rx_n_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/about/id",
        .handler = hdlr_rx_n_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/about/serial",
        .handler = hdlr_rx_n_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/about/mcudevid",
        .handler = hdlr_rx_n_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/about/mcurev",
        .handler = hdlr_rx_n_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/about/mcufuses",
        .handler = hdlr_rx_n_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/about/fw_ver",
        .handler = hdlr_rx_n_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/link/vita_en",
        .handler = hdlr_rx_n_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/link/iface",
        .handler = hdlr_rx_n_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/link/port",
        .handler = hdlr_rx_n_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/link/ip_dest",
        .handler = hdlr_rx_n_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "n"
                "/link/mac_dest",
        .handler = hdlr_rx_n_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "o",
        .symlink_target = "rx/"
                          "o",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/trigger/sma_mode",
        .handler = hdlr_rx_o_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/trigger/trig_sel",
        .handler = hdlr_rx_o_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_o_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_o_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_o_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_o_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_o_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/pwr",
        .handler = hdlr_rx_o_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/stream",
        .handler = hdlr_rx_o_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/rf/freq/val",
        .handler = hdlr_rx_o_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_o_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/rf/freq/lna",
        .handler = hdlr_rx_o_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/rf/freq/band",
        .handler = hdlr_rx_o_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/rf/gain/val",
        .handler = hdlr_rx_o_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/rf/atten/val",
        .handler = hdlr_rx_o_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/status/rfpll_lock",
        .handler = hdlr_rx_o_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/status/adc_alarm",
        .handler = hdlr_rx_o_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/board/dump",
        .handler = hdlr_rx_o_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/board/test",
        .handler = hdlr_rx_o_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/board/temp",
        .handler = hdlr_rx_o_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/board/led",
        .handler = hdlr_rx_o_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/dsp/signed",
        .handler = hdlr_rx_o_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/dsp/gain",
        .handler = hdlr_rx_o_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/dsp/rate",
        .handler = hdlr_rx_o_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/dsp/nco_adj",
        .handler = hdlr_rx_o_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/dsp/rstreq",
        .handler = hdlr_rx_o_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/dsp/loopback",
        .handler = hdlr_rx_o_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/about/id",
        .handler = hdlr_rx_o_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/about/serial",
        .handler = hdlr_rx_o_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/about/mcudevid",
        .handler = hdlr_rx_o_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/about/mcurev",
        .handler = hdlr_rx_o_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/about/mcufuses",
        .handler = hdlr_rx_o_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/about/fw_ver",
        .handler = hdlr_rx_o_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/link/vita_en",
        .handler = hdlr_rx_o_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/link/iface",
        .handler = hdlr_rx_o_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/link/port",
        .handler = hdlr_rx_o_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/link/ip_dest",
        .handler = hdlr_rx_o_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "o"
                "/link/mac_dest",
        .handler = hdlr_rx_o_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "rx_"
                "p",
        .symlink_target = "rx/"
                          "p",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/trigger/sma_mode",
        .handler = hdlr_rx_p_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/trigger/trig_sel",
        .handler = hdlr_rx_p_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/trigger/edge_backoff",
        .handler = hdlr_rx_p_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/trigger/edge_sample_num",
        .handler = hdlr_rx_p_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/trigger/ufl_mode",
        .handler = hdlr_rx_p_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/trigger/ufl_dir",
        .handler = hdlr_rx_p_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/trigger/ufl_pol",
        .handler = hdlr_rx_p_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/pwr",
        .handler = hdlr_rx_p_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/stream",
        .handler = hdlr_rx_p_stream,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/sync",
        .handler = hdlr_rx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/rf/freq/val",
        .handler = hdlr_rx_p_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/rf/freq/lut_en",
        .handler = hdlr_rx_p_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/rf/freq/lna",
        .handler = hdlr_rx_p_rf_freq_lna,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/rf/freq/band",
        .handler = hdlr_rx_p_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/rf/gain/val",
        .handler = hdlr_rx_p_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/rf/atten/val",
        .handler = hdlr_rx_p_rf_atten_val,
        .permissions = RW,
        .def_val = "127",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/status/rfpll_lock",
        .handler = hdlr_rx_p_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/status/adc_alarm",
        .handler = hdlr_rx_p_status_adcalarm,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/board/dump",
        .handler = hdlr_rx_p_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/board/test",
        .handler = hdlr_rx_p_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/board/temp",
        .handler = hdlr_rx_p_rf_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/board/led",
        .handler = hdlr_rx_p_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/dsp/signed",
        .handler = hdlr_rx_p_dsp_signed,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/dsp/gain",
        .handler = hdlr_rx_p_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/dsp/rate",
        .handler = hdlr_rx_p_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/dsp/nco_adj",
        .handler = hdlr_rx_p_dsp_nco_adj,
        .permissions = RW,
        .def_val = "-15000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/dsp/rstreq",
        .handler = hdlr_rx_p_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/dsp/loopback",
        .handler = hdlr_rx_p_dsp_loopback,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/about/id",
        .handler = hdlr_rx_p_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/about/serial",
        .handler = hdlr_rx_p_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/about/mcudevid",
        .handler = hdlr_rx_p_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/about/mcurev",
        .handler = hdlr_rx_p_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/about/mcufuses",
        .handler = hdlr_rx_p_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/about/fw_ver",
        .handler = hdlr_rx_p_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/link/vita_en",
        .handler = hdlr_rx_p_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/link/iface",
        .handler = hdlr_rx_p_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/link/port",
        .handler = hdlr_rx_p_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/link/ip_dest",
        .handler = hdlr_rx_p_link_ip_dest,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "rx/"
                "p"
                "/link/mac_dest",
        .handler = hdlr_rx_p_link_mac_dest,
        .permissions = RW,
        .def_val = "ff:ff:ff:ff:ff:ff",
    },

    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "a",
        .symlink_target = "tx/"
                          "a",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/trigger/sma_mode",
        .handler = hdlr_tx_a_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/trigger/trig_sel",
        .handler = hdlr_tx_a_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_a_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_a_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_a_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_a_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_a_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/trigger/gating",
        .handler = hdlr_tx_a_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/pwr",
        .handler = hdlr_tx_a_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_a_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_a_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_a_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/rf/dac/nco",
        .handler = hdlr_tx_a_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/rf/dac/temp",
        .handler = hdlr_tx_a_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/rf/freq/val",
        .handler = hdlr_tx_a_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_a_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/rf/freq/band",
        .handler = hdlr_tx_a_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_a_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_a_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/rf/gain/val",
        .handler = hdlr_tx_a_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/status/rfpll_lock",
        .handler = hdlr_tx_a_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/status/dacpll_lock",
        .handler = hdlr_tx_a_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/status/dacpll_centre",
        .handler = hdlr_tx_a_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/board/dump",
        .handler = hdlr_tx_a_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/board/test",
        .handler = hdlr_tx_a_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/board/temp",
        .handler = hdlr_tx_a_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/board/led",
        .handler = hdlr_tx_a_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/dsp/gain",
        .handler = hdlr_tx_a_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/dsp/rate",
        .handler = hdlr_tx_a_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/dsp/nco_adj",
        .handler = hdlr_tx_a_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/dsp/rstreq",
        .handler = hdlr_tx_a_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/about/id",
        .handler = hdlr_tx_a_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/about/serial",
        .handler = hdlr_tx_a_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/about/mcudevid",
        .handler = hdlr_tx_a_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/about/mcurev",
        .handler = hdlr_tx_a_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/about/mcufuses",
        .handler = hdlr_tx_a_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/about/fw_ver",
        .handler = hdlr_tx_a_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/link/vita_en",
        .handler = hdlr_tx_a_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/link/iface",
        .handler = hdlr_tx_a_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/link/port",
        .handler = hdlr_tx_a_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_a_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/qa/oflow",
        .handler = hdlr_tx_a_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "a"
                "/qa/uflow",
        .handler = hdlr_tx_a_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "b",
        .symlink_target = "tx/"
                          "b",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/trigger/sma_mode",
        .handler = hdlr_tx_b_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/trigger/trig_sel",
        .handler = hdlr_tx_b_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_b_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_b_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_b_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_b_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_b_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/trigger/gating",
        .handler = hdlr_tx_b_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/pwr",
        .handler = hdlr_tx_b_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_b_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_b_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_b_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/rf/dac/nco",
        .handler = hdlr_tx_b_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/rf/dac/temp",
        .handler = hdlr_tx_b_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/rf/freq/val",
        .handler = hdlr_tx_b_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_b_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/rf/freq/band",
        .handler = hdlr_tx_b_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_b_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_b_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/rf/gain/val",
        .handler = hdlr_tx_b_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/status/rfpll_lock",
        .handler = hdlr_tx_b_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/status/dacpll_lock",
        .handler = hdlr_tx_b_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/status/dacpll_centre",
        .handler = hdlr_tx_b_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/board/dump",
        .handler = hdlr_tx_b_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/board/test",
        .handler = hdlr_tx_b_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/board/temp",
        .handler = hdlr_tx_b_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/board/led",
        .handler = hdlr_tx_b_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/dsp/gain",
        .handler = hdlr_tx_b_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/dsp/rate",
        .handler = hdlr_tx_b_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/dsp/nco_adj",
        .handler = hdlr_tx_b_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/dsp/rstreq",
        .handler = hdlr_tx_b_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/about/id",
        .handler = hdlr_tx_b_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/about/serial",
        .handler = hdlr_tx_b_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/about/mcudevid",
        .handler = hdlr_tx_b_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/about/mcurev",
        .handler = hdlr_tx_b_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/about/mcufuses",
        .handler = hdlr_tx_b_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/about/fw_ver",
        .handler = hdlr_tx_b_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/link/vita_en",
        .handler = hdlr_tx_b_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/link/iface",
        .handler = hdlr_tx_b_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/link/port",
        .handler = hdlr_tx_b_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_b_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/qa/oflow",
        .handler = hdlr_tx_b_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "b"
                "/qa/uflow",
        .handler = hdlr_tx_b_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "c",
        .symlink_target = "tx/"
                          "c",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/trigger/sma_mode",
        .handler = hdlr_tx_c_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/trigger/trig_sel",
        .handler = hdlr_tx_c_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_c_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_c_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_c_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_c_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_c_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/trigger/gating",
        .handler = hdlr_tx_c_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/pwr",
        .handler = hdlr_tx_c_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_c_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_c_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_c_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/rf/dac/nco",
        .handler = hdlr_tx_c_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/rf/dac/temp",
        .handler = hdlr_tx_c_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/rf/freq/val",
        .handler = hdlr_tx_c_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_c_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/rf/freq/band",
        .handler = hdlr_tx_c_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_c_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_c_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/rf/gain/val",
        .handler = hdlr_tx_c_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/status/rfpll_lock",
        .handler = hdlr_tx_c_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/status/dacpll_lock",
        .handler = hdlr_tx_c_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/status/dacpll_centre",
        .handler = hdlr_tx_c_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/board/dump",
        .handler = hdlr_tx_c_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/board/test",
        .handler = hdlr_tx_c_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/board/temp",
        .handler = hdlr_tx_c_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/board/led",
        .handler = hdlr_tx_c_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/dsp/gain",
        .handler = hdlr_tx_c_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/dsp/rate",
        .handler = hdlr_tx_c_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/dsp/nco_adj",
        .handler = hdlr_tx_c_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/dsp/rstreq",
        .handler = hdlr_tx_c_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/about/id",
        .handler = hdlr_tx_c_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/about/serial",
        .handler = hdlr_tx_c_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/about/mcudevid",
        .handler = hdlr_tx_c_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/about/mcurev",
        .handler = hdlr_tx_c_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/about/mcufuses",
        .handler = hdlr_tx_c_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/about/fw_ver",
        .handler = hdlr_tx_c_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/link/vita_en",
        .handler = hdlr_tx_c_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/link/iface",
        .handler = hdlr_tx_c_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/link/port",
        .handler = hdlr_tx_c_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_c_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/qa/oflow",
        .handler = hdlr_tx_c_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "c"
                "/qa/uflow",
        .handler = hdlr_tx_c_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "d",
        .symlink_target = "tx/"
                          "d",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/trigger/sma_mode",
        .handler = hdlr_tx_d_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/trigger/trig_sel",
        .handler = hdlr_tx_d_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_d_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_d_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_d_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_d_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_d_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/trigger/gating",
        .handler = hdlr_tx_d_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/pwr",
        .handler = hdlr_tx_d_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_d_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_d_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_d_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/rf/dac/nco",
        .handler = hdlr_tx_d_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/rf/dac/temp",
        .handler = hdlr_tx_d_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/rf/freq/val",
        .handler = hdlr_tx_d_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_d_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/rf/freq/band",
        .handler = hdlr_tx_d_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_d_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_d_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/rf/gain/val",
        .handler = hdlr_tx_d_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/status/rfpll_lock",
        .handler = hdlr_tx_d_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/status/dacpll_lock",
        .handler = hdlr_tx_d_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/status/dacpll_centre",
        .handler = hdlr_tx_d_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/board/dump",
        .handler = hdlr_tx_d_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/board/test",
        .handler = hdlr_tx_d_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/board/temp",
        .handler = hdlr_tx_d_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/board/led",
        .handler = hdlr_tx_d_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/dsp/gain",
        .handler = hdlr_tx_d_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/dsp/rate",
        .handler = hdlr_tx_d_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/dsp/nco_adj",
        .handler = hdlr_tx_d_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/dsp/rstreq",
        .handler = hdlr_tx_d_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/about/id",
        .handler = hdlr_tx_d_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/about/serial",
        .handler = hdlr_tx_d_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/about/mcudevid",
        .handler = hdlr_tx_d_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/about/mcurev",
        .handler = hdlr_tx_d_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/about/mcufuses",
        .handler = hdlr_tx_d_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/about/fw_ver",
        .handler = hdlr_tx_d_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/link/vita_en",
        .handler = hdlr_tx_d_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/link/iface",
        .handler = hdlr_tx_d_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/link/port",
        .handler = hdlr_tx_d_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_d_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/qa/oflow",
        .handler = hdlr_tx_d_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "d"
                "/qa/uflow",
        .handler = hdlr_tx_d_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "e",
        .symlink_target = "tx/"
                          "e",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/trigger/sma_mode",
        .handler = hdlr_tx_e_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/trigger/trig_sel",
        .handler = hdlr_tx_e_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_e_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_e_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_e_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_e_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_e_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/trigger/gating",
        .handler = hdlr_tx_e_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/pwr",
        .handler = hdlr_tx_e_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_e_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_e_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_e_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/rf/dac/nco",
        .handler = hdlr_tx_e_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/rf/dac/temp",
        .handler = hdlr_tx_e_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/rf/freq/val",
        .handler = hdlr_tx_e_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_e_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/rf/freq/band",
        .handler = hdlr_tx_e_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_e_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_e_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/rf/gain/val",
        .handler = hdlr_tx_e_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/status/rfpll_lock",
        .handler = hdlr_tx_e_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/status/dacpll_lock",
        .handler = hdlr_tx_e_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/status/dacpll_centre",
        .handler = hdlr_tx_e_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/board/dump",
        .handler = hdlr_tx_e_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/board/test",
        .handler = hdlr_tx_e_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/board/temp",
        .handler = hdlr_tx_e_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/board/led",
        .handler = hdlr_tx_e_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/dsp/gain",
        .handler = hdlr_tx_e_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/dsp/rate",
        .handler = hdlr_tx_e_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/dsp/nco_adj",
        .handler = hdlr_tx_e_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/dsp/rstreq",
        .handler = hdlr_tx_e_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/about/id",
        .handler = hdlr_tx_e_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/about/serial",
        .handler = hdlr_tx_e_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/about/mcudevid",
        .handler = hdlr_tx_e_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/about/mcurev",
        .handler = hdlr_tx_e_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/about/mcufuses",
        .handler = hdlr_tx_e_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/about/fw_ver",
        .handler = hdlr_tx_e_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/link/vita_en",
        .handler = hdlr_tx_e_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/link/iface",
        .handler = hdlr_tx_e_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/link/port",
        .handler = hdlr_tx_e_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_e_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/qa/oflow",
        .handler = hdlr_tx_e_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "e"
                "/qa/uflow",
        .handler = hdlr_tx_e_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "f",
        .symlink_target = "tx/"
                          "f",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/trigger/sma_mode",
        .handler = hdlr_tx_f_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/trigger/trig_sel",
        .handler = hdlr_tx_f_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_f_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_f_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_f_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_f_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_f_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/trigger/gating",
        .handler = hdlr_tx_f_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/pwr",
        .handler = hdlr_tx_f_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_f_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_f_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_f_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/rf/dac/nco",
        .handler = hdlr_tx_f_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/rf/dac/temp",
        .handler = hdlr_tx_f_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/rf/freq/val",
        .handler = hdlr_tx_f_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_f_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/rf/freq/band",
        .handler = hdlr_tx_f_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_f_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_f_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/rf/gain/val",
        .handler = hdlr_tx_f_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/status/rfpll_lock",
        .handler = hdlr_tx_f_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/status/dacpll_lock",
        .handler = hdlr_tx_f_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/status/dacpll_centre",
        .handler = hdlr_tx_f_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/board/dump",
        .handler = hdlr_tx_f_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/board/test",
        .handler = hdlr_tx_f_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/board/temp",
        .handler = hdlr_tx_f_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/board/led",
        .handler = hdlr_tx_f_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/dsp/gain",
        .handler = hdlr_tx_f_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/dsp/rate",
        .handler = hdlr_tx_f_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/dsp/nco_adj",
        .handler = hdlr_tx_f_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/dsp/rstreq",
        .handler = hdlr_tx_f_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/about/id",
        .handler = hdlr_tx_f_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/about/serial",
        .handler = hdlr_tx_f_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/about/mcudevid",
        .handler = hdlr_tx_f_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/about/mcurev",
        .handler = hdlr_tx_f_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/about/mcufuses",
        .handler = hdlr_tx_f_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/about/fw_ver",
        .handler = hdlr_tx_f_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/link/vita_en",
        .handler = hdlr_tx_f_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/link/iface",
        .handler = hdlr_tx_f_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/link/port",
        .handler = hdlr_tx_f_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_f_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/qa/oflow",
        .handler = hdlr_tx_f_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "f"
                "/qa/uflow",
        .handler = hdlr_tx_f_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "g",
        .symlink_target = "tx/"
                          "g",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/trigger/sma_mode",
        .handler = hdlr_tx_g_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/trigger/trig_sel",
        .handler = hdlr_tx_g_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_g_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_g_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_g_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_g_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_g_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/trigger/gating",
        .handler = hdlr_tx_g_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/pwr",
        .handler = hdlr_tx_g_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_g_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_g_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_g_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/rf/dac/nco",
        .handler = hdlr_tx_g_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/rf/dac/temp",
        .handler = hdlr_tx_g_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/rf/freq/val",
        .handler = hdlr_tx_g_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_g_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/rf/freq/band",
        .handler = hdlr_tx_g_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_g_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_g_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/rf/gain/val",
        .handler = hdlr_tx_g_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/status/rfpll_lock",
        .handler = hdlr_tx_g_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/status/dacpll_lock",
        .handler = hdlr_tx_g_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/status/dacpll_centre",
        .handler = hdlr_tx_g_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/board/dump",
        .handler = hdlr_tx_g_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/board/test",
        .handler = hdlr_tx_g_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/board/temp",
        .handler = hdlr_tx_g_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/board/led",
        .handler = hdlr_tx_g_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/dsp/gain",
        .handler = hdlr_tx_g_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/dsp/rate",
        .handler = hdlr_tx_g_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/dsp/nco_adj",
        .handler = hdlr_tx_g_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/dsp/rstreq",
        .handler = hdlr_tx_g_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/about/id",
        .handler = hdlr_tx_g_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/about/serial",
        .handler = hdlr_tx_g_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/about/mcudevid",
        .handler = hdlr_tx_g_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/about/mcurev",
        .handler = hdlr_tx_g_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/about/mcufuses",
        .handler = hdlr_tx_g_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/about/fw_ver",
        .handler = hdlr_tx_g_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/link/vita_en",
        .handler = hdlr_tx_g_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/link/iface",
        .handler = hdlr_tx_g_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/link/port",
        .handler = hdlr_tx_g_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_g_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/qa/oflow",
        .handler = hdlr_tx_g_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "g"
                "/qa/uflow",
        .handler = hdlr_tx_g_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "h",
        .symlink_target = "tx/"
                          "h",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/trigger/sma_mode",
        .handler = hdlr_tx_h_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/trigger/trig_sel",
        .handler = hdlr_tx_h_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_h_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_h_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_h_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_h_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_h_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/trigger/gating",
        .handler = hdlr_tx_h_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/pwr",
        .handler = hdlr_tx_h_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_h_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_h_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_h_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/rf/dac/nco",
        .handler = hdlr_tx_h_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/rf/dac/temp",
        .handler = hdlr_tx_h_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/rf/freq/val",
        .handler = hdlr_tx_h_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_h_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/rf/freq/band",
        .handler = hdlr_tx_h_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_h_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_h_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/rf/gain/val",
        .handler = hdlr_tx_h_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/status/rfpll_lock",
        .handler = hdlr_tx_h_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/status/dacpll_lock",
        .handler = hdlr_tx_h_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/status/dacpll_centre",
        .handler = hdlr_tx_h_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/board/dump",
        .handler = hdlr_tx_h_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/board/test",
        .handler = hdlr_tx_h_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/board/temp",
        .handler = hdlr_tx_h_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/board/led",
        .handler = hdlr_tx_h_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/dsp/gain",
        .handler = hdlr_tx_h_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/dsp/rate",
        .handler = hdlr_tx_h_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/dsp/nco_adj",
        .handler = hdlr_tx_h_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/dsp/rstreq",
        .handler = hdlr_tx_h_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/about/id",
        .handler = hdlr_tx_h_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/about/serial",
        .handler = hdlr_tx_h_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/about/mcudevid",
        .handler = hdlr_tx_h_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/about/mcurev",
        .handler = hdlr_tx_h_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/about/mcufuses",
        .handler = hdlr_tx_h_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/about/fw_ver",
        .handler = hdlr_tx_h_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/link/vita_en",
        .handler = hdlr_tx_h_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/link/iface",
        .handler = hdlr_tx_h_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/link/port",
        .handler = hdlr_tx_h_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_h_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/qa/oflow",
        .handler = hdlr_tx_h_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "h"
                "/qa/uflow",
        .handler = hdlr_tx_h_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "i",
        .symlink_target = "tx/"
                          "i",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/trigger/sma_mode",
        .handler = hdlr_tx_i_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/trigger/trig_sel",
        .handler = hdlr_tx_i_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_i_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_i_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_i_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_i_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_i_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/trigger/gating",
        .handler = hdlr_tx_i_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/pwr",
        .handler = hdlr_tx_i_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_i_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_i_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_i_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/rf/dac/nco",
        .handler = hdlr_tx_i_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/rf/dac/temp",
        .handler = hdlr_tx_i_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/rf/freq/val",
        .handler = hdlr_tx_i_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_i_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/rf/freq/band",
        .handler = hdlr_tx_i_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_i_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_i_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/rf/gain/val",
        .handler = hdlr_tx_i_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/status/rfpll_lock",
        .handler = hdlr_tx_i_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/status/dacpll_lock",
        .handler = hdlr_tx_i_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/status/dacpll_centre",
        .handler = hdlr_tx_i_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/board/dump",
        .handler = hdlr_tx_i_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/board/test",
        .handler = hdlr_tx_i_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/board/temp",
        .handler = hdlr_tx_i_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/board/led",
        .handler = hdlr_tx_i_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/dsp/gain",
        .handler = hdlr_tx_i_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/dsp/rate",
        .handler = hdlr_tx_i_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/dsp/nco_adj",
        .handler = hdlr_tx_i_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/dsp/rstreq",
        .handler = hdlr_tx_i_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/about/id",
        .handler = hdlr_tx_i_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/about/serial",
        .handler = hdlr_tx_i_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/about/mcudevid",
        .handler = hdlr_tx_i_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/about/mcurev",
        .handler = hdlr_tx_i_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/about/mcufuses",
        .handler = hdlr_tx_i_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/about/fw_ver",
        .handler = hdlr_tx_i_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/link/vita_en",
        .handler = hdlr_tx_i_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/link/iface",
        .handler = hdlr_tx_i_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/link/port",
        .handler = hdlr_tx_i_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_i_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/qa/oflow",
        .handler = hdlr_tx_i_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "i"
                "/qa/uflow",
        .handler = hdlr_tx_i_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "j",
        .symlink_target = "tx/"
                          "j",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/trigger/sma_mode",
        .handler = hdlr_tx_j_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/trigger/trig_sel",
        .handler = hdlr_tx_j_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_j_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_j_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_j_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_j_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_j_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/trigger/gating",
        .handler = hdlr_tx_j_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/pwr",
        .handler = hdlr_tx_j_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_j_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_j_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_j_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/rf/dac/nco",
        .handler = hdlr_tx_j_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/rf/dac/temp",
        .handler = hdlr_tx_j_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/rf/freq/val",
        .handler = hdlr_tx_j_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_j_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/rf/freq/band",
        .handler = hdlr_tx_j_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_j_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_j_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/rf/gain/val",
        .handler = hdlr_tx_j_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/status/rfpll_lock",
        .handler = hdlr_tx_j_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/status/dacpll_lock",
        .handler = hdlr_tx_j_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/status/dacpll_centre",
        .handler = hdlr_tx_j_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/board/dump",
        .handler = hdlr_tx_j_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/board/test",
        .handler = hdlr_tx_j_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/board/temp",
        .handler = hdlr_tx_j_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/board/led",
        .handler = hdlr_tx_j_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/dsp/gain",
        .handler = hdlr_tx_j_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/dsp/rate",
        .handler = hdlr_tx_j_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/dsp/nco_adj",
        .handler = hdlr_tx_j_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/dsp/rstreq",
        .handler = hdlr_tx_j_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/about/id",
        .handler = hdlr_tx_j_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/about/serial",
        .handler = hdlr_tx_j_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/about/mcudevid",
        .handler = hdlr_tx_j_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/about/mcurev",
        .handler = hdlr_tx_j_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/about/mcufuses",
        .handler = hdlr_tx_j_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/about/fw_ver",
        .handler = hdlr_tx_j_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/link/vita_en",
        .handler = hdlr_tx_j_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/link/iface",
        .handler = hdlr_tx_j_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/link/port",
        .handler = hdlr_tx_j_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_j_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/qa/oflow",
        .handler = hdlr_tx_j_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "j"
                "/qa/uflow",
        .handler = hdlr_tx_j_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "k",
        .symlink_target = "tx/"
                          "k",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/trigger/sma_mode",
        .handler = hdlr_tx_k_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/trigger/trig_sel",
        .handler = hdlr_tx_k_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_k_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_k_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_k_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_k_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_k_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/trigger/gating",
        .handler = hdlr_tx_k_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/pwr",
        .handler = hdlr_tx_k_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_k_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_k_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_k_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/rf/dac/nco",
        .handler = hdlr_tx_k_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/rf/dac/temp",
        .handler = hdlr_tx_k_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/rf/freq/val",
        .handler = hdlr_tx_k_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_k_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/rf/freq/band",
        .handler = hdlr_tx_k_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_k_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_k_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/rf/gain/val",
        .handler = hdlr_tx_k_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/status/rfpll_lock",
        .handler = hdlr_tx_k_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/status/dacpll_lock",
        .handler = hdlr_tx_k_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/status/dacpll_centre",
        .handler = hdlr_tx_k_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/board/dump",
        .handler = hdlr_tx_k_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/board/test",
        .handler = hdlr_tx_k_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/board/temp",
        .handler = hdlr_tx_k_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/board/led",
        .handler = hdlr_tx_k_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/dsp/gain",
        .handler = hdlr_tx_k_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/dsp/rate",
        .handler = hdlr_tx_k_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/dsp/nco_adj",
        .handler = hdlr_tx_k_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/dsp/rstreq",
        .handler = hdlr_tx_k_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/about/id",
        .handler = hdlr_tx_k_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/about/serial",
        .handler = hdlr_tx_k_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/about/mcudevid",
        .handler = hdlr_tx_k_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/about/mcurev",
        .handler = hdlr_tx_k_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/about/mcufuses",
        .handler = hdlr_tx_k_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/about/fw_ver",
        .handler = hdlr_tx_k_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/link/vita_en",
        .handler = hdlr_tx_k_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/link/iface",
        .handler = hdlr_tx_k_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/link/port",
        .handler = hdlr_tx_k_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_k_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/qa/oflow",
        .handler = hdlr_tx_k_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "k"
                "/qa/uflow",
        .handler = hdlr_tx_k_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "l",
        .symlink_target = "tx/"
                          "l",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/trigger/sma_mode",
        .handler = hdlr_tx_l_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/trigger/trig_sel",
        .handler = hdlr_tx_l_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_l_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_l_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_l_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_l_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_l_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/trigger/gating",
        .handler = hdlr_tx_l_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/pwr",
        .handler = hdlr_tx_l_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_l_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_l_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_l_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/rf/dac/nco",
        .handler = hdlr_tx_l_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/rf/dac/temp",
        .handler = hdlr_tx_l_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/rf/freq/val",
        .handler = hdlr_tx_l_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_l_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/rf/freq/band",
        .handler = hdlr_tx_l_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_l_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_l_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/rf/gain/val",
        .handler = hdlr_tx_l_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/status/rfpll_lock",
        .handler = hdlr_tx_l_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/status/dacpll_lock",
        .handler = hdlr_tx_l_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/status/dacpll_centre",
        .handler = hdlr_tx_l_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/board/dump",
        .handler = hdlr_tx_l_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/board/test",
        .handler = hdlr_tx_l_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/board/temp",
        .handler = hdlr_tx_l_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/board/led",
        .handler = hdlr_tx_l_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/dsp/gain",
        .handler = hdlr_tx_l_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/dsp/rate",
        .handler = hdlr_tx_l_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/dsp/nco_adj",
        .handler = hdlr_tx_l_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/dsp/rstreq",
        .handler = hdlr_tx_l_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/about/id",
        .handler = hdlr_tx_l_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/about/serial",
        .handler = hdlr_tx_l_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/about/mcudevid",
        .handler = hdlr_tx_l_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/about/mcurev",
        .handler = hdlr_tx_l_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/about/mcufuses",
        .handler = hdlr_tx_l_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/about/fw_ver",
        .handler = hdlr_tx_l_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/link/vita_en",
        .handler = hdlr_tx_l_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/link/iface",
        .handler = hdlr_tx_l_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/link/port",
        .handler = hdlr_tx_l_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_l_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/qa/oflow",
        .handler = hdlr_tx_l_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "l"
                "/qa/uflow",
        .handler = hdlr_tx_l_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "m",
        .symlink_target = "tx/"
                          "m",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/trigger/sma_mode",
        .handler = hdlr_tx_m_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/trigger/trig_sel",
        .handler = hdlr_tx_m_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_m_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_m_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_m_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_m_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_m_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/trigger/gating",
        .handler = hdlr_tx_m_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/pwr",
        .handler = hdlr_tx_m_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_m_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_m_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_m_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/rf/dac/nco",
        .handler = hdlr_tx_m_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/rf/dac/temp",
        .handler = hdlr_tx_m_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/rf/freq/val",
        .handler = hdlr_tx_m_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_m_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/rf/freq/band",
        .handler = hdlr_tx_m_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_m_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_m_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/rf/gain/val",
        .handler = hdlr_tx_m_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/status/rfpll_lock",
        .handler = hdlr_tx_m_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/status/dacpll_lock",
        .handler = hdlr_tx_m_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/status/dacpll_centre",
        .handler = hdlr_tx_m_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/board/dump",
        .handler = hdlr_tx_m_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/board/test",
        .handler = hdlr_tx_m_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/board/temp",
        .handler = hdlr_tx_m_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/board/led",
        .handler = hdlr_tx_m_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/dsp/gain",
        .handler = hdlr_tx_m_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/dsp/rate",
        .handler = hdlr_tx_m_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/dsp/nco_adj",
        .handler = hdlr_tx_m_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/dsp/rstreq",
        .handler = hdlr_tx_m_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/about/id",
        .handler = hdlr_tx_m_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/about/serial",
        .handler = hdlr_tx_m_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/about/mcudevid",
        .handler = hdlr_tx_m_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/about/mcurev",
        .handler = hdlr_tx_m_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/about/mcufuses",
        .handler = hdlr_tx_m_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/about/fw_ver",
        .handler = hdlr_tx_m_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/link/vita_en",
        .handler = hdlr_tx_m_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/link/iface",
        .handler = hdlr_tx_m_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/link/port",
        .handler = hdlr_tx_m_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_m_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/qa/oflow",
        .handler = hdlr_tx_m_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "m"
                "/qa/uflow",
        .handler = hdlr_tx_m_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "n",
        .symlink_target = "tx/"
                          "n",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/trigger/sma_mode",
        .handler = hdlr_tx_n_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/trigger/trig_sel",
        .handler = hdlr_tx_n_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_n_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_n_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_n_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_n_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_n_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/trigger/gating",
        .handler = hdlr_tx_n_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/pwr",
        .handler = hdlr_tx_n_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_n_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_n_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_n_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/rf/dac/nco",
        .handler = hdlr_tx_n_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/rf/dac/temp",
        .handler = hdlr_tx_n_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/rf/freq/val",
        .handler = hdlr_tx_n_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_n_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/rf/freq/band",
        .handler = hdlr_tx_n_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_n_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_n_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/rf/gain/val",
        .handler = hdlr_tx_n_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/status/rfpll_lock",
        .handler = hdlr_tx_n_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/status/dacpll_lock",
        .handler = hdlr_tx_n_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/status/dacpll_centre",
        .handler = hdlr_tx_n_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/board/dump",
        .handler = hdlr_tx_n_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/board/test",
        .handler = hdlr_tx_n_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/board/temp",
        .handler = hdlr_tx_n_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/board/led",
        .handler = hdlr_tx_n_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/dsp/gain",
        .handler = hdlr_tx_n_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/dsp/rate",
        .handler = hdlr_tx_n_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/dsp/nco_adj",
        .handler = hdlr_tx_n_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/dsp/rstreq",
        .handler = hdlr_tx_n_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/about/id",
        .handler = hdlr_tx_n_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/about/serial",
        .handler = hdlr_tx_n_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/about/mcudevid",
        .handler = hdlr_tx_n_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/about/mcurev",
        .handler = hdlr_tx_n_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/about/mcufuses",
        .handler = hdlr_tx_n_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/about/fw_ver",
        .handler = hdlr_tx_n_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/link/vita_en",
        .handler = hdlr_tx_n_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/link/iface",
        .handler = hdlr_tx_n_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/link/port",
        .handler = hdlr_tx_n_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_n_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/qa/oflow",
        .handler = hdlr_tx_n_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "n"
                "/qa/uflow",
        .handler = hdlr_tx_n_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "o",
        .symlink_target = "tx/"
                          "o",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/trigger/sma_mode",
        .handler = hdlr_tx_o_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/trigger/trig_sel",
        .handler = hdlr_tx_o_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_o_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_o_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_o_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_o_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_o_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/trigger/gating",
        .handler = hdlr_tx_o_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/pwr",
        .handler = hdlr_tx_o_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_o_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_o_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_o_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/rf/dac/nco",
        .handler = hdlr_tx_o_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/rf/dac/temp",
        .handler = hdlr_tx_o_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/rf/freq/val",
        .handler = hdlr_tx_o_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_o_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/rf/freq/band",
        .handler = hdlr_tx_o_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_o_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_o_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/rf/gain/val",
        .handler = hdlr_tx_o_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/status/rfpll_lock",
        .handler = hdlr_tx_o_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/status/dacpll_lock",
        .handler = hdlr_tx_o_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/status/dacpll_centre",
        .handler = hdlr_tx_o_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/board/dump",
        .handler = hdlr_tx_o_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/board/test",
        .handler = hdlr_tx_o_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/board/temp",
        .handler = hdlr_tx_o_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/board/led",
        .handler = hdlr_tx_o_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/dsp/gain",
        .handler = hdlr_tx_o_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/dsp/rate",
        .handler = hdlr_tx_o_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/dsp/nco_adj",
        .handler = hdlr_tx_o_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/dsp/rstreq",
        .handler = hdlr_tx_o_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/about/id",
        .handler = hdlr_tx_o_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/about/serial",
        .handler = hdlr_tx_o_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/about/mcudevid",
        .handler = hdlr_tx_o_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/about/mcurev",
        .handler = hdlr_tx_o_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/about/mcufuses",
        .handler = hdlr_tx_o_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/about/fw_ver",
        .handler = hdlr_tx_o_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/link/vita_en",
        .handler = hdlr_tx_o_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/link/iface",
        .handler = hdlr_tx_o_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/link/port",
        .handler = hdlr_tx_o_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_o_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/qa/oflow",
        .handler = hdlr_tx_o_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "o"
                "/qa/uflow",
        .handler = hdlr_tx_o_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_SYMLINK,
        .path = "tx_"
                "p",
        .symlink_target = "tx/"
                          "p",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/trigger/sma_mode",
        .handler = hdlr_tx_p_trigger_sma_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/trigger/trig_sel",
        .handler = hdlr_tx_p_trigger_trig_sel,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/trigger/edge_backoff",
        .handler = hdlr_tx_p_trigger_edge_backoff,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/trigger/edge_sample_num",
        .handler = hdlr_tx_p_trigger_edge_sample_num,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/trigger/ufl_dir",
        .handler = hdlr_tx_p_trigger_ufl_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/trigger/ufl_mode",
        .handler = hdlr_tx_p_trigger_ufl_mode,
        .permissions = RW,
        .def_val = "level",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/trigger/ufl_pol",
        .handler = hdlr_tx_p_trigger_ufl_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/trigger/gating",
        .handler = hdlr_tx_p_trigger_gating,
        .permissions = RW,
        .def_val = "output",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/pwr",
        .handler = hdlr_tx_p_pwr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/sync",
        .handler = hdlr_tx_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/rf/dac/dither_en",
        .handler = hdlr_tx_p_rf_dac_dither_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/rf/dac/dither_mixer_en",
        .handler = hdlr_tx_p_rf_dac_dither_mixer_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/rf/dac/dither_sra_sel",
        .handler = hdlr_tx_p_rf_dac_dither_sra_sel,
        .permissions = RW,
        .def_val = "6",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/rf/dac/nco",
        .handler = hdlr_tx_p_rf_dac_nco,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/rf/dac/temp",
        .handler = hdlr_tx_p_rf_dac_temp,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/rf/freq/val",
        .handler = hdlr_tx_p_rf_freq_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/rf/freq/lut_en",
        .handler = hdlr_tx_p_rf_freq_lut_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/rf/freq/band",
        .handler = hdlr_tx_p_rf_freq_band,
        .permissions = RW,
        .def_val = "1",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/rf/freq/i_bias",
        .handler = hdlr_tx_p_rf_freq_i_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/rf/freq/q_bias",
        .handler = hdlr_tx_p_rf_freq_q_bias,
        .permissions = RW,
        .def_val = "17",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/rf/gain/val",
        .handler = hdlr_tx_p_rf_gain_val,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/status/rfpll_lock",
        .handler = hdlr_tx_p_status_rfld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/status/dacpll_lock",
        .handler = hdlr_tx_p_status_dacld,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/status/dacpll_centre",
        .handler = hdlr_tx_p_status_dacctr,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/board/dump",
        .handler = hdlr_tx_p_rf_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/board/test",
        .handler = hdlr_tx_p_rf_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/board/temp",
        .handler = hdlr_tx_p_rf_board_temp,
        .permissions = RW,
        .def_val = "23",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/board/led",
        .handler = hdlr_tx_p_rf_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/dsp/gain",
        .handler = hdlr_tx_p_dsp_gain,
        .permissions = RW,
        .def_val = "10",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/dsp/rate",
        .handler = hdlr_tx_p_dsp_rate,
        .permissions = RW,
        .def_val = "1258850",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/dsp/nco_adj",
        .handler = hdlr_tx_p_dsp_nco_adj,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/dsp/rstreq",
        .handler = hdlr_tx_p_dsp_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/about/id",
        .handler = hdlr_tx_p_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/about/serial",
        .handler = hdlr_tx_p_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/about/mcudevid",
        .handler = hdlr_tx_p_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/about/mcurev",
        .handler = hdlr_tx_p_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/about/mcufuses",
        .handler = hdlr_tx_p_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/about/fw_ver",
        .handler = hdlr_tx_p_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/link/vita_en",
        .handler = hdlr_tx_p_link_vita_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/link/iface",
        .handler = hdlr_tx_p_link_iface,
        .permissions = RW,
        .def_val = "sfpa",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/link/port",
        .handler = hdlr_tx_p_link_port,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/qa/fifo_lvl",
        .handler = hdlr_tx_p_qa_fifo_lvl,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/qa/oflow",
        .handler = hdlr_tx_p_qa_oflow,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "tx/"
                "p"
                "/qa/uflow",
        .handler = hdlr_tx_p_qa_uflow,
        .permissions = RW,
        .def_val = "0",
    },

    {
        .type = PROP_TYPE_FILE,
        .path = "time/clk/pps",
        .handler = hdlr_time_clk_pps,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/clk/cur_time",
        .handler = hdlr_time_clk_cur_time,
        .permissions = RW,
        .def_val = "0.0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/status/lmk_lockdetect",
        .handler = hdlr_time_status_ld,
        .permissions = RW,
        .def_val = "unlocked",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/status/lmk_lossoflock",
        .handler = hdlr_time_status_lol,
        .permissions = RW,
        .def_val = "unlocked",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/status/lmk_lockdetect_jesd_pll1",
        .handler = hdlr_time_status_ld_jesd_pll1,
        .permissions = RW,
        .def_val = "unlocked",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/status/lmk_lockdetect_jesd_pll2",
        .handler = hdlr_time_status_ld_jesd_pll2,
        .permissions = RW,
        .def_val = "unlocked",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/status/lmk_lockdetect_pll_pll1",
        .handler = hdlr_time_status_ld_pll_pll1,
        .permissions = RW,
        .def_val = "unlocked",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/status/lmk_lockdetect_pll_pll2",
        .handler = hdlr_time_status_ld_pll_pll2,
        .permissions = RW,
        .def_val = "unlocked",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/status/lmk_lossoflock_jesd_pll1",
        .handler = hdlr_time_status_lol_jesd_pll1,
        .permissions = RW,
        .def_val = "unlocked",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/status/lmk_lossoflock_jesd_pll2",
        .handler = hdlr_time_status_lol_jesd_pll2,
        .permissions = RW,
        .def_val = "unlocked",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/status/lmk_lossoflock_pll_pll1",
        .handler = hdlr_time_status_lol_pll_pll1,
        .permissions = RW,
        .def_val = "unlocked",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/status/lmk_lossoflock_pll_pll2",
        .handler = hdlr_time_status_lol_pll_pll2,
        .permissions = RW,
        .def_val = "unlocked",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/source/ref",
        .handler = hdlr_time_source_ref,
        .permissions = RW,
        .def_val = "internal",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/source/extsine",
        .handler = hdlr_time_source_extsine,
        .permissions = RW,
        .def_val = "sine",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/sync/lmk_sync_tgl_jesd",
        .handler = hdlr_time_sync_lmk_sync_tgl_jesd,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/sync/lmk_sync_tgl_pll",
        .handler = hdlr_time_sync_lmk_sync_tgl_pll,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/sync/lmk_sync_resync_jesd",
        .handler = hdlr_time_sync_lmk_resync_jesd,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/sync/lmk_sync_resync_pll",
        .handler = hdlr_time_sync_lmk_resync_pll,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/sync/lmk_resync_all",
        .handler = hdlr_time_sync_lmk_resync_all,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/board/dump",
        .handler = hdlr_time_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/board/test",
        .handler = hdlr_time_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/board/temp",
        .handler = hdlr_time_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/board/led",
        .handler = hdlr_time_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/about/id",
        .handler = hdlr_time_about_id,
        .permissions = RO,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/about/serial",
        .handler = hdlr_time_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/about/mcudevid",
        .handler = hdlr_time_about_mcudevid,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/about/mcurev",
        .handler = hdlr_time_about_mcurev,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/about/mcufuses",
        .handler = hdlr_time_about_mcufuses,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/about/fw_ver",
        .handler = hdlr_time_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "time/about/sw_ver",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/trigger/sma_dir",
        .handler = hdlr_fpga_trigger_sma_dir,
        .permissions = RW,
        .def_val = "out",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/trigger/sma_pol",
        .handler = hdlr_fpga_trigger_sma_pol,
        .permissions = RW,
        .def_val = "negative",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/about/fw_ver",
        .handler = hdlr_fpga_about_fw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/about/server_ver",
        .handler = hdlr_server_about_fw_ver,
        .permissions = RW,
        .def_val = "",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/about/hw_ver",
        .handler = hdlr_fpga_about_hw_ver,
        .permissions = RW,
        .def_val = VERSION,
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/about/id",
        .handler = hdlr_fpga_about_id,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/about/name",
        .handler = hdlr_invalid,
        .permissions = RO,
        .def_val = "crimson_tng",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/about/serial",
        .handler = hdlr_fpga_about_serial,
        .permissions = RW,
        .def_val = "001",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/about/cmp_time",
        .handler = hdlr_fpga_about_cmp_time,
        .permissions = RW,
        .def_val = "yyyy-mm-dd-hh-mm",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/about/conf_info",
        .handler = hdlr_fpga_about_conf_info,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/dump",
        .handler = hdlr_fpga_board_dump,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/fw_rst",
        .handler = hdlr_fpga_board_fw_rst,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/flow_control/sfpa_port",
        .handler = hdlr_fpga_board_flow_control_sfpa_port,
        .permissions = RW,
        .def_val = "42809",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/flow_control/sfpb_port",
        .handler = hdlr_fpga_board_flow_control_sfpb_port,
        .permissions = RW,
        .def_val = "42809",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/gps_time",
        .handler = hdlr_fpga_board_gps_time,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/gps_frac_time",
        .handler = hdlr_fpga_board_gps_frac_time,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/gps_sync_time",
        .handler = hdlr_fpga_board_gps_sync_time,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/jesd_sync",
        .handler = hdlr_fpga_board_jesd_sync,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/led",
        .handler = hdlr_fpga_board_led,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/rstreq",
        .handler = hdlr_fpga_board_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/reboot",
        .handler = hdlr_fpga_board_reboot,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/sys_rstreq",
        .handler = hdlr_fpga_board_sys_rstreq,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/test",
        .handler = hdlr_fpga_board_test,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/temp",
        .handler = hdlr_fpga_board_temp,
        .permissions = RW,
        .def_val = "20",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/board/gle",
        .handler = hdlr_fpga_board_gle,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/link/rate",
        .handler = hdlr_fpga_link_rate,
        .permissions = RW,
        .def_val = "1250000000",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/link/sfpa/ip_addr",
        .handler = hdlr_fpga_link_sfpa_ip_addr,
        .permissions = RW,
        .def_val = "10.10.10.2",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/link/sfpa/mac_addr",
        .handler = hdlr_fpga_link_sfpa_mac_addr,
        .permissions = RW,
        .def_val = "aa:00:00:00:00:00",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/link/sfpa/ver",
        .handler = hdlr_fpga_link_sfpa_ver,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/link/sfpa/pay_len",
        .handler = hdlr_fpga_link_sfpa_pay_len,
        .permissions = RW,
        .def_val = "1400",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/link/sfpb/ip_addr",
        .handler = hdlr_fpga_link_sfpb_ip_addr,
        .permissions = RW,
        .def_val = "10.10.11.2",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/link/sfpb/mac_addr",
        .handler = hdlr_fpga_link_sfpb_mac_addr,
        .permissions = RW,
        .def_val = "aa:00:00:00:00:01",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/link/sfpb/ver",
        .handler = hdlr_fpga_link_sfpb_ver,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/link/sfpb/pay_len",
        .handler = hdlr_fpga_link_sfpb_pay_len,
        .permissions = RW,
        .def_val = "1400",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/link/net/dhcp_en",
        .handler = hdlr_fpga_link_net_dhcp_en,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/link/net/hostname",
        .handler = hdlr_fpga_link_net_hostname,
        .permissions = RW,
        .def_val = "crimson_tng",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "fpga/link/net/ip_addr",
        .handler = hdlr_fpga_link_net_ip_addr,
        .permissions = RW,
        .def_val = "192.168.10.2",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "save_config",
        .handler = hdlr_save_config,
        .permissions = RW,
        .def_val = "/home/root/profile.cfg",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "load_config",
        .handler = hdlr_load_config,
        .permissions = RW,
        .def_val = "/home/root/profile.cfg",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "cm/chanmask-rx",
        .handler = hdlr_cm_chanmask_rx,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "cm/chanmask-tx",
        .handler = hdlr_cm_chanmask_tx,
        .permissions = RW,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "cm/rx/atten/val",
        .handler = hdlr_cm_rx_atten_val,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "cm/rx/gain/val",
        .handler = hdlr_cm_rx_gain_val,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "cm/tx/gain/val",
        .handler = hdlr_cm_tx_gain_val,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "cm/trx/freq/val",
        .handler = hdlr_cm_trx_freq_val,
        .permissions = WO,
        .def_val = "0",
    },
    {
        .type = PROP_TYPE_FILE,
        .path = "cm/trx/nco_adj",
        .handler = hdlr_cm_trx_nco_adj,
        .permissions = WO,
        .def_val = "0",
    },
};

static const size_t num_properties =
    ((int)sizeof(property_table) / sizeof(*property_table));

const char *tostr(const int num) {
  char *str = calloc(32, sizeof(*str));
  sprintf(str, "%d", num);
  return str;
}
# 3087 "properties.c"
void table_patch(void) {
  const int base = 42820;
  const int offset = ((int)sizeof(names) / sizeof(*names));

  set_property("rx/"
               "a"
               "/link/port",
               tostr(base + ((int)("a"[0] - 'a'))));
  set_property("rx/"
               "b"
               "/link/port",
               tostr(base + ((int)("b"[0] - 'a'))));
  set_property("rx/"
               "c"
               "/link/port",
               tostr(base + ((int)("c"[0] - 'a'))));
  set_property("rx/"
               "d"
               "/link/port",
               tostr(base + ((int)("d"[0] - 'a'))));
  set_property("rx/"
               "e"
               "/link/port",
               tostr(base + ((int)("e"[0] - 'a'))));
  set_property("rx/"
               "f"
               "/link/port",
               tostr(base + ((int)("f"[0] - 'a'))));
  set_property("rx/"
               "g"
               "/link/port",
               tostr(base + ((int)("g"[0] - 'a'))));
  set_property("rx/"
               "h"
               "/link/port",
               tostr(base + ((int)("h"[0] - 'a'))));
  set_property("rx/"
               "i"
               "/link/port",
               tostr(base + ((int)("i"[0] - 'a'))));
  set_property("rx/"
               "j"
               "/link/port",
               tostr(base + ((int)("j"[0] - 'a'))));
  set_property("rx/"
               "k"
               "/link/port",
               tostr(base + ((int)("k"[0] - 'a'))));
  set_property("rx/"
               "l"
               "/link/port",
               tostr(base + ((int)("l"[0] - 'a'))));
  set_property("rx/"
               "m"
               "/link/port",
               tostr(base + ((int)("m"[0] - 'a'))));
  set_property("rx/"
               "n"
               "/link/port",
               tostr(base + ((int)("n"[0] - 'a'))));
  set_property("rx/"
               "o"
               "/link/port",
               tostr(base + ((int)("o"[0] - 'a'))));
  set_property("rx/"
               "p"
               "/link/port",
               tostr(base + ((int)("p"[0] - 'a'))));

  set_property("rx/"
               "a"
               "/link/ip_dest",
               ((int)("a"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "b"
               "/link/ip_dest",
               ((int)("b"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "c"
               "/link/ip_dest",
               ((int)("c"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "d"
               "/link/ip_dest",
               ((int)("d"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "e"
               "/link/ip_dest",
               ((int)("e"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "f"
               "/link/ip_dest",
               ((int)("f"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "g"
               "/link/ip_dest",
               ((int)("g"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "h"
               "/link/ip_dest",
               ((int)("h"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "i"
               "/link/ip_dest",
               ((int)("i"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "j"
               "/link/ip_dest",
               ((int)("j"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "k"
               "/link/ip_dest",
               ((int)("k"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "l"
               "/link/ip_dest",
               ((int)("l"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "m"
               "/link/ip_dest",
               ((int)("m"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "n"
               "/link/ip_dest",
               ((int)("n"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "o"
               "/link/ip_dest",
               ((int)("o"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
  set_property("rx/"
               "p"
               "/link/ip_dest",
               ((int)("p"[0] - 'a')) % 2 == 0 ? "10.10.10.10" : "10.10.11.10");
# 3109 "properties.c"
  set_property("tx/"
               "a"
               "/link/port",
               tostr(base + ((int)("a"[0] - 'a')) + offset));
  set_property("tx/"
               "a"
               "/qa/fifo_lvl",
               tostr(base + ((int)("a"[0] - 'a')) + offset));
  set_property("tx/"
               "a"
               "/qa/oflow",
               tostr(base + ((int)("a"[0] - 'a')) + offset));
  set_property("tx/"
               "a"
               "/qa/uflow",
               tostr(base + ((int)("a"[0] - 'a')) + offset));
  set_property("tx/"
               "b"
               "/link/port",
               tostr(base + ((int)("b"[0] - 'a')) + offset));
  set_property("tx/"
               "b"
               "/qa/fifo_lvl",
               tostr(base + ((int)("b"[0] - 'a')) + offset));
  set_property("tx/"
               "b"
               "/qa/oflow",
               tostr(base + ((int)("b"[0] - 'a')) + offset));
  set_property("tx/"
               "b"
               "/qa/uflow",
               tostr(base + ((int)("b"[0] - 'a')) + offset));
  set_property("tx/"
               "c"
               "/link/port",
               tostr(base + ((int)("c"[0] - 'a')) + offset));
  set_property("tx/"
               "c"
               "/qa/fifo_lvl",
               tostr(base + ((int)("c"[0] - 'a')) + offset));
  set_property("tx/"
               "c"
               "/qa/oflow",
               tostr(base + ((int)("c"[0] - 'a')) + offset));
  set_property("tx/"
               "c"
               "/qa/uflow",
               tostr(base + ((int)("c"[0] - 'a')) + offset));
  set_property("tx/"
               "d"
               "/link/port",
               tostr(base + ((int)("d"[0] - 'a')) + offset));
  set_property("tx/"
               "d"
               "/qa/fifo_lvl",
               tostr(base + ((int)("d"[0] - 'a')) + offset));
  set_property("tx/"
               "d"
               "/qa/oflow",
               tostr(base + ((int)("d"[0] - 'a')) + offset));
  set_property("tx/"
               "d"
               "/qa/uflow",
               tostr(base + ((int)("d"[0] - 'a')) + offset));
  set_property("tx/"
               "e"
               "/link/port",
               tostr(base + ((int)("e"[0] - 'a')) + offset));
  set_property("tx/"
               "e"
               "/qa/fifo_lvl",
               tostr(base + ((int)("e"[0] - 'a')) + offset));
  set_property("tx/"
               "e"
               "/qa/oflow",
               tostr(base + ((int)("e"[0] - 'a')) + offset));
  set_property("tx/"
               "e"
               "/qa/uflow",
               tostr(base + ((int)("e"[0] - 'a')) + offset));
  set_property("tx/"
               "f"
               "/link/port",
               tostr(base + ((int)("f"[0] - 'a')) + offset));
  set_property("tx/"
               "f"
               "/qa/fifo_lvl",
               tostr(base + ((int)("f"[0] - 'a')) + offset));
  set_property("tx/"
               "f"
               "/qa/oflow",
               tostr(base + ((int)("f"[0] - 'a')) + offset));
  set_property("tx/"
               "f"
               "/qa/uflow",
               tostr(base + ((int)("f"[0] - 'a')) + offset));
  set_property("tx/"
               "g"
               "/link/port",
               tostr(base + ((int)("g"[0] - 'a')) + offset));
  set_property("tx/"
               "g"
               "/qa/fifo_lvl",
               tostr(base + ((int)("g"[0] - 'a')) + offset));
  set_property("tx/"
               "g"
               "/qa/oflow",
               tostr(base + ((int)("g"[0] - 'a')) + offset));
  set_property("tx/"
               "g"
               "/qa/uflow",
               tostr(base + ((int)("g"[0] - 'a')) + offset));
  set_property("tx/"
               "h"
               "/link/port",
               tostr(base + ((int)("h"[0] - 'a')) + offset));
  set_property("tx/"
               "h"
               "/qa/fifo_lvl",
               tostr(base + ((int)("h"[0] - 'a')) + offset));
  set_property("tx/"
               "h"
               "/qa/oflow",
               tostr(base + ((int)("h"[0] - 'a')) + offset));
  set_property("tx/"
               "h"
               "/qa/uflow",
               tostr(base + ((int)("h"[0] - 'a')) + offset));
  set_property("tx/"
               "i"
               "/link/port",
               tostr(base + ((int)("i"[0] - 'a')) + offset));
  set_property("tx/"
               "i"
               "/qa/fifo_lvl",
               tostr(base + ((int)("i"[0] - 'a')) + offset));
  set_property("tx/"
               "i"
               "/qa/oflow",
               tostr(base + ((int)("i"[0] - 'a')) + offset));
  set_property("tx/"
               "i"
               "/qa/uflow",
               tostr(base + ((int)("i"[0] - 'a')) + offset));
  set_property("tx/"
               "j"
               "/link/port",
               tostr(base + ((int)("j"[0] - 'a')) + offset));
  set_property("tx/"
               "j"
               "/qa/fifo_lvl",
               tostr(base + ((int)("j"[0] - 'a')) + offset));
  set_property("tx/"
               "j"
               "/qa/oflow",
               tostr(base + ((int)("j"[0] - 'a')) + offset));
  set_property("tx/"
               "j"
               "/qa/uflow",
               tostr(base + ((int)("j"[0] - 'a')) + offset));
  set_property("tx/"
               "k"
               "/link/port",
               tostr(base + ((int)("k"[0] - 'a')) + offset));
  set_property("tx/"
               "k"
               "/qa/fifo_lvl",
               tostr(base + ((int)("k"[0] - 'a')) + offset));
  set_property("tx/"
               "k"
               "/qa/oflow",
               tostr(base + ((int)("k"[0] - 'a')) + offset));
  set_property("tx/"
               "k"
               "/qa/uflow",
               tostr(base + ((int)("k"[0] - 'a')) + offset));
  set_property("tx/"
               "l"
               "/link/port",
               tostr(base + ((int)("l"[0] - 'a')) + offset));
  set_property("tx/"
               "l"
               "/qa/fifo_lvl",
               tostr(base + ((int)("l"[0] - 'a')) + offset));
  set_property("tx/"
               "l"
               "/qa/oflow",
               tostr(base + ((int)("l"[0] - 'a')) + offset));
  set_property("tx/"
               "l"
               "/qa/uflow",
               tostr(base + ((int)("l"[0] - 'a')) + offset));
  set_property("tx/"
               "m"
               "/link/port",
               tostr(base + ((int)("m"[0] - 'a')) + offset));
  set_property("tx/"
               "m"
               "/qa/fifo_lvl",
               tostr(base + ((int)("m"[0] - 'a')) + offset));
  set_property("tx/"
               "m"
               "/qa/oflow",
               tostr(base + ((int)("m"[0] - 'a')) + offset));
  set_property("tx/"
               "m"
               "/qa/uflow",
               tostr(base + ((int)("m"[0] - 'a')) + offset));
  set_property("tx/"
               "n"
               "/link/port",
               tostr(base + ((int)("n"[0] - 'a')) + offset));
  set_property("tx/"
               "n"
               "/qa/fifo_lvl",
               tostr(base + ((int)("n"[0] - 'a')) + offset));
  set_property("tx/"
               "n"
               "/qa/oflow",
               tostr(base + ((int)("n"[0] - 'a')) + offset));
  set_property("tx/"
               "n"
               "/qa/uflow",
               tostr(base + ((int)("n"[0] - 'a')) + offset));
  set_property("tx/"
               "o"
               "/link/port",
               tostr(base + ((int)("o"[0] - 'a')) + offset));
  set_property("tx/"
               "o"
               "/qa/fifo_lvl",
               tostr(base + ((int)("o"[0] - 'a')) + offset));
  set_property("tx/"
               "o"
               "/qa/oflow",
               tostr(base + ((int)("o"[0] - 'a')) + offset));
  set_property("tx/"
               "o"
               "/qa/uflow",
               tostr(base + ((int)("o"[0] - 'a')) + offset));
  set_property("tx/"
               "p"
               "/link/port",
               tostr(base + ((int)("p"[0] - 'a')) + offset));
  set_property("tx/"
               "p"
               "/qa/fifo_lvl",
               tostr(base + ((int)("p"[0] - 'a')) + offset));
  set_property("tx/"
               "p"
               "/qa/oflow",
               tostr(base + ((int)("p"[0] - 'a')) + offset));
  set_property("tx/"
               "p"
               "/qa/uflow",
               tostr(base + ((int)("p"[0] - 'a')) + offset));
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

  const char *vcs = "/var/crimson/state/";
  const size_t vcsl = strlen(vcs);
  char origcwd[MAX_PATH_LEN];
  char *temp;
  size_t path_strlen;
  size_t delta;
  int r;

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

  return NULL;
}

static inline const char *get_home_dir(void) {
  return getpwuid(getuid())->pw_dir;
}

void pass_uart_synth_fd(int fd) { uart_synth_fd = fd; }

void pass_uart_tx_fd(int *fd) { uart_tx_fd = fd; }

void pass_uart_rx_fd(int *fd) { uart_rx_fd = fd; }

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

void sync_channels(uint8_t chan_mask) {
  char str_chan_mask[MAX_PROP_LEN] = "";
  sprintf(str_chan_mask + strlen(str_chan_mask), "%" PRIu8 "", 15);

  strcpy(buf, "power -c ");
  strcat(buf, str_chan_mask);
  strcat(buf, " -a 1\r");
  send_uart_comm(uart_rx_fd[0], (uint8_t *)buf, strlen(buf));
  read_uart(uart_rx_fd[0]);

  strcpy(buf, "power -c ");
  strcat(buf, str_chan_mask);
  strcat(buf, " -d 1\r");
  send_uart_comm(uart_tx_fd[0], (uint8_t *)buf, strlen(buf));
  read_uart(uart_tx_fd[0]);

  char key[] = "00\r";
  char dacalarmA[] = "ff\r";
  char dacalarmB[] = "ff\r";
# 3370 "properties.c"
  write_hps_reg("res_rw7", 0x20000000);
  write_hps_reg("res_rw7", 0);

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

  write_hps_reg("res_rw7", 0);

  usleep(100000);
  strcpy(buf, "clk -y\r");
  send_uart_comm(uart_synth_fd, (uint8_t *)buf, strlen(buf));
  read_uart(uart_synth_fd);

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
}

void set_pll_frequency(int uart_fd, uint64_t reference, pllparam_t *pll,
                       bool tx, size_t channel) {

  strcpy(buf, "rf -v ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(reference / 1000));

  strcat(buf, "\r");
  send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

  strcpy(buf, "rf -r ");
  sprintf(buf + strlen(buf), "%" PRIu16 "", pll->R);
  strcat(buf, "\r");
  send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

  strcpy(buf, "rf -n ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", pll->N);
  strcat(buf, "\r");
  send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

  strcpy(buf, "rf -d ");
  sprintf(buf + strlen(buf), "%" PRIu16 "", pll->d);
  strcat(buf, "\r");
  send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

  strcpy(buf, "rf -t ");
  sprintf(buf + strlen(buf), "%" PRIu8 "", pll->divFBen);
  strcat(buf, "\r");
  send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

  strcpy(buf, "rf -g ");
  sprintf(buf + strlen(buf), "%" PRIu8 "", 1);

  strcat(buf, "\r");
  send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));

  double freq = pll->vcoFreq / pll->d;

  if (synth_lut_is_enabled(tx, channel)) {
    synth_rec_t rec;
    int ret = synth_lut_get(tx, channel, freq, &rec);
    if (EXIT_SUCCESS != ret) {
      PRINT(ERROR, "synth_lut_get( %u, %u, %f ) failed (%d,%s)\n", tx, channel,
            freq, ret, strerror(ret));
    } else {
      PRINT(INFO, "Setting %s %c @ %u MHz with parameters { %u, %u, %u}\n",
            tx ? "TX" : "RX", 'A' + channel, (unsigned)(freq / 1000000),
            rec.core, rec.band, rec.bias);
      snprintf(buf, sizeof(buf), "rf -c %c -A 0 -C %u -B %u -I %u\r",
               'a' + channel, rec.core, rec.band, rec.bias);
      send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));
    }
  } else {

    snprintf(buf, sizeof(buf), "rf -c %c -A 1\r", 'a' + channel);
    send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));
  }

  strcpy(buf, "rf -g ");

  strcpy(buf, "rf -f ");
  sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(freq / 1000));

  strcat(buf, "\r");
  send_uart_comm(uart_fd, (uint8_t *)buf, strlen(buf));
  usleep(100000);
}

int set_pll_frequency2(int actual_uart_fd, uint64_t reference,
                       pllparam_t *pll) {
  int r;

  snprintf(buf, sizeof(buf), "rf -v %" PRIu32 "\r",
           (uint32_t)(reference / 1000));
  r = write(actual_uart_fd, buf, strlen(buf));
  if (strlen(buf) != r) {
    r = errno;
    goto out;
  }

  snprintf(buf, sizeof(buf), "rf -r %" PRIu16 "\r", pll->R);
  r = write(actual_uart_fd, buf, strlen(buf));
  if (strlen(buf) != r) {
    r = errno;
    goto out;
  }

  snprintf(buf, sizeof(buf), "rf -n %" PRIu32 "\r", pll->N);
  r = write(actual_uart_fd, buf, strlen(buf));
  if (strlen(buf) != r) {
    r = errno;
    goto out;
  }

  snprintf(buf, sizeof(buf), "rf -d %" PRIu16 "\r", pll->d);
  r = write(actual_uart_fd, buf, strlen(buf));
  if (strlen(buf) != r) {
    r = errno;
    goto out;
  }

  snprintf(buf, sizeof(buf), "rf -t %" PRIu8 "\r", pll->divFBen);
  r = write(actual_uart_fd, buf, strlen(buf));
  if (strlen(buf) != r) {
    r = errno;
    goto out;
  }

  snprintf(buf, sizeof(buf), "rf -g %" PRIu8 "\r", 1);
  r = write(actual_uart_fd, buf, strlen(buf));
  if (strlen(buf) != r) {
    r = errno;
    goto out;
  }

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

      hdlr_rx_a_rf_freq_val, hdlr_rx_b_rf_freq_val, hdlr_rx_c_rf_freq_val,
      hdlr_rx_d_rf_freq_val, hdlr_rx_e_rf_freq_val, hdlr_rx_f_rf_freq_val,
      hdlr_rx_g_rf_freq_val, hdlr_rx_h_rf_freq_val, hdlr_rx_i_rf_freq_val,
      hdlr_rx_j_rf_freq_val, hdlr_rx_k_rf_freq_val, hdlr_rx_l_rf_freq_val,
      hdlr_rx_m_rf_freq_val, hdlr_rx_n_rf_freq_val, hdlr_rx_o_rf_freq_val,
      hdlr_rx_p_rf_freq_val,

  };

  static const fp_t tx_fp[] = {

      hdlr_tx_a_rf_freq_val, hdlr_tx_b_rf_freq_val, hdlr_tx_c_rf_freq_val,
      hdlr_tx_d_rf_freq_val, hdlr_tx_e_rf_freq_val, hdlr_tx_f_rf_freq_val,
      hdlr_tx_g_rf_freq_val, hdlr_tx_h_rf_freq_val, hdlr_tx_i_rf_freq_val,
      hdlr_tx_j_rf_freq_val, hdlr_tx_k_rf_freq_val, hdlr_tx_l_rf_freq_val,
      hdlr_tx_m_rf_freq_val, hdlr_tx_n_rf_freq_val, hdlr_tx_o_rf_freq_val,
      hdlr_tx_p_rf_freq_val,

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
    PRINT(ERROR, "%s %c: expected: %f, actual: %f\n", tx ? "TX" : "RX",
          'A' + channel, freq, actual_freq);
    goto out;
  }

  flush_uart_comm(tx ? uart_tx_fd[channel] : uart_rx_fd[channel]);

  r = EXIT_SUCCESS;

out:
  return r;
}
