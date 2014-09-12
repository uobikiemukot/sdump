CC      = gcc
LDFLAGS =
CFLAGS  = -Wall -Wextra -std=c99 -pedantic \
	-march=native -Os -pipe -s

HDR = ../stb_image.h ../libnsgif.h ../libnsbmp.h ../lodepng.h
SRC = ../libnsgif.c ../libnsbmp.c ../lodepng.c
DST = sdump

SIXEL_SRC = ../libsixel/dither.c ../libsixel/fromsixel.c ../libsixel/image.c \
	../libsixel/output.c ../libsixel/quant.c ../libsixel/tosixel.c
SIXEL_OBJ = dither.o fromsixel.o image.o \
	output.o quant.o tosixel.o

all: $(DST)

sdump: sdump.c $(HDR) $(SRC) $(SIXEL_SRC)
	$(CC) -c -O3 $(SIXEL_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -I../libsixel \
		$(SIXEL_OBJ) $(SRC) $< -o $@
	rm *.o

clean:
	rm -f $(DST) *.o
