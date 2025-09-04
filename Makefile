all:
	gcc -g -fPIC -I. -c ./deslib.c
	gcc -shared -o libdeslib.so -lavformat -lavcodec -lavutil -lavfilter ./deslib.o
