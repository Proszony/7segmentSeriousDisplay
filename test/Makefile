.PHONY: all unit hardware run-unit run-hardware clean

# Host-only tests: no Raspberry Pi, SPI, GPIO or serial hardware required.
all: unit

unit:
	$(MAKE) -C uart_test unit
	$(MAKE) -C firmware_test unit

# Tests that touch real hardware. Run on a Raspberry Pi.
hardware:
	$(MAKE) -C firmware_test hardware

run-unit: unit
	@status=0; \
	for t in bin/test_uart bin/test_framing bin/test_config; do \
		echo; echo "Running: $$t "; \
		./$$t || status=1; \
	done; \
	exit $$status

run-hardware: hardware
	@status=0; \
	for t in bin/test_daisy_cs; do \
		echo; echo "Running: $$t "; \
		./$$t || status=1; \
	done; \
	exit $$status

clean:
	$(MAKE) -C uart_test clean
	$(MAKE) -C firmware_test clean
	rmdir bin 2>/dev/null || true
