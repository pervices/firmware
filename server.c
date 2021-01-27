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
#include "synth_lut.h"
#include "led.h"
#include "time_it.h"
#include "channels.h"

int main(int argc, char *argv[]) {

    int ret = 0;
    int i = 0;
    cmd_t cmd = {0};

    uint8_t load_profile = 0;
    uint8_t save_profile = 0;

    uint8_t options = 0;

    char load_profile_path[MAX_PROP_LEN];
    char save_profile_path[MAX_PROP_LEN];
    
    char prop_path[32];
    char tmp_char;
    int count_bad = 0;

    const int port_nums[] = {
        /* UDP management port */
        42799,
        /* Crimson ports */
        42800,
        42801,
        42802,
        42803,
        42804,
        42805,
        42806,
        42807,
    };

    int comm_fds[ARRAY_SIZE(port_nums)];

    const char *const enet_dev = "eth0";

    extern int verbose;
    verbose = 0;

    fd_set rfds;
    ret = mmap_init();
    if (EXIT_SUCCESS != ret) {
        PRINT(ERROR, "mmap_init failed\n");
        return ret;
    }
    atexit(mmap_fini);

    // Check for firmware version
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            printf("Branch: %s\n", VERSIONGITBRANCH);
            printf("Revision: %s\n", VERSIONGITREVISION);
            printf("Date: %s UTC\n", VERSIONDATE);

            uint32_t ver39_32, ver31_0;
            uint64_t fpgaver;
            read_hps_reg("sys3", &ver39_32);
            read_hps_reg("sys4", &ver31_0);
            fpgaver = (((uint64_t)ver39_32 & 0xff) << 32) |
                      (((uint64_t)ver31_0 & 0xffffffff) << 0);
            printf("FPGA: %llx\n", fpgaver);

            return 0;
        }
        if (strcmp(argv[i], "-d") == 0) {
            verbose++;
        }
    }

    PRINT(INFO, "Starting Cyan server\n");

    server_init_led();
    
    PRINT(INFO, "Started LEDs\n");

    // Check for an argument for debug mode
    if (argc >= 2) {
        if (strcmp(argv[1], "-d") == 0)
            options |= SERVER_DEBUG_OPT;
    }

    PRINT(INFO, "Checked for debug arg\n");

    // Initialize network communications for each port
    for (i = 0; i < ARRAY_SIZE(port_nums); i++) {
        if (init_udp_comm(&(comm_fds[i]), enet_dev, port_nums[i], 0) < 0) {
            PRINT(ERROR, "%s, cannot initialize network %s\n", __func__,
                  enet_dev);
            return RETURN_ERROR_COMM_INIT;
        }
    }

    PRINT(INFO, "Network init done\n");

    // Buffer used for read/write
    uint8_t buffer[UDP_PAYLOAD_LEN];
    int highest_fd = -1;
    int inotify_fd;
    int ret2;
    struct sockaddr_in sa;
    socklen_t sa_len;

    // Initialize the properties, which is implemented as a Linux file structure
    const int t0 = time_it();
    init_property(options);
    const int t1 = time_it();

    printf("boot time %d\n", t1 - t0);

    inotify_fd = get_inotify_fd();

    // Perform autocalibration of the frequency synthesizers
    // N.B. this must be done after init_property() because uart init is mixed
    // in with it for some reason
    //atexit(synth_lut_disable_all);
    //synth_lut_enable_all_if_calibrated();

    // Pass the profile pointers down to properties.c
    pass_profile_pntr_manager(&load_profile, &save_profile, load_profile_path,
                              save_profile_path);
    
    // TODO find a way to flush uart buffers
    
    // check that time board plls are locked
    count_bad = 1;
    property_good("time/status/status_good");
    while (count_bad == 1) {
        usleep(5000000); // wait
        if (property_good("time/status/status_good") == 1) {
            count_bad = 0;
        } else {        
            PRINT(ERROR,"rebooting time\n");
            set_property("/var/cyan/state/time/reboot","1");
            usleep(5000000); // wait
            PRINT(INFO,"FPGA: reset\n");
            set_property("/var/cyan/state/fpga/reset","3");
            usleep(5000000); // wait
            PRINT(INFO,"sysref pulse attempt\n");
            set_property("/var/cyan/state/time/sync/lmk_sync_tgl_jesd","1");
            usleep(5000000); // wait
            for (i = 0; i < 16; i++) {
                strcpy(&prop_path,"/var/cyan/state/tx/");
                tmp_char = i + 'a';
                strcat(&prop_path,&tmp_char);
                strcat(&prop_path,"/reboot");
                PRINT(INFO,"PROPERTY: %s\n",prop_path);
                set_property(&prop_path,"1");
                usleep(50000); // wait
            }
        }
    }

    // check that tx boards are all good
//    for (i = 0; i < 16; i++) {
//        strcpy(&prop_path,"tx/");
//        tmp_char = i + 'a';
//        strcat(&prop_path,&tmp_char);
//        strcat(&prop_path,"/jesd_status");
//        PRINT(INFO,"PROPERTY: %s\n",prop_path);
//        if (property_good(&prop_path) != 1) {
            // if any is not good reboot that board
//            count_bad += 1;
//            PRINT(ERROR,"JESD: rebooting tx %c\n",tmp_char);
//            strcpy(&prop_path,"/var/cyan/state/tx/");
//            strcat(&prop_path,&tmp_char);
//            strcat(&prop_path,"/reboot");
//            PRINT(INFO,"PROPERTY: %s\n",prop_path);
//            set_property(&prop_path,"1");
//        }
//        usleep(500000); // wait for uart to be ready
//    }
//    PRINT(INFO, "JESD: first pass %i boards bad\n",count_bad);
//    if (count_bad > 0) { // if any had to be rebooted confirm that they came up properly
//        count_bad = 0;
//        usleep(10000000); // wait to ensure that any rebooted boards are up
//        // TODO find a way to flush uart buffer
//        for (i = 0; i < 16; i++) {
//            strcpy(&prop_path,"tx/");
//            tmp_char = i + 'a';
//            strcat(&prop_path,&tmp_char);
//            strcat(&prop_path,"/jesd_status");
//            PRINT(INFO,"PROPERTY: %s\n",prop_path);
//            if (property_good(&prop_path) != 1) {
//                count_bad += 1;
//            }
//            usleep(1000000); // wait for uart to be ready
//         }
//         PRINT(INFO, "JESD: %i boards still bad after reboot\n",count_bad);
//    }
//    if (count_bad > 0) { // if any were still bad try resetting fpga then all boards
//        set_property("fpga/reset","3");
//        for (i = 0; i < 16; i++) {
//            PRINT(INFO,"FPGA: reset\n"); // reset fpga jesd
//            set_property("/var/cyan/state/fpga/reset","3");
//            usleep(5000000); // wait
//            //issue sysref
//            PRINT(INFO,"sysref pulse attempt\n");
//            set_property("/var/cyan/state/time/sync/lmk_sync_tgl_jesd","1");
//            usleep(5000000); // wait
//            for (i = 0; i < 16; i++) {
//                count_bad += 1;
//                strcpy(&prop_path,"/var/cyan/state/tx/");
//                tmp_char = i + 'a';
//                strcat(&prop_path,&tmp_char);
//                strcat(&prop_path,"/reboot");
//                PRINT(INFO,"PROPERTY: %s\n",prop_path);
//                set_property(&prop_path,"1");
//                usleep(50000); // wait
//            }
//        }
//        usleep(10000000); // wait for all boards to reboot
//        count_bad = 0;
//        for (i = 0; i < 16; i++) { // then check that all of the tx boards came up
//            strcpy(&prop_path,"tx/");
//            tmp_char = i + 'a';
//            strcat(&prop_path,&tmp_char);
//            strcat(&prop_path,"/jesd_status");
//            PRINT(INFO,"PROPERTY: %s\n",prop_path);
//            if (property_good(&prop_path) != 1) {
//                count_bad += 1;
//            }
//            usleep(500000); // wait for uart to be ready
//        }
//        PRINT(INFO, "JESD: Final count %i boards bad\n",count_bad);
//    }
//    if (count_bad > 0) { // at this point if a board is still bad we have to restart cyan
//        write_hps_reg("led0", 0); //turn off the bottom led so that the user knows the server has failed
//        abort();
//    }
//
    
    // Let the user know the server is ready to receive commands
    PRINT(INFO, "Cyan server is up\n");
    
    //Poll all UART RX buffers, as they may have stale data.
    

#if 1
    server_ready_led();
    PRINT(INFO, "SERVER READY LED\n");
    // Main loop, look for commands, if exists, service it and respond
    for (;;) {

        // Set up read file descriptor set for select(2)
        FD_ZERO(&rfds);
        for (i = 0; i < ARRAY_SIZE(port_nums); i++) {
            FD_SET(comm_fds[i], &rfds);
            if (comm_fds[i] >= highest_fd) {
                highest_fd = comm_fds[i];
            }
        }
        FD_SET(inotify_fd, &rfds);
        if (inotify_fd >= highest_fd) {
            highest_fd = inotify_fd;
        }

        ret = select(highest_fd + 1, &rfds, NULL, NULL, NULL);

        switch (ret) {

        case 0:
        case -1:

            if (0 == ret) {
                // Timeout has expired (although we have provided no timeout)
                PRINT(VERBOSE, "select timed-out\n");
            } else {
                PRINT(VERBOSE, "select failed on fd %d: %s (%d)\n", -1,
                      strerror(errno), errno);
            }

            continue;
            break;

        default:

            // Service other management requests
            for (i = 0; i < ARRAY_SIZE(port_nums); i++) {

                if (!FD_ISSET(comm_fds[i], &rfds)) {
                    continue;
                }

                sa_len = sizeof(sa);
                memset(buffer, 0, sizeof(buffer));
                ret2 = recvfrom(comm_fds[i], buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr *)&sa, &sa_len);
                if (ret2 < 0) {
                    PRINT(ERROR, "recvfrom failed: %s (%d)\n", strerror(errno),
                          errno);
                    ret--;
                    continue;
                }

                if (RETURN_SUCCESS != parse_cmd(&cmd, buffer)) {

                    PRINT(VERBOSE, "failed to parse command\n");
                    ret--;
                    continue;
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
                ret2 = sendto(comm_fds[i], buffer, strlen((char *)buffer), 0,
                              (struct sockaddr *)&sa, sa_len);
                if (ret2 < 0) {
                    PRINT(ERROR, "sendto failed: %s (%d)\n", strerror(errno),
                          errno);
                    ret--;
                    continue;
                }

                ret--;
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

                ret--;
            }

            if (0 != ret) {
                // Sanity check: this should be zero after servicing fd's!!
                PRINT(VERBOSE, "did not service all channels\n");
            }

            break;
        }
    }

    // Close the file descriptors
    for (i = 0; i < ARRAY_SIZE(port_nums); i++) {
        close_udp_comm(comm_fds[i]);
    }

#endif
    return 0;
}
