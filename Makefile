CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -pthread
INCDIR = include
SRCDIR = src

ALGO_SRC = src/algorithms/rle.c src/algorithms/lzw.c src/algorithms/feistel.c src/algorithms/compression_selector.c
CORE_SRC = src/algorithms.c src/executor.c src/main.c
IO_SRC = src/io/file.c src/io/directory.c
UTIL_SRC = src/utils/utils.c

SRC = $(ALGO_SRC) $(CORE_SRC) $(IO_SRC) $(UTIL_SRC)
OBJ = $(SRC:.c=.o)
BIN = bin/gsea

all: $(BIN)

$(BIN): $(OBJ)
	mkdir -p bin
	$(CC) $(CFLAGS) $(OBJ) -o $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean
