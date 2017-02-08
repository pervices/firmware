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
        PLL1_R_DEFAULT,
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
        
        for (reqFreq = stepFreq * 100; reqFreq <= stepFreq * 6250; reqFreq += stepFreq)
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
                printf("\t Using: VcoFreq: %li, Divider: %li, Rdiv: %li, Ndiv: %li, 2xOut: %i\n",
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
                    printf("\t Using: VcoFreq: %li, Divider: %li, Rdiv: %li, Ndiv: %li, 2xOut: %i\n",
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
        uint64_t mhzFreq = (*reqFreq / 5e6); // MHz truncation
        *reqFreq = mhzFreq * 5e6;

        // Sanitize the input to be within range
        if (*reqFreq > PLL1_RFOUT_MAX_HZ) 	*reqFreq = PLL1_REF_MAX_HZ;
        else if (*reqFreq < PLL1_RFOUT_MIN_HZ) 	*reqFreq = PLL1_RFOUT_MIN_HZ;

        // 1. Determine VCO frequency, and also frequency doubler or output divider values.
        pll_SetVCO(reqFreq, pll);


        // 2. Use the reference to determine R, N, and pfd frequency
        uint64_t reference = (uint64_t)PLL_CORE_REF_FREQ_HZ;
        uint64_t pd_freq = PLL_CORE_REF_FREQ_HZ / (uint64_t)pll->R;

        uint32_t N1 = 0;
        // Determine the values of the N and R dividers for PLL1
        if ( !pll->divFBen ) {
            N1  = (double)pll->vcoFreq / (double)pd_freq;
        } else {
            N1  = (double)pll->vcoFreq / (double)pll->d;
            N1  = N1 / (double)pd_freq;
            //N1 = floor( N1 / (double) pll->d );
        }
        pll->N = (uint32_t)N1;

    if (!pll_CheckParams(pll, 1)) {
#ifndef _PLL_DEBUG_STANDALONE
        PRINT( ERROR, "BAD PLL SETTINGS: PLL1: N: %"PRIu32", R: %"PRIu16", D: %"PRIu16", x2en: %"PRIu8", VCO: %"PRIu64".\n",
                pll->N,
                pll->R,
                pll->d,
                pll->x2en,
                pll->vcoFreq);
#else
        printf("BAD PLL SETTINGS: PLL1: N: %"PRIu32", R: %"PRIu16", D: %"PRIu16", x2en: %"PRIu8", VCO: %"PRIu64".\n",
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

//
// Calculate reasonable PLL1 input reference refrequency; 
// This configures the VCO and Divider values of PLL1.
//

void pll_RefCalc(uint64_t* reqFreq, pllparam_t* pll1) {
    uint64_t ref_freq = PLL1_REF_MAX_HZ;

    //// Also maximizes the reference frequency in order to reduce PLL1's N/R value
    //// Algorithm ensures that ref_freq is within specified range
    //if (*reqFreq > PLL1_REF_MAX_HZ) {

    //    uint64_t min_factor = *reqFreq / PLL1_REF_MAX_HZ;
    //    min_factor += 1; // round up, safer
    //    if ((min_factor & 1) != 0) min_factor += 1; // even factors simplify the PLL1 ratio
    //    ref_freq = *reqFreq / min_factor;

    //} else if (*reqFreq > (PLL1_REF_MAX_HZ / 2) && (*reqFreq > PLL1_REF_MIN_HZ)) {

    //    ref_freq = *reqFreq;

    //} else {// (*reqFreq < (PLL1_REF_MAX_HZ / 2))

    //    uint64_t min_multiple = PLL1_REF_MAX_HZ / *reqFreq;
    //    ref_freq = *reqFreq * min_multiple;

    //}

    // Sanitation Check
    if (ref_freq > PLL1_REF_MAX_HZ) 		ref_freq = PLL1_REF_MAX_HZ;
    else if (ref_freq < PLL1_REF_MIN_HZ)	ref_freq = PLL1_REF_MIN_HZ;

    // Configure VCO frequency and DIV values for PLL1
    if (*reqFreq > PLL1_VCO_MAX_HZ) {
        pll1->x2en = 1;
        pll1->d = 1;
        pll1->vcoFreq = *reqFreq / 2;
    } else if (*reqFreq > PLL1_VCO_MIN_HZ) {
        pll1->x2en = 0;
        pll1->d = 1;
        pll1->vcoFreq = *reqFreq;
    } else {
        uint16_t D = PLL1_VCO_MIN_HZ / *reqFreq;
        D += 1; // round up, safer
        if ((D & 1) != 0) D += 1;
        if (D > PLL1_DIV_MAX) D = PLL1_DIV_MAX;
        pll1->vcoFreq = (uint64_t)D * *reqFreq;
        pll1->d = D;
        pll1->x2en = 0;
    }
}

//Modified from the Rosetta Code;
void rat_approx(double f, uint64_t md, uint64_t* num, uint64_t* denom) {
        /*  a: continued fraction coefficients. */ 
        uint64_t a, h[3] = { 0, 1, 0 }, k[3] = { 1, 0, 0 };
        uint64_t x, d, n = 1;
        uint8_t i, neg = 0;

//         //If the max denominator is <= 1, set the denominator to 1 and return the number
        if (md <= 1) { *denom = 1; *num = (uint64_t) f; return; }

        if (f < 0) { neg = 1; f = -f; }

        while (f != floor(f)) { n <<= 1; f *= 2; }
        d = f;

        /* continued fraction and check denominator each step */
        for (i = 0; i < 64; i++) {
                a = n ? d / n : 0;
                if (i && !a) break;

                x = d; d = n; n = x % n;

                x = a;
                if (k[1] * a + k[0] >= md) {
                        x = (md - k[0]) / k[1];
                        if (x * 2 >= a || k[1] >= md)
                                i = 65;
                        else
                                break;
                }

                h[2] = x * h[1] + h[0]; h[0] = h[1]; h[1] = h[2];
                k[2] = x * k[1] + k[0]; k[0] = k[1]; k[1] = k[2];
        }
        *denom = k[1];
        *num = neg ? -h[1] : h[1];
}

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

void pll_ConformDividers(uint64_t* N, uint64_t* R, pllparam_t* pll) {
    // D is forced in pll_RefCalc to be even and less than 62, if D > 1
    // R is forced to not exceed its maximum due to the continued fraction algorithm in rat_approx
    // N however, may exceed the maximum or minimum bounds that have been set
    // R also has a possiblity of violating the MINIMUM bound, if there is one
    uint64_t Nt = *N;
    uint64_t Rt = *R;

    // If the N maximum bound is exceeded
    // Calculate the worst case N/R delta
    // Move within the +/- of the delta to check for good N values

    double ratio = (double)Nt / (double)Rt;

    if (pll->divFBen) ratio = ratio / pll->d;
    double min_N_ratio = ratio;

    if (Nt > PLL1_N_MAX) {
        double worst_delta = (double)_PLL_OUT_MAX_DEVIATION / (double)PLL_CORE_REF_FREQ_HZ;
        double del = worst_delta / 10.0;
        uint64_t Nmin = Nt;
        uint64_t NminR = Rt;
        uint16_t MAX_R = _PLL_RATS_MAX_DENOM;

        for (MAX_R = _PLL_RATS_MAX_DENOM; MAX_R > 1; MAX_R = MAX_R / 2) {
            ratio = min_N_ratio;
            ratio += worst_delta;
            uint8_t range;
            for (range = 20 + 1; range > 0; range--) { 
                // 20 + 1 to keep range positive
                rat_approx(ratio, MAX_R, &Nt, &Rt);
                if (Nmin > Nt) {
                    Nmin = Nt;
                    min_N_ratio = ratio;
                    NminR = Rt;
                }
                if (Nt < (uint64_t)PLL1_N_MAX) break;
                else ratio -= del;
            }
            if (Nt < (uint64_t)PLL1_N_MAX) break;
#ifdef _PLL_DEBUG_INFO
            printf("Reducing Current-MAX_R: %i.\n", MAX_R);
#endif
            if (MAX_R < (50)) {
                                Nt = Nmin;
                                Rt = NminR;
                                ratio = min_N_ratio;
#ifndef _PLL_DEBUG_STANDALONE
                PRINT( ERROR, "ERROR: Unable to find proper solution (N too large): %"PRIu64", and unable to further reduce N.\n", Nmin);
#endif
                break;
            }
        }
    }
        *N = Nt;
        *R = Rt;

    // If the N and R minimum bounds are violated
    uint64_t MIN_N = PLL1_N_MIN;
    uint64_t MIN_R = PLL1_R_MIN;

    uint64_t multiplier;
    for (multiplier = 2; (Nt < MIN_N) || (Rt < MIN_R); multiplier++) {
        Nt = *N * multiplier;
        Rt = *R * multiplier;
    }
#ifndef _PLL_DEBUG_STANDALONE
        PRINT( VERBOSE,"CD: Best N found: Nt: %li.\n", Nt);
        PRINT( VERBOSE,"CD: Best R found: Rt: %li.\n", Rt);
#endif

    *N = Nt;
    *R = Rt;
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

uint8_t pll_NCalc(uint64_t* reqFreq, pllparam_t* pll1, uint64_t* N) {
        uint8_t rounding_performed = 0;

        // Determine the upper and lower bounds of N for PLL1
        uint64_t Nmax = PLL1_N_MAX;
        uint64_t Nmin = PLL1_N_MIN;

        if ( !pll1->divFBen ) {
            Nmax = floor((double)pll1->vcoFreq / (double)PLL1_PD_MIN_HZ);
        } else {
            Nmax = floor((double)pll1->vcoFreq / (double)pll1->d / (double)PLL1_PD_MIN_HZ);
        }

        if (PLL1_PD_MAX_HZ < PLL1_REF_MAX_HZ) {
                Nmin = ceil((double)pll1->vcoFreq / (double)PLL1_PD_MAX_HZ);
        } else {
                Nmin = ceil((double)pll1->vcoFreq / (double)PLL1_REF_MAX_HZ);
        }

        // Truncate N if out-of-range
        if (Nmax > PLL1_N_MAX) Nmax = PLL1_N_MAX;
        else if (Nmin < PLL1_N_MIN) Nmin = PLL1_N_MIN;
        
        uint64_t Ntemp = Nmin;
        uint64_t Nbest = Ntemp;
        double score = -1;
        double best_score = score;

        for (Ntemp = Nmin; Ntemp <= Nmax; Ntemp++) {
                pll_NScoringFunction(pll1, &Ntemp, &score);
                if (score > best_score) {
                        Nbest = Ntemp;
                        best_score = score;
                }
        }
        *N = Nbest;

        return 0;
}

void pll_NScoringFunction(pllparam_t* pll, uint64_t* N, double* score) {
        
        double div_value = (double)pll->vcoFreq / (double) *N;
        if ( !pll->divFBen ) {
            div_value = div_value / (double)pll->d;
        }

        uint64_t remainder = 0;
        uint64_t divisor = 1;

        while (remainder == 0) {
                remainder = fmodf(div_value,divisor);
                divisor = divisor * 10;
        }

        *score = (double)log10(divisor) / (double) *N;
}
