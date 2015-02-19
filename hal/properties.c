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

#define BASE_SAMPLE_RATE 322265625.0	// SPS
#define ADJ_SAMPLE_RATE  257812500.0	// SPS
#define IPVER_IPV4 0
#define IPVER_IPV6 1

#define PWR_ON	1
#define PWR_OFF	0

#define FREQ_XOVER_PNT 500000000	// 500 MHz is the crossover frequency for high and low band

// define this if DAC NCO is mixed to 15MHz, this removes mixing artifacts
#define DSP_NCO_OFFSET

// static global variables
static int uart_fd = 0;
static uint8_t uart_ret_buf[MAX_UART_RET_LEN] = {};
static uint16_t uart_ret_size = 0;
static char buf[MAX_PROP_LEN] = {};
static uint16_t rd_len;

// by default the board is powered off
static uint8_t rxa_power = PWR_OFF;
static uint8_t rxb_power = PWR_OFF;
static uint8_t rxc_power = PWR_OFF;
static uint8_t rxd_power = PWR_OFF;
static uint8_t txa_power = PWR_OFF;
static uint8_t txb_power = PWR_OFF;
static uint8_t txc_power = PWR_OFF;
static uint8_t txd_power = PWR_OFF;

// state variables
static uint8_t ipver[2] = {IPVER_IPV4, IPVER_IPV4};

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
	//strcpy(buf, "fwd -b 1 -m 'dac -c a -e 0 -n ");
	//strcat(buf, data);
	//strcat(buf, "'\r");
	//send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_dac_pap (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'dac -c a -p ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_dac_interp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'dac -c a -t ");
	strcat(buf, data);
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
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);

	#ifdef DSP_NCO_OFFSET
	freq -= 15000000;	// 15MHz offset
	#endif

	// write kHz to MCU cmd
	strcpy(buf, "fwd -b 1 -m 'rf -c a -f ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", freq / 1000);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// check which band it resides on
	if (freq > FREQ_XOVER_PNT) 	strcpy(buf, "fwd -b 1 -m 'rf -c a -b 1'\r");
	else				strcpy(buf, "fwd -b 1 -m 'rf -c a -b 0'\r");

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c a -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_freq_i_bias (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

static int set_tx_a_rf_freq_q_bias (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

// TODO: make sure the range (0 -> -28 is within the boundaries of requirement
static int set_tx_a_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);

	strcpy(buf, "fwd -b 1 -m 'rf -c a -v ");
	sprintf(buf + strlen(buf), "%i", (gain * -1));
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
	double rate;
	double base_rate_err, adj_rate_err;
	uint32_t old_val;
	int base_decim, adj_decim;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
	base_decim = (int)floor(BASE_SAMPLE_RATE / rate) - 1;
	base_rate_err = (BASE_SAMPLE_RATE / (double)(base_decim + 1)) - rate;

	adj_decim = (int)floor(ADJ_SAMPLE_RATE / rate) - 1;
	adj_rate_err = (ADJ_SAMPLE_RATE / (double)(adj_decim + 1)) - rate;

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && fabs(adj_rate_err) < fabs(base_rate_err)) {
		write_hps_reg( "txa1", adj_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val | 0x1000);
		sprintf(ret, "%lf", ADJ_SAMPLE_RATE/(adj_decim + 1));
	} else {
		write_hps_reg( "txa1", base_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val & ~0x1000);
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(base_decim + 1));
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
		direction = 0;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 1;
	}

	// write NCO adj
	write_hps_reg( "txa0", (freq * pow(2,32)) / ADJ_SAMPLE_RATE);

	// write direction
	read_hps_reg(  "txa4", &old_val);
	write_hps_reg( "txa4", (old_val & ~(0x1 << 3) ) | (direction << 3));
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

static int set_tx_a_link_enable (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txa4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "txa4", old_val | 0x100);
	else				write_hps_reg( "txa4", old_val & ~0x100);
	return RETURN_SUCCESS;
}

static int set_tx_a_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "res_rw0", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "res_rw0", old_val | 0x0c);
	else				write_hps_reg( "res_rw0", old_val & ~0x0c);
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
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && txa_power == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && txa_power == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		// set the board to mute
		strcpy(buf, "fwd -b 1 -m 'board -c a -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in a known state prior to putting it in demo
		// (equivalent to resetting the board)
		strcpy(buf, "fwd -b 1 -m 'board -c a -i'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in demo mode, (need to send it 2 times, errata)
		strcpy(buf, "fwd -b 1 -m 'board -c a -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// re-send JESD sync
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable the DSP cores
		read_hps_reg ( "txa4", &old_val);
		write_hps_reg( "txa4", old_val | 0x2);
		write_hps_reg( "txa4", old_val & (~0x2));
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | 0x2);
		write_hps_reg( "rxa4", old_val & (~0x2));

		// enable 10G transmission
		read_hps_reg ( "txa4", &old_val);
		write_hps_reg( "txa4", old_val | 0x100);

		txa_power = PWR_ON;

	// power off
	} else {
		// mute the board
		strcpy(buf, "fwd -b 1 -m 'board -c a -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "txa4", &old_val);
		write_hps_reg( "txa4", old_val | 0x2);
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txa4", &old_val);
		write_hps_reg( "txa4", old_val & (~0x100));

		txa_power = PWR_OFF;
	}

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_freq (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c a -f ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_bypass (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c a -b ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_gain1 (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 10)	gain = 3;
	else if (gain <= 14) 	gain = 1;
	else 			gain = 0;

	strcpy(buf, "fwd -b 0 -m 'vga -c a -g ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_gain2 (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 13)	gain = 3;
	else if (gain <= 16)	gain = 2;
	else if (gain <= 19) 	gain = 1;
	else 			gain = 0;

	strcpy(buf, "fwd -b 0 -m 'vga -c a -h ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_gain3 (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 13)	gain = 3;
	else if (gain <= 16)	gain = 2;
	else if (gain <= 19) 	gain = 1;
	else 			gain = 0;

	strcpy(buf, "fwd -b 0 -m 'vga -c a -i ");
	sprintf(buf + strlen(buf), "%i", gain);	
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_pgain (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 6)	gain = 0;
	else 			gain = 1;

	strcpy(buf, "fwd -b 0 -m 'vga -c a -p ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_atten1 (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c a -x ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_atten2 (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c a -y ");
	strcat(buf, data);
	strcat(buf, "'\r");
	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_atten3 (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c a -z ");
	strcat(buf, data);
	strcat(buf, "'\r");
	return RETURN_SUCCESS;
}

/* TODO not all frequencies are possible, read in the value and update it */
static int set_rx_a_rf_freq_val (const char* data, char* ret) {
	uint32_t freq;
	int i;
	sscanf(data, "%"SCNd32"", &freq);

	// write kHz to MCU cmd
	strcpy(buf, "fwd -b 0 -m 'rf -c a -f ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", freq / 1000);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// read back the actual frequency
	memset(uart_ret_buf, 0, MAX_UART_RET_LEN);
	recv_uart_comm(uart_fd, uart_ret_buf, &uart_ret_size, MAX_UART_RET_LEN);
	//printf("Received from UART %"PRIu16" bytes: ", uart_ret_size);
	//for (i = 0; i < uart_ret_size; i++)
	//	printf("%c", uart_ret_buf[i]);
	//printf("\n\n\n");

	// check which band it resides on
	if (freq > FREQ_XOVER_PNT) 	strcpy(buf, "fwd -b 0 -m 'rf -c a -b 1'\r");
	else				strcpy(buf, "fwd -b 0 -m 'rf -c a -b 0'\r");

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c a -l ");
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
	// TODO: intelligent gain, maximize SNR

	int gain;
	sscanf(data, "%i", &gain);

	// set the total VGA gain to 66 dB
	strcpy(buf, "fwd -b 0 -m 'vga -c a -g 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'vga -c a -h 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'vga -c a -i 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'vga -c a -p 1'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// attenuate 66 to desired amount
	if (gain < 66) {
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'vga -c a -x ");
		sprintf(buf + strlen(buf), "%i", (66-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'vga -c a -y ");
		sprintf(buf + strlen(buf), "%i", (66-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'vga -c a -z ");
		sprintf(buf + strlen(buf), "%i", (66-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	}
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

static int set_rx_a_dsp_freq (const char* data, char* ret) {
        // don't need to tune DSP chain
	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_rate (const char* data, char* ret) {
	double rate;
	double base_rate_err, adj_rate_err;
	uint32_t old_val;
	int base_decim, adj_decim;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
	base_decim = (int)floor(BASE_SAMPLE_RATE / rate) - 1;
	base_rate_err = (BASE_SAMPLE_RATE / (double)(base_decim + 1)) - rate;

	adj_decim = (int)floor(ADJ_SAMPLE_RATE / rate) - 1;
	adj_rate_err = (ADJ_SAMPLE_RATE / (double)(adj_decim + 1)) - rate;

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && fabs(adj_rate_err) < fabs(base_rate_err)) {
		write_hps_reg( "rxa1", adj_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val | 0x100);
		sprintf(ret, "%lf", ADJ_SAMPLE_RATE/(adj_decim + 1));
	} else {
		write_hps_reg( "rxa1", base_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val & ~0x100);
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(base_decim + 1));
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
		direction = 0;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 1;
	}

	// write NCO adj
	write_hps_reg( "rxa0", (freq * pow(2,32)) / ADJ_SAMPLE_RATE);

	// write direction
	read_hps_reg(  "rxa4", &old_val);
	write_hps_reg( "rxa4", (old_val & ~(0x1 << 3) ) | (direction << 3));
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

static int set_rx_a_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_rx_a_link_enable (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxa4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "rxa4", old_val | 0x100);
	else				write_hps_reg( "rxa4", old_val & ~0x100);
	return RETURN_SUCCESS;
}

static int set_rx_a_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "res_rw0", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "res_rw0", old_val | 0x01);
	else				write_hps_reg( "res_rw0", old_val & ~0x01);
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
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && rxa_power == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && rxa_power == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		// set the board to mute
		strcpy(buf, "fwd -b 0 -m 'board -c a -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in a known state prior to putting it in demo
		// (equivalent to resetting the board)
		strcpy(buf, "fwd -b 0 -m 'board -c a -i'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in demo mode, (need to send it 2 times, errata)
		strcpy(buf, "fwd -b 0 -m 'board -c a -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// re-send JESD sync
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable the DSP cores
		read_hps_reg ( "txa4", &old_val);
		write_hps_reg( "txa4", old_val | 0x2);
		write_hps_reg( "txa4", old_val & (~0x2));
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | 0x2);
		write_hps_reg( "rxa4", old_val & (~0x2));

		// enable 10G transmission
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | 0x100);

		rxa_power = PWR_ON;

	// power off
	} else {
		// mute the board
		strcpy(buf, "fwd -b 0 -m 'board -c a -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "txa4", &old_val);
		write_hps_reg( "txa4", old_val | 0x2);
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & (~0x100));

		rxa_power = PWR_OFF;
	}
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_mixer (const char* data, char* ret) {
	// TODO: Need to implement in MCU code
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_nco (const char* data, char* ret) {
	//strcpy(buf, "fwd -b 1 -m 'dac -c b -e 3 -n ");
	//strcat(buf, data);
	//strcat(buf, "'\r");
	//send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_pap (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'dac -c b -p ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_interp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'dac -c b -t ");
	strcat(buf, data);
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
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);

	#ifdef DSP_NCO_OFFSET
	freq -= 15000000;	// 15MHz offset
	#endif

	// write kHz to MCU cmd
	strcpy(buf, "fwd -b 1 -m 'rf -c b -f ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", freq / 1000);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// check which band it resides on
	if (freq > FREQ_XOVER_PNT) 	strcpy(buf, "fwd -b 1 -m 'rf -c b -b 1'\r");
	else				strcpy(buf, "fwd -b 1 -m 'rf -c b -b 0'\r");

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c b -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_freq_i_bias (const char* data, char* ret) {
	// TODO: MCU code

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_freq_q_bias (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);

	strcpy(buf, "fwd -b 1 -m 'rf -c b -v ");
	sprintf(buf + strlen(buf), "%i", (gain * -1));
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
	double rate;
	double base_rate_err, adj_rate_err;
	uint32_t old_val;
	int base_decim, adj_decim;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
	base_decim = (int)floor(BASE_SAMPLE_RATE / rate) - 1;
	base_rate_err = (BASE_SAMPLE_RATE / (double)(base_decim + 1)) - rate;

	adj_decim = (int)floor(ADJ_SAMPLE_RATE / rate) - 1;
	adj_rate_err = (ADJ_SAMPLE_RATE / (double)(adj_decim + 1)) - rate;

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && fabs(adj_rate_err) < fabs(base_rate_err)) {
		write_hps_reg( "txb1", adj_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val | 0x2000);
		sprintf(ret, "%lf", ADJ_SAMPLE_RATE/(adj_decim + 1));
	} else {
		write_hps_reg( "txb1", base_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val & ~0x2000);
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(base_decim + 1));
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
		direction = 0;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 1;
	}

	// write NCO adj
	write_hps_reg( "txb0", (freq * pow(2,32)) / ADJ_SAMPLE_RATE);

	// write direction
	read_hps_reg(  "txb4", &old_val);
	write_hps_reg( "txb4", (old_val & ~(0x1 << 3) ) | (direction << 3));
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

static int set_tx_b_link_enable (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txb4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "txb4", old_val | 0x100);
	else				write_hps_reg( "txb4", old_val & ~0x100);
	return RETURN_SUCCESS;
}

static int set_tx_b_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "res_rw0", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "res_rw0", old_val | 0xc0);
	else				write_hps_reg( "res_rw0", old_val & ~0xc0);
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
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && txb_power == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && txb_power == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		// set the board to mute
		strcpy(buf, "fwd -b 1 -m 'board -c b -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in a known state prior to putting it in demo
		// (equivalent to resetting the board)
		strcpy(buf, "fwd -b 1 -m 'board -c b -i'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in demo mode, (need to send it 2 times, errata)
		strcpy(buf, "fwd -b 1 -m 'board -c b -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// re-send JESD sync
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable the DSP cores
		read_hps_reg ( "txb4", &old_val);
		write_hps_reg( "txb4", old_val | 0x2);
		write_hps_reg( "txb4", old_val & (~0x2));
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | 0x2);
		write_hps_reg( "rxb4", old_val & (~0x2));

		// enable 10G transmission
		read_hps_reg ( "txb4", &old_val);
		write_hps_reg( "txb4", old_val | 0x100);

		txb_power = PWR_ON;

	// power off
	} else {
		// mute the board
		strcpy(buf, "fwd -b 1 -m 'board -c b -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "txb4", &old_val);
		write_hps_reg( "txb4", old_val | 0x2);
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txb4", &old_val);
		write_hps_reg( "txb4", old_val & (~0x100));

		txb_power = PWR_OFF;
	}

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_freq (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c b -f ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_bypass (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c b -b ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_gain1 (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 10)	gain = 3;
	else if (gain <= 14) 	gain = 1;
	else 			gain = 0;

	strcpy(buf, "fwd -b 0 -m 'vga -c b -g ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_gain2 (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 13)	gain = 3;
	else if (gain <= 16)	gain = 2;
	else if (gain <= 19) 	gain = 1;
	else 			gain = 0;

	strcpy(buf, "fwd -b 0 -m 'vga -c b -h ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_gain3 (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 13)	gain = 3;
	else if (gain <= 16)	gain = 2;
	else if (gain <= 19) 	gain = 1;
	else 			gain = 0;

	strcpy(buf, "fwd -b 0 -m 'vga -c b -i ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_pgain (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 6)	gain = 0;
	else 			gain = 1;

	strcpy(buf, "fwd -b 0 -m 'vga -c b -p ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_atten1 (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c b -x ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_atten2 (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c b -y ");
	strcat(buf, data);
	strcat(buf, "'\r");
	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_atten3 (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c b -z ");
	strcat(buf, data);
	strcat(buf, "'\r");
	return RETURN_SUCCESS;
}

/* TODO not all frequencies are possible, read in the value and update it */
static int set_rx_b_rf_freq_val (const char* data, char* ret) {
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);

	// write kHz to MCU cmd
	strcpy(buf, "fwd -b 0 -m 'rf -c b -f ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", freq / 1000);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// check which band it resides on
	if (freq > FREQ_XOVER_PNT) 	strcpy(buf, "fwd -b 0 -m 'rf -c b -b 1'\r");
	else				strcpy(buf, "fwd -b 0 -m 'rf -c b -b 0'\r");

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c b -l ");
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
	// TODO: intelligent gain, maximize SNR

	int gain;
	sscanf(data, "%i", &gain);

	// set the total VGA gain to 66 dB
	strcpy(buf, "fwd -b 0 -m 'vga -c b -g 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'vga -c b -h 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'vga -c b -i 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'vga -c b -p 1'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// attenuate 66 to desired amount
	if (gain < 66) {
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'vga -c b -x ");
		sprintf(buf + strlen(buf), "%i", (66-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'vga -c b -y ");
		sprintf(buf + strlen(buf), "%i", (66-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'vga -c b -z ");
		sprintf(buf + strlen(buf), "%i", (66-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	}

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

static int set_rx_b_dsp_freq (const char* data, char* ret) {
        // don't need to tune DSP chain
	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_rate (const char* data, char* ret) {
	double rate;
	double base_rate_err, adj_rate_err;
	uint32_t old_val;
	int base_decim, adj_decim;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
	base_decim = (int)floor(BASE_SAMPLE_RATE / rate) - 1;
	base_rate_err = (BASE_SAMPLE_RATE / (double)(base_decim + 1)) - rate;

	adj_decim = (int)floor(ADJ_SAMPLE_RATE / rate) - 1;
	adj_rate_err = (ADJ_SAMPLE_RATE / (double)(adj_decim + 1)) - rate;

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && fabs(adj_rate_err) < fabs(base_rate_err)) {
		write_hps_reg( "rxb1", adj_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val | 0x200);
		sprintf(ret, "%lf", ADJ_SAMPLE_RATE/(adj_decim + 1));
	} else {
		write_hps_reg( "rxb1", base_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val & ~0x200);
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(base_decim + 1));
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
		direction = 0;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 1;
	}

	// write NCO adj
	write_hps_reg( "rxb0", (freq * pow(2,32)) / ADJ_SAMPLE_RATE);

	// write direction
	read_hps_reg(  "rxb4", &old_val);
	write_hps_reg( "rxb4", (old_val & ~(0x1 << 3) ) | (direction << 3));
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

static int set_rx_b_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_rx_b_link_enable (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxb4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "rxb4", old_val | 0x100);
	else				write_hps_reg( "rxb4", old_val & ~0x100);
	return RETURN_SUCCESS;
}

static int set_rx_b_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "res_rw0", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "res_rw0", old_val | 0x10);
	else				write_hps_reg( "res_rw0", old_val & ~0x10);
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
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && rxb_power == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && rxb_power == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		// set the board to mute
		strcpy(buf, "fwd -b 0 -m 'board -c b -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in a known state prior to putting it in demo
		// (equivalent to resetting the board)
		strcpy(buf, "fwd -b 0 -m 'board -c b -i'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in demo mode, (need to send it 2 times, errata)
		strcpy(buf, "fwd -b 0 -m 'board -c b -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// re-send JESD sync
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable the DSP cores
		read_hps_reg ( "txb4", &old_val);
		write_hps_reg( "txb4", old_val | 0x2);
		write_hps_reg( "txb4", old_val & (~0x2));
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | 0x2);
		write_hps_reg( "rxb4", old_val & (~0x2));

		// enable 10G transmission
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | 0x100);

		rxb_power = PWR_ON;

	// power off
	} else {
		// mute the board
		strcpy(buf, "fwd -b 0 -m 'board -c b -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "txb4", &old_val);
		write_hps_reg( "txb4", old_val | 0x2);
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & (~0x100));

		rxb_power = PWR_OFF;
	}
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_mixer (const char* data, char* ret) {
	// TODO: Need to implement in MCU code
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_nco (const char* data, char* ret) {
	//strcpy(buf, "fwd -b 1 -m 'dac -c c -e 0 -n ");
	//strcat(buf, data);
	//strcat(buf, "'\r");
	//send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_pap (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'dac -c c -p ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_interp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'dac -c c -t ");
	strcat(buf, data);
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
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);

	#ifdef DSP_NCO_OFFSET
	freq -= 15000000;	// 15MHz offset
	#endif

	// write kHz to MCU cmd
	strcpy(buf, "fwd -b 1 -m 'rf -c c -f ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", freq / 1000);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// check which band it resides on
	if (freq > FREQ_XOVER_PNT) 	strcpy(buf, "fwd -b 1 -m 'rf -c c -b 1'\r");
	else				strcpy(buf, "fwd -b 1 -m 'rf -c c -b 0'\r");

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c c -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_freq_i_bias (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_freq_q_bias (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);

	strcpy(buf, "fwd -b 1 -m 'rf -c c -v ");
	sprintf(buf + strlen(buf), "%i", (gain * -1));
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
	double rate;
	double base_rate_err, adj_rate_err;
	uint32_t old_val;
	int base_decim, adj_decim;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
	base_decim = (int)floor(BASE_SAMPLE_RATE / rate) - 1;
	base_rate_err = (BASE_SAMPLE_RATE / (double)(base_decim + 1)) - rate;

	adj_decim = (int)floor(ADJ_SAMPLE_RATE / rate) - 1;
	adj_rate_err = (ADJ_SAMPLE_RATE / (double)(adj_decim + 1)) - rate;

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && fabs(adj_rate_err) < fabs(base_rate_err)) {
		write_hps_reg( "txc1", adj_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val | 0x4000);
		sprintf(ret, "%lf", ADJ_SAMPLE_RATE/(adj_decim + 1));
	} else {
		write_hps_reg( "txc1", base_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val & ~0x4000);
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(base_decim + 1));
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
		direction = 0;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 1;
	}

	// write NCO adj
	write_hps_reg( "txc0", (freq * pow(2,32)) / ADJ_SAMPLE_RATE);

	// write direction
	read_hps_reg(  "txc4", &old_val);
	write_hps_reg( "txc4", (old_val & ~(0x1 << 3) ) | (direction << 3));
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

static int set_tx_c_link_enable (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txc4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "txc4", old_val | 0x100);
	else				write_hps_reg( "txc4", old_val & ~0x100);
	return RETURN_SUCCESS;
}

static int set_tx_c_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "res_rw0", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "res_rw0", old_val | 0x0c);
	else				write_hps_reg( "res_rw0", old_val & ~0x0c);
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
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && txc_power == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && txc_power == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		// set the board to mute
		strcpy(buf, "fwd -b 1 -m 'board -c c -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in a known state prior to putting it in demo
		// (equivalent to resetting the board)
		strcpy(buf, "fwd -b 1 -m 'board -c c -i'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in demo mode, (need to send it 2 times, errata)
		strcpy(buf, "fwd -b 1 -m 'board -c c -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// re-send JESD sync
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable the DSP cores
		read_hps_reg ( "txc4", &old_val);
		write_hps_reg( "txc4", old_val | 0x2);
		write_hps_reg( "txc4", old_val & (~0x2));
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | 0x2);
		write_hps_reg( "rxc4", old_val & (~0x2));

		// enable 10G transmission
		read_hps_reg ( "txc4", &old_val);
		write_hps_reg( "txc4", old_val | 0x100);

		txc_power = PWR_ON;

	// power off
	} else {
		// mute the board
		strcpy(buf, "fwd -b 1 -m 'board -c c -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "txc4", &old_val);
		write_hps_reg( "txc4", old_val | 0x2);
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txc4", &old_val);
		write_hps_reg( "txc4", old_val & (~0x100));

		txc_power = PWR_OFF;
	}

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_freq (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c c -f ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_bypass (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c c -b ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_gain1 (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 10)	gain = 3;
	else if (gain <= 14) 	gain = 1;
	else 			gain = 0;

	strcpy(buf, "fwd -b 0 -m 'vga -c c -g ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_gain2 (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 13)	gain = 3;
	else if (gain <= 16)	gain = 2;
	else if (gain <= 19) 	gain = 1;
	else 			gain = 0;

	strcpy(buf, "fwd -b 0 -m 'vga -c c -h ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_gain3 (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 13)	gain = 3;
	else if (gain <= 16)	gain = 2;
	else if (gain <= 19) 	gain = 1;
	else 			gain = 0;

	strcpy(buf, "fwd -b 0 -m 'vga -c c -i ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_pgain (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 6)	gain = 0;
	else 			gain = 1;

	strcpy(buf, "fwd -b 0 -m 'vga -c c -p ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_atten1 (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c c -x ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_atten2 (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c c -y ");
	strcat(buf, data);
	strcat(buf, "'\r");
	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_atten3 (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c c -z ");
	strcat(buf, data);
	strcat(buf, "'\r");
	return RETURN_SUCCESS;
}

/* TODO not all frequencies are possible, read in the value and update it */
static int set_rx_c_rf_freq_val (const char* data, char* ret) {
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);

	// write kHz to MCU cmd
	strcpy(buf, "fwd -b 0 -m 'rf -c c -f ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", freq / 1000);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// check which band it resides on
	if (freq > FREQ_XOVER_PNT) 	strcpy(buf, "fwd -b 0 -m 'rf -c c -b 1'\r");
	else				strcpy(buf, "fwd -b 0 -m 'rf -c c -b 0'\r");

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c c -l ");
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
	// TODO: intelligent gain, maximize SNR

	int gain;
	sscanf(data, "%i", &gain);

	// set the total VGA gain to 66 dB
	strcpy(buf, "fwd -b 0 -m 'vga -c c -g 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'vga -c c -h 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'vga -c c -i 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'vga -c c -p 1'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// attenuate 66 to desired amount
	if (gain < 66) {
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'vga -c c -x ");
		sprintf(buf + strlen(buf), "%i", (66-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'vga -c c -y ");
		sprintf(buf + strlen(buf), "%i", (66-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'vga -c c -z ");
		sprintf(buf + strlen(buf), "%i", (66-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	}
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

static int set_rx_c_dsp_freq (const char* data, char* ret) {
        // don't need to tune DSP chain
	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_rate (const char* data, char* ret) {
	double rate;
	double base_rate_err, adj_rate_err;
	uint32_t old_val;
	int base_decim, adj_decim;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
	base_decim = (int)floor(BASE_SAMPLE_RATE / rate) - 1;
	base_rate_err = (BASE_SAMPLE_RATE / (double)(base_decim + 1)) - rate;

	adj_decim = (int)floor(ADJ_SAMPLE_RATE / rate) - 1;
	adj_rate_err = (ADJ_SAMPLE_RATE / (double)(adj_decim + 1)) - rate;

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && fabs(adj_rate_err) < fabs(base_rate_err)) {
		write_hps_reg( "rxc1", adj_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val | 0x400);
		sprintf(ret, "%lf", ADJ_SAMPLE_RATE/(adj_decim + 1));
	} else {
		write_hps_reg( "rxc1", base_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val & ~0x400);
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(base_decim + 1));
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
		direction = 0;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 1;
	}

	// write NCO adj
	write_hps_reg( "rxc0", (freq * pow(2,32)) / ADJ_SAMPLE_RATE);

	// write direction
	read_hps_reg(  "rxc4", &old_val);
	write_hps_reg( "rxc4", (old_val & ~(0x1 << 3) ) | (direction << 3));
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

static int set_rx_c_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_rx_c_link_enable (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxc4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "rxc4", old_val | 0x100);
	else				write_hps_reg( "rxc4", old_val & ~0x100);
	return RETURN_SUCCESS;
}

static int set_rx_c_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "res_rw0", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "res_rw0", old_val | 0x02);
	else				write_hps_reg( "res_rw0", old_val & ~0x02);
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
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && rxc_power == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && rxc_power == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		// set the board to mute
		strcpy(buf, "fwd -b 0 -m 'board -c c -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in a known state prior to putting it in demo
		// (equivalent to resetting the board)
		strcpy(buf, "fwd -b 0 -m 'board -c c -i'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in demo mode, (need to send it 2 times, errata)
		strcpy(buf, "fwd -b 0 -m 'board -c c -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// re-send JESD sync
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable the DSP cores
		read_hps_reg ( "txc4", &old_val);
		write_hps_reg( "txc4", old_val | 0x2);
		write_hps_reg( "txc4", old_val & (~0x2));
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | 0x2);
		write_hps_reg( "rxc4", old_val & (~0x2));

		// enable 10G transmission
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | 0x100);

		rxc_power = PWR_ON;

	// power off
	} else {
		// mute the board
		strcpy(buf, "fwd -b 0 -m 'board -c c -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "txc4", &old_val);
		write_hps_reg( "txc4", old_val | 0x2);
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & (~0x100));

		rxc_power = PWR_OFF;
	}
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_mixer (const char* data, char* ret) {
	// TODO: Need to implement in MCU code
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_nco (const char* data, char* ret) {
	//strcpy(buf, "fwd -b 1 -m 'dac -c d -e 3 -n ");
	//strcat(buf, data);
	//strcat(buf, "'\r");
	//send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_pap (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'dac -c d -p ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_interp (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'dac -c d -t ");
	strcat(buf, data);
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
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);

	#ifdef DSP_NCO_OFFSET
	freq -= 15000000;	// 15MHz offset
	#endif

	// write kHz to MCU cmd
	strcpy(buf, "fwd -b 1 -m 'rf -c d -f ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", freq / 1000);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// check which band it resides on
	if (freq > FREQ_XOVER_PNT) 	strcpy(buf, "fwd -b 1 -m 'rf -c d -b 1'\r");
	else				strcpy(buf, "fwd -b 1 -m 'rf -c d -b 0'\r");

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c d -l ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_freq_i_bias (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_freq_q_bias (const char* data, char* ret) {
	// TODO: MCU code
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_gain_val (const char* data, char* ret) {
	int gain;
	sscanf(data, "%i", &gain);

	strcpy(buf, "fwd -b 1 -m 'rf -c d -v ");
	sprintf(buf + strlen(buf), "%i", (gain * -1));
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
	double rate;
	double base_rate_err, adj_rate_err;
	uint32_t old_val;
	int base_decim, adj_decim;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
	base_decim = (int)floor(BASE_SAMPLE_RATE / rate) - 1;
	base_rate_err = (BASE_SAMPLE_RATE / (double)(base_decim + 1)) - rate;

	adj_decim = (int)floor(ADJ_SAMPLE_RATE / rate) - 1;
	adj_rate_err = (ADJ_SAMPLE_RATE / (double)(adj_decim + 1)) - rate;

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && fabs(adj_rate_err) < fabs(base_rate_err)) {
		write_hps_reg( "txd1", adj_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val | 0x8000);
		sprintf(ret, "%lf", ADJ_SAMPLE_RATE/(adj_decim + 1));
	} else {
		write_hps_reg( "txd1", base_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val & ~0x8000);
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(base_decim + 1));
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
		direction = 0;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 1;
	}

	// write NCO adj
	write_hps_reg( "txd0", (freq * pow(2,32)) / ADJ_SAMPLE_RATE);

	// write direction
	read_hps_reg(  "txd4", &old_val);
	write_hps_reg( "txd4", (old_val & ~(0x1 << 3) ) | (direction << 3));
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

static int set_tx_d_link_enable (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "txd4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "txd4", old_val | 0x100);
	else				write_hps_reg( "txd4", old_val & ~0x100);
	return RETURN_SUCCESS;
}

static int set_tx_d_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "res_rw0", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "res_rw0", old_val | 0xc0);
	else				write_hps_reg( "res_rw0", old_val & ~0xc0);
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
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && txd_power == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && txd_power == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		// set the board to mute
		strcpy(buf, "fwd -b 1 -m 'board -c d -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in a known state prior to putting it in demo
		// (equivalent to resetting the board)
		strcpy(buf, "fwd -b 1 -m 'board -c d -i'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in demo mode, (need to send it 2 times, errata)
		strcpy(buf, "fwd -b 1 -m 'board -c d -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// re-send JESD sync
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable the DSP cores
		read_hps_reg ( "txd4", &old_val);
		write_hps_reg( "txd4", old_val | 0x2);
		write_hps_reg( "txd4", old_val & (~0x2));
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | 0x2);
		write_hps_reg( "rxd4", old_val & (~0x2));

		// enable 10G transmission
		read_hps_reg ( "txd4", &old_val);
		write_hps_reg( "txd4", old_val | 0x100);

		txd_power = PWR_ON;

	// power off
	} else {
		// mute the board
		strcpy(buf, "fwd -b 1 -m 'board -c d -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "txd4", &old_val);
		write_hps_reg( "txd4", old_val | 0x2);
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "txd4", &old_val);
		write_hps_reg( "txd4", old_val & (~0x100));

		txd_power = PWR_OFF;
	}
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_freq (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c d -f ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_bypass (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c d -b ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_gain1 (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 10)	gain = 3;
	else if (gain <= 14) 	gain = 1;
	else 			gain = 0;

	strcpy(buf, "fwd -b 0 -m 'vga -c d -g ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_gain2 (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 13)	gain = 3;
	else if (gain <= 16)	gain = 2;
	else if (gain <= 19) 	gain = 1;
	else 			gain = 0;

	strcpy(buf, "fwd -b 0 -m 'vga -c d -h ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_gain3 (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 13)	gain = 3;
	else if (gain <= 16)	gain = 2;
	else if (gain <= 19) 	gain = 1;
	else 			gain = 0;

	strcpy(buf, "fwd -b 0 -m 'vga -c d -i ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_pgain (const char* data, char* ret) {
	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);
	if 	(gain <= 6)	gain = 0;
	else 			gain = 1;

	strcpy(buf, "fwd -b 0 -m 'vga -c d -p ");
	sprintf(buf + strlen(buf), "%i", gain);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_atten1 (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c d -x ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_atten2 (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c d -y ");
	strcat(buf, data);
	strcat(buf, "'\r");
	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_atten3 (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'vga -c d -z ");
	strcat(buf, data);
	strcat(buf, "'\r");
	return RETURN_SUCCESS;
}

/* TODO not all frequencies are possible, read in the value and update it */
static int set_rx_d_rf_freq_val (const char* data, char* ret) {
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);

	// write kHz to MCU cmd
	strcpy(buf, "fwd -b 0 -m 'rf -c d -f ");
	sprintf(buf + strlen(buf), "%" PRIu32 "", freq / 1000);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// check which band it resides on
	if (freq > FREQ_XOVER_PNT) 	strcpy(buf, "fwd -b 0 -m 'rf -c d -b 1'\r");
	else				strcpy(buf, "fwd -b 0 -m 'rf -c d -b 0'\r");

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_freq_lna (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c d -l ");
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
	// TODO: intelligent gain, maximize SNR

	int gain;
	sscanf(data, "%i", &gain);

	// set the total VGA gain to 66 dB
	strcpy(buf, "fwd -b 0 -m 'vga -c d -g 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'vga -c d -h 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'vga -c d -i 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'vga -c d -p 1'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// attenuate 66 to desired amount
	if (gain < 66) {
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'vga -c d -x ");
		sprintf(buf + strlen(buf), "%i", (66-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'vga -c d -y ");
		sprintf(buf + strlen(buf), "%i", (66-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'vga -c d -z ");
		sprintf(buf + strlen(buf), "%i", (66-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	}
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

static int set_rx_d_dsp_freq (const char* data, char* ret) {
        // don't need to tune DSP chain
	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_gain (const char* data, char* ret) {
	// TODO: FW code
	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_rate (const char* data, char* ret) {
	double rate;
	double base_rate_err, adj_rate_err;
	uint32_t old_val;
	int base_decim, adj_decim;
	sscanf(data, "%lf", &rate);

	// get the error for base and adj rates
	base_decim = (int)floor(BASE_SAMPLE_RATE / rate) - 1;
	base_rate_err = (BASE_SAMPLE_RATE / (double)(base_decim + 1)) - rate;

	adj_decim = (int)floor(ADJ_SAMPLE_RATE / rate) - 1;
	adj_rate_err = (ADJ_SAMPLE_RATE / (double)(adj_decim + 1)) - rate;

	// set the appropriate sample rate depending on which one is closer
	memset(ret, 0, MAX_PROP_LEN);
	if (rate < (BASE_SAMPLE_RATE / 5) && fabs(adj_rate_err) < fabs(base_rate_err)) {
		write_hps_reg( "rxd1", adj_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val | 0x800);
		sprintf(ret, "%lf", ADJ_SAMPLE_RATE/(adj_decim + 1));
	} else {
		write_hps_reg( "rxd1", base_decim);
		read_hps_reg(  "res_rw0", &old_val);
		write_hps_reg( "res_rw0", old_val & ~0x800);
		sprintf(ret, "%lf", BASE_SAMPLE_RATE/(base_decim + 1));
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
		direction = 0;
	} else if (data[0] == '+') {
		sscanf(data + 1, "%"SCNd32"", &freq);
		direction = 1;
	} else {
		sscanf(data, "%"SCNd32"", &freq);
		direction = 1;
	}

	// write NCO adj
	write_hps_reg( "rxd0", (freq * pow(2,32)) / ADJ_SAMPLE_RATE);

	// write direction
	read_hps_reg(  "rxd4", &old_val);
	write_hps_reg( "rxd4", (old_val & ~(0x1 << 3) ) | (direction << 3));
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

static int set_rx_d_about_id (const char* data, char* ret) {
	// don't need to do anything, save the ID in the file system
	return RETURN_SUCCESS;
}

static int set_rx_d_link_enable (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "rxd4", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "rxd4", old_val | 0x100);
	else				write_hps_reg( "rxd4", old_val & ~0x100);
	return RETURN_SUCCESS;
}

static int set_rx_d_link_vita_en (const char* data, char* ret) {
	uint32_t old_val;
	read_hps_reg(  "res_rw0", &old_val);
	if (strcmp(data, "1") == 0)	write_hps_reg( "res_rw0", old_val | 0x20);
	else				write_hps_reg( "res_rw0", old_val & ~0x20);
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
	sscanf(data, "%"SCNd8"", &power);

	// check it power is already enabled
	if (power >= PWR_ON  && rxd_power == PWR_ON)  return RETURN_SUCCESS;
	if (power == PWR_OFF && rxd_power == PWR_OFF) return RETURN_SUCCESS;

	// power on
	if (power >= PWR_ON) {
		// set the board to mute
		strcpy(buf, "fwd -b 0 -m 'board -c d -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in a known state prior to putting it in demo
		// (equivalent to resetting the board)
		strcpy(buf, "fwd -b 0 -m 'board -c d -i'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// put the board in demo mode, (need to send it 2 times, errata)
		strcpy(buf, "fwd -b 0 -m 'board -c d -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// re-send JESD sync
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable the DSP cores
		read_hps_reg ( "txd4", &old_val);
		write_hps_reg( "txd4", old_val | 0x2);
		write_hps_reg( "txd4", old_val & (~0x2));
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | 0x2);
		write_hps_reg( "rxd4", old_val & (~0x2));

		// enable 10G transmission
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | 0x100);

		rxd_power = PWR_ON;

	// power off
	} else {
		// mute the board
		strcpy(buf, "fwd -b 0 -m 'board -c d -m'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// diable the DSP cores
		read_hps_reg ( "txd4", &old_val);
		write_hps_reg( "txd4", old_val | 0x2);
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | 0x2);

		// disable 10G transmission
		read_hps_reg ( "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & (~0x100));

		rxd_power = PWR_OFF;
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
	//read_hps_reg( "res_ro0", &intpart);
	//read_hps_reg( "res_ro1", &fracpart);
	//printf("Time is: %lf seconds\n", (double)intpart + ((double)fracpart / 100000000) );

	double time;
	sscanf(data, "%lf", &time);
	write_hps_reg( "res_rw1", (uint32_t)time);
	write_hps_reg( "res_rw2", time - (uint32_t)time);

	// toggle the set register
	write_hps_reg( "res_rw3", 1);
	write_hps_reg( "res_rw3", 0);
	return RETURN_SUCCESS;
}

static int set_time_source_rate (const char* data, char* ret) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_source_vco (const char* data, char* ret) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_source_sync (const char* data, char* ret) {
	// Insert MCU/MEM command

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

static int set_fpga_board_jesd_rstreq (const char* data, char* ret) {
	// TODO: MCU code cleanup
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

static int set_fpga_link_loopback (const char* data, char* ret) {
	uint32_t enable;
	sscanf(data, "%"SCNd32"", &enable);
	if (enable)	write_hps_reg( "sys0", 1);
	else		write_hps_reg( "sys0", 0);
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

static int set_poll_en (const char* data, char* ret) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

// Beginning of property table
static prop_t property_table[] = {
	{"tx_a/pwr", get_invalid, set_tx_a_pwr, RW, NO_POLL, "1"},
	{"tx_a/rf/dac/mixer", get_invalid, set_tx_a_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_a/rf/dac/nco", get_invalid, set_tx_a_rf_dac_nco, RW, NO_POLL, "15"},
	{"tx_a/rf/dac/pap", get_invalid, set_tx_a_rf_dac_pap, RW, NO_POLL, "0"},
	{"tx_a/rf/dac/interp", get_invalid, set_tx_a_rf_dac_interp, RW, NO_POLL, "0"},
	{"tx_a/rf/dac/temp", get_tx_a_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_a/rf/dac/iqerr_gain", get_invalid, set_tx_a_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_a/rf/dac/iqerr_phase", get_invalid, set_tx_a_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_a/rf/freq/val", get_invalid, set_tx_a_rf_freq_val, RW, NO_POLL, "945200000"},
	{"tx_a/rf/freq/lna", get_invalid, set_tx_a_rf_freq_lna, RW, NO_POLL, "0"},
	{"tx_a/rf/freq/i_bias", get_invalid, set_tx_a_rf_freq_i_bias, RW, NO_POLL, "0"},
	{"tx_a/rf/freq/q_bias", get_invalid, set_tx_a_rf_freq_q_bias, RW, NO_POLL, "0"},
	{"tx_a/rf/gain/val", get_invalid, set_tx_a_rf_gain_val, RW, NO_POLL, "-1"},
	{"tx_a/rf/board/status", get_tx_a_rf_board_status, set_invalid, RO, POLL, "off"},
	{"tx_a/rf/board/dump", get_invalid, set_tx_a_rf_board_dump, WO, NO_POLL, "0"},
	{"tx_a/rf/board/test", get_invalid, set_tx_a_rf_board_test, WO, NO_POLL, "0"},
	{"tx_a/rf/board/temp", get_tx_a_rf_board_temp, set_invalid, RO, POLL, "23"},
	{"tx_a/rf/board/led", get_invalid, set_tx_a_rf_board_led, WO, NO_POLL, "0"},
	{"tx_a/dsp/freq", get_invalid, set_tx_a_dsp_freq, RW, NO_POLL, "2400"},
	{"tx_a/dsp/gain", get_invalid, set_tx_a_dsp_gain, RW, NO_POLL, "10"},
	{"tx_a/dsp/rate", get_invalid, set_tx_a_dsp_rate, RW, NO_POLL, "1258850"},
	{"tx_a/dsp/nco_adj", get_invalid, set_tx_a_dsp_nco_adj, RW, NO_POLL, "0"},
	{"tx_a/dsp/iqerr_gain", get_invalid, set_tx_a_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_a/dsp/iqerr_phase", get_invalid, set_tx_a_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_a/dsp/rstreq", get_invalid, set_tx_a_dsp_rstreq, WO, NO_POLL, "0"},
	{"tx_a/about/id", get_invalid, set_tx_a_about_id, RW, NO_POLL, "001"},
	{"tx_a/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"tx_a/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_a/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_a/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_a/link/enable", get_invalid, set_tx_a_link_enable, RW, NO_POLL, "1"},
	{"tx_a/link/vita_en", get_invalid, set_tx_a_link_vita_en, RW, NO_POLL, "0"},
	{"tx_a/link/iface", get_invalid, set_tx_a_link_iface, RW, NO_POLL, "sfpa"},
	{"tx_a/link/port", get_invalid, set_tx_a_link_port, RW, NO_POLL, "42824"},
	{"rx_a/pwr", get_invalid, set_rx_a_pwr, RW, NO_POLL, "1"},
	{"rx_a/rf/vga/freq", get_invalid, set_rx_a_rf_vga_freq, RW, NO_POLL, "0"},
	{"rx_a/rf/vga/bypass", get_invalid, set_rx_a_rf_vga_bypass, RW, NO_POLL, "1"},
	{"rx_a/rf/vga/gain1", get_invalid, set_rx_a_rf_vga_gain1, RW, NO_POLL, "15"},
	{"rx_a/rf/vga/gain2", get_invalid, set_rx_a_rf_vga_gain2, RW, NO_POLL, "21"},
	{"rx_a/rf/vga/gain3", get_invalid, set_rx_a_rf_vga_gain3, RW, NO_POLL, "21"},
	{"rx_a/rf/vga/pgain", get_invalid, set_rx_a_rf_vga_pgain, RW, NO_POLL, "9"},
	{"rx_a/rf/vga/atten1", get_invalid, set_rx_a_rf_vga_atten1, RW, NO_POLL, "10"},
	{"rx_a/rf/vga/atten2", get_invalid, set_rx_a_rf_vga_atten2, RW, NO_POLL, "10"},
	{"rx_a/rf/vga/atten3", get_invalid, set_rx_a_rf_vga_atten3, RW, NO_POLL, "10"},
	{"rx_a/rf/freq/val", get_invalid, set_rx_a_rf_freq_val, RW, NO_POLL, "900200000"},
	{"rx_a/rf/freq/lna", get_invalid, set_rx_a_rf_freq_lna, RW, NO_POLL, "0"},
	{"rx_a/rf/freq/varac", get_invalid, set_rx_a_rf_freq_varac, RW, NO_POLL, "0"},
	{"rx_a/rf/gain/val", get_invalid, set_rx_a_rf_gain_val, RW, NO_POLL, "35"},
	{"rx_a/rf/board/status", get_rx_a_rf_board_status, set_invalid, RO, POLL, "off"},
	{"rx_a/rf/board/dump", get_invalid, set_rx_a_rf_board_dump, WO, NO_POLL, "0"},
	{"rx_a/rf/board/test", get_invalid, set_rx_a_rf_board_test, WO, NO_POLL, "0"},
	{"rx_a/rf/board/temp", get_rx_a_rf_board_temp, set_invalid, RO, POLL, "20"},
	{"rx_a/rf/board/led", get_invalid, set_rx_a_rf_board_led, WO, NO_POLL, "0"},
	{"rx_a/dsp/freq", get_invalid, set_rx_a_dsp_freq, RW, NO_POLL, "2400"},
	{"rx_a/dsp/gain", get_invalid, set_rx_a_dsp_gain, RW, NO_POLL, "10"},
	{"rx_a/dsp/rate", get_invalid, set_rx_a_dsp_rate, RW, NO_POLL, "1258850"},
	{"rx_a/dsp/nco_adj", get_invalid, set_rx_a_dsp_nco_adj, RW, NO_POLL, "0"},
	{"rx_a/dsp/iqerr_gain", get_invalid, set_rx_a_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"rx_a/dsp/iqerr_phase", get_invalid, set_rx_a_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"rx_a/dsp/rstreq", get_invalid, set_rx_a_dsp_rstreq, WO, NO_POLL, "0"},
	{"rx_a/about/id", get_invalid, set_rx_a_about_id, RW, NO_POLL, "001"},
	{"rx_a/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"rx_a/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_a/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_a/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_a/link/enable", get_invalid, set_rx_a_link_enable, RW, NO_POLL, "1"},
	{"rx_a/link/vita_en", get_invalid, set_rx_a_link_vita_en, RW, NO_POLL, "1"},
	{"rx_a/link/iface", get_invalid, set_rx_a_link_iface, RW, NO_POLL, "sfpa"},
	{"rx_a/link/port", get_invalid, set_rx_a_link_port, RW, NO_POLL, "42820"},
	{"rx_a/link/ip_dest", get_invalid, set_rx_a_link_ip_dest, RW, NO_POLL, "10.10.10.10"},
	{"rx_a/link/mac_dest", get_invalid, set_rx_a_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"tx_b/pwr", get_invalid, set_tx_b_pwr, RW, NO_POLL, "1"},
	{"tx_b/rf/dac/mixer", get_invalid, set_tx_b_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/nco", get_invalid, set_tx_b_rf_dac_nco, RW, NO_POLL, "15"},
	{"tx_b/rf/dac/pap", get_invalid, set_tx_b_rf_dac_pap, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/interp", get_invalid, set_tx_b_rf_dac_interp, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/temp", get_tx_b_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_b/rf/dac/iqerr_gain", get_invalid, set_tx_b_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/iqerr_phase", get_invalid, set_tx_b_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_b/rf/freq/val", get_invalid, set_tx_b_rf_freq_val, RW, NO_POLL, "945200000"},
	{"tx_b/rf/freq/lna", get_invalid, set_tx_b_rf_freq_lna, RW, NO_POLL, "0"},
	{"tx_b/rf/freq/i_bias", get_invalid, set_tx_b_rf_freq_i_bias, RW, NO_POLL, "0"},
	{"tx_b/rf/freq/q_bias", get_invalid, set_tx_b_rf_freq_q_bias, RW, NO_POLL, "0"},
	{"tx_b/rf/gain/val", get_invalid, set_tx_b_rf_gain_val, RW, NO_POLL, "-1"},
	{"tx_b/rf/board/status", get_tx_b_rf_board_status, set_invalid, RO, POLL, "off"},
	{"tx_b/rf/board/dump", get_invalid, set_tx_b_rf_board_dump, WO, NO_POLL, "0"},
	{"tx_b/rf/board/test", get_invalid, set_tx_b_rf_board_test, WO, NO_POLL, "0"},
	{"tx_b/rf/board/temp", get_tx_b_rf_board_temp, set_invalid, RO, POLL, "23"},
	{"tx_b/rf/board/led", get_invalid, set_tx_b_rf_board_led, WO, NO_POLL, "0"},
	{"tx_b/dsp/freq", get_invalid, set_tx_b_dsp_freq, RW, NO_POLL, "2400"},
	{"tx_b/dsp/gain", get_invalid, set_tx_b_dsp_gain, RW, NO_POLL, "10"},
	{"tx_b/dsp/rate", get_invalid, set_tx_b_dsp_rate, RW, NO_POLL, "1258850"},
	{"tx_b/dsp/nco_adj", get_invalid, set_tx_b_dsp_nco_adj, RW, NO_POLL, "0"},
	{"tx_b/dsp/iqerr_gain", get_invalid, set_tx_b_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_b/dsp/iqerr_phase", get_invalid, set_tx_b_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_b/dsp/rstreq", get_invalid, set_tx_b_dsp_rstreq, WO, NO_POLL, "0"},
	{"tx_b/about/id", get_invalid, set_tx_b_about_id, RW, NO_POLL, "001"},
	{"tx_b/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"tx_b/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_b/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_b/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_b/link/enable", get_invalid, set_tx_b_link_enable, RW, NO_POLL, "1"},
	{"tx_b/link/vita_en", get_invalid, set_tx_b_link_vita_en, RW, NO_POLL, "0"},
	{"tx_b/link/iface", get_invalid, set_tx_b_link_iface, RW, NO_POLL, "sfpb"},
	{"tx_b/link/port", get_invalid, set_tx_b_link_port, RW, NO_POLL, "42825"},
	{"rx_b/pwr", get_invalid, set_rx_b_pwr, RW, NO_POLL, "1"},
	{"rx_b/rf/vga/freq", get_invalid, set_rx_b_rf_vga_freq, RW, NO_POLL, "0"},
	{"rx_b/rf/vga/bypass", get_invalid, set_rx_b_rf_vga_bypass, RW, NO_POLL, "1"},
	{"rx_b/rf/vga/gain1", get_invalid, set_rx_b_rf_vga_gain1, RW, NO_POLL, "15"},
	{"rx_b/rf/vga/gain2", get_invalid, set_rx_b_rf_vga_gain2, RW, NO_POLL, "21"},
	{"rx_b/rf/vga/gain3", get_invalid, set_rx_b_rf_vga_gain3, RW, NO_POLL, "21"},
	{"rx_b/rf/vga/pgain", get_invalid, set_rx_b_rf_vga_pgain, RW, NO_POLL, "9"},
	{"rx_b/rf/vga/atten1", get_invalid, set_rx_b_rf_vga_atten1, RW, NO_POLL, "10"},
	{"rx_b/rf/vga/atten2", get_invalid, set_rx_b_rf_vga_atten2, RW, NO_POLL, "10"},
	{"rx_b/rf/vga/atten3", get_invalid, set_rx_b_rf_vga_atten3, RW, NO_POLL, "10"},
	{"rx_b/rf/freq/val", get_invalid, set_rx_b_rf_freq_val, RW, NO_POLL, "900200000"},
	{"rx_b/rf/freq/lna", get_invalid, set_rx_b_rf_freq_lna, RW, NO_POLL, "0"},
	{"rx_b/rf/freq/varac", get_invalid, set_rx_b_rf_freq_varac, RW, NO_POLL, "0"},
	{"rx_b/rf/gain/val", get_invalid, set_rx_b_rf_gain_val, RW, NO_POLL, "35"},
	{"rx_b/rf/board/status", get_rx_b_rf_board_status, set_invalid, RO, POLL, "off"},
	{"rx_b/rf/board/dump", get_invalid, set_rx_b_rf_board_dump, WO, NO_POLL, "0"},
	{"rx_b/rf/board/test", get_invalid, set_rx_b_rf_board_test, WO, NO_POLL, "0"},
	{"rx_b/rf/board/temp", get_rx_b_rf_board_temp, set_invalid, RO, POLL, "20"},
	{"rx_b/rf/board/led", get_invalid, set_rx_b_rf_board_led, WO, NO_POLL, "0"},
	{"rx_b/dsp/freq", get_invalid, set_rx_b_dsp_freq, RW, NO_POLL, "2400"},
	{"rx_b/dsp/gain", get_invalid, set_rx_b_dsp_gain, RW, NO_POLL, "10"},
	{"rx_b/dsp/rate", get_invalid, set_rx_b_dsp_rate, RW, NO_POLL, "1258850"},
	{"rx_b/dsp/nco_adj", get_invalid, set_rx_b_dsp_nco_adj, RW, NO_POLL, "0"},
	{"rx_b/dsp/iqerr_gain", get_invalid, set_rx_b_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"rx_b/dsp/iqerr_phase", get_invalid, set_rx_b_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"rx_b/dsp/rstreq", get_invalid, set_rx_b_dsp_rstreq, WO, NO_POLL, "0"},
	{"rx_b/about/id", get_invalid, set_rx_b_about_id, RW, NO_POLL, "001"},
	{"rx_b/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"rx_b/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_b/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_b/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_b/link/enable", get_invalid, set_rx_b_link_enable, RW, NO_POLL, "1"},
	{"rx_b/link/vita_en", get_invalid, set_rx_b_link_vita_en, RW, NO_POLL, "1"},
	{"rx_b/link/iface", get_invalid, set_rx_b_link_iface, RW, NO_POLL, "sfpb"},
	{"rx_b/link/port", get_invalid, set_rx_b_link_port, RW, NO_POLL, "42821"},
	{"rx_b/link/ip_dest", get_invalid, set_rx_b_link_ip_dest, RW, NO_POLL, "10.10.11.10"},
	{"rx_b/link/mac_dest", get_invalid, set_rx_b_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"tx_c/pwr", get_invalid, set_tx_c_pwr, RW, NO_POLL, "1"},
	{"tx_c/rf/dac/mixer", get_invalid, set_tx_c_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/nco", get_invalid, set_tx_c_rf_dac_nco, RW, NO_POLL, "15"},
	{"tx_c/rf/dac/pap", get_invalid, set_tx_c_rf_dac_pap, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/interp", get_invalid, set_tx_c_rf_dac_interp, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/temp", get_tx_c_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_c/rf/dac/iqerr_gain", get_invalid, set_tx_c_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/iqerr_phase", get_invalid, set_tx_c_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_c/rf/freq/val", get_invalid, set_tx_c_rf_freq_val, RW, NO_POLL, "945200000"},
	{"tx_c/rf/freq/lna", get_invalid, set_tx_c_rf_freq_lna, RW, NO_POLL, "0"},
	{"tx_c/rf/freq/i_bias", get_invalid, set_tx_c_rf_freq_i_bias, RW, NO_POLL, "0"},
	{"tx_c/rf/freq/q_bias", get_invalid, set_tx_c_rf_freq_q_bias, RW, NO_POLL, "0"},
	{"tx_c/rf/gain/val", get_invalid, set_tx_c_rf_gain_val, RW, NO_POLL, "-1"},
	{"tx_c/rf/board/status", get_tx_c_rf_board_status, set_invalid, RO, POLL, "off"},
	{"tx_c/rf/board/dump", get_invalid, set_tx_c_rf_board_dump, WO, NO_POLL, "0"},
	{"tx_c/rf/board/test", get_invalid, set_tx_c_rf_board_test, WO, NO_POLL, "0"},
	{"tx_c/rf/board/temp", get_tx_c_rf_board_temp, set_invalid, RO, POLL, "23"},
	{"tx_c/rf/board/led", get_invalid, set_tx_c_rf_board_led, WO, NO_POLL, "0"},
	{"tx_c/dsp/freq", get_invalid, set_tx_c_dsp_freq, RW, NO_POLL, "2400"},
	{"tx_c/dsp/gain", get_invalid, set_tx_c_dsp_gain, RW, NO_POLL, "10"},
	{"tx_c/dsp/rate", get_invalid, set_tx_c_dsp_rate, RW, NO_POLL, "1258850"},
	{"tx_c/dsp/nco_adj", get_invalid, set_tx_c_dsp_nco_adj, RW, NO_POLL, "0"},
	{"tx_c/dsp/iqerr_gain", get_invalid, set_tx_c_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_c/dsp/iqerr_phase", get_invalid, set_tx_c_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_c/dsp/rstreq", get_invalid, set_tx_c_dsp_rstreq, WO, NO_POLL, "0"},
	{"tx_c/about/id", get_invalid, set_tx_c_about_id, RW, NO_POLL, "001"},
	{"tx_c/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"tx_c/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_c/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_c/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_c/link/enable", get_invalid, set_tx_c_link_enable, RW, NO_POLL, "1"},
	{"tx_c/link/vita_en", get_invalid, set_tx_c_link_vita_en, RW, NO_POLL, "0"},
	{"tx_c/link/iface", get_invalid, set_tx_c_link_iface, RW, NO_POLL, "sfpa"},
	{"tx_c/link/port", get_invalid, set_tx_c_link_port, RW, NO_POLL, "42826"},
	{"rx_c/pwr", get_invalid, set_rx_c_pwr, RW, NO_POLL, "1"},
	{"rx_c/rf/vga/freq", get_invalid, set_rx_c_rf_vga_freq, RW, NO_POLL, "0"},
	{"rx_c/rf/vga/bypass", get_invalid, set_rx_c_rf_vga_bypass, RW, NO_POLL, "1"},
	{"rx_c/rf/vga/gain1", get_invalid, set_rx_c_rf_vga_gain1, RW, NO_POLL, "15"},
	{"rx_c/rf/vga/gain2", get_invalid, set_rx_c_rf_vga_gain2, RW, NO_POLL, "21"},
	{"rx_c/rf/vga/gain3", get_invalid, set_rx_c_rf_vga_gain3, RW, NO_POLL, "21"},
	{"rx_c/rf/vga/pgain", get_invalid, set_rx_c_rf_vga_pgain, RW, NO_POLL, "9"},
	{"rx_c/rf/vga/atten1", get_invalid, set_rx_c_rf_vga_atten1, RW, NO_POLL, "10"},
	{"rx_c/rf/vga/atten2", get_invalid, set_rx_c_rf_vga_atten2, RW, NO_POLL, "10"},
	{"rx_c/rf/vga/atten3", get_invalid, set_rx_c_rf_vga_atten3, RW, NO_POLL, "10"},
	{"rx_c/rf/freq/val", get_invalid, set_rx_c_rf_freq_val, RW, NO_POLL, "900200000"},
	{"rx_c/rf/freq/lna", get_invalid, set_rx_c_rf_freq_lna, RW, NO_POLL, "0"},
	{"rx_c/rf/freq/varac", get_invalid, set_rx_c_rf_freq_varac, RW, NO_POLL, "0"},
	{"rx_c/rf/gain/val", get_invalid, set_rx_c_rf_gain_val, RW, NO_POLL, "35"},
	{"rx_c/rf/board/status", get_rx_c_rf_board_status, set_invalid, RO, POLL, "off"},
	{"rx_c/rf/board/dump", get_invalid, set_rx_c_rf_board_dump, WO, NO_POLL, "0"},
	{"rx_c/rf/board/test", get_invalid, set_rx_c_rf_board_test, WO, NO_POLL, "0"},
	{"rx_c/rf/board/temp", get_rx_c_rf_board_temp, set_invalid, RO, POLL, "20"},
	{"rx_c/rf/board/led", get_invalid, set_rx_c_rf_board_led, WO, NO_POLL, "0"},
	{"rx_c/dsp/freq", get_invalid, set_rx_c_dsp_freq, RW, NO_POLL, "2400"},
	{"rx_c/dsp/gain", get_invalid, set_rx_c_dsp_gain, RW, NO_POLL, "10"},
	{"rx_c/dsp/rate", get_invalid, set_rx_c_dsp_rate, RW, NO_POLL, "1258850"},
	{"rx_c/dsp/nco_adj", get_invalid, set_rx_c_dsp_nco_adj, RW, NO_POLL, "0"},
	{"rx_c/dsp/iqerr_gain", get_invalid, set_rx_c_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"rx_c/dsp/iqerr_phase", get_invalid, set_rx_c_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"rx_c/dsp/rstreq", get_invalid, set_rx_c_dsp_rstreq, WO, NO_POLL, "0"},
	{"rx_c/about/id", get_invalid, set_rx_c_about_id, RW, NO_POLL, "001"},
	{"rx_c/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"rx_c/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_c/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_c/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_c/link/enable", get_invalid, set_rx_c_link_enable, RW, NO_POLL, "1"},
	{"rx_c/link/vita_en", get_invalid, set_rx_c_link_vita_en, RW, NO_POLL, "1"},
	{"rx_c/link/iface", get_invalid, set_rx_c_link_iface, RW, NO_POLL, "sfpa"},
	{"rx_c/link/port", get_invalid, set_rx_c_link_port, RW, NO_POLL, "42822"},
	{"rx_c/link/ip_dest", get_invalid, set_rx_c_link_ip_dest, RW, NO_POLL, "10.10.10.10"},
	{"rx_c/link/mac_dest", get_invalid, set_rx_c_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"tx_d/pwr", get_invalid, set_tx_d_pwr, RW, NO_POLL, "1"},
	{"tx_d/rf/dac/mixer", get_invalid, set_tx_d_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/nco", get_invalid, set_tx_d_rf_dac_nco, RW, NO_POLL, "15"},
	{"tx_d/rf/dac/pap", get_invalid, set_tx_d_rf_dac_pap, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/interp", get_invalid, set_tx_d_rf_dac_interp, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/temp", get_tx_d_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_d/rf/dac/iqerr_gain", get_invalid, set_tx_d_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/iqerr_phase", get_invalid, set_tx_d_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_d/rf/freq/val", get_invalid, set_tx_d_rf_freq_val, RW, NO_POLL, "945200000"},
	{"tx_d/rf/freq/lna", get_invalid, set_tx_d_rf_freq_lna, RW, NO_POLL, "0"},
	{"tx_d/rf/freq/i_bias", get_invalid, set_tx_d_rf_freq_i_bias, RW, NO_POLL, "0"},
	{"tx_d/rf/freq/q_bias", get_invalid, set_tx_d_rf_freq_q_bias, RW, NO_POLL, "0"},
	{"tx_d/rf/gain/val", get_invalid, set_tx_d_rf_gain_val, RW, NO_POLL, "-1"},
	{"tx_d/rf/board/status", get_tx_d_rf_board_status, set_invalid, RO, POLL, "off"},
	{"tx_d/rf/board/dump", get_invalid, set_tx_d_rf_board_dump, WO, NO_POLL, "0"},
	{"tx_d/rf/board/test", get_invalid, set_tx_d_rf_board_test, WO, NO_POLL, "0"},
	{"tx_d/rf/board/temp", get_tx_d_rf_board_temp, set_invalid, RO, POLL, "23"},
	{"tx_d/rf/board/led", get_invalid, set_tx_d_rf_board_led, WO, NO_POLL, "0"},
	{"tx_d/dsp/freq", get_invalid, set_tx_d_dsp_freq, RW, NO_POLL, "2400"},
	{"tx_d/dsp/gain", get_invalid, set_tx_d_dsp_gain, RW, NO_POLL, "10"},
	{"tx_d/dsp/rate", get_invalid, set_tx_d_dsp_rate, RW, NO_POLL, "1258850"},
	{"tx_d/dsp/nco_adj", get_invalid, set_tx_d_dsp_nco_adj, RW, NO_POLL, "0"},
	{"tx_d/dsp/iqerr_gain", get_invalid, set_tx_d_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_d/dsp/iqerr_phase", get_invalid, set_tx_d_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_d/dsp/rstreq", get_invalid, set_tx_d_dsp_rstreq, WO, NO_POLL, "0"},
	{"tx_d/about/id", get_invalid, set_tx_d_about_id, RW, NO_POLL, "001"},
	{"tx_d/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"tx_d/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_d/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_d/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_d/link/enable", get_invalid, set_tx_d_link_enable, RW, NO_POLL, "1"},
	{"tx_d/link/vita_en", get_invalid, set_tx_d_link_vita_en, RW, NO_POLL, "0"},
	{"tx_d/link/iface", get_invalid, set_tx_d_link_iface, RW, NO_POLL, "sfpb"},
	{"tx_d/link/port", get_invalid, set_tx_d_link_port, RW, NO_POLL, "42827"},
	{"rx_d/pwr", get_invalid, set_rx_d_pwr, RW, NO_POLL, "1"},
	{"rx_d/rf/vga/freq", get_invalid, set_rx_d_rf_vga_freq, RW, NO_POLL, "0"},
	{"rx_d/rf/vga/bypass", get_invalid, set_rx_d_rf_vga_bypass, RW, NO_POLL, "1"},
	{"rx_d/rf/vga/gain1", get_invalid, set_rx_d_rf_vga_gain1, RW, NO_POLL, "15"},
	{"rx_d/rf/vga/gain2", get_invalid, set_rx_d_rf_vga_gain2, RW, NO_POLL, "21"},
	{"rx_d/rf/vga/gain3", get_invalid, set_rx_d_rf_vga_gain3, RW, NO_POLL, "21"},
	{"rx_d/rf/vga/pgain", get_invalid, set_rx_d_rf_vga_pgain, RW, NO_POLL, "9"},
	{"rx_d/rf/vga/atten1", get_invalid, set_rx_d_rf_vga_atten1, RW, NO_POLL, "10"},
	{"rx_d/rf/vga/atten2", get_invalid, set_rx_d_rf_vga_atten2, RW, NO_POLL, "10"},
	{"rx_d/rf/vga/atten3", get_invalid, set_rx_d_rf_vga_atten3, RW, NO_POLL, "10"},
	{"rx_d/rf/freq/val", get_invalid, set_rx_d_rf_freq_val, RW, NO_POLL, "900200000"},
	{"rx_d/rf/freq/lna", get_invalid, set_rx_d_rf_freq_lna, RW, NO_POLL, "0"},
	{"rx_d/rf/freq/varac", get_invalid, set_rx_d_rf_freq_varac, RW, NO_POLL, "0"},
	{"rx_d/rf/gain/val", get_invalid, set_rx_d_rf_gain_val, RW, NO_POLL, "35"},
	{"rx_d/rf/board/status", get_rx_d_rf_board_status, set_invalid, RO, POLL, "off"},
	{"rx_d/rf/board/dump", get_invalid, set_rx_d_rf_board_dump, WO, NO_POLL, "0"},
	{"rx_d/rf/board/test", get_invalid, set_rx_d_rf_board_test, WO, NO_POLL, "0"},
	{"rx_d/rf/board/temp", get_rx_d_rf_board_temp, set_invalid, RO, POLL, "20"},
	{"rx_d/rf/board/led", get_invalid, set_rx_d_rf_board_led, WO, NO_POLL, "0"},
	{"rx_d/dsp/freq", get_invalid, set_rx_d_dsp_freq, RW, NO_POLL, "2400"},
	{"rx_d/dsp/gain", get_invalid, set_rx_d_dsp_gain, RW, NO_POLL, "10"},
	{"rx_d/dsp/rate", get_invalid, set_rx_d_dsp_rate, RW, NO_POLL, "1258850"},
	{"rx_d/dsp/nco_adj", get_invalid, set_rx_d_dsp_nco_adj, RW, NO_POLL, "0"},
	{"rx_d/dsp/iqerr_gain", get_invalid, set_rx_d_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"rx_d/dsp/iqerr_phase", get_invalid, set_rx_d_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"rx_d/dsp/rstreq", get_invalid, set_rx_d_dsp_rstreq, WO, NO_POLL, "0"},
	{"rx_d/about/id", get_invalid, set_rx_d_about_id, RW, NO_POLL, "001"},
	{"rx_d/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"rx_d/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_d/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_d/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_d/link/enable", get_invalid, set_rx_d_link_enable, RW, NO_POLL, "1"},
	{"rx_d/link/vita_en", get_invalid, set_rx_d_link_vita_en, RW, NO_POLL, "1"},
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
	{"time/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"time/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"time/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"fpga/board/status", get_fpga_board_status, set_invalid, RO, POLL, "on"},
	{"fpga/board/dump", get_invalid, set_fpga_board_dump, WO, NO_POLL, "0"},
	{"fpga/board/test", get_invalid, set_fpga_board_test, WO, NO_POLL, "0"},
	{"fpga/board/temp", get_fpga_board_temp, set_invalid, RO, POLL, "20"},
	{"fpga/board/led", get_invalid, set_fpga_board_led, WO, NO_POLL, "0"},
	{"fpga/board/rstreq", get_invalid, set_fpga_board_rstreq, WO, NO_POLL, "0"},
	{"fpga/board/jesd_rstreq", get_invalid, set_fpga_board_jesd_rstreq, WO, NO_POLL, "0"},
	{"fpga/about/id", get_invalid, set_fpga_about_id, RW, NO_POLL, "001"},
	{"fpga/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"fpga/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"fpga/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"fpga/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"fpga/link/rate", get_invalid, set_fpga_link_rate, RW, NO_POLL, "161"},
	{"fpga/link/loopback", get_invalid, set_fpga_link_loopback, RW, NO_POLL, "0"},
	{"fpga/link/sfpa/ip_addr", get_invalid, set_fpga_link_sfpa_ip_addr, RW, NO_POLL, "10.10.10.2"},
	{"fpga/link/sfpa/mac_addr", get_invalid, set_fpga_link_sfpa_mac_addr, RW, NO_POLL, "aa:00:00:00:00:00"},
	{"fpga/link/sfpa/ver", get_invalid, set_fpga_link_sfpa_ver, RW, NO_POLL, "0"},
	{"fpga/link/sfpa/pay_len", get_invalid, set_fpga_link_sfpa_pay_len, RW, NO_POLL, "1400"},
	{"fpga/link/sfpb/ip_addr", get_invalid, set_fpga_link_sfpb_ip_addr, RW, NO_POLL, "10.10.11.2"},
	{"fpga/link/sfpb/mac_addr", get_invalid, set_fpga_link_sfpb_mac_addr, RW, NO_POLL, "aa:00:00:00:00:01"},
	{"fpga/link/sfpb/ver", get_invalid, set_fpga_link_sfpb_ver, RW, NO_POLL, "0"},
	{"fpga/link/sfpb/pay_len", get_invalid, set_fpga_link_sfpb_pay_len, RW, NO_POLL, "1400"},
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
