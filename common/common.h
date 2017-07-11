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

#ifndef COMMON_H_
#define COMMON_H_

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

// Define used for debugging
//#define DEBUG

// Global return values
#define RETURN_SUCCESS			0
#define RETURN_ERROR			-1
#define RETURN_ERROR_PARAM		-2
#define RETURN_ERROR_COMM_INIT		-3
#define RETURN_ERROR_COMM_NO_DATA	-4
#define RETURN_ERROR_COMM_BUS		-5
#define RETURN_ERROR_COMM_MMAP		-6
#define RETURN_ERROR_INOTIFY		-7
#define RETURN_ERROR_GET_PROP		-8
#define RETURN_ERROR_SET_PROP		-9
#define RETURN_ERROR_UART_TIMEOUT	-10
#define RETURN_ERROR_CMD_CURRUPT	-11
#define RETURN_ERROR_INVALID_REGISTER	-12
#define RETURN_ERROR_ADDR_OUT_OF_RANGE	-13
#define RETURN_ERROR_INSUFFICIENT_RESOURCES	-99
#define RETURN_ERROR_UART_FLUSH		-14

// server options
#define SERVER_DEBUG_OPT	(1 << 0)
#define SERVER_RESVD_OPT	(1 << 1)

// General purpose temp buffer size
#define BUF_SIZE 	( 1024 )
#define MAX_PATH_LEN	( 1024 )
#define MAX_PROP_LEN	( 1024 )
#define MAX_UART_LEN	( 8192 )

// macro for string comparison
#define EQUAL_STR(VAR, STR)		(strncmp(VAR, STR, strlen(STR)) == 0)

// macro for mallocing space and filling it up with STR
#define MALLOC_SET_STR(VAR, STR) 		\
	do { VAR = malloc(sizeof(STR));		\
		memcpy(VAR, STR, sizeof(STR));	\
	} while (0)

// Macro to make a folder of properties
#define MAKE_DIR(BUF, PROP_PATH, ROOT_PATH)			\
	do {							\
		memset(BUF, '\0', BUF_SIZE);			\
		memcpy(BUF, "mkdir -p ", sizeof("mkdir -p "));	\
		strcat(BUF, path);				\
		strcat(BUF, PROP_PATH);				\
		system(BUF);					\
	} while (0)

// Macro to make a file for saving property
#define MAKE_PROP(BUF, PROP_PATH, ROOT_PATH)			\
	do {							\
		memset(BUF, '\0', BUF_SIZE);			\
		memcpy(BUF, "touch ", sizeof("touch "));	\
		strcat(BUF, path);				\
		strcat(BUF, PROP_PATH);				\
		system(BUF);					\
	} while (0)

// Output data structure
typedef struct outdata {
	uint8_t status;	// status of the operation
	uint32_t seq;	// sequence of cmd, must match	NOT YET IMPLEMENTED
	char* data;		// data the be sent back
} outdata_t;

// Input data structure
typedef struct indata {
	uint32_t seq;	// sequence of cmd, must match NOT YET IMPLEMENTED
	uint8_t ctgry;	// category
	uint8_t op;		// operation
	char* cmd;		// command
	char* data;		// write data if operation is a SET
} indata_t;

// UDP ports
typedef enum {
	UDP_MGMT_PORT = 42799,
	UDP_RXA_PORT  = 42800,
	UDP_RXB_PORT  = 42801,
	UDP_RXC_PORT  = 42802,
	UDP_RXD_PORT  = 42803,
	UDP_TXA_PORT  = 42804,
	UDP_TXB_PORT  = 42805,
	UDP_TXC_PORT  = 42806,
	UDP_TXD_PORT  = 42807,
	num_udp_ports = 9
} udp_port_t;

enum {
UDP_FLOW_CNTRL_PORT = 42808,	// this one has to be the last one
};

// Boolean datatype
typedef enum {
	FALSE,
	TRUE
} boolean;

typedef enum {
	ERROR,
	INFO,
	DEBUG,
	VERBOSE,
	DUMP
} print_t;

// printf wrapper
int PRINT( print_t priority, const char* format, ... );

#define LOG_FILE	( "/var/crimson/crimson.log" )
#define DUMP_FILE	( "/var/crimson/dump.log" )

#endif /* COMMON_H_ */
