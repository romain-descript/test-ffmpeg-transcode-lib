all:
	gcc -g -fPIC -I. -c ./deslib.c
	gcc -shared -o libdeslib.dylib -lavformat -lavcodec -lavutil -lavfilter ./deslib.o
	gcc -L. -ldeslib -lavformat -lavcodec -lavutil -lavfilter -o main ./main.c
