CXX_FLAGS = -Wall -Wextra -Wpedantic -Werror -std=c++14 -g
SDL_FLAGS = -D_REENTRANT -I/usr/include/SDL2 -lSDL2

all: chip8

chip8: chip8.cpp
	g++ chip8.cpp $(CXX_FLAGS) $(SDL_FLAGS) -o chip8 

clean:
	rm chip8
