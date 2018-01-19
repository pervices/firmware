#ifndef COMMON_ARRAY_UTILS_H_
#define COMMON_ARRAY_UTILS_H_

#ifndef ARRAY_SIZE
#define ARRAY_SIZE( x ) ( sizeof( x ) / sizeof( (x)[ 0 ] ) )
#endif

#ifndef ARRAY_OFFSET
#define ARRAY_OFFSET( it, arr ) ( ( it - arr ) / sizeof( *it ) )
#endif

#endif /* COMMON_ARRAY_UTILS_H_ */
