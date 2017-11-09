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

#include "mmap.h"
#include "properties.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "property_manager.h"

#define BASE_SAMPLE_RATE 325000000.0	// SPS
#define RESAMP_SAMPLE_RATE  260000000.0	// SPS
#define IPVER_IPV4 0
#define IPVER_IPV6 1
//truncate DSP NCO CONST last 4 digits to ensure dac NCO can divide evenly
#define DSP_NCO_CONST ((double)13.215283987692307692307692307692307692307692307692307690000)	// (2^32) / (322265625) 
#define DAC_NCO_CONST ((double)216519.21285435076923076923076923076923076923076923076919296)	// (2^48) / (4*322265625)

/* Forwarding is now obsolete */
//#define FWD_CMD 	1
//#define NO_FWD_CMD	0

#define PWR_ON	1
#define PWR_OFF	0

#define STREAM_ON	1
#define STREAM_OFF	0

#define FREQ_XOVER_PNT 100000000	// 100 MHz is the crossover frequency for high and low band

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
const static char* reg4[] = {"rxa4", "rxb4", "rxc4", "rxd4", "txa4", "txb4", "txc4", "txd4"};
static int i_bias[] = {17, 17, 17, 17};
static int q_bias[] = {17, 17, 17, 17};

// profile pointers
uint8_t* _save_profile;
uint8_t* _load_profile;
char* _save_profile_path;
char* _load_profile_path;

// state variables
static uint8_t ipver[2] = {IPVER_IPV4, IPVER_IPV4};

// helper function to check if the buffer contains a character, strstr() won't work because no NULL terminator
static int contains (const char* str, char letter, int size) {
	int i = 0, cnt = 0;
	for (i = 0; i < size; i++) {
		if (str[i] == letter) cnt++;
	}
	return cnt;
}

// helper function to read back from UART after a UART command
static int read_uart(int uartfd) {
	char buf[MAX_UART_LEN] = {};
	memset(buf, 0, MAX_UART_LEN);

	uint16_t total_bytes = 0, cur_bytes = 0;

	while ( contains(buf, '>', total_bytes) < 1 ) {
	   if (recv_uart_comm(uartfd, ((uint8_t*)buf) + total_bytes,
	   		&cur_bytes, MAX_UART_LEN - total_bytes)) {
		return 0;
	   }
	   total_bytes += cur_bytes;
	}

	/* Forwarding is now obsolete */

	// if fwd, remove everything prior to the second message
	//if (fwd) {
	//	uint16_t pos = 0, real_size = 0;
	//	while (buf[pos] != '>') pos++;
	//	pos++;
	//	real_size = total_bytes - pos;
	//	memcpy(buf, buf + pos, real_size);
	//	memset(buf + real_size, 0, MAX_UART_LEN - real_size);
	//}

	printf("%s\n", buf);
	strncpy((char*)uart_ret_buf, buf, MAX_UART_RET_LEN-1);
	return RETURN_SUCCESS;
}

// helper function to find the optimal value for the sample rate blocks
static uint16_t get_optimal_sr_factor(double rate, double base_rate, double* err) {
	double max_factor = 65536;	// 2^16
	double min_factor = 1;
	double lower_factor_violation = 0;
	double upper_factor_violation = 0;

	double lower_factor = (double)floor(base_rate / rate);
	double upper_factor = lower_factor + 1;

   // error bounds check
   if (lower_factor > max_factor || lower_factor < min_factor) lower_factor_violation = 1;
   if (upper_factor > max_factor || upper_factor < min_factor) upper_factor_violation = 1;

   double lower_factor_error = (base_rate / (double)lower_factor) - rate;
   double upper_factor_error = rate - (base_rate / (double)upper_factor);

   if (lower_factor_violation && upper_factor_violation) {
	   return 0xffff;
   } else if (lower_factor_violation) {
	   if (err) *err = upper_factor_error;
	   return (uint16_t)(upper_factor - 1);
   } else if (upper_factor_violation) {
	   if (err) *err = lower_factor_error;
	   return (uint16_t)(lower_factor - 1);
   } else {		// Nothing is wrong, then
	   if (lower_factor_error < upper_factor_error) {
		   if (err) *err = lower_factor_error;
		   return (uint16_t)(lower_factor - 1);
	   } else {
		   if (err) *err = upper_factor_error;
		   return (uint16_t)(upper_factor - 1);
	   }
   }
}

static int valid_edge_backoff( const char *data, uint32_t *val ) {
	if ( 1 == sscanf( data, "%" PRIu32, val ) ) {
		return RETURN_SUCCESS;
	} else {
		PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)" ); \
		return RETURN_ERROR_PARAM; \
	}
}

static int valid_edge_sample_num( const char *data, uint64_t *val ) {
	if ( 1 == sscanf( data, "%" PRIu64, val ) ) {
		return RETURN_SUCCESS;
	} else {
		PRINT(ERROR, "Invalid argument: '%s'\n", data ? data : "(null)" );
		return RETURN_ERROR_PARAM;
	}
}

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

static int valid_gating_mode( const char *data, bool *dsp ) {
	if ( false ) {
	} else if ( 0 == strncmp( "dsp", data, strlen( "dsp" ) ) ) {
		*dsp = true;
	} else if ( 0 == strncmp( "output", data, strlen( "output" ) ) ) {
		*dsp = false;
	} else {
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

// XXX: @CF: 20171108: Statement Expressions are bad... but this code will be replaced soon anyway
#define set_reg_bits( name, shift, mask, val ) 	({ \
	int _r; \
	uint32_t _t; \
	_r = read_hps_reg( name, & _t ); \
	if ( RETURN_SUCCESS != _r ) { \
		PRINT( ERROR, "read_hps_reg( '%s' ) failed: %d\n", name, _r ); \
	} else { \
		_t &= ~( mask << shift ); \
		_t |= val << shift; \
		_r = write_hps_reg( name, _t ); \
		if ( RETURN_SUCCESS != _r ) { \
			PRINT( ERROR, "write_hps_reg( '%s' ) failed: %d\n", name, _r ); \
		} \
	} \
	_r; \
})

static int set_sma_dir( bool in ) {
	return set_reg_bits( "sys2", 4, 1, in );
}

static int set_sma_pol( bool positive ) {
	return set_reg_bits( "sys2", 6, 1, positive );
}

static int set_edge_backoff( bool tx, const char *chan, uint32_t backoff ) {
	char regname[ 8 ];
	snprintf( regname, sizeof( regname ), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 9 : 12 );
	return set_reg_bits( regname, 0, 32, backoff );
}

static int set_edge_sample_num( bool tx, const char *chan, uint64_t num ) {
	int r;
	uint32_t val_msw;
	uint32_t val_lsw;

	char regname_msw[ 8 ];
	char regname_lsw[ 8 ];

	snprintf( regname_msw, sizeof( regname_msw ), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 7 : 10 );
	snprintf( regname_lsw, sizeof( regname_lsw ), "%s%s%u", tx ? "tx" : "rx", chan, tx ? 8 : 11 );

	val_msw = num >> 32;
	val_lsw = num & 0xffffffff;

	return set_reg_bits( regname_msw, 0, 32, val_msw ) || set_reg_bits( regname_lsw, 0, 32, val_lsw );
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

static int set_gating_mode( const char *chan, bool dsp ) {
	char reg_name[ 8 ];
	snprintf( reg_name, sizeof( reg_name ), "tx%s6", chan );
	return set_reg_bits( reg_name, 12, 1, dsp );
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


// Beginning of property functions, very long because each property needs to be
// handled explicitly
static int hdlr_invalid (const char* data, char* ret) {
	PRINT( ERROR,"Cannot invoke a set on this property\n");
	return RETURN_ERROR_SET_PROP;
}

static int hdlr_tx_a_rf_dac_dither_en (const char* data, char* ret) {
	int r;
	int en;

	r = sscanf( data, "%d", &en );
	if ( 1 != r ) {
		return RETURN_ERROR;
	}
	if ( en < 0 || en > 1 ) {
		return RETURN_ERROR_PARAM;
	}
	snprintf( buf, sizeof( buf ), "dac -c a -l %u\r", en );
	sprintf( ret, "%u", en );
	send_uart_comm( uart_tx_fd, (uint8_t*)buf, strlen(buf) );
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_dac_dither_mixer_en (const char* data, char* ret) {
	int r;
	int en;

	r = sscanf( data, "%d", &en );
	if ( 1 != r ) {
		return RETURN_ERROR;
	}
	if ( en < 0 || en > 1 ) {
		return RETURN_ERROR_PARAM;
	}
	snprintf( buf, sizeof( buf ), "dac -c a -3 %u\r", en );
	sprintf( ret, "%u", en );
	send_uart_comm( uart_tx_fd, (uint8_t*)buf, strlen(buf) );
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_dac_dither_sra_sel (const char* data, char* ret) {
	int r;
	int db;
	int sel;

	r = sscanf( data, "%d", &db );
	if ( 1 != r ) {
		return RETURN_ERROR;
	}
	if ( db < 6 || db > 96 || 0 != db % 6 ) {
		return RETURN_ERROR_PARAM;
	}

	sprintf( ret, "%u", db );

	 // 96 dB (max) := 0x0, 90 dB := 0x1, ... 6 dB (min) := 0xf
	sel = 16 - ( db / 6 );

	snprintf( buf, sizeof( buf ), "dac -c a -b %u\r", sel );
	send_uart_comm( uart_tx_fd, (uint8_t*)buf, strlen(buf) );
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_dac_nco (const char* data, char* ret) {
	double freq;
	sscanf(data, "%lf", &freq);
	uint64_t nco_steps = (uint64_t)round(freq * DAC_NCO_CONST);
	sprintf(ret, "%lf", (double)nco_steps / DAC_NCO_CONST);

	strcpy(buf, "dac -c a -e 0 -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	strcpy(buf, "dac -o ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_dac_temp (const char* data, char* ret) {
	strcpy(buf, "board -c a -t\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_freq_val (const char* data, char* ret) {
	uint64_t freq = 0;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c a -z\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

        // if freq out of bounds, kill channel
        if (( freq < PLL1_RFOUT_MIN_HZ ) || (freq > PLL1_RFOUT_MAX_HZ )) {
		strcpy(buf, "board -c a -k\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		// Turn OFF TXA on HPS
		uint32_t old_val;

		// disable DSP cores
		read_hps_reg ( "txa4", &old_val);
		write_hps_reg( "txa4", old_val | 0x2);

		// disable channel
		read_hps_reg ( "txa4", &old_val);
		write_hps_reg( "txa4", old_val & (~0x100));

		tx_power[0] = PWR_OFF;

		PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting Down TXA.\n");

		return RETURN_ERROR;
	}

	// run the pll calc algorithm
	pllparam_t pll;
        long double outfreq = 0;
	outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c a \r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_tx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll);

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "rf -c a -b ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_freq_i_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(i_bias[0]));
   strcpy(buf, "rf -c a -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[0]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[0]);
   strcat(buf, " -m\r");
   send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_freq_q_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(q_bias[0]));
   strcpy(buf, "rf -c a -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[0]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[0]);
   strcat(buf, " -m\r");
   send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);

	// 0   -> 126	attenuation only
	// 127		0dB
	
	if (gain > 127)		gain = 127;
	else if (gain < 0) 	gain = 0;

	strcpy(buf, "rf -c a -a ");
	sprintf(buf + strlen(buf), "%i", 127 - gain);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_board_dump (const char* data, char* ret) {
	// send the uart commands and read back the output and write to file

	// DAC
	strcpy(buf, "dump -c a -d\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	PRINT(DUMP, "[Board: tx_a Chip: DAC] %s\n", uart_ret_buf);

	// GPIOX
	strcpy(buf, "dump -c a -g\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	PRINT(DUMP, "[Board: tx_a Chip: GPIOX] %s\n", uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "board -c a -t\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_a_status_rfld (const char* data, char* ret) {
	strcpy(buf, "status -c a -l\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_a_status_dacld(const char* data, char* ret) {
	strcpy(buf, "status -c a -p\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_a_status_dacctr(const char* data, char* ret) {
	strcpy(buf, "status -c a -e\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_a_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l\r");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
	uint16_t base_factor, resamp_factor;
	double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base rate
	base_factor   = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
	resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate
	memset(ret, 0, MAX_PROP_LEN);

	if (resamp_err < base_err) {
		write_hps_reg( "txa1", resamp_factor);
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)(resamp_factor + 1));
		//Set gain adjustment
		read_hps_reg( "txga",  &old_val);
		write_hps_reg( "txga", (old_val & ~(0xff << 0) ) | (interp_gain_lut[(resamp_factor)] << 0));
	} else {
		write_hps_reg( "txa1", base_factor);
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)(base_factor + 1));
		//Set gain adjustment
		read_hps_reg( "txga",  &old_val);
		write_hps_reg( "txga", (old_val & ~(0xff << 0) ) | (interp_gain_lut[(base_factor)] << 0));
	}

	// DSP Reset
	read_hps_reg(  "txa4", &old_val);
	write_hps_reg( "txa4", old_val |  0x2);
	write_hps_reg( "txa4", old_val & ~0x2);

	return RETURN_SUCCESS;
}

static int hdlr_tx_a_dsp_nco_adj (const char* data, char* ret) {
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
	write_hps_reg( "txa0", nco_steps);
	if (direction > 0) {
		sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
	} else {
		sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
	}

	// write direction
	read_hps_reg(  "txa4", &old_val);
	write_hps_reg( "txa4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txa4", &old_val);
	write_hps_reg( "txa4", old_val |  0x2);
	write_hps_reg( "txa4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int hdlr_tx_about_serial (const char* data, char* ret) {
	strcpy(buf, "status -s\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_about_mcudevid (const char* data, char* ret) {
	strcpy(buf, "status -d\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_about_mcurev (const char* data, char* ret) {
	strcpy(buf, "status -v\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_about_mcufuses (const char* data, char* ret) {
	strcpy(buf, "status -f\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_about_fw_ver (const char* data, char* ret) {
	strcpy(buf, "board -v\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_a_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txa4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "txa4", old_val | (1 << 14));
	else				write_hps_reg( "txa4", old_val & ~(1 << 14));

	//sync_channels( 15 );

	return RETURN_SUCCESS;
}

static int hdlr_tx_a_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "txa5", port);
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_qa_fifo_lvl (const char* data, char* ret) {
	uint32_t lvl;
	read_hps_reg( "res_ro4", & lvl );
	lvl &= 0xffff;
	sprintf( ret, "%u", lvl );
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_qa_oflow (const char* data, char* ret) {
	uint32_t count;
	// this is technically a 64-bit register, but we currently only need the bottom 32-bits
	read_hps_reg( "flc14", & count );
	sprintf( ret, "%u", count );
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_qa_uflow (const char* data, char* ret) {
	uint32_t count;
	// this is technically a 64-bit register, but we currently only need the bottom 32-bits
	read_hps_reg( "flc6", & count );
	sprintf( ret, "%u", count );
	return RETURN_SUCCESS;
}

static int hdlr_tx_a_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
   uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check if power is already enabled
	if (power >= PWR_ON  && tx_power[0] == PWR_ON)  return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		tx_power[0] = PWR_ON;

      // board commands
		strcpy(buf, "board -c a -d\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
      usleep(200000);

		// disable dsp channels
      for(i = 0; i < (NUM_CHANNELS * 2); i++) {
         read_hps_reg ( reg4[i], &old_val);
		   write_hps_reg( reg4[i], old_val & ~0x100);
      }

      // send sync pulse
      sync_channels(15);

		// enable active dsp channels, and reset the DSP
      for(i = 0; i < NUM_CHANNELS; i++) {
         if (tx_power[i] == PWR_ON) {
            read_hps_reg ( reg4[i+4], &old_val);
	         write_hps_reg( reg4[i+4], old_val | 0x100);
        		read_hps_reg ( reg4[i+4], &old_val);
   		   write_hps_reg( reg4[i+4], old_val | 0x2);
	      	write_hps_reg( reg4[i+4], old_val & (~0x2));
         }
         if (rx_power[i] == PWR_ON) {
      		read_hps_reg ( reg4[i], &old_val);
		      write_hps_reg( reg4[i], old_val | 0x100);
        		read_hps_reg ( reg4[i], &old_val);
   		   write_hps_reg( reg4[i], old_val | 0x2);
	      	write_hps_reg( reg4[i], old_val & (~0x2));
         }
      }

	// power off
	} else {
		// kill the channel
		strcpy(buf, "board -c a -k\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		// disable DSP cores
		read_hps_reg ( "txa4", &old_val);
		write_hps_reg( "txa4", old_val | 0x2);

		// disable channel
		read_hps_reg ( "txa4", &old_val);
		write_hps_reg( "txa4", old_val & (~0x100));

		tx_power[0] = PWR_OFF;
	}

	return RETURN_SUCCESS;
}

static int hdlr_tx_sync (const char* data, char* ret) {
	uint32_t old_val;

	// toggle the bit sys0[6]
	read_hps_reg ( "sys0", &old_val);
	write_hps_reg( "sys0", old_val | 0x40);
	write_hps_reg( "sys0", old_val & (~0x40));

	return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_freq_val (const char* data, char* ret) {
	uint64_t freq = 0;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c a -z\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

        // if freq out of bounds, kill channel
        if (( freq < PLL1_RFOUT_MIN_HZ ) || (freq > PLL1_RFOUT_MAX_HZ )) {
		strcpy(buf, "board -c a -k\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		// Turn OFF RXA on HPS
		uint32_t old_val;

		// disable DSP core
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | 0x2);

		// disable channel
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & (~0x100));

		rx_power[0] = PWR_OFF;
		rx_stream[0] = STREAM_OFF;

		PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting Down RXA.\n");

		return RETURN_ERROR;
	}

	// run the pll calc algorithm
	pllparam_t pll;
        long double outfreq = 0;
	outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c a \r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_rx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll);

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "rf -c a -l ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "rf -c a -b ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);
	
	if (gain > 126)		gain = 126;
	else if (gain < 0)	gain  = 0;

	if (gain % 2) gain++;		// Odd Number

	// 0 -> 126 gain
	strcpy(buf, "vga -c a -g ");
	sprintf(buf + strlen(buf), "%i", gain >> 1);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_atten_val(const char* data, char* ret) {
	int atten;
	sscanf(data, "%i", &atten);

	if (atten > 127)		atten = 127;
	else if (atten < 0) 	atten = 0;

	strcpy(buf, "rf -c a -a ");
	sprintf(buf + strlen(buf), "%i", atten);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_board_dump (const char* data, char* ret) {
	// send the uart commands and read back the output and write to file

	// ADC
	strcpy(buf, "dump -c a -a\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	PRINT(DUMP, "[Board: rx_a Chip: ADC] %s\n", uart_ret_buf);

	// GPIOX
	strcpy(buf, "dump -c a -g\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	PRINT(DUMP, "[Board: rx_a Chip: GPIOX] %s\n", uart_ret_buf);

	// ADC Driver
	strcpy(buf, "dump -c a -v\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	PRINT(DUMP, "[Board: rx_a Chip: ADC Driver] %s\n", uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "board -c a -t\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_a_status_rfld (const char* data, char* ret) {
	strcpy(buf, "status -c a -l\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_a_status_adcalarm (const char* data, char* ret) {
	strcpy(buf, "status -c a -a\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_a_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l\r");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_dsp_signed (const char* data, char* ret) {
   uint32_t old_val, sign;
   sscanf(data, "%u", &sign);
   sign = sign ? 0 : 1;

	read_hps_reg( "rxa4", &old_val);
   old_val &= ~(1 << 4);
	write_hps_reg( "rxa4", old_val | (sign << 4));
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
	uint16_t base_factor, resamp_factor;
	double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base rate
	base_factor   = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
	resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate
	memset(ret, 0, MAX_PROP_LEN);
	int gain_factor;

	if (resamp_err < base_err) {
		write_hps_reg( "rxa1", resamp_factor);
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)(resamp_factor + 1));
		//Set gain adjustment
		gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 0) )  | (((uint16_t)gain_factor) << 0));
	} else {
		write_hps_reg( "rxa1", base_factor);
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)(base_factor + 1));
		//Set gain adjustment
		gain_factor = decim_gain_lut[(resamp_factor)];
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 0) ) | (((uint16_t)gain_factor) << 0));
	}

	// DSP Reset
	read_hps_reg(  "rxa4", &old_val);
	write_hps_reg( "rxa4", old_val |  0x2);
	write_hps_reg( "rxa4", old_val & ~0x2);

   return RETURN_SUCCESS;
}

static int hdlr_rx_a_dsp_nco_adj (const char* data, char* ret) {
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
	write_hps_reg( "rxa0", nco_steps);
	if (direction > 0) {
		sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
	} else {
		sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
	}

	// write direction
	read_hps_reg(  "rxa4", &old_val);
	write_hps_reg( "rxa4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxa4", &old_val);
	write_hps_reg( "rxa4", old_val |  0x2);
	write_hps_reg( "rxa4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_dsp_loopback (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxa4", &old_val);
	if (strcmp(data, "1") == 0)   write_hps_reg( "rxa4", (old_val & ~0x1e00) | 0x400);
   else                          write_hps_reg( "rxa4", (old_val & ~0x1e00) | 0x000);
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int hdlr_rx_about_serial (const char* data, char* ret) {
	strcpy(buf, "status -s\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_about_mcudevid (const char* data, char* ret) {
	strcpy(buf, "status -d\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_about_mcurev (const char* data, char* ret) {
	strcpy(buf, "status -v\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_about_mcufuses (const char* data, char* ret) {
	strcpy(buf, "status -f\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_about_fw_ver (const char* data, char* ret) {
	strcpy(buf, "board -v\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_a_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxa4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "rxa4", old_val | (1 << 14));
	else				write_hps_reg( "rxa4", old_val & ~(1 << 14));

	//sync_channels( 15 );

	return RETURN_SUCCESS;
}

static int hdlr_rx_a_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "rxa8", port);
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_link_ip_dest (const char* data, char* ret) {
	uint8_t ip[4];
	sscanf(data, "%"SCNd8".%"SCNd8".%"SCNd8".%"SCNd8"", ip, ip+1, ip+2, ip+3);
	write_hps_reg( "rxa5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_link_mac_dest (const char* data, char* ret) {
	uint8_t mac[6];
	sscanf(data, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8"",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	write_hps_reg( "rxa6", (mac[0] << 8) | (mac[1]) );
	write_hps_reg( "rxa7", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
	return RETURN_SUCCESS;
}

static int hdlr_rx_a_stream (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t stream;
	sscanf(data, "%"SCNd8"", &stream);

	// if stream > 1, check the status of the stream
	if (stream > 1) {
		sprintf(ret, "%u", rx_stream[0]);	// Alert File Tree
		return RETURN_SUCCESS;
	}

	// Stream is already ON or OFF then return
	if (stream == rx_stream[0]) return RETURN_SUCCESS;

	// Otherwise make the change accordingly
	if (stream > 0) {	// TURN THE STREAM ON
		if (rx_power[0] == PWR_ON) {
			read_hps_reg ( reg4[0], &old_val);
			write_hps_reg( reg4[0], old_val | 0x100);

			read_hps_reg ( reg4[0], &old_val);
			write_hps_reg( reg4[0], old_val | 0x2);
			write_hps_reg( reg4[0], old_val & (~0x2));

			rx_stream[0] = STREAM_ON;
		} else {
			// Do not turn ON stream if channel is OFF
			sprintf(ret, "%u", 0);	// Alert File Tree
		}
	} else {			// TURN THE STREAM OFF
		// disable DSP core
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | 0x2);

		// disable channel
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & (~0x100));

		rx_stream[0] = STREAM_OFF;
	}

	return RETURN_SUCCESS;
}

static int hdlr_rx_a_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
	uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check if power is already enabled
	if (power >= PWR_ON  && rx_power[0] == PWR_ON)  return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		rx_power[0] = PWR_ON;

		// board command
		strcpy(buf, "board -c a -d\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
		usleep(200000);

		// disable dsp channels
		for(i = 0; i < (NUM_CHANNELS * 2); i++) {
			read_hps_reg ( reg4[i], &old_val);
			write_hps_reg( reg4[i], old_val & ~0x100);
		}

		// send sync pulse
		sync_channels(15);

		// Enable active dsp channels, and reset DSP
		for (i = 0; i < NUM_CHANNELS; i++) {
			if (tx_power[i] == PWR_ON) {
				read_hps_reg ( reg4[i+4], &old_val);
				write_hps_reg( reg4[i+4], old_val | 0x100);
				read_hps_reg ( reg4[i+4], &old_val);
				write_hps_reg( reg4[i+4], old_val | 0x2);
				write_hps_reg( reg4[i+4], old_val & (~0x2));
			}
			if (rx_stream[i] == STREAM_ON) {
				read_hps_reg ( reg4[i], &old_val);
				write_hps_reg( reg4[i], old_val | 0x100);
				read_hps_reg ( reg4[i], &old_val);
				write_hps_reg( reg4[i], old_val | 0x2);
				write_hps_reg( reg4[i], old_val & (~0x2));
			}
		}

	// power off & stream off
	} else {
		rx_power[0] = PWR_OFF;
		rx_stream[0] = STREAM_OFF;

		// kill the channel
		strcpy(buf, "board -c a -k\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		// disable DSP core
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | 0x2);

		// disable channel
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & (~0x100));
	}
	return RETURN_SUCCESS;
}

static int hdlr_rx_sync (const char* data, char* ret) {
	uint32_t old_val;

	// toggle the bit sys0[5]
	read_hps_reg ( "sys0", &old_val);
	write_hps_reg( "sys0", old_val | 0x20);
	write_hps_reg( "sys0", old_val & (~0x20));

	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_dac_dither_en (const char* data, char* ret) {
	int r;
	int en;

	r = sscanf( data, "%d", &en );
	if ( 1 != r ) {
		return RETURN_ERROR;
	}
	if ( en < 0 || en > 1 ) {
		return RETURN_ERROR_PARAM;
	}
	snprintf( buf, sizeof( buf ), "dac -c b -l %u\r", en );
	sprintf( ret, "%u", en );
	send_uart_comm( uart_tx_fd, (uint8_t*)buf, strlen(buf) );
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_dac_dither_mixer_en (const char* data, char* ret) {
	int r;
	int en;

	r = sscanf( data, "%d", &en );
	if ( 1 != r ) {
		return RETURN_ERROR;
	}
	if ( en < 0 || en > 1 ) {
		return RETURN_ERROR_PARAM;
	}
	snprintf( buf, sizeof( buf ), "dac -c b -3 %u\r", en );
	sprintf( ret, "%u", en );
	send_uart_comm( uart_tx_fd, (uint8_t*)buf, strlen(buf) );
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_dac_dither_sra_sel (const char* data, char* ret) {
	int r;
	int db;
	int sel;

	r = sscanf( data, "%d", &db );
	if ( 1 != r ) {
		return RETURN_ERROR;
	}
	if ( db < 6 || db > 96 || 0 != db % 6 ) {
		return RETURN_ERROR_PARAM;
	}

	sprintf( ret, "%u", db );

	 // 96 dB (max) := 0x0, 90 dB := 0x1, ... 6 dB (min) := 0xf
	sel = 16 - ( db / 6 );

	snprintf( buf, sizeof( buf ), "dac -c b -b %u\r", sel );
	send_uart_comm( uart_tx_fd, (uint8_t*)buf, strlen(buf) );
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_dac_nco (const char* data, char* ret) {
	double freq;
	sscanf(data, "%lf", &freq);
	uint64_t nco_steps = (uint64_t)round(freq * DAC_NCO_CONST);
	sprintf(ret, "%lf", (double)nco_steps / DAC_NCO_CONST);

	strcpy(buf, "dac -c b -e 1 -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	strcpy(buf, "dac -o ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_dac_temp (const char* data, char* ret) {
	strcpy(buf, "board -c b -t\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_freq_val (const char* data, char* ret) {
	uint64_t freq = 0;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c b -z\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

        // if freq out of bounds, kill channel
        if (( freq < PLL1_RFOUT_MIN_HZ ) || (freq > PLL1_RFOUT_MAX_HZ )) {
		strcpy(buf, "board -c b -k\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		// Turn OFF TXB on HPS
		uint32_t old_val;

		// disable the DSP cores
		read_hps_reg ( "txb4", &old_val);
		write_hps_reg( "txb4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txb4", &old_val);
		write_hps_reg( "txb4", old_val & (~0x100));

		tx_power[1] = PWR_OFF;

		PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting Down TXB.\n");

		return RETURN_ERROR;
	}

	// run the pll calc algorithm
	pllparam_t pll;
        long double outfreq = 0;
	outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c b \r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_tx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll);

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "rf -c b -b ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_freq_i_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(i_bias[1]));
   strcpy(buf, "rf -c b -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[1]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[1]);
   strcat(buf, " -m\r");
   send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_freq_q_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(q_bias[1]));
   strcpy(buf, "rf -c b -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[1]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[1]);
   strcat(buf, " -m\r");
   send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);

	// 0   -> 126	attenuation only
	// 127		0dB
	
	if (gain > 127)		gain = 127;
	else if (gain < 0) 	gain = 0;

	strcpy(buf, "rf -c b -a ");
	sprintf(buf + strlen(buf), "%i", 127 - gain);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_board_dump (const char* data, char* ret) {
	// send the uart commands and read back the output and write to file

	// DAC
	strcpy(buf, "dump -c b -d\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	PRINT(DUMP, "[Board: tx_b Chip: DAC] %s\n", uart_ret_buf);

	// GPIOX
	strcpy(buf, "dump -c b -g\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	PRINT(DUMP, "[Board: tx_b Chip: GPIOX] %s\n", uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "board -c b -t\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}


static int hdlr_tx_b_status_rfld (const char* data, char* ret) {
	strcpy(buf, "status -c b -l\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_b_status_dacld(const char* data, char* ret) {
	strcpy(buf, "status -c b -p\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_b_status_dacctr(const char* data, char* ret) {
	strcpy(buf, "status -c b -e\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_b_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l\r");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
	uint16_t base_factor, resamp_factor;
	double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base rate
	base_factor   = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
	resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate
	memset(ret, 0, MAX_PROP_LEN);

	if (resamp_err < base_err) {
		write_hps_reg( "txb1", resamp_factor);
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)(resamp_factor + 1));
		//Set gain adjustment
		read_hps_reg( "txga",  &old_val);
		write_hps_reg( "txga", (old_val & ~(0xff << 8) ) | (interp_gain_lut[(resamp_factor)] << 8));
	} else {
		write_hps_reg( "txb1", base_factor);
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)(base_factor + 1));
		//Set gain adjustment
		read_hps_reg( "txga",  &old_val);
		write_hps_reg( "txga", (old_val & ~(0xff << 8) ) | (interp_gain_lut[(base_factor)] << 8));
	}

	// DSP Reset
	read_hps_reg(  "txb4", &old_val);
	write_hps_reg( "txb4", old_val |  0x2);
	write_hps_reg( "txb4", old_val & ~0x2);

	return RETURN_SUCCESS;
}

static int hdlr_tx_b_dsp_nco_adj (const char* data, char* ret) {
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
	write_hps_reg( "txb0", nco_steps);
	if (direction > 0) {
		sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
	} else {
		sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
	}

	// write direction
	read_hps_reg(  "txb4", &old_val);
	write_hps_reg( "txb4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txb4", &old_val);
	write_hps_reg( "txb4", old_val |  0x2);
	write_hps_reg( "txb4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txb4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "txb4", old_val | (1 << 14));
	else				write_hps_reg( "txb4", old_val & ~(1 << 14));

	//sync_channels( 15 );

	return RETURN_SUCCESS;
}

static int hdlr_tx_b_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "txb5", port);
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_qa_fifo_lvl (const char* data, char* ret) {
	uint32_t lvl;
	read_hps_reg( "res_ro5", & lvl );
	lvl &= 0xffff;
	sprintf( ret, "%u", lvl );
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_qa_oflow (const char* data, char* ret) {
	uint32_t count;
	// this is technically a 64-bit register, but we currently only need the bottom 32-bits
	read_hps_reg( "flc16", & count );
	sprintf( ret, "%u", count );
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_qa_uflow (const char* data, char* ret) {
	uint32_t count;
	// this is technically a 64-bit register, but we currently only need the bottom 32-bits
	read_hps_reg( "flc8", & count );
	sprintf( ret, "%u", count );
	return RETURN_SUCCESS;
}

static int hdlr_tx_b_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
   uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && tx_power[1] == PWR_ON)  return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		tx_power[1] = PWR_ON;

      // board commands
		strcpy(buf, "board -c b -d\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
		usleep(200000);

		// disable dsp channels
      for(i = 0; i < (NUM_CHANNELS * 2); i++) {
         read_hps_reg ( reg4[i], &old_val);
		   write_hps_reg( reg4[i], old_val & ~0x100);
      }

      // send sync pulse
      sync_channels(15);

		// enable active dsp channels, and reset the DSP
      for(i = 0; i < NUM_CHANNELS; i++) {
         if (tx_power[i] == PWR_ON) {
            read_hps_reg ( reg4[i+4], &old_val);
	         write_hps_reg( reg4[i+4], old_val | 0x100);
        		read_hps_reg ( reg4[i+4], &old_val);
   		   write_hps_reg( reg4[i+4], old_val | 0x2);
	      	write_hps_reg( reg4[i+4], old_val & (~0x2));
         }
         if (rx_power[i] == PWR_ON) {
      		read_hps_reg ( reg4[i], &old_val);
		      write_hps_reg( reg4[i], old_val | 0x100);
        		read_hps_reg ( reg4[i], &old_val);
   		   write_hps_reg( reg4[i], old_val | 0x2);
	      	write_hps_reg( reg4[i], old_val & (~0x2));
         }
      }

	// power off
	} else {
		// kill the channel
		strcpy(buf, "board -c b -k\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		// disable the DSP cores
		read_hps_reg ( "txb4", &old_val);
		write_hps_reg( "txb4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txb4", &old_val);
		write_hps_reg( "txb4", old_val & (~0x100));

		tx_power[1] = PWR_OFF;
	}

	return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_freq_val (const char* data, char* ret) {
	uint64_t freq = 0;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c b -z\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

        // if freq out of bounds, kill channel
        if (( freq < PLL1_RFOUT_MIN_HZ ) || (freq > PLL1_RFOUT_MAX_HZ )) {
		strcpy(buf, "board -c b -k\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		// Turn OFF RXB on HPS
		uint32_t old_val;

		// disable DSP core
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | 0x2);

		// disable channel
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & (~0x100));

		rx_power[1] = PWR_OFF;
		rx_stream[1] = STREAM_OFF;

		PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting Down RXB.\n");

		return RETURN_ERROR;
	}

	// run the pll calc algorithm
	pllparam_t pll;
        long double outfreq = 0;
	outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c b \r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_rx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll);

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "rf -c b -l ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "rf -c b -b ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);
	
	if (gain > 126)		gain = 126;
	else if (gain < 0)	gain  = 0;

	if (gain % 2) gain++;		// Odd Number

	// 0 -> 126 gain
	strcpy(buf, "vga -c b -g ");
	sprintf(buf + strlen(buf), "%i", gain >> 1);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_atten_val(const char* data, char* ret) {
	int atten;
	sscanf(data, "%i", &atten);

	if (atten > 127)		atten = 127;
	else if (atten < 0) 	atten = 0;

	strcpy(buf, "rf -c b -a ");
	sprintf(buf + strlen(buf), "%i", atten);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_board_dump (const char* data, char* ret) {
	// send the uart commands and read back the output and write to file

	// ADC
	strcpy(buf, "dump -c b -a\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	PRINT(DUMP, "[Board: rx_b Chip: ADC] %s\n", uart_ret_buf);

	// GPIOX
	strcpy(buf, "dump -c b -g\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	PRINT(DUMP, "[Board: rx_b Chip: GPIOX] %s\n", uart_ret_buf);

	// ADC Driver
	strcpy(buf, "dump -c b -v\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	PRINT(DUMP, "[Board: rx_b Chip: ADC Driver] %s\n", uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "board -c b -t\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_b_status_rfld (const char* data, char* ret) {
	strcpy(buf, "status -c b -l\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_b_status_adcalarm (const char* data, char* ret) {
	strcpy(buf, "status -c b -a\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_b_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l\r");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_dsp_signed (const char* data, char* ret) {
   uint32_t old_val, sign;
   sscanf(data, "%u", &sign);
   sign = sign ? 0 : 1;

	read_hps_reg( "rxb4", &old_val);
   old_val &= ~(1 << 4);
	write_hps_reg( "rxb4", old_val | (sign << 4));
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
	uint16_t base_factor, resamp_factor;
	double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base rate
	base_factor   = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
	resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate
	memset(ret, 0, MAX_PROP_LEN);
	int gain_factor;

	if (resamp_err < base_err) {
		write_hps_reg( "rxb1", resamp_factor);
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)(resamp_factor + 1));
		//Set gain adjustment
		gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 8) )  | (((uint16_t)gain_factor) << 8));
	} else {
		write_hps_reg( "rxb1", base_factor);
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)(base_factor + 1));
		//Set gain adjustment
		gain_factor = decim_gain_lut[(resamp_factor)];
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 8) ) | (((uint16_t)gain_factor) << 8));
	}

	// DSP Reset
	read_hps_reg(  "rxb4", &old_val);
	write_hps_reg( "rxb4", old_val |  0x2);
	write_hps_reg( "rxb4", old_val & ~0x2);

	return RETURN_SUCCESS;
}

static int hdlr_rx_b_dsp_nco_adj (const char* data, char* ret) {
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
	write_hps_reg( "rxb0", nco_steps);
	if (direction > 0) {
		sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
	} else {
		sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
	}

	// write direction
	read_hps_reg(  "rxb4", &old_val);
	write_hps_reg( "rxb4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxb4", &old_val);
	write_hps_reg( "rxb4", old_val |  0x2);
	write_hps_reg( "rxb4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_dsp_loopback (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxb4", &old_val);
	if (strcmp(data, "1") == 0)   write_hps_reg( "rxb4", (old_val & ~0x1e00) | 0x400);
   else                          write_hps_reg( "rxb4", (old_val & ~0x1e00) | 0x200);
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxb4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "rxb4", old_val | (1 << 14));
	else				write_hps_reg( "rxb4", old_val & ~(1 << 14));

	//sync_channels( 15 );

	return RETURN_SUCCESS;
}

static int hdlr_rx_b_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "rxb8", port);
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_link_ip_dest (const char* data, char* ret) {
	uint8_t ip[4];
	sscanf(data, "%"SCNd8".%"SCNd8".%"SCNd8".%"SCNd8"", ip, ip+1, ip+2, ip+3);
	write_hps_reg( "rxb5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_link_mac_dest (const char* data, char* ret) {
	uint8_t mac[6];
	sscanf(data, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8"",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	write_hps_reg( "rxb6", (mac[0] << 8) | (mac[1]) );
	write_hps_reg( "rxb7", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
	return RETURN_SUCCESS;
}

static int hdlr_rx_b_stream (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t stream;
	sscanf(data, "%"SCNd8"", &stream);

	// if stream > 1, check the status of the stream
	if (stream > 1) {
		sprintf(ret, "%u", rx_stream[1]);	// Alert File Tree
		return RETURN_SUCCESS;
	}

	// Stream is already ON or OFF then return
	if (stream == rx_stream[1]) return RETURN_SUCCESS;

	// Otherwise make the change accordingly
	if (stream > 0) {	// TURN THE STREAM ON
		if (rx_power[1] == PWR_ON) {
			read_hps_reg ( reg4[1], &old_val);
			write_hps_reg( reg4[1], old_val | 0x100);

			read_hps_reg ( reg4[1], &old_val);
			write_hps_reg( reg4[1], old_val | 0x2);
			write_hps_reg( reg4[1], old_val & (~0x2));

			rx_stream[1] = STREAM_ON;
		} else {
			// Do not turn ON stream if channel is OFF
			sprintf(ret, "%u", 0);	// Alert File Tree
		}
	} else {			// TURN THE STREAM OFF
		// disable DSP core
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | 0x2);

		// disable channel
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & (~0x100));

		rx_stream[1] = STREAM_OFF;
	}

	return RETURN_SUCCESS;
}

static int hdlr_rx_b_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
	uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && rx_power[1] == PWR_ON)  return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		rx_power[1] = PWR_ON;

		// board commands
		strcpy(buf, "board -c b -d\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
		usleep(200000);

		// disable dsp channels
		for(i = 0; i < (NUM_CHANNELS * 2); i++) {
			read_hps_reg ( reg4[i], &old_val);
			write_hps_reg( reg4[i], old_val & ~0x100);
		}

		// send sync pulse
		sync_channels(15);

		// Enable active dsp channels, and reset DSP
		for (i = 0; i < NUM_CHANNELS; i++) {
			if (tx_power[i] == PWR_ON) {
				read_hps_reg ( reg4[i+4], &old_val);
				write_hps_reg( reg4[i+4], old_val | 0x100);
				read_hps_reg ( reg4[i+4], &old_val);
				write_hps_reg( reg4[i+4], old_val | 0x2);
				write_hps_reg( reg4[i+4], old_val & (~0x2));
			}
			if (rx_stream[i] == STREAM_ON) {
				read_hps_reg ( reg4[i], &old_val);
				write_hps_reg( reg4[i], old_val | 0x100);
				read_hps_reg ( reg4[i], &old_val);
				write_hps_reg( reg4[i], old_val | 0x2);
				write_hps_reg( reg4[i], old_val & (~0x2));
			}
		}

	// power off
	} else {
		rx_power[1] = PWR_OFF;
		rx_stream[1] = STREAM_OFF;

		// kill the channel
		strcpy(buf, "board -c b -k\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		// disable DSP core
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | 0x2);

		// disable channel
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & (~0x100));
	}
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_dac_dither_en (const char* data, char* ret) {
	int r;
	int en;

	r = sscanf( data, "%d", &en );
	if ( 1 != r ) {
		return RETURN_ERROR;
	}
	if ( en < 0 || en > 1 ) {
		return RETURN_ERROR_PARAM;
	}
	snprintf( buf, sizeof( buf ), "dac -c c -l %u\r", en );
	sprintf( ret, "%u", en );
	send_uart_comm( uart_tx_fd, (uint8_t*)buf, strlen(buf) );
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_dac_dither_mixer_en (const char* data, char* ret) {
	int r;
	int en;

	r = sscanf( data, "%d", &en );
	if ( 1 != r ) {
		return RETURN_ERROR;
	}
	if ( en < 0 || en > 1 ) {
		return RETURN_ERROR_PARAM;
	}
	snprintf( buf, sizeof( buf ), "dac -c c -3 %u\r", en );
	sprintf( ret, "%u", en );
	send_uart_comm( uart_tx_fd, (uint8_t*)buf, strlen(buf) );
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_dac_dither_sra_sel (const char* data, char* ret) {
	int r;
	int db;
	int sel;

	r = sscanf( data, "%d", &db );
	if ( 1 != r ) {
		return RETURN_ERROR;
	}
	if ( db < 6 || db > 96 || 0 != db % 6 ) {
		return RETURN_ERROR_PARAM;
	}

	sprintf( ret, "%u", db );

	 // 96 dB (max) := 0x0, 90 dB := 0x1, ... 6 dB (min) := 0xf
	sel = 16 - ( db / 6 );

	snprintf( buf, sizeof( buf ), "dac -c c -b %u\r", sel );
	send_uart_comm( uart_tx_fd, (uint8_t*)buf, strlen(buf) );
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_dac_nco (const char* data, char* ret) {
	double freq;
	sscanf(data, "%lf", &freq);
	uint64_t nco_steps = (uint64_t)round(freq * DAC_NCO_CONST);
	sprintf(ret, "%lf", (double)nco_steps / DAC_NCO_CONST);

	strcpy(buf, "dac -c c -e 0 -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	strcpy(buf, "dac -o ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_dac_temp (const char* data, char* ret) {
	strcpy(buf, "board -c c -t\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_freq_val (const char* data, char* ret) {
	uint64_t freq = 0;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c c -z\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

        // if freq out of bounds, kill channel
        if (( freq < PLL1_RFOUT_MIN_HZ ) || (freq > PLL1_RFOUT_MAX_HZ )) {
		strcpy(buf, "board -c c -k\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		// Turn OFF TXC on HPS
		uint32_t old_val;

		// disable the DSP cores
		read_hps_reg ( "txc4", &old_val);
		write_hps_reg( "txc4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txc4", &old_val);
		write_hps_reg( "txc4", old_val & (~0x100));

		tx_power[2] = PWR_OFF;

		PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting Down TXC.\n");

		return RETURN_ERROR;
	}

	// run the pll calc algorithm
	pllparam_t pll;
        long double outfreq = 0;
	outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c c \r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_tx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll);

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "rf -c c -b ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_freq_i_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(i_bias[2]));
   strcpy(buf, "rf -c c -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[2]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[2]);
   strcat(buf, " -m\r");
   send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_freq_q_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(q_bias[2]));
   strcpy(buf, "rf -c c -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[2]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[2]);
   strcat(buf, " -m\r");
   send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);

	// 0   -> 126	attenuation only
	// 127		0dB
	
	if (gain > 127)		gain = 127;
	else if (gain < 0) 	gain = 0;

	strcpy(buf, "rf -c c -a ");
	sprintf(buf + strlen(buf), "%i", 127 - gain);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_board_dump (const char* data, char* ret) {
	// send the uart commands and read back the output and write to file

	// DAC
	strcpy(buf, "dump -c c -d\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	PRINT(DUMP, "[Board: tx_c Chip: DAC] %s\n", uart_ret_buf);

	// GPIOX
	strcpy(buf, "dump -c c -g\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	PRINT(DUMP, "[Board: tx_c Chip: GPIOX] %s\n", uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "board -c c -t\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_c_status_rfld (const char* data, char* ret) {
	strcpy(buf, "status -c c -l\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_c_status_dacld(const char* data, char* ret) {
	strcpy(buf, "status -c c -p\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_c_status_dacctr(const char* data, char* ret) {
	strcpy(buf, "status -c c -e\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_c_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l\r");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
	uint16_t base_factor, resamp_factor;
	double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base rate
	base_factor   = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
	resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate
	memset(ret, 0, MAX_PROP_LEN);

	if (resamp_err < base_err) {
		write_hps_reg( "txc1", resamp_factor);
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)(resamp_factor + 1));
		//Set gain adjustment
		read_hps_reg( "txga",  &old_val);
		write_hps_reg( "txga", (old_val & ~(0xff << 16) ) | (interp_gain_lut[(resamp_factor)] << 16));
	} else {
		write_hps_reg( "txc1", base_factor);
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)(base_factor + 1));
		//Set gain adjustment
		read_hps_reg( "txga",  &old_val);
		write_hps_reg( "txga", (old_val & ~(0xff << 16) ) | (interp_gain_lut[(base_factor)] << 16));
	}

	// DSP Reset
	read_hps_reg(  "txc4", &old_val);
	write_hps_reg( "txc4", old_val |  0x2);
	write_hps_reg( "txc4", old_val & ~0x2);

	return RETURN_SUCCESS;
}

static int hdlr_tx_c_dsp_nco_adj (const char* data, char* ret) {
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
	write_hps_reg( "txc0", nco_steps);
	if (direction > 0) {
		sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
	} else {
		sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
	}

	// write direction
	read_hps_reg(  "txc4", &old_val);
	write_hps_reg( "txc4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txc4", &old_val);
	write_hps_reg( "txc4", old_val |  0x2);
	write_hps_reg( "txc4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txc4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "txc4", old_val | (1 << 14));
	else				write_hps_reg( "txc4", old_val & ~(1 << 14));

	//sync_channels( 15 );

	return RETURN_SUCCESS;
}

static int hdlr_tx_c_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "txc5", port);
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_qa_fifo_lvl (const char* data, char* ret) {
	uint32_t lvl;
	read_hps_reg( "res_ro6", & lvl );
	lvl &= 0xffff;
	sprintf( ret, "%u", lvl );
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_qa_oflow (const char* data, char* ret) {
	uint32_t count;
	// this is technically a 64-bit register, but we currently only need the bottom 32-bits
	read_hps_reg( "flc18", & count );
	sprintf( ret, "%u", count );
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_qa_uflow (const char* data, char* ret) {
	uint32_t count;
	// this is technically a 64-bit register, but we currently only need the bottom 32-bits
	read_hps_reg( "flc10", & count );
	sprintf( ret, "%u", count );
	return RETURN_SUCCESS;
}

static int hdlr_tx_c_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
   uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && tx_power[2] == PWR_ON)  return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		tx_power[2] = PWR_ON;

      // board commands
		strcpy(buf, "board -c c -d\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
		usleep(200000);

		// disable dsp channels
      for(i = 0; i < (NUM_CHANNELS * 2); i++) {
         read_hps_reg ( reg4[i], &old_val);
		   write_hps_reg( reg4[i], old_val & ~0x100);
      }

      // send sync pulse
      sync_channels(15);

		// enable active dsp channels, and reset the DSP
      for(i = 0; i < NUM_CHANNELS; i++) {
         if (tx_power[i] == PWR_ON) {
            read_hps_reg ( reg4[i+4], &old_val);
	         write_hps_reg( reg4[i+4], old_val | 0x100);
        		read_hps_reg ( reg4[i+4], &old_val);
   		   write_hps_reg( reg4[i+4], old_val | 0x2);
	      	write_hps_reg( reg4[i+4], old_val & (~0x2));
         }
         if (rx_power[i] == PWR_ON) {
      		read_hps_reg ( reg4[i], &old_val);
		      write_hps_reg( reg4[i], old_val | 0x100);
        		read_hps_reg ( reg4[i], &old_val);
   		   write_hps_reg( reg4[i], old_val | 0x2);
	      	write_hps_reg( reg4[i], old_val & (~0x2));
         }
      }

	// power off
	} else {
		// kill the channel
		strcpy(buf, "board -c c -k\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		// disable the DSP cores
		read_hps_reg ( "txc4", &old_val);
		write_hps_reg( "txc4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txc4", &old_val);
		write_hps_reg( "txc4", old_val & (~0x100));

		tx_power[2] = PWR_OFF;
	}

	return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_freq_val (const char* data, char* ret) {
	uint64_t freq = 0;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c c -z\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

        // if freq out of bounds, kill channel
        if (( freq < PLL1_RFOUT_MIN_HZ ) || (freq > PLL1_RFOUT_MAX_HZ )) {
		strcpy(buf, "board -c c -k\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		// Turn OFF RXC on HPS
		uint32_t old_val;

		// disable the DSP cores
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & (~0x100));

		rx_power[2] = PWR_OFF;
		rx_stream[2] = STREAM_OFF;

		PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting Down RXC.\n");

		return RETURN_ERROR;
	}

	// run the pll calc algorithm
	pllparam_t pll;
        long double outfreq = 0;
	outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c c \r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_rx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll);

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "rf -c c -l ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "rf -c c -b ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);
	
	if (gain > 126)		gain = 126;
	else if (gain < 0)	gain  = 0;

	if (gain % 2) gain++;		// Odd Number

	// 0 -> 126 gain
	strcpy(buf, "vga -c c -g ");
	sprintf(buf + strlen(buf), "%i", gain >> 1);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_atten_val(const char* data, char* ret) {
	int atten;
	sscanf(data, "%i", &atten);

	if (atten > 127)		atten = 127;
	else if (atten < 0) 	atten = 0;

	strcpy(buf, "rf -c c -a ");
	sprintf(buf + strlen(buf), "%i", atten);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_board_dump (const char* data, char* ret) {
	// send the uart commands and read back the output and write to file

	// ADC
	strcpy(buf, "dump -c c -a\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	PRINT(DUMP, "[Board: rx_c Chip: ADC] %s\n", uart_ret_buf);

	// GPIOX
	strcpy(buf, "dump -c c -g\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	PRINT(DUMP, "[Board: rx_c Chip: GPIOX] %s\n", uart_ret_buf);

	// ADC Driver
	strcpy(buf, "dump -c c -v\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	PRINT(DUMP, "[Board: rx_c Chip: ADC Driver] %s\n", uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "board -c c -t\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_c_status_rfld (const char* data, char* ret) {
	strcpy(buf, "status -c c -l\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_c_status_adcalarm (const char* data, char* ret) {
	strcpy(buf, "status -c c -a\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_c_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l\r");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_dsp_signed (const char* data, char* ret) {
   uint32_t old_val, sign;
   sscanf(data, "%u", &sign);
   sign = sign ? 0 : 1;

	read_hps_reg( "rxc4", &old_val);
   old_val &= ~(1 << 4);
	write_hps_reg( "rxc4", old_val | (sign << 4));
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
	uint16_t base_factor, resamp_factor;
	double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base rate
	base_factor   = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
	resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate
	memset(ret, 0, MAX_PROP_LEN);
	int gain_factor;
	if (resamp_err < base_err) {
		write_hps_reg( "rxc1", resamp_factor);
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)(resamp_factor + 1));
		//Set gain adjustment
		gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 16) ) | (((uint16_t)gain_factor) << 16));
	} else {
		write_hps_reg( "rxc1", base_factor);
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)(base_factor + 1));
		//Set gain adjustment
		gain_factor = decim_gain_lut[(resamp_factor)];
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 16) ) | (((uint16_t)gain_factor) << 16));
	}

	// DSP Reset
	read_hps_reg(  "rxc4", &old_val);
	write_hps_reg( "rxc4", old_val |  0x2);
	write_hps_reg( "rxc4", old_val & ~0x2);

	return RETURN_SUCCESS;
}

static int hdlr_rx_c_dsp_nco_adj (const char* data, char* ret) {
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
	write_hps_reg( "rxc0", nco_steps);
	if (direction > 0) {
		sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
	} else {
		sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
	}

	// write direction
	read_hps_reg(  "rxc4", &old_val);
	write_hps_reg( "rxc4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxc4", &old_val);
	write_hps_reg( "rxc4", old_val |  0x2);
	write_hps_reg( "rxc4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_dsp_loopback (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxc4", &old_val);
	if (strcmp(data, "1") == 0)   write_hps_reg( "rxc4", (old_val & ~0x1e00) | 0x400);
   else                          write_hps_reg( "rxc4", (old_val & ~0x1e00) | 0x000);
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxc4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "rxc4", old_val | (1 << 14));
	else				write_hps_reg( "rxc4", old_val & ~(1 << 14));

	//sync_channels( 15 );

	return RETURN_SUCCESS;
}

static int hdlr_rx_c_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "rxc8", port);
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_link_ip_dest (const char* data, char* ret) {
	uint8_t ip[4];
	sscanf(data, "%"SCNd8".%"SCNd8".%"SCNd8".%"SCNd8"", ip, ip+1, ip+2, ip+3);
	write_hps_reg( "rxc5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_link_mac_dest (const char* data, char* ret) {
	uint8_t mac[6];
	sscanf(data, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8"",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	write_hps_reg( "rxc6", (mac[0] << 8) | (mac[1]) );
	write_hps_reg( "rxc7", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
	return RETURN_SUCCESS;
}

static int hdlr_rx_c_stream (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t stream;
	sscanf(data, "%"SCNd8"", &stream);

	// if stream > 1, check the status of the stream
	if (stream > 1) {
		sprintf(ret, "%u", rx_stream[2]);	// Alert File Tree
		return RETURN_SUCCESS;
	}

	// Stream is already ON or OFF then return
	if (stream == rx_stream[2]) return RETURN_SUCCESS;

	// Otherwise make the change accordingly
	if (stream > 0) {	// TURN THE STREAM ON
		if (rx_power[2] == PWR_ON) {
			read_hps_reg ( reg4[2], &old_val);
			write_hps_reg( reg4[2], old_val | 0x100);

			read_hps_reg ( reg4[2], &old_val);
			write_hps_reg( reg4[2], old_val | 0x2);
			write_hps_reg( reg4[2], old_val & (~0x2));

			rx_stream[2] = STREAM_ON;
		} else {
			// Do not turn ON stream if channel is OFF
			sprintf(ret, "%u", 0);	// Alert File Tree
		}
	} else {			// TURN THE STREAM OFF
		// disable DSP core
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | 0x2);

		// disable channel
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & (~0x100));

		rx_stream[2] = STREAM_OFF;
	}

	return RETURN_SUCCESS;
}

static int hdlr_rx_c_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
	uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && rx_power[2] == PWR_ON)  return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		rx_power[2] = PWR_ON;

		// board commands
		strcpy(buf, "board -c c -d\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
		usleep(200000);

		// disable dsp channels
		for(i = 0; i < (NUM_CHANNELS * 2); i++) {
			read_hps_reg ( reg4[i], &old_val);
			write_hps_reg( reg4[i], old_val & ~0x100);
		}

		// send sync pulse
		sync_channels(15);

		// Enable active dsp channels, and reset DSP
		for (i = 0; i < NUM_CHANNELS; i++) {
			if (tx_power[i] == PWR_ON) {
				read_hps_reg ( reg4[i+4], &old_val);
				write_hps_reg( reg4[i+4], old_val | 0x100);
				read_hps_reg ( reg4[i+4], &old_val);
				write_hps_reg( reg4[i+4], old_val | 0x2);
				write_hps_reg( reg4[i+4], old_val & (~0x2));
			}
			if (rx_stream[i] == STREAM_ON) {
				read_hps_reg ( reg4[i], &old_val);
				write_hps_reg( reg4[i], old_val | 0x100);
				read_hps_reg ( reg4[i], &old_val);
				write_hps_reg( reg4[i], old_val | 0x2);
				write_hps_reg( reg4[i], old_val & (~0x2));
			}
		}

	// power off
	} else {
		// kill the channel
		strcpy(buf, "board -c c -k\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		// disable the DSP cores
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & (~0x100));

		rx_power[2] = PWR_OFF;
		rx_stream[2] = STREAM_OFF;
	}
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_dac_dither_en (const char* data, char* ret) {
	int r;
	int en;

	r = sscanf( data, "%d", &en );
	if ( 1 != r ) {
		return RETURN_ERROR;
	}
	if ( en < 0 || en > 1 ) {
		return RETURN_ERROR_PARAM;
	}
	snprintf( buf, sizeof( buf ), "dac -c d -l %u\r", en );
	sprintf( ret, "%u", en );
	send_uart_comm( uart_tx_fd, (uint8_t*)buf, strlen(buf) );
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_dac_dither_mixer_en (const char* data, char* ret) {
	int r;
	int en;

	r = sscanf( data, "%d", &en );
	if ( 1 != r ) {
		return RETURN_ERROR;
	}
	if ( en < 0 || en > 1 ) {
		return RETURN_ERROR_PARAM;
	}
	snprintf( buf, sizeof( buf ), "dac -c d -3 %u\r", en );
	sprintf( ret, "%u", en );
	send_uart_comm( uart_tx_fd, (uint8_t*)buf, strlen(buf) );
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_dac_dither_sra_sel (const char* data, char* ret) {
	int r;
	int db;
	int sel;

	r = sscanf( data, "%d", &db );
	if ( 1 != r ) {
		return RETURN_ERROR;
	}
	if ( db < 6 || db > 96 || 0 != db % 6 ) {
		return RETURN_ERROR_PARAM;
	}

	sprintf( ret, "%u", db );

	 // 96 dB (max) := 0x0, 90 dB := 0x1, ... 6 dB (min) := 0xf
	sel = 16 - ( db / 6 );

	snprintf( buf, sizeof( buf ), "dac -c d -b %u\r", sel );
	send_uart_comm( uart_tx_fd, (uint8_t*)buf, strlen(buf) );
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_dac_nco (const char* data, char* ret) {
	double freq;
	sscanf(data, "%lf", &freq);
	uint64_t nco_steps = (uint64_t)round(freq * DAC_NCO_CONST);
	sprintf(ret, "%lf", (double)nco_steps / DAC_NCO_CONST);

	strcpy(buf, "dac -c d -e 1 -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(nco_steps >> 32));
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	strcpy(buf, "dac -o ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)nco_steps);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_dac_temp (const char* data, char* ret) {
	strcpy(buf, "board -c d -t\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_freq_val (const char* data, char* ret) {
	uint64_t freq = 0;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c d -z\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

        // if freq out of bounds, kill channel
        if (( freq < PLL1_RFOUT_MIN_HZ ) || (freq > PLL1_RFOUT_MAX_HZ )) {
		strcpy(buf, "board -c d -k\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		// Turn OFF TXD on HPS
		uint32_t old_val;

		// disable the DSP cores
		read_hps_reg ( "txd4", &old_val);
		write_hps_reg( "txd4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txd4", &old_val);
		write_hps_reg( "txd4", old_val & (~0x100));

		tx_power[3] = PWR_OFF;

		PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting Down TXD.\n");

		return RETURN_ERROR;
	}

	// run the pll calc algorithm
	pllparam_t pll;
        long double outfreq = 0;
	outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c d \r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_tx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll);

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "rf -c d -b ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_freq_i_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(i_bias[3]));
   strcpy(buf, "rf -c d -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[3]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[3]);
   strcat(buf, " -m\r");
   send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_freq_q_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(q_bias[3]));
   strcpy(buf, "rf -c d -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[3]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[3]);
   strcat(buf, " -m\r");
   send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);

	// 0   -> 126	attenuation only
	// 127		0dB
	
	if (gain > 127)		gain = 127;
	else if (gain < 0) 	gain = 0;

	strcpy(buf, "rf -c d -a ");
	sprintf(buf + strlen(buf), "%i", 127 - gain);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_board_dump (const char* data, char* ret) {
	// send the uart commands and read back the output and write to file

	// DAC
	strcpy(buf, "dump -c d -d\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	PRINT(DUMP, "[Board: tx_d Chip: DAC] %s\n", uart_ret_buf);

	// GPIOX
	strcpy(buf, "dump -c d -g\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	PRINT(DUMP, "[Board: tx_d Chip: GPIOX] %s\n", uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "board -c d -t\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_d_status_rfld (const char* data, char* ret) {
	strcpy(buf, "status -c d -l\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_d_status_dacld(const char* data, char* ret) {
	strcpy(buf, "status -c d -p\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_d_status_dacctr(const char* data, char* ret) {
	strcpy(buf, "status -c d -e\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_tx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_tx_d_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l\r");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
	uint16_t base_factor, resamp_factor;
	double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base rate
	base_factor   = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
	resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate
	memset(ret, 0, MAX_PROP_LEN);

	if (resamp_err < base_err) {
		write_hps_reg( "txd1", resamp_factor);
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)(resamp_factor + 1));
		//Set gain adjustment
		read_hps_reg( "txga",  &old_val);
		write_hps_reg( "txga", (old_val & ~(0xff << 24) ) | (interp_gain_lut[(resamp_factor)] << 24));
	} else {
		write_hps_reg( "txd1", base_factor);
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)(base_factor + 1));
		//Set gain adjustment
		read_hps_reg( "txga",  &old_val);
		write_hps_reg( "txga", (old_val & ~(0xff << 24) ) | (interp_gain_lut[(base_factor)] << 24));
	}

	// DSP Reset
	read_hps_reg(  "txd4", &old_val);
	write_hps_reg( "txd4", old_val |  0x2);
	write_hps_reg( "txd4", old_val & ~0x2);

	return RETURN_SUCCESS;
}

static int hdlr_tx_d_dsp_nco_adj (const char* data, char* ret) {
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
	write_hps_reg( "txd0", nco_steps);
	if (direction > 0) {
		sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
	} else {
		sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
	}

	// write direction
	read_hps_reg(  "txd4", &old_val);
	write_hps_reg( "txd4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txd4", &old_val);
	write_hps_reg( "txd4", old_val |  0x2);
	write_hps_reg( "txd4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txd4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "txd4", old_val | (1 << 14));
	else				write_hps_reg( "txd4", old_val & ~(1 << 14));

	//sync_channels( 15 );

	return RETURN_SUCCESS;
}

static int hdlr_tx_d_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "txd5", port);
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_qa_fifo_lvl (const char* data, char* ret) {
	uint32_t lvl;
	read_hps_reg( "res_ro7", & lvl );
	lvl &= 0xffff;
	sprintf( ret, "%u", lvl );
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_qa_oflow (const char* data, char* ret) {
	uint32_t count;
	// this is technically a 64-bit register, but we currently only need the bottom 32-bits
	read_hps_reg( "flc20", & count );
	sprintf( ret, "%u", count );
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_qa_uflow (const char* data, char* ret) {
	uint32_t count;
	// this is technically a 64-bit register, but we currently only need the bottom 32-bits
	read_hps_reg( "flc12", & count );
	sprintf( ret, "%u", count );
	return RETURN_SUCCESS;
}

static int hdlr_tx_d_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
   uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && tx_power[3] == PWR_ON)  return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		tx_power[3] = PWR_ON;

      // board commands
		strcpy(buf, "board -c d -d\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
		usleep(200000);

		// disable dsp channels
      for(i = 0; i < (NUM_CHANNELS * 2); i++) {
         read_hps_reg ( reg4[i], &old_val);
		   write_hps_reg( reg4[i], old_val & ~0x100);
      }

      // send sync pulse
      sync_channels(15);

		// enable active dsp channels, and reset the DSP
      for(i = 0; i < NUM_CHANNELS; i++) {
         if (tx_power[i] == PWR_ON) {
            read_hps_reg ( reg4[i+4], &old_val);
	         write_hps_reg( reg4[i+4], old_val | 0x100);
        		read_hps_reg ( reg4[i+4], &old_val);
   		   write_hps_reg( reg4[i+4], old_val | 0x2);
	      	write_hps_reg( reg4[i+4], old_val & (~0x2));
         }
         if (rx_power[i] == PWR_ON) {
      		read_hps_reg ( reg4[i], &old_val);
		      write_hps_reg( reg4[i], old_val | 0x100);
        		read_hps_reg ( reg4[i], &old_val);
   		   write_hps_reg( reg4[i], old_val | 0x2);
	      	write_hps_reg( reg4[i], old_val & (~0x2));
         }
      }

	// power off
	} else {
		// kill the channel
		strcpy(buf, "board -c d -k\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		// disable the DSP cores
		read_hps_reg ( "txd4", &old_val);
		write_hps_reg( "txd4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txd4", &old_val);
		write_hps_reg( "txd4", old_val & (~0x100));

		tx_power[3] = PWR_OFF;
	}
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_freq_val (const char* data, char* ret) {
	uint64_t freq = 0;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c d -z\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

        // if freq out of bounds, kill channel
        if (( freq < PLL1_RFOUT_MIN_HZ ) || (freq > PLL1_RFOUT_MAX_HZ )) {
		strcpy(buf, "board -c d -k\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		// Turn OFF RXD on HPS
		uint32_t old_val;

		// disable the DSP cores
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & (~0x100));

		rx_power[3] = PWR_OFF;
		rx_stream[3] = STREAM_OFF;

		PRINT(ERROR, "Requested Synthesizer Frequency is < 53 MHz: Shutting Down RXD.\n");

		return RETURN_ERROR;
	}

	// run the pll calc algorithm
	pllparam_t pll;
        long double outfreq = 0;
	outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c d \r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_rx_fd, (uint64_t)PLL_CORE_REF_FREQ_HZ, &pll);

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "rf -c d -l ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "rf -c d -b ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);
	
	if (gain > 126)		gain = 126;
	else if (gain < 0)	gain  = 0;

	if (gain % 2) gain++;		// Odd Number

	// 0 -> 126 gain
	strcpy(buf, "vga -c d -g ");
	sprintf(buf + strlen(buf), "%i", gain >> 1);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_atten_val(const char* data, char* ret) {
	int atten;
	sscanf(data, "%i", &atten);

	if (atten > 127)		atten = 127;
	else if (atten < 0) 	atten = 0;

	strcpy(buf, "rf -c d -a ");
	sprintf(buf + strlen(buf), "%i", atten);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_board_dump (const char* data, char* ret) {
	// send the uart commands and read back the output and write to file

	// ADC
	strcpy(buf, "dump -c d -a\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	PRINT(DUMP, "[Board: rx_d Chip: ADC] %s\n", uart_ret_buf);

	// GPIOX
	strcpy(buf, "dump -c d -g\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	PRINT(DUMP, "[Board: rx_d Chip: GPIOX] %s\n", uart_ret_buf);

	// ADC Driver
	strcpy(buf, "dump -c d -v\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	PRINT(DUMP, "[Board: rx_d Chip: ADC Driver] %s\n", uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "board -c d -t\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_d_status_rfld (const char* data, char* ret) {
	strcpy(buf, "status -c d -l\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_d_status_adcalarm (const char* data, char* ret) {
	strcpy(buf, "status -c d -a\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_rx_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_rx_d_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l\r");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}


static int hdlr_rx_d_dsp_signed (const char* data, char* ret) {
   uint32_t old_val, sign;
   sscanf(data, "%u", &sign);
   sign = sign ? 0 : 1;

	read_hps_reg( "rxd4", &old_val);
   old_val &= ~(1 << 4);
	write_hps_reg( "rxd4", old_val | (sign << 4));
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
	uint16_t base_factor, resamp_factor;
	double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base rate
	base_factor   = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
	resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate
	memset(ret, 0, MAX_PROP_LEN);
	int gain_factor;

	if (resamp_err < base_err) {
		write_hps_reg( "rxd1", resamp_factor);
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)(resamp_factor + 1));
		//Set gain adjustment
		gain_factor = decim_gain_lut[(resamp_factor)] * 1.025028298;
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 24) )  | (((uint16_t)gain_factor) << 24));
	} else {
		write_hps_reg( "rxd1", base_factor);
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)(base_factor + 1));
		//Set gain adjustment
		gain_factor = decim_gain_lut[(resamp_factor)];
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 24) ) | (((uint16_t)gain_factor) << 24));
	}

	// DSP Reset
	read_hps_reg(  "rxd4", &old_val);
	write_hps_reg( "rxd4", old_val |  0x2);
	write_hps_reg( "rxd4", old_val & ~0x2);

	return RETURN_SUCCESS;
}

static int hdlr_rx_d_dsp_nco_adj (const char* data, char* ret) {
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
	write_hps_reg( "rxd0", nco_steps);
	if (direction > 0) {
		sprintf(ret, "-%lf", (double)nco_steps / DSP_NCO_CONST);
	} else {
		sprintf(ret, "%lf", (double)nco_steps / DSP_NCO_CONST);
	}

	// write direction
	read_hps_reg(  "rxd4", &old_val);
	write_hps_reg( "rxd4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxd4", &old_val);
	write_hps_reg( "rxd4", old_val |  0x2);
	write_hps_reg( "rxd4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_dsp_loopback (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxd4", &old_val);
	if (strcmp(data, "1") == 0)   write_hps_reg( "rxd4", (old_val & ~0x1e00) | 0x400);
   else                          write_hps_reg( "rxd4", (old_val & ~0x1e00) | 0x200);
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxd4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "rxd4", old_val | (1 << 14));
	else				write_hps_reg( "rxd4", old_val & ~(1 << 14));

	//sync_channels( 15 );

	return RETURN_SUCCESS;
}

static int hdlr_rx_d_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "rxd8", port);
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_link_ip_dest (const char* data, char* ret) {
	uint8_t ip[4];
	sscanf(data, "%"SCNd8".%"SCNd8".%"SCNd8".%"SCNd8"", ip, ip+1, ip+2, ip+3);
	write_hps_reg( "rxd5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_link_mac_dest (const char* data, char* ret) {
	uint8_t mac[6];
	sscanf(data, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8"",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	write_hps_reg( "rxd6", (mac[0] << 8) | (mac[1]) );
	write_hps_reg( "rxd7", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
	return RETURN_SUCCESS;
}

static int hdlr_rx_d_stream (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t stream;
	sscanf(data, "%"SCNd8"", &stream);

	// if stream > 1, check the status of the stream
	if (stream > 1) {
		sprintf(ret, "%u", rx_stream[3]);	// Alert File Tree
		return RETURN_SUCCESS;
	}

	// Stream is already ON or OFF then return
	if (stream == rx_stream[3]) return RETURN_SUCCESS;

	// Otherwise make the change accordingly
	if (stream > 0) {	// TURN THE STREAM ON
		if (rx_power[3] == PWR_ON) {
			read_hps_reg ( reg4[3], &old_val);
			write_hps_reg( reg4[3], old_val | 0x100);

			read_hps_reg ( reg4[3], &old_val);
			write_hps_reg( reg4[3], old_val | 0x2);
			write_hps_reg( reg4[3], old_val & (~0x2));

			rx_stream[3] = STREAM_ON;
		} else {
			// Do not turn ON stream if channel is OFF
			sprintf(ret, "%u", 0);	// Alert File Tree
		}
	} else {			// TURN THE STREAM OFF
		// disable DSP core
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | 0x2);

		// disable channel
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & (~0x100));

		rx_stream[3] = STREAM_OFF;
	}

	return RETURN_SUCCESS;
}

static int hdlr_rx_d_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
	uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && rx_power[3] == PWR_ON)  return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		rx_power[3] = PWR_ON;

		// board commands
		strcpy(buf, "board -c d -d\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
		usleep(200000);

		// disable dsp channels
		for(i = 0; i < (NUM_CHANNELS * 2); i++) {
			read_hps_reg ( reg4[i], &old_val);
			write_hps_reg( reg4[i], old_val & ~0x100);
		}

		// send sync pulse
		sync_channels(15);

		// Enable active dsp channels, and reset DSP
		for (i = 0; i < NUM_CHANNELS; i++) {
			if (tx_power[i] == PWR_ON) {
				read_hps_reg ( reg4[i+4], &old_val);
				write_hps_reg( reg4[i+4], old_val | 0x100);
				read_hps_reg ( reg4[i+4], &old_val);
				write_hps_reg( reg4[i+4], old_val | 0x2);
				write_hps_reg( reg4[i+4], old_val & (~0x2));
			}
			if (rx_stream[i] == STREAM_ON) {
				read_hps_reg ( reg4[i], &old_val);
				write_hps_reg( reg4[i], old_val | 0x100);
				read_hps_reg ( reg4[i], &old_val);
				write_hps_reg( reg4[i], old_val | 0x2);
				write_hps_reg( reg4[i], old_val & (~0x2));
			}
		}

	// power off
	} else {
		// kill the channel
		strcpy(buf, "board -c d -k\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		// disable the DSP cores
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & (~0x100));

		rx_power[3] = PWR_OFF;
		rx_stream[3] = STREAM_OFF;
	}
	return RETURN_SUCCESS;
}

static int hdlr_time_clk_pps (const char* data, char* ret) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int hdlr_time_clk_cur_time (const char* data, char* ret) {
	// test by reading it before writing to it
	//uint32_t intpart, fracpart;
	//read_hps_reg( "sys5", &intpart);
	//read_hps_reg( "sys6", &fracpart);
	//printf("Time is: %lf seconds\n", (double)intpart + ((double)fracpart / 100000000) );

	//double time;
	//sscanf(data, "%lf", &time);
	//write_hps_reg( "sys7", (uint32_t)time);
	//write_hps_reg( "sys8", time - (uint32_t)time);

	long double time;
	sscanf(data, "%Lf", &time);
	write_hps_reg("sys9", (uint32_t)(((uint64_t)time) & 0x00000000FFFFFFFF));
	write_hps_reg("sys10", (uint32_t)(((uint64_t)time) >> 32) & 0x00000000FFFFFFFF);

	write_hps_reg("sys11", (uint32_t)(time-(uint64_t)time) & 0x00000000FFFFFFFF);
	//write_hps_reg("sys12", (uint32_t)((time-(uint64_t)time)>>32) & 0x00000000FFFFFFFF);
	// toggle the set register
	write_hps_reg( "sys13", 1);
	write_hps_reg( "sys13", 0);
	return RETURN_SUCCESS;
}

static int hdlr_time_source_vco (const char* data, char* ret) {
	if (strcmp(data, "external") == 0) {
		strcpy(buf, "clk -v 1\r");
	} else if (strcmp(data, "internal") == 0) {
		strcpy(buf, "clk -v 0\r");
	}
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_time_source_sync (const char* data, char* ret) {
	if (strcmp(data, "external") == 0) {
		strcpy(buf, "clk -n 1\r");
	} else if (strcmp(data, "internal") == 0) {
		strcpy(buf, "clk -n 0\r");
	}
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

// 10 MHz clock
static int hdlr_time_source_ref (const char* data, char* ret) {
	if (strcmp(data, "external") == 0) {
		strcpy(buf, "clk -t 1\r");
	} else if (strcmp(data, "internal") == 0) {
		strcpy(buf, "clk -t 0\r");
	}
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

// External Source Buffer Select
static int hdlr_time_source_extsine (const char* data, char* ret) {
	if (strcmp(data, "sine") == 0) {
		strcpy(buf, "HMC -h 1 -b 0\r");
                send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	} else if (strcmp(data, "LVPECL") == 0) {
		strcpy(buf, "HMC -h 1 -b 1\r");
                send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	} else {
                strcpy(buf, "HMC -h 1 -B\r");
                send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
                read_uart(uart_synth_fd);
                strcpy(ret, (char*)uart_ret_buf);
        }
	return RETURN_SUCCESS;
}

// Toggle SPI Sync
static int hdlr_time_sync_lmk_sync_tgl_jesd (const char* data, char* ret) {
	if (strcmp(data, "0") != 0) {
		strcpy(buf, "sync -k\r");
	} 
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}
// Toggle SPI Sync
static int hdlr_time_sync_lmk_sync_tgl_pll (const char* data, char* ret) {
	if (strcmp(data, "0") != 0) {
		strcpy(buf, "sync -q\r");
	} 
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

// Resync output edges with Ref
static int hdlr_time_sync_lmk_resync_jesd (const char* data, char* ret) {
	if (strcmp(data, "0") != 0) {
		strcpy(buf, "sync -j\r");
	} 
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

// Resync output edges with Ref
static int hdlr_time_sync_lmk_resync_pll (const char* data, char* ret) {
	if (strcmp(data, "0") != 0) {
		strcpy(buf, "sync -p\r");
	} 
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

// Resync output edges with Ref
static int hdlr_time_sync_lmk_resync_all (const char* data, char* ret) {
	if (strcmp(data, "0") != 0) {
		strcpy(buf, "sync -r\r");
	} 
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

// TODO: Enable DevClock Output
static int hdlr_time_source_devclk (const char* data, char* ret) {
	if (strcmp(data, "external") == 0) {
		strcpy(buf, "clk -t 1\r");
	} else if (strcmp(data, "internal") == 0) {
		strcpy(buf, "clk -t 0\r");
	}
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

// TODO: Enable PLL Output
static int hdlr_time_source_pll (const char* data, char* ret) {
	if (strcmp(data, "external") == 0) {
		strcpy(buf, "clk -t 1\r");
	} else if (strcmp(data, "internal") == 0) {
		strcpy(buf, "clk -t 0\r");
	}
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;

}

static int hdlr_time_status_ld (const char* data, char* ret) {
	strcpy(buf, "status -l\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);
	return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd_pll1 (const char* data, char* ret) {
	strcpy(buf, "status -l 11\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);
	return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_jesd_pll2 (const char* data, char* ret) {
	strcpy(buf, "status -l 12\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);
	return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_pll_pll1 (const char* data, char* ret) {
	strcpy(buf, "status -l 21\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);
	return RETURN_SUCCESS;
}

static int hdlr_time_status_ld_pll_pll2 (const char* data, char* ret) {
	strcpy(buf, "status -l 22\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);
	return RETURN_SUCCESS;
}

static int hdlr_time_status_lol (const char* data, char* ret) {
	strcpy(buf, "status -o\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);
	return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd_pll1 (const char* data, char* ret) {
	strcpy(buf, "status -o 11\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);
	return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_jesd_pll2 (const char* data, char* ret) {
	strcpy(buf, "status -o 12\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);
	return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_pll_pll1 (const char* data, char* ret) {
	strcpy(buf, "status -o 21\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);
	return RETURN_SUCCESS;
}

static int hdlr_time_status_lol_pll_pll2 (const char* data, char* ret) {
	strcpy(buf, "status -o 22\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);
	return RETURN_SUCCESS;
}

static int hdlr_time_board_dump (const char* data, char* ret) {
	// send the uart commands and read back the output and write to file

	// Diagnostic Dump of Clk Board
	strcpy(buf, "board -e\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	PRINT(DUMP, "[Board: Time Regdump] %s\n", uart_ret_buf);

        return RETURN_SUCCESS;
}

static int hdlr_time_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int hdlr_time_board_temp (const char* data, char* ret) {
	strcpy(buf, "board -t\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_time_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_time_about_id (const char* data, char* ret) {
	// Do Nothing, store in filesystem
	return RETURN_SUCCESS;
}

static int hdlr_time_about_serial (const char* data, char* ret) {
	strcpy(buf, "status -s\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_time_about_mcudevid (const char* data, char* ret) {
	strcpy(buf, "status -d\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_time_about_mcurev (const char* data, char* ret) {
	strcpy(buf, "status -v\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);

        return RETURN_SUCCESS;
}

static int hdlr_time_about_mcufuses (const char* data, char* ret) {
	strcpy(buf, "status -f\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_time_about_fw_ver (const char* data, char* ret) {
	strcpy(buf, "board -v\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	strcpy(ret, (char*)uart_ret_buf);

	return RETURN_SUCCESS;
}

static int hdlr_fpga_board_dump (const char* data, char* ret) {

	// dump all of the board logs
	hdlr_tx_a_rf_board_dump(NULL, NULL);
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

static int hdlr_fpga_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gle (const char* data, char* ret) {
		
	if (strcmp(data, "1") == 0) {
	    strcpy(buf, "board -g 1\r");
	    send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	    usleep(50000);

	    strcpy(buf, "board -g 1\r");
	    send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	    usleep(50000);

	    strcpy(buf, "board -g 1\r");
	    send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	    usleep(50000);
	}
	if (strcmp(data, "2") == 0) {
	    strcpy(buf, "board -g 2\r");
	    send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	    usleep(50000);

	    strcpy(buf, "board -g 2\r");
	    send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	    usleep(50000);

	    strcpy(buf, "board -g 2\r");
	    send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	    usleep(50000);
	}
	return RETURN_SUCCESS;
}


static int hdlr_fpga_board_temp (const char* data, char* ret) {
	//strcpy(buf, "board -t\r");
	//send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	//read_uart(NO_FWD_CMD);
	//strcpy(ret, (char*)uart_ret_buf);
	uint32_t old_val;
	read_hps_reg ( "sys14", &old_val);

	//mask off temp
	old_val = old_val & 0xff;

	//if value >= 0x80 (=128), subtract 0x80 and convert to int
	if (old_val >= 128){
		old_val = old_val - 128;
		sprintf(ret, "temp +%lu degC\n", old_val);
	}
	//if value < 0x80, subtract 0x3a (=58) and convert to negative int
	else if (old_val < 128){
		old_val = old_val - 58;
		sprintf(ret, "temp -%lu degC\n", old_val);
	}

	return RETURN_SUCCESS;
}

static int hdlr_fpga_board_led (const char* data, char* ret) {
	//strcpy(buf, "board -l ");
	//strcat(buf, data);
	//strcat(buf, "\r");
	//send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int hdlr_fpga_board_rstreq (const char* data, char* ret) {
	//strcpy(buf, "fpga -r \r");
	//send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	
	/* TODO: Implement DIG Board FPGA Reset */

	return RETURN_SUCCESS;
}

static int hdlr_fpga_board_reboot (const char* data, char* ret){
    if(strcmp(data, "1") == 0){
	uint32_t reboot;

	//Write 0 to bit[16] of sys 0 in order to reboot
	read_hps_reg("sys0", &reboot);
	reboot = (reboot & 0xFFFEFFFF);
	write_hps_reg("sys0", reboot);
	return RETURN_SUCCESS;
    }
    return RETURN_SUCCESS;
}

static int hdlr_fpga_board_jesd_sync (const char* data, char* ret) {
	//strcpy(buf, "fpga -o \r");
	//send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	sync_channels(15);
	return RETURN_SUCCESS;
}

static int hdlr_fpga_board_sys_rstreq (const char* data, char* ret) {
	strcpy(buf, "board -r\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	usleep(700000);

	strcpy(buf, "board -r\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));
	usleep(50000);

	strcpy(buf, "board -r\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));
	usleep(50000);

	/* TODO: Implement DIG board Reset */
	return RETURN_SUCCESS;
}

static int hdlr_fpga_board_flow_control_sfpX_port (const char* data, char* ret, unsigned sfp_port) {

	static const unsigned udp_port_max = (1 << 16) - 1;
	static const unsigned sfp_port_max = 1;

	unsigned udp_port;
	uint32_t flc0_reg;
	uint32_t mask;

	if ( sfp_port > sfp_port_max ) {
		return RETURN_ERROR_PARAM;
	}
	if ( 1 != sscanf( data, "%u", &udp_port ) ) {
		return RETURN_ERROR_PARAM;
	}

	udp_port = udp_port > udp_port_max ? udp_port_max : udp_port;

	// if number of sfp_ports ever changes, this code needs to be changed
	// a good reason to use structures to access memory-mapped registers.
	read_hps_reg( "flc0", & flc0_reg );
	mask = 0xffff << ( sfp_port * 16 );
	flc0_reg &= ~mask;
	flc0_reg |= ( udp_port << ( sfp_port * 16 ) ) & mask;
	write_hps_reg( "flc0", flc0_reg );

	sprintf( ret, "%u", udp_port );

	return RETURN_SUCCESS;
}
static inline int hdlr_fpga_board_flow_control_sfpa_port (const char* data, char* ret) {
	return hdlr_fpga_board_flow_control_sfpX_port( data, ret, 0 );
}
static inline int hdlr_fpga_board_flow_control_sfpb_port (const char* data, char* ret) {
	return hdlr_fpga_board_flow_control_sfpX_port( data, ret, 1 );
}

static int hdlr_fpga_board_fw_rst (const char* data, char* ret) {
	uint32_t old_val;

	// toggle the bit sys0[4]
	read_hps_reg ( "sys0", &old_val);
	write_hps_reg( "sys0", old_val | 0x10);
	write_hps_reg( "sys0", old_val & (~0x10));

	return RETURN_SUCCESS;
}

static int hdlr_fpga_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int hdlr_fpga_about_cmp_time (const char* data, char* ret) {
	uint32_t old_val;
	int year, month, day, hour, min;
	read_hps_reg ( "sys15", &old_val);
	//get year
	year  = (old_val & 0xfff00000) >> 20;
	month = (old_val & 0x000f0000) >> 16;
	day   = (old_val & 0x0000f800) >> 11;
	hour  = (old_val & 0x000007c0) >>  6;
	min   =  old_val & 0x0000003f;

	sprintf(ret, "cmp. time %i-%i-%i %i:%i (yyyy-MM-dd HH:mm) \n", year, month, day, hour, min);

	return RETURN_SUCCESS;
}

static int hdlr_fpga_about_conf_info (const char* data, char* ret){
	uint64_t old_val;
	read_hps_reg ( "sys18", &old_val);

	sprintf(ret,  "config. info. 0x%02x \n", old_val);
	return RETURN_SUCCESS;
}

static int hdlr_fpga_about_serial (const char* data, char* ret){
	uint64_t old_val;
	uint32_t old_val1;
	uint32_t old_val2;
	read_hps_reg ( "sys16", &old_val1);
	read_hps_reg ( "sys17", &old_val2);

	//append values
	old_val = (old_val2 << 32) | old_val1;

	sprintf(ret, "serial number 0x%02x%02x \n", old_val2, old_val1);
	return RETURN_SUCCESS;
}

static int hdlr_fpga_trigger_sma_dir (const char* data, char* ret) {
	int r;
	bool val;
	r = valid_trigger_dir( data, & val ) || set_sma_dir( val );
	return r;
}

static int hdlr_fpga_trigger_sma_pol (const char* data, char* ret) {
	int r;
	bool val;
	r = valid_trigger_pol( data, & val ) || set_sma_pol( val );
	return r;
}

// TODO: Move FWversion code to ARM, edit MAKE file with version info, refer to MCU code
static int hdlr_fpga_about_fw_ver (const char* data, char* ret) {
	//strcpy(buf, "board -v\r");
	//send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	//read_uart(NO_FWD_CMD);
	//strcpy(ret, (char*)uart_ret_buf);
	uint64_t old_val;
	uint32_t old_val1;
	uint32_t old_val2;
	read_hps_reg ( "sys3", &old_val2);
	read_hps_reg ( "sys4", &old_val1);

	// bits sys3[7:0]
	old_val2 = old_val2 & 0xff;

	//append values
	old_val = (old_val2 << 32) | old_val1;


	sprintf(ret, "ver. 0x%02x%02x \n", old_val2, old_val1);
	return RETURN_SUCCESS;
}

static int hdlr_server_about_fw_ver (const char* data, char* ret) {
	FILE *fp=NULL;
	char buf[MAX_PROP_LEN] = {0};
	if((fp = popen("/usr/bin/server -v", "r")) == NULL){
		PRINT(ERROR, "Error opening pipe!\n");
		return RETURN_ERROR;
	}
	while(fgets(buf, MAX_PROP_LEN, fp) != NULL){
		strncat(ret, buf, MAX_PROP_LEN);
	}
	if(pclose(fp)){
		return RETURN_ERROR;
	}
	return RETURN_SUCCESS;
}

static int hdlr_fpga_about_hw_ver (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg ( "sys1", &old_val);

	// bits sys1[10:7]
	old_val = (old_val >> 7) & 0xf;

	sprintf(ret, "ver. 0x%02x", old_val);
	return RETURN_SUCCESS;
}

static int hdlr_fpga_link_rate (const char* data, char* ret) {
   // TODO: Need to implement in FW
	return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpa_ip_addr (const char* data, char* ret) {
	uint32_t ip[4];
	if (ipver[0] == IPVER_IPV4) {
		sscanf(data, "%"SCNd32".%"SCNd32".%"SCNd32".%"SCNd32"",
			ip, ip+1, ip+2, ip+3);
		write_hps_reg( "net5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	} else if (ipver[0] == IPVER_IPV6) {
		sscanf(data, "%"SCNx32":%"SCNx32":%"SCNx32":%"SCNx32"",
			ip, ip+1, ip+2, ip+3);
		write_hps_reg( "net1", ip[0]);
		write_hps_reg( "net2", ip[1]);
		write_hps_reg( "net3", ip[2]);
		write_hps_reg( "net4", ip[3]);
	}
	return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpa_mac_addr (const char* data, char* ret) {
	uint8_t mac[6];
	sscanf(data, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8"",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	write_hps_reg( "net11", (mac[0] << 8) | (mac[1]) );
	write_hps_reg( "net12", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
	return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpa_ver (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t ver;
	sscanf(data, "%"SCNd8"", &ver);
	read_hps_reg(  "net0", &old_val);
	if (ver > 0)	write_hps_reg( "net0", (old_val | 0x4));
	else		write_hps_reg( "net0", (old_val & ~(0x4) ));
	return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpa_pay_len (const char* data, char* ret) {
	uint32_t old_val;
	uint32_t pay_len;
	sscanf(data, "%"SCNd32"", &pay_len);
	read_hps_reg(  "net0", &old_val);
	write_hps_reg( "net0",  (old_val & ~(0xffff0000) ) | (pay_len << 16));
	return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpb_ip_addr (const char* data, char* ret) {
	uint32_t ip[4];
	if (ipver[1] == IPVER_IPV4) {
		sscanf(data, "%"SCNd32".%"SCNd32".%"SCNd32".%"SCNd32"",
			ip, ip+1, ip+2, ip+3);
		ip[0] = (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]);
		write_hps_reg( "net20", ip[0]);
	} else if (ipver[1] == IPVER_IPV6) {
		sscanf(data, "%"SCNx32":%"SCNx32":%"SCNx32":%"SCNx32"",
			ip, ip+1, ip+2, ip+3);
		write_hps_reg( "net16", ip[0]);
		write_hps_reg( "net17", ip[1]);
		write_hps_reg( "net18", ip[2]);
		write_hps_reg( "net19", ip[3]);
	}
	return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpb_mac_addr (const char* data, char* ret) {
	uint8_t mac[6];
	sscanf(data, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8"",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	write_hps_reg( "net26", (mac[0] << 8) | (mac[1]) );
	write_hps_reg( "net27", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
	return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpb_ver (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t ver;
	sscanf(data, "%"SCNd8"", &ver);
	read_hps_reg(  "net15", &old_val);
	if (ver > 0)	write_hps_reg( "net15", (old_val & ~(1 << 2) ) | (1 << 2));
	else		write_hps_reg( "net15", (old_val & ~(1 << 2) ));
	return RETURN_SUCCESS;
}

static int hdlr_fpga_link_sfpb_pay_len (const char* data, char* ret) {
	uint32_t old_val;
	uint32_t pay_len;
	sscanf(data, "%"SCNd32"", &pay_len);
	read_hps_reg(  "net15", &old_val);
	write_hps_reg( "net15",  (old_val & ~(0xffff0000) ) | (pay_len << 16));
	return RETURN_SUCCESS;
}

static int hdlr_fpga_link_net_dhcp_en (const char* data, char* ret) {

	return RETURN_SUCCESS;
}

static int hdlr_fpga_link_net_hostname (const char* data, char* ret) {
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

static int hdlr_fpga_link_net_ip_addr (const char* data, char* ret) {
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
	strcat(command,	"/ /etc/init.d/mcu_init.sh");
	system(command);
	return RETURN_SUCCESS;
}

static int hdlr_save_config (const char* data, char* ret) {
	*_save_profile = 1;
	strcpy(_save_profile_path, data);
	return RETURN_SUCCESS;
}

static int hdlr_load_config (const char* data, char* ret) {
	*_load_profile = 1;
	strcpy(_load_profile_path, data);
	return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gps_time (const char* data, char* ret) {
	uint32_t gps_time_lh = 0, gps_time_uh = 0;
	char gps_split[MAX_PROP_LEN];

	read_hps_reg( "sys5", &gps_time_lh);
	read_hps_reg( "sys6", &gps_time_uh );

	snprintf(gps_split, MAX_PROP_LEN, "%i", gps_time_uh);
	strncpy(ret, gps_split, MAX_PROP_LEN);
	snprintf(gps_split, MAX_PROP_LEN, "%i", gps_time_lh);
	strncat(ret, gps_split, MAX_PROP_LEN);

	return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gps_frac_time (const char* data, char* ret) {
	uint32_t gps_frac_time_lh = 0, gps_frac_time_uh = 0;
	char gps_split[MAX_PROP_LEN];
	read_hps_reg( "sys7", &gps_frac_time_lh);
	read_hps_reg( "sys8", &gps_frac_time_uh);
	
	snprintf(gps_split, MAX_PROP_LEN, "%i", gps_frac_time_uh);
	strncpy(ret, gps_split, MAX_PROP_LEN);
	snprintf(gps_split, MAX_PROP_LEN, "%i", gps_frac_time_lh);
	strncat(ret, gps_split, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gps_sync_time (const char* data, char* ret) {
	uint32_t systime_lh = 0;
	uint32_t systime_uh = 0;
	read_hps_reg( "sys5", &systime_lh );
	read_hps_reg( "sys6", &systime_uh );
	write_hps_reg( "sys9", systime_lh );
	write_hps_reg( "sys10", systime_uh );
	write_hps_reg( "sys11", 0 ); // set frac_time to 0
	write_hps_reg( "sys12", 0 ); // set frac_time to 0
	write_hps_reg( "sys13", 1 ); // writing 1, then 0 to sys9 sets the time
	write_hps_reg( "sys13", 0 ); // to what is written in sys7 and sys8

	return RETURN_SUCCESS;
}

static uint16_t cm_chanmask_get( const char *path ) {
	uint32_t r;

	FILE* fp;

	r = 0;

	if ( !(fp = fopen(path, "r")) ) {
		PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
		return r;
	}
	fscanf( fp, "%x", & r );
	fclose( fp );

	return r;
}


static int hdlr_cm_chanmask_rx (const char *data, char *ret) {
	uint32_t mask;

	if ( 1 != sscanf( data, "%x", &mask ) ) {
		return RETURN_ERROR_PARAM;
	}

	mask &= 0xffff;
	sprintf( ret, "%x", mask );

	return RETURN_SUCCESS;
}

static int hdlr_cm_chanmask_tx (const char *data, char *ret) {
	uint32_t mask;

	if ( 1 != sscanf( data, "%x", &mask ) ) {
		return RETURN_ERROR_PARAM;
	}

	mask &= 0xffff;
	sprintf( ret, "%x", mask );

	return RETURN_SUCCESS;
}

static int hdlr_cm_rx_atten_val (const char *data, char *ret) {
	int r;

	char inbuf[ 256 ];
	char outbuf[ 256 ];

	uint32_t mask_rx;

	int wd_backup;
	prop_t *prop;
	int (*hdlr)( const char *, char *);
	int i;

	int atten = 0;

	mask_rx = cm_chanmask_get( "/var/crimson/state/cm/chanmask-rx" );

	sscanf( data, "%lf", & atten );

	sprintf( inbuf, "%lf", atten );

	for( i = 0; i < NUM_CHANNELS; i++ ) {

		if ( 0 == ( mask_rx & (1 << i) ) ) {
			continue;
		}

		switch( i ) {
		case 0: hdlr = hdlr_rx_a_rf_atten_val; break;
		case 1: hdlr = hdlr_rx_b_rf_atten_val; break;
		case 2: hdlr = hdlr_rx_c_rf_atten_val; break;
		case 3: hdlr = hdlr_rx_d_rf_atten_val; break;
		default: continue;
		}

		// call the handler directly
		r = hdlr( inbuf, outbuf );
		if( RETURN_SUCCESS != r ) {
			return r;
		}

		// disable inotify, write the value back to the file, re-enable inotify
		prop = get_prop_from_hdlr( hdlr );
		wd_backup = prop->wd;
		prop->wd = -1;
		set_property( prop -> path, inbuf );
		prop->wd = wd_backup;
	}

	return RETURN_SUCCESS;
}

static int hdlr_cm_rx_gain_val (const char *data, char *ret) {
	int r;

	char inbuf[ 256 ];
	char outbuf[ 256 ];

	uint32_t mask_rx;

	int wd_backup;
	prop_t *prop;
	int (*hdlr)( const char *, char *);
	int i;

	double gain = 0;

	mask_rx = cm_chanmask_get( "/var/crimson/state/cm/chanmask-rx" );

	sscanf( data, "%lf", & gain );

	sprintf( inbuf, "%lf", gain );

	for( i = 0; i < NUM_CHANNELS; i++ ) {

		if ( 0 == ( mask_rx & (1 << i) ) ) {
			continue;
		}

		switch( i ) {
		case 0: hdlr = hdlr_rx_a_rf_gain_val; break;
		case 1: hdlr = hdlr_rx_b_rf_gain_val; break;
		case 2: hdlr = hdlr_rx_c_rf_gain_val; break;
		case 3: hdlr = hdlr_rx_d_rf_gain_val; break;
		default: continue;
		}

		// call the handler directly
		r = hdlr( inbuf, outbuf );
		if( RETURN_SUCCESS != r ) {
			return r;
		}

		// disable inotify, write the value back to the file, re-enable inotify
		prop = get_prop_from_hdlr( hdlr );
		wd_backup = prop->wd;
		prop->wd = -1;
		set_property( prop -> path, inbuf );
		prop->wd = wd_backup;
	}

	return RETURN_SUCCESS;
}

static int hdlr_cm_tx_gain_val (const char *data, char *ret) {
	int r;

	char inbuf[ 256 ];
	char outbuf[ 256 ];

	uint32_t mask_tx;

	int wd_backup;
	prop_t *prop;
	int (*hdlr)( const char *, char *);
	int i;

	double gain = 0;

	mask_tx = cm_chanmask_get( "/var/crimson/state/cm/chanmask-tx" );

	sscanf( data, "%lf", & gain );

	sprintf( inbuf, "%lf", gain );

	for( i = 0; i < NUM_CHANNELS; i++ ) {

		if ( 0 == ( mask_tx & (1 << i) ) ) {
			continue;
		}

		switch( i ) {
		case 0: hdlr = hdlr_tx_a_rf_gain_val; break;
		case 1: hdlr = hdlr_tx_b_rf_gain_val; break;
		case 2: hdlr = hdlr_tx_c_rf_gain_val; break;
		case 3: hdlr = hdlr_tx_d_rf_gain_val; break;
		default: continue;
		}

		// call the handler directly
		r = hdlr( inbuf, outbuf );
		if( RETURN_SUCCESS != r ) {
			return r;
		}

		// disable inotify, write the value back to the file, re-enable inotify
		prop = get_prop_from_hdlr( hdlr );
		wd_backup = prop->wd;
		prop->wd = -1;
		set_property( prop -> path, inbuf );
		prop->wd = wd_backup;
	}

	return RETURN_SUCCESS;
}

static int hdlr_cm_trx_freq_val (const char *data, char *ret) {
	int r;

	char inbuf[ 256 ];
	char outbuf[ 256 ];

	uint32_t sync_mode;
	uint32_t sync_mask;

	uint32_t mask_rx;
	uint32_t mask_tx;

	int wd_backup;
	prop_t *prop;
	int (*hdlr)( const char *, char *);
	int i;

	double freq = 0;

	read_hps_reg( "sync_mode", & sync_mode );
	read_hps_reg( "sync_mask", & sync_mask );

	mask_rx = cm_chanmask_get( "/var/crimson/state/cm/chanmask-rx" );
	mask_tx = cm_chanmask_get( "/var/crimson/state/cm/chanmask-tx" );

	sync_mask = ( mask_tx << 16 ) | mask_rx;
	if ( 0 == sync_mask ) {
		return RETURN_SUCCESS;
	}

	sscanf( data, "%lf", & freq );

	sync_mode |= 1; // internal sync
	write_hps_reg( "sync_mode", sync_mode );

	write_hps_reg( "sync_mask", sync_mask );

	sprintf( inbuf, "%lf", freq );

	for( i = 0; i < NUM_CHANNELS; i++ ) {

		if ( 0 == ( mask_rx & (1 << i) ) ) {
			continue;
		}

		switch( i ) {
		case 0: hdlr = hdlr_rx_a_rf_freq_val; break;
		case 1: hdlr = hdlr_rx_b_rf_freq_val; break;
		case 2: hdlr = hdlr_rx_c_rf_freq_val; break;
		case 3: hdlr = hdlr_rx_d_rf_freq_val; break;
		default: continue;
		}

		// call the handler directly
		r = hdlr( inbuf, outbuf );
		if( RETURN_SUCCESS != r ) {
			return r;
		}

		// disable inotify, write the value back to the file, re-enable inotify
		prop = get_prop_from_hdlr( hdlr );
		wd_backup = prop->wd;
		prop->wd = -1;
		set_property( prop -> path, inbuf );
		prop->wd = wd_backup;
	}

	for( i = 0; i < NUM_CHANNELS; i++ ) {

		if ( 0 == ( mask_tx & (1 << i) ) ) {
			continue;
		}

		switch( i ) {
		case 0: hdlr = hdlr_tx_a_rf_freq_val; break;
		case 1: hdlr = hdlr_tx_b_rf_freq_val; break;
		case 2: hdlr = hdlr_tx_c_rf_freq_val; break;
		case 3: hdlr = hdlr_tx_d_rf_freq_val; break;
		default: continue;
		}

		// call the handler directly
		r = hdlr( inbuf, outbuf );
		if( RETURN_SUCCESS != r ) {
			return r;
		}

		// disable inotify, write the value back to the file, re-enable inotify
		prop = get_prop_from_hdlr( hdlr );
		wd_backup = prop->wd;
		prop->wd = -1;
		set_property( prop -> path, inbuf );
		prop->wd = wd_backup;
	}

	return RETURN_SUCCESS;
}

static int hdlr_cm_trx_nco_adj (const char *data, char *ret) {
	int r;

	char inbuf[ 256 ];
	char outbuf[ 256 ];

	uint32_t sync_mode;
	uint32_t sync_mask;

	uint32_t mask_rx;
	uint32_t mask_tx;

	int wd_backup;
	prop_t *prop;
	int (*hdlr)( const char *, char *);
	int i;

	double freq = 0;

	read_hps_reg( "sync_mode", & sync_mode );
	read_hps_reg( "sync_mask", & sync_mask );

	mask_rx = cm_chanmask_get( "/var/crimson/state/cm/chanmask-rx" );
	mask_tx = cm_chanmask_get( "/var/crimson/state/cm/chanmask-tx" );

	sync_mask = ( mask_tx << 16 ) | mask_rx;
	if ( 0 == sync_mask ) {
		return RETURN_SUCCESS;
	}

	sscanf( data, "%lf", & freq );

	sync_mode |= 1; // internal sync
	write_hps_reg( "sync_mode", sync_mode );

	write_hps_reg( "sync_mask", sync_mask );

	sprintf( inbuf, "%lf", freq );

	for( i = 0; i < NUM_CHANNELS; i++ ) {

		if ( 0 == ( mask_rx & (1 << i) ) ) {
			continue;
		}

		switch( i ) {
		case 0: hdlr = hdlr_rx_a_dsp_nco_adj; break;
		case 1: hdlr = hdlr_rx_b_dsp_nco_adj; break;
		case 2: hdlr = hdlr_rx_c_dsp_nco_adj; break;
		case 3: hdlr = hdlr_rx_d_dsp_nco_adj; break;
		default: continue;
		}

		// call the handler directly
		r = hdlr( inbuf, outbuf );
		if( RETURN_SUCCESS != r ) {
			return r;
		}

		// disable inotify, write the value back to the file, re-enable inotify
		prop = get_prop_from_hdlr( hdlr );
		wd_backup = prop->wd;
		prop->wd = -1;
		set_property( prop -> path, inbuf );
		prop->wd = wd_backup;
	}

	for( i = 0; i < NUM_CHANNELS; i++ ) {

		if ( 0 == ( mask_tx & (1 << i) ) ) {
			continue;
		}

		switch( i ) {
		case 0: hdlr = hdlr_tx_a_dsp_nco_adj; break;
		case 1: hdlr = hdlr_tx_b_dsp_nco_adj; break;
		case 2: hdlr = hdlr_tx_c_dsp_nco_adj; break;
		case 3: hdlr = hdlr_tx_d_dsp_nco_adj; break;
		default: continue;
		}

		// call the handler directly
		r = hdlr( inbuf, outbuf );
		if( RETURN_SUCCESS != r ) {
			return r;
		}

		// disable inotify, write the value back to the file, re-enable inotify
		prop = get_prop_from_hdlr( hdlr );
		wd_backup = prop->wd;
		prop->wd = -1;
		set_property( prop -> path, inbuf );
		prop->wd = wd_backup;
	}

	return RETURN_SUCCESS;
}

#define DEFINE_FILE_PROP( n, h, p, v ) \
	{ .type = PROP_TYPE_FILE, .path = n, .handler = h, .permissions = p, .def_val = v, }

#define DEFINE_SYMLINK_PROP( n, t ) \
	{ .type = PROP_TYPE_SYMLINK, .path = n, .symlink_target = t, }

// XXX: @CF: If the port were encoded as an integer it would be easier to define (possibly can still using some wild macro expansions)
// TODO: @CF: We should be using pmt's for arguments to refactor common parsing code; handlers should be passed integers, doubles, and so on, not necessarily strings.
// TODO: @CF: Alphabetically sort the properties
// TODO: @CF: Enumerate properties and assign them via enumeration rather than automatic indexing
#define DEFINE_RX_CHANNEL( _c, _p, _ip ) \
	DEFINE_SYMLINK_PROP( "rx_" #_c, "rx/" #_c ), \
	DEFINE_FILE_PROP( "rx/" #_c "/trigger/sma_mode",  hdlr_rx_ ## _c ## _trigger_sma_mode,  RW,  "level" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/trigger/trig_sel",  hdlr_rx_ ## _c ## _trigger_trig_sel,  RW,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/trigger/edge_backoff",  hdlr_rx_ ## _c ## _trigger_edge_backoff,  RW,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/trigger/edge_sample_num",  hdlr_rx_ ## _c ## _trigger_edge_sample_num,  RW,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/trigger/ufl_mode",  hdlr_rx_ ## _c ## _trigger_ufl_mode,  RW,  "level" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/trigger/ufl_dir",  hdlr_rx_ ## _c ## _trigger_ufl_dir,  RW,  "out" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/trigger/ufl_pol",  hdlr_rx_ ## _c ## _trigger_ufl_pol,  RW,  "negative" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/pwr",  hdlr_rx_ ## _c ## _pwr,  RW,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/stream",  hdlr_rx_ ## _c ## _stream,  RW,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/sync",  hdlr_rx_sync,  WO,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/rf/freq/val",  hdlr_rx_ ## _c ## _rf_freq_val,  RW,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/rf/freq/lna",  hdlr_rx_ ## _c ## _rf_freq_lna,  RW,  "1" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/rf/freq/band",  hdlr_rx_ ## _c ## _rf_freq_band,  RW,  "1" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/rf/gain/val",  hdlr_rx_ ## _c ## _rf_gain_val,  RW,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/rf/atten/val",  hdlr_rx_ ## _c ## _rf_atten_val,  RW,  "127" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/status/rfpll_lock",  hdlr_rx_ ## _c ## _status_rfld,  RW,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/status/adc_alarm",  hdlr_rx_ ## _c ## _status_adcalarm,  RW,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/board/dump",  hdlr_rx_ ## _c ## _rf_board_dump,  WO,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/board/test",  hdlr_rx_ ## _c ## _rf_board_test,  WO,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/board/temp",  hdlr_rx_ ## _c ## _rf_board_temp,  RW,  "20" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/board/led",  hdlr_rx_ ## _c ## _rf_board_led,  WO,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/dsp/signed",  hdlr_rx_ ## _c ## _dsp_signed,  RW,  "1" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/dsp/gain",  hdlr_rx_ ## _c ## _dsp_gain,  RW,  "10" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/dsp/rate",  hdlr_rx_ ## _c ## _dsp_rate,  RW,  "1258850" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/dsp/nco_adj",  hdlr_rx_ ## _c ## _dsp_nco_adj,  RW,  "-15000000" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/dsp/rstreq",  hdlr_rx_ ## _c ## _dsp_rstreq,  WO,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/dsp/loopback",  hdlr_rx_ ## _c ## _dsp_loopback,  RW,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/about/id",  hdlr_rx_ ## _c ## _about_id,  RW,  "001" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/about/serial",  hdlr_rx_about_serial,  RW,  "001" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/about/mcudevid",  hdlr_rx_about_mcudevid,  RW,  "001" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/about/mcurev",  hdlr_rx_about_mcurev,  RW,  "001" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/about/mcufuses",  hdlr_rx_about_mcufuses,  RW,  "001" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/about/fw_ver",  hdlr_rx_about_fw_ver,  RW,  VERSION ), \
	DEFINE_FILE_PROP( "rx/" #_c "/about/sw_ver",  hdlr_invalid,  RO,  VERSION ), \
	DEFINE_FILE_PROP( "rx/" #_c "/link/vita_en",  hdlr_rx_ ## _c ## _link_vita_en,  RW,  "0" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/link/iface",  hdlr_rx_ ## _c ## _link_iface,  RW,  "sfpa" ), \
	DEFINE_FILE_PROP( "rx/" #_c "/link/port",  hdlr_rx_ ## _c ## _link_port,  RW, #_p ), \
	DEFINE_FILE_PROP( "rx/" #_c "/link/ip_dest",  hdlr_rx_ ## _c ## _link_ip_dest,  RW,  _ip ), \
	DEFINE_FILE_PROP( "rx/" #_c "/link/mac_dest",  hdlr_rx_ ## _c ## _link_mac_dest,  RW,  "ff:ff:ff:ff:ff:ff" )

// XXX: @CF: if the port were encoded as an integer it would be easier to define
// XXX: @CF: we should be using pmt's for arguments to refactor common parsing code.
// XXX: @CF: handlers should be passed integers, doubles, and so on, not necessarily strings.
#define DEFINE_TX_CHANNEL( _c, _p ) \
	DEFINE_SYMLINK_PROP( "tx_" #_c, "tx/" #_c ), \
	DEFINE_FILE_PROP( "tx/" #_c "/trigger/sma_mode",  hdlr_tx_ ## _c ## _trigger_sma_mode,  RW,  "level" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/trigger/trig_sel",  hdlr_tx_ ## _c ## _trigger_trig_sel,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/trigger/edge_backoff",  hdlr_tx_ ## _c ## _trigger_edge_backoff,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/trigger/edge_sample_num",  hdlr_tx_ ## _c ## _trigger_edge_sample_num,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/trigger/ufl_dir",  hdlr_tx_ ## _c ## _trigger_ufl_dir,  RW,  "out" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/trigger/ufl_mode",  hdlr_tx_ ## _c ## _trigger_ufl_mode,  RW,  "level" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/trigger/ufl_pol",  hdlr_tx_ ## _c ## _trigger_ufl_pol,  RW,  "negative" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/trigger/gating",  hdlr_tx_ ## _c ## _trigger_gating,  RW,  "output" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/pwr",  hdlr_tx_ ## _c ##_pwr,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/sync",  hdlr_tx_sync,  WO,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/rf/dac/dither_en",  hdlr_tx_ ## _c ## _rf_dac_dither_en,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/rf/dac/dither_mixer_en",  hdlr_tx_ ## _c ## _rf_dac_dither_mixer_en,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/rf/dac/dither_sra_sel",  hdlr_tx_ ## _c ## _rf_dac_dither_sra_sel,  RW,  "6" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/rf/dac/nco",  hdlr_tx_ ## _c ## _rf_dac_nco,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/rf/dac/temp",  hdlr_tx_ ## _c ## _rf_dac_temp,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/rf/freq/val",  hdlr_tx_ ## _c ## _rf_freq_val,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/rf/freq/band",  hdlr_tx_ ## _c ## _rf_freq_band,  RW,  "1" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/rf/freq/i_bias",  hdlr_tx_ ## _c ## _rf_freq_i_bias,  RW,  "17" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/rf/freq/q_bias",  hdlr_tx_ ## _c ## _rf_freq_q_bias,  RW,  "17" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/rf/gain/val",  hdlr_tx_ ## _c ## _rf_gain_val,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/status/rfpll_lock",  hdlr_tx_ ## _c ## _status_rfld,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/status/dacpll_lock",  hdlr_tx_ ## _c ## _status_dacld,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/status/dacpll_centre",  hdlr_tx_ ## _c ## _status_dacctr,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/board/dump",  hdlr_tx_ ## _c ## _rf_board_dump,  WO,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/board/test",  hdlr_tx_ ## _c ## _rf_board_test,  WO,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/board/temp",  hdlr_tx_ ## _c ## _rf_board_temp,  RW,  "23" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/board/led",  hdlr_tx_ ## _c ## _rf_board_led,  WO,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/dsp/gain",  hdlr_tx_ ## _c ## _dsp_gain,  RW,  "10" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/dsp/rate",  hdlr_tx_ ## _c ## _dsp_rate,  RW,  "1258850" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/dsp/nco_adj",  hdlr_tx_ ## _c ## _dsp_nco_adj,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/dsp/rstreq",  hdlr_tx_ ## _c ## _dsp_rstreq,  WO,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/about/id",  hdlr_tx_ ## _c ## _about_id,  RW,  "001" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/about/serial",  hdlr_tx_about_serial,  RW,  "001" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/about/mcudevid",  hdlr_tx_about_mcudevid,  RW,  "001" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/about/mcurev",  hdlr_tx_about_mcurev,  RW,  "001" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/about/mcufuses",  hdlr_tx_about_mcufuses,  RW,  "001" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/about/fw_ver",  hdlr_tx_about_fw_ver,  RW,  VERSION ), \
	DEFINE_FILE_PROP( "tx/" #_c "/about/sw_ver",  hdlr_invalid,  RO,  VERSION ), \
	DEFINE_FILE_PROP( "tx/" #_c "/link/vita_en",  hdlr_tx_ ## _c ## _link_vita_en,  RW,  "0" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/link/iface",  hdlr_tx_ ## _c ## _link_iface,  RW,  "sfpa" ), \
	DEFINE_FILE_PROP( "tx/" #_c "/link/port",  hdlr_tx_ ## _c ## _link_port,  RW,  #_p ), \
	DEFINE_FILE_PROP( "tx/" #_c "/qa/fifo_lvl",  hdlr_tx_ ## _c ## _qa_fifo_lvl,  RW,  #_p ), \
	DEFINE_FILE_PROP( "tx/" #_c "/qa/oflow",  hdlr_tx_ ## _c ## _qa_oflow,  RW,  #_p ), \
	DEFINE_FILE_PROP( "tx/" #_c "/qa/uflow",  hdlr_tx_ ## _c ## _qa_uflow,  RW,  #_p )

#define DEFINE_TIME() \
	DEFINE_FILE_PROP( "time/clk/pps",  hdlr_time_clk_pps,  RW,  "0" ), \
	DEFINE_FILE_PROP( "time/clk/cur_time",  hdlr_time_clk_cur_time,  RW,  "0.0" ), \
	DEFINE_FILE_PROP( "time/status/lmk_lockdetect",  hdlr_time_status_ld,  RW,  "unlocked" ), \
	DEFINE_FILE_PROP( "time/status/lmk_lossoflock",  hdlr_time_status_lol,  RW,  "unlocked" ), \
	DEFINE_FILE_PROP( "time/status/lmk_lockdetect_jesd_pll1",  hdlr_time_status_ld_jesd_pll1,  RW,  "unlocked" ), \
	DEFINE_FILE_PROP( "time/status/lmk_lockdetect_jesd_pll2",  hdlr_time_status_ld_jesd_pll2,  RW,  "unlocked" ), \
	DEFINE_FILE_PROP( "time/status/lmk_lockdetect_pll_pll1",  hdlr_time_status_ld_pll_pll1,  RW,  "unlocked" ), \
	DEFINE_FILE_PROP( "time/status/lmk_lockdetect_pll_pll2",  hdlr_time_status_ld_pll_pll2,  RW,  "unlocked" ), \
    DEFINE_FILE_PROP( "time/status/lmk_lossoflock_jesd_pll1",  hdlr_time_status_lol_jesd_pll1,  RW,  "unlocked" ), \
	DEFINE_FILE_PROP( "time/status/lmk_lossoflock_jesd_pll2",  hdlr_time_status_lol_jesd_pll2,  RW,  "unlocked" ), \
	DEFINE_FILE_PROP( "time/status/lmk_lossoflock_pll_pll1",  hdlr_time_status_lol_pll_pll1,  RW,  "unlocked" ), \
	DEFINE_FILE_PROP( "time/status/lmk_lossoflock_pll_pll2",  hdlr_time_status_lol_pll_pll2,  RW,  "unlocked" ), \
    DEFINE_FILE_PROP( "time/source/ref",  hdlr_time_source_ref,  RW,  "internal" ), \
	DEFINE_FILE_PROP( "time/source/extsine",  hdlr_time_source_extsine,  RW,  "sine" ), \
	DEFINE_FILE_PROP( "time/sync/lmk_sync_tgl_jesd",  hdlr_time_sync_lmk_sync_tgl_jesd,  WO,  "0" ), \
	DEFINE_FILE_PROP( "time/sync/lmk_sync_tgl_pll",  hdlr_time_sync_lmk_sync_tgl_pll,  WO,  "0" ), \
	DEFINE_FILE_PROP( "time/sync/lmk_sync_resync_jesd",  hdlr_time_sync_lmk_resync_jesd,  WO,  "0" ), \
	DEFINE_FILE_PROP( "time/sync/lmk_sync_resync_pll",  hdlr_time_sync_lmk_resync_pll,  WO,  "0" ), \
	DEFINE_FILE_PROP( "time/sync/lmk_resync_all",  hdlr_time_sync_lmk_resync_all,  WO,  "0" ), \
	DEFINE_FILE_PROP( "time/board/dump",  hdlr_time_board_dump,  WO,  "0" ), \
	DEFINE_FILE_PROP( "time/board/test",  hdlr_time_board_test,  WO,  "0" ), \
	DEFINE_FILE_PROP( "time/board/temp",  hdlr_time_board_temp,  RW,  "20" ), \
	DEFINE_FILE_PROP( "time/board/led",  hdlr_time_board_led,  WO,  "0" ), \
	DEFINE_FILE_PROP( "time/about/id",   hdlr_time_about_id,  RO,   "001" ), \
	DEFINE_FILE_PROP( "time/about/serial",  hdlr_time_about_serial,  RW,  "001" ), \
	DEFINE_FILE_PROP( "time/about/mcudevid",  hdlr_time_about_mcudevid,  RW,  "001" ), \
	DEFINE_FILE_PROP( "time/about/mcurev",  hdlr_time_about_mcurev,  RW,  "001" ), \
	DEFINE_FILE_PROP( "time/about/mcufuses",  hdlr_time_about_mcufuses,  RW,  "001" ), \
	DEFINE_FILE_PROP( "time/about/fw_ver",  hdlr_time_about_fw_ver,  RW,  VERSION ), \
	DEFINE_FILE_PROP( "time/about/sw_ver",  hdlr_invalid,  RO,  VERSION )

#define DEFINE_FPGA() \
	DEFINE_FILE_PROP( "fpga/trigger/sma_dir",  hdlr_fpga_trigger_sma_dir,  RW,  "out" ), \
	DEFINE_FILE_PROP( "fpga/trigger/sma_pol",  hdlr_fpga_trigger_sma_pol,  RW,  "negative" ), \
	DEFINE_FILE_PROP( "fpga/about/fw_ver",  hdlr_fpga_about_fw_ver,  RW,  VERSION ), \
	DEFINE_FILE_PROP( "fpga/about/server_ver",  hdlr_server_about_fw_ver,  RW, NULL), \
	DEFINE_FILE_PROP( "fpga/about/hw_ver",  hdlr_fpga_about_hw_ver,  RW,  VERSION ), \
	DEFINE_FILE_PROP( "fpga/about/id",  hdlr_fpga_about_id,  RW,  "001" ), \
	DEFINE_FILE_PROP( "fpga/about/name",  hdlr_invalid,  RO,  "crimson_tng" ), \
	DEFINE_FILE_PROP( "fpga/about/serial",  hdlr_fpga_about_serial,  RW,  "001" ), \
	DEFINE_FILE_PROP( "fpga/about/cmp_time",  hdlr_fpga_about_cmp_time,  RW,  "yyyy-mm-dd-hh-mm" ), \
	DEFINE_FILE_PROP( "fpga/about/conf_info",  hdlr_fpga_about_conf_info,  RW,  "0" ), \
	DEFINE_FILE_PROP( "fpga/board/dump",  hdlr_fpga_board_dump,  WO,  "0" ), \
	DEFINE_FILE_PROP( "fpga/board/fw_rst",  hdlr_fpga_board_fw_rst,  WO,  "0" ), \
	DEFINE_FILE_PROP( "fpga/board/flow_control/sfpa_port", hdlr_fpga_board_flow_control_sfpa_port, RW, "42809" ), \
	DEFINE_FILE_PROP( "fpga/board/flow_control/sfpb_port", hdlr_fpga_board_flow_control_sfpb_port, RW, "42809" ), \
	DEFINE_FILE_PROP( "fpga/board/gps_time",  hdlr_fpga_board_gps_time,  RW,  "0" ), \
	DEFINE_FILE_PROP( "fpga/board/gps_frac_time",  hdlr_fpga_board_gps_frac_time,  RW,  "0" ), \
	DEFINE_FILE_PROP( "fpga/board/gps_sync_time",  hdlr_fpga_board_gps_sync_time,  RW,  "0" ), \
	DEFINE_FILE_PROP( "fpga/board/jesd_sync",  hdlr_fpga_board_jesd_sync,  WO,  "0" ), \
	DEFINE_FILE_PROP( "fpga/board/led",  hdlr_fpga_board_led,  WO,  "0" ), \
	DEFINE_FILE_PROP( "fpga/board/rstreq",  hdlr_fpga_board_rstreq,  WO,  "0" ), \
	DEFINE_FILE_PROP( "fpga/board/reboot",  hdlr_fpga_board_reboot,  RW,  "0" ), \
	DEFINE_FILE_PROP( "fpga/board/sys_rstreq",  hdlr_fpga_board_sys_rstreq,  WO,  "0" ), \
	DEFINE_FILE_PROP( "fpga/board/test",  hdlr_fpga_board_test,  WO,  "0" ), \
	DEFINE_FILE_PROP( "fpga/board/temp",  hdlr_fpga_board_temp,  RW,  "20" ), \
	DEFINE_FILE_PROP( "fpga/board/gle",  hdlr_fpga_board_gle,  RW,  "0" ), \
	DEFINE_FILE_PROP( "fpga/link/rate",  hdlr_fpga_link_rate,  RW,  "1250000000" ), \
	DEFINE_FILE_PROP( "fpga/link/sfpa/ip_addr",  hdlr_fpga_link_sfpa_ip_addr,  RW,  "10.10.10.2" ), \
	DEFINE_FILE_PROP( "fpga/link/sfpa/mac_addr",  hdlr_fpga_link_sfpa_mac_addr,  RW,  "aa:00:00:00:00:00" ), \
	DEFINE_FILE_PROP( "fpga/link/sfpa/ver",  hdlr_fpga_link_sfpa_ver,  RW,  "0" ), \
	DEFINE_FILE_PROP( "fpga/link/sfpa/pay_len",  hdlr_fpga_link_sfpa_pay_len,  RW,  "1400" ), \
	DEFINE_FILE_PROP( "fpga/link/sfpb/ip_addr",  hdlr_fpga_link_sfpb_ip_addr,  RW,  "10.10.11.2" ), \
	DEFINE_FILE_PROP( "fpga/link/sfpb/mac_addr",  hdlr_fpga_link_sfpb_mac_addr,  RW,  "aa:00:00:00:00:01" ), \
	DEFINE_FILE_PROP( "fpga/link/sfpb/ver",  hdlr_fpga_link_sfpb_ver,  RW,  "0" ), \
	DEFINE_FILE_PROP( "fpga/link/sfpb/pay_len",  hdlr_fpga_link_sfpb_pay_len,  RW,  "1400" ), \
	DEFINE_FILE_PROP( "fpga/link/net/dhcp_en",  hdlr_fpga_link_net_dhcp_en,  RW,  "0" ), \
	DEFINE_FILE_PROP( "fpga/link/net/hostname",  hdlr_fpga_link_net_hostname,  RW,  "crimson_tng" ), \
	DEFINE_FILE_PROP( "fpga/link/net/ip_addr",  hdlr_fpga_link_net_ip_addr,  RW,  "192.168.10.2" )
	
#define DEFINE_CM() \
      DEFINE_FILE_PROP( "cm/chanmask-rx", hdlr_cm_chanmask_rx, RW, "0" ), \
      DEFINE_FILE_PROP( "cm/chanmask-tx", hdlr_cm_chanmask_tx, RW, "0" ), \
      DEFINE_FILE_PROP( "cm/rx/atten/val", hdlr_cm_rx_atten_val, WO, "0" ), \
      DEFINE_FILE_PROP( "cm/rx/gain/val", hdlr_cm_rx_gain_val, WO, "0" ), \
      DEFINE_FILE_PROP( "cm/tx/gain/val", hdlr_cm_tx_gain_val, WO, "0" ), \
      DEFINE_FILE_PROP( "cm/trx/freq/val", hdlr_cm_trx_freq_val, WO, "0" ), \
      DEFINE_FILE_PROP( "cm/trx/nco_adj", hdlr_cm_trx_nco_adj, WO, "0" )

// Beginning of property table
static prop_t property_table[] = {

	DEFINE_RX_CHANNEL( a, 42820, "10.10.10.10" ),
	DEFINE_RX_CHANNEL( b, 42821, "10.10.11.10" ),
	DEFINE_RX_CHANNEL( c, 42822, "10.10.10.10" ),
	DEFINE_RX_CHANNEL( d, 42823, "10.10.11.10" ),

	DEFINE_TX_CHANNEL( a, 42824 ),
	DEFINE_TX_CHANNEL( b, 42825 ),
	DEFINE_TX_CHANNEL( c, 42826 ),
	DEFINE_TX_CHANNEL( d, 42827 ),

	DEFINE_TIME(),
	DEFINE_FPGA(),

	DEFINE_FILE_PROP( "save_config",  hdlr_save_config,  RW,  "/home/root/profile.cfg" ),
	DEFINE_FILE_PROP( "load_config",  hdlr_load_config,  RW,  "/home/root/profile.cfg" ),

	DEFINE_CM(),
};
static size_t num_properties = sizeof(property_table) / sizeof(property_table[0]);

// Beginning of functions
size_t get_num_prop(void) {
	return num_properties;
}

prop_t* get_prop(size_t idx) {
	return (property_table + idx);
}

prop_t* get_prop_from_wd(int wd) {
	size_t i;
	for (i = 0; i < num_properties; i++) {
		if (property_table[i].wd == wd)
			return (property_table + i);
	}

	// no matching prop found
	return NULL;
}

prop_t *get_prop_from_hdlr( int (*hdlr)(const char*, char*) ) {
	size_t i;
	for (i = 0; i < num_properties; i++) {
		if ( property_table[ i ].handler == hdlr ) {
			return & property_table[ i ];
		}
	}

	// no matching prop found
	return NULL;
}

int resolve_symbolic_property_name( const char *prop, char *path, size_t n ) {

	const char *vcs = "/var/crimson/state/";
	const size_t vcsl = strlen( vcs );
	char origcwd [MAX_PATH_LEN];
	char *temp;
	size_t path_strlen;
	size_t delta;
	int r;

#if MAX_PATH_LEN < PATH_MAX
#error MAX_PATH_LEN is too small
#endif

	getcwd( origcwd, sizeof( origcwd ) );
	chdir( vcs );
	temp = (void *) realpath( prop, path );
	chdir( origcwd );
	if ( NULL == temp ) {
		PRINT( ERROR, "unable to find a property corresponding to '%s'\n", prop );
		return RETURN_ERROR_SET_PROP;
	}

	path_strlen = strlen( path );
	r = strncmp( vcs, path, vcsl );
	if ( 0 == r ) {
		delta = path_strlen - vcsl;
		memmove( path, path + vcsl, delta );
		path[ delta ] = '\0';
	}

//	if ( 0 != strcmp( path, prop ) ) {
//		PRINT( INFO, "%s(): resolved symbolic link: '%s' => '%s'\n", __func__, prop, path );
//	}

	return RETURN_SUCCESS;
}


prop_t* get_prop_from_cmd(const char* cmd) {
	char path[ MAX_PATH_LEN ];
	size_t i;

	if ( RETURN_SUCCESS == resolve_symbolic_property_name( cmd, path, sizeof( path ) ) ) {
		cmd = path;
	}

	for (i = 0; i < num_properties; i++) {
		if ( (strcmp(property_table[i].path, cmd) == 0) &&
			(strlen(property_table[i].path) == strlen(cmd)) )
			return (property_table + i);
	}

	// no matching prop found
	return NULL;
}

static inline const char* get_home_dir(void) {
	return getpwuid(getuid()) -> pw_dir;
}

void pass_uart_synth_fd(int fd) {
	uart_synth_fd = fd;
}

void pass_uart_tx_fd(int fd) {
	uart_tx_fd = fd;
}

void pass_uart_rx_fd(int fd) {
	uart_rx_fd = fd;
}

char* get_abs_path(prop_t* prop, char* path) {
	strcpy(path, "/var/crimson");
	strcat(path, "/state/");
	strcat(path, prop -> path);
	return path;
}

char* get_abs_dir(prop_t* prop, char* path) {
	size_t len = 0;
	size_t i = 0;
	while (prop -> path[i]) {
		if (prop -> path[i] == '/') len = i;
		i++;
	}

	strcpy(path, "/var/crimson");
	strcat(path, "/state/");

	size_t temp_len = 0;

	if (len != 0) {
		temp_len = strlen(path);
		memcpy(path + temp_len, prop -> path, len);
		path[temp_len + len] = '\0';
	}

	return path;
}


char* get_root (prop_t* prop, char* root) {
	int i;
	for(i = 0; prop -> path[i] != '/' && prop -> path[i] != '\0'; i++) {
		root[i] = prop -> path[i];
	}
	root[i] = 0;

	return root;
}

void pass_profile_pntr_prop(uint8_t* load, uint8_t* save, char* load_path, char* save_path) {
	_load_profile = load;
	_save_profile = save;
	_load_profile_path = load_path;
	_save_profile_path = save_path;
}

void sync_channels(uint8_t chan_mask) {
	char str_chan_mask[MAX_PROP_LEN] = "";
	sprintf(str_chan_mask + strlen(str_chan_mask), "%" PRIu8 "", 15);
	//usleep(300000); // Some wait time for the reset to be ready
	/* Bring the ADCs & DACs into 'demo' mode for JESD */

	// RX - ADCs
	strcpy(buf, "power -c ");
	strcat(buf, str_chan_mask);
	strcat(buf, " -a 1\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_rx_fd );

	// TX - DACs
	strcpy(buf, "power -c ");
	strcat(buf, str_chan_mask);
	strcat(buf, " -d 1\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_tx_fd );

	/***********************************
	 * Start loop.
	 * Issue JESD, then read to see if
	 * bad
	 **********************************/
	char key[]  = "00\r";
	char dacalarmA[]  = "ff\r";
	char dacalarmB[]  = "ff\r";


#ifdef SYNC_CHECK_DAC_LOOP
	for (int i = 0; i < 15; i += 1){

		//Put FPGA JESD core in reset
		write_hps_reg( "res_rw7",0x40000000);
		write_hps_reg( "res_rw7",0);

		/* Initiate the SYSREF sequence for jesd
		* Set all boards' SYSREF detection gate to ON */
		strcpy(buf, "board -c ");
		strcat(buf, str_chan_mask);
		strcat(buf, " -s 1\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_rx_fd );
		strcpy(buf, "board -c ");
		strcat(buf, str_chan_mask);
		strcat(buf, " -s 1\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_tx_fd );

		/* Trigger a SYSREF pulse */
		//JESD core out of reset
		usleep(100000); // Some wait time for MCUs to be ready
		strcpy(buf, "clk -y\r");
		send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_synth_fd );

		//Do it again

		strcpy(buf, "board -c ");
		strcat(buf, str_chan_mask);
		strcat(buf, " -s 1\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_tx_fd );
		usleep(100000); // Some wait time for MCUs to be ready
		strcpy(buf, "clk -y\r");
		send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_synth_fd );
		usleep(100000); // Some wait time for MCUs to be ready

		//CHECK IF ALARMS
		strcpy(buf, "dac -c a -s\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_tx_fd );

		strcpy(dacalarmA, (char*)uart_ret_buf);

		//CHECK IF ALARMS
		strcpy(buf, "dac -c d -s\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_tx_fd );

		strcpy(dacalarmB, (char*)uart_ret_buf);

		if ((dacalarmA[0] == key[0]) &&  (dacalarmA[1] == key[1]) &&
			(dacalarmB[0] == key[0]) &&  (dacalarmB[1] == key[1])	){
			break;
		}
		else{
			usleep(200000); // Some wait time for MCUs to be ready
		}
	}
	/* Turn off all boards' SYSREF detection gates */
	strcpy(buf, "board -c ");
	strcat(buf, str_chan_mask);
	strcat(buf, " -s 0\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_rx_fd );
	strcpy(buf, "board -c ");
	strcat(buf, str_chan_mask);
	strcat(buf, " -s 0\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_tx_fd );


#else
	//Put FPGA JESD core in reset
	write_hps_reg( "res_rw7",0x40000000);
	write_hps_reg( "res_rw7",0);

	/* Initiate the SYSREF sequence for jesd
	* Set all boards' SYSREF detection gate to ON */
	strcpy(buf, "board -c ");
	strcat(buf, str_chan_mask);
	strcat(buf, " -s 1\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_rx_fd );
	strcpy(buf, "board -c ");
	strcat(buf, str_chan_mask);
	strcat(buf, " -s 1\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_tx_fd );

	/* Trigger a SYSREF pulse */
	//JESD core out of reset
	write_hps_reg( "res_rw7",0);

	usleep(100000); // Some wait time for MCUs to be ready
	strcpy(buf, "clk -y\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_synth_fd );

	/* Turn off all boards' SYSREF detection gates */
	strcpy(buf, "board -c ");
	strcat(buf, str_chan_mask);
	strcat(buf, " -s 0\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_rx_fd );
	strcpy(buf, "board -c ");
	strcat(buf, str_chan_mask);
	strcat(buf, " -s 0\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf)); read_uart( uart_tx_fd );

#endif

}

void set_pll_frequency(int uart_fd, uint64_t reference, pllparam_t* pll) {
    // extract pll1 variables and pass to MCU (ADF4355/ADF5355)

    // Send Reference to MCU ( No Need ATM since fixed reference )
    strcpy(buf, "rf -v ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(reference/1000)); // Send reference in kHz
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

    // write ADF4355/5355 R
    strcpy(buf, "rf -r ");
    sprintf(buf + strlen(buf), "%" PRIu16 "", pll->R);
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

    // write ADF4355/ADF5355 N
    strcpy(buf, "rf -n ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", pll->N);
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

    // write ADF4355/ADF5355 D
    strcpy(buf, "rf -d ");
    sprintf(buf + strlen(buf), "%" PRIu16 "", pll->d);
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

    // write ADF4355/ADF5355 feedback mode
    strcpy(buf, "rf -t ");
    sprintf(buf + strlen(buf), "%" PRIu8 "", pll->divFBen);
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

    // write ADF4355/ADF5355 Output RF Power
    strcpy(buf, "rf -g ");
    sprintf(buf + strlen(buf), "%" PRIu8 "", 1 /*pll->power*/);    // default to lower mid power
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

    // write ADF4355/ADF5355 Output Frequency
    strcpy(buf, "rf -f ");
    sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)((pll->vcoFreq / pll->d) / 1000)); // Send output frequency in kHz
    strcat(buf, "\r");
    send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));


    usleep(100000);
}
