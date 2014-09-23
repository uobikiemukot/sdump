CC ?= gcc
#CC ?= clang

CFLAGS  ?= -Wall -Wextra -std=c99 -pedantic \
-O3 -pipe -s
#-Og -g -rdynamic #-pg
LDFLAGS = -ljpeg -lpng -lsixel

HDR = w3mimg_sixel.h ../libnsgif.h ../libnsbmp.h \
	../util.h ../loader.h ../image.h \
	parsearg.h sixel.h
SRC = w3mimg_sixel.c ../libnsgif.c ../libnsbmp.c
DST = w3mimg_sixel

all:  $(DST)

w3mimg_sixel: $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $@

clean:
	rm -f $(DST)
