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

// Header
#include "mkdir_p.h"

// Internal includes
#include "common/common.h"

// External includes
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

// TMP for exit
#include <stdlib.h>

int mkdir_p(const char* path) {

    // Read: list directory contents
    // Write: create new files
    // Execute: enter the directory
    // TODO: update the permissions to be only
    int mkdir_r = mkdir(path, 0777);

    if(mkdir_r < 0) {
        int mkdir_error = errno;
        PRINT(ERROR, "mkdir_error: %s\n", strerror(mkdir_error));
        PRINT(ERROR, "path: %s\n", path);
        exit(mkdir_error);
    }

    return 0;


//     while(path[0] ~= NULL) {
//
//     }
}

int creat_with_dir(const char* path, mode_t mode) {

    int file = creat(path, mode);

    if(file < 0) {
        int e = errno;
        PRINT(ERROR, "path: %s\n", path);
        PRINT(ERROR, "error: %s\n", strerrno(e));
    } else {
        close(file);
    }
}
































