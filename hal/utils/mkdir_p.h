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
 *
 */
int creat_with_dir(const char* path, mode_t mode);
