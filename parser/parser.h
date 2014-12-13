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

#ifndef PARSER_H_
#define PARSER_H_

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"

typedef enum {OP_GET, OP_SET, OP_ERROR} operation_t;
typedef enum {CMD_SUCCESS, CMD_ERROR} cmd_status_t;

typedef struct cmd {
	uint32_t seq;
	operation_t op;	
	cmd_status_t status;
	char prop[MAX_PATH_LEN]; // prop is a path variable in linux
	char data[MAX_PROP_LEN]; // data is the property value
} cmd_t;

int parse_cmd(cmd_t* cmd, uint8_t* data);
void build_cmd(cmd_t* cmd, uint8_t* data, size_t max_size);

#endif
