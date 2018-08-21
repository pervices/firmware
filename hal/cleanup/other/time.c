
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
		strcpy(buf, "HMC -h 1 -b 1\r");
                send_uart_comm(uart_synth_fd, (uint8_t*)buf, strlen(buf));
	} else if (strcmp(data, "LVPECL") == 0) {
		strcpy(buf, "HMC -h 1 -b 0\r");
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

