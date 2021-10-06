SUBDIRS = \
	drivers

noinst_LTLIBRARIES = \
	libhal.la

libhal_la_SOURCES = \
	comm_manager.c \
	pllcalc.c \
	properties_tate.c \
	properties_tate_8r.c \
	properties_tate_4r4t.c \
	properties_tate_4r4t_3g.c \
	properties_tate_1bbrx_3g.c \
	properties_vaunt.c \
	property_manager.c
libhal_la_CFLAGS = \
	-DVERSION=VERSIONDATE
libhal_la_LIBADD = \
	../common/libcommon.la
