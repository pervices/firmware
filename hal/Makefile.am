SUBDIRS = \
	drivers

noinst_LTLIBRARIES = \
	libhal.la

libhal_la_SOURCES = \
	pllcalc.c \
	comm_manager.c \
	properties.c \
	property_manager.c \
	../synth_lut.c
libhal_la_CFLAGS = \
	-DVERSION=VERSIONDATE
libhal_la_LIBADD = \
	../common/libcommon.la
