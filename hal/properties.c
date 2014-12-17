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

static int uart_fd = 0;

// Beginning of property functions, very long because each property needs to be
// handled explicitly
static int get_invalid (const char* data) {
	printf("Cannot invoke a get on this property\n");
	return RETURN_ERROR_GET_PROP;
}

static int set_invalid (const char* data) {
	printf("Cannot invoke a set on this property\n");
	return RETURN_ERROR_SET_PROP;
}
static int set_tx_a_rf_dac_mixer (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_dac_nco (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_dac_pap (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_dac_interp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_a_rf_dac_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_dac_iqerr_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_dac_iqerr_phase (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_freq_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_a_rf_freq_band (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_freq_lna (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_freq_i_bias (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_freq_q_bias (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_gain_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_a_rf_board_status (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_board_dump (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_board_test (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_a_rf_board_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_rf_board_led (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_freq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_interp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_rate (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_nco_adj (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_iqerr_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_iqerr_phase (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_dsp_rstreq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_about_id (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_link_loopback (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_link_iface (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_link_port (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_link_ip_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_link_ip_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_link_mac_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_link_mac_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_link_pay_len (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_link_ver (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_a_pwr (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_freq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_bypass (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_gain1 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_gain2 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_gain3 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_pgain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_atten1 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_atten2 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_vga_atten3 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_freq_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_rx_a_rf_freq_band (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_freq_lna (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_freq_varac (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_gain_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_rx_a_rf_board_status (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_board_dump (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_board_test (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_rx_a_rf_board_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_rf_board_led (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_freq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_interp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_rate (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_nco_adj (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_iqerr_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_iqerr_phase (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_dsp_rstreq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_about_id (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_link_loopback (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_link_iface (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_link_port (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_link_ip_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_link_ip_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_link_mac_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_link_mac_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_link_pay_len (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_link_ver (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_a_pwr (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_mixer (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_nco (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_pap (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_interp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_b_rf_dac_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_iqerr_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_dac_iqerr_phase (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_freq_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_b_rf_freq_band (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_freq_lna (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_freq_i_bias (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_freq_q_bias (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_gain_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_b_rf_board_status (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_board_dump (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_board_test (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_b_rf_board_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_rf_board_led (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_freq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_interp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_rate (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_nco_adj (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_iqerr_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_iqerr_phase (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_dsp_rstreq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_about_id (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_link_loopback (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_link_iface (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_link_port (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_link_ip_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_link_ip_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_link_mac_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_link_mac_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_link_pay_len (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_link_ver (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_b_pwr (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_freq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_bypass (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_gain1 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_gain2 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_gain3 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_pgain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_atten1 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_atten2 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_vga_atten3 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_freq_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_rx_b_rf_freq_band (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_freq_lna (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_freq_varac (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_gain_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_rx_b_rf_board_status (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_board_dump (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_board_test (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_rx_b_rf_board_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_rf_board_led (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_freq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_interp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_rate (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_nco_adj (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_iqerr_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_iqerr_phase (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_dsp_rstreq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_about_id (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_link_loopback (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_link_iface (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_link_port (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_link_ip_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_link_ip_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_link_mac_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_link_mac_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_link_pay_len (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_link_ver (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_b_pwr (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_mixer (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_nco (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_pap (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_interp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_c_rf_dac_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_iqerr_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_dac_iqerr_phase (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_freq_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_c_rf_freq_band (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_freq_lna (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_freq_i_bias (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_freq_q_bias (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_gain_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_c_rf_board_status (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_board_dump (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_board_test (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_c_rf_board_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_rf_board_led (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_freq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_interp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_rate (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_nco_adj (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_iqerr_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_iqerr_phase (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_dsp_rstreq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_about_id (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_link_loopback (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_link_iface (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_link_port (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_link_ip_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_link_ip_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_link_mac_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_link_mac_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_link_pay_len (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_link_ver (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_c_pwr (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_freq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_bypass (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_gain1 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_gain2 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_gain3 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_pgain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_atten1 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_atten2 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_vga_atten3 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_freq_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_rx_c_rf_freq_band (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_freq_lna (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_freq_varac (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_gain_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_rx_c_rf_board_status (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_board_dump (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_board_test (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_rx_c_rf_board_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_rf_board_led (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_freq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_interp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_rate (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_nco_adj (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_iqerr_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_iqerr_phase (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_dsp_rstreq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_about_id (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_link_loopback (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_link_iface (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_link_port (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_link_ip_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_link_ip_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_link_mac_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_link_mac_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_link_pay_len (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_link_ver (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_c_pwr (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_mixer (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_nco (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_pap (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_interp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_d_rf_dac_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_iqerr_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_dac_iqerr_phase (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_freq_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_d_rf_freq_band (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_freq_lna (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_freq_i_bias (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_freq_q_bias (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_gain_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_d_rf_board_status (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_board_dump (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_board_test (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_tx_d_rf_board_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_rf_board_led (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_freq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_interp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_rate (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_nco_adj (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_iqerr_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_iqerr_phase (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_dsp_rstreq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_about_id (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_link_loopback (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_link_iface (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_link_port (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_link_ip_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_link_ip_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_link_mac_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_link_mac_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_link_pay_len (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_link_ver (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_tx_d_pwr (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_freq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_bypass (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_gain1 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_gain2 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_gain3 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_pgain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_atten1 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_atten2 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_vga_atten3 (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_freq_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_rx_d_rf_freq_band (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_freq_lna (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_freq_varac (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_gain_val (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_rx_d_rf_board_status (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_board_dump (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_board_test (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_rx_d_rf_board_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_rf_board_led (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_freq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_interp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_rate (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_nco_adj (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_iqerr_gain (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_iqerr_phase (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_dsp_rstreq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_about_id (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_link_loopback (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_link_iface (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_link_port (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_link_ip_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_link_ip_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_link_mac_src (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_link_mac_dest (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_link_pay_len (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_link_ver (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_rx_d_pwr (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_clk_rate (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_clk_pps (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_clk_cur_time (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_source_vco (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_source_sync (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_source_ref (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_time_board_status (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_board_dump (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_board_test (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_time_board_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_board_led (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_time_about_id (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_fpga_board_status (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_fpga_board_dump (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_fpga_board_test (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int get_fpga_board_temp (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_fpga_board_led (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_fpga_board_rstreq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_fpga_board_jesd_rstreq (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_fpga_about_id (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_fpga_link_port (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_fpga_link_ip (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_fpga_link_mac (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_fpga_link_ver (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_fpga_link_rate (const char* data) {
	// Insert MCU/MEM command

	return RETURN_SUCCESS;
}

static int set_poll_en (const char* data) {
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
	{"tx_a/rf/freq/band", get_tx_a_rf_freq_band, set_invalid, RO, POLL, "1"},
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
	{"tx_a/dsp/interp", get_invalid, set_tx_a_dsp_interp, RW, NO_POLL, "256"},
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
	{"tx_a/link/loopback", get_invalid, set_tx_a_link_loopback, RW, NO_POLL, "0"},
	{"tx_a/link/iface", get_invalid, set_tx_a_link_iface, RW, NO_POLL, "10g"},
	{"tx_a/link/port", get_invalid, set_tx_a_link_port, RW, NO_POLL, "42820"},
	{"tx_a/link/ip_src", get_invalid, set_tx_a_link_ip_src, RW, NO_POLL, "10.10.10.2"},
	{"tx_a/link/ip_dest", get_invalid, set_tx_a_link_ip_dest, RW, NO_POLL, "10.10.10.1"},
	{"tx_a/link/mac_src", get_invalid, set_tx_a_link_mac_src, RW, NO_POLL, "aa:aa:aa:aa:aa:aa"},
	{"tx_a/link/mac_dest", get_invalid, set_tx_a_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"tx_a/link/pay_len", get_invalid, set_tx_a_link_pay_len, RW, NO_POLL, "1400"},
	{"tx_a/link/ver", get_invalid, set_tx_a_link_ver, RW, NO_POLL, "0"},
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
	{"rx_a/rf/freq/band", get_rx_a_rf_freq_band, set_invalid, RO, POLL, "1"},
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
	{"rx_a/dsp/interp", get_invalid, set_rx_a_dsp_interp, RW, NO_POLL, "0"},
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
	{"rx_a/link/loopback", get_invalid, set_rx_a_link_loopback, RW, NO_POLL, "0"},
	{"rx_a/link/iface", get_invalid, set_rx_a_link_iface, RW, NO_POLL, "10g"},
	{"rx_a/link/port", get_invalid, set_rx_a_link_port, RW, NO_POLL, "42820"},
	{"rx_a/link/ip_src", get_invalid, set_rx_a_link_ip_src, RW, NO_POLL, "10.10.10.2"},
	{"rx_a/link/ip_dest", get_invalid, set_rx_a_link_ip_dest, RW, NO_POLL, "10.10.10.1"},
	{"rx_a/link/mac_src", get_invalid, set_rx_a_link_mac_src, RW, NO_POLL, "aa:aa:aa:aa:aa:aa"},
	{"rx_a/link/mac_dest", get_invalid, set_rx_a_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"rx_a/link/pay_len", get_invalid, set_rx_a_link_pay_len, RW, NO_POLL, "1400"},
	{"rx_a/link/ver", get_invalid, set_rx_a_link_ver, RW, NO_POLL, "0"},
	{"rx_a/pwr", get_invalid, set_rx_a_pwr, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/mixer", get_invalid, set_tx_b_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/nco", get_invalid, set_tx_b_rf_dac_nco, RW, NO_POLL, "1475.5"},
	{"tx_b/rf/dac/pap", get_invalid, set_tx_b_rf_dac_pap, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/interp", get_invalid, set_tx_b_rf_dac_interp, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/temp", get_tx_b_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_b/rf/dac/iqerr_gain", get_invalid, set_tx_b_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_b/rf/dac/iqerr_phase", get_invalid, set_tx_b_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_b/rf/freq/val", get_invalid, set_tx_b_rf_freq_val, RW, NO_POLL, "2400"},
	{"tx_b/rf/freq/band", get_tx_b_rf_freq_band, set_invalid, RO, POLL, "1"},
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
	{"tx_b/dsp/interp", get_invalid, set_tx_b_dsp_interp, RW, NO_POLL, "256"},
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
	{"tx_b/link/loopback", get_invalid, set_tx_b_link_loopback, RW, NO_POLL, "0"},
	{"tx_b/link/iface", get_invalid, set_tx_b_link_iface, RW, NO_POLL, "10g"},
	{"tx_b/link/port", get_invalid, set_tx_b_link_port, RW, NO_POLL, "42820"},
	{"tx_b/link/ip_src", get_invalid, set_tx_b_link_ip_src, RW, NO_POLL, "10.10.10.2"},
	{"tx_b/link/ip_dest", get_invalid, set_tx_b_link_ip_dest, RW, NO_POLL, "10.10.10.1"},
	{"tx_b/link/mac_src", get_invalid, set_tx_b_link_mac_src, RW, NO_POLL, "aa:aa:aa:aa:aa:aa"},
	{"tx_b/link/mac_dest", get_invalid, set_tx_b_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"tx_b/link/pay_len", get_invalid, set_tx_b_link_pay_len, RW, NO_POLL, "1400"},
	{"tx_b/link/ver", get_invalid, set_tx_b_link_ver, RW, NO_POLL, "0"},
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
	{"rx_b/rf/freq/band", get_rx_b_rf_freq_band, set_invalid, RO, POLL, "1"},
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
	{"rx_b/dsp/interp", get_invalid, set_rx_b_dsp_interp, RW, NO_POLL, "0"},
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
	{"rx_b/link/loopback", get_invalid, set_rx_b_link_loopback, RW, NO_POLL, "0"},
	{"rx_b/link/iface", get_invalid, set_rx_b_link_iface, RW, NO_POLL, "10g"},
	{"rx_b/link/port", get_invalid, set_rx_b_link_port, RW, NO_POLL, "42820"},
	{"rx_b/link/ip_src", get_invalid, set_rx_b_link_ip_src, RW, NO_POLL, "10.10.10.2"},
	{"rx_b/link/ip_dest", get_invalid, set_rx_b_link_ip_dest, RW, NO_POLL, "10.10.10.1"},
	{"rx_b/link/mac_src", get_invalid, set_rx_b_link_mac_src, RW, NO_POLL, "aa:aa:aa:aa:aa:aa"},
	{"rx_b/link/mac_dest", get_invalid, set_rx_b_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"rx_b/link/pay_len", get_invalid, set_rx_b_link_pay_len, RW, NO_POLL, "1400"},
	{"rx_b/link/ver", get_invalid, set_rx_b_link_ver, RW, NO_POLL, "0"},
	{"rx_b/pwr", get_invalid, set_rx_b_pwr, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/mixer", get_invalid, set_tx_c_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/nco", get_invalid, set_tx_c_rf_dac_nco, RW, NO_POLL, "1475.5"},
	{"tx_c/rf/dac/pap", get_invalid, set_tx_c_rf_dac_pap, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/interp", get_invalid, set_tx_c_rf_dac_interp, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/temp", get_tx_c_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_c/rf/dac/iqerr_gain", get_invalid, set_tx_c_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_c/rf/dac/iqerr_phase", get_invalid, set_tx_c_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_c/rf/freq/val", get_invalid, set_tx_c_rf_freq_val, RW, NO_POLL, "2400"},
	{"tx_c/rf/freq/band", get_tx_c_rf_freq_band, set_invalid, RO, POLL, "1"},
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
	{"tx_c/dsp/interp", get_invalid, set_tx_c_dsp_interp, RW, NO_POLL, "256"},
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
	{"tx_c/link/loopback", get_invalid, set_tx_c_link_loopback, RW, NO_POLL, "0"},
	{"tx_c/link/iface", get_invalid, set_tx_c_link_iface, RW, NO_POLL, "10g"},
	{"tx_c/link/port", get_invalid, set_tx_c_link_port, RW, NO_POLL, "42820"},
	{"tx_c/link/ip_src", get_invalid, set_tx_c_link_ip_src, RW, NO_POLL, "10.10.10.2"},
	{"tx_c/link/ip_dest", get_invalid, set_tx_c_link_ip_dest, RW, NO_POLL, "10.10.10.1"},
	{"tx_c/link/mac_src", get_invalid, set_tx_c_link_mac_src, RW, NO_POLL, "aa:aa:aa:aa:aa:aa"},
	{"tx_c/link/mac_dest", get_invalid, set_tx_c_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"tx_c/link/pay_len", get_invalid, set_tx_c_link_pay_len, RW, NO_POLL, "1400"},
	{"tx_c/link/ver", get_invalid, set_tx_c_link_ver, RW, NO_POLL, "0"},
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
	{"rx_c/rf/freq/band", get_rx_c_rf_freq_band, set_invalid, RO, POLL, "1"},
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
	{"rx_c/dsp/interp", get_invalid, set_rx_c_dsp_interp, RW, NO_POLL, "0"},
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
	{"rx_c/link/loopback", get_invalid, set_rx_c_link_loopback, RW, NO_POLL, "0"},
	{"rx_c/link/iface", get_invalid, set_rx_c_link_iface, RW, NO_POLL, "10g"},
	{"rx_c/link/port", get_invalid, set_rx_c_link_port, RW, NO_POLL, "42820"},
	{"rx_c/link/ip_src", get_invalid, set_rx_c_link_ip_src, RW, NO_POLL, "10.10.10.2"},
	{"rx_c/link/ip_dest", get_invalid, set_rx_c_link_ip_dest, RW, NO_POLL, "10.10.10.1"},
	{"rx_c/link/mac_src", get_invalid, set_rx_c_link_mac_src, RW, NO_POLL, "aa:aa:aa:aa:aa:aa"},
	{"rx_c/link/mac_dest", get_invalid, set_rx_c_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"rx_c/link/pay_len", get_invalid, set_rx_c_link_pay_len, RW, NO_POLL, "1400"},
	{"rx_c/link/ver", get_invalid, set_rx_c_link_ver, RW, NO_POLL, "0"},
	{"rx_c/pwr", get_invalid, set_rx_c_pwr, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/mixer", get_invalid, set_tx_d_rf_dac_mixer, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/nco", get_invalid, set_tx_d_rf_dac_nco, RW, NO_POLL, "1475.5"},
	{"tx_d/rf/dac/pap", get_invalid, set_tx_d_rf_dac_pap, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/interp", get_invalid, set_tx_d_rf_dac_interp, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/temp", get_tx_d_rf_dac_temp, set_invalid, RO, POLL, "0"},
	{"tx_d/rf/dac/iqerr_gain", get_invalid, set_tx_d_rf_dac_iqerr_gain, RW, NO_POLL, "0"},
	{"tx_d/rf/dac/iqerr_phase", get_invalid, set_tx_d_rf_dac_iqerr_phase, RW, NO_POLL, "0"},
	{"tx_d/rf/freq/val", get_invalid, set_tx_d_rf_freq_val, RW, NO_POLL, "2400"},
	{"tx_d/rf/freq/band", get_tx_d_rf_freq_band, set_invalid, RO, POLL, "1"},
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
	{"tx_d/dsp/interp", get_invalid, set_tx_d_dsp_interp, RW, NO_POLL, "256"},
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
	{"tx_d/link/loopback", get_invalid, set_tx_d_link_loopback, RW, NO_POLL, "0"},
	{"tx_d/link/iface", get_invalid, set_tx_d_link_iface, RW, NO_POLL, "10g"},
	{"tx_d/link/port", get_invalid, set_tx_d_link_port, RW, NO_POLL, "42820"},
	{"tx_d/link/ip_src", get_invalid, set_tx_d_link_ip_src, RW, NO_POLL, "10.10.10.2"},
	{"tx_d/link/ip_dest", get_invalid, set_tx_d_link_ip_dest, RW, NO_POLL, "10.10.10.1"},
	{"tx_d/link/mac_src", get_invalid, set_tx_d_link_mac_src, RW, NO_POLL, "aa:aa:aa:aa:aa:aa"},
	{"tx_d/link/mac_dest", get_invalid, set_tx_d_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"tx_d/link/pay_len", get_invalid, set_tx_d_link_pay_len, RW, NO_POLL, "1400"},
	{"tx_d/link/ver", get_invalid, set_tx_d_link_ver, RW, NO_POLL, "0"},
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
	{"rx_d/rf/freq/band", get_rx_d_rf_freq_band, set_invalid, RO, POLL, "1"},
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
	{"rx_d/dsp/interp", get_invalid, set_rx_d_dsp_interp, RW, NO_POLL, "0"},
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
	{"rx_d/link/loopback", get_invalid, set_rx_d_link_loopback, RW, NO_POLL, "0"},
	{"rx_d/link/iface", get_invalid, set_rx_d_link_iface, RW, NO_POLL, "10g"},
	{"rx_d/link/port", get_invalid, set_rx_d_link_port, RW, NO_POLL, "42820"},
	{"rx_d/link/ip_src", get_invalid, set_rx_d_link_ip_src, RW, NO_POLL, "10.10.10.2"},
	{"rx_d/link/ip_dest", get_invalid, set_rx_d_link_ip_dest, RW, NO_POLL, "10.10.10.1"},
	{"rx_d/link/mac_src", get_invalid, set_rx_d_link_mac_src, RW, NO_POLL, "aa:aa:aa:aa:aa:aa"},
	{"rx_d/link/mac_dest", get_invalid, set_rx_d_link_mac_dest, RW, NO_POLL, "ff:ff:ff:ff:ff:ff"},
	{"rx_d/link/pay_len", get_invalid, set_rx_d_link_pay_len, RW, NO_POLL, "1400"},
	{"rx_d/link/ver", get_invalid, set_rx_d_link_ver, RW, NO_POLL, "0"},
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
	{"fpga/link/port", get_invalid, set_fpga_link_port, RW, NO_POLL, "42820"},
	{"fpga/link/ip", get_invalid, set_fpga_link_ip, RW, NO_POLL, "10.10.10.2"},
	{"fpga/link/mac", get_invalid, set_fpga_link_mac, RW, NO_POLL, "aa:aa:aa:aa:aa:aa"},
	{"fpga/link/ver", get_invalid, set_fpga_link_ver, RW, NO_POLL, "0"},
	{"fpga/link/rate", get_invalid, set_fpga_link_rate, RW, NO_POLL, "161"},
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
