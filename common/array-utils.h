#ifndef COMMON_ARRAY_UTILS_H_
#define COMMON_ARRAY_UTILS_H_

/* ARRAY_SIZE([])
 * This handy macro will return the number of elements in an array.
 * The build will break if ARRAY_SIZE is used on anything but an array. */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)                                                        \
    (sizeof(arr) / sizeof((arr)[0]) +                                          \
     sizeof(typeof(int[1 - 2 * !!__builtin_types_compatible_p(                 \
                                   typeof(arr), typeof(&arr[0]))])) *          \
         0)
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
