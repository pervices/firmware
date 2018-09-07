SUBDIRS = \
	common \
	hal \
	parser

bin_PROGRAMS = \
	server \
	mcu \
	mem

server_SOURCES = \
	server.c \
	led.c \
	synth_lut.c

server_LDADD = \
	common/libcommon.la \
	hal/libhal.la \
	hal/drivers/mmap/libmmap.la \
	hal/drivers/udp/libudp.la \
	hal/drivers/uart/libuart.la \
	parser/libparser.la \
	-lpthread

mcu_SOURCES = \
	mcu.c
mcu_LDADD = \
	common/libcommon.la \
	hal/libhal.la \
	hal/drivers/mmap/libmmap.la \
	hal/drivers/uart/libuart.la \
	hal/drivers/udp/libudp.la

mem_SOURCES = \
	mem.c
mem_LDADD = \
	common/libcommon.la \
	hal/libhal.la \
	hal/drivers/mmap/libmmap.la
