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

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h>
#include "common.h"
#include "comm_manager.h"
#include "property_manager.h"
#include "parser.h"

#define ENET_DEV "eth0"

// timers for polling
static struct timeval tstart;
static struct timeval tend;

// return 1 if timeout, 0 if not
static uint8_t timeout(uint32_t timeout) {
	gettimeofday(&tend, NULL);
	if ( ((tend.tv_usec + 1000000 * tend.tv_sec)
		- (tstart.tv_usec + 1000000 * tstart.tv_sec) - 26) > timeout)
		return 1;
	else
		return 0;
}

// profile flags, read in this file, triggered in properties.c
uint8_t load_profile = 0;
uint8_t save_profile = 0;
char load_profile_path[MAX_PROP_LEN];
char save_profile_path[MAX_PROP_LEN];

// execution options
uint8_t options = 0;

// comm ports
int comm_fds[num_udp_ports] = {0};
int port_nums[num_udp_ports] = {
	UDP_MGMT_PORT,
	UDP_RXA_PORT,
	UDP_RXB_PORT,
	UDP_RXC_PORT,
	UDP_RXD_PORT,
	UDP_TXA_PORT,
	UDP_TXB_PORT,
	UDP_TXC_PORT,
	UDP_TXD_PORT };

// main loop
int main(int argc, char *argv[]) {
	int ret = 0;
	int i = 0;
	cmd_t cmd;

	// check for an argument for debug mode
	if (argc >= 2) {
		if (strcmp(argv[1], "-d") == 0)
			options |= SERVER_DEBUG_OPT;
	}

	// Initialize network communications for each port
	for( i = 0; i < num_udp_ports; i++) {
		if ( init_udp_comm(&(comm_fds[i]), ENET_DEV, port_nums[i], 0) < 0 ) {
			printf("ERROR: %s, cannot initialize network %s\n", __func__, ENET_DEV);
			return RETURN_ERROR_COMM_INIT;
		}
	}

	// Buffer used for read/write
	uint8_t buffer[UDP_PAYLOAD_LEN];
	uint16_t received_bytes = 0;

	// initialize the properties, which is implemented as a Linux file structure
	init_property(options);

	// pass the profile pointers down to properties.c
	pass_profile_pntr_manager(&load_profile, &save_profile, load_profile_path, save_profile_path);

	// let the user know the server is ready to receive commands
	printf("- Crimson server is up.\n");

	i = 0;
	// main loop, look for commands, if exists, service it and respond
	while (1) {
		// prevent busy wait, taking up too much CPU resources
		usleep(1);

		// poll for all status properties every 3s
		if (timeout(3000000) == 1) {
			//update_status_properties();
			gettimeofday(&tstart, NULL);
		}

		// check for input commands from UDP
		if (recv_udp_comm(comm_fds[i], buffer, &received_bytes, UDP_PAYLOAD_LEN) >= 0) {
			if (parse_cmd(&cmd, buffer) != RETURN_SUCCESS) break;

			// Debug print
			/*printf("\tRecevied:\n");
			printf("\tSeq:    %"PRIu32"\n", cmd.seq);
			printf("\tOp:     %i\n", cmd.op);
			printf("\tStatus: %i\n", cmd.status);
			printf("\tProp:   %s\n", cmd.prop);
			printf("\tData:   %s\n", cmd.data);*/

			cmd.status = CMD_SUCCESS;

			if (cmd.op == OP_GET) {
				if (get_property(cmd.prop, cmd.data, MAX_PROP_LEN) != RETURN_SUCCESS)
					cmd.status = CMD_ERROR;
			} else {
				if (set_property(cmd.prop, cmd.data) != RETURN_SUCCESS)
					cmd.status = CMD_ERROR;
			}

			build_cmd(&cmd, buffer, UDP_PAYLOAD_LEN);

			send_udp_comm(comm_fds[i], buffer, strlen((char*)buffer));
		}

		// check if any files/properties have been modified through shell
		check_property_inotifies();

		// check if any of the writes/reads were made to save/load profiles
		// priority given to saving profile
		if (save_profile) {
			save_properties(save_profile_path);
			save_profile = 0;
		}

		if (load_profile) {
			load_properties(load_profile_path);
			load_profile = 0;
		}

		// increment to service the other ports
		i = (i + 1) % num_udp_ports;
	}

	// close the file descriptors
	for( i = 0; i < num_udp_ports; i++) {
		close_udp_comm(comm_fds[i]);
	}

	return ret;
}
