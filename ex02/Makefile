PROGRAM = cachebench
SOURCES = $(PROGRAM).c

CC = gcc -march=native -O3 -g
RDTSCP = $(shell cat /proc/cpuinfo | grep -o -m 1 rdtscp)

$(PROGRAM): $(SOURCES)
	$(if $(RDTSCP),$(shell echo "#define RDTSCP_SUPPORTED" > options.h),$(shell echo "" > options.h))
	$(CC) -o $(PROGRAM) $^

new: clean $(PROGRAM)

clean:
	rm -rf $(PROGRAM) options.h
