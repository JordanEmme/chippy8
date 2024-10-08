CXX=g++
SDL2CFLAGS= $(sdl2-config --cflags)

CXXFLAGS= -Og -c --std=c++14 -Wall -Werror $(SDL2CFLAGS)
LDFLAGS= -lSDL2

chippy8: main.o
	$(CXX) $(LDFLAGS) -o chippy8 main.o

main.o: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp

clean:
	rm chippy8 main.o
