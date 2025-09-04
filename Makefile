all:
	gcc -g -fPIC -I. -lavformat -lavcodec -lavutil -lavfilter ./deslib.c -o deslib
