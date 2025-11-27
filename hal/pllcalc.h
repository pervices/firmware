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

/**
 * \file pllcalc.h
 *
 * \brief PLL Calculator header file
 *
 */

/**
 * \par PLL Calculator header file
 *
 * This file describes the PLL header files
 *
 * \par Content
 *
 * This header includes all PLL specific functions.
 *
 */

#ifndef _PLLCALC_H_
#define _PLLCALC_H_

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <math.h>
#include <stdlib.h>

// For stand alone operation;
//#define _PLL_DEBUG_STANDALONE //Standalone program, compile with: gcc -lm pllcalc.c and run with: ./a.out
//#define TATE_NRNT // or #define VAUNT and #define RTM10
//#define _PLL_DEBUG_INFO
//#define _PLL_DEBUG_VERBOSE

#ifndef _PLL_DEBUG_STANDALONE
#include "common.h"
#endif

// // PLL Calculator Parameters
// The _PLL_RATS_MAX_DENOM value effectively determines the largest R value to
// used during approximation, and has the largest impact on overall tuning
// accuracy.
#define _PLL_RATS_MAX_DENOM 511 // 16383 // 4095 // 2047 // 1023 // 511

// For further accuracy, one may also adjust the PLL1_N_MAX parameter.
// Increasing both will lead to better frequency accuracy on the radio side,
// but at the cost of accuracy.

// Core reference feeds to PLL0
#if defined(VAUNT) || defined(AVERY)
    // Used to convert specified RTMX specified in the compile flags to HARDWARE_RTM_VER
    #include "hal/variant_config/vaunt_rtm_config.h"

    #if HARDWARE_RTM_VER == 6 || HARDWARE_RTM_VER == 7
        #define PLL_CORE_REF_FREQ_HZ 25000000ULL // Default Reference Frequency used.
        #define PLL_CORE_REF_FREQ_HZ_S "25000000"
        #define AVERY_REF_FREQ_HZ PLL_CORE_REF_FREQ_HZ
    #elif HARDWARE_RTM_VER == 8
        #define PLL_CORE_REF_FREQ_HZ 5000000ULL // Default Reference Frequency used.
        #define PLL_CORE_REF_FREQ_HZ_S "5000000"
        #define AVERY_REF_FREQ_HZ PLL_CORE_REF_FREQ_HZ
    #elif HARDWARE_RTM_VER == 9 || HARDWARE_RTM_VER == 12 || HARDWARE_RTM_VER == 15
        #define PLL_CORE_REF_FREQ_HZ 10000000ULL // Default Reference Frequency used.
        #define PLL_CORE_REF_FREQ_HZ_S "10000000"
        #define AVERY_REF_FREQ_HZ PLL_CORE_REF_FREQ_HZ
    #elif HARDWARE_RTM_VER == 10 || HARDWARE_RTM_VER == 11
        #define PLL_CORE_REF_FREQ_HZ 10000000ULL // Default Reference Frequency used.
        #define PLL_CORE_REF_FREQ_HZ_S "10000000"
        // RTM10 currently uses 5MHz reference, with doubler on ADF5355/LMX2572 active, LMX2595 on avery does not use the doubler so it needs a different define
        #define AVERY_REF_FREQ_HZ PLL_CORE_REF_FREQ_HZ/2
    #else
        #error "Invalid RTM specified"
    #endif

#elif defined(TATE_NRNT) || defined(LILY)
// TODO LILY: confirm this is correct
    #define PLL_CORE_REF_FREQ_HZ 100000000ULL // Default Reference Frequency used.
#else
    #error "You must specify either ( VAUNT | TATE_NRNT | LILY ) when compiling this project."
#endif

// PLL IDs
#define PLL_ID_ADF5355 5355
#define PLL_ID_LMX2595 2595
#define PLL_ID_LMX2572 2572

// ADF4355 PLL Specifications
#define PLL1_REF_MAX_HZ 600000000ULL
#define PLL1_REF_MIN_HZ 10000000ULL
#define PLL1_RFOUT_MAX_HZ 6800000000ULL
#define PLL1_RFOUT_MIN_HZ 54000000ULL
#define PLL1_VCO_MIN_HZ 3400000000ULL
#define PLL1_VCO_MAX_HZ 6800000000ULL
#define PLL1_N_MIN 23 // Minimum allowable N value (per datasheet)
#define PLL1_N_MAX                                                             \
    4024 // 7000 // 524255 // 131072 // 65535 // 32767 // 16383 // 255
         // //Maximum allowable N value (per performance/judgement)
#define PLL1_R_MIN 1 // Require lower R divider to ensure reliable PDF frequency
#define PLL1_R_MAX 100
#define PLL1_DIV_MAX 64
#define PLL1_DIV_MIN 1
#define PLL1_FB_THRESHOLD 575000000ULL

#define PLL_PARAM_GOOD 0

// ADF4355 Default specs
#define PLL1_R_FIXED                                                           \
    (1) // R value must be fixed to ensure consistent PFD frequency (13MHz)
#define PLL1_R_FIXED_5                                                         \
    (5) // R value must be fixed to ensure consistent PFD frequency (13MHz)
#define PLL_ADF5355_MAX_N                                                      \
    (23) // R value must be fixed to ensure consistent PFD frequency (13MHz)

#define PLL1_N_DEFAULT                                                         \
    (112)                  // N value (N^2 contribution to PLL noise) [16..4096]
#define PLL1_D_DEFAULT (2) // RFoutput divider value (1,2,4,6..58,60,62)
#define PLL1_X2EN_DEFAULT                                                      \
    (0) // RFoutput doubler enabled (0=off, 1=on (RFout is doubled))
#define PLL1_OUTFREQ_DEFAULT (2800000000) // Resulting VCO Output Frequency
#define PLL1_FB_DEFAULT (1)               // VCO divider feedback for ADF5355


// LMX2595 Specifications
// (NB: a lot of this is duplicated from lmx2595_ll.h)
#define LMX2595_RFOUT_MAX_HZ 20000000000ULL     // from datasheet
#define LMX2595_RFOUT_MIN_HZ 10000000ULL        // from datasheet
#define LMX2595_VCO_MAX1_HZ 15000000000ULL      // vco frequency limit if d is 6 or less
#define LMX2595_VCO_MAX2_HZ 11500000000ULL      // vco frequency limit if d if 8 or more
#define LMX2595_D_THRESH_VCO 7                  // set border between check for VCO upper limit
#define LMX2595_VCO_MIN_HZ 7500000000ULL        // from datasheet
// TODO LILY: confirm this is correct
#if defined(TATE_NRNT) || defined(LILY)
    #define LMX2595_DIV_MAX 384     // datasheet says 768, but at that div we cannot use synch to ensure phase coherency across channels
#elif defined (VAUNT) || defined (AVERY)
    #define LMX2595_DIV_MAX 768
#else
    #error "You must specify either ( VAUNT | AVERY | TATE_NRNT | LILY ) when compiling this project."
#endif
#define LMX2595_DIV_MIN 1       // from datasheet
#define LMX2595_N_MAX 524287    // from datasheet
#define LMX2595_N_MIN 28        // from datasheet
#define LMX2595_R_MAX 255       // could technically get higher using pre-div, but currently unsupported
#define LMX2595_R_MIN 1         // from datasheet

// LMX2572 Specifications
// (NB: a lot of this is duplicated from lmx2572_ll.h)
#define LMX2572_RFOUT_MAX_HZ 6400000000ULL
#define LMX2572_RFOUT_MIN_HZ 12500000ULL
#define LMX2572_VCO_MAX_HZ 6400000000ULL
#define LMX2572_VCO_MIN_HZ 3200000000ULL
#define LMX2572_DIV_MAX 256
#define LMX2572_DIV_MIN 1
#define LMX2572_N_MAX 524287
#define LMX2572_N_MIN 20
#define LMX2572_N_DIV_MIN_HIBAND    24
#define LMX2572_N_DIV_HIBAND_THRESH 4000000000ULL // MHz
#define LMX2572_R_MAX 255       // could technically get higher using pre-div, but currently unsupported
#define LMX2572_R_MIN 1

/*

 * This file has the following sections:
 *
 * - Include all required headers for busses, etc.
 * - Declare all synthesizer functions in synth.c
 * - Declare all MCU specific buses and structures (SPI, I2C, UART, etc)
 * - Declare all board specific chips and structures (PLLs, DACs, VGAs, etc)
 * - Define and name all chip specific pins
 *
 */

// Define a generic structure to hold PLL values.
typedef struct {
    uint32_t id;            // ID code for PLL IC
    uint64_t ref_freq;      // Core Reference Frequency (in Hz)
    uint16_t R;             // Reference divider R
    uint32_t N;             // VCO Frequency divider N
    uint16_t d;             // VCO Output Frequency divider
    uint8_t x2en;           // VCO Output Frequency doubler (enabled when 1)
    long double vcoFreq;    // Resulting VCO Output Frequency
    uint8_t divFBen;        // Feedback from divider (enabled when 1, default 0)
    uint64_t rf_out_max;    // limit for RF output (in Hz)
    uint64_t rf_out_min;    // limit for RF output (in Hz)
    uint64_t vco_f_max;     // limit for VCO freq (in Hz)
    uint64_t vco_f_min;     // limit for VCO freq (in Hz)
    uint16_t d_max;         // limit for output divider d
    uint16_t d_min;         // limit for output divider d
    uint32_t n_max;         // limit for feedback divider n
    uint16_t n_min;         // limit for feedback divider n
    uint16_t r_max;         // limit for reference divider r
    uint16_t r_min;         // limit for reference divider r
} pllparam_t;

// PLL Constructors

//default ADF5355 constructor
static pllparam_t __attribute__ ((unused)) pll_def_adf5355 = {   PLL_ID_ADF5355,         PLL_CORE_REF_FREQ_HZ,
                                        PLL1_R_FIXED,           PLL1_N_DEFAULT,
                                        PLL1_D_DEFAULT,         PLL1_X2EN_DEFAULT,
                                        PLL1_OUTFREQ_DEFAULT,   PLL1_FB_DEFAULT,
                                        PLL1_RFOUT_MAX_HZ,      PLL1_RFOUT_MIN_HZ,
                                        PLL1_VCO_MAX_HZ,        PLL1_VCO_MIN_HZ,
                                        PLL1_DIV_MAX,           PLL1_DIV_MIN,
                                        PLL1_N_MAX,             PLL1_N_MIN,
                                        PLL1_R_MAX,             PLL1_R_MIN
};

//ADF5355 constructor for r divider = 5
static pllparam_t __attribute__ ((unused)) pll_def_adf5355_r_5 = {   PLL_ID_ADF5355,         PLL_CORE_REF_FREQ_HZ,
                                            PLL1_R_FIXED_5,         PLL1_N_DEFAULT,
                                            PLL1_D_DEFAULT,         PLL1_X2EN_DEFAULT,
                                            PLL1_OUTFREQ_DEFAULT,   PLL1_FB_DEFAULT,
                                            PLL1_RFOUT_MAX_HZ,      PLL1_RFOUT_MIN_HZ,
                                            PLL1_VCO_MAX_HZ,        PLL1_VCO_MIN_HZ,
                                            PLL1_DIV_MAX,           PLL1_DIV_MIN,
                                            PLL1_N_MAX,             PLL1_N_MIN,
                                            PLL1_R_MAX,             PLL1_R_MIN
};

// default LMX2595 constructor
static pllparam_t __attribute__ ((unused)) pll_def_lmx2595 = {   PLL_ID_LMX2595,         PLL_CORE_REF_FREQ_HZ,
                                        PLL1_R_FIXED,           PLL1_N_DEFAULT,
                                        PLL1_D_DEFAULT,         PLL1_X2EN_DEFAULT,
                                        PLL1_OUTFREQ_DEFAULT,   PLL1_FB_DEFAULT,
                                        LMX2595_RFOUT_MAX_HZ,   LMX2595_RFOUT_MIN_HZ,
                                        LMX2595_VCO_MAX1_HZ,    LMX2595_VCO_MIN_HZ,
                                        LMX2595_DIV_MAX,        LMX2595_DIV_MIN,
                                        LMX2595_N_MAX,          LMX2595_N_MIN,
                                        LMX2595_R_MAX,          LMX2595_R_MIN
};

#if defined(VAUNT) || defined(AVERY)
// default LMX2572 constructor
static pllparam_t __attribute__ ((unused)) pll_def_lmx2572 = {
    PLL_ID_LMX2572,         PLL_CORE_REF_FREQ_HZ,
    PLL1_R_FIXED,           PLL1_N_DEFAULT,
    PLL1_D_DEFAULT,         PLL1_X2EN_DEFAULT,
    PLL1_OUTFREQ_DEFAULT,   PLL1_FB_DEFAULT,
    LMX2572_RFOUT_MAX_HZ,   LMX2572_RFOUT_MIN_HZ,
    LMX2572_VCO_MAX_HZ,     LMX2572_VCO_MIN_HZ,
    LMX2572_DIV_MAX,        LMX2572_DIV_MIN,
    LMX2572_N_MAX,          LMX2572_N_MIN,
    LMX2572_R_MAX,          LMX2572_R_MIN
};

static pllparam_t __attribute__ ((unused)) pll_def_lmx2595_avery = {
    PLL_ID_LMX2595,         AVERY_REF_FREQ_HZ,
    PLL1_R_FIXED,           PLL1_N_DEFAULT,
    PLL1_D_DEFAULT,         PLL1_X2EN_DEFAULT,
    PLL1_OUTFREQ_DEFAULT,   PLL1_FB_DEFAULT,
    LMX2595_RFOUT_MAX_HZ,   LMX2595_RFOUT_MIN_HZ,
    LMX2595_VCO_MAX1_HZ,    LMX2595_VCO_MIN_HZ,
    LMX2595_DIV_MAX,        LMX2595_DIV_MIN,
    LMX2595_N_MAX,          LMX2595_N_MIN,
    LMX2595_R_MAX,          LMX2595_R_MIN
};
#endif

// Set Output Frequency
double setFreq(uint64_t *reqFreq,
               pllparam_t *pll1); // Returns the actual frequency set.

// Return 1 if all sanitary checks for pllparam_t have passed
uint8_t pll_CheckParams(pllparam_t *pllparam, uint8_t is_pll1);

// Determine the VCO and Output Settings of the PLL
void pll_SetVCO(uint64_t *reqFreq, pllparam_t *pllparam);

#endif
