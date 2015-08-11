/**
* \file pllcalc.c
*
* \brief PLL Value calculator
*
* Compile with:
* gcc -lm pllcalc.c
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

#define _PLLDEBUG_ON
#define _PLLDEBUG_OFF_VERBOSE

#define PLL0_R_DEFAULT		( 2 )		// R value (we aim to minimize this)
#define PLL0_N_DEFAULT		( 30 )          // N value (N^2 contribution to PLL noise) [16..4096]
#define PLL0_D_DEFAULT		( 30 )		// RFoutput divider value (1,2,4,6..58,60,62)
#define PLL0_X2EN_DEFAULT	( 0 )           // RFoutput doubler enabled (0=off, 1=on (RFout is doubled))
#define PLL0_OUTFREQ_DEFAULT	( 50000000 )	// Resulting VCO Output Frequency

#define PLL1_R_DEFAULT		( 1 )		// R value (we aim to minimize this)
#define PLL1_N_DEFAULT		( 30 )          // N value (N^2 contribution to PLL noise) [16..4096]
#define PLL1_D_DEFAULT		( 30 )		// RFoutput divider value (1,2,4,6..58,60,62)
#define PLL1_X2EN_DEFAULT	( 0 )           // RFoutput doubler enabled (0=off, 1=on (RFout is doubled))
#define PLL1_OUTFREQ_DEFAULT	( 1500000000 )	// Resulting VCO Output Frequency

// PLL Constructors
pllparam_t pll0_def = {
	PLL0_R_DEFAULT,
	PLL0_N_DEFAULT,
	PLL0_D_DEFAULT,
	PLL0_X2EN_DEFAULT,
	PLL0_OUTFREQ_DEFAULT
};

pllparam_t pll1_def = {
	PLL1_R_DEFAULT,
	PLL1_N_DEFAULT,
	PLL1_D_DEFAULT,
	PLL1_X2EN_DEFAULT,
	PLL1_OUTFREQ_DEFAULT
};

// ==========================
// Common functions
// ==========================
double setFreq(uint64_t* reqFreq, pllparam_t* pll0, pllparam_t* pll1) {

  // Crimson has two PLLs feeding each other. The first PLL is used to generate
  // a reference frequency that is used by the second PLL to synthesize a clean
  // output mixing frequency.

  // The procedure to calculate all the PLL parameters is this:
  //
  // 1. Determine the VCO frequency we need to tune the 2nd PLL (output) to. 
  //	-This also effectively sets the divider or doubler values.
  //
  // 1a. If the output falls between the natural VCO frequency, we just use the fundamental VCO frequency.
  //
  // 1b. If the output frequency is between 3-6GHz, we just use the doubler and divide the desired output by 2 to obtain the fundamental VCO frequency.
  //
  // 1c. If the output falls below 3GHz, then we find the smallest divider we can use (for stability, if the frequency is greater than 1500
  // 
  // 2. Once we've determined the doubler values, we need to select a reasonable N value.
  //
  // 2a. The minimum N value is 16.
  //
  // 2b. We have chosen the PFD frequency range to be between 2.5-70MHz. This effectively places an upper bound on N.
  //	(This ensures we can use the same code when operating in Exact-Frequency Franctional mode, while also ensuring we don't do anything too crazy)
  //
  // 3. Once we have selected a reasonable N value, we can assume that to be the reference, which (in the ideal case) sees us setting R=1 (best phase performance).
  //
  // 4. Once we have figured out the reference, we can use that to calculate the PLL0 parameters, from which we have both PLL0 and PLL1 parameters.

	*pll0 = pll0_def;
	*pll1 = pll1_def;

	uint64_t temp = *reqFreq;

	// round the required Frequency to the nearest MHz
	uint64_t mhzFreq = (*reqFreq / 1e6); // MHz truncation
	uint64_t hunkhzFreq = *reqFreq / 1e5;// 100kHz truncation
	mhzFreq *= 10;
	if ((hunkhzFreq - mhzFreq) > 4) {	// round accordingly
		mhzFreq += 10;
	}

	*reqFreq = mhzFreq * 1e5;

    // Sanitize the input to be within range
    if (*reqFreq > PLL1_RFOUT_MAX_HZ) 		*reqFreq = PLL1_RFOUT_MAX_HZ;
    else if (*reqFreq < PLL1_RFOUT_MIN_HZ) 	*reqFreq = PLL1_RFOUT_MIN_HZ;

	// Determine the VCO and Output Divider (d) values of PLL1
	pll_SetVCO(reqFreq, pll1, 1);

	uint64_t N1 = 1;
	uint64_t R1 = 1;

	// Determine best values of the Feedback Divider (N)
	if (pll_NCalc(reqFreq, pll1, &N1)) {
		//printf("REQUIRED FREQUENCY HAS BEEN ROUNDED TO THE NEAREST 100 Hz.\n");
	}

	pll1->N = N1;
	uint64_t pd_input = pll1->outFreq / pll1->N;

	// Calculate the necessary Reference Divider (R) value within the restrictions defined
	R1 = PLL1_REF_MAX_HZ / pd_input; // floor happens as its an integer operation

	if (R1 > _PLL_RATS_MAX_DENOM) pll1->R = _PLL_RATS_MAX_DENOM;
	else if (R1 < PLL1_R_MIN) pll1->R = PLL1_R_MIN;

	pll1->R = R1;

	// Determine the Reference Frequency required
	uint64_t reference = pd_input * pll1->R;

	// Determine the VCO and Output Divider (d) values of PLL0
	pll_SetVCO(&reference, pll0, 0);
	

    //// Determine the PLL VCO Frequencies and output divider for PLL0 and PLL1
    //pll_RefCalc(reqFreq, pll0, pll1);

    //// Determine the values of the N and R dividers for PLL0
    double pll0_ratio = (double)pll0->outFreq / (double)PLL_CORE_REF_FREQ_HZ;

    uint64_t N0 = 1;
    uint64_t R0 = 1;

    rat_approx(pll0_ratio, (uint64_t)_PLL_RATS_MAX_DENOM, &N0, &R0);

    // Massage N0 and R0 values to fit within the specified restrictions
    pll_ConformDividers(&N0, &R0, 0);

    // Assign the new N0 and R0 values to PLL0
    pll0->N = (uint32_t)N0;
    pll0->R = (uint16_t)R0;

    // Recalculate the VCO frequency as there is a dependency on N0/R0
    pll0->outFreq = round(((double)PLL_CORE_REF_FREQ_HZ * (double)pll0->N) / (double)pll0->R);

    //// Determine the values of the N and R dividers for PLL1
    //uint64_t reference = (pll0->outFreq * (uint64_t)(pll0->x2en + 1)) / (uint64_t)pll0->d;
    //double pll1_ratio = (double)pll1->outFreq / (double)reference;

    ////uint64_t N1 = 1;
    ////uint64_t R1 = 1;

    //rat_approx(pll1_ratio, (uint64_t)_PLL_RATS_MAX_DENOM, &N1, &R1);

    //// Massage the N1 and R1 values to fit within the specified restrictions
    //pll_ConformDividers(&N1, &R1, 1);

    //// Assign the new N1 and R1 values to PLL1
    //pll1->N = (uint32_t)N1;
    //pll1->R = (uint16_t)R1;

    //// Recalculate the VCO frequency as there is a dependency on N1/R1
    //pll1->outFreq = (reference * pll1->N) / pll1->R;

#ifdef _PLL_DEBUG_RAM_ON
    printf("setFreq PLL1-N: %li, PLL1-R: %i.\n", pll1->N, pll1->R);
    printf("setFreq PLL1-VCO: %li.\n",pll1->outFreq); // PLL1-N/R: %lf.\n", pll1->outFreq, pll1_ratio);
    printf("setFreq PLL0-VCO: %li, PLL0-x2en: %i, PLL0-D: %i.\n", pll0->outFreq, pll0->x2en, pll0->d);
    printf("setFreq RefFreq: %li.\n", reference);
    printf("setFreq PLL1-VCO: %li, PLL1-D: %i.\n", pll1->outFreq, pll1->d);
#endif

    if (!pll_CheckParams(pll0, 0)) {
        fprintf(stderr, "BAD PLL SETTINGS: PLL0: N: %"PRIu32", R: %"PRIu16", D: %"PRIu16", x2en: %"PRIu8", VCO: %"PRIu64".\n",
                pll0->N,
                pll0->R,
                pll0->d,
                pll0->x2en,
                pll0->outFreq);
    }

    if (!pll_CheckParams(pll1, 1)) {
        fprintf(stderr, "BAD PLL SETTINGS: PLL0: N: %"PRIu32", R: %"PRIu16", D: %"PRIu16", x2en: %"PRIu8", VCO: %"PRIu64".\n",
                pll1->N,
                pll1->R,
                pll1->d,
                pll1->x2en,
                pll1->outFreq);
    }

	*reqFreq = temp;


	double actual_reference = (double)((uint64_t)PLL_CORE_REF_FREQ_HZ * pll0->N) / (double)(pll0->R);


    if (pll0->x2en)	actual_reference *= 2.0;
	else 		actual_reference /= (double)pll0->d;

	double actual_output = (actual_reference * (double)pll1->N) / (double)pll1->R;

	if (pll1->x2en)	actual_output *= 2.0;
	else		actual_output /= (double)pll1->d;

    return actual_output;
};

//
// Calculate reasonable PLL1 input reference refrequency; 
// This configures the VCO and Divider values of PLL0 and PLL1.
//

void pll_RefCalc(uint64_t* reqFreq, pllparam_t* pll0, pllparam_t* pll1) {
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

    //// Configure VCO frequency and DIV values for PLL0
    //if (ref_freq > PLL0_VCO_MAX_HZ) {
    //    pll0->x2en = 1;
    //    pll0->d = 1;
    //    pll0->outFreq = ref_freq / 2;

    //} else if (ref_freq > PLL0_VCO_MIN_HZ) {
    //    pll0->x2en = 0;
    //    pll0->d = 1;
    //    pll0->outFreq = ref_freq;

    //} else {
    //    uint16_t D = PLL0_VCO_MIN_HZ / ref_freq;
    //    D += 1; // round up, safer
    //    if ((D & 1) != 0) D += 1;
    //    if (D > PLL0_DIV_MAX) D = PLL0_DIV_MAX;
    //    pll0->outFreq = (uint64_t)D * ref_freq;
    //    pll0->d = D;
    //    pll0->x2en = 0;
    //}
	
	//pll0->outFreq = 1600000000;
	//pll0->d = 16;
	//pll0->x2en = 0;
	//pll0->N = 16;
	//pll0->R = 1;

    // Configure VCO frequency and DIV values for PLL1
    if (*reqFreq > PLL1_VCO_MAX_HZ) {
        pll1->x2en = 1;
        pll1->d = 1;
        pll1->outFreq = *reqFreq / 2;
    } else if (*reqFreq > PLL1_VCO_MIN_HZ) {
        pll1->x2en = 0;
        pll1->d = 1;
        pll1->outFreq = *reqFreq;
    } else {
        uint16_t D = PLL1_VCO_MIN_HZ / *reqFreq;
        D += 1; // round up, safer
        if ((D & 1) != 0) D += 1;
        if (D > PLL1_DIV_MAX) D = PLL1_DIV_MAX;
        pll1->outFreq = (uint64_t)D * *reqFreq;
        pll1->d = D;
        pll1->x2en = 0;
    }

#ifdef _PLL_DEBUG_RAM_ON
    printf("RefCalc RefFreq: %li, PLL0-D: %li, PLL0-VCO: %li.\n", ref_freq, pll0->d, pll0->outFreq);
#endif
}

//Modified from the Rosetta Code;
void rat_approx(double f, uint64_t md, uint64_t* num, uint64_t* denom) {
	/*  a: continued fraction coefficients. */ 
	uint64_t a, h[3] = { 0, 1, 0 }, k[3] = { 1, 0, 0 };
	uint64_t x, d, n = 1;
	uint8_t i, neg = 0;
 
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
            (pllparam->d > PLL1_DIV_MAX) 		||
	    (pllparam->d < 1) 				||
            (pllparam->d > 1 && (pllparam->d & 1) != 0) ||
            (pllparam->x2en > 1) 			||
            (pllparam->outFreq > PLL1_VCO_MAX_HZ) 	||
	    (pllparam->outFreq < PLL1_VCO_MIN_HZ)) {
                return 0;
        }

    } else { // is_pll0
        if ((pllparam->N > PLL0_N_MAX) 			||
	    (pllparam->N < PLL0_N_MIN) 			||
            (pllparam->R > _PLL_RATS_MAX_DENOM) 	||
            (pllparam->d > PLL0_DIV_MAX) 		||
            (pllparam->d < 1) 				||
            (pllparam->d > 1 && (pllparam->d & 1) != 0) ||
            (pllparam->x2en > 0) 			|| // No Doubler for PLL0
            (pllparam->outFreq > PLL0_VCO_MAX_HZ) 	||
            (pllparam->outFreq < PLL0_VCO_MIN_HZ)) {
                return 0;
        }
    }

    return 1;
}

// N must be more than 16
void pll_ConformDividers(uint64_t* N, uint64_t* R, uint8_t is_pll1) {
    // D is forced in pll_RefCalc to be even and less than 62, if D > 1
    // R is forced to not exceed its maximum due to the continued fraction algorithm in rat_approx
    // N however, may exceed the maximum or minimum bounds that have been set
    // R also has a possiblity of violating the MINIMUM bound, if there is one
    uint64_t Nt = *N;
    uint64_t Rt = *R;
    
    // If the N maximum bound is exceeded
    // Calculate the worst case N/R delta
    // Move within the +/- of the delta to check for good N values
    uint64_t MAX_N;
    uint64_t REF;
    if (is_pll1) {
        MAX_N = PLL1_N_MAX;
        REF = PLL1_REF_MAX_HZ;
    } else {
        MAX_N = PLL0_N_MAX;
        REF = PLL_CORE_REF_FREQ_HZ;
    }

    if (Nt > MAX_N) { //printf("N found to be more than N_MAX: %li.\n", Nt);
        double ratio = (double)Nt / (double)Rt;
        double worst_delta = (double)_PLL_OUT_MAX_DEVIATION / (double)REF;
        double del = worst_delta / 10.0;
		double min_N_ratio = ratio;
		uint64_t Nmin = Nt;
		uint64_t NminR = Rt;
        uint16_t MAX_R;
        for (MAX_R = _PLL_RATS_MAX_DENOM; MAX_R > 1; MAX_R = MAX_R / 2) {
			ratio = min_N_ratio;
            ratio += worst_delta;
            uint8_t range;
            for (range = 20 + 1; range > 0; range--) { // 20 + 1 to keep range positive
                //if (range == 10) continue;
                rat_approx(ratio, MAX_R, &Nt, &Rt);
				if (Nmin > Nt) {
					Nmin = Nt;
					min_N_ratio = ratio;
					NminR = Rt;
				}
#ifdef _PLL_DEBUG_RAM_ON
                printf("In Loop: Possible N: %li.\n", Nt);
#endif
                if (Nt < (uint64_t)MAX_N) break;
                else ratio -= del;
            }
            if (Nt < (uint64_t)MAX_N) break;

#ifdef _PLL_DEBUG_RAM_ON
            printf("Reducing Current-MAX_R: %i.\n", MAX_R);
#endif
            if (MAX_R < (50)) {
				Nt = Nmin;
				Rt = NminR;
				ratio = min_N_ratio;
                fprintf(stderr, "UNABLE TO FIND PROPER SOLUTION: N TOO LARGE: %"PRIu64", UNABLE TO REDUCE N.\n", Nmin);
                break;
            }
        }
    }

	*N = Nt;
	*R = Rt;

    // If the N and R minimum bounds are violated
    uint64_t MIN_N;
    uint64_t MIN_R;

    if (is_pll1) {
        MIN_N = PLL1_N_MIN;
        MIN_R = PLL1_R_MIN;
    } else {
        MIN_N = PLL1_N_MIN;
        MIN_R = PLL1_R_MIN;
    }

    uint64_t multiplier;
    for (multiplier = 2; (Nt < MIN_N) || (Rt < MIN_R); multiplier++) {
        Nt = *N * multiplier;
        Rt = *R * multiplier;
    }

#ifdef _PLL_DEBUG_RAM_ON
	printf("CD: Best N found: Nt: %li.\n", Nt);
	printf("CD: Best R found: Rt: %li.\n", Rt);
#endif

    *N = Nt;
    *R = Rt;
}

void pll_SetVCO(uint64_t* reqFreq, pllparam_t* pll, uint8_t is_pll1) {
	if (is_pll1) {
    		// Configure VCO frequency and DIV values for PLL1
    		if (*reqFreq > PLL1_VCO_MAX_HZ) {
    		    pll->x2en = 1;
    		    pll->d = 1;
    		    pll->outFreq = *reqFreq / 2;
    		} else if (*reqFreq > PLL1_VCO_MIN_HZ) {
    		    pll->x2en = 0;
    		    pll->d = 1;
    		    pll->outFreq = *reqFreq;
    		} else {
    		    uint16_t D = PLL1_VCO_MAX_HZ / *reqFreq;
    		    // D += 1; // round up, safer
    		    if ((D & 1) != 0) D -= 1;
    		    if (D > PLL1_DIV_MAX) D = PLL1_DIV_MAX;
		    if (D < PLL1_DIV_MIN) D = PLL1_DIV_MIN;
    		    pll->outFreq = (uint64_t)D * *reqFreq;
    		    pll->d = D;
    		    pll->x2en = 0;
    		}
	} else { // (is_pll0)
    		// Configure VCO frequency and DIV values for PLL0
    		if (*reqFreq > PLL0_VCO_MAX_HZ) {
    		    pll->x2en = 1;
    		    pll->d = 1;
    		    pll->outFreq = *reqFreq / 2;
    		} else if (*reqFreq > PLL0_VCO_MIN_HZ) {
    		    pll->x2en = 0;
    		    pll->d = 1;
    		    pll->outFreq = *reqFreq;
    		} else {
    		    uint16_t D = PLL0_VCO_MAX_HZ / *reqFreq;
    		    // D += 1; // round up, safer
    		    if ((D & 1) != 0) D -= 1;
    		    if (D > PLL0_DIV_MAX) D = PLL0_DIV_MAX;
		    if (D < PLL0_DIV_MIN) D = PLL0_DIV_MIN;
    		    pll->outFreq = (uint64_t)D * *reqFreq;
    		    pll->d = D;
    		    pll->x2en = 0;
    		}
	}
}

uint8_t pll_NCalc(uint64_t* reqFreq, pllparam_t* pll1, uint64_t* N) { //printf("In NCalc...\n");
	uint8_t rounding_performed = 0;

	// Determine the upper and lower bounds of N for PLL1
	uint64_t Nmax = floor((double)pll1->outFreq / (double)PLL1_PD_MIN_HZ);
	uint64_t Nmin;
	if (PLL1_PD_MAX_HZ < PLL1_REF_MAX_HZ) {
		Nmin = ceil((double)pll1->outFreq / (double)PLL1_PD_MAX_HZ);
	} else {
		Nmin = ceil((double)pll1->outFreq / (double)PLL1_REF_MAX_HZ);
	}

	// Truncate N if out-of-range
	if (Nmax > PLL1_N_MAX) Nmax = PLL1_N_MAX;
	else if (Nmin < PLL1_N_MIN) Nmin = PLL1_N_MIN;
	
	// Floor the required frequency to the nearest 100
	uint16_t rem = *reqFreq % 100;
	if (rem != 0) {
		*reqFreq = *reqFreq - rem;
		rounding_performed = 1;
		//printf("Rounding Performed...\n");
	}

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
#ifdef _PLL_DEBUG_RAM_ON
		printf("NCalc: Current-N: %li, Current-Score: %lf.\n", Ntemp, score);
		printf("NCalc: Best-N: %li, Best-Score: %lf.\n", Nbest, best_score);
#endif
	}
	*N = Nbest;

	return rounding_performed;
}

void pll_NScoringFunction(pllparam_t* pll, uint64_t* N, double* score) {
	double div_value = (double)pll->outFreq / (double) *N;

	uint64_t remainder = 0;
	uint64_t divisor = 1;

	while (remainder == 0) {
		remainder = fmodf(div_value,divisor);
		divisor = divisor * 10;
#ifdef _PLL_DEBUG_RAM_ON
		printf("NSF: Remainder: %li, Divisor: %li.\n", remainder, divisor);
#endif
	}

	*score = (double)log10(divisor) / (double) *N;
}
