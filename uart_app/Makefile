CXX ?= g++
CXXFLAGS := -O2 -Wall -std=c++17 -pthread

PKG_CONFIG ?= pkg-config
# OpenCV 5 na Archu, OpenCV 4 na Debianie/Ubuntu/Fedorze.
OPENCV_PC  ?= $(shell $(PKG_CONFIG) --exists opencv5 && echo opencv5 || echo opencv4)
PKG_MODULES := $(OPENCV_PC) sdl2 libavformat libavcodec libavutil libswresample

PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKG_MODULES) 2>/dev/null)
PKG_LIBS   := $(shell $(PKG_CONFIG) --libs   $(PKG_MODULES) 2>/dev/null)

PREFIX  ?= /usr/local
VERSION ?= 1.0.0

SRCS := src/main.cpp src/video.cpp src/cli_parser.cpp src/framing.cpp src/uart.cpp src/audio.cpp
OBJS := $(patsubst src/%.cpp, build/%.o, $(SRCS))
TARGET := build/app

PC_FILE := build/pkgconfig/uart_app.pc
PC_TEMPLATE := ../pkgconfig/uart_app.pc.in

all: check-deps $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(PKG_LIBS)

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(PKG_CFLAGS) -I include -c $< -o $@

check-deps:
	@PKG_CONFIG="$(PKG_CONFIG)" ../scripts/check-deps.sh

# Sprawdza realne zależności dynamiczne zbudowanej aplikacji (ldd).
# Wyłapuje biblioteki, których nie ma w metadanych pkg-config,
# np. VTK wciągane przez -lopencv_viz / -lopencv_highgui.
check-runtime: $(TARGET)
	@missing=$$(ldd $(TARGET) 2>/dev/null | awk '/not found/ {print $$1}'); \
	if [ -n "$$missing" ]; then \
		echo "Brakuje bibliotek runtime (ldd: not found):"; \
		echo "$$missing" | sed 's/^/  - /'; \
		echo; \
		echo "Znajdź pakiet dostarczający bibliotekę:"; \
		echo "  Arch:     sudo pacman -Fy && pacman -F <lib>"; \
		echo "  Debian:   sudo apt-file update && apt-file search <lib>"; \
		echo "  Fedora:   dnf provides '*/<lib>'"; \
		echo "  openSUSE: zypper what-provides <lib>"; \
		echo "  Alpine:   apk search -v <lib>"; \
		exit 1; \
	fi; \
	echo "Runtime: wszystkie biblioteki dynamiczne znalezione."

$(PC_FILE): $(PC_TEMPLATE) Makefile FORCE
	@mkdir -p $(dir $@)
	@sed -e 's|@PREFIX@|$(PREFIX)|g' \
	     -e 's|@VERSION@|$(VERSION)|g' \
	     -e 's|@OPENCV_PC@|$(OPENCV_PC)|g' $< > $@.tmp
	@if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi

pc: $(PC_FILE)

install: $(PC_FILE)
	install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m644 $(PC_FILE) $(DESTDIR)$(PREFIX)/lib/pkgconfig/uart_app.pc
	install -d $(DESTDIR)$(PREFIX)/include/uart_app
	install -m644 include/*.h $(DESTDIR)$(PREFIX)/include/uart_app/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/pkgconfig/uart_app.pc
	rm -rf $(DESTDIR)$(PREFIX)/include/uart_app

clean:
	rm -rf build $(OBJS) $(TARGET)

.PHONY: all check-deps check-runtime pc install uninstall clean FORCE
