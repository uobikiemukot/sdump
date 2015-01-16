CC ?= gcc
#CC ?= clang

CFLAGS  ?= -Wall -Wextra -std=c99 -pedantic \
-O3 -pipe -s
#-Og -g -rdynamic #-pg
LDFLAGS ?= -ljpeg -lpng -lsixel

HDR = ../libnsgif.h ../libnsbmp.h \
	../util.h ../loader.h ../image.h \
	../sixel.h parsearg.h
SRC = yaimg-sixel.c ../libnsgif.c ../libnsbmp.c
DST = yaimg-sixel

all:  $(DST)

yaimg-sixel: $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $@

clean:
	rm -f $(DST)
