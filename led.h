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

#ifndef LED_H_
#define LED_H_

#include "mmap.h"

#define LED_STATE_INIT  1
#define LED_STATE_READY 2
#define LED_STATE_ERROR 3
#define LED_STATE_UNKNOWN 0xff

void set_led_state(uint8_t state);

uint8_t get_led_state(void);

/* led control codes:
0x0     off
0x1     solid green
0x9     solid red
0x70003 flashing green
0x8000A flashing red
*/

#endif
