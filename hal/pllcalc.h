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
#include "math.h"

//PLL Calculator Parameters
// The _PLL_RATS_MAX_DENOM value effectively determines the largest R value to 
// used during approximation, and has the largest impact on overall tuning
// accuracy.
#define _PLL_RATS_MAX_DENOM 40 // 16383 // 4095 // 2047 // 1023 // 511

// For further accuracy, one may also adjust the PLL1_N_MAX parameter. 
// Increasing both will lead to better frequency accuracy on the radio side, 
// but at the cost of accuracy.

#define _PLL_OUT_MAX_DEVIATION  200000

//Core reference feeds to PLL0
#define PLL_CORE_REF_FREQ_HZ	100000000ULL

//HMC830 PLL Specifications
#define PLL0_RFOUT_MAX_HZ	3000000000ULL
#define PLL0_RFOUT_MIN_HZ	25000000ULL
#define PLL0_VCO_MIN_HZ		1500000000ULL
#define PLL0_VCO_MAX_HZ		3000000000ULL
#define PLL0_PD_MIN_HZ		0	// DC Value
#define PLL0_PD_MAX_HZ		125000000ULL
#define PLL0_N_MIN		16
#define PLL0_N_MAX		7000 // 524255 // 131072 // 65535 // 32767 // 16383 // 255
#define PLL0_R_MIN		1
#define PLL0_DIV_STEPS		2
#define PLL0_DIV_MAX		62
#define PLL0_DIV_MIN		2

//HMC833 PLL Specifications
#define PLL1_REF_MAX_HZ		100000000ULL // Have to change Reg08h[21] of HMC833 if higher (upto 350MHz) necessary
#define PLL1_REF_MIN_HZ		20000000ULL
#define PLL1_RFOUT_MAX_HZ	6000000000ULL
#define PLL1_RFOUT_MIN_HZ	25000000ULL
#define PLL1_VCO_MIN_HZ		1500000000ULL
#define PLL1_VCO_MAX_HZ		3000000000ULL
#define PLL1_PD_MIN_HZ		20000000ULL	// To Maximize PD spur width
#define PLL1_PD_MAX_HZ		125000000ULL
#define PLL1_N_MIN		16	 //Minimum allowable N value (per datasheet)
#define PLL1_N_MAX		7000 // 524255 // 131072 // 65535 // 32767 // 16383 // 255	//Maximum allowable N value (per performance/judgement)
#define PLL1_R_MIN		1
#define PLL1_DIV_STEPS		2
#define PLL1_DIV_MAX		62
#define PLL1_DIV_MIN		2

#define PLL_PARAM_GOOD		0

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
	uint64_t outFreq; //Resulting VCO Output Frequency
} pllparam_t;

// Set Output Frequency
double setFreq(uint64_t* reqFreq, pllparam_t* pll0, pllparam_t* pll1); //Returns the actual frequency set.

// Determine PLL1 Reference frequency (and ensuring it falls within an acceptable range)
void pll_RefCalc(uint64_t* reqFreq, pllparam_t* pll0, pllparam_t* pll1);

// Return Rational approximation
void rat_approx(double f, uint64_t md, uint64_t* num, uint64_t* denom);

// Return 1 if all sanitary checks for pllparam_t have passed
uint8_t pll_CheckParams(pllparam_t* pllparam, uint8_t is_pll1);

// Massage the N and R values to satisfy restrictions while keeping the ratio constant
void pll_ConformDividers(uint64_t* N, uint64_t* R, uint8_t is_pll1);

// Determine the VCO and Output Settings of the PLL
void pll_SetVCO(uint64_t* reqFreq, pllparam_t* pllparam, uint8_t is_pll1);

// Determine the best value for PLL1.N
uint8_t pll_NCalc(uint64_t* reqFreq, pllparam_t* pll1, uint64_t* N);

// Scoring Function to Determine Best N value
void pll_NScoringFunction(pllparam_t* pll, uint64_t* N, double* score);

#endif
