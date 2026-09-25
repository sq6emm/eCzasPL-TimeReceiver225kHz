# e-CzasPL receiver firmware for dsPIC33FJ128GP804 (MPLAB XC16).
# Boards fitted with a dsPIC33FJ64MC804 (same pinout): make MCU=33FJ64MC804
# make NMEA_DEBUG=1: also send the diagnostics on the NMEA port ($PECZ)
#   make            -> build/eczas_receiver.hex
#   make host-test  -> build and run the host test harness (gcc)
XC16_DIR ?= $(firstword $(wildcard $(HOME)/.local/microchip/xc16/v*) $(wildcard /opt/microchip/xc16/v*))
CC      = $(XC16_DIR)/bin/xc16-gcc
BIN2HEX = $(XC16_DIR)/bin/xc16-bin2hex
MCU     ?= 33FJ128GP804
# NMEA_DEBUG=1 copies the diagnostics to the NMEA port as $PECZ sentences
# (longer than NMEA 0183's 82 characters; for test benches)
NMEA_DEBUG ?= 0

SRC = $(wildcard src/core/*.c) $(wildcard src/hw/*.c)
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
CFLAGS  = -mcpu=$(MCU) -omf=elf -O1 -std=gnu99 -Wall -msmart-io=1 -mconst-in-code \
          -Isrc/core -Isrc/hw -mno-eds-warn -DDEBUG_TO_NMEA=$(NMEA_DEBUG)
LDFLAGS = -mcpu=$(MCU) -omf=elf -Wl,--script=p$(MCU).gld,--heap=0,--stack=1024,-Map=build/eczas_receiver.map,--report-mem

all: build/eczas_receiver.hex

build/%.o: src/%.c $(wildcard src/core/*.h src/hw/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/eczas_receiver.elf: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) -lm

build/eczas_receiver.hex: build/eczas_receiver.elf
	$(BIN2HEX) -omf=elf $<

host-test:
	$(MAKE) -C host test

clean:
	rm -rf build
	$(MAKE) -C host clean

.PHONY: all clean host-test
