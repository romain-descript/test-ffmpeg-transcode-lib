OS_NAME := $(shell uname -s)

ifeq ($(OS_NAME),Linux)
    DYNLIB_EXT := .so
else
    DYNLIB_EXT := .dylib
endif

all:
	mkdir -p dist/
	gcc -g -fPIC -I. -c ./src/deslib.c -o ./dist/deslib.o
	gcc -shared -o dist/libdeslib$(DYNLIB_EXT) -lavformat -lavcodec -lavutil -lavfilter ./dist/deslib.o
	gcc -L./dist -I./src -ldeslib -lavformat -lavcodec -lavutil -lavfilter -o dist/mp4 ./examples/mp4.c
	gcc -L./dist -I./src -ldeslib -lavformat -lavcodec -lavutil -lavfilter -o dist/aac ./examples/aac.c

clean:
	rm -rf dist/
