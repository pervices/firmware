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

int verbose;

void PRINT_WRAPPER( print_t priority, const char* format, ... ) {

	FILE* o;

	va_list args;
	va_start( args, format );

	o = NULL;

	switch( priority ){
		case ERROR:
			o = stderr;
			break;

		case VERBOSE:
			if ( verbose >= 1 ){
				o = stdout;
			}
			break;

		case DEBUG:
			if ( verbose >= 2 ){
				o = stdout;
			}
			break;

		case INFO:
		case DUMP:
		default:
			o = stdout;
			break;
	}

	if ( NULL != o ) {
		vfprintf( o, format, args );
	}

	va_end( args );
}
