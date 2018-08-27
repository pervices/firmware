#ifndef COMMON_ARRAY_UTILS_H_
#define COMMON_ARRAY_UTILS_H_

#if defined(__GNUC__) && defined(__GNUC_MINOR__)
 #define GNUC_VERSION \
     (__GNUC__ << 16) + __GNUC_MINOR__
 #define GNUC_PREREQ(maj, min) \
     (GNUC_VERSION >= ((maj) << 16) + (min))
#else
 #define GNUC_PREREQ(maj, min) 0
#endif
#define BUILD_BUG_ON_ZERO(e) \
    (sizeof(struct { int:-!!(e)*1234; }))
#if GNUC_PREREQ(3, 1)
 #define SAME_TYPE(a, b) \
     __builtin_types_compatible_p(typeof(a), typeof(b))
 #define MUST_BE_ARRAY(a) \
     BUILD_BUG_ON_ZERO(SAME_TYPE((a), &(*a)))
#else
 #define MUST_BE_ARRAY(a) \
     BUILD_BUG_ON_ZERO(sizeof(a) % sizeof(*a))
#endif
#ifdef __cplusplus
 template <typename T, size_t N>
 char ( &ARRAY_SIZE_HELPER( T (&array)[N] ))[N];
 #define ARRAY_SIZE( array ) \
      (sizeof( ARRAY_SIZE_HELPER( array ) ))
#else
 #define ARRAY_SIZE(a) ( \
     (sizeof(a) / sizeof(*a)) \
     + MUST_BE_ARRAY(a))
#endif

#ifndef ARRAY_OFFSET
#define ARRAY_OFFSET(it, arr) (((char *)(it) - (char *)(arr)) / sizeof(*(it)))
#endif

#ifndef FOR_EACH
#define FOR_EACH(it, arr)                                                      \
    for (it = (arr); ARRAY_OFFSET(it, arr) < ARRAY_SIZE(arr);                  \
         it = ARRAY_OFFSET(it, arr) < ARRAY_SIZE(arr) ? (it) + 1 : NULL)
#endif

#endif /* COMMON_ARRAY_UTILS_H_ */
