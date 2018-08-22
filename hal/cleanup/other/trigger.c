/* ================================================================================================================== */
/* --------------------------------------------------- TRIGGER------------------------------------------------------- */
/* ================================================================================================================== */

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

#define DEFINE_TRIGGER_FUNCS(_trx, ch)                                                                                 \
    static int hdlr_##_trx##_##ch##_trigger_sma_mode(const char *data, char *ret) {                                    \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_trigger_mode(data, &val) || set_trigger_mode(true, !strcmp(#_trx, "tx"), #ch, val);                  \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_##_trx##_##ch##_trigger_edge_backoff(const char *data, char *ret) {                                \
        uint32_t val;                                                                                                  \
        int r;                                                                                                         \
        r = valid_edge_backoff(data, &val) || set_edge_backoff(!strcmp(#_trx, "tx"), #ch, val);                        \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_##_trx##_##ch##_trigger_edge_sample_num(const char *data, char *ret) {                             \
        uint64_t val;                                                                                                  \
        int r;                                                                                                         \
        r = valid_edge_sample_num(data, &val) || set_edge_sample_num(!strcmp(#_trx, "tx"), #ch, val);                  \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_##_trx##_##ch##_trigger_trig_sel(const char *data, char *ret) {                                    \
        uint32_t val;                                                                                                  \
        int r;                                                                                                         \
        r = valid_trigger_sel(data, &val) || set_trigger_sel(!strcmp(#_trx, "tx"), #ch, val);                          \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_##_trx##_##ch##_trigger_ufl_dir(const char *data, char *ret) {                                     \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_trigger_dir(data, &val) || set_trigger_ufl_dir(!strcmp(#_trx, "tx"), #ch, val);                      \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_##_trx##_##ch##_trigger_ufl_mode(const char *data, char *ret) {                                    \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_trigger_mode(data, &val) || set_trigger_mode(false, !strcmp(#_trx, "tx"), #ch, val);                 \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static int hdlr_##_trx##_##ch##_trigger_ufl_pol(const char *data, char *ret) {                                     \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_trigger_pol(data, &val) || set_trigger_ufl_pol(!strcmp(#_trx, "tx"), #ch, val);                      \
        return r;                                                                                                      \
    }

#define DEFINE_TX_GATING_FUNC(ch)                                                                                      \
    static int hdlr_tx_##ch##_trigger_gating(const char *data, char *ret) {                                            \
        int r;                                                                                                         \
        bool val;                                                                                                      \
        r = valid_gating_mode(data, &val) || set_gating_mode(#ch, val);                                                \
        return r;                                                                                                      \
    }

#define DEFINE_TX_GATING_FUNCS()                                                                                       \
    DEFINE_TX_GATING_FUNC(a)                                                                                           \
    DEFINE_TX_GATING_FUNC(b)                                                                                           \
    DEFINE_TX_GATING_FUNC(c)                                                                                           \
    DEFINE_TX_GATING_FUNC(d)

#define DEFINE_TX_TRIGGER_FUNCS()                                                                                      \
    DEFINE_TRIGGER_FUNCS(tx, a)                                                                                        \
    DEFINE_TRIGGER_FUNCS(tx, b)                                                                                        \
    DEFINE_TRIGGER_FUNCS(tx, c)                                                                                        \
    DEFINE_TRIGGER_FUNCS(tx, d)

#define DEFINE_RX_TRIGGER_FUNCS()                                                                                      \
    DEFINE_TRIGGER_FUNCS(rx, a)                                                                                        \
    DEFINE_TRIGGER_FUNCS(rx, b)                                                                                        \
    DEFINE_TRIGGER_FUNCS(rx, c)                                                                                        \
    DEFINE_TRIGGER_FUNCS(rx, d)

DEFINE_TX_GATING_FUNCS()
DEFINE_RX_TRIGGER_FUNCS()
DEFINE_TX_TRIGGER_FUNCS()
