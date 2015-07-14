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
#include "pllcalc.h"

#define BASE_SAMPLE_RATE 322265625.0	// SPS
#define RESAMP_SAMPLE_RATE  257812500.0	// SPS
#define IPVER_IPV4 0
#define IPVER_IPV6 1

#define DSP_NCO_CONST ((double)13.3274136700121212121)	// (2^32) / (322265625)
#define DAC_NCO_CONST ((double)218356.345569478593939)	// (2^48) / (4*322265625)

#define FWD_CMD 	1
#define NO_FWD_CMD	0

#define PWR_ON	1
#define PWR_OFF	0

#define FREQ_XOVER_PNT 100000000	// 100 MHz is the crossover frequency for high and low band

#define NUM_CHANNELS 4

// static global variables
static int uart_fd = 0;
static uint8_t uart_ret_buf[MAX_UART_RET_LEN] = {};
static uint16_t uart_ret_size = 0;
static char buf[MAX_PROP_LEN] = {};
static uint16_t rd_len;

// by default the board is powered off
static uint8_t rx_power[] = {PWR_OFF, PWR_OFF, PWR_OFF, PWR_OFF};
static uint8_t tx_power[] = {PWR_OFF, PWR_OFF, PWR_OFF, PWR_OFF};
const static char* chan_letter[] = {"a", "b", "c", "d"};
const static char* reg4[] = {"rxa4", "rxb4", "rxc4", "rxd4", "txa4", "txb4", "txc4", "txd4"};
static int i_bias[] = {17, 17, 17, 17};
static int q_bias[] = {17, 17, 17, 17};

// state variables
static uint8_t ipver[2] = {IPVER_IPV4, IPVER_IPV4};

// helper function to check if the buffer contains a character, strstr() won't work because no NULL terminator
static int contains (uint8_t* buf, size_t len, uint8_t ch) {
	int i = 0;
	for (i = 0; i < len; i++) {
		if (buf[i] == ch) return 1;
	}
	return 0;
}

// helper function to read back from UART after a UART command
static int read_uart(int fwd) {
	int counter = 0;

	// read uart return messages
	memset(uart_ret_buf, 0, MAX_UART_RET_LEN);
	counter = 0;
	while (counter == 0 || !contains(uart_ret_buf, counter, '>')) {
		recv_uart_comm(uart_fd, uart_ret_buf + counter, &uart_ret_size, MAX_UART_RET_LEN - counter);
		counter += uart_ret_size;
		if (counter > MAX_UART_RET_LEN) break;
	}

	// uart command is forwarded, discard the first read
	if (fwd) {
		memset(uart_ret_buf, 0, MAX_UART_RET_LEN);
		counter = 0;
		while (counter == 0 || !contains(uart_ret_buf, counter, '>')) {
			recv_uart_comm(uart_fd, uart_ret_buf + counter, &uart_ret_size, MAX_UART_RET_LEN - counter);
			counter += uart_ret_size;
			if (counter > MAX_UART_RET_LEN) break;
		}
	}

	// add null terminator
	uart_ret_buf[counter] = '\0';
	//printf("%s\n", uart_ret_buf);

	return RETURN_SUCCESS;
}

// helper function to find the optimal value for the sample rate blocks
static uint16_t get_optimal_sr_factor(double rate, double base_rate, double* err) {
   uint16_t tot_factor = floor(base_rate / rate);
   uint16_t max_factor = 256;
   uint16_t min_factor = 1;

   // error bounds check
   if (tot_factor > max_factor * max_factor || tot_factor < min_factor * min_factor)
      return 0xffff;

   double min_error = rate;
   uint16_t optimal = 0;

   uint32_t val = 0;
   for (;val <= 0xffff; val++) {
      uint16_t first  = (((val >> 0) & 0xff) + 1);
      uint16_t second = (((val >> 8) & 0xff) + 1);
      double error  = fabs( (base_rate / (double)(first * second)) - rate);
      if (error < min_error) {
         optimal = val;
         min_error = error;
      }
   }

   if (err) *err = min_error;
   return optimal;
}

// Beginning of property functions, very long because each property needs to be
// handled explicitly
static int get_invalid (const char* data, char* ret) {
	printf("Cannot invoke a get on this property\n");
	return RETURN_ERROR_GET_PROP;
}

static int set_invalid (const char* data, char* ret) {
	printf("Cannot invoke a set on this property\n");
	return RETURN_ERROR_SET_PROP;
}
static int set_tx_a_rf_dac_mixer (const char* data, char* ret) {
	// TODO: Need to implement in MCU code
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_dac_nco (const char* data, char* ret) {
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);
	freq *= DAC_NCO_CONST;

	strcpy(buf, "fwd -b 1 -m 'dac -c a -e 0 -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(freq >> 32));
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	strcpy(buf, "fwd -b 1 -m 'dac -o ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)freq);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int get_tx_a_rf_dac_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'board -c a -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_dac_iqerr_gain (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_dac_iqerr_phase (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

/* TODO not all frequencies are possible, read in the value and update it */
static int set_tx_a_rf_freq_val (const char* data, char* ret) {
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// run the pll calc algorithm
	pllparam_t pll0;
	pllparam_t pll1;
	double outfreq = setFreq(&freq, &pll0, &pll1);

	// extract pllX variables and pass to MCU
	// HMC830
	strcpy(buf, "fwd -b 1 -m 'rf -c a -p 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC830 (PLL0) R
	strcpy(buf, "fwd -b 1 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(10000);

	// write HMC830 (PLL0) d
	strcpy(buf, "fwd -b 1 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(10000);

	// write HMC830 (PLL0) N
	strcpy(buf, "fwd -b 1 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll0.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(10000);

	// HMC833
	strcpy(buf, "fwd -b 1 -m 'rf -c a -p 1'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC833 (PLL1) R
	strcpy(buf, "fwd -b 1 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(10000);

	// write HMC833 (PLL1) d
	strcpy(buf, "fwd -b 1 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(10000);

	// write HMC833 (PLL1) N
	strcpy(buf, "fwd -b 1 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll1.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(10000);

	// write HMC833 (PLL1) x2en
	strcpy(buf, "fwd -b 1 -m 'rf -x ");
	sprintf(buf + strlen(buf), "%" PRIu8 "", pll1.x2en);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c a -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c a -b ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_freq_i_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(i_bias[0]));
   strcpy(buf, "fwd -b 1 -m 'rf -c a -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[0]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[0]);
   strcat(buf, " -m'\r");
   send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_freq_q_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(q_bias[0]));
   strcpy(buf, "fwd -b 1 -m 'rf -c a -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[0]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[0]);
   strcat(buf, " -m'\r");
   send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

// TODO: make sure the range (0 -> -28 is within the boundaries of requirement
static int set_tx_a_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);

	strcpy(buf, "fwd -b 1 -m 'rf -c a -v ");
	sprintf(buf + strlen(buf), "%i", (28-gain));
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int get_tx_a_rf_board_status (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_board_dump (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int get_tx_a_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'board -c a -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'board -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_freq (const char* data, char* ret) {
	// don't need to tune DSP chain
	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
   uint16_t base_factor, resamp_factor;
   double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
   base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
   resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && resamp_err < base_err) {
		write_hps_reg( "txa1", resamp_factor);
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)( (((resamp_factor >> 0) & 0xff) + 1) * (((resamp_factor >> 8) & 0xff) + 1) ));
	} else {
		write_hps_reg( "txa1", base_factor);
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)( (((base_factor >> 0) & 0xff) + 1) * (((base_factor >> 8) & 0xff) + 1) ));
	}

	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	uint32_t old_val;
	uint8_t direction;

	// check for a minus or plus sign at the front
	if (data[0] == '-') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 0;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 0;
	}

	// write NCO adj
	write_hps_reg( "txa0", freq * DSP_NCO_CONST);

	// write direction
	read_hps_reg(  "txa4", &old_val);
	write_hps_reg( "txa4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_iqerr_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_iqerr_phase (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txa4", &old_val);
	write_hps_reg( "txa4", old_val |  0x2);
	write_hps_reg( "txa4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int set_tx_a_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_tx_a_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txa4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "txa4", old_val | (1 << 14));
	else				write_hps_reg( "txa4", old_val & ~(1 << 14));
	return RETURN_SUCCESS;
}

static int set_tx_a_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_tx_a_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "txa5", port);
	return RETURN_SUCCESS;
}

static int set_tx_a_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
   uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && tx_power[0] == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && tx_power[0] == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		tx_power[0] = PWR_ON;

      // board commands
		strcpy(buf, "fwd -b 1 -m 'board -c a -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
      usleep(500000);

		// disable dsp channels
      for(i = 0; i < (NUM_CHANNELS * 2); i++) {
         read_hps_reg ( reg4[i], &old_val);
		   write_hps_reg( reg4[i], old_val & ~0x100);
      }

      // set all active DAC's to demo mode
      for (i = 0; i < NUM_CHANNELS; i++) {
         if (tx_power[i] == PWR_ON) {
	         strcpy(buf, "fwd -b 1 -m 'power -c ");
            strcat(buf, chan_letter[i]);
         	strcat(buf, " -d 1'\r");
   	      send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
         }
      }

      // send sync pulse
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

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
		// mute the board
		strcpy(buf, "fwd -b 1 -m 'board -c a -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

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

/* TODO not all frequencies are possible, read in the value and update it */
static int set_rx_a_rf_freq_val (const char* data, char* ret) {
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// run the pll calc algorithm
	pllparam_t pll0;
	pllparam_t pll1;
	double outfreq = setFreq(&freq, &pll0, &pll1);

	// extract pllX variables and pass to MCU
	// HMC830
	strcpy(buf, "fwd -b 0 -m 'rf -c a -p 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC830 (PLL0) R
	strcpy(buf, "fwd -b 0 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC830 (PLL0) d
	strcpy(buf, "fwd -b 0 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC830 (PLL0) N
	strcpy(buf, "fwd -b 0 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll0.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// HMC833
	strcpy(buf, "fwd -b 0 -m 'rf -c a -p 1'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC833 (PLL1) R
	strcpy(buf, "fwd -b 0 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) d
	strcpy(buf, "fwd -b 0 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) N
	strcpy(buf, "fwd -b 0 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll1.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) x2en
	strcpy(buf, "fwd -b 0 -m 'rf -x ");
	sprintf(buf + strlen(buf), "%" PRIu8 "", pll1.x2en);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c a -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c a -b ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_freq_varac (const char* data, char* ret) {
	// TODO: Victor is going to do math from Voltage -> Clk Cycles
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_gain_val (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c a -g ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int get_rx_a_rf_board_status (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_board_dump (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int get_rx_a_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'board -c a -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'board -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_signed (const char* data, char* ret) {
   uint32_t old_val, sign;
   sscanf(data, "%u", &sign);
   sign = sign ? 0 : 1;

	read_hps_reg( "rxa4", &old_val);
   old_val &= ~(1 << 4);
	write_hps_reg( "rxa4", old_val | (sign << 4));
	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_freq (const char* data, char* ret) {
        // don't need to tune DSP chain
	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
   uint16_t base_factor, resamp_factor;
   double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
   base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
   resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && resamp_err < base_err) {
		write_hps_reg( "rxa1", resamp_factor);
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)( (((resamp_factor >> 0) & 0xff) + 1) * (((resamp_factor >> 8) & 0xff) + 1) ));
	} else {
		write_hps_reg( "rxa1", base_factor);
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)( (((base_factor >> 0) & 0xff) + 1) * (((base_factor >> 8) & 0xff) + 1) ));
	}

	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	uint32_t old_val;
	uint8_t direction;

	// check for a minus or plus sign at the front
	if (data[0] == '-') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 0;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 0;
	}

	// write NCO adj
	write_hps_reg( "rxa0", freq * DSP_NCO_CONST);

	// write direction
	read_hps_reg(  "rxa4", &old_val);
	write_hps_reg( "rxa4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_iqerr_gain (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_iqerr_phase (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxa4", &old_val);
	write_hps_reg( "rxa4", old_val |  0x2);
	write_hps_reg( "rxa4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_loopback (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxa4", &old_val);
	if (strcmp(data, "1") == 0)   write_hps_reg( "rxa4", (old_val & ~0x1e00) | 0x400);
   else                          write_hps_reg( "rxa4", (old_val & ~0x1e00) | 0x000);
	return RETURN_SUCCESS;
}

static int set_rx_a_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_rx_a_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxa4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "rxa4", old_val | (1 << 14));
	else				write_hps_reg( "rxa4", old_val & ~(1 << 14));
	return RETURN_SUCCESS;
}

static int set_rx_a_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_rx_a_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "rxa8", port);
	return RETURN_SUCCESS;
}

static int set_rx_a_link_ip_dest (const char* data, char* ret) {
	uint8_t ip[4];
	sscanf(data, "%"SCNd8".%"SCNd8".%"SCNd8".%"SCNd8"", ip, ip+1, ip+2, ip+3);
	write_hps_reg( "rxa5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	return RETURN_SUCCESS;
}

static int set_rx_a_link_mac_dest (const char* data, char* ret) {
	uint8_t mac[6];
	sscanf(data, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8"",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	write_hps_reg( "rxa6", (mac[0] << 8) | (mac[1]) );
	write_hps_reg( "rxa7", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
	return RETURN_SUCCESS;
}

static int set_rx_a_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
   uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && rx_power[0] == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && rx_power[0] == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		rx_power[0] = PWR_ON;

      // board commands
		strcpy(buf, "fwd -b 0 -m 'board -c a -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
      usleep(500000);

		// disable dsp channels
      for(i = 0; i < (NUM_CHANNELS * 2); i++) {
         read_hps_reg ( reg4[i], &old_val);
		   write_hps_reg( reg4[i], old_val & ~0x100);
      }

      // set all active DAC's to demo mode
      for (i = 0; i < NUM_CHANNELS; i++) {
         if (tx_power[i] == PWR_ON) {
	         strcpy(buf, "fwd -b 1 -m 'power -c ");
            strcat(buf, chan_letter[i]);
         	strcat(buf, " -d 1'\r");
   	      send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
         }
      }

      // send sync pulse
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

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
		rx_power[0] = PWR_OFF;

		// mute the board
		strcpy(buf, "fwd -b 0 -m 'board -c a -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// disable DSP core
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | 0x2);

		// disable channel
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & (~0x100));
	}
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_mixer (const char* data, char* ret) {
	// TODO: Need to implement in MCU code
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_nco (const char* data, char* ret) {
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);
	freq *= DAC_NCO_CONST;

	strcpy(buf, "fwd -b 1 -m 'dac -c b -e 1 -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(freq >> 32));
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	strcpy(buf, "fwd -b 1 -m 'dac -o ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)freq);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int get_tx_b_rf_dac_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'board -c b -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_iqerr_gain (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_iqerr_phase (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

/* TODO not all frequencies are possible, read in the value and update it */
static int set_tx_b_rf_freq_val (const char* data, char* ret) {
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// run the pll calc algorithm
	pllparam_t pll0;
	pllparam_t pll1;
	double outfreq = setFreq(&freq, &pll0, &pll1);

	// extract pllX variables and pass to MCU
	// HMC830
	strcpy(buf, "fwd -b 1 -m 'rf -c b -p 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC830 (PLL0) R
	strcpy(buf, "fwd -b 1 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(10000);

	// write HMC830 (PLL0) d
	strcpy(buf, "fwd -b 1 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(10000);

	// write HMC830 (PLL0) N
	strcpy(buf, "fwd -b 1 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll0.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(10000);

	// HMC833
	strcpy(buf, "fwd -b 1 -m 'rf -c b -p 1'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC833 (PLL1) R
	strcpy(buf, "fwd -b 1 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(10000);

	// write HMC833 (PLL1) d
	strcpy(buf, "fwd -b 1 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(10000);

	// write HMC833 (PLL1) N
	strcpy(buf, "fwd -b 1 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll1.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(10000);

	// write HMC833 (PLL1) x2en
	strcpy(buf, "fwd -b 1 -m 'rf -x ");
	sprintf(buf + strlen(buf), "%" PRIu8 "", pll1.x2en);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c b -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c b -b ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_freq_i_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(i_bias[1]));
   strcpy(buf, "fwd -b 1 -m 'rf -c b -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[1]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[1]);
   strcat(buf, " -m'\r");
   send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_freq_q_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(q_bias[1]));
   strcpy(buf, "fwd -b 1 -m 'rf -c b -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[1]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[1]);
   strcat(buf, " -m'\r");
   send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);

	strcpy(buf, "fwd -b 1 -m 'rf -c b -v ");
	sprintf(buf + strlen(buf), "%i", (28-gain));
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int get_tx_b_rf_board_status (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_board_dump (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int get_tx_b_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'board -c b -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'board -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_freq (const char* data, char* ret) {
        // don't need to tune DSP chain
	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
   uint16_t base_factor, resamp_factor;
   double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
   base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
   resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && resamp_err < base_err) {
		write_hps_reg( "txb1", resamp_factor);
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)( (((resamp_factor >> 0) & 0xff) + 1) * (((resamp_factor >> 8) & 0xff) + 1) ));
	} else {
		write_hps_reg( "txb1", base_factor);
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)( (((base_factor >> 0) & 0xff) + 1) * (((base_factor >> 8) & 0xff) + 1) ));
	}

	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	uint32_t old_val;
	uint8_t direction;

	// check for a minus or plus sign at the front
	if (data[0] == '-') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 0;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 0;
	}

	// write NCO adj
	write_hps_reg( "txb0", freq * DSP_NCO_CONST);

	// write direction
	read_hps_reg(  "txb4", &old_val);
	write_hps_reg( "txb4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_iqerr_gain (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_iqerr_phase (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txb4", &old_val);
	write_hps_reg( "txb4", old_val |  0x2);
	write_hps_reg( "txb4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int set_tx_b_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_tx_b_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txb4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "txb4", old_val | (1 << 14));
	else				write_hps_reg( "txb4", old_val & ~(1 << 14));
	return RETURN_SUCCESS;
}

static int set_tx_b_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_tx_b_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "txb5", port);
	return RETURN_SUCCESS;
}

static int set_tx_b_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
   uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && tx_power[1] == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && tx_power[1] == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		tx_power[1] = PWR_ON;

      // board commands
		strcpy(buf, "fwd -b 1 -m 'board -c b -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
      usleep(500000);

		// disable dsp channels
      for(i = 0; i < (NUM_CHANNELS * 2); i++) {
         read_hps_reg ( reg4[i], &old_val);
		   write_hps_reg( reg4[i], old_val & ~0x100);
      }

      // set all active DAC's to demo mode
      for (i = 0; i < NUM_CHANNELS; i++) {
         if (tx_power[i] == PWR_ON) {
	         strcpy(buf, "fwd -b 1 -m 'power -c ");
            strcat(buf, chan_letter[i]);
         	strcat(buf, " -d 1'\r");
   	      send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
         }
      }

      // send sync pulse
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

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
		// mute the board
		strcpy(buf, "fwd -b 1 -m 'board -c b -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "txb4", &old_val);
		write_hps_reg( "txb4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txb4", &old_val);
		write_hps_reg( "txb4", old_val & (~0x100));

		tx_power[1] = PWR_OFF;
	}

	return RETURN_SUCCESS;
}

/* TODO not all frequencies are possible, read in the value and update it */
static int set_rx_b_rf_freq_val (const char* data, char* ret) {
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// run the pll calc algorithm
	pllparam_t pll0;
	pllparam_t pll1;
	double outfreq = setFreq(&freq, &pll0, &pll1);

	// extract pllX variables and pass to MCU
	// HMC830
	strcpy(buf, "fwd -b 0 -m 'rf -c b -p 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC830 (PLL0) R
	strcpy(buf, "fwd -b 0 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC830 (PLL0) d
	strcpy(buf, "fwd -b 0 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC830 (PLL0) N
	strcpy(buf, "fwd -b 0 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll0.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// HMC833
	strcpy(buf, "fwd -b 0 -m 'rf -c b -p 1'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC833 (PLL1) R
	strcpy(buf, "fwd -b 0 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) d
	strcpy(buf, "fwd -b 0 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) N
	strcpy(buf, "fwd -b 0 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll1.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) x2en
	strcpy(buf, "fwd -b 0 -m 'rf -x ");
	sprintf(buf + strlen(buf), "%" PRIu8 "", pll1.x2en);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c b -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c b -b ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_freq_varac (const char* data, char* ret) {
	// TODO: Victor is going to do math from Voltage -> Clk Cycles
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_gain_val (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c b -g ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int get_rx_b_rf_board_status (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_board_dump (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int get_rx_b_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'board -c b -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'board -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_signed (const char* data, char* ret) {
   uint32_t old_val, sign;
   sscanf(data, "%u", &sign);
   sign = sign ? 0 : 1;

	read_hps_reg( "rxb4", &old_val);
   old_val &= ~(1 << 4);
	write_hps_reg( "rxb4", old_val | (sign << 4));
	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_freq (const char* data, char* ret) {
        // don't need to tune DSP chain
	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
   uint16_t base_factor, resamp_factor;
   double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
   base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
   resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && resamp_err < base_err) {
		write_hps_reg( "rxb1", resamp_factor);
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)( (((resamp_factor >> 0) & 0xff) + 1) * (((resamp_factor >> 8) & 0xff) + 1) ));
	} else {
		write_hps_reg( "rxb1", base_factor);
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)( (((base_factor >> 0) & 0xff) + 1) * (((base_factor >> 8) & 0xff) + 1) ));
	}

	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	uint32_t old_val;
	uint8_t direction;

	// check for a minus or plus sign at the front
	if (data[0] == '-') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 0;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 0;
	}

	// write NCO adj
	write_hps_reg( "rxb0", freq * DSP_NCO_CONST);

	// write direction
	read_hps_reg(  "rxb4", &old_val);
	write_hps_reg( "rxb4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_iqerr_gain (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_iqerr_phase (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxb4", &old_val);
	write_hps_reg( "rxb4", old_val |  0x2);
	write_hps_reg( "rxb4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_loopback (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxb4", &old_val);
	if (strcmp(data, "1") == 0)   write_hps_reg( "rxb4", (old_val & ~0x1e00) | 0x400);
   else                          write_hps_reg( "rxb4", (old_val & ~0x1e00) | 0x200);
	return RETURN_SUCCESS;
}

static int set_rx_b_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_rx_b_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxb4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "rxb4", old_val | (1 << 14));
	else				write_hps_reg( "rxb4", old_val & ~(1 << 14));
	return RETURN_SUCCESS;
}

static int set_rx_b_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_rx_b_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "rxb8", port);
	return RETURN_SUCCESS;
}

static int set_rx_b_link_ip_dest (const char* data, char* ret) {
	uint8_t ip[4];
	sscanf(data, "%"SCNd8".%"SCNd8".%"SCNd8".%"SCNd8"", ip, ip+1, ip+2, ip+3);
	write_hps_reg( "rxb5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	return RETURN_SUCCESS;
}

static int set_rx_b_link_mac_dest (const char* data, char* ret) {
	uint8_t mac[6];
	sscanf(data, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8"",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	write_hps_reg( "rxb6", (mac[0] << 8) | (mac[1]) );
	write_hps_reg( "rxb7", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
	return RETURN_SUCCESS;
}

static int set_rx_b_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
   uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && rx_power[1] == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && rx_power[1] == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		rx_power[1] = PWR_ON;

      // board commands
		strcpy(buf, "fwd -b 0 -m 'board -c b -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
      usleep(500000);

		// disable dsp channels
      for(i = 0; i < (NUM_CHANNELS * 2); i++) {
         read_hps_reg ( reg4[i], &old_val);
		   write_hps_reg( reg4[i], old_val & ~0x100);
      }

      // set all active DAC's to demo mode
      for (i = 0; i < NUM_CHANNELS; i++) {
         if (tx_power[i] == PWR_ON) {
	         strcpy(buf, "fwd -b 1 -m 'power -c ");
            strcat(buf, chan_letter[i]);
         	strcat(buf, " -d 1'\r");
   	      send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
         }
      }

      // send sync pulse
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

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
		rx_power[1] = PWR_OFF;

		// mute the board
		strcpy(buf, "fwd -b 0 -m 'board -c b -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// disable DSP core
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | 0x2);

		// disable channel
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & (~0x100));
	}
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_mixer (const char* data, char* ret) {
	// TODO: Need to implement in MCU code
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_nco (const char* data, char* ret) {
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);
	freq *= DAC_NCO_CONST;

	strcpy(buf, "fwd -b 1 -m 'dac -c c -e 0 -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(freq >> 32));
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	strcpy(buf, "fwd -b 1 -m 'dac -o ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)freq);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int get_tx_c_rf_dac_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'board -c c -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_iqerr_gain (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_iqerr_phase (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

/* TODO not all frequencies are possible, read in the value and update it */
static int set_tx_c_rf_freq_val (const char* data, char* ret) {
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// run the pll calc algorithm
	pllparam_t pll0;
	pllparam_t pll1;
	double outfreq = setFreq(&freq, &pll0, &pll1);

	// extract pllX variables and pass to MCU
	// HMC830
	strcpy(buf, "fwd -b 1 -m 'rf -c c -p 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC830 (PLL0) R
	strcpy(buf, "fwd -b 1 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC830 (PLL0) d
	strcpy(buf, "fwd -b 1 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC830 (PLL0) N
	strcpy(buf, "fwd -b 1 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll0.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// HMC833
	strcpy(buf, "fwd -b 1 -m 'rf -c c -p 1'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC833 (PLL1) R
	strcpy(buf, "fwd -b 1 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) d
	strcpy(buf, "fwd -b 1 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) N
	strcpy(buf, "fwd -b 1 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll1.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) x2en
	strcpy(buf, "fwd -b 1 -m 'rf -x ");
	sprintf(buf + strlen(buf), "%" PRIu8 "", pll1.x2en);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c c -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c c -b ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_freq_i_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(i_bias[2]));
   strcpy(buf, "fwd -b 1 -m 'rf -c c -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[2]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[2]);
   strcat(buf, " -m'\r");
   send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_freq_q_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(q_bias[2]));
   strcpy(buf, "fwd -b 1 -m 'rf -c c -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[2]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[2]);
   strcat(buf, " -m'\r");
   send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);

	strcpy(buf, "fwd -b 1 -m 'rf -c c -v ");
	sprintf(buf + strlen(buf), "%i", (28-gain));
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int get_tx_c_rf_board_status (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_board_dump (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int get_tx_c_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'board -c c -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'board -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_freq (const char* data, char* ret) {
        // don't need to tune DSP chain
	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
   uint16_t base_factor, resamp_factor;
   double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
   base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
   resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && resamp_err < base_err) {
		write_hps_reg( "txc1", resamp_factor);
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)( (((resamp_factor >> 0) & 0xff) + 1) * (((resamp_factor >> 8) & 0xff) + 1) ));
	} else {
		write_hps_reg( "txc1", base_factor);
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)( (((base_factor >> 0) & 0xff) + 1) * (((base_factor >> 8) & 0xff) + 1) ));
	}

	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	uint32_t old_val;
	uint8_t direction;

	// check for a minus or plus sign at the front
	if (data[0] == '-') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 0;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 0;
	}

	// write NCO adj
	write_hps_reg( "txc0", freq * DSP_NCO_CONST);

	// write direction
	read_hps_reg(  "txc4", &old_val);
	write_hps_reg( "txc4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_iqerr_gain (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_iqerr_phase (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txc4", &old_val);
	write_hps_reg( "txc4", old_val |  0x2);
	write_hps_reg( "txc4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int set_tx_c_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_tx_c_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txc4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "txc4", old_val | (1 << 14));
	else				write_hps_reg( "txc4", old_val & ~(1 << 14));
	return RETURN_SUCCESS;
}

static int set_tx_c_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_tx_c_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "txc5", port);
	return RETURN_SUCCESS;
}

static int set_tx_c_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
   uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && tx_power[2] == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && tx_power[2] == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		tx_power[2] = PWR_ON;

      // board commands
		strcpy(buf, "fwd -b 1 -m 'board -c c -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
      usleep(500000);

		// disable dsp channels
      for(i = 0; i < (NUM_CHANNELS * 2); i++) {
         read_hps_reg ( reg4[i], &old_val);
		   write_hps_reg( reg4[i], old_val & ~0x100);
      }

      // set all active DAC's to demo mode
      for (i = 0; i < NUM_CHANNELS; i++) {
         if (tx_power[i] == PWR_ON) {
	         strcpy(buf, "fwd -b 1 -m 'power -c ");
            strcat(buf, chan_letter[i]);
         	strcat(buf, " -d 1'\r");
   	      send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
         }
      }

      // send sync pulse
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

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
		// mute the board
		strcpy(buf, "fwd -b 1 -m 'board -c c -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "txc4", &old_val);
		write_hps_reg( "txc4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txc4", &old_val);
		write_hps_reg( "txc4", old_val & (~0x100));

		tx_power[2] = PWR_OFF;
	}

	return RETURN_SUCCESS;
}

/* TODO not all frequencies are possible, read in the value and update it */
static int set_rx_c_rf_freq_val (const char* data, char* ret) {
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// run the pll calc algorithm
	pllparam_t pll0;
	pllparam_t pll1;
	double outfreq = setFreq(&freq, &pll0, &pll1);

	// extract pllX variables and pass to MCU
	// HMC830
	strcpy(buf, "fwd -b 0 -m 'rf -c c -p 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC830 (PLL0) R
	strcpy(buf, "fwd -b 0 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC830 (PLL0) d
	strcpy(buf, "fwd -b 0 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC830 (PLL0) N
	strcpy(buf, "fwd -b 0 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll0.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// HMC833
	strcpy(buf, "fwd -b 0 -m 'rf -c c -p 1'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC833 (PLL1) R
	strcpy(buf, "fwd -b 0 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) d
	strcpy(buf, "fwd -b 0 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) N
	strcpy(buf, "fwd -b 0 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll1.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) x2en
	strcpy(buf, "fwd -b 0 -m 'rf -x ");
	sprintf(buf + strlen(buf), "%" PRIu8 "", pll1.x2en);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c c -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c c -b ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_freq_varac (const char* data, char* ret) {
	// TODO: Victor is going to do math from Voltage -> Clk Cycles
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_gain_val (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c c -g ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int get_rx_c_rf_board_status (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_board_dump (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int get_rx_c_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'board -c c -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'board -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_signed (const char* data, char* ret) {
   uint32_t old_val, sign;
   sscanf(data, "%u", &sign);
   sign = sign ? 0 : 1;

	read_hps_reg( "rxc4", &old_val);
   old_val &= ~(1 << 4);
	write_hps_reg( "rxc4", old_val | (sign << 4));
	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_freq (const char* data, char* ret) {
        // don't need to tune DSP chain
	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
   uint16_t base_factor, resamp_factor;
   double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
   base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
   resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && resamp_err < base_err) {
		write_hps_reg( "rxc1", resamp_factor);
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)( (((resamp_factor >> 0) & 0xff) + 1) * (((resamp_factor >> 8) & 0xff) + 1) ));
	} else {
		write_hps_reg( "rxc1", base_factor);
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)( (((base_factor >> 0) & 0xff) + 1) * (((base_factor >> 8) & 0xff) + 1) ));
	}

	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	uint32_t old_val;
	uint8_t direction;

	// check for a minus or plus sign at the front
	if (data[0] == '-') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 0;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 0;
	}

	// write NCO adj
	write_hps_reg( "rxc0", freq * DSP_NCO_CONST);

	// write direction
	read_hps_reg(  "rxc4", &old_val);
	write_hps_reg( "rxc4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_iqerr_gain (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_iqerr_phase (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxc4", &old_val);
	write_hps_reg( "rxc4", old_val |  0x2);
	write_hps_reg( "rxc4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_loopback (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxc4", &old_val);
	if (strcmp(data, "1") == 0)   write_hps_reg( "rxc4", (old_val & ~0x1e00) | 0x400);
   else                          write_hps_reg( "rxc4", (old_val & ~0x1e00) | 0x000);
	return RETURN_SUCCESS;
}

static int set_rx_c_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_rx_c_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxc4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "rxc4", old_val | (1 << 14));
	else				write_hps_reg( "rxc4", old_val & ~(1 << 14));
	return RETURN_SUCCESS;
}

static int set_rx_c_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_rx_c_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "rxc8", port);
	return RETURN_SUCCESS;
}

static int set_rx_c_link_ip_dest (const char* data, char* ret) {
	uint8_t ip[4];
	sscanf(data, "%"SCNd8".%"SCNd8".%"SCNd8".%"SCNd8"", ip, ip+1, ip+2, ip+3);
	write_hps_reg( "rxc5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	return RETURN_SUCCESS;
}

static int set_rx_c_link_mac_dest (const char* data, char* ret) {
	uint8_t mac[6];
	sscanf(data, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8"",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	write_hps_reg( "rxc6", (mac[0] << 8) | (mac[1]) );
	write_hps_reg( "rxc7", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
	return RETURN_SUCCESS;
}

static int set_rx_c_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
   uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && rx_power[2] == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && rx_power[2] == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		rx_power[2] = PWR_ON;

      // board commands
		strcpy(buf, "fwd -b 0 -m 'board -c c -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
      usleep(500000);

		// disable dsp channels
      for(i = 0; i < (NUM_CHANNELS * 2); i++) {
         read_hps_reg ( reg4[i], &old_val);
		   write_hps_reg( reg4[i], old_val & ~0x100);
      }

      // set all active DAC's to demo mode
      for (i = 0; i < NUM_CHANNELS; i++) {
         if (tx_power[i] == PWR_ON) {
	         strcpy(buf, "fwd -b 1 -m 'power -c ");
            strcat(buf, chan_letter[i]);
         	strcat(buf, " -d 1'\r");
   	      send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
         }
      }

      // send sync pulse
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

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
		// mute the board
		strcpy(buf, "fwd -b 0 -m 'board -c c -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & (~0x100));

		rx_power[2] = PWR_OFF;
	}
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_mixer (const char* data, char* ret) {
	// TODO: Need to implement in MCU code
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_nco (const char* data, char* ret) {
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);
	freq *= DAC_NCO_CONST;

	strcpy(buf, "fwd -b 1 -m 'dac -c d -e 1 -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)(freq >> 32));
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	strcpy(buf, "fwd -b 1 -m 'dac -o ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", (uint32_t)freq);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	return RETURN_SUCCESS;
}

static int get_tx_d_rf_dac_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'board -c d -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_iqerr_gain (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_iqerr_phase (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

/* TODO not all frequencies are possible, read in the value and update it */
static int set_tx_d_rf_freq_val (const char* data, char* ret) {
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// run the pll calc algorithm
	pllparam_t pll0;
	pllparam_t pll1;
	double outfreq = setFreq(&freq, &pll0, &pll1);

	// extract pllX variables and pass to MCU
	// HMC830
	strcpy(buf, "fwd -b 1 -m 'rf -c d -p 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC830 (PLL0) R
	strcpy(buf, "fwd -b 1 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC830 (PLL0) d
	strcpy(buf, "fwd -b 1 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC830 (PLL0) N
	strcpy(buf, "fwd -b 1 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll0.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// HMC833
	strcpy(buf, "fwd -b 1 -m 'rf -c d -p 1'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC833 (PLL1) R
	strcpy(buf, "fwd -b 1 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) d
	strcpy(buf, "fwd -b 1 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) N
	strcpy(buf, "fwd -b 1 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll1.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) x2en
	strcpy(buf, "fwd -b 1 -m 'rf -x ");
	sprintf(buf + strlen(buf), "%" PRIu8 "", pll1.x2en);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c d -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c d -b ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_freq_i_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(i_bias[3]));
   strcpy(buf, "fwd -b 1 -m 'rf -c d -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[3]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[3]);
   strcat(buf, " -m'\r");
   send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_freq_q_bias (const char* data, char* ret) {
   sscanf(data, "%i", &(q_bias[3]));
   strcpy(buf, "fwd -b 1 -m 'rf -c d -i ");
   sprintf(buf + strlen(buf), "%i", i_bias[3]);
   strcat(buf, " -q ");
   sprintf(buf + strlen(buf), "%i", q_bias[3]);
   strcat(buf, " -m'\r");
   send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);

	strcpy(buf, "fwd -b 1 -m 'rf -c d -v ");
	sprintf(buf + strlen(buf), "%i", (28-gain));
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int get_tx_d_rf_board_status (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_board_dump (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int get_tx_d_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'board -c d -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'board -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_freq (const char* data, char* ret) {
        // don't need to tune DSP chain
	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
   uint16_t base_factor, resamp_factor;
   double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
   base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
   resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && resamp_err < base_err) {
		write_hps_reg( "txd1", resamp_factor);
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)( (((resamp_factor >> 0) & 0xff) + 1) * (((resamp_factor >> 8) & 0xff) + 1) ));
	} else {
		write_hps_reg( "txd1", base_factor);
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)( (((base_factor >> 0) & 0xff) + 1) * (((base_factor >> 8) & 0xff) + 1) ));
	}

	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	uint32_t old_val;
	uint8_t direction;

	// check for a minus or plus sign at the front
	if (data[0] == '-') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 0;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 0;
	}

	// write NCO adj
	write_hps_reg( "txd0", freq * DSP_NCO_CONST);

	// write direction
	read_hps_reg(  "txd4", &old_val);
	write_hps_reg( "txd4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_iqerr_gain (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_iqerr_phase (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txd4", &old_val);
	write_hps_reg( "txd4", old_val |  0x2);
	write_hps_reg( "txd4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int set_tx_d_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_tx_d_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txd4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "txd4", old_val | (1 << 14));
	else				write_hps_reg( "txd4", old_val & ~(1 << 14));
	return RETURN_SUCCESS;
}

static int set_tx_d_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_tx_d_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "txd5", port);
	return RETURN_SUCCESS;
}

static int set_tx_d_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
   uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && tx_power[3] == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && tx_power[3] == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		tx_power[3] = PWR_ON;

      // board commands
		strcpy(buf, "fwd -b 1 -m 'board -c d -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
      usleep(500000);

		// disable dsp channels
      for(i = 0; i < (NUM_CHANNELS * 2); i++) {
         read_hps_reg ( reg4[i], &old_val);
		   write_hps_reg( reg4[i], old_val & ~0x100);
      }

      // set all active DAC's to demo mode
      for (i = 0; i < NUM_CHANNELS; i++) {
         if (tx_power[i] == PWR_ON) {
	         strcpy(buf, "fwd -b 1 -m 'power -c ");
            strcat(buf, chan_letter[i]);
         	strcat(buf, " -d 1'\r");
   	      send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
         }
      }

      // send sync pulse
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

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
		// mute the board
		strcpy(buf, "fwd -b 1 -m 'board -c d -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "txd4", &old_val);
		write_hps_reg( "txd4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txd4", &old_val);
		write_hps_reg( "txd4", old_val & (~0x100));

		tx_power[3] = PWR_OFF;
	}
	return RETURN_SUCCESS;
}

/* TODO not all frequencies are possible, read in the value and update it */
static int set_rx_d_rf_freq_val (const char* data, char* ret) {
	uint64_t freq;
	sscanf(data, "%"SCNd64"", &freq);

	// run the pll calc algorithm
	pllparam_t pll0;
	pllparam_t pll1;
	double outfreq = setFreq(&freq, &pll0, &pll1);

	// extract pllX variables and pass to MCU
	// HMC830
	strcpy(buf, "fwd -b 0 -m 'rf -c d -p 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC830 (PLL0) R
	strcpy(buf, "fwd -b 0 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC830 (PLL0) d
	strcpy(buf, "fwd -b 0 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll0.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC830 (PLL0) N
	strcpy(buf, "fwd -b 0 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll0.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// HMC833
	strcpy(buf, "fwd -b 0 -m 'rf -c d -p 1'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// write HMC833 (PLL1) R
	strcpy(buf, "fwd -b 0 -m 'rf -r ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.R);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) d
	strcpy(buf, "fwd -b 0 -m 'rf -d ");
	sprintf(buf + strlen(buf), "%" PRIu16 "", pll1.d);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) N
	strcpy(buf, "fwd -b 0 -m 'rf -n ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", pll1.N);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	usleep(100000);

	// write HMC833 (PLL1) x2en
	strcpy(buf, "fwd -b 0 -m 'rf -x ");
	sprintf(buf + strlen(buf), "%" PRIu8 "", pll1.x2en);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	sprintf(ret, "%lf", outfreq);

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c d -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_freq_band (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c d -b ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_freq_varac (const char* data, char* ret) {
	// TODO: Victor is going to do math from Voltage -> Clk Cycles
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_gain_val (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c d -g ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int get_rx_d_rf_board_status (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_board_dump (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int get_rx_d_rf_board_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'board -c d -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_board_led (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'board -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}


static int set_rx_d_dsp_signed (const char* data, char* ret) {
   uint32_t old_val, sign;
   sscanf(data, "%u", &sign);
   sign = sign ? 0 : 1;

	read_hps_reg( "rxd4", &old_val);
   old_val &= ~(1 << 4);
	write_hps_reg( "rxd4", old_val | (sign << 4));
	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_freq (const char* data, char* ret) {
        // don't need to tune DSP chain
	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_rate (const char* data, char* ret) {
	uint32_t old_val;
   uint16_t base_factor, resamp_factor;
   double base_err, resamp_err;
	double rate;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
   base_factor = get_optimal_sr_factor(rate, BASE_SAMPLE_RATE, &base_err);
   resamp_factor = get_optimal_sr_factor(rate, RESAMP_SAMPLE_RATE, &resamp_err);

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && resamp_err < base_err) {
		write_hps_reg( "rxd1", resamp_factor);
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | (1 << 15));
		sprintf(ret, "%lf", RESAMP_SAMPLE_RATE/(double)( (((resamp_factor >> 0) & 0xff) + 1) * (((resamp_factor >> 8) & 0xff) + 1) ));
	} else {
		write_hps_reg( "rxd1", base_factor);
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & ~(1 << 15));
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(double)( (((base_factor >> 0) & 0xff) + 1) * (((base_factor >> 8) & 0xff) + 1) ));
	}

	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	uint32_t old_val;
	uint8_t direction;

	// check for a minus or plus sign at the front
	if (data[0] == '-') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 0;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 0;
	}

	// write NCO adj
	write_hps_reg( "rxd0", freq * DSP_NCO_CONST);

	// write direction
	read_hps_reg(  "rxd4", &old_val);
	write_hps_reg( "rxd4", (old_val & ~(0x1 << 13) ) | (direction << 13));
	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_iqerr_gain (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_iqerr_phase (const char* data, char* ret) {
	// TODO: FW work
	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_rstreq (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxd4", &old_val);
	write_hps_reg( "rxd4", old_val |  0x2);
	write_hps_reg( "rxd4", old_val & ~0x2);
	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_loopback (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxd4", &old_val);
	if (strcmp(data, "1") == 0)   write_hps_reg( "rxd4", (old_val & ~0x1e00) | 0x400);
   else                          write_hps_reg( "rxd4", (old_val & ~0x1e00) | 0x200);
	return RETURN_SUCCESS;
}

static int set_rx_d_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_rx_d_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxd4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "rxd4", old_val | (1 << 14));
	else				write_hps_reg( "rxd4", old_val & ~(1 << 14));
	return RETURN_SUCCESS;
}

static int set_rx_d_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_rx_d_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "rxd8", port);
	return RETURN_SUCCESS;
}

static int set_rx_d_link_ip_dest (const char* data, char* ret) {
	uint8_t ip[4];
	sscanf(data, "%"SCNd8".%"SCNd8".%"SCNd8".%"SCNd8"", ip, ip+1, ip+2, ip+3);
	write_hps_reg( "rxd5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	return RETURN_SUCCESS;
}

static int set_rx_d_link_mac_dest (const char* data, char* ret) {
	uint8_t mac[6];
	sscanf(data, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8"",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	write_hps_reg( "rxd6", (mac[0] << 8) | (mac[1]) );
	write_hps_reg( "rxd7", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
	return RETURN_SUCCESS;
}

static int set_rx_d_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
   uint8_t i;
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && rx_power[3] == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && rx_power[3] == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		rx_power[3] = PWR_ON;

      // board commands
		strcpy(buf, "fwd -b 0 -m 'board -c d -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
      usleep(500000);

		// disable dsp channels
      for(i = 0; i < (NUM_CHANNELS * 2); i++) {
         read_hps_reg ( reg4[i], &old_val);
		   write_hps_reg( reg4[i], old_val & ~0x100);
      }

      // set all active DAC's to demo mode
      for (i = 0; i < NUM_CHANNELS; i++) {
         if (tx_power[i] == PWR_ON) {
	         strcpy(buf, "fwd -b 1 -m 'power -c ");
            strcat(buf, chan_letter[i]);
         	strcat(buf, " -d 1'\r");
   	      send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
         }
      }

      // send sync pulse
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

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
		// mute the board
		strcpy(buf, "fwd -b 0 -m 'board -c d -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & (~0x100));

		rx_power[3] = PWR_OFF;
	}
	return RETURN_SUCCESS;
}

static int set_time_clk_pps (const char* data, char* ret) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_clk_cur_time (const char* data, char* ret) {
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

static int set_time_source_rate (const char* data, char* ret) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_source_vco (const char* data, char* ret) {
	if (strcmp(data, "external") == 0) {
		strcpy(buf, "fwd -b 2 -m 'clk -v 1'\r");
	} else if (strcmp(data, "internal") == 0) {
		strcpy(buf, "fwd -b 2 -m 'clk -v 0'\r");
	}
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_time_source_sync (const char* data, char* ret) {
	if (strcmp(data, "external") == 0) {
		strcpy(buf, "fwd -b 2 -m 'clk -n 1'\r");
	} else if (strcmp(data, "internal") == 0) {
		strcpy(buf, "fwd -b 2 -m 'clk -n 0'\r");
	}
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

// 10 MHz clock
static int set_time_source_ref (const char* data, char* ret) {
	if (strcmp(data, "external") == 0) {
		strcpy(buf, "fwd -b 2 -m 'clk -t 1'\r");
	} else if (strcmp(data, "internal") == 0) {
		strcpy(buf, "fwd -b 2 -m 'clk -t 0'\r");
	}
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int get_time_board_status (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_time_board_dump (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_time_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int get_time_board_temp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 2 -m 'board -t'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_time_board_led (const char* data, char* ret) {
	strcpy(buf, "fwd -b 2 -m 'board -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_time_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int get_fpga_board_status (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_fpga_board_dump (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int set_fpga_board_test (const char* data, char* ret) {
	// TODO: MCU code cleanup
	return RETURN_SUCCESS;
}

static int get_fpga_board_temp (const char* data, char* ret) {
	strcpy(buf, "board -t");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	recv_uart_comm(uart_fd, (uint8_t*)data, &rd_len, MAX_PROP_LEN);
	return RETURN_SUCCESS;
}

static int set_fpga_board_led (const char* data, char* ret) {
	strcpy(buf, "board -l ");
	strcat(buf, data);
	strcat(buf, "\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_fpga_board_rstreq (const char* data, char* ret) {
	strcpy(buf, "fpga -r \r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_fpga_board_jesd_sync (const char* data, char* ret) {
	strcpy(buf, "fpga -o \r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_fpga_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_fpga_link_rate (const char* data, char* ret) {
   // TODO: Need to implement in FW
	return RETURN_SUCCESS;
}

static int set_fpga_link_sfpa_ip_addr (const char* data, char* ret) {
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

static int set_fpga_link_sfpa_mac_addr (const char* data, char* ret) {
	uint8_t mac[6];
	sscanf(data, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8"",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	write_hps_reg( "net11", (mac[0] << 8) | (mac[1]) );
	write_hps_reg( "net12", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
	return RETURN_SUCCESS;
}

static int set_fpga_link_sfpa_ver (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t ver;
	sscanf(data, "%"SCNd8"", &ver);
	read_hps_reg(  "net0", &old_val);
	if (ver > 0)	write_hps_reg( "net0", (old_val | 0x4));
	else		write_hps_reg( "net0", (old_val & ~(0x4) ));
	return RETURN_SUCCESS;
}

static int set_fpga_link_sfpa_pay_len (const char* data, char* ret) {
	uint32_t old_val;
	uint32_t pay_len;
	sscanf(data, "%"SCNd32"", &pay_len);
	read_hps_reg(  "net0", &old_val);
	write_hps_reg( "net0",  (old_val & ~(0xffff0000) ) | (pay_len << 16));
	return RETURN_SUCCESS;
}

static int set_fpga_link_sfpb_ip_addr (const char* data, char* ret) {
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

static int set_fpga_link_sfpb_mac_addr (const char* data, char* ret) {
	uint8_t mac[6];
	sscanf(data, "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8"",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
	write_hps_reg( "net26", (mac[0] << 8) | (mac[1]) );
	write_hps_reg( "net27", (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]);
	return RETURN_SUCCESS;
}

static int set_fpga_link_sfpb_ver (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t ver;
	sscanf(data, "%"SCNd8"", &ver);
	read_hps_reg(  "net15", &old_val);
	if (ver > 0)	write_hps_reg( "net15", (old_val & ~(1 << 2) ) | (1 << 2));
	else		write_hps_reg( "net15", (old_val & ~(1 << 2) ));
	return RETURN_SUCCESS;
}

static int set_fpga_link_sfpb_pay_len (const char* data, char* ret) {
	uint32_t old_val;
	uint32_t pay_len;
	sscanf(data, "%"SCNd32"", &pay_len);
	read_hps_reg(  "net15", &old_val);
	write_hps_reg( "net15",  (old_val & ~(0xffff0000) ) | (pay_len << 16));
	return RETURN_SUCCESS;
}

static int set_fpga_link_net_dhcp_en (const char* data, char* ret) {

	return RETURN_SUCCESS;
}

static int set_fpga_link_net_hostname (const char* data, char* ret) {
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

static int set_fpga_link_net_ip_addr (const char* data, char* ret) {
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

static int set_poll_en (const char* data, char* ret) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

// Beginning of property table
static prop_t property_table[] = {
	{"tx_a/pwr", get_invalid, set_tx_a_pwr, RW, NO_POLL, "0"},
	{"tx_a/rf/dac/mixer", get_invalid, set_tx_a_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_a/rf/dac/nco", get_invalid, set_tx_a_rf_dac_nco, RW, NO_POLL, "15000000"},
	{"tx_a/rf/dac/temp", get_tx_a_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_a/rf/dac/iqerr_gain", get_invalid, set_tx_a_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_a/rf/dac/iqerr_phase", get_invalid, set_tx_a_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_a/rf/freq/val", get_invalid, set_tx_a_rf_freq_val, RW, NO_POLL, "450000000"},
	{"tx_a/rf/freq/lna", get_invalid, set_tx_a_rf_freq_lna, RW, NO_POLL, "0"},
	{"tx_a/rf/freq/band", get_invalid, set_tx_a_rf_freq_band, RW, NO_POLL, "1"},
	{"tx_a/rf/freq/i_bias", get_invalid, set_tx_a_rf_freq_i_bias, RW, NO_POLL, "17"},
	{"tx_a/rf/freq/q_bias", get_invalid, set_tx_a_rf_freq_q_bias, RW, NO_POLL, "17"},
	{"tx_a/rf/gain/val", get_invalid, set_tx_a_rf_gain_val, RW, NO_POLL, "14"},
	{"tx_a/board/status", get_tx_a_rf_board_status, set_invalid, RO, POLL, "off"},
	{"tx_a/board/dump", get_invalid, set_tx_a_rf_board_dump, WO, NO_POLL, "0"},
	{"tx_a/board/test", get_invalid, set_tx_a_rf_board_test, WO, NO_POLL, "0"},
	{"tx_a/board/temp", get_tx_a_rf_board_temp, set_invalid, RO, POLL, "23"},
	{"tx_a/board/led", get_invalid, set_tx_a_rf_board_led, WO, NO_POLL, "0"},
	{"tx_a/dsp/freq", get_invalid, set_tx_a_dsp_freq, RW, NO_POLL, "2400"},
	{"tx_a/dsp/gain", get_invalid, set_tx_a_dsp_gain, RW, NO_POLL, "10"},
	{"tx_a/dsp/rate", get_invalid, set_tx_a_dsp_rate, RW, NO_POLL, "1258850"},
	{"tx_a/dsp/nco_adj", get_invalid, set_tx_a_dsp_nco_adj, RW, NO_POLL, "0"},
	{"tx_a/dsp/iqerr_gain", get_invalid, set_tx_a_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_a/dsp/iqerr_phase", get_invalid, set_tx_a_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_a/dsp/rstreq", get_invalid, set_tx_a_dsp_rstreq, WO, NO_POLL, "0"},
	{"tx_a/about/id", get_invalid, set_tx_a_about_id, RW, NO_POLL, "001"},
	{"tx_a/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"tx_a/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"tx_a/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"tx_a/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"tx_a/link/vita_en", get_invalid, set_tx_a_link_vita_en, RW, NO_POLL, "0"},
	{"tx_a/link/iface", get_invalid, set_tx_a_link_iface, RW, NO_POLL, "sfpa"},
	{"tx_a/link/port", get_invalid, set_tx_a_link_port, RW, NO_POLL, "42824"},
	{"rx_a/pwr", get_invalid, set_rx_a_pwr, RW, NO_POLL, "0"},
	{"rx_a/rf/freq/val", get_invalid, set_rx_a_rf_freq_val, RW, NO_POLL, "450000000"},
	{"rx_a/rf/freq/lna", get_invalid, set_rx_a_rf_freq_lna, RW, NO_POLL, "0"},
	{"rx_a/rf/freq/band", get_invalid, set_rx_a_rf_freq_band, RW, NO_POLL, "1"},
	{"rx_a/rf/freq/varac", get_invalid, set_rx_a_rf_freq_varac, RW, NO_POLL, "0"},
	{"rx_a/rf/gain/val", get_invalid, set_rx_a_rf_gain_val, RW, NO_POLL, "35"},
	{"rx_a/board/status", get_rx_a_rf_board_status, set_invalid, RO, POLL, "off"},
	{"rx_a/board/dump", get_invalid, set_rx_a_rf_board_dump, WO, NO_POLL, "0"},
	{"rx_a/board/test", get_invalid, set_rx_a_rf_board_test, WO, NO_POLL, "0"},
	{"rx_a/board/temp", get_rx_a_rf_board_temp, set_invalid, RO, POLL, "20"},
	{"rx_a/board/led", get_invalid, set_rx_a_rf_board_led, WO, NO_POLL, "0"},
	{"rx_a/dsp/signed", get_invalid, set_rx_a_dsp_signed, RW, NO_POLL, "1"},
	{"rx_a/dsp/freq", get_invalid, set_rx_a_dsp_freq, RW, NO_POLL, "2400"},
	{"rx_a/dsp/gain", get_invalid, set_rx_a_dsp_gain, RW, NO_POLL, "10"},
	{"rx_a/dsp/rate", get_invalid, set_rx_a_dsp_rate, RW, NO_POLL, "1258850"},
	{"rx_a/dsp/nco_adj", get_invalid, set_rx_a_dsp_nco_adj, RW, NO_POLL, "-15000000"},
	{"rx_a/dsp/iqerr_gain", get_invalid, set_rx_a_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"rx_a/dsp/iqerr_phase", get_invalid, set_rx_a_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"rx_a/dsp/rstreq", get_invalid, set_rx_a_dsp_rstreq, WO, NO_POLL, "0"},
	{"rx_a/dsp/loopback", get_invalid, set_rx_a_dsp_loopback, RW, NO_POLL, "0"},
	{"rx_a/about/id", get_invalid, set_rx_a_about_id, RW, NO_POLL, "001"},
	{"rx_a/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"rx_a/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"rx_a/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"rx_a/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"rx_a/link/vita_en", get_invalid, set_rx_a_link_vita_en, RW, NO_POLL, "0"},
	{"rx_a/link/iface", get_invalid, set_rx_a_link_iface, RW, NO_POLL, "sfpa"},
	{"rx_a/link/port", get_invalid, set_rx_a_link_port, RW, NO_POLL, "42820"},
	{"rx_a/link/ip_dest", get_invalid, set_rx_a_link_ip_dest, RW, NO_POLL, "10.10.10.10"},
	{"rx_a/link/mac_dest", get_invalid, set_rx_a_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"tx_b/pwr", get_invalid, set_tx_b_pwr, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/mixer", get_invalid, set_tx_b_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/nco", get_invalid, set_tx_b_rf_dac_nco, RW, NO_POLL, "15000000"},
	{"tx_b/rf/dac/temp", get_tx_b_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_b/rf/dac/iqerr_gain", get_invalid, set_tx_b_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/iqerr_phase", get_invalid, set_tx_b_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_b/rf/freq/val", get_invalid, set_tx_b_rf_freq_val, RW, NO_POLL, "450000000"},
	{"tx_b/rf/freq/lna", get_invalid, set_tx_b_rf_freq_lna, RW, NO_POLL, "0"},
	{"tx_b/rf/freq/band", get_invalid, set_tx_b_rf_freq_band, RW, NO_POLL, "1"},
	{"tx_b/rf/freq/i_bias", get_invalid, set_tx_b_rf_freq_i_bias, RW, NO_POLL, "17"},
	{"tx_b/rf/freq/q_bias", get_invalid, set_tx_b_rf_freq_q_bias, RW, NO_POLL, "17"},
	{"tx_b/rf/gain/val", get_invalid, set_tx_b_rf_gain_val, RW, NO_POLL, "14"},
	{"tx_b/board/status", get_tx_b_rf_board_status, set_invalid, RO, POLL, "off"},
	{"tx_b/board/dump", get_invalid, set_tx_b_rf_board_dump, WO, NO_POLL, "0"},
	{"tx_b/board/test", get_invalid, set_tx_b_rf_board_test, WO, NO_POLL, "0"},
	{"tx_b/board/temp", get_tx_b_rf_board_temp, set_invalid, RO, POLL, "23"},
	{"tx_b/board/led", get_invalid, set_tx_b_rf_board_led, WO, NO_POLL, "0"},
	{"tx_b/dsp/freq", get_invalid, set_tx_b_dsp_freq, RW, NO_POLL, "2400"},
	{"tx_b/dsp/gain", get_invalid, set_tx_b_dsp_gain, RW, NO_POLL, "10"},
	{"tx_b/dsp/rate", get_invalid, set_tx_b_dsp_rate, RW, NO_POLL, "1258850"},
	{"tx_b/dsp/nco_adj", get_invalid, set_tx_b_dsp_nco_adj, RW, NO_POLL, "0"},
	{"tx_b/dsp/iqerr_gain", get_invalid, set_tx_b_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_b/dsp/iqerr_phase", get_invalid, set_tx_b_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_b/dsp/rstreq", get_invalid, set_tx_b_dsp_rstreq, WO, NO_POLL, "0"},
	{"tx_b/about/id", get_invalid, set_tx_b_about_id, RW, NO_POLL, "001"},
	{"tx_b/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"tx_b/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"tx_b/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"tx_b/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"tx_b/link/vita_en", get_invalid, set_tx_b_link_vita_en, RW, NO_POLL, "0"},
	{"tx_b/link/iface", get_invalid, set_tx_b_link_iface, RW, NO_POLL, "sfpb"},
	{"tx_b/link/port", get_invalid, set_tx_b_link_port, RW, NO_POLL, "42825"},
	{"rx_b/pwr", get_invalid, set_rx_b_pwr, RW, NO_POLL, "0"},
	{"rx_b/rf/freq/val", get_invalid, set_rx_b_rf_freq_val, RW, NO_POLL, "450000000"},
	{"rx_b/rf/freq/lna", get_invalid, set_rx_b_rf_freq_lna, RW, NO_POLL, "0"},
	{"rx_b/rf/freq/band", get_invalid, set_rx_b_rf_freq_band, RW, NO_POLL, "1"},
	{"rx_b/rf/freq/varac", get_invalid, set_rx_b_rf_freq_varac, RW, NO_POLL, "0"},
	{"rx_b/rf/gain/val", get_invalid, set_rx_b_rf_gain_val, RW, NO_POLL, "35"},
	{"rx_b/board/status", get_rx_b_rf_board_status, set_invalid, RO, POLL, "off"},
	{"rx_b/board/dump", get_invalid, set_rx_b_rf_board_dump, WO, NO_POLL, "0"},
	{"rx_b/board/test", get_invalid, set_rx_b_rf_board_test, WO, NO_POLL, "0"},
	{"rx_b/board/temp", get_rx_b_rf_board_temp, set_invalid, RO, POLL, "20"},
	{"rx_b/board/led", get_invalid, set_rx_b_rf_board_led, WO, NO_POLL, "0"},
	{"rx_b/dsp/signed", get_invalid, set_rx_b_dsp_signed, RW, NO_POLL, "1"},
	{"rx_b/dsp/freq", get_invalid, set_rx_b_dsp_freq, RW, NO_POLL, "2400"},
	{"rx_b/dsp/gain", get_invalid, set_rx_b_dsp_gain, RW, NO_POLL, "10"},
	{"rx_b/dsp/rate", get_invalid, set_rx_b_dsp_rate, RW, NO_POLL, "1258850"},
	{"rx_b/dsp/nco_adj", get_invalid, set_rx_b_dsp_nco_adj, RW, NO_POLL, "-15000000"},
	{"rx_b/dsp/iqerr_gain", get_invalid, set_rx_b_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"rx_b/dsp/iqerr_phase", get_invalid, set_rx_b_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"rx_b/dsp/rstreq", get_invalid, set_rx_b_dsp_rstreq, WO, NO_POLL, "0"},
	{"rx_b/dsp/loopback", get_invalid, set_rx_b_dsp_loopback, RW, NO_POLL, "0"},
	{"rx_b/about/id", get_invalid, set_rx_b_about_id, RW, NO_POLL, "001"},
	{"rx_b/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"rx_b/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"rx_b/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"rx_b/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"rx_b/link/vita_en", get_invalid, set_rx_b_link_vita_en, RW, NO_POLL, "0"},
	{"rx_b/link/iface", get_invalid, set_rx_b_link_iface, RW, NO_POLL, "sfpb"},
	{"rx_b/link/port", get_invalid, set_rx_b_link_port, RW, NO_POLL, "42821"},
	{"rx_b/link/ip_dest", get_invalid, set_rx_b_link_ip_dest, RW, NO_POLL, "10.10.11.10"},
	{"rx_b/link/mac_dest", get_invalid, set_rx_b_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"tx_c/pwr", get_invalid, set_tx_c_pwr, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/mixer", get_invalid, set_tx_c_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/nco", get_invalid, set_tx_c_rf_dac_nco, RW, NO_POLL, "15000000"},
	{"tx_c/rf/dac/temp", get_tx_c_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_c/rf/dac/iqerr_gain", get_invalid, set_tx_c_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/iqerr_phase", get_invalid, set_tx_c_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_c/rf/freq/val", get_invalid, set_tx_c_rf_freq_val, RW, NO_POLL, "450000000"},
	{"tx_c/rf/freq/lna", get_invalid, set_tx_c_rf_freq_lna, RW, NO_POLL, "0"},
	{"tx_c/rf/freq/band", get_invalid, set_tx_c_rf_freq_band, RW, NO_POLL, "1"},
	{"tx_c/rf/freq/i_bias", get_invalid, set_tx_c_rf_freq_i_bias, RW, NO_POLL, "17"},
	{"tx_c/rf/freq/q_bias", get_invalid, set_tx_c_rf_freq_q_bias, RW, NO_POLL, "17"},
	{"tx_c/rf/gain/val", get_invalid, set_tx_c_rf_gain_val, RW, NO_POLL, "14"},
	{"tx_c/board/status", get_tx_c_rf_board_status, set_invalid, RO, POLL, "off"},
	{"tx_c/board/dump", get_invalid, set_tx_c_rf_board_dump, WO, NO_POLL, "0"},
	{"tx_c/board/test", get_invalid, set_tx_c_rf_board_test, WO, NO_POLL, "0"},
	{"tx_c/board/temp", get_tx_c_rf_board_temp, set_invalid, RO, POLL, "23"},
	{"tx_c/board/led", get_invalid, set_tx_c_rf_board_led, WO, NO_POLL, "0"},
	{"tx_c/dsp/freq", get_invalid, set_tx_c_dsp_freq, RW, NO_POLL, "2400"},
	{"tx_c/dsp/gain", get_invalid, set_tx_c_dsp_gain, RW, NO_POLL, "10"},
	{"tx_c/dsp/rate", get_invalid, set_tx_c_dsp_rate, RW, NO_POLL, "1258850"},
	{"tx_c/dsp/nco_adj", get_invalid, set_tx_c_dsp_nco_adj, RW, NO_POLL, "0"},
	{"tx_c/dsp/iqerr_gain", get_invalid, set_tx_c_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_c/dsp/iqerr_phase", get_invalid, set_tx_c_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_c/dsp/rstreq", get_invalid, set_tx_c_dsp_rstreq, WO, NO_POLL, "0"},
	{"tx_c/about/id", get_invalid, set_tx_c_about_id, RW, NO_POLL, "001"},
	{"tx_c/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"tx_c/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"tx_c/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"tx_c/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"tx_c/link/vita_en", get_invalid, set_tx_c_link_vita_en, RW, NO_POLL, "0"},
	{"tx_c/link/iface", get_invalid, set_tx_c_link_iface, RW, NO_POLL, "sfpa"},
	{"tx_c/link/port", get_invalid, set_tx_c_link_port, RW, NO_POLL, "42826"},
	{"rx_c/pwr", get_invalid, set_rx_c_pwr, RW, NO_POLL, "0"},
	{"rx_c/rf/freq/val", get_invalid, set_rx_c_rf_freq_val, RW, NO_POLL, "450000000"},
	{"rx_c/rf/freq/lna", get_invalid, set_rx_c_rf_freq_lna, RW, NO_POLL, "0"},
	{"rx_c/rf/freq/band", get_invalid, set_rx_c_rf_freq_band, RW, NO_POLL, "1"},
	{"rx_c/rf/freq/varac", get_invalid, set_rx_c_rf_freq_varac, RW, NO_POLL, "0"},
	{"rx_c/rf/gain/val", get_invalid, set_rx_c_rf_gain_val, RW, NO_POLL, "35"},
	{"rx_c/board/status", get_rx_c_rf_board_status, set_invalid, RO, POLL, "off"},
	{"rx_c/board/dump", get_invalid, set_rx_c_rf_board_dump, WO, NO_POLL, "0"},
	{"rx_c/board/test", get_invalid, set_rx_c_rf_board_test, WO, NO_POLL, "0"},
	{"rx_c/board/temp", get_rx_c_rf_board_temp, set_invalid, RO, POLL, "20"},
	{"rx_c/board/led", get_invalid, set_rx_c_rf_board_led, WO, NO_POLL, "0"},
	{"rx_c/dsp/signed", get_invalid, set_rx_c_dsp_signed, RW, NO_POLL, "1"},
	{"rx_c/dsp/freq", get_invalid, set_rx_c_dsp_freq, RW, NO_POLL, "2400"},
	{"rx_c/dsp/gain", get_invalid, set_rx_c_dsp_gain, RW, NO_POLL, "10"},
	{"rx_c/dsp/rate", get_invalid, set_rx_c_dsp_rate, RW, NO_POLL, "1258850"},
	{"rx_c/dsp/nco_adj", get_invalid, set_rx_c_dsp_nco_adj, RW, NO_POLL, "-15000000"},
	{"rx_c/dsp/iqerr_gain", get_invalid, set_rx_c_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"rx_c/dsp/iqerr_phase", get_invalid, set_rx_c_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"rx_c/dsp/rstreq", get_invalid, set_rx_c_dsp_rstreq, WO, NO_POLL, "0"},
	{"rx_c/dsp/loopback", get_invalid, set_rx_c_dsp_loopback, RW, NO_POLL, "0"},
	{"rx_c/about/id", get_invalid, set_rx_c_about_id, RW, NO_POLL, "001"},
	{"rx_c/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"rx_c/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"rx_c/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"rx_c/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"rx_c/link/vita_en", get_invalid, set_rx_c_link_vita_en, RW, NO_POLL, "0"},
	{"rx_c/link/iface", get_invalid, set_rx_c_link_iface, RW, NO_POLL, "sfpa"},
	{"rx_c/link/port", get_invalid, set_rx_c_link_port, RW, NO_POLL, "42822"},
	{"rx_c/link/ip_dest", get_invalid, set_rx_c_link_ip_dest, RW, NO_POLL, "10.10.10.10"},
	{"rx_c/link/mac_dest", get_invalid, set_rx_c_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"tx_d/pwr", get_invalid, set_tx_d_pwr, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/mixer", get_invalid, set_tx_d_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/nco", get_invalid, set_tx_d_rf_dac_nco, RW, NO_POLL, "15000000"},
	{"tx_d/rf/dac/temp", get_tx_d_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_d/rf/dac/iqerr_gain", get_invalid, set_tx_d_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/iqerr_phase", get_invalid, set_tx_d_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_d/rf/freq/val", get_invalid, set_tx_d_rf_freq_val, RW, NO_POLL, "450000000"},
	{"tx_d/rf/freq/lna", get_invalid, set_tx_d_rf_freq_lna, RW, NO_POLL, "0"},
	{"tx_d/rf/freq/band", get_invalid, set_tx_d_rf_freq_band, RW, NO_POLL, "1"},
	{"tx_d/rf/freq/i_bias", get_invalid, set_tx_d_rf_freq_i_bias, RW, NO_POLL, "17"},
	{"tx_d/rf/freq/q_bias", get_invalid, set_tx_d_rf_freq_q_bias, RW, NO_POLL, "17"},
	{"tx_d/rf/gain/val", get_invalid, set_tx_d_rf_gain_val, RW, NO_POLL, "14"},
	{"tx_d/board/status", get_tx_d_rf_board_status, set_invalid, RO, POLL, "off"},
	{"tx_d/board/dump", get_invalid, set_tx_d_rf_board_dump, WO, NO_POLL, "0"},
	{"tx_d/board/test", get_invalid, set_tx_d_rf_board_test, WO, NO_POLL, "0"},
	{"tx_d/board/temp", get_tx_d_rf_board_temp, set_invalid, RO, POLL, "23"},
	{"tx_d/board/led", get_invalid, set_tx_d_rf_board_led, WO, NO_POLL, "0"},
	{"tx_d/dsp/freq", get_invalid, set_tx_d_dsp_freq, RW, NO_POLL, "2400"},
	{"tx_d/dsp/gain", get_invalid, set_tx_d_dsp_gain, RW, NO_POLL, "10"},
	{"tx_d/dsp/rate", get_invalid, set_tx_d_dsp_rate, RW, NO_POLL, "1258850"},
	{"tx_d/dsp/nco_adj", get_invalid, set_tx_d_dsp_nco_adj, RW, NO_POLL, "0"},
	{"tx_d/dsp/iqerr_gain", get_invalid, set_tx_d_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_d/dsp/iqerr_phase", get_invalid, set_tx_d_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_d/dsp/rstreq", get_invalid, set_tx_d_dsp_rstreq, WO, NO_POLL, "0"},
	{"tx_d/about/id", get_invalid, set_tx_d_about_id, RW, NO_POLL, "001"},
	{"tx_d/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"tx_d/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"tx_d/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"tx_d/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"tx_d/link/vita_en", get_invalid, set_tx_d_link_vita_en, RW, NO_POLL, "0"},
	{"tx_d/link/iface", get_invalid, set_tx_d_link_iface, RW, NO_POLL, "sfpb"},
	{"tx_d/link/port", get_invalid, set_tx_d_link_port, RW, NO_POLL, "42827"},
	{"rx_d/pwr", get_invalid, set_rx_d_pwr, RW, NO_POLL, "0"},
	{"rx_d/rf/freq/val", get_invalid, set_rx_d_rf_freq_val, RW, NO_POLL, "450000000"},
	{"rx_d/rf/freq/lna", get_invalid, set_rx_d_rf_freq_lna, RW, NO_POLL, "0"},
	{"rx_d/rf/freq/band", get_invalid, set_rx_d_rf_freq_band, RW, NO_POLL, "1"},
	{"rx_d/rf/freq/varac", get_invalid, set_rx_d_rf_freq_varac, RW, NO_POLL, "0"},
	{"rx_d/rf/gain/val", get_invalid, set_rx_d_rf_gain_val, RW, NO_POLL, "35"},
	{"rx_d/board/status", get_rx_d_rf_board_status, set_invalid, RO, POLL, "off"},
	{"rx_d/board/dump", get_invalid, set_rx_d_rf_board_dump, WO, NO_POLL, "0"},
	{"rx_d/board/test", get_invalid, set_rx_d_rf_board_test, WO, NO_POLL, "0"},
	{"rx_d/board/temp", get_rx_d_rf_board_temp, set_invalid, RO, POLL, "20"},
	{"rx_d/board/led", get_invalid, set_rx_d_rf_board_led, WO, NO_POLL, "0"},
	{"rx_d/dsp/signed", get_invalid, set_rx_d_dsp_signed, RW, NO_POLL, "1"},
	{"rx_d/dsp/freq", get_invalid, set_rx_d_dsp_freq, RW, NO_POLL, "2400"},
	{"rx_d/dsp/gain", get_invalid, set_rx_d_dsp_gain, RW, NO_POLL, "10"},
	{"rx_d/dsp/rate", get_invalid, set_rx_d_dsp_rate, RW, NO_POLL, "1258850"},
	{"rx_d/dsp/nco_adj", get_invalid, set_rx_d_dsp_nco_adj, RW, NO_POLL, "-15000000"},
	{"rx_d/dsp/iqerr_gain", get_invalid, set_rx_d_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"rx_d/dsp/iqerr_phase", get_invalid, set_rx_d_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"rx_d/dsp/rstreq", get_invalid, set_rx_d_dsp_rstreq, WO, NO_POLL, "0"},
	{"rx_d/dsp/loopback", get_invalid, set_rx_d_dsp_loopback, RW, NO_POLL, "0"},
	{"rx_d/about/id", get_invalid, set_rx_d_about_id, RW, NO_POLL, "001"},
	{"rx_d/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"rx_d/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"rx_d/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"rx_d/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"rx_d/link/vita_en", get_invalid, set_rx_d_link_vita_en, RW, NO_POLL, "0"},
	{"rx_d/link/iface", get_invalid, set_rx_d_link_iface, RW, NO_POLL, "sfpb"},
	{"rx_d/link/port", get_invalid, set_rx_d_link_port, RW, NO_POLL, "42823"},
	{"rx_d/link/ip_dest", get_invalid, set_rx_d_link_ip_dest, RW, NO_POLL, "10.10.11.10"},
	{"rx_d/link/mac_dest", get_invalid, set_rx_d_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"time/clk/pps", get_invalid, set_time_clk_pps, RW, NO_POLL, "0"},
	{"time/clk/cur_time", get_invalid, set_time_clk_cur_time, RW, NO_POLL, "0.0"},
	{"time/source/rate", get_invalid, set_time_source_rate, RW, NO_POLL, "10000000"},
	{"time/source/vco", get_invalid, set_time_source_vco, RW, NO_POLL, "internal"},
	{"time/source/sync", get_invalid, set_time_source_sync, RW, NO_POLL, "internal"},
	{"time/source/ref", get_invalid, set_time_source_ref, RW, NO_POLL, "internal"},
	{"time/board/status", get_time_board_status, set_invalid, RO, POLL, "on"},
	{"time/board/dump", get_invalid, set_time_board_dump, WO, NO_POLL, "0"},
	{"time/board/test", get_invalid, set_time_board_test, WO, NO_POLL, "0"},
	{"time/board/temp", get_time_board_temp, set_invalid, RO, POLL, "20"},
	{"time/board/led", get_invalid, set_time_board_led, WO, NO_POLL, "0"},
	{"time/about/id", get_invalid, set_time_about_id, RW, NO_POLL, "001"},
	{"time/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"time/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"time/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"time/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"fpga/board/status", get_fpga_board_status, set_invalid, RO, POLL, "on"},
	{"fpga/board/dump", get_invalid, set_fpga_board_dump, WO, NO_POLL, "0"},
	{"fpga/board/test", get_invalid, set_fpga_board_test, WO, NO_POLL, "0"},
	{"fpga/board/temp", get_fpga_board_temp, set_invalid, RO, POLL, "20"},
	{"fpga/board/led", get_invalid, set_fpga_board_led, WO, NO_POLL, "0"},
	{"fpga/board/rstreq", get_invalid, set_fpga_board_rstreq, WO, NO_POLL, "0"},
	{"fpga/board/jesd_sync", get_invalid, set_fpga_board_jesd_sync, WO, NO_POLL, "0"},
	{"fpga/about/id", get_invalid, set_fpga_about_id, RW, NO_POLL, "001"},
	{"fpga/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"fpga/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"fpga/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"fpga/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, VERSION},
	{"fpga/link/rate", get_invalid, set_fpga_link_rate, RW, NO_POLL, "1250000000"},      // BPS (10G/8)
	{"fpga/link/sfpa/ip_addr", get_invalid, set_fpga_link_sfpa_ip_addr, RW, NO_POLL, "10.10.10.2"},
	{"fpga/link/sfpa/mac_addr", get_invalid, set_fpga_link_sfpa_mac_addr, RW, NO_POLL, "aa:00:00:00:00:00"},
	{"fpga/link/sfpa/ver", get_invalid, set_fpga_link_sfpa_ver, RW, NO_POLL, "0"},
	{"fpga/link/sfpa/pay_len", get_invalid, set_fpga_link_sfpa_pay_len, RW, NO_POLL, "1400"},
	{"fpga/link/sfpb/ip_addr", get_invalid, set_fpga_link_sfpb_ip_addr, RW, NO_POLL, "10.10.11.2"},
	{"fpga/link/sfpb/mac_addr", get_invalid, set_fpga_link_sfpb_mac_addr, RW, NO_POLL, "aa:00:00:00:00:01"},
	{"fpga/link/sfpb/ver", get_invalid, set_fpga_link_sfpb_ver, RW, NO_POLL, "0"},
	{"fpga/link/sfpb/pay_len", get_invalid, set_fpga_link_sfpb_pay_len, RW, NO_POLL, "1400"},
	{"fpga/link/net/dhcp_en", get_invalid, set_fpga_link_net_dhcp_en, RW, NO_POLL, "0"},
	{"fpga/link/net/hostname", get_invalid, set_fpga_link_net_hostname, RW, NO_POLL, "crimson"},
	{"fpga/link/net/ip_addr", get_invalid, set_fpga_link_net_ip_addr, RW, NO_POLL, "192.168.10.2"},
	{"poll_en", get_invalid, set_poll_en, RW, NO_POLL, "1"}
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

inline void pass_uart_fd(int fd) {
	uart_fd = fd;
}

char* get_abs_path(prop_t* prop, char* path) {
	strcpy(path, get_home_dir());
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

	strcpy(path, get_home_dir());
	strcat(path, "/state/");

	size_t temp_len = strlen(path);
	memcpy(path + temp_len, prop -> path, len);
	path[temp_len + len] = '\0';

	return path;
}
