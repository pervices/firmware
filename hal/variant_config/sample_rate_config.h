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

#ifdef S1000

    #define S_MAT_RATE "1000"

    // Sample rates are in samples per second (SPS).
    #define RX_BASE_SAMPLE_RATE   1000000000.0
    #define RX_DSP_SAMPLE_RATE   (RX_BASE_SAMPLE_RATE/2.0)
    #define TX_BASE_SAMPLE_RATE   1000000000.0
    #define TX_DSP_SAMPLE_RATE   500000000.0
    #define RESAMP_SAMPLE_RATE 160000000.0  //After 4/5 resampling //NB: Tate 64t does NOT support 4/5 resampling
    // (2 ^ 32) / (RX_DSP_SAMPLE_RATE)
    #define RX_DSP_NCO_CONST \
        ((double)8.589934592)

    //Remember to adjust hdlr_tx_##ch##_trigger_edge_sample_num when changing the sample rate

    // TX_DSP_NCO_CONST = (2 ^ 32) / (TX_DSP_SAMPLE_RATE)
    #define TX_DSP_NCO_CONST \
        ((double)4.294967296)

    #define MAX_DSP_NCO (TX_BASE_SAMPLE_RATE / 2.0)
    //max nco of the AD9176, higher nco's result in errors in the board
    //the nco cna probably be rasied if those errors are fixed
    #define MAX_DAC_NCO 4000000000

    //TX sample rate factor must be less than thist
    #define MAX_TX_SAMPLE_FACTOR 65535.0 //2^16-1
    #define MIN_TX_SAMPLE_RATE (TX_DSP_SAMPLE_RATE/MAX_TX_SAMPLE_FACTOR)

#else
    #error Invalid maximum sample rate specified (MHz), must be: S1000. S3000 is not implemented for NRNT yet
#endif
