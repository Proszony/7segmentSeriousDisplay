CXX := g++
CXXFLAGS := -O2 -Wall -std=c++17 -pthread
PKG_CFLAGS := $(shell pkg-config --cflags opencv4)
PKG_LIBS := $(shell pkg-config --libs opencv4 sdl2)
FFMPEG_LIBS := -lavformat -lavcodec -lavutil -lswresample

SRCS := src/main.cpp src/video.cpp src/cli_parser.cpp src/framing.cpp src/uart.cpp src/audio.cpp
OBJS := $(patsubst src/%.cpp, build/%.o, $(SRCS))
TARGET := build/app

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(PKG_LIBS) $(FFMPEG_LIBS)

build/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(PKG_CFLAGS) -I include -c $< -o $@

clean:
	rm -rf /build #$(OBJS) $(TARGET)

.PHONY: all clean
