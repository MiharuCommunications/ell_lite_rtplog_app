# Makefile for RTP Plot Program
CC = gcc
CFLAGS = -O3 -march=native -funroll-loops -flto -pipe -fomit-frame-pointer -DNDEBUG -Wall -Wextra
TARGET = plot
SRC = src/plot.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
