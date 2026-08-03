CXX := g++
CXXFLAGS := -O2 -Wall -std=c++17
TARGET := display

all: $(TARGET)

$(TARGET): display.cpp config.cpp ../uart_app/src/framing.cpp ../uart_app/src/uart.cpp
	$(CXX) $(CXXFLAGS) -pthread -I ../uart_app/include -o $@ $^ -lgpiod

clean: 
	rm -f $(TARGET)

.PHONY: all clean
