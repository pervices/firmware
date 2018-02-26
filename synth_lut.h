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
 * Enable usage of synthesizer calibration tables when setting frequency.
 *
 * If the calibration tables do not exist, they will be created.
 * Auto-calibration generates tables for the entire frequency range
 * and can take some time as a result.
 *
 * @param channel The channel to enable calibration on
 * @return        0 on success or an errno value on error
 */
int synth_lut_enable( const bool tx, const size_t channel );
/**
 * Enable usage of synthesizer calibration tables when setting frequency
 * for all channels.
 *
 * @return 0 on success or an errno value on error
 */
int synth_lut_enable_all();

/**
 * Enable usage of synthesizer calibration tables when setting frequency.
 */
void synth_lut_disable( const bool tx, const size_t channel );

/**
 * Disable usage of synthesizer calibration tables when setting frequency
 * for all channels.
 */
void synth_lut_disable_all();

/**
 * Enable usage of synthesizer calibration tables when setting frequency
 * for all channels.
 *
 * @return        0 on success, or an errno value on error.
 */
int synth_lut_enable_all_if_calibrated();

/**
 * Check whether the synthesizer calibration tables are enabled.
 *
 * @return true when enabled, false otherwise.
 */
bool synth_lut_is_enabled( const bool tx, const size_t channel );

/**
 * Clear synth calibration tables for one channel.
 * This will call synth_lut_disable() internally.
 */
void synth_lut_erase( const bool tx, const size_t channel );

/**
 * Clear all synth calibration tables. This will call synth_lut_disable()
 * internally.
 */
void synth_lut_erase_all();

/**
 * Check whether calibration data exists for a specific channel.
 *
 * @param tx      true if the setting is for tx
 * @param channel the channel (number, i.e. 0, rather than 'A')
 * @return        true if calibration data exists, otherwise false.
 */
bool synth_lut_is_calibrated( const bool tx, const size_t channel );

/**
 * Get the calibrated synthesizer settings for a specific frequency on a specific channel.
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
int synth_lut_get( const bool tx, const uint8_t channel, const double freq, synth_rec_t *rec );

#endif /* SYNTH_LUT_H_ */
