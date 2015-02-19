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

// General purpose temp buffer size
#define BUF_SIZE 	( 100 )
#define MAX_PATH_LEN	( 1024 )
#define MAX_PROP_LEN	( 1024 )

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

#endif /* COMMON_H_ */
