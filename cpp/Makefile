CXX := g++
CXXFLAGS := -O2 -Wall -std=c++17 -pthread
PKG_CFLAGS := $(shell pkg-config --cflags opencv4 sdl2)
PKG_LIBS := $(shell pkg-config --libs opencv4 sdl2)
FFMPEG_LIBS := -lavformat -lavcodec -lavutil -lswresample

SRCS := main.cpp video.cpp audio.cpp cli_parser.cpp
OBJS := $(SRCS:.cpp=.o)
TARGET := app

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(PKG_LIBS) $(FFMPEG_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(PKG_CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

