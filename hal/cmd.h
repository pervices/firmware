//
// Copyright 2014 - 2018 Per Vices Corporation
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

#ifndef _CMD_H_
#define _CMD_H_

typedef struct
{
    int (*fun)(const char*, char*);
    double time;
}
Cmd;

void cmd_push(const Cmd);

Cmd cmd_pop(void);

void cmd_set_time(const double time);

void cmd_poll_and_execute(void);

#endif
