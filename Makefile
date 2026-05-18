# Makefile for RTP Plot Program
CC = gcc
PI = pyinstaller
CFLAGS = -O3 -march=native -funroll-loops -flto -pipe -fomit-frame-pointer -DNDEBUG -Wall -Wextra -Wpedantic -shared
PFLAGS = --onedir --noconfirm
TARGET = ell-lite-rtplog_app
CSRC = src/ell-lite-rtplog_anlys.c
LIB = ell-lite-rtplog_anlys.so
PSRC = src/ell-lite-rtplog_app.py

all: $(TARGET)

$(TARGET): $(LIB) $(PSRC)
	$(PI) $(PFLAGS) --add-binary="$(LIB):." $(PSRC)
	cp ./dist/ell-lite-rtplog_app/ell-lite-rtplog_app ./

$(LIB): $(CSRC) 
	$(CC) $(CFLAGS) -o $(LIB) $(CSRC)

clean:
	rm -f $(TARGET) $(LIB)
	rm -rf build dist
