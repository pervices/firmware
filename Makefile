#
# Copyright 2014 Per Vices Corporation
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http:#www.gnu.org/licenses/>.
#

# Cross compile toolchain
#CC = $(CRIMSON_ROOTDIR)/build/gcc/bin/arm-linux-gnueabihf-gcc
CC = arm-linux-gnueabihf-gcc

# Cross compile flags
CFLAGS = -c -O0 -g3 -Wall -fmessage-length=0

# Linker flags
LDFLAGS = -lm

# Out Directory
OUTDIR = $(CRIMSON_ROOTDIR)/out

# Makefile uses sh as shell
SHELL = /bin/sh

# Object files are source files with .c replaced with .o
OBJECTS = $(addprefix $(OUTDIR)/obj/,$(SOURCES:.c=.o))

# Root Directory
export CRIMSON_ROOTDIR = $(shell pwd)

# Output Executable
EXECUTABLE = $(addprefix $(OUTDIR)/bin/,crimson-server)

# Source Files
SOURCES = server.c

# Includes
INCLUDES += -I$(OUTDIR)/inc

# Sub-folders which contains Makefiles
# Only include subfolders one level under and in the order of dependencies
# Build order is: left --> right
SUBDIRS += common hal parser

all: MAKE_OUTDIR MAKE_SUBDIR $(SOURCES) $(EXECUTABLE)

# Links all the object files together for output
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(wildcard $(OUTDIR)/obj/*) -o $@

# Generates all of the object files from the source files
$(OUTDIR)/obj/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@
	@cp $< $(OUTDIR)/src

# Recursive build of all the sub_directories
MAKE_SUBDIR:
	$(foreach SUBDIR, $(SUBDIRS), $(MAKE) --no-print-directory -C $(SUBDIR) -f Makefile;)

# Generates the output directory
MAKE_OUTDIR:
	@mkdir -p $(OUTDIR)/obj
	@mkdir -p $(OUTDIR)/inc
	@mkdir -p $(OUTDIR)/src
	@mkdir -p $(OUTDIR)/bin
#find . -name '*\.h' | grep -v ./out | xargs -i cp -f {} $(OUTDIR)/inc/
#find . -name '*\.c' | grep -v ./out | xargs -i cp -f {} $(OUTDIR)/src/

# Clean the output directory
clean:
	rm -rf $(OUTDIR)
