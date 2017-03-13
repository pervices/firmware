/*
 * (c) 2014 Per Vices Corporation.
 * See license.txt for license information.
 */

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

#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include <stdlib.h>
#include <math.h>


// For stand alone operation;
//#define _PLL_DEBUG_STANDALONE //Standalone program, compile with: gcc -lm pllcalc.c
//#define _PLL_DEBUG_INFO
//#define _PLL_DEBUG_VERBOSE

#ifndef _PLL_DEBUG_STANDALONE
#include "common.h"
#endif

//PLL Calculator Parameters
// The _PLL_RATS_MAX_DENOM value effectively determines the largest R value to 
// used during approximation, and has the largest impact on overall tuning
// accuracy.
#define _PLL_RATS_MAX_DENOM 511 // 16383 // 4095 // 2047 // 1023 // 511

// For further accuracy, one may also adjust the PLL1_N_MAX parameter. 
// Increasing both will lead to better frequency accuracy on the radio side, 
// but at the cost of accuracy.

#define _PLL_OUT_MAX_DEVIATION  500000

//Core reference feeds to PLL0
#define PLL_CORE_REF_FREQ_HZ	325000000ULL //Default Reference Frequency used.

//ADF4355 PLL Specifications
#define PLL1_REF_MAX_HZ		600000000ULL
#define PLL1_REF_MIN_HZ		10000000ULL
#define PLL1_RFOUT_MAX_HZ	6800000000ULL
#define PLL1_RFOUT_MIN_HZ	54000000ULL
#define PLL1_VCO_MIN_HZ		3400000000ULL
#define PLL1_VCO_MAX_HZ		6800000000ULL
#define PLL1_N_MIN		23   //Minimum allowable N value (per datasheet)
#define PLL1_N_MAX		4024    // 7000 // 524255 // 131072 // 65535 // 32767 // 16383 // 255	//Maximum allowable N value (per performance/judgement)
#define PLL1_R_MIN		1   //Require lower R divider to ensure reliable PDF frequency
#define PLL1_R_MAX		100
#define PLL1_DIV_MAX		64
#define PLL1_DIV_MIN		1

#define PLL_PARAM_GOOD		0

// ADF4355 Default specs
#define PLL1_R_FIXED		( 25 )		// R value must be fixed to ensure consistent PFD frequency (13MHz)

#define PLL1_N_DEFAULT		( 572 )         // N value (N^2 contribution to PLL noise) [16..4096]
#define PLL1_D_DEFAULT		( 2 )		// RFoutput divider value (1,2,4,6..58,60,62)
#define PLL1_X2EN_DEFAULT	( 0 )           // RFoutput doubler enabled (0=off, 1=on (RFout is doubled))
#define PLL1_OUTFREQ_DEFAULT	( 2904700000 )	// Resulting VCO Output Frequency
#define PLL1_FB_DEFAULT         ( 1 )           // VCO divider feedback


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
 typedef struct{
	uint16_t R;	//Reference divider R
   	uint32_t N;	//VCO Frequency divider N
	uint16_t d;	//VCO Output Frequency divider
	uint8_t  x2en;	//VCO Output Frequency doubler (enabled when 1)
	long double vcoFreq; //Resulting VCO Output Frequency
	uint8_t divFBen; //Feedback from divider (enabled when 1, default 0)
} pllparam_t;

// Set Output Frequency
double setFreq(uint64_t* reqFreq, pllparam_t* pll1); //Returns the actual frequency set.

// Return 1 if all sanitary checks for pllparam_t have passed
uint8_t pll_CheckParams(pllparam_t* pllparam, uint8_t is_pll1);

// Determine the VCO and Output Settings of the PLL
void pll_SetVCO(uint64_t* reqFreq, pllparam_t* pllparam);


#endif
