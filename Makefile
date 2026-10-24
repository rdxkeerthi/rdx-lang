# ============================================================================
#  Makefile — RDX Forensics & Cyber Investigation Language (Standalone)
# ============================================================================

CC ?= gcc
CFLAGS = -std=c11 -O3 -Wall -Wextra
SRCS = src/main.c src/rdx_engine.c src/rdx_cyber.c src/rdx_diag.c
OUT = rdx
PREFIX ?= /usr/local

all: $(OUT)

$(OUT): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) -lm

install: $(OUT)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(OUT) $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f $(OUT) $(OUT).exe rdx_bin rdx_bin.exe

.PHONY: all install clean
