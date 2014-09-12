CC      = gcc

CFLAGS  = -Wall -Wextra -std=c99 -pedantic \
-march=native -Ofast -flto -pipe -s
#-Og -g -rdynamic #-pg
LDFLAGS = -ljpeg -lpng -lsixel

HDR = stb_image.h libnsgif.h libnsbmp.h \
	sdump.h util.h loader.h image.h 
SRC = sdump.c libnsgif.c libnsbmp.c

DST = sdump

all:  $(DST)

sdump: sdump.c libnsgif.c libnsbmp.c $(HDR)
	$(CC) $(CFLAGS) $(LDFLAGS) libnsgif.c libnsbmp.c $< -o $@

clean:
	rm -f $(DST)
