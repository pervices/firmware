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

// NCO shift in the ADC in 1Gsps mode of the ADC32RF45
// There is a fixed NCO shift (250MHz) in the ADC in this mode. It is mostly compensated for by another NCO in the ADC (249.98MHz)
#define RX_NCO_SHIFT_3G_TO_1G (250000000 - 249984741.211)
