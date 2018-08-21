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

static int hdlr_tx_sync (const char* data, char* ret) {
	uint32_t old_val;

	// toggle the bit sys0[6]
	read_hps_reg ( "sys0", &old_val);
	write_hps_reg( "sys0", old_val | 0x40);
	write_hps_reg( "sys0", old_val & (~0x40));

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
