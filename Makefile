PREFIX  ?= /usr/local
DESTDIR ?=

export PREFIX
export DESTDIR

.PHONY: all uart_app check-deps check-runtime pc install uninstall clean

all: uart_app

uart_app:
	$(MAKE) -C uart_app

check-deps:
	$(MAKE) -C uart_app check-deps

check-runtime:
	$(MAKE) -C uart_app check-runtime

pc:
	$(MAKE) -C uart_app pc

install:
	$(MAKE) -C uart_app install

uninstall:
	$(MAKE) -C uart_app uninstall

clean:
	$(MAKE) -C uart_app clean
