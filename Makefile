CXX := g++
SDL2CFLAGS := $(shell sdl2-config --cflags)

CXXFLAGS := -O2 -c --std=c++17 -Wall -Werror $(SDL2CFLAGS)
LDFLAGS := $(shell sdl2-config --libs)
chippy8: chippy8.o
	$(CXX) $(LDFLAGS) -o chippy8 chippy8.o

chippy8.o: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o chippy8.o

clean:
	rm chippy8*
