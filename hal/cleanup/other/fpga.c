
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
	uint32_t old_val;
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
	old_val = ((uint64_t)old_val2 << 32) | (uint64_t)old_val1;

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
	old_val = ((uint64_t)old_val2 << 32) | (uint64_t)old_val1;


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

