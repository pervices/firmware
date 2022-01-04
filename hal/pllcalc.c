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

#ifdef _PLL_DEBUG_STANDALONE
// ==========================
// Main
// ==========================
int main(void) {
    uint64_t reqFreq = 250e6;
    pllparam_t pll = pll_def_adf5355;
//     pllparam_t pll = pll_def_lmx2595;
    const uint64_t core_ref_freq = pll.ref_freq;

    // Debug parameters;
    double max_diff = core_ref_freq / pll.R;
    double max_N = PLL1_N_MAX;
    double max_R = PLL1_R_MAX;
    int printdebug = 0;
    uint64_t stepFreq = (1000000);

    // for (reqFreq = stepFreq * 100; reqFreq <= stepFreq * 6250; reqFreq +=
    // stepFreq)
    for (reqFreq = 700e6; reqFreq <= 6800000000; reqFreq += stepFreq) {

        // This is the main function, everything after is for statistics
        setFreq(&reqFreq, &pll);

        double actual_reference = (double)(uint64_t)core_ref_freq;
        double actual_output = (double)pll.vcoFreq / (double)pll.d;

        // if (pll.x2en)	actual_output *= 2.0;
        // else		actual_output /= (double)pll1.d;

        double noise = ((double)pll.N) / ((double)pll.R);
        double dbc_noise1 = (double)pll.N / (double)pll.R;

        if (pll.x2en) {
            noise *= 2.0;
            dbc_noise1 *= 2.0;
            actual_output *= 2.0;
        } else {
            noise = noise / pll.d;
            dbc_noise1 = dbc_noise1 / pll.d;
        }

        double diff = (double)reqFreq - actual_output;

#ifdef _PLL_DEBUG_VERBOSE
        printf("Requested: %" PRIu64
               ", Reference: %.10lf, Output: %.10lf, Difference: %.10lf, "
               "Noise: %.10lf, Noise (dBc): %.10lf \n",
               reqFreq, actual_reference, actual_output, diff,
               20 * log10(noise), 20 * log10(dbc_noise1));
        printf("\t Using: VcoFreq: %Lf, Divider: %li, Rdiv: %li, Ndiv: %li, "
               "2xOut: %i\n",
               pll.vcoFreq, pll.d, pll.R, pll.N, pll.x2en);
#endif

        if (diff < 0) {
            diff *= -1.0;
        }

        if (diff > max_diff) {
            max_diff = diff;
            printdebug = 1;
        }

        if (pll.R > max_R) {
            max_R = pll.R;
            printdebug = 1;
        }

        if (pll.N > max_N) {
            max_N = pll.N;
            printdebug = 1;
        }

#ifdef _PLL_DEBUG_INFO
        if (printdebug) {
            printf("ERROR: Parameter Violation\n");
            printf("\t Requested: %" PRIu64
                   ", Reference: %.10lf, Output: %.10lf, Difference: %.10lf, "
                   "Noise: %.10lf, Noise (dBc): %.10lf \n",
                   reqFreq, actual_reference, actual_output, diff,
                   20 * log10(noise), 20 * log10(dbc_noise1));
            printf("\t Using: VcoFreq: %Lf, Divider: %li, Rdiv: %li, Ndiv: "
                   "%li, 2xOut: %i\n",
                   pll.vcoFreq, pll.d, pll.R, pll.N, pll.x2en);
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
double setFreq(uint64_t *reqFreq, pllparam_t *pll) {

    // Crimson has two PLLs feeding each other. The first PLL is used to
    // generate a reference frequency that is used by the second PLL to
    // synthesize a clean output mixing frequency.

    // The procedure to calculate all the PLL parameters is this:
    //
    // 1. Determine the VCO frequency we need to tune to.
    //	-This also effectively sets the divider or doubler values.
    //      -To ensure loop stability and coherency, we also set R
    //
    // 1a. If the output falls between the natural VCO frequency, we just use
    // the fundamental VCO frequency.
    //
    // 1b. If the output frequency is falls above the VCO band, we simply use
    // the doubler and divide the desired output by 2 to obtain the fundamental
    // VCO frequency.
    //
    // 1c. If the output is below the VCO band, we find the smallest divider we
    // can use
    //
    // 2. Once we've determined the doubler values, we use the need to select a
    // reasonable N value.
    //
    //  - This will depend on our feedback network.
    //  - Assumes a single PLL design, and therefore fixed reference.
    //

    uint64_t temp = *reqFreq;

    // round the required Frequency to the nearest 5MHz
    // uint64_t mhzFreq = (*reqFreq / 5e6); // MHz truncation
    //*reqFreq = mhzFreq * 5e6;

    // Sanitize the input to be within range
    if (*reqFreq > pll->rf_out_max)
        *reqFreq = pll->rf_out_max;
    else if (*reqFreq < pll->rf_out_min)
        *reqFreq = pll->rf_out_min;

    // 1. Determine VCO frequency, frequency doubler, and output divider values.
    pll_SetVCO(reqFreq, pll);

    // 2. Use the reference to determine N, and pfd frequency
    long double pd_freq =
            (long double)pll->ref_freq / (long double)pll->R;

    double N1 = 0;

    if (pll->id == PLL_ID_ADF5355) {
        // For phase coherency we need to stick with step sizes
        // corresponding to a reference as defined (currently 25e6)
        // However when reqFreq is below threshold we will divide
        // reference by 5.
        // Determine the values of the N and dividers for PLL1
        if (!pll->divFBen) {
            pll->divFBen = 0;
            N1 = (double)pll->vcoFreq / (double)pd_freq;
        } else {
            N1 = (double)pll->vcoFreq / (double)pll->d;
            N1 = N1 / (long double)pll->ref_freq;
            if (N1 < 1)
                N1 = 1;
        }
        pll->N = (uint32_t)N1;
        pll->N = (uint32_t)N1 * (uint32_t)pll->R;

        // Set correct, actual, VCO frequency based on output frequency
        if (!pll->divFBen) {
            pll->vcoFreq = (long double)pd_freq * (long double)pll->N;
        } else {
            pll->vcoFreq =
                (long double)pd_freq * (uint64_t)pll->N * (uint64_t)pll->d;
        }
    } else if (pll->id == PLL_ID_LMX2595) {
        // determine includedDivide so we know how much to divide N by
        uint8_t includedDivide = 1;
        if (pll->d > 1) {
            if ((pll->d % 3 == 0 ) && (pll->d != 24) && (pll->d != 192)) {
                includedDivide = 6;
            } else {
                includedDivide = 4;
            }
        }
        N1 = (double)pll->vcoFreq / (double)pd_freq / (double)includedDivide;
        pll->N = (uint32_t)N1;
        // set VCO to actual value based on N
        pll->vcoFreq = (long double)pd_freq * (uint64_t)pll->N * includedDivide;
    }

    if (!pll_CheckParams(pll, 1)) {
#ifndef _PLL_DEBUG_STANDALONE
        PRINT(INFO,
              "Bad pll calc result, retrying: PLL1: N: %" PRIu32 ", R: %" PRIu16
              ", D: %" PRIu16 ", x2en: %" PRIu8 ", VCO: %Lf.\n",
              pll->N, pll->R, pll->d, pll->x2en, pll->vcoFreq);
#else
        printf("BAD PLL SETTINGS: PLL1: N: %" PRIu32 ", R: %" PRIu16
               ", D: %" PRIu16 ", x2en: %" PRIu8 ", VCO: %Lf.\n",
               pll->N, pll->R, pll->d, pll->x2en, pll->vcoFreq);
#endif
    }

    *reqFreq = temp;
    double actual_output =
        ((double)pll->vcoFreq / (double)pll->d) +
        (pll->x2en) * ((double)pll->vcoFreq / (double)pll->d);

    return actual_output;
};

uint8_t pll_CheckParams(pllparam_t *pll, uint8_t is_pll1) {

    if (!is_pll1) {//currently only PLL1 is supported for ADF5355
        return 0;
    }
        
    // checks for all PLLs
    if ((pll->N > pll->n_max) || (pll->N < pll->n_min) ||                         // N is valid
        (pll->R > pll->r_max) || (pll->R < pll->r_min) ||                         // R is valid
        (pll->d > pll->d_max) || (pll->d < pll->d_min) ||                         // d is valid
        (pll->vcoFreq > pll->vco_f_max) || (pll->vcoFreq < pll->vco_f_min) ||     // vco freq is valid
        (pll->x2en > 1)) {
        return 0;
        }
    // PLL specific checks
    if (pll->id == PLL_ID_ADF5355) { //
        if ((pll->R > _PLL_RATS_MAX_DENOM) ||         // ensure R is not greater than _PLL_RATS_MAX_DENOM
            (pll->d > 1 && (pll->d & 1) != 0)) {      // ensure d is an even number
            return 0;
        }
    }else if (pll->id == PLL_ID_LMX2595) { //
        if ((pll->vcoFreq > LMX2595_VCO_MAX2_HZ) && (pll->d > LMX2595_D_THRESH_VCO)){    // different VCO freq limit if d is too high
            return 0;
        }
    }

    return 1;
}

void pll_SetVCO(uint64_t *reqFreq, pllparam_t *pll) {
    // Configure VCO frequency and DIV values for PLL1
    if (*reqFreq > pll->vco_f_max) {                    // if we need doubler
        pll->x2en = 1;
        pll->d = 1;
        pll->vcoFreq = *reqFreq / 2;
    } else if (*reqFreq > pll->vco_f_min) {             // if we don't need doubler or divider
        pll->x2en = 0;
        pll->d = 1;
        pll->vcoFreq = *reqFreq;
    } else if (pll->id == PLL_ID_ADF5355){              // determine divider for ADF5355
        pll->x2en = 0;
        double D_float = (double)pll->vco_f_min / (double)*reqFreq;
        uint16_t D = ceil(D_float);
        { // round up to nearest power of 2 (ADF4355/ADF5355)
            D--;
            D = D | (D >> 1);
            D = D | (D >> 2);
            D = D | (D >> 4);
            D++;
        }
        if (D > pll->d_max)
            D = pll->d_max;
        if (D < pll->d_min)
            D = pll->d_min;
        pll->d = D;
        pll->vcoFreq = (uint64_t)D * *reqFreq;
    } else if (pll->id == PLL_ID_LMX2595){             // determine divider for LMX2595
        pll->x2en = 0;
        // determine D based on Table 8 pg 27 of LMX2595 datasheet
        uint16_t D = 0;
        if (*reqFreq > 3750000000 ) { D = 2; }
        else if (*reqFreq > 1875000000 ) { D = 4; }
        else if (*reqFreq > 1250000000 ) { D = 6; }
        else if (*reqFreq > 950000000  ) { D = 8; }
        else if (*reqFreq > 630000000 ) { D = 12; }
        else if (*reqFreq > 470000000 ) { D = 16; }
        else if (*reqFreq > 320000000 ) { D = 24; }
        else if (*reqFreq > 235000000 ) { D = 32; }
        else if (*reqFreq > 160000000 ) { D = 48; }
        else if (*reqFreq > 130000000 ) { D = 64; }
        else if (*reqFreq > 110000000 ) { D = 72; }
        else if (*reqFreq > 80000000 ) { D = 96; }
        else if (*reqFreq > 60000000 ) { D = 128; }
        else if (*reqFreq > 40000000 ) { D = 192; }
        else if (*reqFreq > 30000000 ) { D = 256; }
        else if (*reqFreq > 19532000 ) { D = 384; }
        //else if (*reqFreq > 20000000 ) {D = 384; }    // to allow synchronizing for phase coherency across RF channels D < 512
        //else if (*reqFreq > 15000000 ) { D = 512; }
        //else if (*reqFreq >= 10000000 ) { D = 768; }
        else { D = 0 ;}                                 // if reqFreq is too low d=0 will cause error during check
        pll->d = D;
        pll->vcoFreq = (uint64_t)(D * (*reqFreq));
    } else {                                            // if unknown IC set d to zero, which is expected to
        pll->d = 0;                                     // cause an error because d should always be 1 or more
    }
}
