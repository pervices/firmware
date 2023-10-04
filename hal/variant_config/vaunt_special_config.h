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

//

#pragma once

//Decodes flags for special variant
// Currently implemented mask values:
// bit 0: flag indicates the unit is a variant with a fixed 40GHz shift (+ normal tunable stuff)

// Decode bitmask
#ifdef F1
    #define FIXED_40G 1
#elif defined(F0)
    #define FIXED_40G 0
#else
    #error "This file must be compiled with a valid flag combination (F0, F1)"
#endif
