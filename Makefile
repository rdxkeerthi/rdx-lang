# RDX Linux Native Makefile (Freestanding)
# Builds the RDX compiler/JIT without libc using direct Linux syscalls

CC = gcc
CFLAGS = -std=c11 -nostdlib -fno-builtin -fno-stack-protector -O2

SRCS = src/main.c src/lexer.c src/parser.c src/analyzer.c src/codegen.c src/diag.c
OUT = rdx_bin

all: $(OUT)

$(OUT): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -f $(OUT)

.PHONY: all clean
