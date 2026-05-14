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

/**
 * Creates the parent directory of the provided path
 *
 * @param path The path of the file/folder whose directory we want to create. Must be less than MAX_PATH_LEN
 * @param mode The permissions for the directory
 *
 * @return Returns 0 on success, -errno on failure
 */
int create_parent_dir(const char* path, mode_t mode) {
    char parent_path[PATH_MAX];

    parent_pa
}

int creat_with_dir(const char* path, mode_t mode) {

    // TODO: make sure mode is bing applied to the file, and not this operation

    // Create file
    int prop_a = creat(path, mode);

    // The file was successfully created
    if(prop_a >= 0) {
        close(prop_a);
        return RETURN_SUCCESS;
    }
    // The file already exists
    else if(errno == EEXIST) {
        return RETURN_SUCCESS;
    }
    // Unexpected failure
    else if(errno != MISSING DIRECTORY) {
        int e = errno;
        PRINT(ERROR, "1path: %s\n", path);
        PRINT(ERROR, "1 error: %s\n", strerror(e));
        return e;
    }

    do {
        // Get parent directory
        // Create parent directory

    } while (errno != MISSING DIRECTORY && errno != SUCCESS);

    // Directory for file was created
    if(errno == SUCCESS) {
        int prop_b = creat(path, mode);

        if(prop_b <0) {
            // ERROR
            return errno;
        } else {
            close(prop_b);
            return RETURN_SUCCESS;
        }
    }
    // Failed to create directory for the file
    else {
        return FAILURE;
    }
}
































