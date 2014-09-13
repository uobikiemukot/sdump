# sdump

sixel image dumper

this program is a variant of [idump](https://github.com/uobikiemukot/idump)

## install

you need to install following library before build

-	libsixel (https://github.com/saitoha/libsixel)
-	libpng
-	libjpeg or libjpeg-turbo

then just type "make"

## install (static)

static link version (located in ./static) depends no extra (shared) library

just type "cd static; make"

-	libsixel (included in ./static/libsixel)
-	stb_image.h (for jpeg)
-	lodepng.h, lodepng.c (for png)

## usage

 $ sdump [-h] [-f] [-r angle] image

 $ cat image | sdump

 $ wget -q -O - url | sdump

## options

-	-h: show help
-	-f: fit image to display size (reduce only)
-	-r: rotate image (90 or 180 or 270)

## supported image format

-	jpeg by libjpeg
-	png by libpng
-	gif by libnsgif
-	bmp by libnsbmp
-	pnm by sdump

## wrapper scripts

-	surl: equal "wget -q -O - url | sdump" (depends wget)
-	sviewer: take multiple files as arguments 
-	spdf: pdf viewer (depends mupdf >= 1.5)

## license

The MIT License (MIT)

Copyright (c) 2014 haru <uobikiemukot at gmail dot com>
