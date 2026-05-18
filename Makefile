# Makefile for RTP Plot Program
CC = gcc
PI = pyinstaller

SRCDIR  = src
BINDIR  = bin
WORKDIR = build

CFLAGS = -O3 -march=x86-64 -mtune=generic -funroll-loops -flto -pipe -fomit-frame-pointer -DNDEBUG -Wall -Wextra -Wpedantic -shared
PFLAGS = --onefile --noconfirm --exclude numpy --exclude pandas --distpath $(BINDIR) --specpath $(WORKDIR) --workpath $(WORKDIR)
TARGET = $(WORKDIR)/ell-lite-rtplog_app
LIB    = $(WORKDIR)/ell-lite-rtplog_anlys.so
CSRC   = $(SRCDIR)/ell-lite-rtplog_anlys.c
PSRC   = $(SRCDIR)/ell-lite-rtplog_app.py

all: $(TARGET)

$(TARGET): $(LIB) $(PSRC)
	$(PI) $(PFLAGS) --add-binary="$(notdir $(LIB)):." $(PSRC)

$(LIB): $(CSRC) 
	mkdir -p $(WORKDIR)
	$(CC) $(CFLAGS) -o $(LIB) $(CSRC)

clean:
	rm -rf $(WORKDIR) 
