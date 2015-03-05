#CC ?= gcc
CC ?= clang

CFLAGS ?= -Wall -Wextra -std=c99 -pedantic \
-g -rdynamic -pipe \
-I./libsixel -I./libsixel/include
#-march=native -Ofast -flto -pipe -s
#-O3 -pipe -s 
LDFLAGS = -ljpeg -lpng #-lsixel
LIBSIXEL_CONFIG_H = libsixel/config.h

HDR = stb_image.h libnsgif.h libnsbmp.h \
	sdump.h util.h loader.h image.h sixel_util.h
SRC = sdump.c libnsgif.c libnsbmp.c

SIXEL_SRC = ./libsixel/src/dither.c ./libsixel/src/fromsixel.c \
	./libsixel/src/output.c ./libsixel/src/quant.c \
	./libsixel/src/tosixel.c
SIXEL_OBJ = dither.o fromsixel.o \
	output.o quant.o tosixel.o

DST = sdump

all:  $(DST)

$(LIBSIXEL_CONFIG_H):
	cd libsixel && ./configure

sdump: $(SRC) $(HDR) $(LIBSIXEL_CONFIG_H)
	$(CC) -I./libsixel -I./libsixel/include \
		-g -rdynamic -c $(SIXEL_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SIXEL_OBJ) $(SRC) -o $@
	rm -f *.o

16bpp_test: 16bpp_test.c libnsgif.c libnsbmp.c $(HDR) $(LIBSIXEL_CONFIG_H)
	$(CC) -I./libsixel -I./libsixel/include \
		-g -rdynamic -c $(SIXEL_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SIXEL_OBJ) 16bpp_test.c libnsgif.c libnsbmp.c -o $@
	rm -f *.o

clean:
	rm -f $(DST) sdump-nolibsixel *.o
