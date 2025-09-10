.PHONY : clean all
default: all

OBJECTS  = $(patsubst src/%.c, dist/src/%.o, $(shell echo src/*.c))
EXAMPLES = $(patsubst examples/%.c, dist/examples/%, $(shell echo examples/*.c))
CC       = gcc

# Use pkg-config to find FFmpeg include and library paths
FFMPEG_CFLAGS = $(shell pkg-config --cflags libavformat libavcodec libavutil libavfilter)
FFMPEG_LIBS   = $(shell pkg-config --libs libavformat libavcodec libavutil libavfilter)

CFLAGS   = -g -fPIC $(FFMPEG_CFLAGS)
LDFLAGS  = $(FFMPEG_LIBS)

ifeq ($(shell uname -s),Linux)
    DYNLIB_EXT = .so
else
    DYNLIB_EXT = .dylib
endif

dist/src/libmts-ffmpeg-wrapper$(DYNLIB_EXT): $(OBJECTS) dist/src
	$(CC) -o $@ -shared $(LDFLAGS) $<

dist/src:
	mkdir -p dist/src

dist/examples:
	mkdir -p dist/examples

dist/src/%.o: src/%.c dist/src
	$(CC) $(CFLAGS) -c $< -o $@

dist/examples/%: examples/%.c dist/src/libmts-ffmpeg-wrapper$(DYNLIB_EXT) dist/examples
	$(CC) -L./dist/src -I./src -lmts-ffmpeg-wrapper $(LDFLAGS) -o $@ $<

all: dist/src/libmts-ffmpeg-wrapper$(DYNLIB_EXT) $(EXAMPLES)

clean:
	rm -rf dist/
