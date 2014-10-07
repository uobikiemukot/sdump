#CC ?= gcc
CC ?= clang

CFLAGS  = -Wall -Wextra -std=c99 -pedantic \
-g -Og -rdynamic -pipe \
-I./libsixel-normalize-depth -I./libsixel-normalize-depth/include
#-march=native -Ofast -flto -pipe -s
#-O3 -pipe -s 
LDFLAGS = -ljpeg -lpng -lsixel

HDR = stb_image.h libnsgif.h libnsbmp.h \
	sdump.h util.h loader.h image.h sixel_util.h
SRC = sdump.c libnsgif.c libnsbmp.c

SIXEL_SRC = ./libsixel-normalize-depth/src/dither.c ./libsixel-normalize-depth/src/fromsixel.c \
	./libsixel-normalize-depth/src/output.c ./libsixel-normalize-depth/src/quant.c \
	./libsixel-normalize-depth/src/tosixel.c
SIXEL_OBJ = dither.o fromsixel.o \
	output.o quant.o tosixel.o

DST = sdump

all:  $(DST)

sdump: $(SRC) $(HDR)
	$(CC) -I./libsixel-normalize-depth -I./libsixel-normalize-depth/include \
		-g -Og -rdynamic -c $(SIXEL_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SIXEL_OBJ) $(SRC) -o $@
	rm -f *.o

16bpp_test: 16bpp_test.c libnsgif.c libnsbmp.c $(HDR)
	$(CC) -I./libsixel-normalize-depth -I./libsixel-normalize-depth/include \
		-g -Og -rdynamic -c $(SIXEL_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SIXEL_OBJ) 16bpp_test.c libnsgif.c libnsbmp.c -o $@
	rm -f *.o

clean:
	rm -f $(DST) sdump-nolibsixel *.o
