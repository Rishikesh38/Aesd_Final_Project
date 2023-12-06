# Makefile for client_sock.c

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11

SRC = client_sock.c
OBJ = $(SRC:.c=.o)
TARGET = client_sock

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)
