//
// Copyright 2026 Per Vices Corporation
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

#pragma once

#include <sys/types.h>

/**
 * Copies the directory of the file/directory refered to by path to dir.
 * This exists because libc's dirname is unreasonably complicated to use. This funtion will ignore trailing /. It will return an error there is no parent directory (i.e /example/ and example will return an error, /demo/example/ and demo/example/ will pass).
 * @param dir The buffer to write the directory to. It must be at least the length of path (including the null terminator)
 * @param path A null terminated string containing the path of the file whose directory you want
 * @return Returns 0 on success. At present it returns INT_MIN. It may return -error_code in the future.
 */
int simple_dirname(char* dir, const char* path);

/**
 * C equivalent of mkdir -p
 *
 * @param path The path of the directory to create
 *
 * @return Error code of the first failed sys call.
 */
int mkdir_p(const char* path);

/**
 * Functions the same as creat and mkdir -p for the directory containing the file
 * @param path The path of the file to create. Much be less than MAX_PATH_LEN
 * @param mode The permissions for the file to be created
 */
int creat_with_dir(const char* path, mode_t mode);
