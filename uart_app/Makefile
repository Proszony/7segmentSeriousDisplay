CXX := g++
CXXFLAGS := -O2 -Wall -std=c++17 -pthread
PKG_CFLAGS := $(shell pkg-config --cflags opencv4)
PKG_LIBS := $(shell pkg-config --libs opencv4 sdl2)
FFMPEG_LIBS := -lavformat -lavcodec -lavutil -lswresample

SRCS := src/main.cpp src/video.cpp src/cli_parser.cpp src/framing.cpp src/uart.cpp src/audio.cpp
OBJS := $(SRCS:.cpp=.o)
TARGET := app

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(PKG_LIBS) $(FFMPEG_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(PKG_CFLAGS) -I include -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean