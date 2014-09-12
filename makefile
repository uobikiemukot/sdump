CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -pedantic \
-march=native -Ofast -flto -pipe -s
#-Og -g -rdynamic #-pg
LDFLAGS = -ljpeg -lpng

HDR = stb_image.h libnsgif.h libnsbmp.h \
	sdump.h util.h loader.h image.h 
SRC = sdump.c libnsgif.c libnsbmp.c

SIXEL_SRC = libsixel/dither.c libsixel/fromsixel.c libsixel/image.c \
	libsixel/output.c libsixel/quant.c libsixel/tosixel.c
SIXEL_OBJ = dither.o fromsixel.o image.o \
	output.o quant.o tosixel.o

DST = sdump

all:  $(DST)

sdump: sdump.c libnsgif.c libnsbmp.c $(HDR)
	$(CC) -c -O3 $(SIXEL_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -I./libsixel \
		$(SIXEL_OBJ) libnsgif.c libnsbmp.c $< -o $@
	rm *.o

clean:
	rm -f $(DST)
