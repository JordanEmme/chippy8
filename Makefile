CXX=g++
SDL2CFLAGS= $(sdl2-config --cflags)

CXXFLAGS= -Og -c --std=c++14 -Wall -Werror $(SDL2CFLAGS)
LDFLAGS= -lSDL2

chippy8: chippy8.o
	$(CXX) $(LDFLAGS) -o chippy8 chippy8.o

chippy8.o: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o chippy8.o

clean:
	rm chippy8*
