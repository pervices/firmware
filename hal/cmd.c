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

#include "cmd.h"

#include <stdio.h>
#include <stdint.h>

#define RING_SIZE 512

struct
{
    uint64_t a;
    uint64_t b;
    double cmd_time;
    Cmd items[RING_SIZE];
}
ring;

void cmd_push(const Cmd cmd)
{
    ring.items[ring.b++] = cmd;
    ring.b %= RING_SIZE;
}

Cmd cmd_pop(void)
{
    const Cmd cmd = ring.items[ring.a++];
    ring.a %= RING_SIZE;
    return cmd;
}

void cmd_set_time(const double time)
{
    ring.cmd_time = time;
}

// To be threaded.
void cmd_poll_and_execute(void)
{
}
