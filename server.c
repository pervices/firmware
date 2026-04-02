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
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <sys/select.h>
#include <sys/time.h>
#include "common.h"
#include "mmap.h"
#include "comm_manager.h"
#include "property_manager.h"
#include "array-utils.h"
#include "properties.h"
#include "parser.h"
#include "led.h"
#include "time_it.h"
#include "channels.h"

#if defined(VAUNT) || defined (AVERY)
    //sync_lut is used to deal with Crimson specific hardware limitations
    #include "synth_lut.h"
    #include "variant_config/vaunt_rtm_config.h"
#endif

#include "hal/utils/print_version.h"

/**
 * Services UDP messages
 * This is a helper function to make the code more readable
 * @param udp_comm_fds An array of the file descriptors of the UDP sockets to process
 * @param udp_comm_fds_length The number of elements in udp_comm_fds
 * @param save_profile TODO: figure out what profile is
 * @param save_profile_path TODO: figure out what profile is
 * @param load_profile TODO: figure out what profile is
 * @param load_profile_path TODO: figure out what profile is
 */
void service_udp_requests(int* udp_comm_fds, int udp_comm_fds_length, int save_profile, char* save_profile_path, int load_profile, char* load_profile_path);

int main(int argc, char *argv[]) {

    int ret = 0;

    uint8_t load_profile = 0;
    uint8_t save_profile = 0;

    uint8_t options = 0;

    char load_profile_path[MAX_PROP_LEN];
    char save_profile_path[MAX_PROP_LEN];

    // The port the host listens on for TCP connections
    const int tcp_listener_port = 42798;

    const int udp_port_nums[] = {
        // /* UDP management port */
        // 42799,
        // /* Crimson ports */
        // 42800,
        // 42801,
        // 42802,
        // 42803,
        // 42804,
        // 42805,
        // 42806,
        // 42807,
    };

    // File descriptor of the listener used to start TCP connections
    int tcp_listener_fd = -1;

    // UDP fds are stored after TCP fds
    int udp_comm_fds[ARRAY_SIZE(udp_port_nums)];

    extern int verbose;
    verbose = 0;

    ret = mmap_init();
    if (EXIT_SUCCESS != ret) {
        PRINT(ERROR, "mmap_init failed\n");
        return ret;
    }
    atexit(mmap_fini);

    // Check for firmware version
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[i], "-d") == 0) {
            verbose++;
        }
    }
    
#ifdef TATE_NRNT
    // Linux will freeze if attempting to boot an hps only image
    if(is_hps_only()) {
        PRINT(ERROR, "HPS only FPGA image detected. Cancelling server boot. The unit will not run\n");
        return 1;
    }
#endif

#if defined(TATE_NRNT)
    PRINT(INFO, "Starting Cyan server\n");
#elif defined(LILY)
    PRINT(INFO, "Starting Lily server\n");
#elif defined(VAUNT)
    PRINT(INFO, "Starting Crimson server\n");
#elif defined (AVERY)
    PRINT(INFO, "Starting Calamine server\n");
#else
    #error "You must specify either ( VAUNT | AVERY | TATE_NRNT | LILY ) when compiling this project."
#endif


    set_led_state(LED_STATE_INIT);
    
    PRINT(INFO, "Started LEDs\n");

    // Check for an argument for debug mode
    if (argc >= 2) {
        if (strcmp(argv[1], "-d") == 0)
            options |= SERVER_DEBUG_OPT;
    }

    // Initialize network communications for TCP ports
    int init_tcp_comm_r = init_tcp_comm(&tcp_listener_fd, tcp_listener_port);
    if (init_tcp_comm_r < 0) {
        PRINT(ERROR, "Initializing TCP management socket failed with error code %s\n", strerror(errno));
        return RETURN_ERROR_COMM_INIT;
    }

    // Initialize network communications for UDP ports
    for (int i = 0; i < ARRAY_SIZE(udp_port_nums); i++) {
        int init_udp_comm_r = init_udp_comm(&udp_comm_fds[i], udp_port_nums[i]);
        if (init_udp_comm_r < 0) {
            PRINT(ERROR, "Initializing UDP management socket failed with error code %s\n", strerror(errno));
            return RETURN_ERROR_COMM_INIT;
        }
    }

    PRINT(INFO, "Network init done\n");

    // Initialize the properties, which is implemented as a Linux file structure
    const int t0 = time_it();
    init_property(options);
    const int t1 = time_it();

    PRINT(INFO, "boot time %d\n", t1 - t0);

#if defined(VAUNT) || defined(AVERY)
    // Perform autocalibration of the frequency synthesizers
    // N.B. this must be done after init_property() because uart init is mixed
    // in with it for some reason
    atexit(synth_lut_disable_all);
    synth_lut_enable_all_if_calibrated();
#elif defined(TATE_NRNT) || defined(LILY)
    // No-op
#else
    #error "You must specify either ( VAUNT | AVERY | TATE_NRNT | LILY ) when compiling this project."
#endif

    // Pass the profile pointers down to properties.c
    pass_profile_pntr_manager(&load_profile, &save_profile, load_profile_path,
                              save_profile_path);

#if defined(VAUNT) || defined(AVERY)
    // No-op
#elif defined(TATE_NRNT) || defined(LILY)
    /* Final checks
     * 1. Set time board to pulsed mode.
     * 2. Ensure that time board is locked. I think that we are not permitted to reset the tim eboard anymore because that would mess up the TX JESD part of the FPGA, so it would be bad id the time board is not locked. Double check this.
     * 3. Ensure TX boards and RX board JESD links are good. This has to be a single step, because the process for fixing any bad JESD links (for either type of board) involves resetting JESD part of FPGA, which might cause other boards to lose JESD link.
      */

    // 1. set the time board back to pulsed sysref mode
    set_property("time/sync/sysref_mode","pulsed");
     
// 2. check that time board plls are locked
    if (property_good("time/status/status_good") != 1) {
        PRINT(ERROR,"TIME BOARD PLLs UNLOCKED: Stopping server.\n");
        set_led_state(LED_STATE_ERROR);
        usleep(10000000); // wait 10 seconds to make it clear that the server has failed, in case auto-retry is enabled
        abort();
    }
#else
    #error "You must specify either ( VAUNT | AVERY | TATE_NRNT | LILY ) when compiling this project."
#endif

// Let systemd know that the server has come up
system("systemd-notify --ready");

// Let the user know the server is ready to receive commands
#if defined(TATE_NRNT)
    PRINT(INFO, "Cyan server is up\n");
#elif defined(LILY)
    PRINT(INFO, "Lily server is up\n");
#elif defined(VAUNT)
    PRINT(INFO, "Crimson server is up\n");
#elif defined(AVERY)
    PRINT(INFO, "Calamine server is up\n");
#else
    #error "You must specify either ( VAUNT | AVERY | TATE_NRNT | LILY ) when compiling this project."
#endif

    set_led_state(LED_STATE_READY);

    // Main loop, look for commands, if exists, service it and respond
    for (;;) {
        service_udp_requests(udp_comm_fds, ARRAY_SIZE(udp_port_nums), save_profile, save_profile_path, load_profile, load_profile_path);

        // TODO service TCP requests
    }

    // Close network sockets
    close(tcp_listener_fd);

    for (int i = 0; i < ARRAY_SIZE(udp_comm_fds); i++) {
        close(udp_comm_fds[i]);
    }

    return 0;
}

void service_udp_requests(int* udp_comm_fds, int udp_comm_fds_length, int save_profile, char* save_profile_path, int load_profile, char* load_profile_path) {

    // Set up read file descriptor set for select(2)
    fd_set rfds;
    FD_ZERO(&rfds);

    int highest_fd = -1;
    for (int i = 0; i < udp_comm_fds_length; i++) {
        FD_SET(udp_comm_fds[i], &rfds);
        if (udp_comm_fds[i] >= highest_fd) {
            highest_fd = udp_comm_fds[i];
        }
    }

    int inotify_fd = get_inotify_fd();

    FD_SET(inotify_fd, &rfds);
    if (inotify_fd >= highest_fd) {
        highest_fd = inotify_fd;
    }

    // TODO: rename select_r to something better
    int select_r = select(highest_fd + 1, &rfds, NULL, NULL, NULL);

    // Buffer used for read/write
    uint8_t buffer[UDP_PAYLOAD_LEN];

    switch (select_r) {

        case 0:
        case -1:

            if (0 == select_r) {
                // Timeout has expired (although we have provided no timeout)
                PRINT(VERBOSE, "select timed-out\n");
            } else {
                PRINT(VERBOSE, "select failed on fd %d: %s (%d)\n", -1,
                        strerror(errno), errno);
            }

            return;
            break;

        default:
            // Service other management requests
            for (int i = 0; i < udp_comm_fds_length; i++) {

                if (!FD_ISSET(udp_comm_fds[i], &rfds)) {
                    continue;
                }

                // Struct used to store where a packet was received from so it can be replied to
                struct sockaddr_in sa;
                socklen_t sa_len;

                sa_len = sizeof(sa);
                memset(buffer, 0, sizeof(buffer));
                int recv_ret = recvfrom(udp_comm_fds[i], buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr *)&sa, &sa_len);
                if (recv_ret < 0) {
                    PRINT(ERROR, "recvfrom failed: %s (%d)\n", strerror(errno),
                            errno);
                    select_r--;
                    // continue;
                }

                cmd_t cmd = {0};

                if (RETURN_SUCCESS != parse_cmd(&cmd, buffer)) {

                    PRINT(ERROR, "failed to parse command\n");
                    select_r--;
                    // continue;
                }

                cmd.status = CMD_SUCCESS;

                if (cmd.op == OP_GET) {
                    if (get_property(cmd.prop, cmd.data, MAX_PROP_LEN) !=
                        RETURN_SUCCESS) {
                        cmd.status = CMD_ERROR;
                    }
                } else {
                    if (set_property(cmd.prop, cmd.data) != RETURN_SUCCESS) {
                        cmd.status = CMD_ERROR;
                    }
                }

                build_cmd(&cmd, buffer, UDP_PAYLOAD_LEN);
                int send_ret = sendto(udp_comm_fds[i], buffer, strlen((char *)buffer), 0,
                                (struct sockaddr *)&sa, sa_len);
                if (send_ret < 0) {
                    PRINT(ERROR, "sendto failed: %s (%d)\n", strerror(errno),
                            errno);
                    select_r--;
                    continue;
                }

                select_r--;
            }

            // Service inotify
            if (FD_ISSET(inotify_fd, &rfds)) {

                // Check if any files/properties have been modified through
                // shell
                check_property_inotifies();

                // Check if any of the writes/reads were made to save/load
                // profiles priority given to saving profile
                if (save_profile) {
                    save_properties(save_profile_path);
                    save_profile = 0;
                }

                if (load_profile) {
                    load_properties(load_profile_path);
                    load_profile = 0;
                }

                select_r--;
            }

            if (0 != select_r) {
                // Sanity check: this should be zero after servicing fd's!!
                PRINT(VERBOSE, "did not service all channels\n");
            }

            break;
    }
}
