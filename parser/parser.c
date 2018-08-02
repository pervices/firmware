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

#include "parser.h"


typedef enum {STATE_SEQ, STATE_OP, STATE_PROP, STATE_DATA} state_t;

// seq,op,prop,[data]
int parse_cmd(cmd_t* cmd, uint8_t* data) {
	int r;
	char* tok;
	char* saveptr = (char *)data;
	// our packets are either "123,get,foo", or "123,set,foo,bar", so there are a maximum of 4 fields
	const unsigned n_max = 4;
	unsigned n;
	int check;

	PRINT( INFO, "parse(%s)\n", saveptr);

	if (!data || !cmd) return RETURN_ERROR_PARAM;

	for(
		// for loop init
		n = 0,
		tok = strtok_r( saveptr, ",", & saveptr );

		// for loop condition
		NULL != tok && n < n_max;

		// for loop update
		tok = strtok_r( saveptr, ",", & saveptr ),
		n++
	) {
		switch( n ) {

		case 0:
			// parse a number from a string
			// sscanf
			// on error, set r = ...ERROR...; goto out;
			check = sscanf(tok, "%u", &cmd -> seq);
			if (check != 1) {
				PRINT(ERROR, "Unable to parse command.");
				r = RETURN_ERROR;
				goto out;
			}
			break;

		case 1:
			// convert "get" or "set" to cmd_op_t
			// strncmp
			// on error, set r = ...ERROR...; goto out;
			if ( strncmp(tok, "get", 3) == 0 ){
				cmd -> op = OP_GET;
			}else if (strncmp(tok, "set", 3) == 0){
				cmd -> op = OP_SET;
			}else {
				cmd -> op = OP_ERROR;
				PRINT(ERROR, "Improper command format, options are get and set only.");
				r = RETURN_ERROR;
				goto out;
			}
			break;

		case 2:
			// strncpy
			strncpy(cmd -> prop, tok, sizeof(cmd -> prop));
			break;

		case 3:
			// strncpy
			strncpy(cmd -> data, tok, sizeof(cmd -> data));
			/* no break */
		default:
			break;
		}
	}
	r = RETURN_SUCCESS;
out:
	return r;
}

// seq,status,[data]
void build_cmd(cmd_t* cmd, uint8_t* data, size_t max_size) {
	if (!cmd || !data) return;

	memset(data, 0, max_size);
	snprintf((char*)data, max_size, "%"PRIu32",%i,%s", cmd -> seq, cmd -> status, cmd -> data);
}
