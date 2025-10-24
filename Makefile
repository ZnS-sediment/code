.RECIPEPREFIX := >
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Iinclude $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf

SRCS = $(wildcard src/*.cpp)
OBJS = $(patsubst src/%.cpp, build/%.o, $(SRCS))
TARGET = myfs

all: $(TARGET)

$(TARGET): $(OBJS)
> $(CXX) -o $@ $^ $(LDFLAGS)

build/%.o: src/%.cpp
> @mkdir -p build
> $(CXX) $(CXXFLAGS) -c $< -o $@

run: all
> ./$(TARGET)

clean:
> @echo "Cleaning up..."
> rm -rf build $(TARGET) disk.img