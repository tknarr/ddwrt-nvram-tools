.PHONY: all clean

all: nvram_dump nvram_build

nvram_dump: nvram_dump.c

nvram_build: nvram_build.c

clean:
	rm -f nvram_dump nvram_build
