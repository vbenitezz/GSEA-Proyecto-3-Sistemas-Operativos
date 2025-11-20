CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -pthread

SRC = src/main.c \
      src/io/file.c \
      src/io/directory.c \
      src/utils/utils.c \
      src/pipeline/executor.c \
      src/algorithms/algorithms.c

OBJ = $(SRC:.c=.o)
BIN = bin/gsea

all: $(BIN)

$(BIN): $(OBJ)
	mkdir -p bin
	$(CC) $(OBJ) -o $(BIN) $(CFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ) $(BIN)
