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

#include "udp.h"

// Function not used. Removing to fix compiler warnings.
#if 0

    static int establish_eth_settings(eth_t *eth) {
        struct ifreq ifr;

        // local socket
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

        // MAC Address
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, eth->iface, IF_NAMESIZE);
        if ((ioctl(sockfd, SIOCGIFHWADDR, &ifr)) == -1) {
            PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
            return RETURN_ERROR_COMM_INIT;
        }
        memcpy(&(eth->hwa), &ifr.ifr_hwaddr.sa_data, 6);

        // IP address
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, eth->iface, IF_NAMESIZE);
        if ((ioctl(sockfd, SIOCGIFADDR, &ifr)) == -1) {
            PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
            return RETURN_ERROR_COMM_INIT;
        }
        memcpy(&eth->ipa, &(*(struct sockaddr_in *)&ifr.ifr_addr).sin_addr, 4);

        // Broadcast
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, eth->iface, IF_NAMESIZE);
        if ((ioctl(sockfd, SIOCGIFBRDADDR, &ifr)) == -1) {
            PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
            return RETURN_ERROR_COMM_INIT;
        }
        memcpy(&eth->bcast, &(*(struct sockaddr_in *)&ifr.ifr_broadaddr).sin_addr,
               4);

        // Gateway TODO
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, eth->iface, IF_NAMESIZE);
        if ((ioctl(sockfd, SIOCGIFBRDADDR, &ifr)) == -1) {
            PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
            return RETURN_ERROR_COMM_INIT;
        }
        memcpy(&eth->bcast, &(*(struct sockaddr_in *)&ifr.ifr_broadaddr).sin_addr,
               4);

        // Netmask
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, eth->iface, IF_NAMESIZE);
        if ((ioctl(sockfd, SIOCGIFNETMASK, &ifr)) == -1) {
            PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
            return RETURN_ERROR_COMM_INIT;
        }
        memcpy(&eth->nmask.s_addr,
               &(*(struct sockaddr_in *)&ifr.ifr_netmask).sin_addr, 4);

        // MTU
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, eth->iface, IF_NAMESIZE);
        if ((ioctl(sockfd, SIOCGIFMTU, &ifr)) == -1) {
            PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
            return RETURN_ERROR_COMM_INIT;
        }
        eth->mtu = ifr.ifr_mtu;

        return RETURN_SUCCESS;
    }
#endif

int get_ip(eth_t* eth, char* str, int size)
{
    inet_ntop(AF_INET, &(eth->ipa), str, size);
    return RETURN_SUCCESS;
}

int get_bcast(eth_t* eth, char* str, int size)
{
    inet_ntop(AF_INET, &(eth->bcast), str, size);
    return RETURN_SUCCESS;
}

int get_nmask(eth_t* eth, char* str, int size)
{
    inet_ntop(AF_INET, &(eth->nmask), str, size);
    return RETURN_SUCCESS;
}

int get_mac(eth_t* eth, char* str, int size)
{
    strcpy(str, (char*)ether_ntoa(&(eth->hwa)));
    return RETURN_SUCCESS;
}

int get_gate(eth_t* eth, char* str, int size)
{
    inet_ntop(AF_INET, &(eth->gate), str, size);
    return RETURN_SUCCESS;
}

int establish_udp_connection(udp_dev_t* udp, in_port_t port)
{

    // open the socket
    udp->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp->sockfd < 0) {
        PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
        return RETURN_ERROR_COMM_INIT;
    }

    // zero out the structure and apply the configurations for the socket
    memset((char*)&(udp->si), 0, sizeof(udp->si));
    udp->si.sin_family = AF_INET;
    udp->si.sin_port = htons(port);
    udp->si.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind the settings to the socket
    if (bind(udp->sockfd, (struct sockaddr*)&(udp->si), sizeof(udp->si)) < 0) {
        PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
        return RETURN_ERROR_COMM_INIT;
    }
    udp->slen = sizeof(udp->si);

    // connection has been established
    PRINT(VERBOSE, "%s(): UDP connection up\n", __func__);
    return RETURN_SUCCESS;
}

// send data back
// the sender's IP address is populated in slen on recvfrom()
int send_udp(udp_dev_t* udp, uint8_t* data, uint16_t size)
{
    PRINT(VERBOSE, "Sending: ");
    int i;
    for (i = 0; i < size; i++)
        PRINT(VERBOSE, "%c", data[i]);
    PRINT(VERBOSE, "\n");

    if (sendto(udp->sockfd, (void*)(data), size, 0,
            (struct sockaddr*)&(udp->si), udp->slen)
        < 0) {
        PRINT(ERROR, "%s(), %s\n", __func__, strerror(errno));
        return RETURN_ERROR_COMM_BUS;
    }

    return RETURN_SUCCESS;
}

// non-blocking receive from UDP
int recv_udp(udp_dev_t* udp, uint8_t* data, uint16_t* size, uint16_t max_size)
{
    int bytes = 0;

    memset(data, 0, max_size);

    bytes = recvfrom(udp->sockfd, (void*)(data), max_size, 0,
        (struct sockaddr*)&(udp->si), (socklen_t*)(&(udp->slen)));

    if (bytes < 0) {
        // non-blocking will return -1 if no data
        if (bytes != -1)
            PRINT(ERROR, "%s(): ERROR, %s\n", __func__, strerror(errno));
        return RETURN_ERROR_COMM_NO_DATA;
    } else {
        PRINT(VERBOSE, "Receiving: ");
        int i;
        for (i = 0; i < bytes; i++)
            PRINT(VERBOSE, "%c", data[i]);
        PRINT(VERBOSE, "\n");

        *size = (uint8_t)bytes;
        return RETURN_SUCCESS;
    }
}

int severe_udp_connection(udp_dev_t* udp)
{
    PRINT(VERBOSE, "Severing udp connection\n");
    return close(udp->sockfd);
}
