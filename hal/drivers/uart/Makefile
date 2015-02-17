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

# Check to see if the root directory is specified
ifdef CRIMSON_ROOTDIR

#CC = $(CRIMSON_ROOTDIR)/build/gcc/bin/arm-linux-gnueabihf-gcc
CC = arm-linux-gnueabihf-gcc
CFLAGS = -c -O0 -g3 -Wall -c -fmessage-length=0
LDFLAGS = -lm
OUTDIR = $(CRIMSON_ROOTDIR)/out
SHELL = /bin/sh
INCLUDES += -I$(OUTDIR)/inc

# Source Files, only change this part of the file!
SOURCES = uart.c
SUBDIRS +=

OBJECTS = $(addprefix $(OUTDIR)/obj/,$(SOURCES:.c=.o))

all: MAKE_OUTDIR MAKE_SUBDIR $(SOURCES) $(OBJECTS)

# Generates all of the object files from the source files
$(OUTDIR)/obj/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@
	@cp -f $< $(OUTDIR)/src

# Recursive build of all the sub_directories
MAKE_SUBDIR:
	@$(foreach SUBDIR, $(SUBDIRS), $(MAKE) --no-print-directory -C $(SUBDIR) -f Makefile;)

# Copies header files over
MAKE_OUTDIR:
	@cp -f *.h $(OUTDIR)/inc

# If not building from root
else
all:
	@echo ERROR: CRIMSON_ROOTDIR not specified, please build from top of tree.

endif
