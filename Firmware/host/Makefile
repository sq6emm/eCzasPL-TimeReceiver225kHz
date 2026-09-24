# Host build of the firmware core (DSP, frame decoder, timekeeper) + test harness.
CORE = ../src/core
SRC  = sim.c $(CORE)/dsp.c $(CORE)/frame.c $(CORE)/timekeeper.c $(CORE)/nmea.c $(CORE)/sintab.c
CFLAGS ?= -O2 -g -Wall -Wextra -Wno-unused-parameter -I$(CORE)

sim: $(SRC) $(wildcard $(CORE)/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm

sim-static: $(SRC) $(wildcard $(CORE)/*.h)
	$(CC) $(CFLAGS) -static -o $@ $(SRC) -lm

test: sim
	./sim -t

clean:
	rm -f sim sim-static
