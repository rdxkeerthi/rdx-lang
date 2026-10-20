# ============================================================================
#  Makefile — RDX Forensics & Cyber Investigation Language (Standalone)
# ============================================================================

CC ?= gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra
SRCS = src/main.c src/rdx_engine.c src/rdx_cyber.c src/rdx_diag.c
OUT = rdx

all: $(OUT)

$(OUT): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -f $(OUT) $(OUT).exe rdx_bin rdx_bin.exe

.PHONY: all clean
