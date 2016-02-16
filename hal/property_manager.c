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

#include <unistd.h>
#include "common.h"
#include "properties.h"
#include "comm_manager.h"
#include "property_manager.h"
#include "mmap.h"	// shouldn't need to include this, this is here for errata fixing
#include "uart.h"

#define EVENT_SIZE 	(sizeof(struct inotify_event))
#define EVENT_BUF_LEN 	( 1024 * (EVENT_SIZE + 16) )

// UART communication manager's file descriptor
//static int uart_comm_fd;
static int uart_synth_comm_fd;
static int uart_tx_comm_fd;
static int uart_rx_comm_fd;

// Inotify's file descriptor
static int inotify_fd;

// Options passed from server.c
static uint8_t _options = 0;

// Helper function to write to property
static void write_to_file(const char* path, const char* data) {
	FILE* fd;
	if ( !(fd = fopen(path, "w")) ) {
		PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
		return;
	}
	fprintf(fd, "%s", data);
	fclose(fd);

	PRINT(VERBOSE, "wrote to file: %s (%s)\n", path, data);
}

// Helper function to read to property
static void read_from_file(const char* path, char* data, size_t max_len) {
	FILE* fd;
	if ( !(fd = fopen(path, "r")) ) {
		PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
		return;
	}
	fgets(data, max_len, fd);
	fclose(fd);
	
	// remove the new line at the end of the file
	size_t pos = 0;
	while(data[pos] != '\n' && data[pos] != '\0') pos++;
	data[pos] = '\0';

	PRINT(VERBOSE, "read from file: %s (%s)\n", path, data);
}

// Helper function to make properties
static inline void make_prop(prop_t* prop) {
	char cmd [MAX_PATH_LEN];
	char path[MAX_PATH_LEN];

	// mkdir -p /home/root/state/*
	strcpy(cmd, "mkdir -p ");
	strcat(cmd, get_abs_dir(prop, path));
	system(cmd);
	//PRINT( VERBOSE,"executing: %s\n", cmd);

	// touch /home/root/state/*
	strcpy(cmd, "touch ");
	strcat(cmd, get_abs_path(prop, path));
	system(cmd);
	//PRINT( VERBOSE,"executing: %s\n", cmd);

	// if read only property, change permissions
	if (prop -> permissions == RO) {
		// chmod a-w /home/root/state/*
		strcpy(cmd, "chmod 0444 ");
		strcat(cmd, get_abs_path(prop, path));
		system(cmd);
	} else if (prop -> permissions == WO) {
		// chmod a-r /home/root/state/*
		strcpy(cmd, "chmod 0222 ");
		strcat(cmd, get_abs_path(prop, path));
		system(cmd);
	}
}

// Helper function to add the property to inotify
// crimson continues if this fails, but will give an error
static void add_prop_to_inotify(prop_t* prop) {
	char path[MAX_PATH_LEN];

	// check if RO property
	if (prop -> permissions != RO) {
		prop -> wd = inotify_add_watch( inotify_fd,
			get_abs_path(prop, path), IN_CLOSE_WRITE);
	}

	if (prop -> wd < 0)
		PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
}

// Helper function to call power-on reset values
static void init_prop_val(prop_t* prop) {
	char path [MAX_PATH_LEN];
	memset(path, 0, MAX_PATH_LEN);

	// exceptions for values that must persist through hard resets
	if (	strcmp(prop -> path, "fpga/link/net/hostname") == 0 ||
		strcmp(prop -> path, "fpga/link/net/ip_addr") == 0  ||
		strcmp(prop -> path, "save_config") == 0 ||
		strcmp(prop -> path, "load_config") == 0
	) {
		return;
	}

	// if not WO property
	if  (prop -> permissions != WO) {
		write_to_file(get_abs_path(prop, path), prop -> def_val);
	}
}

// Helper function for building a tree in the home directory
static void build_tree(void) {
	PRINT(VERBOSE, "Building tree, %i properties found\n", get_num_prop());

	size_t i;
	for (i = 0; i < get_num_prop(); i++) {
		make_prop(get_prop(i));
		add_prop_to_inotify(get_prop(i));
		init_prop_val(get_prop(i));
		PRINT(VERBOSE, "made prop: %s wd: %i\n", get_prop(i) -> path, get_prop(i) -> wd);
	}

	// force property initofy check (writing of defaults) after init
	check_property_inotifies();

	PRINT( VERBOSE, "Last wd val: %i\n", get_prop(i-1) -> wd);
	PRINT( VERBOSE, "Done building tree\n");
}

// Initialize handler functions
int init_property(uint8_t options) {
	// uart transactions
	PRINT( VERBOSE,"Initializing UART\n");

	// save the options
	_options = options;
	set_uart_debug_opt(options);
	set_mem_debug_opt(options);

	//if ( init_uart_comm(&uart_comm_fd, UART_DEV, 0) < 0 ) {
	//	PRINT(ERROR, "%s, cannot initialize uart %s\n", __func__, UART_DEV1);
	//	return RETURN_ERROR_COMM_INIT;
	//}
	
	if ( init_uart_comm(&uart_synth_comm_fd, UART_SYNTH, 0) < 0 ) {
		PRINT(ERROR, "%s, cannot initialize uart %s\n", __func__, UART_SYNTH);
		return RETURN_ERROR_COMM_INIT;
	}

	if ( init_uart_comm(&uart_tx_comm_fd, UART_TX, 0) < 0 ) {
		PRINT(ERROR, "%s, cannot initialize uart %s\n", __func__, UART_TX);
		return RETURN_ERROR_COMM_INIT;
	}
	
	if ( init_uart_comm(&uart_rx_comm_fd, UART_RX, 0) < 0 ) {
		PRINT(ERROR, "%s, cannot initialize uart %s\n", __func__, UART_RX);
		return RETURN_ERROR_COMM_INIT;
	}
	
	PRINT(VERBOSE, "init_uart_comm(): UART connections up\n");
	PRINT(VERBOSE, "Initializing Inotify\n");

	// inotify
	if ( (inotify_fd = inotify_init()) < 0) {
		PRINT( ERROR,"%s, cannot initialize inotify\n", __func__);
		return RETURN_ERROR_INOTIFY;
	}

	// set inotify to non-blocking
	fcntl(inotify_fd, F_SETFL, fcntl(inotify_fd, F_GETFL) | O_NONBLOCK);

	// pass the uart handler to the property handlers
	pass_uart_synth_fd(uart_synth_comm_fd);
	pass_uart_tx_fd(uart_tx_comm_fd);
	pass_uart_rx_fd(uart_rx_comm_fd);

	PRINT( INFO,"Building the property tree\n");
	build_tree();

	return RETURN_SUCCESS;
}

// non-standard set property (file modification)
void check_property_inotifies(void) {
	uint8_t buf[EVENT_BUF_LEN];
	char prop_data[MAX_PROP_LEN];
	char prop_ret[MAX_PROP_LEN];
	char path[MAX_PATH_LEN];
	ssize_t len = read(inotify_fd, buf, EVENT_BUF_LEN);

	ssize_t i = 0;
	while (i < len) {
		// gets the event structure
		struct inotify_event* event = (struct inotify_event*) &buf[i];
		prop_t* prop = get_prop_from_wd(event -> wd);

		// check if prop exists, prop will not exist if concurrent modifications were made to the file while in this loop
		if (event -> mask & IN_CLOSE_WRITE && prop) {
			PRINT( VERBOSE,"Property located at %s has been modified, executing handler\n", prop -> path);

			// empty out the buffers
			memset(prop_data, 0, MAX_PROP_LEN);
			memset(prop_ret,  0, MAX_PROP_LEN);

			// read the change from the file
			read_from_file(get_abs_path(prop, path), prop_data, MAX_PROP_LEN);
			strcpy(prop_ret, prop_data);

			PRINT( DEBUG,"Inotify handler [Prop: %s Data: %s]\n", prop -> path, prop_data);
			prop -> handler(prop_data, prop_ret);

			// if the return value didn't change, don't write to file again
			if ( strcmp(prop_ret, prop_data) != 0) {
				// temperarily remove property from inotify so the file update won't trigger another inotify event
				if (inotify_rm_watch( inotify_fd, prop -> wd) < 0) {
					PRINT( ERROR, "%s(), %s\n", __func__, strerror(errno));
				}
				PRINT( VERBOSE,"Removed inotify, wd: %i\n", prop -> wd);

				// write output of handler to property
				write_to_file(get_abs_path(prop, path), prop_ret);

				// re-add property to inotify
				prop -> wd = inotify_add_watch( inotify_fd, get_abs_path(prop, path), IN_CLOSE_WRITE);
				if (prop -> wd < 0) {
					PRINT( ERROR, "%s(), %s\n", __func__, strerror(errno));
				}
				PRINT( VERBOSE,"Re-added to inotify, wd: %i\n", prop -> wd);
			}
		}

		i += sizeof(struct inotify_event) + event -> len;
	}
}

// Save properties to file
int save_properties(const char* file) {
	// open the file, if file exists, overwrite the file
	FILE* fout = fopen(file, "w");
	if (!fout) return RETURN_ERROR;

	// temp buffer to store the property values
	char prop_val [MAX_PROP_LEN];

	// loop through all properties and write them to file
	size_t i;
	for (i = 0; i < get_num_prop(); i++) {
		prop_t* cur_prop = get_prop(i);
		if (cur_prop && cur_prop -> permissions != WO) {
			get_property(cur_prop -> path, prop_val, MAX_PROP_LEN);
			fprintf(fout, "%s,%s\n", cur_prop -> path, prop_val);
		}
	}

	// close the file
	fclose(fout);

	return RETURN_SUCCESS;
}

// Load properties from file
int load_properties(const char* file) {
	PRINT( DEBUG,"loading properties from %s!\n", file);

	// open the file for reading
	FILE* fin = fopen(file, "r");
	if (!fin) return RETURN_ERROR;

	// temp buffer to store the property values
	char prop [MAX_PROP_LEN] = {0};

	// loop through all properties, if there are incompatibilities, error out
	size_t i;
	for (i = 0; i < get_num_prop(); i++) {
		// read from file and update the property table
		if (fscanf(fin, "%s", prop) == EOF) {
			break;
		}

		// if property exceeded the MAX_PROP_LEN
		prop[MAX_PROP_LEN - 1] = 0;

		// divide up the property and value
		char* prop_val = strchr(prop, ',');
		*prop_val = 0;
		prop_val++;

		// get pointer to current property
		prop_t* cur_prop = get_prop_from_cmd(prop);
		if (!cur_prop) {
			PRINT( ERROR,"invalid property %s\n", prop);
			continue;
		}
		strcpy(cur_prop -> def_val, prop_val);

		// write the current property to device
		init_prop_val(get_prop(i));
	}

	// close the file
	fclose(fin);

	return RETURN_SUCCESS;
}

// Standard get property
int get_property(const char* prop, char* data, size_t max_len) {
	PRINT( VERBOSE,"%s(): %s\n", __func__, prop);

	memset(data, 0, max_len);
	char path [MAX_PATH_LEN];
	prop_t* temp = get_prop_from_cmd(prop);

	// check if valid property
	if (!temp) {
		PRINT( ERROR,"Property: %s does not exist\n", prop);
		return RETURN_ERROR_SET_PROP;		
	}

	// check if WO property
	if (temp -> permissions == WO) {
		PRINT( ERROR,"Cannot invoke a get on this property\n");
		return RETURN_ERROR_GET_PROP;
	}

	read_from_file(get_abs_path(temp, path), data, max_len);
	return RETURN_SUCCESS;
}

// standard set property
int set_property(const char* prop, const char* data) {
	PRINT( VERBOSE,"%s(): %s\n", __func__, prop);

	char path [MAX_PATH_LEN];
	prop_t* temp = get_prop_from_cmd(prop);

	// check if valid property
	if (!temp) {
		PRINT( ERROR,"Property: %s does not exist\n", prop);
		return RETURN_ERROR_SET_PROP;		
	}

	// check if RO property
	if (temp -> permissions == RO) {
		PRINT( ERROR,"Cannot invoke a set on this property\n");
		return RETURN_ERROR_SET_PROP;
	}

	// enable channel if it has not been enabled yet
	// (enabling the channel later will erase the current channels, so enable now)
	char root[MAX_PATH_LEN] = {0};
	get_root(temp, root);

	// check if the root is either rx or tx
	root[2] = 0;	// temporarily end string at '_'
	if ( !strcmp(root, "rx") || !strcmp(root, "tx") ) {
		root[2] = '_';				// re-insert '_'
		strcat(root + strlen(root), "/pwr");
		prop_t* pwr_prop = get_prop_from_cmd(root);
		char pwr_path[MAX_PATH_LEN] = {0};

		write_to_file(get_abs_path(pwr_prop, pwr_path), "1");
	}

	write_to_file(get_abs_path(temp, path), data);
	check_property_inotifies();

	return RETURN_SUCCESS;
}

// Pass the pointers for load/saving profiles flags
void pass_profile_pntr_manager(uint8_t* load, uint8_t* save, char* load_path, char* save_path) {
	pass_profile_pntr_prop(load, save, load_path, save_path);
}
