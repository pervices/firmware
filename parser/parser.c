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

#define DELIMINATOR ','
#define STRING_GET "get"
#define STRING_SET "set"

typedef enum {STATE_SEQ, STATE_OP, STATE_PROP, STATE_DATA} state_t;

// seq,op,prop,[data]
int parse_cmd(cmd_t* cmd, uint8_t* data) {
	if (!data || !cmd) return RETURN_ERROR_PARAM;

	size_t pos = 0;
	size_t start = 0;
	state_t cur_state = STATE_SEQ;
	uint8_t finish = 0;

	// default settings
	cmd -> seq = 0;
	cmd -> op = OP_ERROR;
	cmd -> status = CMD_ERROR;
	memset(cmd -> prop, 0, MAX_PATH_LEN);
	memset(cmd -> data, 0, MAX_PROP_LEN);

	printf("Parsing: %s\n", data);

	while (data[pos] != 0 || pos - start != 0) {
		// if a deliminator is found
		if (data[pos] == DELIMINATOR || data[pos] == '\0') {
			data[pos] = '\0';

			// state machine
			switch (cur_state) {
			case STATE_SEQ:
				sscanf((char*)(data + start), "%"SCNd32"", &(cmd -> seq));
				cur_state = STATE_OP;
				break;

			case STATE_OP:
				if (strcmp((char*)(data + start), STRING_GET) == 0) {
					cmd -> op = OP_GET;
				} else {
					cmd -> op = OP_SET;
				}
				cur_state = STATE_PROP;
				break;

			case STATE_PROP:
				strncpy(cmd -> prop, (char*)(data + start), MAX_PATH_LEN);
				if(cmd -> op == OP_GET) {
					cur_state = STATE_SEQ;
					finish = 1;
				} else {
					cur_state = STATE_DATA;
				}
				break;

			case STATE_DATA:
				strncpy(cmd -> data, (char*)(data + start), MAX_PROP_LEN);
				cur_state = STATE_SEQ;
				finish = 1;
				break;

			default:
				cur_state = STATE_SEQ;
				break;
			}

			// setup for the next parameter
			start = pos+1;
		}
		pos++;
	}

	printf("Parsing: %i characters\n", pos);

	// if command did not receive everything it expects
	if (finish == 0) {
		printf("ERROR: %s, currupt command\n", __func__);
		cmd -> status = CMD_ERROR;
		return RETURN_ERROR_CMD_CURRUPT;
	} else {
		cmd -> status = CMD_SUCCESS;
		return RETURN_SUCCESS;
	}
}

// seq,status,[data]
void build_cmd(cmd_t* cmd, uint8_t* data, size_t max_size) {
	if (!cmd || !data) return;

	memset(data, 0, max_size);
	snprintf((char*)data, max_size, "%"PRIu32",%i,%s", cmd -> seq, cmd -> status, cmd -> data);
}
