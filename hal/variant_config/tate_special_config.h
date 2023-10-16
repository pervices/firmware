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
// bit 0: 3G to 1G conversion, sets flag USE_3G_AS_1G

// Decode bitmask
#ifdef F1
    #define USE_3G_AS_1G 1
    #define S_USE_3G_AS_1G "1"
#elif defined(F0)
    #define USE_3G_AS_1G 0
    #define S_USE_3G_AS_1G "0"
#else
    #error "This file must be compiled with a valid flag combination (F0, F1)"
#endif

// NCO shift in the ADC in 1Gsps mode of the ADC32RF45
#define RX_NCO_SHIFT_3G_TO_1G 250000000
