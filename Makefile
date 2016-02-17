.PHONY: all clean

all: nvram_dump nvram_build default_dump

nvram_dump: nvram_dump.c

nvram_build: nvram_build.c

default_dump: default_dump.c

clean:
	rm -f nvram_dump nvram_build default_dump
