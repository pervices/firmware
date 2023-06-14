//
// Copyright 2014 - 2023 Per Vices Corporation
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

#ifdef RTM6
    #define RTM_VER 6
    #error "RTM6 not added to variants yet"

#elif RTM7
    #define RTM_VER 7
    #error "RTM7 not added to variants yet"

#elif RTM8
    #define RTM_VER 8
    #error "RTM8 not added to variants yet"

#elif RTM9
    #define RTM_VER 9

#else
    #error "This file must be compiled with a valid hardware revision (RTM6, RTM7, RTM8, RTM9)"
#endif
