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
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <libgen.h>

// TMP for exit
#include <stdlib.h>

int simple_dirname(char* dir, const char* path) {
    // Start at the second last char in path so that we skip trailing '/'
    size_t i = strlen(path) - 2;

    // Find the last / in the path. We can skip the case where i == 0 because having the only / in the path be at the start is an error since there there is no parent
    while(i > 0) {
        if(path[i] == '/') {
            break;
        }
        i--;
    }

    // No parent directory
    if(i == 0) {
        // TODO: find a good error code
        return ~0;
    }

    // Copy the parent directory
    for(size_t c = 0; c < i; c++) {
        dir[c] = path[c];
    }
    // Add the null terminator
    dir[i] = 0;

    return RETURN_SUCCESS;
}

int mkdir_p(const char* path) {

    // Read: list directory contents
    // Write: create new files
    // Execute: enter the directory
    // TODO: update the permissions to be only
    int mkdir_r = mkdir(path, 0777);

    // Success, no further action required
    if(mkdir_r == 0) {
        return RETURN_SUCCESS;
    }

    int mkdir_error = errno;

    // The parent directory doesn't exist yet
    if(mkdir_error == ENOENT) {
        char dir[MAX_PATH_LEN];
        int dirname_r = simple_dirname(dir, path);

        if(dirname_r < 0) {
            PRINT(ERROR, "Unable to extract directory name from %s\n", path);
        }

        // Recursively attempt to create parent directory
        int mk_parent_r = mkdir_p(dir);

        // Propogate error if the attempt to make the parent failed
        if(mk_parent_r < 0) {
            return mk_parent_r;
        }

        // Try making the directory again now that the parent exists
        int mkdir_r2 = mkdir(path, 0777);

        if(mkdir_r2 < 0) {
            int e = errno;
            PRINT(ERROR, "Unable to directory %s due to: %s\n", path, e);
            return -e;
        }

        return RETURN_SUCCESS;
    } else {
        PRINT(ERROR, "mkdir failed with error code: %s\n", strerror(mkdir_error));

        return -mkdir_error;
    }
}

int touch_p(const char* path, mode_t mode) {

    // Create file
    int file_a = open(path, /* Create file if it doesn't already exist */O_CREAT, mode);

    // The file was successfully created
    if(file_a >= 0) {
        close(file_a);
        return RETURN_SUCCESS;
    }

    // The directory for the file doesn't exist
    if(errno == ENOENT) {
        char dir[MAX_PATH_LEN];

        int dirname_r = simple_dirname(dir, path);

        if(dirname_r < 0) {
            PRINT(ERROR, "Unable to extract directory name from %s\n", path);
            return dirname_r;
        }

        int mkdir_p_r = mkdir_p(dir);

        // Unable to create the directory
        if(mkdir_p_r < 0) {
            PRINT(ERROR, "Unable to create directory %s for file %s\n", dir, path);
            return mkdir_p_r;
        }

        // Attempt to create the file
        int file_b = open(path, O_CREAT, mode);

        if(file_b < 0) {
            int e = errno;
            PRINT(ERROR, "Unable to creat file with path %s after successfully creating its folder %s\n", path, dir);
            return e;
        }

        close(file_b);

        return RETURN_SUCCESS;
    }
    // Unexpected failure
    else {
        int e = errno;
        PRINT(ERROR, "Unable to creat file: %s\n", strerror(errno));

        return -e;
    }
}
































