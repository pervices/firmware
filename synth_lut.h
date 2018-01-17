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

#endif /* SYNTH_LUT_H_ */
