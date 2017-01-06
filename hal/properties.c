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
	strcpy((char*)uart_ret_buf, buf);

	return RETURN_SUCCESS;
}

// helper function to find the optimal value for the sample rate blocks
static uint16_t get_optimal_sr_factor(double rate, double base_rate, double* err) {
   uint32_t max_factor = 65536;	// 2^16
   uint32_t min_factor = 1;
   uint8_t lower_factor_violation = 0;
   uint8_t upper_factor_violation = 0;

   uint32_t lower_factor = (uint32_t)floor(base_rate / rate);
   uint32_t upper_factor = lower_factor + 1;

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

// Beginning of property functions, very long because each property needs to be
// handled explicitly
static int hdlr_invalid (const char* data, char* ret) {
	PRINT( ERROR,"Cannot invoke a set on this property\n");
	return RETURN_ERROR_SET_PROP;
}

static int hdlr_tx_a_rf_dac_dither_en (const char* data, char* ret) {
	int r;
	int en;
	char buf[] = "dac -c a -l \0\r";

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
	char buf[] = "dac -c a -3 \0\r";

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
	char buf[] = "dac -c a -b \0\0\r";

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
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c a -z\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

	// if freq is less than 53MHz, kill the channel
	if ( freq < 53000000ULL ) {
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
	double outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c a -p 0\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_tx_fd, (uint64_t)325000000ULL, &pll);

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

static int hdlr_tx_a_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l ");
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
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c a -z\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

	// if freq is less than 53MHz, kill the channel
	if ( freq < 53000000ULL ) {
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
	double outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c a -p 0\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_rx_fd, (uint64_t)325000000ULL, &pll);

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

static int hdlr_rx_a_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l ");
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

	if (resamp_err < base_err) {
		write_hps_reg( "rxa1", resamp_factor);
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)(resamp_factor + 1));
		//Set gain adjustment
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 0) ) | (decim_gain_lut[(resamp_factor)] << 0));
	} else {
		write_hps_reg( "rxa1", base_factor);
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)(base_factor + 1));
		//Set gain adjustment
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 0) ) | (decim_gain_lut[(base_factor)] << 0));
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
	char buf[] = "dac -c b -l \0\r";

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
	char buf[] = "dac -c b -3 \0\r";

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
	char buf[] = "dac -c b -b \0\0\r";

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
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c b -z\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

	// if freq is less than 53MHz, kill the channel
	if ( freq < 53000000ULL ) {
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
	double outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c b -p 0\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_tx_fd, (uint64_t)325000000, &pll);

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

static int hdlr_tx_b_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l ");
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
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c b -z\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

	// if freq is less than 53MHz, kill the channel
	if ( freq < 53000000ULL ) {
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
	double outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c b -p 0\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_rx_fd, (uint64_t)325000000ULL, &pll);

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

static int hdlr_rx_b_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l ");
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

	if (resamp_err < base_err) {
		write_hps_reg( "rxb1", resamp_factor);
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)(resamp_factor + 1));
		//Set gain adjustment
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 8) ) | (decim_gain_lut[(resamp_factor)] << 8));
	} else {
		write_hps_reg( "rxb1", base_factor);
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)(base_factor + 1));
		//Set gain adjustment
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 8) ) | (decim_gain_lut[(base_factor)] << 8));
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
	char buf[] = "dac -c c -l \0\r";

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
	char buf[] = "dac -c c -3 \0\r";

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
	char buf[] = "dac -c c -b \0\0\r";

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
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c c -z\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

	// if freq is less than 53MHz, kill the channel
	if ( freq < 53000000ULL ) {
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
	double outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c c -p 0\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_tx_fd, (uint64_t)325000000ULL, &pll);

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

static int hdlr_tx_c_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l ");
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
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c c -z\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

	// if freq is less than 53MHz, kill the channel
	if ( freq < 53000000ULL ) {
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
	double outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c c -p 0\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_rx_fd, (uint64_t)325000000ULL, &pll);

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

static int hdlr_rx_c_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l ");
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

	if (resamp_err < base_err) {
		write_hps_reg( "rxc1", resamp_factor);
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)(resamp_factor + 1));
		//Set gain adjustment
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 16) ) | (decim_gain_lut[(resamp_factor)] << 16));
	} else {
		write_hps_reg( "rxc1", base_factor);
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)(base_factor + 1));
		//Set gain adjustment
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 16) ) | (decim_gain_lut[(base_factor)] << 16));
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
	char buf[] = "dac -c d -l \0\r";

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
	char buf[] = "dac -c d -3 \0\r";

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
	char buf[] = "dac -c d -b \0\0\r";

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
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c d -z\r");
		send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

	// if freq is less than 53MHz, kill the channel
	if ( freq < 53000000ULL ) {
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
	double outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c d -p 0\r");
	send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_tx_fd, (uint64_t)325000000ULL, &pll);

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

static int hdlr_tx_d_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l ");
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
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// if freq = 0, mute PLL
	if ( freq == 0 ) {
		strcpy(buf, "rf -c d -z\r");
		send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

		return RETURN_SUCCESS;
	}

	// if freq is less than 53MHz, kill the channel
	if ( freq < 53000000ULL ) {
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
	double outfreq = setFreq(&freq, &pll);

	strcpy(buf, "rf -c d -p 0\r");
	send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

	// TODO: pll1.power setting TBD (need to modify pllparam_t)

	// Send Parameters over to the MCU
	set_pll_frequency(uart_rx_fd, (uint64_t)325000000ULL, &pll);

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

static int hdlr_rx_d_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l ");
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

	if (resamp_err < base_err) {
		write_hps_reg( "rxd1", resamp_factor);
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)(resamp_factor + 1));
		//Set gain adjustment
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 24) ) | (decim_gain_lut[(resamp_factor)] << 24));
	} else {
		write_hps_reg( "rxd1", base_factor);
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)(base_factor + 1));
		//Set gain adjustment
		read_hps_reg( "rxga",  &old_val);
		write_hps_reg( "rxga", (old_val & ~(0xff << 24) ) | (decim_gain_lut[(base_factor)] << 24));
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

	double time;
	sscanf(data, "%lf", &time);
	write_hps_reg( "sys7", (uint32_t)time);
	write_hps_reg( "sys8", time - (uint32_t)time);

	// toggle the set register
	write_hps_reg( "sys9", 1);
	write_hps_reg( "sys9", 0);
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

static int hdlr_time_board_dump (const char* data, char* ret) {
	// send the uart commands and read back the output and write to file

	// FANOUT
	strcpy(buf, "dump -f\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	PRINT(DUMP, "[Board: time Chip: FANOUT] %s\n", uart_ret_buf);

	// CLK
	strcpy(buf, "dump -c\r");
	send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	read_uart(uart_synth_fd);
	PRINT(DUMP, "[Board: time Chip: CLK] %s\n", uart_ret_buf);

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
	// don't need to do anything, save the ID in the file system
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

static int hdlr_fpga_board_temp (const char* data, char* ret) {
	//strcpy(buf, "board -t\r");
	//send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	//read_uart(NO_FWD_CMD);
	//strcpy(ret, (char*)uart_ret_buf);

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

// TODO: Move FWversion code to ARM, edit MAKE file with version info, refer to MCU code
static int hdlr_fpga_about_fw_ver (const char* data, char* ret) {
	//strcpy(buf, "board -v\r");
	//send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	//read_uart(NO_FWD_CMD);
	//strcpy(ret, (char*)uart_ret_buf);

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
	uint32_t gps_time = 0;
	read_hps_reg( "sys5", &gps_time );
	sprintf(ret, "%i", gps_time);

	return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gps_frac_time (const char* data, char* ret) {
	uint32_t gps_frac_time = 0;
	read_hps_reg( "sys6", &gps_frac_time );
	sprintf(ret, "%i", gps_frac_time);

	return RETURN_SUCCESS;
}

static int hdlr_fpga_board_gps_sync_time (const char* data, char* ret) {
	uint32_t systime  = 0;
	read_hps_reg( "sys5", &systime );
	write_hps_reg( "sys7", systime );
	write_hps_reg( "sys8", 0 ); // set frac_time to 0
	write_hps_reg( "sys9", 1 ); // writing 1, then 0 to sys9 sets the time
	write_hps_reg( "sys9", 0 ); // to what is written in sys7 and sys8

	return RETURN_SUCCESS;
}
// Beginning of property table
static prop_t property_table[] = {
	{"tx_a/pwr", hdlr_tx_a_pwr, RW, "0"},
	{"tx_a/sync", hdlr_tx_sync, WO, "0"},
	{"tx_a/rf/dac/dither_en", hdlr_tx_a_rf_dac_dither_en, RW, "0"},
	{"tx_a/rf/dac/dither_mixer_en", hdlr_tx_a_rf_dac_dither_mixer_en, RW, "0"},
	{"tx_a/rf/dac/dither_sra_sel", hdlr_tx_a_rf_dac_dither_sra_sel, RW, "96"},
	{"tx_a/rf/dac/nco", hdlr_tx_a_rf_dac_nco, RW, "15000000"},
	{"tx_a/rf/dac/temp", hdlr_tx_a_rf_dac_temp, RW, "0"},
	{"tx_a/rf/freq/val", hdlr_tx_a_rf_freq_val, RW, "0"},
	{"tx_a/rf/freq/band", hdlr_tx_a_rf_freq_band, RW, "1"},
	{"tx_a/rf/freq/i_bias", hdlr_tx_a_rf_freq_i_bias, RW, "17"},
	{"tx_a/rf/freq/q_bias", hdlr_tx_a_rf_freq_q_bias, RW, "17"},
	{"tx_a/rf/gain/val", hdlr_tx_a_rf_gain_val, RW, "0"},
	{"tx_a/board/dump", hdlr_tx_a_rf_board_dump, WO, "0"},
	{"tx_a/board/test", hdlr_tx_a_rf_board_test, WO, "0"},
	{"tx_a/board/temp", hdlr_tx_a_rf_board_temp, RW, "23"},
	{"tx_a/board/led", hdlr_tx_a_rf_board_led, WO, "0"},
	{"tx_a/dsp/gain", hdlr_tx_a_dsp_gain, RW, "10"},
	{"tx_a/dsp/rate", hdlr_tx_a_dsp_rate, RW, "1258850"},
	{"tx_a/dsp/nco_adj", hdlr_tx_a_dsp_nco_adj, RW, "0"},
	{"tx_a/dsp/rstreq", hdlr_tx_a_dsp_rstreq, WO, "0"},
	{"tx_a/about/id", hdlr_tx_a_about_id, RW, "001"},
	{"tx_a/about/serial", hdlr_invalid, RO, "001"},
	{"tx_a/about/fw_ver", hdlr_tx_about_fw_ver, RW, VERSION},
	{"tx_a/about/sw_ver", hdlr_invalid, RO, VERSION},
	{"tx_a/link/vita_en", hdlr_tx_a_link_vita_en, RW, "0"},
	{"tx_a/link/iface", hdlr_tx_a_link_iface, RW, "sfpa"},
	{"tx_a/link/port", hdlr_tx_a_link_port, RW, "42824"},
	{"rx_a/pwr", hdlr_rx_a_pwr, RW, "0"},
	{"rx_a/stream", hdlr_rx_a_stream, RW, "0"},
	{"rx_a/sync", hdlr_rx_sync, WO, "0"},
	{"rx_a/rf/freq/val", hdlr_rx_a_rf_freq_val, RW, "0"},
	{"rx_a/rf/freq/lna", hdlr_rx_a_rf_freq_lna, RW, "0"},
	{"rx_a/rf/freq/band", hdlr_rx_a_rf_freq_band, RW, "1"},
	{"rx_a/rf/gain/val", hdlr_rx_a_rf_gain_val, RW, "0"},
	{"rx_a/rf/atten/val", hdlr_rx_a_rf_atten_val, RW, "127"},
	{"rx_a/board/dump", hdlr_rx_a_rf_board_dump, WO, "0"},
	{"rx_a/board/test", hdlr_rx_a_rf_board_test, WO, "0"},
	{"rx_a/board/temp", hdlr_rx_a_rf_board_temp, RW, "20"},
	{"rx_a/board/led", hdlr_rx_a_rf_board_led, WO, "0"},
	{"rx_a/dsp/signed", hdlr_rx_a_dsp_signed, RW, "1"},
	{"rx_a/dsp/gain", hdlr_rx_a_dsp_gain, RW, "10"},
	{"rx_a/dsp/rate", hdlr_rx_a_dsp_rate, RW, "1258850"},
	{"rx_a/dsp/nco_adj", hdlr_rx_a_dsp_nco_adj, RW, "-15000000"},
	{"rx_a/dsp/rstreq", hdlr_rx_a_dsp_rstreq, WO, "0"},
	{"rx_a/dsp/loopback", hdlr_rx_a_dsp_loopback, RW, "0"},
	{"rx_a/about/id", hdlr_rx_a_about_id, RW, "001"},
	{"rx_a/about/serial", hdlr_invalid, RO, "001"},
	{"rx_a/about/fw_ver", hdlr_rx_about_fw_ver, RW, VERSION},
	{"rx_a/about/sw_ver", hdlr_invalid, RO, VERSION},
	{"rx_a/link/vita_en", hdlr_rx_a_link_vita_en, RW, "0"},
	{"rx_a/link/iface", hdlr_rx_a_link_iface, RW, "sfpa"},
	{"rx_a/link/port", hdlr_rx_a_link_port, RW, "42820"},
	{"rx_a/link/ip_dest", hdlr_rx_a_link_ip_dest, RW, "10.10.10.10"},
	{"rx_a/link/mac_dest", hdlr_rx_a_link_mac_dest, RW, "ff:ff:ff:ff:ff:ff"},
	{"tx_b/pwr", hdlr_tx_b_pwr, RW, "0"},
	{"tx_b/sync", hdlr_tx_sync, WO, "0"},
	{"tx_b/rf/dac/dither_en", hdlr_tx_b_rf_dac_dither_en, RW, "0"},
	{"tx_b/rf/dac/dither_mixer_en", hdlr_tx_b_rf_dac_dither_mixer_en, RW, "0"},
	{"tx_b/rf/dac/dither_sra_sel", hdlr_tx_b_rf_dac_dither_sra_sel, RW, "96"},
	{"tx_b/rf/dac/nco", hdlr_tx_b_rf_dac_nco, RW, "15000000"},
	{"tx_b/rf/dac/temp", hdlr_tx_b_rf_dac_temp, RW, "0"},
	{"tx_b/rf/freq/val", hdlr_tx_b_rf_freq_val, RW, "0"},
	{"tx_b/rf/freq/band", hdlr_tx_b_rf_freq_band, RW, "1"},
	{"tx_b/rf/freq/i_bias", hdlr_tx_b_rf_freq_i_bias, RW, "17"},
	{"tx_b/rf/freq/q_bias", hdlr_tx_b_rf_freq_q_bias, RW, "17"},
	{"tx_b/rf/gain/val", hdlr_tx_b_rf_gain_val, RW, "0"},
	{"tx_b/board/dump", hdlr_tx_b_rf_board_dump, WO, "0"},
	{"tx_b/board/test", hdlr_tx_b_rf_board_test, WO, "0"},
	{"tx_b/board/temp", hdlr_tx_b_rf_board_temp, RW, "23"},
	{"tx_b/board/led", hdlr_tx_b_rf_board_led, WO, "0"},
	{"tx_b/dsp/gain", hdlr_tx_b_dsp_gain, RW, "10"},
	{"tx_b/dsp/rate", hdlr_tx_b_dsp_rate, RW, "1258850"},
	{"tx_b/dsp/nco_adj", hdlr_tx_b_dsp_nco_adj, RW, "0"},
	{"tx_b/dsp/rstreq", hdlr_tx_b_dsp_rstreq, WO, "0"},
	{"tx_b/about/id", hdlr_tx_b_about_id, RW, "001"},
	{"tx_b/about/serial", hdlr_invalid, RO, "001"},
	{"tx_b/about/fw_ver", hdlr_tx_about_fw_ver, RW, VERSION},
	{"tx_b/about/sw_ver", hdlr_invalid, RO, VERSION},
	{"tx_b/link/vita_en", hdlr_tx_b_link_vita_en, RW, "0"},
	{"tx_b/link/iface", hdlr_tx_b_link_iface, RW, "sfpb"},
	{"tx_b/link/port", hdlr_tx_b_link_port, RW, "42825"},
	{"rx_b/pwr", hdlr_rx_b_pwr, RW, "0"},
	{"rx_b/stream", hdlr_rx_b_stream, RW, "0"},
	{"rx_b/sync", hdlr_rx_sync, WO, "0"},
	{"rx_b/rf/freq/val", hdlr_rx_b_rf_freq_val, RW, "0"},
	{"rx_b/rf/freq/lna", hdlr_rx_b_rf_freq_lna, RW, "0"},
	{"rx_b/rf/freq/band", hdlr_rx_b_rf_freq_band, RW, "1"},
	{"rx_b/rf/gain/val", hdlr_rx_b_rf_gain_val, RW, "0"},
	{"rx_b/rf/atten/val", hdlr_rx_b_rf_atten_val, RW, "127"},
	{"rx_b/board/dump", hdlr_rx_b_rf_board_dump, WO, "0"},
	{"rx_b/board/test", hdlr_rx_b_rf_board_test, WO, "0"},
	{"rx_b/board/temp", hdlr_rx_b_rf_board_temp, RW, "20"},
	{"rx_b/board/led", hdlr_rx_b_rf_board_led, WO, "0"},
	{"rx_b/dsp/signed", hdlr_rx_b_dsp_signed, RW, "1"},
	{"rx_b/dsp/gain", hdlr_rx_b_dsp_gain, RW, "10"},
	{"rx_b/dsp/rate", hdlr_rx_b_dsp_rate, RW, "1258850"},
	{"rx_b/dsp/nco_adj", hdlr_rx_b_dsp_nco_adj, RW, "-15000000"},
	{"rx_b/dsp/rstreq", hdlr_rx_b_dsp_rstreq, WO, "0"},
	{"rx_b/dsp/loopback", hdlr_rx_b_dsp_loopback, RW, "0"},
	{"rx_b/about/id", hdlr_rx_b_about_id, RW, "001"},
	{"rx_b/about/serial", hdlr_invalid, RO, "001"},
	{"rx_b/about/fw_ver", hdlr_rx_about_fw_ver, RW, VERSION},
	{"rx_b/about/sw_ver", hdlr_invalid, RO, VERSION},
	{"rx_b/link/vita_en", hdlr_rx_b_link_vita_en, RW, "0"},
	{"rx_b/link/iface", hdlr_rx_b_link_iface, RW, "sfpb"},
	{"rx_b/link/port", hdlr_rx_b_link_port, RW, "42821"},
	{"rx_b/link/ip_dest", hdlr_rx_b_link_ip_dest, RW, "10.10.11.10"},
	{"rx_b/link/mac_dest", hdlr_rx_b_link_mac_dest, RW, "ff:ff:ff:ff:ff:ff"},
	{"tx_c/pwr", hdlr_tx_c_pwr, RW, "0"},
	{"tx_c/sync", hdlr_tx_sync, WO, "0"},
	{"tx_c/rf/dac/dither_en", hdlr_tx_c_rf_dac_dither_en, RW, "0"},
	{"tx_c/rf/dac/dither_mixer_en", hdlr_tx_c_rf_dac_dither_mixer_en, RW, "0"},
	{"tx_c/rf/dac/dither_sra_sel", hdlr_tx_c_rf_dac_dither_sra_sel, RW, "96"},
	{"tx_c/rf/dac/nco", hdlr_tx_c_rf_dac_nco, RW, "15000000"},
	{"tx_c/rf/dac/temp", hdlr_tx_c_rf_dac_temp, RW, "0"},
	{"tx_c/rf/freq/val", hdlr_tx_c_rf_freq_val, RW, "0"},
	{"tx_c/rf/freq/band", hdlr_tx_c_rf_freq_band, RW, "1"},
	{"tx_c/rf/freq/i_bias", hdlr_tx_c_rf_freq_i_bias, RW, "17"},
	{"tx_c/rf/freq/q_bias", hdlr_tx_c_rf_freq_q_bias, RW, "17"},
	{"tx_c/rf/gain/val", hdlr_tx_c_rf_gain_val, RW, "0"},
	{"tx_c/board/dump", hdlr_tx_c_rf_board_dump, WO, "0"},
	{"tx_c/board/test", hdlr_tx_c_rf_board_test, WO, "0"},
	{"tx_c/board/temp", hdlr_tx_c_rf_board_temp, RW, "23"},
	{"tx_c/board/led", hdlr_tx_c_rf_board_led, WO, "0"},
	{"tx_c/dsp/gain", hdlr_tx_c_dsp_gain, RW, "10"},
	{"tx_c/dsp/rate", hdlr_tx_c_dsp_rate, RW, "1258850"},
	{"tx_c/dsp/nco_adj", hdlr_tx_c_dsp_nco_adj, RW, "0"},
	{"tx_c/dsp/rstreq", hdlr_tx_c_dsp_rstreq, WO, "0"},
	{"tx_c/about/id", hdlr_tx_c_about_id, RW, "001"},
	{"tx_c/about/serial", hdlr_invalid, RO, "001"},
	{"tx_c/about/fw_ver", hdlr_tx_about_fw_ver, RW, VERSION},
	{"tx_c/about/sw_ver", hdlr_invalid, RO, VERSION},
	{"tx_c/link/vita_en", hdlr_tx_c_link_vita_en, RW, "0"},
	{"tx_c/link/iface", hdlr_tx_c_link_iface, RW, "sfpa"},
	{"tx_c/link/port", hdlr_tx_c_link_port, RW, "42826"},
	{"rx_c/pwr", hdlr_rx_c_pwr, RW, "0"},
	{"rx_c/stream", hdlr_rx_c_stream, RW, "0"},
	{"rx_c/sync", hdlr_rx_sync, WO, "0"},
	{"rx_c/rf/freq/val", hdlr_rx_c_rf_freq_val, RW, "0"},
	{"rx_c/rf/freq/lna", hdlr_rx_c_rf_freq_lna, RW, "0"},
	{"rx_c/rf/freq/band", hdlr_rx_c_rf_freq_band, RW, "1"},
	{"rx_c/rf/gain/val", hdlr_rx_c_rf_gain_val, RW, "0"},
	{"rx_c/rf/atten/val", hdlr_rx_c_rf_atten_val, RW, "127"},
	{"rx_c/board/dump", hdlr_rx_c_rf_board_dump, WO, "0"},
	{"rx_c/board/test", hdlr_rx_c_rf_board_test, WO, "0"},
	{"rx_c/board/temp", hdlr_rx_c_rf_board_temp, RW, "20"},
	{"rx_c/board/led", hdlr_rx_c_rf_board_led, WO, "0"},
	{"rx_c/dsp/signed", hdlr_rx_c_dsp_signed, RW, "1"},
	{"rx_c/dsp/gain", hdlr_rx_c_dsp_gain, RW, "10"},
	{"rx_c/dsp/rate", hdlr_rx_c_dsp_rate, RW, "1258850"},
	{"rx_c/dsp/nco_adj", hdlr_rx_c_dsp_nco_adj, RW, "-15000000"},
	{"rx_c/dsp/rstreq", hdlr_rx_c_dsp_rstreq, WO, "0"},
	{"rx_c/dsp/loopback", hdlr_rx_c_dsp_loopback, RW, "0"},
	{"rx_c/about/id", hdlr_rx_c_about_id, RW, "001"},
	{"rx_c/about/serial", hdlr_invalid, RO, "001"},
	{"rx_c/about/fw_ver", hdlr_rx_about_fw_ver, RW, VERSION},
	{"rx_c/about/sw_ver", hdlr_invalid, RO, VERSION},
	{"rx_c/link/vita_en", hdlr_rx_c_link_vita_en, RW, "0"},
	{"rx_c/link/iface", hdlr_rx_c_link_iface, RW, "sfpa"},
	{"rx_c/link/port", hdlr_rx_c_link_port, RW, "42822"},
	{"rx_c/link/ip_dest", hdlr_rx_c_link_ip_dest, RW, "10.10.10.10"},
	{"rx_c/link/mac_dest", hdlr_rx_c_link_mac_dest, RW, "ff:ff:ff:ff:ff:ff"},
	{"tx_d/pwr", hdlr_tx_d_pwr, RW, "0"},
	{"tx_d/sync", hdlr_tx_sync, WO, "0"},	
	{"tx_d/rf/dac/dither_en", hdlr_tx_d_rf_dac_dither_en, RW, "0"},
	{"tx_d/rf/dac/dither_mixer_en", hdlr_tx_d_rf_dac_dither_mixer_en, RW, "0"},
	{"tx_d/rf/dac/dither_sra_sel", hdlr_tx_d_rf_dac_dither_sra_sel, RW, "96"},
	{"tx_d/rf/dac/nco", hdlr_tx_d_rf_dac_nco, RW, "15000000"},
	{"tx_d/rf/dac/temp", hdlr_tx_d_rf_dac_temp, RW, "0"},
	{"tx_d/rf/freq/val", hdlr_tx_d_rf_freq_val, RW, "0"},
	{"tx_d/rf/freq/band", hdlr_tx_d_rf_freq_band, RW, "1"},
	{"tx_d/rf/freq/i_bias", hdlr_tx_d_rf_freq_i_bias, RW, "17"},
	{"tx_d/rf/freq/q_bias", hdlr_tx_d_rf_freq_q_bias, RW, "17"},
	{"tx_d/rf/gain/val", hdlr_tx_d_rf_gain_val, RW, "0"},
	{"tx_d/board/dump", hdlr_tx_d_rf_board_dump, WO, "0"},
	{"tx_d/board/test", hdlr_tx_d_rf_board_test, WO, "0"},
	{"tx_d/board/temp", hdlr_tx_d_rf_board_temp, RW, "23"},
	{"tx_d/board/led", hdlr_tx_d_rf_board_led, WO, "0"},
	{"tx_d/dsp/gain", hdlr_tx_d_dsp_gain, RW, "10"},
	{"tx_d/dsp/rate", hdlr_tx_d_dsp_rate, RW, "1258850"},
	{"tx_d/dsp/nco_adj", hdlr_tx_d_dsp_nco_adj, RW, "0"},
	{"tx_d/dsp/rstreq", hdlr_tx_d_dsp_rstreq, WO, "0"},
	{"tx_d/about/id", hdlr_tx_d_about_id, RW, "001"},
	{"tx_d/about/serial", hdlr_invalid, RO, "001"},
	{"tx_d/about/fw_ver", hdlr_tx_about_fw_ver, RW, VERSION},
	{"tx_d/about/sw_ver", hdlr_invalid, RO, VERSION},
	{"tx_d/link/vita_en", hdlr_tx_d_link_vita_en, RW, "0"},
	{"tx_d/link/iface", hdlr_tx_d_link_iface, RW, "sfpb"},
	{"tx_d/link/port", hdlr_tx_d_link_port, RW, "42827"},
	{"rx_d/pwr", hdlr_rx_d_pwr, RW, "0"},
	{"rx_d/stream", hdlr_rx_d_stream, RW, "0"},
	{"rx_d/sync", hdlr_rx_sync, WO, "0"},
	{"rx_d/rf/freq/val", hdlr_rx_d_rf_freq_val, RW, "0"},
	{"rx_d/rf/freq/lna", hdlr_rx_d_rf_freq_lna, RW, "0"},
	{"rx_d/rf/freq/band", hdlr_rx_d_rf_freq_band, RW, "1"},
	{"rx_d/rf/gain/val", hdlr_rx_d_rf_gain_val, RW, "0"},
	{"rx_d/rf/atten/val", hdlr_rx_d_rf_atten_val, RW, "127"},
	{"rx_d/board/dump", hdlr_rx_d_rf_board_dump, WO, "0"},
	{"rx_d/board/test", hdlr_rx_d_rf_board_test, WO, "0"},
	{"rx_d/board/temp", hdlr_rx_d_rf_board_temp, RW, "20"},
	{"rx_d/board/led", hdlr_rx_d_rf_board_led, WO, "0"},
	{"rx_d/dsp/signed", hdlr_rx_d_dsp_signed, RW, "1"},
	{"rx_d/dsp/gain", hdlr_rx_d_dsp_gain, RW, "10"},
	{"rx_d/dsp/rate", hdlr_rx_d_dsp_rate, RW, "1258850"},
	{"rx_d/dsp/nco_adj", hdlr_rx_d_dsp_nco_adj, RW, "-15000000"},
	{"rx_d/dsp/rstreq", hdlr_rx_d_dsp_rstreq, WO, "0"},
	{"rx_d/dsp/loopback", hdlr_rx_d_dsp_loopback, RW, "0"},
	{"rx_d/about/id", hdlr_rx_d_about_id, RW, "001"},
	{"rx_d/about/serial", hdlr_invalid, RO, "001"},
	{"rx_d/about/fw_ver", hdlr_rx_about_fw_ver, RW, VERSION},
	{"rx_d/about/sw_ver", hdlr_invalid, RO, VERSION},
	{"rx_d/link/vita_en", hdlr_rx_d_link_vita_en, RW, "0"},
	{"rx_d/link/iface", hdlr_rx_d_link_iface, RW, "sfpb"},
	{"rx_d/link/port", hdlr_rx_d_link_port, RW, "42823"},
	{"rx_d/link/ip_dest", hdlr_rx_d_link_ip_dest, RW, "10.10.11.10"},
	{"rx_d/link/mac_dest", hdlr_rx_d_link_mac_dest, RW, "ff:ff:ff:ff:ff:ff"},
	{"time/clk/pps", hdlr_time_clk_pps, RW, "0"},
	{"time/clk/cur_time", hdlr_time_clk_cur_time, RW, "0.0"},
	//{"time/source/vco", hdlr_time_source_vco, RW, "external"},
	//{"time/source/sync", hdlr_time_source_sync, RW, "external"},
	{"time/source/ref", hdlr_time_source_ref, RW, "internal"},
	//{"time/source/devclk", hdlr_time_source_devclk, RW, "external"},
	//{"time/source/pll", hdlr_time_source_pll, RW, "external"},
	{"time/board/dump", hdlr_time_board_dump, WO, "0"},
	{"time/board/test", hdlr_time_board_test, WO, "0"},
	{"time/board/temp", hdlr_time_board_temp, RW, "20"},
	{"time/board/led", hdlr_time_board_led, WO, "0"},
	{"time/about/id", hdlr_time_about_id, RW, "001"},
	{"time/about/serial", hdlr_invalid, RO, "001"},
	{"time/about/fw_ver", hdlr_time_about_fw_ver, RW, VERSION},
	{"time/about/sw_ver", hdlr_invalid, RO, VERSION},
	{"fpga/board/dump", hdlr_fpga_board_dump, WO, "0"},
	{"fpga/board/test", hdlr_fpga_board_test, WO, "0"},
	{"fpga/board/temp", hdlr_fpga_board_temp, RW, "20"},
	{"fpga/board/led", hdlr_fpga_board_led, WO, "0"},
	{"fpga/board/rstreq", hdlr_fpga_board_rstreq, WO, "0"},
	{"fpga/board/jesd_sync", hdlr_fpga_board_jesd_sync, WO, "0"},
	{"fpga/board/fw_rst", hdlr_fpga_board_fw_rst, WO, "0"},
	{"fpga/board/sys_rstreq", hdlr_fpga_board_sys_rstreq, WO, "0"},
	{"fpga/about/name", hdlr_invalid, RO, "crimson_tng"},
	{"fpga/about/id", hdlr_fpga_about_id, RW, "001"},
	{"fpga/about/serial", hdlr_invalid, RO, "001"},
	{"fpga/about/fw_ver", hdlr_fpga_about_fw_ver, RW, VERSION},
	{"fpga/about/hw_ver", hdlr_fpga_about_hw_ver, RW, VERSION},
	{"fpga/about/sw_ver", hdlr_invalid, RO, VERSION},
	{"fpga/link/rate", hdlr_fpga_link_rate, RW, "1250000000"},      // BPS (10G/8)
	{"fpga/link/sfpa/ip_addr", hdlr_fpga_link_sfpa_ip_addr, RW, "10.10.10.2"},
	{"fpga/link/sfpa/mac_addr", hdlr_fpga_link_sfpa_mac_addr, RW, "aa:00:00:00:00:00"},
	{"fpga/link/sfpa/ver", hdlr_fpga_link_sfpa_ver, RW, "0"},
	{"fpga/link/sfpa/pay_len", hdlr_fpga_link_sfpa_pay_len, RW, "1400"},
	{"fpga/link/sfpb/ip_addr", hdlr_fpga_link_sfpb_ip_addr, RW, "10.10.11.2"},
	{"fpga/link/sfpb/mac_addr", hdlr_fpga_link_sfpb_mac_addr, RW, "aa:00:00:00:00:01"},
	{"fpga/link/sfpb/ver", hdlr_fpga_link_sfpb_ver, RW, "0"},
	{"fpga/link/sfpb/pay_len", hdlr_fpga_link_sfpb_pay_len, RW, "1400"},
	{"fpga/link/net/dhcp_en", hdlr_fpga_link_net_dhcp_en, RW, "0"},
	{"fpga/link/net/hostname", hdlr_fpga_link_net_hostname, RW, "crimson_tng"},
	{"fpga/link/net/ip_addr", hdlr_fpga_link_net_ip_addr, RW, "192.168.10.2"},
	{"save_config", hdlr_save_config, RW, "/home/root/profile.cfg"},
	{"load_config", hdlr_load_config, RW, "/home/root/profile.cfg"},
	{"fpga/board/gps_time", hdlr_fpga_board_gps_time, RW, "0"},
	{"fpga/board/gps_frac_time", hdlr_fpga_board_gps_frac_time, RW, "0"},
	{"fpga/board/gps_sync_time", hdlr_fpga_board_gps_sync_time, RW, "0"}
};
static size_t num_properties = sizeof(property_table) / sizeof(property_table[0]);

// Beginning of functions
inline size_t get_num_prop(void) {
	return num_properties;
}

inline prop_t* get_prop(size_t idx) {
	return (property_table + idx);
}

inline prop_t* get_prop_from_wd(int wd) {
	size_t i;
	for (i = 0; i < num_properties; i++) {
		if (property_table[i].wd == wd)
			return (property_table + i);
	}

	// no matching prop found
	return NULL;
}

inline prop_t* get_prop_from_cmd(const char* cmd) {
	size_t i;
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

inline void pass_uart_synth_fd(int fd) {
	uart_synth_fd = fd;
}

inline void pass_uart_tx_fd(int fd) {
	uart_tx_fd = fd;
}

inline void pass_uart_rx_fd(int fd) {
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
    sprintf(str_chan_mask + strlen(str_chan_mask), "%" PRIu8 "", chan_mask);

    /* Bring the ADCs & DACs into 'demo' mode for JESD */

    // RX - ADCs
    strcpy(buf, "power -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -a 1\r");
    send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

    // TX - DACs
    strcpy(buf, "power -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -d 1\r");
    send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

    /* Initiate the SYSREF sequence for jesd
     * Set all boards' SYSREF detection gate to ON */

    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 1\r");
    send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 1\r");
    send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

    /* Trigger a SYSREF pulse */

    usleep(100000);		// Some wait time for MCUs to be ready
    strcpy(buf, "clk -y -y -y\r");
    send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
    usleep(100000);

    /* Turn off all boards' SYSREF detection gates */

    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 0\r");
    send_uart_comm(uart_rx_fd, (uint8_t*)buf, strlen(buf));

    strcpy(buf, "board -c ");
    strcat(buf, str_chan_mask);
    strcat(buf, " -s 0\r");
    send_uart_comm(uart_tx_fd, (uint8_t*)buf, strlen(buf));

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
