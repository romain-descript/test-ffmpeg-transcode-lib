.PHONY : clean all
default: all

OBJECTS  = $(patsubst src/%.c, dist/%.o, $(shell echo src/*.c))
EXAMPLES = $(patsubst examples/%.c, dist/%.exe, $(shell echo examples/*.c))
CC       = gcc
CFLAGS   = -g -fPIC
FFLIBS   = -lavformat -lavcodec -lavutil -lavfilter
LDFLAGS  = $(FFLIBS)

ifeq ($(shell uname -s),Linux)
    DYNLIB_EXT = .so
else
    DYNLIB_EXT = .dylib
endif

dist/libdeslib$(DYNLIB_EXT): $(OBJECTS)
	$(CC) -o $@ -shared $(LDFLAGS) $<

dist:
	mkdir -p dist

dist/%.o: src/%.c dist
	$(CC) $(CFLAGS) -c $< -o $@

dist/%.exe: examples/%.c dist/libdeslib$(DYNLIB_EXT)
	$(CC) -L./dist -I./src -ldeslib $(LDFLAGS) -o $@ $<

all: $(EXAMPLES) dist/libdeslib$(DYNLIB_EXT)

clean:
	rm -rf dist/
