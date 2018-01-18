#ifndef COMMON_ARRAY_UTILS_H_
#define COMMON_ARRAY_UTILS_H_

#ifndef ARRAY_SIZE
#define ARRAY_SIZE( x ) ( sizeof( x ) / sizeof( (x)[ 0 ] ) )
#endif

#ifndef ARRAY_OFFSET
#define ARRAY_OFFSET( it, arr ) ( ( it - arr ) / sizeof( *it ) )
#endif

#ifndef FOR_EACH
#define FOR_EACH( it, arr ) \
	for( (it) = ((typeof(it))arr); (it) < & (arr)[ ARRAY_SIZE( arr ) ]; (it) = (it) >= & (arr)[ ARRAY_SIZE( arr ) ] ? NULL : it + 1 )
#endif

#endif /* COMMON_ARRAY_UTILS_H_ */
