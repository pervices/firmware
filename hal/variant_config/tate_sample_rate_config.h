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

// Lily will be limited to 500Msps output but the internals operate at up to 1Gsps
#if (defined(S1000) && defined(TATE_NRNT)) || (defined(S500) && defined(LILY))

// Maximum sample rate the device will output
    #if (defined(S1000) && defined(TATE_NRNT))
        #define S_MAX_RATE "1000000000"
    #elif (defined(S500) && defined(LILY))
        #define S_MAX_RATE "500000000"
    #else
        #error "ifdef error, this should be unreachable"
    #endif

    // Sample rates are in samples per second (SPS).
    #define RX_BASE_SAMPLE_RATE   1000000000.0
    #define RX_DSP_SAMPLE_RATE   (RX_BASE_SAMPLE_RATE/2.0)
    #define TX_BASE_SAMPLE_RATE   1000000000.0
    #define TX_DSP_SAMPLE_RATE   500000000.0
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

    //RX sample rate factor must be less than this
    #define MAX_RX_SAMPLE_FACTOR 65535.0 //2^16-1
    #define MIN_RX_SAMPLE_RATE (RX_DSP_SAMPLE_RATE/MAX_RX_SAMPLE_FACTOR)

    //TX sample rate factor must be less than this
    #define MAX_TX_SAMPLE_FACTOR 65535.0 //2^16-1
    #define MIN_TX_SAMPLE_RATE (TX_DSP_SAMPLE_RATE/MAX_TX_SAMPLE_FACTOR)

    // Over the wire format, number bits per half of the iq pair
    // Note: the FPGA was hardcoded to assume sc16 in several places. If you see any values multiplied by OWT / 16 its because the actual value needs to be scales to produce a result giving the same number of bits as would occur with sc16
    #define DEAULT_OTW_RX 16
    #define S_DEAULT_OTW_RX "16"
    #define DEAULT_OTW_TX 16
    #define S_DEAULT_OTW_TX "16"

    // Number of samples requested from rx must be a multiple of this, applies to both trigger and nsamps mode
    #define NSAMPS_MULTIPLE_RX 1
    #define S_NSAMPS_MULTIPLE_RX "1"

    // On 1Gsps sysref is sampled on the falling edge of devclk
    // Most of the system activates on the rising edge of devclk
    #define RX_SYSREF_FALLING_EDGE 1

    // Sysref delays to try if the default fails
    #define NUM_DIGITAL_SYSREF_DELAYS 0
    const int possible_digital_sysref_delays[NUM_DIGITAL_SYSREF_DELAYS] = {};//{24, 25, 26};

    #define NUM_ANALOG_SYSREF_DELAYS 0
    //0 means delay of 0, 1 = delay of 600ps, 2 = 900ps, 3=1200ps...
    //Multiples of 1000ps are meaningless at 1G
    const int possible_analog_sysref_delays[NUM_ANALOG_SYSREF_DELAYS] =  {};//{0, 1, 2, 3, 4, 5, 6, 7, 8, 10};

    // Flag indicating the unit is capable to transmitting little endian samples
    #define LITTLE_ENDIAN_SUPPORTED 1

    // Maximum number of times the server will attempt to reset JESD IP before moving on to other methods
    // 3 For normal 1G, 1 for 3G when used as 1G
    #define JESD_MAX_RESET_ATTEMPTS (USE_3G_AS_1G ? 1 : 3)

    // Maximum length of VITA for rx
    #define RX_MAX_PAYLOAD 8900

#elif defined(TATE_NRNT) && defined(S3000)

    #define S_MAX_RATE "3000000000"

    // Sample rates are in samples per second (SPS).
    #define RX_BASE_SAMPLE_RATE   3000000000.0
    //RX_BASE_SAMPLE_RATE/2.0 is done because of a filter that halves the effective sample rate, however that part is not implemented yet
    //#define RX_DSP_SAMPLE_RATE   (RX_BASE_SAMPLE_RATE/2.0)
    #define RX_DSP_SAMPLE_RATE   (RX_BASE_SAMPLE_RATE)
    #define TX_BASE_SAMPLE_RATE   3000000000.0
    #define TX_DSP_SAMPLE_RATE   3000000000.0
    // (2 ^ 32) / (RX_DSP_SAMPLE_RATE)
    #define RX_DSP_NCO_CONST ((double)1.43165576533)

    //Remember to adjust hdlr_tx_##ch##_trigger_edge_sample_num when changing the sample rate

    // TX_DSP_NCO_CONST = (2 ^ 32) / (TX_DSP_SAMPLE_RATE)
    #define TX_DSP_NCO_CONST ((double)1.43165576533)

    #define MAX_DSP_NCO (TX_BASE_SAMPLE_RATE / 2.0)
    //max nco of the AD9176, higher nco's result in errors in the board
    //the nco cna probably be rasied if those errors are fixed
    #define MAX_DAC_NCO 4000000000

    //RX sample rate factor must be less than this
    #define MAX_RX_SAMPLE_FACTOR 65535.0 //2^16-1
    //#define MIN_RX_SAMPLE_RATE (RX_DSP_SAMPLE_RATE/MAX_RX_SAMPLE_FACTOR)
    //Currently 3G does not use the dsp, so it can only run at max rate
    #define MIN_RX_SAMPLE_RATE RX_BASE_SAMPLE_RATE

    //TX sample rate factor must be less than this
    #define MAX_TX_SAMPLE_FACTOR 65535.0 //2^16-1
    //#define MIN_TX_SAMPLE_RATE (TX_DSP_SAMPLE_RATE/MAX_TX_SAMPLE_FACTOR)
    //Currently 3G does not use the dsp, so it can only run at max rate
    #define MIN_TX_SAMPLE_RATE TX_BASE_SAMPLE_RATE

    // Over the wire format, number bits per half of the iq pair
    // Note: the FPGA was hardcoded to assume sc16 in several places. If you see any values multiplied by OWT / 16 its because the actual value needs to be scales to produce a result giving the same number of bits as would occur with sc16
    #define DEAULT_OTW_RX 16
    #define S_DEAULT_OTW_RX "16"
    #define DEAULT_OTW_TX 16
    #define S_DEAULT_OTW_TX "16"

    // Number of samples requested from rx must be a multiple of this, applies to both trigger and nsamps mode
    // TODO: verify how this requirement changes with the sample size changes
    #define NSAMPS_MULTIPLE_RX 2944
    #define S_NSAMPS_MULTIPLE_RX "2944"

    // On 3Gsps sysref is sampled on the falling edge of devclk
    // Most of the system activates on the rising edge of devclk
    #define RX_SYSREF_FALLING_EDGE 0

    // Sysref delays to try if the default fails
    #define NUM_DIGITAL_SYSREF_DELAYS 1
    const int possible_digital_sysref_delays[NUM_DIGITAL_SYSREF_DELAYS] = {24};

    #define NUM_ANALOG_SYSREF_DELAYS 10
    //0 means delay of 0, 1 = delay of 600ps, 2 = 900ps, 3=1200ps...
    //Multiples of 1000ps are meaningless at 1G
    const int possible_analog_sysref_delays[NUM_ANALOG_SYSREF_DELAYS] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10};

    // Flag indicating the unit is capable to transmitting little endian samples
    // NOTE: prior to May 2024 3G FPGA did not support little endian
    #define LITTLE_ENDIAN_SUPPORTED 1

    // Maximum number of times the server will attempt to reset JESD IP before moving on to other methods
    // 3 For normal 1G, 1 for 3G when used as 1G
    #define JESD_MAX_RESET_ATTEMPTS 1

    // Maximum length of VITA for rx
    #define RX_MAX_PAYLOAD 8872

#else
    #error Invalid maximum sample/product combination rate specified (MHz), must be: TATE_NRNT S1000, TATE_NRNT S3000, LILY S500
#endif
