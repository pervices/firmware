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

#define BASE_SAMPLE_RATE 322.265625	// MHz
#define BASE_SAMPLE_FREQ 322265625	// Hz
#define IPVER_IPV4 0
#define IPVER_IPV6 1

// static global variables
static int uart_fd = 0;
static char buf[MAX_PROP_LEN] = {};
static uint16_t rd_len;

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
	strcpy(buf, "fwd -b 1 -m 'dac -c a -e 0 -n ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
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
	strcpy(buf, "fwd -b 1 -m 'board -c a -t'");
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

static int set_tx_a_rf_freq_val (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c a -f ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
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
	strcpy(buf, "fwd -b 1 -m 'rf -c a -v ");
	strcat(buf, data);
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
	strcpy(buf, "fwd -b 1 -m 'board -c a -t'");
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
	sscanf(data, "%lf", &rate);
	write_hps_reg( "txa1", (int)floor(BASE_SAMPLE_RATE / rate) );
	memset(ret, 0, MAX_PROP_LEN);
	sprintf(ret, "%lf", BASE_SAMPLE_RATE/((int)floor(BASE_SAMPLE_RATE / rate)) );
	return RETURN_SUCCESS;
}

// TODO need to support up/down convert
static int set_tx_a_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);
	write_hps_reg( "txa0", (freq * pow(2,32)) / BASE_SAMPLE_FREQ);
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

static int set_tx_a_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_tx_a_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "net13", port);
	return RETURN_SUCCESS;
}

static int set_tx_a_link_pay_len (const char* data, char* ret) {
	// TODO: FW support required
	return RETURN_SUCCESS;
}

static int set_tx_a_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
	sscanf(data, "%"SCNd8"", &power);

	// ENABLE
	if (power > 0) {
		// set current tx board to demo mode
		strcpy(buf, "fwd -b 1 -m 'board -c a -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// reset DSP FPGA registers for current channel
		write_hps_reg( "txa0", 0x0);
		write_hps_reg( "txa1", 0xff);
		write_hps_reg( "txa4", 0x0);

	// DISABLE
	} else {
		// set all ADC's to demo mode
		strcpy(buf, "fwd -b 0 -m 'power -c 15 -a 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// disable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & ~(1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & ~(1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & ~(1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & ~(1<<8));

		// disable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val & ~(1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val & ~(1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val & ~(1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val & ~(1<<8));

		// set current TX to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 1 -m 'board -c a -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// set all DACs to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 1 -m 'power -c 15 -d 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// send the sync pulse from the LMK
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | (1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | (1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | (1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | (1<<8));

		// enable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val | (1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val | (1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val | (1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val | (1<<8));
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

static int set_rx_a_rf_freq_val (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c a -f ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
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

	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);

	// set the total VGA gain to 42 dB
	strcpy(buf, "fwd -b 0 -m 'rf -c a -g 3'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'rf -c a -h 2'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'rf -c a -i 2'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'rf -c a -p 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// attenuate 42 to desired amount
	if (gain < 42) {
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'rf -c a -x ");
		sprintf(buf + strlen(buf), "%i", (42-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'rf -c a -y ");
		sprintf(buf + strlen(buf), "%i", (42-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'rf -c a -z ");
		sprintf(buf + strlen(buf), "%i", (42-gain)/3);
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
	strcpy(buf, "fwd -b 0 -m 'board -c a -t'");
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
	sscanf(data, "%lf", &rate);
	write_hps_reg( "rxa1", (int)floor(BASE_SAMPLE_RATE / rate) );
	memset(ret, 0, MAX_PROP_LEN);
	sprintf(ret, "%lf", BASE_SAMPLE_RATE/((int)floor(BASE_SAMPLE_RATE / rate)) );
	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);
	write_hps_reg( "rxa0", (freq * pow(2,32)) / BASE_SAMPLE_FREQ);
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

static int set_rx_a_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_rx_a_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "net14", port);
	return RETURN_SUCCESS;
}

static int set_rx_a_link_ip_dest (const char* data, char* ret) {
	uint8_t ip[4];
	sscanf(data, "%"SCNd8".%"SCNd8".%"SCNd8".%"SCNd8"", ip, ip+1, ip+2, ip+3);
	write_hps_reg( "rxa5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	return RETURN_SUCCESS;
}

static int set_rx_a_link_mac_dest (const char* data, char* ret) {
	// TODO: FW and register addition required
	return RETURN_SUCCESS;
}

static int set_rx_a_link_pay_len (const char* data, char* ret) {
	// TODO: FW support required
	return RETURN_SUCCESS;
}

static int set_rx_a_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
	sscanf(data, "%"SCNd8"", &power);

	// ENABLE
	if (power > 0) {
		// set current rx board to demo mode
		strcpy(buf, "fwd -b 0 -m 'board -c a -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// reset DSP FPGA registers for current channel
		write_hps_reg( "rxa0", 0x0);
		write_hps_reg( "rxa1", 0xff);
		write_hps_reg( "rxa4", 0x0);

	// DISABLE
	} else {
		// set current rx board to demo mode
		strcpy(buf, "fwd -b 0 -m 'board -c a -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// set all ADC's to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'power -c 15 -a 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// disable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & ~(1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & ~(1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & ~(1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & ~(1<<8));

		// disable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val & ~(1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val & ~(1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val & ~(1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val & ~(1<<8));

		// set all DACs to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 1 -m 'power -c 15 -d 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// send the sync pulse from the LMK
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | (1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | (1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | (1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | (1<<8));

		// enable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val | (1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val | (1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val | (1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val | (1<<8));
	}
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_mixer (const char* data, char* ret) {
	// TODO: Need to implement in MCU code
	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_nco (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'dac -c b -e 1 -n ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
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
	strcpy(buf, "fwd -b 1 -m 'board -c b -t'");
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

static int set_tx_b_rf_freq_val (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c b -f ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
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
	strcpy(buf, "fwd -b 1 -m 'rf -c b -v ");
	strcat(buf, data);
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
	strcpy(buf, "fwd -b 1 -m 'board -c b -t'");
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
	sscanf(data, "%lf", &rate);
	write_hps_reg( "txb1", (int)floor(BASE_SAMPLE_RATE / rate) );
	memset(ret, 0, MAX_PROP_LEN);
	sprintf(ret, "%lf", BASE_SAMPLE_RATE/((int)floor(BASE_SAMPLE_RATE / rate)) );
	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);
	write_hps_reg( "txb0", (freq * pow(2,32)) / BASE_SAMPLE_FREQ);
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

static int set_tx_b_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_tx_b_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "net28", port);
	return RETURN_SUCCESS;
}

static int set_tx_b_link_pay_len (const char* data, char* ret) {
	// TODO: FW support required
	return RETURN_SUCCESS;
}

static int set_tx_b_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
	sscanf(data, "%"SCNd8"", &power);

	// ENABLE
	if (power > 0) {
		// set current tx board to demo mode
		strcpy(buf, "fwd -b 1 -m 'board -c b -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// reset DSP FPGA registers for current channel
		write_hps_reg( "txa0", 0x0);
		write_hps_reg( "txa1", 0xff);
		write_hps_reg( "txa4", 0x0);

	// DISABLE
	} else {
		// set all ADC's to demo mode
		strcpy(buf, "fwd -b 0 -m 'power -c 15 -a 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// disable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & ~(1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & ~(1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & ~(1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & ~(1<<8));

		// disable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val & ~(1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val & ~(1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val & ~(1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val & ~(1<<8));

		// set current TX to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 1 -m 'board -c b -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// set all DACs to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 1 -m 'power -c 15 -d 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// send the sync pulse from the LMK
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | (1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | (1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | (1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | (1<<8));

		// enable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val | (1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val | (1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val | (1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val | (1<<8));
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

static int set_rx_b_rf_freq_val (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c b -f ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
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

	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);

	// set the total VGA gain to 42 dB
	strcpy(buf, "fwd -b 0 -m 'rf -c b -g 3'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'rf -c b -h 2'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'rf -c b -i 2'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'rf -c b -p 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// attenuate 42 to desired amount
	if (gain < 42) {
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'rf -c b -x ");
		sprintf(buf + strlen(buf), "%i", (42-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'rf -c b -y ");
		sprintf(buf + strlen(buf), "%i", (42-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'rf -c b -z ");
		sprintf(buf + strlen(buf), "%i", (42-gain)/3);
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
	strcpy(buf, "fwd -b 0 -m 'board -c b -t'");
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
	sscanf(data, "%lf", &rate);
	write_hps_reg( "rxb1", (int)floor(BASE_SAMPLE_RATE / rate) );
	memset(ret, 0, MAX_PROP_LEN);
	sprintf(ret, "%lf", BASE_SAMPLE_RATE/((int)floor(BASE_SAMPLE_RATE / rate)) );
	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);
	write_hps_reg( "rxb0", (freq * pow(2,32)) / BASE_SAMPLE_FREQ);
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

static int set_rx_b_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_rx_b_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "net29", port);
	return RETURN_SUCCESS;
}

static int set_rx_b_link_ip_dest (const char* data, char* ret) {
	uint8_t ip[4];
	sscanf(data, "%"SCNd8".%"SCNd8".%"SCNd8".%"SCNd8"", ip, ip+1, ip+2, ip+3);
	write_hps_reg( "rxb5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	return RETURN_SUCCESS;
}

static int set_rx_b_link_mac_dest (const char* data, char* ret) {
	// TODO: FW and register addition required
	return RETURN_SUCCESS;
}

static int set_rx_b_link_pay_len (const char* data, char* ret) {
	// TODO: FW support required
	return RETURN_SUCCESS;
}

static int set_rx_b_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
	sscanf(data, "%"SCNd8"", &power);

	// ENABLE
	if (power > 0) {
		// set current rx board to demo mode
		strcpy(buf, "fwd -b 0 -m 'board -c b -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// reset DSP FPGA registers for current channel
		write_hps_reg( "rxa0", 0x0);
		write_hps_reg( "rxa1", 0xff);
		write_hps_reg( "rxa4", 0x0);

	// DISABLE
	} else {
		// set current rx board to demo mode
		strcpy(buf, "fwd -b 0 -m 'board -c b -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// set all ADC's to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'power -c 15 -a 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// disable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & ~(1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & ~(1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & ~(1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & ~(1<<8));

		// disable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val & ~(1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val & ~(1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val & ~(1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val & ~(1<<8));

		// set all DACs to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 1 -m 'power -c 15 -d 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// send the sync pulse from the LMK
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | (1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | (1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | (1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | (1<<8));

		// enable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val | (1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val | (1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val | (1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val | (1<<8));
	}
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_mixer (const char* data, char* ret) {
	// TODO: Need to implement in MCU code
	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_nco (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'dac -c c -e 2 -n ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
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
	strcpy(buf, "fwd -b 1 -m 'board -c c -t'");
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

static int set_tx_c_rf_freq_val (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c c -f ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
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
	strcpy(buf, "fwd -b 1 -m 'rf -c c -v ");
	strcat(buf, data);
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
	strcpy(buf, "fwd -b 1 -m 'board -c c -t'");
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
	sscanf(data, "%lf", &rate);
	write_hps_reg( "txc1", (int)floor(BASE_SAMPLE_RATE / rate) );
	memset(ret, 0, MAX_PROP_LEN);
	sprintf(ret, "%lf", BASE_SAMPLE_RATE/((int)floor(BASE_SAMPLE_RATE / rate)) );
	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);
	write_hps_reg( "txc0", (freq * pow(2,32)) / BASE_SAMPLE_FREQ);
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

static int set_tx_c_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_tx_c_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "net13", port);
	return RETURN_SUCCESS;
}

static int set_tx_c_link_pay_len (const char* data, char* ret) {
	// TODO: FW support required
	return RETURN_SUCCESS;
}

static int set_tx_c_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
	sscanf(data, "%"SCNd8"", &power);

	// ENABLE
	if (power > 0) {
		// set current tx board to demo mode
		strcpy(buf, "fwd -b 1 -m 'board -c c -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// reset DSP FPGA registers for current channel
		write_hps_reg( "txa0", 0x0);
		write_hps_reg( "txa1", 0xff);
		write_hps_reg( "txa4", 0x0);

	// DISABLE
	} else {
		// set all ADC's to demo mode
		strcpy(buf, "fwd -b 0 -m 'power -c 15 -a 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// disable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & ~(1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & ~(1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & ~(1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & ~(1<<8));

		// disable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val & ~(1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val & ~(1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val & ~(1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val & ~(1<<8));

		// set current TX to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 1 -m 'board -c c -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// set all DACs to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 1 -m 'power -c 15 -d 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// send the sync pulse from the LMK
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | (1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | (1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | (1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | (1<<8));

		// enable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val | (1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val | (1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val | (1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val | (1<<8));
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

static int set_rx_c_rf_freq_val (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c c -f ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
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

	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);

	// set the total VGA gain to 42 dB
	strcpy(buf, "fwd -b 0 -m 'rf -c c -g 3'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'rf -c c -h 2'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'rf -c c -i 2'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'rf -c c -p 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// attenuate 42 to desired amount
	if (gain < 42) {
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'rf -c c -x ");
		sprintf(buf + strlen(buf), "%i", (42-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'rf -c c -y ");
		sprintf(buf + strlen(buf), "%i", (42-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'rf -c c -z ");
		sprintf(buf + strlen(buf), "%i", (42-gain)/3);
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
	strcpy(buf, "fwd -b 0 -m 'board -c c -t'");
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
	sscanf(data, "%lf", &rate);
	write_hps_reg( "rxc1", (int)floor(BASE_SAMPLE_RATE / rate) );
	memset(ret, 0, MAX_PROP_LEN);
	sprintf(ret, "%lf", BASE_SAMPLE_RATE/((int)floor(BASE_SAMPLE_RATE / rate)) );
	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);
	write_hps_reg( "rxc0", (freq * pow(2,32)) / BASE_SAMPLE_FREQ);
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

static int set_rx_c_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_rx_c_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "net14", port);
	return RETURN_SUCCESS;
}

static int set_rx_c_link_ip_dest (const char* data, char* ret) {
	uint8_t ip[4];
	sscanf(data, "%"SCNd8".%"SCNd8".%"SCNd8".%"SCNd8"", ip, ip+1, ip+2, ip+3);
	write_hps_reg( "rxc5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	return RETURN_SUCCESS;
}

static int set_rx_c_link_mac_dest (const char* data, char* ret) {
	// TODO: FW and register addition required
	return RETURN_SUCCESS;
}

static int set_rx_c_link_pay_len (const char* data, char* ret) {
	// TODO: FW support required
	return RETURN_SUCCESS;
}

static int set_rx_c_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
	sscanf(data, "%"SCNd8"", &power);

	// ENABLE
	if (power > 0) {
		// set current rx board to demo mode
		strcpy(buf, "fwd -b 0 -m 'board -c c -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// reset DSP FPGA registers for current channel
		write_hps_reg( "rxa0", 0x0);
		write_hps_reg( "rxa1", 0xff);
		write_hps_reg( "rxa4", 0x0);

	// DISABLE
	} else {
		// set current rx board to demo mode
		strcpy(buf, "fwd -b 0 -m 'board -c c -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// set all ADC's to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'power -c 15 -a 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// disable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & ~(1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & ~(1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & ~(1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & ~(1<<8));

		// disable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val & ~(1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val & ~(1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val & ~(1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val & ~(1<<8));

		// set all DACs to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 1 -m 'power -c 15 -d 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// send the sync pulse from the LMK
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | (1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | (1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | (1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | (1<<8));

		// enable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val | (1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val | (1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val | (1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val | (1<<8));
	}
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_mixer (const char* data, char* ret) {
	// TODO: Need to implement in MCU code
	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_nco (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'dac -c d -e 3 -n ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
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
	strcpy(buf, "fwd -b 1 -m 'board -c d -t'");
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

static int set_tx_d_rf_freq_val (const char* data, char* ret) {
	strcpy(buf, "fwd -b 1 -m 'rf -c d -f ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
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
	strcpy(buf, "fwd -b 1 -m 'rf -c d -v ");
	strcat(buf, data);
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
	strcpy(buf, "fwd -b 1 -m 'board -c d -t'");
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
	sscanf(data, "%lf", &rate);
	write_hps_reg( "txd1", (int)floor(BASE_SAMPLE_RATE / rate) );
	memset(ret, 0, MAX_PROP_LEN);
	sprintf(ret, "%lf", BASE_SAMPLE_RATE/((int)floor(BASE_SAMPLE_RATE / rate)) );
	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);
	write_hps_reg( "txd0", (freq * pow(2,32)) / BASE_SAMPLE_FREQ);
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

static int set_tx_d_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_tx_d_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "net28", port);
	return RETURN_SUCCESS;
}

static int set_tx_d_link_pay_len (const char* data, char* ret) {
	// TODO: FW support required
	return RETURN_SUCCESS;
}

static int set_tx_d_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
	sscanf(data, "%"SCNd8"", &power);

	// ENABLE
	if (power > 0) {
		// set current tx board to demo mode
		strcpy(buf, "fwd -b 1 -m 'board -c d -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// reset DSP FPGA registers for current channel
		write_hps_reg( "txa0", 0x0);
		write_hps_reg( "txa1", 0xff);
		write_hps_reg( "txa4", 0x0);

	// DISABLE
	} else {
		// set all ADC's to demo mode
		strcpy(buf, "fwd -b 0 -m 'power -c 15 -a 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// disable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & ~(1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & ~(1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & ~(1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & ~(1<<8));

		// disable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val & ~(1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val & ~(1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val & ~(1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val & ~(1<<8));

		// set current TX to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 1 -m 'board -c d -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// set all DACs to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 1 -m 'power -c 15 -d 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// send the sync pulse from the LMK
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | (1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | (1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | (1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | (1<<8));

		// enable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val | (1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val | (1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val | (1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val | (1<<8));
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

static int set_rx_d_rf_freq_val (const char* data, char* ret) {
	strcpy(buf, "fwd -b 0 -m 'rf -c d -f ");
	strcat(buf, data);
	strcat(buf, "'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
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

	uint8_t gain;
	sscanf(data, "%"SCNd8"", &gain);

	// set the total VGA gain to 42 dB
	strcpy(buf, "fwd -b 0 -m 'rf -c d -g 3'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'rf -c d -h 2'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'rf -c d -i 2'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
	memset(buf, 0, MAX_PROP_LEN);
	strcpy(buf, "fwd -b 0 -m 'rf -c d -p 0'\r");
	send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

	// attenuate 42 to desired amount
	if (gain < 42) {
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'rf -c d -x ");
		sprintf(buf + strlen(buf), "%i", (42-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'rf -c d -y ");
		sprintf(buf + strlen(buf), "%i", (42-gain)/3);
		strcat(buf, "'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'rf -c d -z ");
		sprintf(buf + strlen(buf), "%i", (42-gain)/3);
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
	strcpy(buf, "fwd -b 0 -m 'board -c d -t'");
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
	sscanf(data, "%lf", &rate);
	write_hps_reg( "rxd1", (int)floor(BASE_SAMPLE_RATE / rate) );
	memset(ret, 0, MAX_PROP_LEN);
	sprintf(ret, "%lf", BASE_SAMPLE_RATE/((int)floor(BASE_SAMPLE_RATE / rate)) );
	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_nco_adj (const char* data, char* ret) {
	uint32_t freq;
	sscanf(data, "%"SCNd32"", &freq);
	write_hps_reg( "rxd0", (freq * pow(2,32)) / BASE_SAMPLE_FREQ);
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

static int set_rx_d_link_iface (const char* data, char* ret) {
	// TODO: FW support for streaming to management port required
	return RETURN_SUCCESS;
}

static int set_rx_d_link_port (const char* data, char* ret) {
	uint32_t port;
	sscanf(data, "%"SCNd32"", &port);
	write_hps_reg( "net29", port);
	return RETURN_SUCCESS;
}

static int set_rx_d_link_ip_dest (const char* data, char* ret) {
	uint8_t ip[4];
	sscanf(data, "%"SCNd8".%"SCNd8".%"SCNd8".%"SCNd8"", ip, ip+1, ip+2, ip+3);
	write_hps_reg( "rxd5", (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]) );
	return RETURN_SUCCESS;
}

static int set_rx_d_link_mac_dest (const char* data, char* ret) {
	// TODO: FW and register addition required
	return RETURN_SUCCESS;
}

static int set_rx_d_link_pay_len (const char* data, char* ret) {
	// TODO: FW support required
	return RETURN_SUCCESS;
}

static int set_rx_d_pwr (const char* data, char* ret) {
	uint32_t old_val;
	uint8_t power;
	sscanf(data, "%"SCNd8"", &power);

	// ENABLE
	if (power > 0) {
		// set current rx board to demo mode
		strcpy(buf, "fwd -b 0 -m 'board -c d -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// reset DSP FPGA registers for current channel
		write_hps_reg( "rxa0", 0x0);
		write_hps_reg( "rxa1", 0xff);
		write_hps_reg( "rxa4", 0x0);

	// DISABLE
	} else {
		// set current rx board to demo mode
		strcpy(buf, "fwd -b 0 -m 'board -c d -d'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// set all ADC's to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 0 -m 'power -c 15 -a 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// disable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val & ~(1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val & ~(1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val & ~(1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val & ~(1<<8));

		// disable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val & ~(1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val & ~(1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val & ~(1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val & ~(1<<8));

		// set all DACs to demo mode
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fwd -b 1 -m 'power -c 15 -d 1'\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// send the sync pulse from the LMK
		memset(buf, 0, MAX_PROP_LEN);
		strcpy(buf, "fpga -o\r");
		send_uart_comm(uart_fd, (uint8_t*)buf, strlen(buf));

		// enable all RX JESD lanes
		read_hps_reg(  "rxa4", &old_val);
		write_hps_reg( "rxa4", old_val | (1<<8));
		read_hps_reg(  "rxb4", &old_val);
		write_hps_reg( "rxb4", old_val | (1<<8));
		read_hps_reg(  "rxc4", &old_val);
		write_hps_reg( "rxc4", old_val | (1<<8));
		read_hps_reg(  "rxd4", &old_val);
		write_hps_reg( "rxd4", old_val | (1<<8));

		// enable all TX JESD lanes
		read_hps_reg(  "txa4", &old_val);
		write_hps_reg( "txa4", old_val | (1<<8));
		read_hps_reg(  "txb4", &old_val);
		write_hps_reg( "txb4", old_val | (1<<8));
		read_hps_reg(  "txc4", &old_val);
		write_hps_reg( "txc4", old_val | (1<<8));
		read_hps_reg(  "txd4", &old_val);
		write_hps_reg( "txd4", old_val | (1<<8));
	}
	return RETURN_SUCCESS;
}

static int set_time_clk_rate (const char* data, char* ret) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_clk_pps (const char* data, char* ret) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_clk_cur_time (const char* data, char* ret) {
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

static int set_time_source_ref (const char* data, char* ret) {
	// Insert MCU/MEM command

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
	strcpy(buf, "fwd -b 2 -m 'board -t'");
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
	if (ver > 0)	write_hps_reg( "net0", (old_val & ~(1 << 2) ) | (1 << 2));
	else		write_hps_reg( "net0", (old_val & ~(1 << 2) ));
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

static int set_poll_en (const char* data, char* ret) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

// Beginning of property table
static prop_t property_table[] = {
	{"tx_a/rf/dac/mixer", get_invalid, set_tx_a_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_a/rf/dac/nco", get_invalid, set_tx_a_rf_dac_nco, RW, NO_POLL, "1475.5"},
	{"tx_a/rf/dac/pap", get_invalid, set_tx_a_rf_dac_pap, RW, NO_POLL, "0"},
	{"tx_a/rf/dac/interp", get_invalid, set_tx_a_rf_dac_interp, RW, NO_POLL, "0"},
	{"tx_a/rf/dac/temp", get_tx_a_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_a/rf/dac/iqerr_gain", get_invalid, set_tx_a_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_a/rf/dac/iqerr_phase", get_invalid, set_tx_a_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_a/rf/freq/val", get_invalid, set_tx_a_rf_freq_val, RW, NO_POLL, "2400"},
	{"tx_a/rf/freq/lna", get_invalid, set_tx_a_rf_freq_lna, RW, NO_POLL, "0"},
	{"tx_a/rf/freq/i_bias", get_invalid, set_tx_a_rf_freq_i_bias, RW, NO_POLL, "0"},
	{"tx_a/rf/freq/q_bias", get_invalid, set_tx_a_rf_freq_q_bias, RW, NO_POLL, "0"},
	{"tx_a/rf/gain/val", get_invalid, set_tx_a_rf_gain_val, RW, NO_POLL, "10"},
	{"tx_a/rf/board/status", get_tx_a_rf_board_status, set_invalid, RO, POLL, "off"},
	{"tx_a/rf/board/dump", get_invalid, set_tx_a_rf_board_dump, WO, NO_POLL, "0"},
	{"tx_a/rf/board/test", get_invalid, set_tx_a_rf_board_test, WO, NO_POLL, "0"},
	{"tx_a/rf/board/temp", get_tx_a_rf_board_temp, set_invalid, RO, POLL, "23"},
	{"tx_a/rf/board/led", get_invalid, set_tx_a_rf_board_led, WO, NO_POLL, "0"},
	{"tx_a/dsp/freq", get_invalid, set_tx_a_dsp_freq, RW, NO_POLL, "2400"},
	{"tx_a/dsp/gain", get_invalid, set_tx_a_dsp_gain, RW, NO_POLL, "10"},
	{"tx_a/dsp/rate", get_invalid, set_tx_a_dsp_rate, RW, NO_POLL, "0"},
	{"tx_a/dsp/nco_adj", get_invalid, set_tx_a_dsp_nco_adj, RW, NO_POLL, "0"},
	{"tx_a/dsp/iqerr_gain", get_invalid, set_tx_a_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_a/dsp/iqerr_phase", get_invalid, set_tx_a_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_a/dsp/rstreq", get_invalid, set_tx_a_dsp_rstreq, WO, NO_POLL, "0"},
	{"tx_a/about/id", get_invalid, set_tx_a_about_id, RW, NO_POLL, "001"},
	{"tx_a/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"tx_a/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_a/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_a/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_a/link/iface", get_invalid, set_tx_a_link_iface, RW, NO_POLL, "10g"},
	{"tx_a/link/port", get_invalid, set_tx_a_link_port, RW, NO_POLL, "42820"},
	{"tx_a/link/pay_len", get_invalid, set_tx_a_link_pay_len, RW, NO_POLL, "1400"},
	{"tx_a/pwr", get_invalid, set_tx_a_pwr, RW, NO_POLL, "0"},
	{"rx_a/rf/vga/freq", get_invalid, set_rx_a_rf_vga_freq, RW, NO_POLL, "0"},
	{"rx_a/rf/vga/bypass", get_invalid, set_rx_a_rf_vga_bypass, RW, NO_POLL, "1"},
	{"rx_a/rf/vga/gain1", get_invalid, set_rx_a_rf_vga_gain1, RW, NO_POLL, "9"},
	{"rx_a/rf/vga/gain2", get_invalid, set_rx_a_rf_vga_gain2, RW, NO_POLL, "12"},
	{"rx_a/rf/vga/gain3", get_invalid, set_rx_a_rf_vga_gain3, RW, NO_POLL, "12"},
	{"rx_a/rf/vga/pgain", get_invalid, set_rx_a_rf_vga_pgain, RW, NO_POLL, "3"},
	{"rx_a/rf/vga/atten1", get_invalid, set_rx_a_rf_vga_atten1, RW, NO_POLL, "0"},
	{"rx_a/rf/vga/atten2", get_invalid, set_rx_a_rf_vga_atten2, RW, NO_POLL, "0"},
	{"rx_a/rf/vga/atten3", get_invalid, set_rx_a_rf_vga_atten3, RW, NO_POLL, "0"},
	{"rx_a/rf/freq/val", get_invalid, set_rx_a_rf_freq_val, RW, NO_POLL, "2400"},
	{"rx_a/rf/freq/lna", get_invalid, set_rx_a_rf_freq_lna, RW, NO_POLL, "0"},
	{"rx_a/rf/freq/varac", get_invalid, set_rx_a_rf_freq_varac, RW, NO_POLL, "0"},
	{"rx_a/rf/gain/val", get_invalid, set_rx_a_rf_gain_val, RW, NO_POLL, "10"},
	{"rx_a/rf/board/status", get_rx_a_rf_board_status, set_invalid, RO, POLL, "off"},
	{"rx_a/rf/board/dump", get_invalid, set_rx_a_rf_board_dump, WO, NO_POLL, "0"},
	{"rx_a/rf/board/test", get_invalid, set_rx_a_rf_board_test, WO, NO_POLL, "0"},
	{"rx_a/rf/board/temp", get_rx_a_rf_board_temp, set_invalid, RO, POLL, "20"},
	{"rx_a/rf/board/led", get_invalid, set_rx_a_rf_board_led, WO, NO_POLL, "0"},
	{"rx_a/dsp/freq", get_invalid, set_rx_a_dsp_freq, RW, NO_POLL, "2400"},
	{"rx_a/dsp/gain", get_invalid, set_rx_a_dsp_gain, RW, NO_POLL, "10"},
	{"rx_a/dsp/rate", get_invalid, set_rx_a_dsp_rate, RW, NO_POLL, "0"},
	{"rx_a/dsp/nco_adj", get_invalid, set_rx_a_dsp_nco_adj, RW, NO_POLL, "0"},
	{"rx_a/dsp/iqerr_gain", get_invalid, set_rx_a_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"rx_a/dsp/iqerr_phase", get_invalid, set_rx_a_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"rx_a/dsp/rstreq", get_invalid, set_rx_a_dsp_rstreq, WO, NO_POLL, "0"},
	{"rx_a/about/id", get_invalid, set_rx_a_about_id, RW, NO_POLL, "001"},
	{"rx_a/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"rx_a/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_a/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_a/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_a/link/iface", get_invalid, set_rx_a_link_iface, RW, NO_POLL, "10g"},
	{"rx_a/link/port", get_invalid, set_rx_a_link_port, RW, NO_POLL, "42820"},
	{"rx_a/link/ip_dest", get_invalid, set_rx_a_link_ip_dest, RW, NO_POLL, "10.10.10.1"},
	{"rx_a/link/mac_dest", get_invalid, set_rx_a_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"rx_a/link/pay_len", get_invalid, set_rx_a_link_pay_len, RW, NO_POLL, "1400"},
	{"rx_a/pwr", get_invalid, set_rx_a_pwr, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/mixer", get_invalid, set_tx_b_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/nco", get_invalid, set_tx_b_rf_dac_nco, RW, NO_POLL, "1475.5"},
	{"tx_b/rf/dac/pap", get_invalid, set_tx_b_rf_dac_pap, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/interp", get_invalid, set_tx_b_rf_dac_interp, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/temp", get_tx_b_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_b/rf/dac/iqerr_gain", get_invalid, set_tx_b_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/iqerr_phase", get_invalid, set_tx_b_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_b/rf/freq/val", get_invalid, set_tx_b_rf_freq_val, RW, NO_POLL, "2400"},
	{"tx_b/rf/freq/lna", get_invalid, set_tx_b_rf_freq_lna, RW, NO_POLL, "0"},
	{"tx_b/rf/freq/i_bias", get_invalid, set_tx_b_rf_freq_i_bias, RW, NO_POLL, "0"},
	{"tx_b/rf/freq/q_bias", get_invalid, set_tx_b_rf_freq_q_bias, RW, NO_POLL, "0"},
	{"tx_b/rf/gain/val", get_invalid, set_tx_b_rf_gain_val, RW, NO_POLL, "10"},
	{"tx_b/rf/board/status", get_tx_b_rf_board_status, set_invalid, RO, POLL, "off"},
	{"tx_b/rf/board/dump", get_invalid, set_tx_b_rf_board_dump, WO, NO_POLL, "0"},
	{"tx_b/rf/board/test", get_invalid, set_tx_b_rf_board_test, WO, NO_POLL, "0"},
	{"tx_b/rf/board/temp", get_tx_b_rf_board_temp, set_invalid, RO, POLL, "23"},
	{"tx_b/rf/board/led", get_invalid, set_tx_b_rf_board_led, WO, NO_POLL, "0"},
	{"tx_b/dsp/freq", get_invalid, set_tx_b_dsp_freq, RW, NO_POLL, "2400"},
	{"tx_b/dsp/gain", get_invalid, set_tx_b_dsp_gain, RW, NO_POLL, "10"},
	{"tx_b/dsp/rate", get_invalid, set_tx_b_dsp_rate, RW, NO_POLL, "0"},
	{"tx_b/dsp/nco_adj", get_invalid, set_tx_b_dsp_nco_adj, RW, NO_POLL, "0"},
	{"tx_b/dsp/iqerr_gain", get_invalid, set_tx_b_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_b/dsp/iqerr_phase", get_invalid, set_tx_b_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_b/dsp/rstreq", get_invalid, set_tx_b_dsp_rstreq, WO, NO_POLL, "0"},
	{"tx_b/about/id", get_invalid, set_tx_b_about_id, RW, NO_POLL, "001"},
	{"tx_b/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"tx_b/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_b/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_b/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_b/link/iface", get_invalid, set_tx_b_link_iface, RW, NO_POLL, "10g"},
	{"tx_b/link/port", get_invalid, set_tx_b_link_port, RW, NO_POLL, "42820"},
	{"tx_b/link/pay_len", get_invalid, set_tx_b_link_pay_len, RW, NO_POLL, "1400"},
	{"tx_b/pwr", get_invalid, set_tx_b_pwr, RW, NO_POLL, "0"},
	{"rx_b/rf/vga/freq", get_invalid, set_rx_b_rf_vga_freq, RW, NO_POLL, "0"},
	{"rx_b/rf/vga/bypass", get_invalid, set_rx_b_rf_vga_bypass, RW, NO_POLL, "1"},
	{"rx_b/rf/vga/gain1", get_invalid, set_rx_b_rf_vga_gain1, RW, NO_POLL, "9"},
	{"rx_b/rf/vga/gain2", get_invalid, set_rx_b_rf_vga_gain2, RW, NO_POLL, "12"},
	{"rx_b/rf/vga/gain3", get_invalid, set_rx_b_rf_vga_gain3, RW, NO_POLL, "12"},
	{"rx_b/rf/vga/pgain", get_invalid, set_rx_b_rf_vga_pgain, RW, NO_POLL, "3"},
	{"rx_b/rf/vga/atten1", get_invalid, set_rx_b_rf_vga_atten1, RW, NO_POLL, "0"},
	{"rx_b/rf/vga/atten2", get_invalid, set_rx_b_rf_vga_atten2, RW, NO_POLL, "0"},
	{"rx_b/rf/vga/atten3", get_invalid, set_rx_b_rf_vga_atten3, RW, NO_POLL, "0"},
	{"rx_b/rf/freq/val", get_invalid, set_rx_b_rf_freq_val, RW, NO_POLL, "2400"},
	{"rx_b/rf/freq/lna", get_invalid, set_rx_b_rf_freq_lna, RW, NO_POLL, "0"},
	{"rx_b/rf/freq/varac", get_invalid, set_rx_b_rf_freq_varac, RW, NO_POLL, "0"},
	{"rx_b/rf/gain/val", get_invalid, set_rx_b_rf_gain_val, RW, NO_POLL, "10"},
	{"rx_b/rf/board/status", get_rx_b_rf_board_status, set_invalid, RO, POLL, "off"},
	{"rx_b/rf/board/dump", get_invalid, set_rx_b_rf_board_dump, WO, NO_POLL, "0"},
	{"rx_b/rf/board/test", get_invalid, set_rx_b_rf_board_test, WO, NO_POLL, "0"},
	{"rx_b/rf/board/temp", get_rx_b_rf_board_temp, set_invalid, RO, POLL, "20"},
	{"rx_b/rf/board/led", get_invalid, set_rx_b_rf_board_led, WO, NO_POLL, "0"},
	{"rx_b/dsp/freq", get_invalid, set_rx_b_dsp_freq, RW, NO_POLL, "2400"},
	{"rx_b/dsp/gain", get_invalid, set_rx_b_dsp_gain, RW, NO_POLL, "10"},
	{"rx_b/dsp/rate", get_invalid, set_rx_b_dsp_rate, RW, NO_POLL, "0"},
	{"rx_b/dsp/nco_adj", get_invalid, set_rx_b_dsp_nco_adj, RW, NO_POLL, "0"},
	{"rx_b/dsp/iqerr_gain", get_invalid, set_rx_b_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"rx_b/dsp/iqerr_phase", get_invalid, set_rx_b_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"rx_b/dsp/rstreq", get_invalid, set_rx_b_dsp_rstreq, WO, NO_POLL, "0"},
	{"rx_b/about/id", get_invalid, set_rx_b_about_id, RW, NO_POLL, "001"},
	{"rx_b/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"rx_b/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_b/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_b/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_b/link/iface", get_invalid, set_rx_b_link_iface, RW, NO_POLL, "10g"},
	{"rx_b/link/port", get_invalid, set_rx_b_link_port, RW, NO_POLL, "42820"},
	{"rx_b/link/ip_dest", get_invalid, set_rx_b_link_ip_dest, RW, NO_POLL, "10.10.10.1"},
	{"rx_b/link/mac_dest", get_invalid, set_rx_b_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"rx_b/link/pay_len", get_invalid, set_rx_b_link_pay_len, RW, NO_POLL, "1400"},
	{"rx_b/pwr", get_invalid, set_rx_b_pwr, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/mixer", get_invalid, set_tx_c_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/nco", get_invalid, set_tx_c_rf_dac_nco, RW, NO_POLL, "1475.5"},
	{"tx_c/rf/dac/pap", get_invalid, set_tx_c_rf_dac_pap, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/interp", get_invalid, set_tx_c_rf_dac_interp, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/temp", get_tx_c_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_c/rf/dac/iqerr_gain", get_invalid, set_tx_c_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/iqerr_phase", get_invalid, set_tx_c_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_c/rf/freq/val", get_invalid, set_tx_c_rf_freq_val, RW, NO_POLL, "2400"},
	{"tx_c/rf/freq/lna", get_invalid, set_tx_c_rf_freq_lna, RW, NO_POLL, "0"},
	{"tx_c/rf/freq/i_bias", get_invalid, set_tx_c_rf_freq_i_bias, RW, NO_POLL, "0"},
	{"tx_c/rf/freq/q_bias", get_invalid, set_tx_c_rf_freq_q_bias, RW, NO_POLL, "0"},
	{"tx_c/rf/gain/val", get_invalid, set_tx_c_rf_gain_val, RW, NO_POLL, "10"},
	{"tx_c/rf/board/status", get_tx_c_rf_board_status, set_invalid, RO, POLL, "off"},
	{"tx_c/rf/board/dump", get_invalid, set_tx_c_rf_board_dump, WO, NO_POLL, "0"},
	{"tx_c/rf/board/test", get_invalid, set_tx_c_rf_board_test, WO, NO_POLL, "0"},
	{"tx_c/rf/board/temp", get_tx_c_rf_board_temp, set_invalid, RO, POLL, "23"},
	{"tx_c/rf/board/led", get_invalid, set_tx_c_rf_board_led, WO, NO_POLL, "0"},
	{"tx_c/dsp/freq", get_invalid, set_tx_c_dsp_freq, RW, NO_POLL, "2400"},
	{"tx_c/dsp/gain", get_invalid, set_tx_c_dsp_gain, RW, NO_POLL, "10"},
	{"tx_c/dsp/rate", get_invalid, set_tx_c_dsp_rate, RW, NO_POLL, "0"},
	{"tx_c/dsp/nco_adj", get_invalid, set_tx_c_dsp_nco_adj, RW, NO_POLL, "0"},
	{"tx_c/dsp/iqerr_gain", get_invalid, set_tx_c_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_c/dsp/iqerr_phase", get_invalid, set_tx_c_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_c/dsp/rstreq", get_invalid, set_tx_c_dsp_rstreq, WO, NO_POLL, "0"},
	{"tx_c/about/id", get_invalid, set_tx_c_about_id, RW, NO_POLL, "001"},
	{"tx_c/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"tx_c/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_c/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_c/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_c/link/iface", get_invalid, set_tx_c_link_iface, RW, NO_POLL, "10g"},
	{"tx_c/link/port", get_invalid, set_tx_c_link_port, RW, NO_POLL, "42820"},
	{"tx_c/link/pay_len", get_invalid, set_tx_c_link_pay_len, RW, NO_POLL, "1400"},
	{"tx_c/pwr", get_invalid, set_tx_c_pwr, RW, NO_POLL, "0"},
	{"rx_c/rf/vga/freq", get_invalid, set_rx_c_rf_vga_freq, RW, NO_POLL, "0"},
	{"rx_c/rf/vga/bypass", get_invalid, set_rx_c_rf_vga_bypass, RW, NO_POLL, "1"},
	{"rx_c/rf/vga/gain1", get_invalid, set_rx_c_rf_vga_gain1, RW, NO_POLL, "9"},
	{"rx_c/rf/vga/gain2", get_invalid, set_rx_c_rf_vga_gain2, RW, NO_POLL, "12"},
	{"rx_c/rf/vga/gain3", get_invalid, set_rx_c_rf_vga_gain3, RW, NO_POLL, "12"},
	{"rx_c/rf/vga/pgain", get_invalid, set_rx_c_rf_vga_pgain, RW, NO_POLL, "3"},
	{"rx_c/rf/vga/atten1", get_invalid, set_rx_c_rf_vga_atten1, RW, NO_POLL, "0"},
	{"rx_c/rf/vga/atten2", get_invalid, set_rx_c_rf_vga_atten2, RW, NO_POLL, "0"},
	{"rx_c/rf/vga/atten3", get_invalid, set_rx_c_rf_vga_atten3, RW, NO_POLL, "0"},
	{"rx_c/rf/freq/val", get_invalid, set_rx_c_rf_freq_val, RW, NO_POLL, "2400"},
	{"rx_c/rf/freq/lna", get_invalid, set_rx_c_rf_freq_lna, RW, NO_POLL, "0"},
	{"rx_c/rf/freq/varac", get_invalid, set_rx_c_rf_freq_varac, RW, NO_POLL, "0"},
	{"rx_c/rf/gain/val", get_invalid, set_rx_c_rf_gain_val, RW, NO_POLL, "10"},
	{"rx_c/rf/board/status", get_rx_c_rf_board_status, set_invalid, RO, POLL, "off"},
	{"rx_c/rf/board/dump", get_invalid, set_rx_c_rf_board_dump, WO, NO_POLL, "0"},
	{"rx_c/rf/board/test", get_invalid, set_rx_c_rf_board_test, WO, NO_POLL, "0"},
	{"rx_c/rf/board/temp", get_rx_c_rf_board_temp, set_invalid, RO, POLL, "20"},
	{"rx_c/rf/board/led", get_invalid, set_rx_c_rf_board_led, WO, NO_POLL, "0"},
	{"rx_c/dsp/freq", get_invalid, set_rx_c_dsp_freq, RW, NO_POLL, "2400"},
	{"rx_c/dsp/gain", get_invalid, set_rx_c_dsp_gain, RW, NO_POLL, "10"},
	{"rx_c/dsp/rate", get_invalid, set_rx_c_dsp_rate, RW, NO_POLL, "0"},
	{"rx_c/dsp/nco_adj", get_invalid, set_rx_c_dsp_nco_adj, RW, NO_POLL, "0"},
	{"rx_c/dsp/iqerr_gain", get_invalid, set_rx_c_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"rx_c/dsp/iqerr_phase", get_invalid, set_rx_c_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"rx_c/dsp/rstreq", get_invalid, set_rx_c_dsp_rstreq, WO, NO_POLL, "0"},
	{"rx_c/about/id", get_invalid, set_rx_c_about_id, RW, NO_POLL, "001"},
	{"rx_c/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"rx_c/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_c/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_c/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_c/link/iface", get_invalid, set_rx_c_link_iface, RW, NO_POLL, "10g"},
	{"rx_c/link/port", get_invalid, set_rx_c_link_port, RW, NO_POLL, "42820"},
	{"rx_c/link/ip_dest", get_invalid, set_rx_c_link_ip_dest, RW, NO_POLL, "10.10.10.1"},
	{"rx_c/link/mac_dest", get_invalid, set_rx_c_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"rx_c/link/pay_len", get_invalid, set_rx_c_link_pay_len, RW, NO_POLL, "1400"},
	{"rx_c/pwr", get_invalid, set_rx_c_pwr, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/mixer", get_invalid, set_tx_d_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/nco", get_invalid, set_tx_d_rf_dac_nco, RW, NO_POLL, "1475.5"},
	{"tx_d/rf/dac/pap", get_invalid, set_tx_d_rf_dac_pap, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/interp", get_invalid, set_tx_d_rf_dac_interp, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/temp", get_tx_d_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_d/rf/dac/iqerr_gain", get_invalid, set_tx_d_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/iqerr_phase", get_invalid, set_tx_d_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_d/rf/freq/val", get_invalid, set_tx_d_rf_freq_val, RW, NO_POLL, "2400"},
	{"tx_d/rf/freq/lna", get_invalid, set_tx_d_rf_freq_lna, RW, NO_POLL, "0"},
	{"tx_d/rf/freq/i_bias", get_invalid, set_tx_d_rf_freq_i_bias, RW, NO_POLL, "0"},
	{"tx_d/rf/freq/q_bias", get_invalid, set_tx_d_rf_freq_q_bias, RW, NO_POLL, "0"},
	{"tx_d/rf/gain/val", get_invalid, set_tx_d_rf_gain_val, RW, NO_POLL, "10"},
	{"tx_d/rf/board/status", get_tx_d_rf_board_status, set_invalid, RO, POLL, "off"},
	{"tx_d/rf/board/dump", get_invalid, set_tx_d_rf_board_dump, WO, NO_POLL, "0"},
	{"tx_d/rf/board/test", get_invalid, set_tx_d_rf_board_test, WO, NO_POLL, "0"},
	{"tx_d/rf/board/temp", get_tx_d_rf_board_temp, set_invalid, RO, POLL, "23"},
	{"tx_d/rf/board/led", get_invalid, set_tx_d_rf_board_led, WO, NO_POLL, "0"},
	{"tx_d/dsp/freq", get_invalid, set_tx_d_dsp_freq, RW, NO_POLL, "2400"},
	{"tx_d/dsp/gain", get_invalid, set_tx_d_dsp_gain, RW, NO_POLL, "10"},
	{"tx_d/dsp/rate", get_invalid, set_tx_d_dsp_rate, RW, NO_POLL, "0"},
	{"tx_d/dsp/nco_adj", get_invalid, set_tx_d_dsp_nco_adj, RW, NO_POLL, "0"},
	{"tx_d/dsp/iqerr_gain", get_invalid, set_tx_d_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_d/dsp/iqerr_phase", get_invalid, set_tx_d_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_d/dsp/rstreq", get_invalid, set_tx_d_dsp_rstreq, WO, NO_POLL, "0"},
	{"tx_d/about/id", get_invalid, set_tx_d_about_id, RW, NO_POLL, "001"},
	{"tx_d/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"tx_d/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_d/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_d/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"tx_d/link/iface", get_invalid, set_tx_d_link_iface, RW, NO_POLL, "10g"},
	{"tx_d/link/port", get_invalid, set_tx_d_link_port, RW, NO_POLL, "42820"},
	{"tx_d/link/pay_len", get_invalid, set_tx_d_link_pay_len, RW, NO_POLL, "1400"},
	{"tx_d/pwr", get_invalid, set_tx_d_pwr, RW, NO_POLL, "0"},
	{"rx_d/rf/vga/freq", get_invalid, set_rx_d_rf_vga_freq, RW, NO_POLL, "0"},
	{"rx_d/rf/vga/bypass", get_invalid, set_rx_d_rf_vga_bypass, RW, NO_POLL, "1"},
	{"rx_d/rf/vga/gain1", get_invalid, set_rx_d_rf_vga_gain1, RW, NO_POLL, "9"},
	{"rx_d/rf/vga/gain2", get_invalid, set_rx_d_rf_vga_gain2, RW, NO_POLL, "12"},
	{"rx_d/rf/vga/gain3", get_invalid, set_rx_d_rf_vga_gain3, RW, NO_POLL, "12"},
	{"rx_d/rf/vga/pgain", get_invalid, set_rx_d_rf_vga_pgain, RW, NO_POLL, "3"},
	{"rx_d/rf/vga/atten1", get_invalid, set_rx_d_rf_vga_atten1, RW, NO_POLL, "0"},
	{"rx_d/rf/vga/atten2", get_invalid, set_rx_d_rf_vga_atten2, RW, NO_POLL, "0"},
	{"rx_d/rf/vga/atten3", get_invalid, set_rx_d_rf_vga_atten3, RW, NO_POLL, "0"},
	{"rx_d/rf/freq/val", get_invalid, set_rx_d_rf_freq_val, RW, NO_POLL, "2400"},
	{"rx_d/rf/freq/lna", get_invalid, set_rx_d_rf_freq_lna, RW, NO_POLL, "0"},
	{"rx_d/rf/freq/varac", get_invalid, set_rx_d_rf_freq_varac, RW, NO_POLL, "0"},
	{"rx_d/rf/gain/val", get_invalid, set_rx_d_rf_gain_val, RW, NO_POLL, "10"},
	{"rx_d/rf/board/status", get_rx_d_rf_board_status, set_invalid, RO, POLL, "off"},
	{"rx_d/rf/board/dump", get_invalid, set_rx_d_rf_board_dump, WO, NO_POLL, "0"},
	{"rx_d/rf/board/test", get_invalid, set_rx_d_rf_board_test, WO, NO_POLL, "0"},
	{"rx_d/rf/board/temp", get_rx_d_rf_board_temp, set_invalid, RO, POLL, "20"},
	{"rx_d/rf/board/led", get_invalid, set_rx_d_rf_board_led, WO, NO_POLL, "0"},
	{"rx_d/dsp/freq", get_invalid, set_rx_d_dsp_freq, RW, NO_POLL, "2400"},
	{"rx_d/dsp/gain", get_invalid, set_rx_d_dsp_gain, RW, NO_POLL, "10"},
	{"rx_d/dsp/rate", get_invalid, set_rx_d_dsp_rate, RW, NO_POLL, "0"},
	{"rx_d/dsp/nco_adj", get_invalid, set_rx_d_dsp_nco_adj, RW, NO_POLL, "0"},
	{"rx_d/dsp/iqerr_gain", get_invalid, set_rx_d_dsp_iqerr_gain, RW, NO_POLL, "0"},
	{"rx_d/dsp/iqerr_phase", get_invalid, set_rx_d_dsp_iqerr_phase, RW, NO_POLL, "0"},
	{"rx_d/dsp/rstreq", get_invalid, set_rx_d_dsp_rstreq, WO, NO_POLL, "0"},
	{"rx_d/about/id", get_invalid, set_rx_d_about_id, RW, NO_POLL, "001"},
	{"rx_d/about/serial", get_invalid, set_invalid, RO, NO_POLL, "001"},
	{"rx_d/about/fw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_d/about/hw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_d/about/sw_ver", get_invalid, set_invalid, RO, NO_POLL, "12-12-2014"},
	{"rx_d/link/iface", get_invalid, set_rx_d_link_iface, RW, NO_POLL, "10g"},
	{"rx_d/link/port", get_invalid, set_rx_d_link_port, RW, NO_POLL, "42820"},
	{"rx_d/link/ip_dest", get_invalid, set_rx_d_link_ip_dest, RW, NO_POLL, "10.10.10.1"},
	{"rx_d/link/mac_dest", get_invalid, set_rx_d_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"rx_d/link/pay_len", get_invalid, set_rx_d_link_pay_len, RW, NO_POLL, "1400"},
	{"rx_d/pwr", get_invalid, set_rx_d_pwr, RW, NO_POLL, "0"},
	{"time/clk/rate", get_invalid, set_time_clk_rate, RW, NO_POLL, "322"},
	{"time/clk/pps", get_invalid, set_time_clk_pps, RW, NO_POLL, "0"},
	{"time/clk/cur_time", get_invalid, set_time_clk_cur_time, RW, NO_POLL, "0"},
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
	{"fpga/link/sfpa/mac_addr", get_invalid, set_fpga_link_sfpa_mac_addr, RW, NO_POLL, "aa:aa:aa:aa:aa:aa"},
	{"fpga/link/sfpa/ver", get_invalid, set_fpga_link_sfpa_ver, RW, NO_POLL, "0"},
	{"fpga/link/sfpb/ip_addr", get_invalid, set_fpga_link_sfpb_ip_addr, RW, NO_POLL, "10.10.10.2"},
	{"fpga/link/sfpb/mac_addr", get_invalid, set_fpga_link_sfpb_mac_addr, RW, NO_POLL, "aa:aa:aa:aa:aa:aa"},
	{"fpga/link/sfpb/ver", get_invalid, set_fpga_link_sfpb_ver, RW, NO_POLL, "0"},
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
