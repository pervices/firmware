#ifndef COMMON_ARRAY_UTILS_H_
#define COMMON_ARRAY_UTILS_H_

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
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
