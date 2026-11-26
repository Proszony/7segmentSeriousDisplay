CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra
CXXFLAGS += -std=c++17

INCLUDES := -I ../common -I ../../uart_app/include
BIN := ../bin
UART_SRC := ../../uart_app/src/uart.cpp
FRAMING_SRC := ../../uart_app/src/framing.cpp

.PHONY: unit clean

unit: $(BIN)/test_uart $(BIN)/test_framing

$(BIN):
	@mkdir -p $@

$(BIN)/test_uart: test_uart.cpp $(UART_SRC) | $(BIN)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -pthread -o $@ $^

$(BIN)/test_framing: test_framing.cpp $(FRAMING_SRC) | $(BIN)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

clean:
	rm -f $(BIN)/test_uart $(BIN)/test_framing
