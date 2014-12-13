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

ifdef CRIMSON_ROOTDIR

SUBDIRS += udp mmap uart

all: MAKE_SUBDIR

MAKE_SUBDIR:
	$(foreach SUBDIR, $(SUBDIRS), $(MAKE) --no-print-directory -C $(SUBDIR) -f Makefile;)

else
all:
	@echo ERROR: CRIMSON_ROOTDIR not specified, please build from top of tree.

endif
