BIN=sdl2im
LIBS=sdl2

all: $(BIN)

$(BIN): sdl2im.c
	gcc sdl2im.c -o $@ -std=c99 `pkg-config --cflags --libs $(LIBS)` -lSDL2_ttf
