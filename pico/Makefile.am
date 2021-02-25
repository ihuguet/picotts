## Makefile.am -- Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libttspico.la
libttspico_la_SOURCES = \
	lib/picoacph.c \
	lib/picoapi.c \
	lib/picobase.c \
	lib/picocep.c \
	lib/picoctrl.c \
	lib/picodata.c \
	lib/picodbg.c \
	lib/picoextapi.c \
	lib/picofftsg.c \
	lib/picokdbg.c \
	lib/picokdt.c \
	lib/picokfst.c \
	lib/picoklex.c \
	lib/picoknow.c \
	lib/picokpdf.c \
	lib/picokpr.c \
	lib/picoktab.c \
	lib/picoos.c \
	lib/picopal.c \
	lib/picopam.c \
	lib/picopr.c \
	lib/picorsrc.c \
	lib/picosa.c \
	lib/picosig.c \
	lib/picosig2.c \
	lib/picospho.c \
	lib/picotok.c \
	lib/picotrns.c \
	lib/picowa.c

libttspico_ladir = $(includedir)
libttspico_la_HEADERS = \
    lib/picoacph.h \
    lib/picoapid.h \
    lib/picoapi.h \
    lib/picobase.h \
    lib/picocep.h \
    lib/picoctrl.h \
    lib/picodata.h \
    lib/picodbg.h \
    lib/picodefs.h \
    lib/picodsp.h \
    lib/picoextapi.h \
    lib/picofftsg.h \
    lib/picokdbg.h \
    lib/picokdt.h \
    lib/picokfst.h \
    lib/picoklex.h \
    lib/picoknow.h \
    lib/picokpdf.h \
    lib/picokpr.h \
    lib/picoktab.h \
    lib/picoos.h \
    lib/picopal.h \
    lib/picopam.h \
    lib/picopltf.h \
    lib/picopr.h \
    lib/picorsrc.h \
    lib/picosa.h \
    lib/picosig2.h \
    lib/picosig.h \
    lib/picospho.h \
    lib/picotok.h \
    lib/picotrns.h \
    lib/picowa.h

libttspico_la_LIBADD = -lm

picolangdir = $(datadir)/pico/lang
picolang_DATA = \
    lang/de-DE_gl0_sg.bin \
    lang/de-DE_ta.bin \
    lang/en-GB_kh0_sg.bin \
    lang/en-GB_ta.bin \
    lang/en-US_lh0_sg.bin \
    lang/en-US_ta.bin \
    lang/es-ES_ta.bin \
    lang/es-ES_zl0_sg.bin \
    lang/fr-FR_nk0_sg.bin \
    lang/fr-FR_ta.bin \
    lang/it-IT_cm0_sg.bin \
    lang/it-IT_ta.bin

bin_PROGRAMS = pico2wave
pico2wave_SOURCES = \
	bin/pico2wave.c
pico2wave_LDADD = \
	libttspico.la -lpopt
pico2wave_CFLAGS = -Wall -Dpicolangdir=\"$(picolangdir)\" -I lib

