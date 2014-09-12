# sdump

sixel image dumper

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
