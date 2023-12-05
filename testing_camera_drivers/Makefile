# Makefile for compiling camera_drivers.c on Raspberry Pi

CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lm -lpthread

TARGET = camera_app
SRC = camera_drivers.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)
