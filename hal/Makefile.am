SUBDIRS = \
	drivers

noinst_LTLIBRARIES = \
	libhal.la

libhal_la_SOURCES = \
	comm_manager.c \
	pllcalc.c \
	properties_tate_nrnt.c \
	properties_vaunt.c \
	property_manager.c
libhal_la_CFLAGS = \
	-DVERSION=VERSIONDATE
libhal_la_LIBADD = \
	../common/libcommon.la
