//
// Copyright 2014 - 2019 Per Vices Corporation
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

#ifndef COMMON_ARRAY_UTILS_H_
#define COMMON_ARRAY_UTILS_H_

/* ARRAY_SIZE([])
 * This handy macro will return the number of elements in an array.
 * The build will break if ARRAY_SIZE is used on anything but an array. */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
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
