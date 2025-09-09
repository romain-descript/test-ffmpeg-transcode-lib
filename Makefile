.PHONY : clean all
default: all

OBJECTS  = $(patsubst src/%.c, dist/src/%.o, $(shell echo src/*.c))
EXAMPLES = $(patsubst examples/%.c, dist/examples/%, $(shell echo examples/*.c))
CC       = gcc
CFLAGS   = -g -fPIC
FFLIBS   = -lavformat -lavcodec -lavutil -lavfilter
LDFLAGS  = $(FFLIBS)

ifeq ($(shell uname -s),Linux)
    DYNLIB_EXT = .so
else
    DYNLIB_EXT = .dylib
endif

dist/src/libdeslib$(DYNLIB_EXT): $(OBJECTS) dist/src
	$(CC) -o $@ -shared $(LDFLAGS) $<

dist/src:
	mkdir -p dist/src

dist/examples:
	mkdir -p dist/examples

dist/src/%.o: src/%.c dist/src
	$(CC) $(CFLAGS) -c $< -o $@

dist/examples/%: examples/%.c dist/src/libdeslib$(DYNLIB_EXT) dist/examples
	$(CC) -L./dist/src -I./src -ldeslib $(LDFLAGS) -o $@ $<

all: dist/src/libdeslib$(DYNLIB_EXT) $(EXAMPLES)

clean:
	rm -rf dist/
