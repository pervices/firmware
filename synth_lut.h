#ifndef SYNTH_LUT_H_
#define SYNTH_LUT_H_

#include <stdbool.h>
#include <stdint.h>

struct synth_rec __attribute__(( packed ));

typedef struct synth_rec {
	uint8_t core;
	uint8_t band;
	uint8_t bias;
} synth_rec_t;

/**
 * Initialize the synth calibration table for each board on the system. This
 * implementation performs auto-calibration if the calibration for a specific
 * table does not exist.
 *
 * @return 0 on success or an errno value on error
 */
int synth_lut_init();

/**
 * Discard any resources required for synth calibration tables.
 */
void synth_lut_fini();

/**
 * Get the step size of the local oscillator
 * @return the step size in Hz
 */
double synth_lut_get_lo_step_size();

/**
 * Get highest frequency that the board is capable of.
 * @return the highest frequency in Hz
 */
double synth_lut_get_freq_top();

/**
 * Get the optimized synth settings for a specific frequency.
 *
 * @param tx      true if the setting is for tx
 * @param channel the channel (number, i.e. 0, rather than 'A')
 * @param freq    the LO frequency to tune for
 * @param rec     pointer to store output synth values
 * @return        0 on success.
 *                EINVAL if rec is NULL.
 *                EINVAL if channel is out of range.
 *                EINVAL if freq is not an exact positive multiple of the LO step size.
 */
int synth_lut_get_rec( const bool tx, const uint8_t channel, const double freq, synth_rec_t *rec );

#endif /* SYNTH_LUT_H_ */
