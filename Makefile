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
CC ?= arm-unknown-linux-gnueabihf-gcc

# Cross compile flags
CFLAGS = -c -O0 -g3 -Wall -fmessage-length=0

# Linker flags
LDFLAGS = -lm -lrt

# Root Directory
export CRIMSON_ROOTDIR = $(shell pwd)

# Out Directory
OUTDIR = $(CRIMSON_ROOTDIR)/out

# Makefile uses sh as shell
SHELL = /bin/sh

# Object files are source files with .c replaced with .o
OBJECTS = $(addprefix $(OUTDIR)/obj/main/,$(SOURCES:.c=.o))

# Output Executable
EXECS = $(addprefix $(OUTDIR)/bin/,$(SOURCES:.c=))

# Source Files
SOURCES = server.c mem.c mcu.c
TARGETS = $(SOURCES:.c=)

# Includes
INCLUDES += -I$(OUTDIR)/inc

# Sub-folders which contains Makefiles
# Only include subfolders one level under and in the order of dependencies
# Build order is: left --> right
SUBDIRS += common hal parser

# Specify Current branch
VERSION_GIT_BRANCH := $(shell git rev-parse --abbrev-ref HEAD)

# Specify Git revision
VERSION_GIT_REVISION := $(shell git describe --abbrev=8 --dirty --always --long)

# Specify build date and time
VERSION_DATE := $(shell TZ=UTC date "+%F-%T")


# Append to internal CFLAGS;
iCFLAGS = $(CFLAGS)
iCFLAGS += -DVERSIONGITBRANCH=\"$(VERSION_GIT_BRANCH)\"
iCFLAGS += -DVERSIONGITREVISION=\"$(VERSION_GIT_REVISION)\"
iCFLAGS += -DVERSIONDATE=\"$(VERSION_DATE)\"

all: $(EXECS)


install: $(EXECS)
	$(foreach EXEC, $(EXECS), install -m 0755 $(EXEC) /usr/bin;)

# Links all the object files together for output
define AUTO_TARGET
$(addprefix $(OUTDIR)/bin/,$(1)): $(OBJECTS)
	$(CC) $(LDFLAGS) $$(wildcard $(OUTDIR)/obj/*.o) $(addprefix $(OUTDIR)/obj/main/,$(1)).o -o $(addprefix $(OUTDIR)/bin/,$(1))
endef
$(foreach TARGET, $(TARGETS), $(eval $(call AUTO_TARGET, $(TARGET)) ))

# Generates all of the object files from the source files
$(OUTDIR)/obj/main/%.o: %.c | MAKE_SUBDIR
	$(CC) $(iCFLAGS) $(INCLUDES) $< -o $@
	@cp -f $< $(OUTDIR)/src

# Recursive build of all the sub_directories
MAKE_SUBDIR: MAKE_OUTDIR
	$(foreach SUBDIR, $(SUBDIRS), $(MAKE) --no-print-directory -C $(SUBDIR) -f Makefile CC="$(CC)";)

# Generates the output directory
MAKE_OUTDIR:
	@mkdir -p $(OUTDIR)/obj
	@mkdir -p $(OUTDIR)/obj/main
	@mkdir -p $(OUTDIR)/inc
	@mkdir -p $(OUTDIR)/src
	@mkdir -p $(OUTDIR)/bin
#find . -name '*\.h' | grep -v ./out | xargs -i cp -f {} $(OUTDIR)/inc/
#find . -name '*\.c' | grep -v ./out | xargs -i cp -f {} $(OUTDIR)/src/

# Clean the output directory
clean:
	rm -rf $(OUTDIR)

# Print the value of any MAKEFILE variable
# Useful for debugging makefiles.
print-%: ; @echo $*=$($*)
