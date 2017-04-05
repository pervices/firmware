/**
* \file pllcalc.c
*
* \brief PLL Value calculator
*
*/

/**
* \mainpage PLL Value calculator
*
* \par PLL Calculator
*
* This is the main PLL calculator file, which determines the correct values
* to feed into the HMC PLLs.
*
* \par Content
*
* This application is called by the Rx and Tx boards. Its purpose is to:
* 
* -# Calculate the optimal values for the first and second PLL.
*
* Note that while functions are declared first, they are defined after main().
* 
*/

#include "pllcalc.h"

// PLL Constructors

pllparam_t pll_def = {
        PLL1_R_FIXED,
        PLL1_N_DEFAULT,
        PLL1_D_DEFAULT,
        PLL1_X2EN_DEFAULT,
        PLL1_OUTFREQ_DEFAULT,
        PLL1_FB_DEFAULT
};

#ifdef _PLL_DEBUG_STANDALONE
// ==========================
// Main
// ==========================
int main (void)
{
        uint64_t reqFreq = 250e6;
        pllparam_t pll;
        
        //Debug parameters;
        double max_diff = 5000001;
        double max_N = PLL1_N_MAX;
        double max_R = PLL1_R_MAX;
        int printdebug = 0;
        uint64_t stepFreq = (1000000);
        
        //for (reqFreq = stepFreq * 100; reqFreq <= stepFreq * 6250; reqFreq += stepFreq)
	for (reqFreq = 2600000000; reqFreq <= 2900000000; reqFreq += stepFreq)
        {

                // This is the main function, everything after is for statistics
                setFreq(&reqFreq, &pll);

                double actual_reference = (double)(uint64_t)PLL_CORE_REF_FREQ_HZ;
                double actual_output = (double)pll.vcoFreq / (double)pll.d;

                //if (pll.x2en)	actual_output *= 2.0;
                //else		actual_output /= (double)pll1.d;

                double diff = (double)reqFreq - actual_output;
                double noise = ((double)pll.N ) / ((double)pll.R);
                double dbc_noise1 = (double) pll.N / (double) pll.R;

                if (pll.x2en) {
                        noise *= 2.0;
                        dbc_noise1 *= 2.0;
                } else {
                        noise = noise / pll.d;
                        dbc_noise1 = dbc_noise1 / pll.d;
                }

                dbc_noise1 = dbc_noise1 / 10000;

                #ifdef _PLL_DEBUG_VERBOSE
                printf("Requested: %"PRIu64", Reference: %.10lf, Output: %.10lf, Difference: %.10lf, Noise: %.10lf, Noise (dBc): %.10lf \n",
                                reqFreq,
                                actual_reference,
                                actual_output,
                                diff,
                                20 * log10(noise),
                                20 * log10(dbc_noise1)
                        );
                printf("\t Using: VcoFreq: %Lf, Divider: %li, Rdiv: %li, Ndiv: %li, 2xOut: %i\n",
                                pll.vcoFreq,
                                pll.d,
                                pll.R,
                                pll.N,
                                pll.x2en
                        );
                #endif

                if (diff < 0) {
                        diff *= -1.0;
                }

                if (diff > max_diff)  {
                        max_diff = diff;
                        printdebug = 1;
                }

                if (pll.R > max_R)  {
                        max_R = pll.R;
                        printdebug = 1;
                }

                if (pll.N > max_N)  {
                        max_N = pll.N;
                        printdebug = 1;
                }

                #ifdef _PLL_DEBUG_INFO
                if (printdebug) {
                    printf("ERROR: Parameter Violation\n");
                    printf("\t Requested: %"PRIu64", Reference: %.10lf, Output: %.10lf, Difference: %.10lf, Noise: %.10lf, Noise (dBc): %.10lf \n",
                                    reqFreq,
                                    actual_reference,
                                    actual_output,
                                    diff,
                                    20 * log10(noise),
                                    20 * log10(dbc_noise1)
                            );
                    printf("\t Using: VcoFreq: %Lf, Divider: %li, Rdiv: %li, Ndiv: %li, 2xOut: %i\n",
                                    pll.vcoFreq,
                                    pll.d,
                                    pll.R,
                                    pll.N,
                                    pll.x2en
                            );
                    printdebug = 0;
                }
                #endif
        }

        #ifdef _PLL_DEBUG_INFO
            fprintf(stderr, "MAXIMUM ERROR: %.10lf.\n", max_diff);
        #endif
        return 0;
        
};
#endif

// ==========================
// Common functions
// ==========================
double setFreq(uint64_t* reqFreq, pllparam_t* pll) {

// Crimson has two PLLs feeding each other. The first PLL is used to generate
// a reference frequency that is used by the second PLL to synthesize a clean
// output mixing frequency.

// The procedure to calculate all the PLL parameters is this:
//
// 1. Determine the VCO frequency we need to tune to.
//	-This also effectively sets the divider or doubler values.
//      -To ensure loop stability and coherency, we also set R
//
// 1a. If the output falls between the natural VCO frequency, we just use the fundamental VCO frequency.
//
// 1b. If the output frequency is falls above the VCO band, we simply use the doubler and divide the desired output by 2 to obtain the fundamental VCO frequency.
//
// 1c. If the output is below the VCO band, we find the smallest divider we can use
// 
// 2. Once we've determined the doubler values, we use the need to select a reasonable N value.
//
//  - This will depend on our feedback network.
//  - Assumes a single PLL design, and therefore fixed reference.
//

        *pll = pll_def;

        uint64_t temp = *reqFreq;

        // round the required Frequency to the nearest 5MHz
        //uint64_t mhzFreq = (*reqFreq / 5e6); // MHz truncation
        //*reqFreq = mhzFreq * 5e6;

        // Sanitize the input to be within range
        if (*reqFreq > PLL1_RFOUT_MAX_HZ) 	*reqFreq = PLL1_REF_MAX_HZ;
        else if (*reqFreq < PLL1_RFOUT_MIN_HZ) 	*reqFreq = PLL1_RFOUT_MIN_HZ;

        // 1. Determine VCO frequency, and also frequency doubler or output divider values.
        pll_SetVCO(reqFreq, pll);
	
        // 2. Use the reference to determine R, N, and pfd frequency
       long double pd_freq = (long double)PLL_CORE_REF_FREQ_HZ / (long double)pll->R;
	

        double N1 = 0;
        // Determine the values of the N and dividers for PLL1
        if ( !pll->divFBen || *reqFreq < PLL1_FB_THRESHOLD ) {
            N1  = (double)pll->vcoFreq / (double)pd_freq;
        } else {
            N1  = (double)pll->vcoFreq / (double)pll->d;
            N1  = N1 / (long double)pd_freq;
	    if (N1 < 1) N1 = 1;
        }
        pll->N = (uint32_t)N1;

        //Set correct, actual, VCO frequency based on output frequency
	if ( !pll->divFBen || *reqFreq < PLL1_FB_THRESHOLD ) {
            pll->vcoFreq = (long double)pd_freq * (long double)pll->N;
        } else {
            pll->vcoFreq = (long double)pd_freq * (uint64_t)pll->N * (uint64_t)pll->d;
        }

    if (!pll_CheckParams(pll, 1)) {
#ifndef _PLL_DEBUG_STANDALONE
        PRINT( ERROR, "BAD PLL SETTINGS: PLL1: N: %"PRIu32", R: %"PRIu16", D: %"PRIu16", x2en: %"PRIu8", VCO: %Lf.\n",
                pll->N,
                pll->R,
                pll->d,
                pll->x2en,
                pll->vcoFreq);
#else
        printf("BAD PLL SETTINGS: PLL1: N: %"PRIu32", R: %"PRIu16", D: %"PRIu16", x2en: %"PRIu8", VCO: %Lf.\n",
                pll->N,
                pll->R,
                pll->d,
                pll->x2en,
                pll->vcoFreq);
#endif
    }

        *reqFreq = temp;
        double actual_output = ((double)pll->vcoFreq / (double)pll->d) + (pll->x2en)*((double)pll->vcoFreq / (double)pll->d) ;

        return actual_output;
};


uint8_t pll_CheckParams(pllparam_t* pllparam, uint8_t is_pll1) {

    if (is_pll1) {
        if ((pllparam->N > PLL1_N_MAX) 			||
            (pllparam->N < PLL1_N_MIN) 			||
            (pllparam->R > _PLL_RATS_MAX_DENOM) 	||
            (pllparam->R > PLL1_R_MAX)           	||
            (pllparam->R < PLL1_R_MIN)           	||
            (pllparam->d > PLL1_DIV_MAX) 		||
            (pllparam->d < 1) 				||
            (pllparam->d > 1 && (pllparam->d & 1) != 0) ||
            (pllparam->x2en > 1) 			||
            (pllparam->vcoFreq > PLL1_VCO_MAX_HZ) 	||
            (pllparam->vcoFreq < PLL1_VCO_MIN_HZ)) {
                return 0;
        }
    }

    return 1;
}


void pll_SetVCO(uint64_t* reqFreq, pllparam_t* pll) {
        // Configure VCO frequency and DIV values for PLL1
        if (*reqFreq > PLL1_VCO_MAX_HZ) {
            pll->x2en = 1;
            pll->d = 1;
            pll->vcoFreq = *reqFreq / 2;
        } else if (*reqFreq > PLL1_VCO_MIN_HZ) {
            pll->x2en = 0;
            pll->d = 1;
            pll->vcoFreq = *reqFreq;
        } else {
            pll->x2en = 0;
            double D_float = (double)PLL1_VCO_MIN_HZ / (double)*reqFreq;
            uint16_t D = ceil(D_float);
            {	// round up to nearest power of 2 (ADF4355/ADF5355)
                D--;
                D = D | (D >> 1);
                D = D | (D >> 2);
                D = D | (D >> 4);
                D++;
            }
            if (D > PLL1_DIV_MAX) D = PLL1_DIV_MAX;
            if (D < PLL1_DIV_MIN) D = PLL1_DIV_MIN;
            pll->d = D;
            pll->vcoFreq = (uint64_t)D * *reqFreq;
        }
}

