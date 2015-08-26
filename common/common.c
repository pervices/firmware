//
// Copyright 2014 Per Vices Corporation
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

#include "common.h"

static FILE* fout = NULL;

int PRINT( print_t priority, const char* format, ... ) {
	int ret = 0;
	va_list args;
	va_start(args, format);

	// open the file
	if (!fout) {
		fout = fopen("/home/root/crimson.log", "a");
	}

	// get the time
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	// pre-pend the print message with time and info
	char newfmt[BUF_SIZE] = {0};
	switch (priority) {
		case ERROR:
			snprintf(newfmt, BUF_SIZE, "[%5ld.%03ld] ERROR: ", (long)ts.tv_sec, ts.tv_nsec / 1000000UL);
			break;
		case INFO:
			snprintf(newfmt, BUF_SIZE, "[%5ld.%03ld] INFO:  ", (long)ts.tv_sec, ts.tv_nsec / 1000000UL);
			break;
		case DEBUG:
			snprintf(newfmt, BUF_SIZE, "[%5ld.%03ld] DEBUG: ", (long)ts.tv_sec, ts.tv_nsec / 1000000UL);
			break;
		case VERBOSE:
			snprintf(newfmt, BUF_SIZE, "[%5ld.%03ld] VERB:  ", (long)ts.tv_sec, ts.tv_nsec / 1000000UL);
			break;
		default:
			snprintf(newfmt, BUF_SIZE, "[%5ld.%03ld] DFLT:  ", (long)ts.tv_sec, ts.tv_nsec / 1000000UL);
			break;
	}
	strcpy(newfmt + strlen(newfmt), format);

	// DEBUG or INFO or ERROR, file
	if (fout && priority <= DEBUG) {
		ret = vfprintf(fout, newfmt, args );
	}

	// INFO, stdout
	if (priority == INFO) {
		ret = vprintf(newfmt, args);
	}

	// ERROR, stderr
	if (priority == ERROR ) {
		ret = vfprintf(stderr, newfmt, args );
	}

	if (priority == VERBOSE) {
		// do nothing as of now
	}

	// flush the file
	fflush(fout);

	va_end(args);
	return ret;
}
