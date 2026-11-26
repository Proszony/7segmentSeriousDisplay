CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra
CXXFLAGS += -std=c++17

INCLUDES := -I ../common -I ../../firmware
BIN := ../bin

.PHONY: unit hardware clean

unit: $(BIN)/test_config

hardware: $(BIN)/test_daisy_cs

$(BIN):
	@mkdir -p $@

$(BIN)/test_config: test_config.cpp ../../firmware/config.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

$(BIN)/test_daisy_cs: test_daisy_cs.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ -lgpiod

clean:
	rm -f $(BIN)/test_config $(BIN)/test_daisy_cs
