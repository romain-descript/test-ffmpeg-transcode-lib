all:
	gcc -g -L/tmp/ffmpeg/lib -I /tmp/ffmpeg/include -lavformat -lavcodec -lavutil -lavfilter ./deslib.c -o deslib
