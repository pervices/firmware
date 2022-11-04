//
// Copyright 2014 - 2022 Per Vices Corporation
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

#pragma once

#ifdef RTM3
    #define RTM_VER 3

    // On 1Gsps sysref is activated on the falling edge of sysref, other stuff is active on the rising edge
    // To compensate for this some revisions swapped dev clk on hardware
    #define INVERT_DEV_CLOCK_HARDWARE_SWAP 1

    // Default phase shift of sysref in VCO clock cycles
    #define DEFAULT_DIGITAL_SYSREF_DELAY 24

#elif RTM4
    #define RTM_VER 4

    // On 1Gsps sysref is activated on the falling edge of sysref, other stuff is active on the rising edge
    // To compensate for this some revisions swapped dev clk on hardware
    #define INVERT_DEV_CLOCK_HARDWARE_SWAP 1

    // Default phase shift of sysref in VCO clock cycles
    #define DEFAULT_DIGITAL_SYSREF_DELAY 24

#elif RTM5
    #define RTM_VER 5

    // On 1Gsps sysref is activated on the falling edge of sysref, other stuff is active on the rising edge
    // To compensate for this some revisions swapped dev clk on hardware
    #define INVERT_DEV_CLOCK_HARDWARE_SWAP 0

    // Default phase shift of sysref in VCO clock cycles
    #define DEFAULT_DIGITAL_SYSREF_DELAY 24

#else
    #error "This file must be compiled with a valid hardware revision (RTM3, RTM4, RTM5)"
#endif
