# Makefile for server_sock.c

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11

SRC = server_sock.c camera_drivers.c
OBJ = $(SRC:.c=.o)
TARGET = server_sock

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)
