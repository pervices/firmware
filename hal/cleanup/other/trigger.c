

static int valid_trigger_mode( const char *data, bool *edge ) {

	if ( false ) {
	} else if ( 0 == strncmp( "edge", data, strlen( "edge" ) ) ) {
		*edge = true;
	} else if ( 0 == strncmp( "level", data, strlen( "level" ) ) ) {
		*edge = false;
	} else {
		return RETURN_ERROR_PARAM;
	}

	return RETURN_SUCCESS;
}

static int valid_trigger_pol( const char *data, bool *positive ) {

	if ( false ) {
	} else if ( 0 == strncmp( "positive", data, strlen( "positive" ) ) ) {
		*positive = true;
	} else if ( 0 == strncmp( "negative", data, strlen( "negative" ) ) ) {
		*positive = false;
	} else {
		PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)" );
		return RETURN_ERROR_PARAM;
	}
	return RETURN_SUCCESS;
}

static int valid_trigger_sel( const char *data, uint32_t *sel ) {
	int r;

	r = sscanf( data, "%" PRIu32, sel );
	if ( 1 != r || *sel >= 4 ) {
		PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)" );
		return RETURN_ERROR_PARAM;
	}

	return RETURN_SUCCESS;
}

static int valid_trigger_dir( const char *data, bool *in ) {
	if ( false ) {
	} else if ( 0 == strncmp( "in", data, strlen( "in" ) ) ) {
		*in = true;
	} else if ( 0 == strncmp( "out", data, strlen( "out" ) ) ) {
		*in = false;
	} else {
		PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)" );
		return RETURN_ERROR_PARAM;
	}
	return RETURN_SUCCESS;
}

static int set_trigger_ufl_dir( bool tx, const char *chan, bool in ) {
	char reg_name[ 8 ];
	snprintf( reg_name, sizeof( reg_name ), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 6 : 9 );
	return set_reg_bits( reg_name, 9, 1, in );
}

static int set_trigger_sel( bool tx, const char *chan, uint32_t sel ) {
	char reg_name[ 8 ];
	snprintf( reg_name, sizeof( reg_name ), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 6 : 9 );
	return set_reg_bits( reg_name, 10, 0b11, sel );
}

static int set_trigger_mode( bool sma, bool tx, const char *chan, bool edge ) {
	unsigned shift;
	char reg_name[ 8 ];
	snprintf( reg_name, sizeof( reg_name ), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 6 : 9 );
	shift = sma ? 0 : 4;
	return set_reg_bits( reg_name, shift, 1, edge );
}

static int set_trigger_ufl_pol( bool tx, const char *chan, bool positive ) {
	char reg_name[ 8 ];
	snprintf( reg_name, sizeof( reg_name ), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 6 : 9 );
	return set_reg_bits( reg_name, 8, 1, positive );
}

#define DEFINE_TRIGGER_FUNCS( _trx, _c ) \
static int hdlr_ ## _trx ## _ ## _c ## _trigger_sma_mode( const char *data, char *ret ) { \
	int r; \
	bool val; \
	r = valid_trigger_mode( data, & val ) || set_trigger_mode( true, ! strcmp( #_trx, "tx" ), #_c, val ); \
	return r; \
} \
\
static int hdlr_ ## _trx ## _ ## _c ## _trigger_edge_backoff (const char *data, char* ret) { \
	uint32_t val; \
	int r; \
	r = valid_edge_backoff( data, & val ) || set_edge_backoff( ! strcmp( #_trx, "tx" ), #_c, val ); \
	return r; \
} \
\
static int hdlr_ ## _trx ## _ ## _c ## _trigger_edge_sample_num (const char *data, char* ret) { \
	uint64_t val; \
	int r; \
	r = valid_edge_sample_num( data, & val ) || set_edge_sample_num( ! strcmp( #_trx, "tx" ), #_c, val ); \
	return r; \
} \
\
static int hdlr_ ## _trx ## _ ## _c ## _trigger_trig_sel (const char *data, char* ret) { \
	uint32_t val; \
	int r; \
	r = valid_trigger_sel( data, & val ) || set_trigger_sel( ! strcmp( #_trx, "tx" ), #_c, val ); \
	return r; \
} \
\
static int hdlr_ ## _trx ## _ ## _c ## _trigger_ufl_dir (const char *data, char* ret) { \
	int r; \
	bool val; \
	r = valid_trigger_dir( data, & val ) || set_trigger_ufl_dir( ! strcmp( #_trx, "tx" ), #_c, val ); \
	return r; \
} \
\
static int hdlr_ ## _trx ## _ ## _c ## _trigger_ufl_mode (const char *data, char* ret) { \
	int r; \
	bool val; \
	r = valid_trigger_mode( data, & val ) || set_trigger_mode( false, ! strcmp( #_trx, "tx" ), #_c, val ); \
	return r; \
} \
\
static int hdlr_ ## _trx ## _ ## _c ## _trigger_ufl_pol (const char *data, char* ret) { \
	int r; \
	bool val; \
	r = valid_trigger_pol( data, & val ) || set_trigger_ufl_pol( ! strcmp( #_trx, "tx" ), #_c, val ); \
	return r; \
}

#define DEFINE_TX_GATING_FUNC( _c ) \
static int hdlr_tx_ ## _c ## _trigger_gating (const char *data, char* ret) { \
	int r; \
	bool val; \
	r = valid_gating_mode( data, & val ) || set_gating_mode( #_c, val ); \
	return r; \
}

#define DEFINE_TX_TRIGGER_FUNCS() \
	DEFINE_TRIGGER_FUNCS( tx, a ); \
	DEFINE_TRIGGER_FUNCS( tx, b ); \
	DEFINE_TRIGGER_FUNCS( tx, c ); \
	DEFINE_TRIGGER_FUNCS( tx, d ); \
	DEFINE_TX_GATING_FUNC( a ); \
	DEFINE_TX_GATING_FUNC( b ); \
	DEFINE_TX_GATING_FUNC( c ); \
	DEFINE_TX_GATING_FUNC( d )


#define DEFINE_RX_TRIGGER_FUNCS() \
	DEFINE_TRIGGER_FUNCS( rx, a ); \
	DEFINE_TRIGGER_FUNCS( rx, b ); \
	DEFINE_TRIGGER_FUNCS( rx, c ); \
	DEFINE_TRIGGER_FUNCS( rx, d )

DEFINE_RX_TRIGGER_FUNCS();
DEFINE_TX_TRIGGER_FUNCS();


