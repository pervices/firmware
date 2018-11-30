//
// Copyright 2014 - 2019 Per Vices Corporation
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

#ifndef UDP_H_
#define UDP_H_

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netinet/ether.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include "common.h"

typedef struct {
    char iface[50];        // interface name, ex. eth0
    struct ether_addr hwa; // mac address
    struct in_addr ipa;    // ip address
    struct in_addr bcast;  // broadcast
    struct in_addr nmask;  // net mask
    struct in_addr gate;   // default gateway
    uint32_t port;         // port number
    u_short mtu;           // MTU size
} eth_t;

typedef struct udp_dev {
    int sockfd;            // socket file descriptor
    uint16_t opt;          // options
    eth_t *eth;            // general ethernet settings
    struct sockaddr_in si; // socket prameters
    int slen;              // socket length
} udp_dev_t;

int get_ip(eth_t *eth, char *str, int size);
int get_bcast(eth_t *eth, char *str, int size);
int get_nmask(eth_t *eth, char *str, int size);
int get_mac(eth_t *eth, char *str, int size);
int get_gate(eth_t *eth, char *str, int size);

int establish_udp_connection(udp_dev_t *udp, unsigned short port);
int severe_udp_connection(udp_dev_t *udp);

int recv_udp(udp_dev_t *udp, uint8_t *data, uint16_t *size, uint16_t max_size);
int send_udp(udp_dev_t *udp, uint8_t *data, uint16_t size);

#endif
