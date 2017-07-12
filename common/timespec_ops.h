#ifndef COMMON_TIMESPEC_OPS_H_
#define COMMON_TIMESPEC_OPS_H_

#include <math.h>

#include <time.h> // struct timespec

// The functions here are meant to be variants of those in sys/time.h
// but using struct timespec instead of struct timeval.
// The naming convention is such that "timer" is replaced with "times".

// XXX: @CF: 20170712: consider rewriting this using only integer math when free time arises

#define _ns_per_s 1000000000
#define _todouble( a ) ( (double) (a)->tv_sec + ( (double) (a)->tv_nsec ) / _ns_per_s )
#define _fromdouble( a, b ) \
	do { \
		double _fractional; \
		double _integral; \
		_fractional = modf( a, & _integral ); \
		(b)->tv_sec = _integral; \
		(b)->tv_nsec = _fractional * _ns_per_s; \
	} while( 0 )

static inline void timesadd( struct timespec *a, struct timespec *b, struct timespec *res ) {
	double sum;
	sum = _todouble( a ) + _todouble( b );
	_fromdouble( sum, res );
}

static inline void timessub( struct timespec *a, struct timespec *b, struct timespec *res ) {
	double diff = _todouble( a ) - _todouble( b );
	_fromdouble( diff, res );
}

static inline void timesclear( struct timespec *tsp ) {
	tsp->tv_sec = 0;
	tsp->tv_nsec = 0;
}

static inline int timesisset( struct timespec *tsp ) {
	return 0 == tsp->tv_sec && 0 == tsp->tv_nsec;
}

#define timescmp( a, b, CMP ) _todouble( a ) CMP _todouble( b )

#endif /* COMMON_TIMESPEC_OPS_H_ */
